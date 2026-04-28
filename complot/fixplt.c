/* fixplt.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void		VecMul3D(double c[4],    double a[4][4], double x[4]);
static void		MatMul3D(double c[4][4], double a[4][4], double b[4][4]);
static void		MatInv3D(double c[4][4], double a[4][4]);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */

/* ===========================================================================
--  Plot3DTransform - Routine to "pre-apply" a transform onto the X,Y,Z
--  triples before they are converted to inches and pixel levels.  Internally
--  it is applied after the normalization, but really doesn't matter.
--
--  Plot3DTransform(int key, REAL gp_me[3][3], REAL gp_old[3][3]);
--
--  Inputs: key - operation
--            -1 ==> query current matrix only
--             0 ==> reset to NULL the transform
--             1 ==> restore matrix to value of gp_me
--             2 ==> pre-multiply matrix by gp_me
--          gp_me - if not NULL, a "rotation" transformation applied after
--                  conversion user->inches, but before to pixels.  Should be
--                  pure rotation or axis exchange (LHS to RHS for example).
--                  If NULL, key 1 will reset to identify, key 2 unchanged.
--          gp_old - if not NULL, will receive copy of current matrix.
--
-- Note: This is fundamentally different from Plot3DMultiply which does a
--       one shot change on the array.  This is effectively permanent.
=========================================================================== */
void Plot3DTransform(int key, REAL gp_me[3][3], REAL gp_old[3][3]) {

	int i,j;
	double user[4][4], utmp[4][4];
	
/* Return old value if user interested */
	if (gp_old != NULL) {
		for (i=0; i<3; i++) for (j=0; j<3; j++) gp_old[i][j] = PL_Plot.gp[i][j];
	}

	if (key == 0 || gp_me == NULL) {
		for (i=0; i<4; i++) for (j=0; j<4; j++) PL_Plot.gp[i][j] = (i == j) ? 1.0f : 0.0f ;
	} else if (key == 1) {
		for (i=0; i<3; i++) for (j=0; j<3; j++) PL_Plot.gp[i][j] = gp_me[i][j];
	} else if (key == 2) {
		for (i=0; i<4; i++) for (j=0; j<4; j++) user[i][j] = (i == j) ? 1 : 0 ;
		for (i=0; i<3; i++) for (j=0; j<3; j++) user[i][j] = gp_me[i][j];
		for (i=0; i<4; i++) for (j=0; j<4; j++) utmp[i][j] = PL_Plot.gp[i][j];
		MatMul3D(utmp, utmp, user);
		for (i=0; i<4; i++) for (j=0; j<4; j++) PL_Plot.gp[i][j] = (REAL) utmp[i][j];
	} else {											/* Don't do SAVE if otherwise */
		return;
	}

	if (PlotWindow->Save.On) {
		if (gp_me == NULL || key == 0) {
			SV_PutCmd(SV_RESET3DTRANSFORM,0,0,0);
		} else {
			SV_PutCmd(SV_SET3DTRANSFORM,0,9,0);
			for (i=0; i<3; i++) for (j=0; j<3; j++) SV_PutReal(PL_Plot.gp[i][j]);
		}
	}

	PlotFixInternal();							/* And make sure put in place */
	return;
}


/* ===========================================================================
--  Plot3DMultiply - Routine to "multiply" the current transformation by a
--  specific factor.  It is assumed that the matrix is valid before it is
--  applied.
--
--  Plot3DMultiply(int key, REAL gp_me[4][4]);
--
--  Inputs: key - OR'd operations
--                   1 ==> apply to user->inch matrix
--                   2 ==> apply to inch->pixel matrix
--                   4 ==> apply to user->pixel matrix
--                   8 ==> recalculate inch->user matrix
--                   0 ==> post multiply matrix
--                 100 ==> pre apply the matrix
--            gp - matrix to use
--
--  Note: This is a one-shot deal.  Validity of matrix is presupposed.
=========================================================================== */
void Plot3DMultiply(int key, REAL gp_me[4][4]) {

	int i,j,k;
	double utmp[4][4], user[4][4];
	REAL (*rtmp)[4][4];
	
	if (key == 0 || gp_me == NULL) return;
	
	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_3DMULTIPLY,1,16,0);
		SV_PutInt(key);
		for (i=0; i<4; i++) for (j=0; j<4; j++) SV_PutReal(gp_me[i][j]);
	}

	for (i=0; i<4; i++) for (j=0; j<4; j++) user[i][j] = gp_me[i][j];

	for (k=0; k<3; k++) {
		if (! (key & (0x01<<k))) continue;
		switch (k) {
			case 0:	rtmp = &PL_Plot.ui; break;
			case 1:	rtmp = &PL_Plot.ip; break;
			case 2:	rtmp = &PL_Plot.up; break;
		}
		for (i=0; i<4; i++) for (j=0; j<4; j++) utmp[i][j] = (*rtmp)[i][j];
		if (key & 0x100) 	MatMul3D(utmp, user, utmp);
		else					MatMul3D(utmp, utmp, user);
		for (i=0; i<4; i++) for (j=0; j<4; j++) (*rtmp)[i][j] = (REAL) utmp[i][j];
	}

	if (key & 0x08) {
		for (i=0; i<4; i++) for (j=0; j<4; j++) utmp[i][j] = PL_Plot.ui[i][j];
		MatInv3D(user, utmp);					/* user->inch = inverse(iu) */
		for (i=0; i<4; i++) for (j=0; j<4; j++) PL_Plot.iu[i][j] = (REAL) user[i][j];
	}
		
	return;
}


/* ===========================================================================
--  PlotFixInternal - Initialize clipping and preset coordinate conversion constants
--
--  Usage: PlotFixInternal
--
--  Inputs:  Couples DRIVER.INS common block with COMPLOT.INS to properly
--           generate FIXPLOT common blocks.
=========================================================================== */
void PlotFixInternal(void) {	

	double ui[4][4], ip[4][4], ig[4][4], ip_2d[4][4], ct[4][4], o[4], x[4];
	double factr, xmin,xmax,ymin,ymax, ux,theta;
	double xl,xh,yl,yh;
	int i,j,k;

	DspClipSet clip;
	
/*  First, set parameters for scaling */
	switch (PlotWindow->orient) {
		case LANDSCAPE:
		case INV_LANDSCAPE:
			PL_Plot.xscale = (REAL) DEVICE->xperinch;
			PL_Plot.yscale = (REAL) DEVICE->yperinch;
			PL_Plot.xhigh  = (REAL) DEVICE->xmax;
			PL_Plot.yhigh  = (REAL) DEVICE->ymax;
			break;
		case PORTRAIT:
		case INV_PORTRAIT:
			PL_Plot.xscale = (REAL) DEVICE->yperinch;
			PL_Plot.yscale = (REAL) DEVICE->xperinch;
			PL_Plot.xhigh  = (REAL) DEVICE->ymax;
			PL_Plot.yhigh  = (REAL) DEVICE->xmax;
			break;
	}
	PL_Plot.xhigh = (PL_Plot.xhigh-1)/PL_Plot.xscale;	/* High inches - 1 dot safety */
	PL_Plot.yhigh = (PL_Plot.yhigh-1)/PL_Plot.yscale;	/* High inches - 1 dot safety */

/* Check if we need to resize the PL_SubPage information */
	if (PL_SubPage.active) {
		if (! PL_SubPage.init) {
			PL_SubPage.xshift = ((REAL) PL_Plot.xhigh) / PL_SubPage.ncols;
			PL_SubPage.yshift = ((REAL) PL_Plot.yhigh) / PL_SubPage.nrows;
			PL_SubPage.factr  = min(PL_SubPage.xshift/PlotWindow->xsize, PL_SubPage.yshift/PlotWindow->ysize);
			PL_SubPage.factr *= 0.95f;						/* And 5% margin for error */
			PL_SubPage.init = TRUE;
		}
		PL_Plot.xhigh = PL_SubPage.xshift / PL_SubPage.factr * 0.95f;
		PL_Plot.yhigh = PL_SubPage.yshift / PL_SubPage.factr * 0.95f;
	}

/* Effective "shrink factor" is combination of request and PL_SubPage value */
	factr = PlotWindow->factr;
	if (PL_SubPage.active) factr *= PL_SubPage.factr;
	PL_Plot.factr = (REAL) factr;
	
/* Fixmod: 1 ==> Pen up, solid; 3 => pen down, solid */
	PL_Plot.fixmod = (PlotWindow->lintyp == 1) ? 1 : 3 ;

/*  Conversion factors for segmented lines.  patx is x scaling in pixels */
	PL_Plot.patx = 1.0f / ( PlotWindow->patsiz * (REAL) factr * PL_Plot.xscale );
	PL_Plot.paty = 1.0f / ( PlotWindow->patsiz * (REAL) factr * PL_Plot.yscale );

/* ---------------------------------------------------------------------------
-- Start with the conversion from user units or inches to prescaled units
-- Prescaled units are floating point, 0.5 > than the integer plotter units:
--
-- Create 3 generalized transformation matrices converting from:
--   PL_PLOT.ui[4][4] - User to inches
--   PL_PLOT.ip[4][4] - Inches to pixels
--   PL_PLOT.up[4][4] - User to pixels    =    [ip] * [ui]
--
--   [xinch]      [ ui[0][0]  ui[0][1]  ui[0][2]  ui[0][3] ]  [ x ]
--   |yinch|  =   | ui[1][0]  ui[1][1]  ui[1][2]  ui[1][3] |  | y |
--   |zinch|      | ui[2][0]  ui[2][1]  ui[2][2]  ui[2][3] |  | z |
--   [  1  ]      [ ui[3][0]  ui[3][1]  ui[3][2]  ui[3][3] ]  [ 1 ]
--
-- ip[] and up[] also include conversion for rotation of the plot on the
-- output page.  Page always assumed LANDSCAPE orientation, allow any of
-- the four rotations.
--
-- ip_2D is needed later for clipping box.  Handle outside of mode_3d loop
---------------------------------------------------------------------------- */
	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) ip_2d[i][j] = (i==j) ? 1 : 0 ;
	}
	ip_2d[0][0] = PL_Plot.xscale * factr;
	ip_2d[0][3] = PL_Plot.xscale * factr * PlotWindow->xorg ;
	ip_2d[1][1] = PL_Plot.yscale * factr;
	ip_2d[1][3] = PL_Plot.yscale * factr * PlotWindow->yorg ;

/* Add the offset to handle subpaging */
	if (PL_SubPage.active) {
		ip_2d[0][3] += PL_Plot.xscale * PL_SubPage.xshift * (PL_SubPage.page%PL_SubPage.ncols) ;
		ip_2d[1][3] += PL_Plot.yscale * PL_SubPage.yshift * (PL_SubPage.nrows-(PL_SubPage.page/PL_SubPage.ncols)%PL_SubPage.nrows-1) ;
	}

/* Now, handle the scaleing for 2D or 3D as appropriate */
	if (! PlotWindow->mode_3d) {						/* 2D mode						*/

		for (i=0; i<4; i++) {
			for (j=0; j<4; j++) ui[i][j] = (i==j) ? 1 : 0 ;
		}
		
		ui[0][0] = PlotWindow->xfact;					/* User to inches */
		ui[0][3] = PlotWindow->xoff;
		ui[1][1] = PlotWindow->yfact;
		ui[1][3] = PlotWindow->yoff;
		for (i=0; i<4; i++) {
			for (j=0; j<4; j++) PL_Plot.ui[i][j] = (REAL) ui[i][j];
		}
		
		for (i=0; i<4; i++) {
			for (j=0; j<4; j++) ip[i][j] = ip_2d[i][j];
		}

/* Handle the 3D transformations now */
	} else {

/* Nominal equivalent size (will become new inches of 3D viewport) */
		ux = min(PlotWindow->xsize-PlotWindow->xmarg[0]-PlotWindow->xmarg[1],
					PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]);

/* Step 1 - normalize all axes to range from 0 to ux */
		for (i=0;i<4;i++) {for (j=0; j<4; j++) ui[i][j] = (i==j)?1:0;}
		if (PlotWindow->usrnbl) {
			ui[0][0] = ux / (PlotWindow->xmax-PlotWindow->xmin);
			ui[1][1] = ux / (PlotWindow->ymax-PlotWindow->ymin);
			ui[2][2] = ux / (PlotWindow->zmax-PlotWindow->zmin);
			ui[0][3] = -ui[0][0] * (PlotWindow->xmin);
			ui[1][3] = -ui[1][1] * (PlotWindow->ymin);
			ui[2][3] = -ui[2][2] * (PlotWindow->zmin);
		}

/* Call this my conversion from user space to inches */
		for (i=0; i<4; i++) {
			for (j=0; j<4; j++) PL_Plot.ui[i][j] = (REAL) ui[i][j];
		}

/* Step 2a - start with generic transform from user gp */
		for (i=0;i<4;i++) for (j=0; j<4; j++) ip[i][j] = PL_Plot.gp[i][j];

/* Step 2 - do my rotations on the inch scale, in order z, x, y */
		for (i=0;i<4;i++) for (j=0; j<4; j++) ct[i][j] = (i==j)?1:0;
		theta = PlotWindow->rotate[2] * 0.017453292519943f;
		ct[0][0] =  cos(theta);					/* Transform matrix			*/
		ct[1][1] =  cos(theta);					/* Interpret as [row][col]	*/
		ct[0][1] = -sin(theta);
		ct[1][0] =  sin(theta);
		MatMul3D(ip, ct, ip);

		for (i=0;i<4;i++) for (j=0; j<4; j++) ct[i][j] = (i==j)?1:0;
		theta = PlotWindow->rotate[0] * 0.017453292519943f;
		ct[1][1] =  cos(theta);					/* Rotation about my x axis */
		ct[2][2] =  cos(theta);
		ct[1][2] = -sin(theta);
		ct[2][1] =  sin(theta);
		MatMul3D(ip, ct, ip);

		for (i=0;i<4;i++) for (j=0; j<4; j++) ct[i][j] = (i==j)?1:0;
		theta = PlotWindow->rotate[1] * 0.017453292519943f;
		ct[0][0] =  cos(theta);					/* Rotation about my y axis */
		ct[2][2] =  cos(theta);
		ct[0][2] =  sin(theta);
		ct[2][0] = -sin(theta);
		MatMul3D(ip, ct, ip);

/* Step 3 - determine extent of box [xmin,ymin,zmin][xmax,ymax,zmax] */
		xmin = ymin = +DBL_MAX;					/* Start way off base	*/
		xmax = ymax = -DBL_MAX;
		x[3] = 1;									/* Unitary vector			*/
		for (i=0; i<2; i++) {
			x[0] = (i==0) ? 0 : ux;
			for (j=0; j<2; j++) {
				x[1] = (j==0) ? 0 : ux;
				for (k=0; k<2; k++) {
					x[2] = (k==0) ? 0 : ux;
					VecMul3D(o, ip, x);
					xmin = min(xmin, o[0]);
					xmax = max(xmax, o[0]);
					ymin = min(ymin, o[1]);
					ymax = max(ymax, o[1]);
				}
			}
		}

/* Step 4 - Transform so goes from xmin/xmax goes xorg+xmarg to xorg+xsize-xmarg */
		for (i=0;i<4;i++) {for (j=0; j<4; j++) ct[i][j] = (i==j)?1:0;}
		ct[0][0] = (PlotWindow->xsize-PlotWindow->xmarg[0]-PlotWindow->xmarg[1]) / (xmax-xmin);
		ct[0][3] =  PlotWindow->xorg + PlotWindow->xmarg[0] - xmin*ct[0][0];
		ct[1][1] = (PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]) / (ymax-ymin);
		ct[1][3] =  PlotWindow->yorg + PlotWindow->ymarg[0] - ymin*ct[1][1];
		MatMul3D(ip, ct, ip);

/* Step 5 - scale by the factr and by x/y scale */
		for (i=0;i<4;i++) {for (j=0; j<4; j++) ct[i][j] = (i==j)?1:0;}
		ct[0][0] = PL_Plot.xscale * factr;
		ct[1][1] = PL_Plot.yscale * factr;
		ct[2][2] = PL_Plot.xscale * factr;				/* Use X again */
		MatMul3D(ip, ct, ip);								/* Handles scaling */
	}

/* Save this transformation, just before Landscape/portrait scaling, as */
/* the matrix ug (user to grid).  Needed primarily by 3D to get direction */
/* of drawing components */
	for (i=0; i<4; i++) {for (j=0; j<4; j++) PL_Plot.ig[i][j] = (REAL) (ig[i][j] = ip[i][j]);}
	
/* Finally, in both cases create user->pixel and inch->user */
/* In both cases, do the translation/shift to handle portrait, etc ... */
/* Also, fill in the orient_angle so can inform as appropriate */
	for (i=0;i<4;i++) {for (j=0;j<4;j++) ct[i][j] = (i==j)?1:0;}
	switch (PlotWindow->orient) {
		case LANDSCAPE:
			PL_Plot.orient_angle = 0;
			break;
		case INV_LANDSCAPE:
			PL_Plot.orient_angle = 180;
			ct[0][0] = ct[1][1] = -1;
			ct[0][3] = DEVICE->xmax;
			ct[1][3] = DEVICE->ymax;
			break;
		case PORTRAIT:
			PL_Plot.orient_angle = 270;			/* Currently 270 degrees	*/
			ct[0][0] = 0;								/* X gets Y coordinate		*/
			ct[0][1] = 1;
			ct[1][0] = -1;								/* Y get ymax - X coordinate */
			ct[1][1] = 0;
			ct[1][3] = DEVICE->ymax;
			break;
		case INV_PORTRAIT:
				PL_Plot.orient_angle = 90;				/* Currently 90 degrees	*/
			ct[0][0] = 0;								/* X gets xmax - Y coordinate */
			ct[0][1] = -1;
			ct[0][3] = DEVICE->xmax;
			ct[1][0] = 1;								/* Y get X coordinate */
			ct[1][1] = 0;
			break;
	}
	MatMul3D(ip,    ct, ip);
	MatMul3D(ip_2d, ct, ip_2d);					/* Needed for clipping viewport */
	for (i=0; i<4; i++) {for (j=0; j<4; j++) PL_Plot.ip[i][j] = (REAL) ip[i][j];}

/* Step 6 - combine so can get from user to pixels and grid also */
	MatMul3D(ct, ip, ui);								/* user->pixel = ip * ui */
	for (i=0; i<4; i++) {for (j=0; j<4; j++) PL_Plot.up[i][j] = (REAL) ct[i][j];}
	MatMul3D(ct, ig, ui);
	for (i=0; i<4; i++) {for (j=0; j<4; j++) PL_Plot.ug[i][j] = (REAL) ct[i][j];}

	MatInv3D(ct, ui);									/* user->inch = inverse(iu) */
	for (i=0; i<4; i++) {for (j=0; j<4; j++) PL_Plot.iu[i][j] = (REAL) ct[i][j];}

/* -------------------------------------------------------------------------
-- Calculate clipping limits in "inches" first, but also in pixels
-- In full mode, 
-- (1) at least 0
-- (2) smaller of current setting (from above) and physical effective size
--
-- Note: Clipping is a 2D operation, not 3D.  This creates some real problems
--       since the mode changes all the time.
------------------------------------------------------------------------- */
/* Calculate clipping size in "inches" and in "pixels" */
	if (PlotWindow->clpmod == 1) {				/* Maximum clipping box */
/*		printf("Setting full size\n"); */
		PlotWindow->clpxl  = 0.0f;
		PlotWindow->clpyl  = 0.0f;
		PlotWindow->clpxh  = 1.0E20f;
		PlotWindow->clpyh  = 1.0E20f;
	} else if (PlotWindow->clpmod == 0) {		/* Box mode */
/*		printf("Setting reduced size\n"); */
		PlotWindow->clpxl = PlotWindow->xorg + PlotWindow->xmarg[0] ;
		PlotWindow->clpxh = PlotWindow->xorg + PlotWindow->xsize - PlotWindow->xmarg[1] ;
		PlotWindow->clpyl = PlotWindow->yorg + PlotWindow->ymarg[0] ;
		PlotWindow->clpyh = PlotWindow->yorg + PlotWindow->ysize - PlotWindow->ymarg[1] ;
	}

/* Stay in nominal units inch space of pseudo page, not full physical page */
/* Uses therefore PlotWindow->factr rather than full factr */
	PlotWindow->clpxl = max(0.0f, min(PL_Plot.xhigh/PlotWindow->factr, PlotWindow->clpxl));
	PlotWindow->clpxh = max(0.0f, min(PL_Plot.xhigh/PlotWindow->factr, PlotWindow->clpxh));
	PlotWindow->clpyl = max(0.0f, min(PL_Plot.yhigh/PlotWindow->factr, PlotWindow->clpyl));
	PlotWindow->clpyh = max(0.0f, min(PL_Plot.yhigh/PlotWindow->factr, PlotWindow->clpyh));

/* Convert clip window from inches to pixels. Subtle - clip window above in	*/
/* absolute inches w/o origin - origin is included in transforms elsewhere		*/
/* For 2D work, can use actual transform, for 3D, have to use "effective 2d"	*/
	xl = ip_2d[0][0]*(PlotWindow->clpxl-PlotWindow->xorg) + ip_2d[0][1]*(PlotWindow->clpyl-PlotWindow->yorg) + ip_2d[0][3];
	yl = ip_2d[1][0]*(PlotWindow->clpxl-PlotWindow->xorg) + ip_2d[1][1]*(PlotWindow->clpyl-PlotWindow->yorg) + ip_2d[1][3];
	xh = ip_2d[0][0]*(PlotWindow->clpxh-PlotWindow->xorg) + ip_2d[0][1]*(PlotWindow->clpyh-PlotWindow->yorg) + ip_2d[0][3];
	yh = ip_2d[1][0]*(PlotWindow->clpxh-PlotWindow->xorg) + ip_2d[1][1]*(PlotWindow->clpyh-PlotWindow->yorg) + ip_2d[1][3];

	PL_Plot.pclpxl = (REAL) max(0,            min(xl,xh));
	PL_Plot.pclpxh = (REAL) min(DEVICE->xmax, max(xl,xh));
	PL_Plot.pclpyl = (REAL) max(0,            min(yl,yh));
	PL_Plot.pclpyh = (REAL) min(DEVICE->ymax, max(yl,yh));

/* Pass some information on to the screen driver */
	clip.xl = (int) PL_Plot.pclpxl;
	clip.xh = (int) PL_Plot.pclpxh;
	clip.yl = (int) PL_Plot.pclpyl;
	clip.yh = (int) PL_Plot.pclpyh;
	(*DEVICE->dsptch)(TELLCLIP, DEVICE->DriverBlock, (DSP *) &clip);	/* Tell driver about clipping */

	PL_Plot.IsValid = TRUE;								/* Mark it as done */
	return;
}

/* ===========================================================================
-- Usage:       void MatMul3D(double result[4][4], double a[4][4], double b[4][4]);
--
-- Description: Multiplies matrix a*b returning result.
--
-- Inputs:      a,b - 3D matrices
--
-- Outputs:     result - result of multiplication
--
-- Returns:     void
--
-- Notes:       Simple matrix multiply
=========================================================================== */
static void MatMul3D(double c[4][4], double a[4][4], double b[4][4]) {

	double o[4][4];
	int i,j,k;
	
	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) {
			o[i][j] = 0;
			for (k=0; k<4; k++) 
				o[i][j] += a[i][k] * b[k][j];
		}
	}
	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) c[i][j] = o[i][j];
	}

	return;
}

/* ===========================================================================
-- Usage:       void MatInv3D(double result[4][4], double a[4][4]);
--
-- Description: Inverts a 4x4 matrix.
--
-- Inputs:      a - 3D matrices
--
-- Outputs:     result - result of inverse
--
-- Returns:     void
--
-- Notes:       Simple Gaussian elimination matrix inverse with pivoting
=========================================================================== */
static void MatInv3D(double c[4][4], double a[4][4]) {

	double o[4][8], tmp, amax;
	int i,j,k;
	
	for (i=0; i<4; i++) {							/* Set up [a][I] */
		for (j=0; j<4; j++) {
			o[i][j] = a[i][j];
			o[i][j+4] = (i==j) ? 1 : 0;
		}
	}

/* Make upper triangular */
	for (i=0; i<4; i++) {							/* Work down row at a time */
		amax = -1; k = -1;
		for (j=i; j<4; j++) {						/* Find largest element		*/
			if (fabs(o[j][i]) > amax) {amax = fabs(o[j][i]); k = j;}
		}
		if (k != i) {									/* Shall I pivot?				*/
			for (j=0; j<8; j++) {tmp = o[i][j]; o[i][j] = o[k][j]; o[k][j] = tmp;}
		}
		if ( (tmp = o[i][i]) != 1)
			for (j=i; j<8; j++) o[i][j] /= tmp;		/* Normalize diagonal to one	*/
		for (j=i+1; j<4; j++) {
			if ( (tmp = o[j][i]) != 0) 
				for (k=i; k<8; k++) o[j][k] -= tmp*o[i][k];
		}
	}
	
/* Now, back substitute to make identity */
	for (i=3; i; i--) {
		for (j=i-1; j>=0; j--) {
			if (o[j][i] != 0) {
				for (k=4; k<8; k++) o[j][k] -= o[j][i]*o[i][k];
				o[j][i] = 0;
			}
		}
	}

	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) c[i][j] = o[i][j+4];
	}

	{
		double r[4][4], err=0;
		MatMul3D(r, a, c);
		for (i=0; i<4; i++) {
			for (j=0; j<4; j++) err += fabs(r[i][j]-((i==j)?1:0));
		}
		if (err > 1E-7) TTYprintf("Matrix inversion error: %g\n", err);
	}
	return;
}


/* ===========================================================================
-- Usage:       void VecMul3D(double result[4], double a[4][4], double x[4]);
--
-- Description: Multiplies (matrix a)*(vector x) returning (vector result).
--
-- Inputs:      a - 3D matrices
--              x - column vector
--
-- Outputs:     result - result of multiplication
--
-- Returns:     void
--
-- Notes:       
=========================================================================== */
static void VecMul3D(double c[4], double a[4][4], double x[4]) {

	double o[4];
	int i,j;
	
	for (i=0; i<4; i++) {
		o[i] = 0;
		for (j=0; j<4; j++) o[i] += a[i][j] * x[j];
	}
	for (i=0; i<4; i++) c[i] = o[i];
	return;
}


/* ===========================================================================
-- Usage: void PlotConvert2DScales(int mode, REAL x, REAL y, REAL *xp, REAL *yp) {
--
-- Description: Converts between various modes of the coordinates
--
-- Inputs:      a,b - 3D matrices
--
-- Outputs:     result - result of multiplication
--
-- Returns:     void
--
-- Notes:       NORM  - normalized coordinate range (0-1 within plot area)
--              INCH  - quasi-inches of user (scaled)
--              USER  - user scales via PlotSetRange
--              PIXEL - absolute position from screen driver (pixels)
--
-- Routine handles internally setting of PlotWindow->usrnbl where inch and
-- user are synonymous units of measure.
=========================================================================== */
void PlotConvert2DScales(int mode, REAL x, REAL y, REAL *xp, REAL *yp) {

	double det;
	REAL xt, yt;									/* Temporary coordinates */

	if (! PlotWindow->usrnbl) {					/* User & inch are synonymous */
		if (mode == USER_TO_PIXEL) mode = INCH_TO_PIXEL;
		if (mode == PIXEL_TO_USER) mode = PIXEL_TO_INCH;
		if (mode == USER_TO_INCH)  mode = -1;	/* Do nothing */
		if (mode == INCH_TO_USER)  mode = -1;
	}

	switch (mode) {
		case -1:										/* Special no-conversion mode */
			xt = x;
			yt = y;
			break;
		case NORM_TO_INCH:						/* Convert [0-1] to inches posn */
			xt = PlotWindow->xmarg[0] + x*(PlotWindow->xsize-PlotWindow->xmarg[0]-PlotWindow->xmarg[1]);
			yt = PlotWindow->ymarg[0] + y*(PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]);
			break;
		case INCH_TO_NORM:
			xt = (x-PlotWindow->xmarg[0]) / (PlotWindow->xsize-PlotWindow->xmarg[0]-PlotWindow->xmarg[1]);
			yt = (y-PlotWindow->ymarg[0]) / (PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]);
			break;
		case USER_TO_INCH:
			xt = PL_Plot.ui[0][0]*x + PL_Plot.ui[0][1]*y + PL_Plot.ui[0][3];
			yt = PL_Plot.ui[1][0]*x + PL_Plot.ui[1][1]*y + PL_Plot.ui[1][3];
			break;
		case INCH_TO_USER:
			xt = PL_Plot.iu[0][0]*x + PL_Plot.iu[0][1]*y + PL_Plot.iu[0][3];
			yt = PL_Plot.iu[1][0]*x + PL_Plot.iu[1][1]*y + PL_Plot.iu[1][3];
			break;
		case USER_TO_PIXEL:
			xt = PL_Plot.up[0][0]*x + PL_Plot.up[0][1]*y + PL_Plot.up[0][3] + 0.5f;
			yt = PL_Plot.up[1][0]*x + PL_Plot.up[1][1]*y + PL_Plot.up[1][3] + 0.5f;
/*			xt = min( max(xt, PL_Plot.pclpxl), PL_Plot.pclpxh);
			yt = min( max(yt, PL_Plot.pclpyl), PL_Plot.pclpyh);
*/			break;
		case USER_TO_GRID:
			xt = PL_Plot.ug[0][0]*x + PL_Plot.ug[0][1]*y + PL_Plot.ug[0][3] + 0.5f;
			yt = PL_Plot.ug[1][0]*x + PL_Plot.ug[1][1]*y + PL_Plot.ug[1][3] + 0.5f;
			break;
		case INCH_TO_GRID:
			xt = PL_Plot.ig[0][0]*x + PL_Plot.ig[0][1]*y + PL_Plot.ig[0][3] + 0.5f;
			yt = PL_Plot.ig[1][0]*x + PL_Plot.ig[1][1]*y + PL_Plot.ig[1][3] + 0.5f;
			break;
		case PIXEL_TO_USER:
			x   = x-PL_Plot.up[0][3];
			y   = y-PL_Plot.up[1][3];
			det = PL_Plot.up[0][0]*PL_Plot.up[1][1] - PL_Plot.up[1][0]*PL_Plot.up[0][1];
			xt =  (REAL) ((x*PL_Plot.up[1][1]-y*PL_Plot.up[0][1]) / det);
			yt = (REAL) (-(x*PL_Plot.up[1][0]-y*PL_Plot.up[0][0]) / det);
			break;
		case INCH_TO_PIXEL:
			xt = PL_Plot.ip[0][0]*x + PL_Plot.ip[0][1]*y + PL_Plot.ip[0][3] + 0.5f;
			yt = PL_Plot.ip[1][0]*x + PL_Plot.ip[1][1]*y + PL_Plot.ip[1][3] + 0.5f;
/*			xt = min( max(xt, PL_Plot.pclpxl), PL_Plot.pclpxh);
			yt = min( max(yt, PL_Plot.pclpyl), PL_Plot.pclpyh);
*/			break;
		case PIXEL_TO_INCH:
			x   = x-PL_Plot.ip[0][3];
			y   = y-PL_Plot.ip[1][3];
			det = PL_Plot.ip[0][0]*PL_Plot.ip[1][1] - PL_Plot.ip[1][0]*PL_Plot.ip[0][1];
			xt =  (REAL) ((x*PL_Plot.ip[1][1]-y*PL_Plot.ip[0][1]) / det);
			yt =  (REAL) (-(x*PL_Plot.ip[1][0]-y*PL_Plot.ip[0][0]) / det);
			break;
		default:
			xt = yt = 0;
	}
	if (xp != NULL) *xp = xt;
	if (yp != NULL) *yp = yt;
	return;
}


/* ===========================================================================
-- Removed zt=0 for USER_TO_PIXEL and INCH_TO_PIXEL so you would have the
-- effective Z value even if not used.  (7/31/95)
=========================================================================== */
void PlotConvert3DScales(int mode, REAL x, REAL y, REAL z, REAL *xp, REAL *yp, REAL *zp) {
	
	REAL xt, yt, zt;							/* Temporary coordinates */

	if (! PlotWindow->mode_3d) {
		PlotConvert2DScales(mode, x,y, xp,yp);
		if (zp != NULL) *zp = z;
		return;
	}

	if (! PlotWindow->usrnbl) {					/* User & inch are synonymous */
		if (mode == USER_TO_PIXEL) mode = INCH_TO_PIXEL;
		if (mode == PIXEL_TO_USER) mode = PIXEL_TO_INCH;
		if (mode == USER_TO_INCH)  mode = -1;	/* Do nothing */
		if (mode == INCH_TO_USER)  mode = -1;
	}

	switch (mode) {
		case -1:										/* Special no-conversion mode */
			xt = x;
			yt = y;
			zt = z;
			break;
		case USER_TO_INCH:
			xt = PL_Plot.ui[0][0]*x + PL_Plot.ui[0][1]*y + PL_Plot.ui[0][2]*z + PL_Plot.ui[0][3];
			yt = PL_Plot.ui[1][0]*x + PL_Plot.ui[1][1]*y + PL_Plot.ui[1][2]*z + PL_Plot.ui[1][3];
			zt = PL_Plot.ui[2][0]*x + PL_Plot.ui[2][1]*y + PL_Plot.ui[2][2]*z + PL_Plot.ui[2][3];
			break;
		case INCH_TO_USER:
			xt = PL_Plot.iu[0][0]*x + PL_Plot.iu[0][1]*y + PL_Plot.iu[0][2]*z + PL_Plot.iu[0][3];
			yt = PL_Plot.iu[1][0]*x + PL_Plot.iu[1][1]*y + PL_Plot.iu[1][2]*z + PL_Plot.iu[1][3];
			zt = PL_Plot.iu[2][0]*x + PL_Plot.iu[2][1]*y + PL_Plot.iu[2][2]*z + PL_Plot.iu[2][3];
			break;
		case USER_TO_PIXEL:
			xt = PL_Plot.up[0][0]*x + PL_Plot.up[0][1]*y + PL_Plot.up[0][2]*z + PL_Plot.up[0][3] + 0.5f;
			yt = PL_Plot.up[1][0]*x + PL_Plot.up[1][1]*y + PL_Plot.up[1][2]*z + PL_Plot.up[1][3] + 0.5f;
/*			xt = min( max(xt, PL_Plot.pclpxl), PL_Plot.pclpxh);
			yt = min( max(yt, PL_Plot.pclpyl), PL_Plot.pclpyh);
*/			zt = PL_Plot.up[2][0]*x + PL_Plot.up[2][1]*y + PL_Plot.up[2][2]*z + PL_Plot.up[2][3] + 0.5f;
			break;
		case INCH_TO_PIXEL:
			xt = PL_Plot.ip[0][0]*x + PL_Plot.ip[0][1]*y + PL_Plot.ip[0][2]*z + PL_Plot.ip[0][3] + 0.5f;
			yt = PL_Plot.ip[1][0]*x + PL_Plot.ip[1][1]*y + PL_Plot.ip[1][2]*z + PL_Plot.ip[1][3] + 0.5f;
/*			xt = min( max(xt, PL_Plot.pclpxl), PL_Plot.pclpxh);
			yt = min( max(yt, PL_Plot.pclpyl), PL_Plot.pclpyh);
*/			zt = PL_Plot.ip[2][0]*x + PL_Plot.ip[2][1]*y + PL_Plot.ip[2][2]*z + PL_Plot.ip[2][3] + 0.5f;
			break;
		case USER_TO_GRID:
			xt = PL_Plot.ug[0][0]*x + PL_Plot.ug[0][1]*y + PL_Plot.ug[0][2]*z + PL_Plot.ug[0][3] + 0.5f;
			yt = PL_Plot.ug[1][0]*x + PL_Plot.ug[1][1]*y + PL_Plot.ug[1][2]*z + PL_Plot.ug[1][3] + 0.5f;
			zt = PL_Plot.ug[2][0]*x + PL_Plot.ug[2][1]*y + PL_Plot.ug[2][2]*z + PL_Plot.ug[2][3] + 0.5f;
			break;
		case INCH_TO_GRID:
			xt = PL_Plot.ig[0][0]*x + PL_Plot.ig[0][1]*y + PL_Plot.ig[0][2]*z + PL_Plot.ig[0][3] + 0.5f;
			yt = PL_Plot.ig[1][0]*x + PL_Plot.ig[1][1]*y + PL_Plot.ig[1][2]*z + PL_Plot.ig[1][3] + 0.5f;
			zt = PL_Plot.ig[2][0]*x + PL_Plot.ig[2][1]*y + PL_Plot.ig[2][2]*z + PL_Plot.ig[2][3] + 0.5f;
			break;
		default:
			xt = yt = zt = 0;
	}
	if (xp != NULL) *xp = xt;
	if (yp != NULL) *yp = yt;
	if (zp != NULL) *zp = zt;
	return;
}
