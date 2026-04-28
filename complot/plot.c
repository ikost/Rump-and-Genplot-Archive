/* plot.c - Vector drawing function */

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
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>

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

typedef struct _vec_stroke {
	REAL x1,y1;								/* Starting point */
	REAL x2,y2;								/* Ending   point */
} VECTOR;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL DoClip(VECTOR *u);
PRIVATE void exclude(VECTOR v);
PRIVATE void Plot3DVector(VECTOR *u);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
REAL Plot_Z_Default = 0.0f;					/* Default Z value on 2D plot */
REAL Plot_Z_Inch_Default=0.0f;				/* Use privately with axis.c	*/

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
	PRIVATE INTEGER	ex_nvect;							/* Number of vectors	*/
	PRIVATE REAL		ex_vlow[21],ex_vhigh[21];		/* Vector set			*/

/* =============================================================================
--     PlotMove     - Routine to do the basic plotting functions and movements
--     PlotMoveInch - Plot in inches rather than user movements
--     PlotMove3D   - Use full 3D plot capability
--
--     Usage: CALL PLOT    (X,Y,IPEN)
--	     CALL PLOTINCH(X,Y,IPEN)
--	     CALL PLOT3d  (X,Y,Z,IPEN)
--
--     Inputs: X,Y  - X,Y coordinates of point or line
--             IPEN - Pen control
--                  <0 - Re-origin to this point after this command.
--             IABS(IPEN) =
--                   1 => Pen status unchanged and movement made
--                   2 => Pen down with movement
--                   3 => Pen up   with movement
--                   4 => Draw a point at coordinate (point only - not symbol)
--
--     Notes: There are two conversion applied to each coordinate before
--            plotting.  The first converts the user coordinates to equivalent
--            inches.  The second then maps these inches on the plot device.
--               X(inches) = XOFF + XFACT * X(user)          User conversion
--               X(screen) = X(inches) + XORG                Re-origining
--               X(pixels) = X(screen) * XSCALE * FACTR      Actual pixels
--            Most of these routines work in X(inches) because of the dashes
--            which have repeat length related to inches.  The where command
--            corrects for the inches and converts it to X(user).
--
--   WARNING: NOTE THAT FACTR is now a true factor.  Plot will look exactly
--            the same except for shrinkage or expansion.  Clipping WILL NOT
--            change and user must beware.
--            ALSO - Re-origining is now relative to current plotting and not
--                   the absolute origin.  IE. The second time you call for
--                   a re-origin, it will occur where that point would have
--                   plotted.  THIS MAY CHANGE DEPENDING ON MY DISPOSITION.
--
--   January 8, 1984 - MOT
--     Corrected error in clipping routine.  Did not account for the
--     reorigining previously.
--   September 26, 1984 - MOT
--     Changed algorithm for dashed lines so only one clip operation
--     was necessary.  Dramatically increased speed with dashed lines.
--   September, 1986 - LRD
--     Rewrite to collapse all of the rescaling into a single set of
--     operations, which is more appropriate for what is effectively an
--     inner loop.  This requires a couple of combined scale factors to be
--     generated whenever anything changes.  See FIXPLT.C for that code.  
-- THIS WILL CAUSE CONFUSION IF PEOPLE ARE NOT AWAKE WHEN THEY MODIFY THIS CODE
--     Look carefully at FIXPLT and the places where it is called.
--     Completely deleted code for reorigining from PLOT.
--
--   October, 1986 - MOT
--     Moved pen change out of various modes.  Done once if not in mode 3.
--   Dec. 28, 1986 - MOT
--     Added code for FLIPXY.  Changed conversion of X,Y real into parm block
============================================================================= */
void PlotMove(REAL xp, REAL yp, INTEGER ipen) {

	REAL x,y;
	INTEGER i;
	static VECTOR vect;

	if (PlotExcludeCurve == NULL) {
		PlotMove3D(xp, yp, Plot_Z_Default, ipen);
		return;
	}

/* Draw with exclusion.  Only works ONLY for pen 2,3 or 4 operations */
	if ( (ipen == 1) || (ipen == 3) ) {				/* Just set new position */
		i = ipen;											/* with same pen value */
	} else if (ipen == 4) {								/* For point, do short vector */
		vect.x1 = xp;										/* Set up into vector space */
		vect.y1 = yp;
		vect.x2 = xp+.003f/PlotWindow->xfact;		/* Very small horizontal move */
		vect.y2 = yp;
		exclude(vect);										/* Exclude it */
		if ( (ex_nvect >= 1) && (ex_vlow[0] == 0.0f) )
			i = 4;											/* Either draw point  */
		else
			i = 3;											/* Or move w/ pen UP */
	} else {													/* Here if drawing vector */
		vect.x2 = xp;										/* Setup the vector */
		vect.y2 = yp;
		if (xp == vect.x1) vect.x2 += .003f/PlotWindow->xfact;
		exclude(vect);
		for (i=0; i<ex_nvect; i++) {
			if ( (i != 0) || (ex_vlow[i] != 0.0f) ) {
				x = vect.x1+ex_vlow[i]*(xp-vect.x1);
				y = vect.y1+ex_vlow[i]*(yp-vect.y1);
				PlotMove3D(x, y, Plot_Z_Default, 3);	/* Pen up! */
			}
			x = vect.x1+ex_vhigh[i]*(xp-vect.x1);
			y = vect.y1+ex_vhigh[i]*(yp-vect.y1);
         PlotMove3D(x, y, Plot_Z_Default, 2);		/* Pen down! */
		}
		if (ex_nvect == 0)
			i = 3;											/* Last one is pen up */
		else if (ex_vhigh[ex_nvect-1] != 1.0f)
			i = 3;
		else
			i = 0;
	}

	if (i != 0) PlotMove3D(xp, yp, Plot_Z_Default, i);	/* Make sure at edge */
	vect.x1 = xp;													/* Save for later		*/
	vect.y1 = yp;
	return;
}


/* =============================================================================
-- This routine is equivalent to PLOT routine above except that no exclusion
-- is ever performed.  All vectors are plotted unless clipped by the margins.
--
-- Usage: CALL PlotMove3D(x, y, z, pen)
--
-- Inputs: x,y,z (REAL*4) Pen position desired
--         ipen (INT*2) Pen operation (see above for explanation)
============================================================================= */
void PlotMove3D(REAL xp, REAL yp, REAL zp, INTEGER ipen) {

	static struct _dm2 {
		INTEGER mode;
		INTEGER nextfixmod;
		/*                     (lintyp=1)   (lintyp>1)		*/
		/*							  U,S   D,S    U,D    D,D		*/
		 } stable[16] = {     {1,1}, {2,2}, {1,3}, {3,4},	 /* 1  Pen unchanged */
									 {2,2}, {2,2}, {3,4}, {3,4},	 /* 2  Pen down      */
									 {1,1}, {1,1}, {1,3}, {1,3},	 /* 3  Pen up			*/
									 {4,1}, {4,1}, {4,3}, {4,3} }; /* 4  Point mode		*/

	LOGICAL	moved;
	union {										/* For dispatch to driver */
		DspLinfnc line;
		DspPntfnc point;
	} dsp;
	INTEGER	nxtpen;							/* Next pen value for DASHES */
	INTEGER  i,mode;							/* Random variables */
	UINT		ui;
	REAL		x,y,								/* Working coordinates */
				xs,ys,							/* Just useful */
				veclen;							/* Current vector length (DASHES) */
	static REAL xsave, ysave;				/* Need these to stick around */

	VECTOR u;									/* Current vector points to CLIP */

/* ... First thing is to store as HCOPY request */
	if (PlotWindow->Save.On) {
		if (! PlotWindow->mode_3d) {				/* Standard 2-D or 3-D case? */
			i = SV_PLOT;
			ui = 2;
		} else {
			i = SV_PLOT3D;
			ui = 3;
		}
		SV_PutCmd(i,1,ui,0);							/* Send PLOT command */
		SV_PutReal(xp);
		SV_PutReal(yp);
		if (PlotWindow->mode_3d) SV_PutReal(zp);
		SV_PutInt(ipen);
	}

	if (! PL_Plot.IsValid) PlotFixInternal();		/* Make sure valid coordinates */

/* ... Here we go, move request point to plotter space (still floating point)
       Scaled variables: XFP, XOFFP, YFP, YOFFP */
	if (! PlotWindow->mode_3d) {
		if (PlotWindow->usrnbl) {
			u.x2 = xp*PL_Plot.up[0][0] + yp*PL_Plot.up[0][1] + PL_Plot.up[0][3] + 0.5f;
			u.y2 = xp*PL_Plot.up[1][0] + yp*PL_Plot.up[1][1] + PL_Plot.up[1][3] + 0.5f;
		} else {
			u.x2 = xp*PL_Plot.ip[0][0] + yp*PL_Plot.ip[0][1] + PL_Plot.ip[0][3] + 0.5f;
			u.y2 = xp*PL_Plot.ip[1][0] + yp*PL_Plot.ip[1][1] + PL_Plot.ip[1][3] + 0.5f;
		}
	} else {
		if (PlotWindow->usrnbl) {
			u.x2 = xp*PL_Plot.up[0][0] + yp*PL_Plot.up[0][1] + zp*PL_Plot.up[0][2] + PL_Plot.up[0][3] + 0.5f;
			u.y2 = xp*PL_Plot.up[1][0] + yp*PL_Plot.up[1][1] + zp*PL_Plot.up[1][2] + PL_Plot.up[1][3] + 0.5f;
		} else {
			u.x2 = xp*PL_Plot.ip[0][0] + yp*PL_Plot.ip[0][1] + zp*PL_Plot.ip[0][2] + PL_Plot.ip[0][3] + 0.5f;
			u.y2 = xp*PL_Plot.ip[1][0] + yp*PL_Plot.ip[1][1] + zp*PL_Plot.ip[1][2] + PL_Plot.ip[1][3] + 0.5f;
		}
	}

	mode = 4 * (abs(ipen)-1) + PL_Plot.fixmod - 1;
	PL_Plot.fixmod = stable[mode].nextfixmod;

	switch (stable[mode].mode) {

/* ... Move with pen up */
		case 1:
			PlotWindow->xloc = u.x2;					/* Current location is here */
			PlotWindow->yloc = u.y2;
			break;


/* ... Move with pen down - solid line */
		case 2:
			u.x1 = PlotWindow->xloc;			/* XORG is current offset */
			u.y1 = PlotWindow->yloc;			/* True plotting position */
			PlotWindow->xloc = u.x2;			/* Where we will be (ignore clip) */
			PlotWindow->yloc = u.y2;

			if (DoClip(&u)) {						/* u equivalenced to X1, Y1, X2, Y2 */
				if (PlotWindow->mode_3d) {
					Plot3DVector(&u);
				} else {
					dsp.line.x1 = (INTEGER) u.x1;				/* Normal vector for DSPTCH */
					dsp.line.y1 = (INTEGER) u.y1;
					dsp.line.x2 = (INTEGER) u.x2;
					dsp.line.y2 = (INTEGER) u.y2;
					(*DEVICE->dsptch)(LINFNC, DEVICE->DriverBlock, (DSP *) &dsp);	/* Dispatch and draw */
				}
			}
			break;
			

/* ... Move with pen down - dashed lines */
		case 3:

/* ... Determine if we have moved and save values of X and Y */
			moved = (PlotWindow->xloc != xsave) || (PlotWindow->yloc != ysave);
			xs = u.x2;
			ys = u.y2;

/* ... First clip vector so only work with part on screen */
			u.x1 = PlotWindow->xloc;						/* XORG is current offset */
         u.y1 = PlotWindow->yloc;						/* True plotting position */
			if (DoClip(&u)) {									/* Skip out if nothing */
            PlotWindow->xloc = u.x1;					/* ONLY WORK IF ON SCREEN */
            PlotWindow->yloc = u.y1;
            x   = u.x2;										/* Saves lots of clipping */
            y   = u.y2;										/* Boy is this dangerous */

            veclen = (REAL) sqrt(((x-u.x1)*PL_Plot.patx)*((x-u.x1)*PL_Plot.patx) + ((y-u.y1)*PL_Plot.paty)*((y-u.y1)*PL_Plot.paty));
				if (moved) {										/* Check if pen moved */
               PlotWindow->iseg = PlotWindow->segbgn;	/* Pen had been moved!! */
					PlotWindow->seglen = (REAL) PL_seglns[PlotWindow->iseg];
					PlotWindow->dshpen = 2;						/* Local pen starts down */
				}
				xsave = x;											/* Place we were; */
				ysave = y;

/* ... Now repeat till we finish the length of the vector */
				while (veclen > 0.0f) {
					if (veclen > PlotWindow->seglen) {				/* Are we still too long? */
						x = PlotWindow->xloc + (xsave-PlotWindow->xloc)*PlotWindow->seglen/veclen;
						y = PlotWindow->yloc + (ysave-PlotWindow->yloc)*PlotWindow->seglen/veclen;
						veclen = veclen - PlotWindow->seglen;	/* Interpolate between */
						if (++(PlotWindow->iseg) > PlotWindow->segend) PlotWindow->iseg = PlotWindow->segbgn;
						PlotWindow->seglen = (REAL) PL_seglns[PlotWindow->iseg];
						nxtpen = 5 - PlotWindow->dshpen;
					} else {
						x = xsave;										/* Can do entire segment */
						y = ysave;
						PlotWindow->seglen = PlotWindow->seglen - veclen;	/* Decrement available length */
						veclen = 0.0f;									/* Vector is done */
						nxtpen = PlotWindow->dshpen;				/* No flip of state later */
					}
					if (PlotWindow->dshpen == 2) {				/* Do we need to plot? */
						dsp.line.x1 = (INTEGER) PlotWindow->xloc;	/* Normal vector for DSPTCH */
						dsp.line.y1 = (INTEGER) PlotWindow->yloc;
						dsp.line.x2 = (INTEGER) x;
						dsp.line.y2 = (INTEGER) y;
						(*DEVICE->dsptch)(LINFNC, DEVICE->DriverBlock, (DSP *) &dsp);	/* Dispatch, draw vector */
					}
					PlotWindow->xloc = x;							/* Where we are now */
					PlotWindow->yloc = y;
					PlotWindow->dshpen = nxtpen;					/* Flip state usually */
				}
			}

/* ... All done, just set where we should be */
         PlotWindow->xloc = xsave = xs ;
         PlotWindow->yloc = ysave = ys ;
			break;

/* ... Pen request for a point only */
		case 4:
			if ( (u.x2 >= PL_Plot.pclpxl) && (u.x2 <= PL_Plot.pclpxh) &&
				  (u.y2 >= PL_Plot.pclpyl) && (u.y2 <= PL_Plot.pclpyh) ) {
				dsp.point.x = (INTEGER) u.x2;
				dsp.point.y = (INTEGER) u.y2;
				(*DEVICE->dsptch)(PNTFNC, DEVICE->DriverBlock, (DSP *) &dsp);
			}
			break;
	}
	return;
}


/* ===========================================================================
--   Routine to plot via inches rather than coordinate space    
=========================================================================== */
void PlotMoveInch(REAL xp, REAL yp, INTEGER ipen) {
	PlotMove3DInch(xp, yp, Plot_Z_Inch_Default, ipen);
	return;
}

/* ---------------------------------------- */
void PlotMove3DInch(REAL xp, REAL yp, REAL zp, INTEGER ipen) {

	REAL x,y,z;

	if (! PlotWindow->mode_3d) {
		if (PlotWindow->usrnbl) {
			x = PL_Plot.iu[0][0]*xp + PL_Plot.iu[0][1]*yp + PL_Plot.iu[0][3];
			y = PL_Plot.iu[1][0]*xp + PL_Plot.iu[1][1]*yp + PL_Plot.iu[1][3];
		} else {
			x = xp; y = yp;
		}
		PlotMove( x, y, ipen);
	} else {
		if (PlotWindow->usrnbl) {
			x = PL_Plot.iu[0][0]*xp + PL_Plot.iu[0][1]*yp + PL_Plot.iu[0][2]*zp + PL_Plot.iu[0][3];
			y = PL_Plot.iu[1][0]*xp + PL_Plot.iu[1][1]*yp + PL_Plot.iu[1][2]*zp + PL_Plot.iu[1][3];
			z = PL_Plot.iu[2][0]*xp + PL_Plot.iu[2][1]*yp + PL_Plot.iu[2][2]*zp + PL_Plot.iu[2][3];
		} else {
			x = xp; y = yp, z=zp;
		}
		PlotMove3D(x, y, z, ipen);
	}
	return;
}


/* ============================================================================
--     CLIP - Routine to clip given segment to fit within bounds specified
--
--     Usage: LOG = CLIP(U)
--
--     Inputs: U(1),U(2) - X,Y origin of vector
--             U(3),U(4) - X,Y end of vector
--
--     Output: U(1),U(2) - Origin of vector within plot bounds
--             U(3),U(4) - End    of vector within plot bounds
--             CLIP  - TRUE if any of vector is with plot bounds.
--                     FALSE if all outside of bounds.
--
--     Notes: This routine takes the two endpoints and returns endpoints which
--            lie within a box defined by (CLPXL,CLPYL) and (CLPXH,XLPYH).
--            This is an absolute pain but makes pretty graphs.
============================================================================= */
LOGICAL PlotInBounds(REAL xp, REAL yp, REAL zp) {

	REAL pxl_x, pxl_y, pxl_z;

/* Calculate the pixel measures and compare to clipping */
	PlotConvert3DScales(USER_TO_PIXEL, xp, yp, zp, &pxl_x, &pxl_y, &pxl_z);

/* Be a little conservative */
	return(	(pxl_x >= PL_Plot.pclpxl-1) && (pxl_x <= PL_Plot.pclpxh+1) &&
				(pxl_y >= PL_Plot.pclpyl-1) && (pxl_y <= PL_Plot.pclpyh+1) );
}

/* ===========================================================================
=========================================================================== */
PRIVATE LOGICAL DoClip(VECTOR *u) {

	REAL tmp;
	LOGICAL swap=FALSE;							/* Assume points not swapped	*/
	LOGICAL both=FALSE;							/* Both are not initially out */
	INTEGER code1=0, code2=0;

#define LEFT  0x01					/* Bit fields for position */
#define RIGHT 0x02
#define BELOW 0x04
#define ABOVE 0x08

/* ... Determine which are out and how much out.  Get a word with bits set as
   ... follows.  The center region is OKAY-DOKEY.  Ie. 1000 is above and center

      0110 | 0010 | 1010         0001 => below   0101 => left  below
      -----|------|-----         0010 => above   0110 => left  above
      0100 | 0000 | 1000         0100 => left    1001 => right below
      -----|------|-----         1000 => right   1010 => right above
      0101 | 0001 | 1001
*/

	if (u->x1 < PL_Plot.pclpxl)					/* Checks X variables */
		code1 |= LEFT;
	else if (u->x1 > PL_Plot.pclpxh) 
		code1 |= RIGHT;

	if (u->y1 < PL_Plot.pclpyl)					/* Now check the Y var */
		code1 |= BELOW;
	else if (u->y1 > PL_Plot.pclpyh) 
		code1 |= ABOVE;

	if (u->x2 < PL_Plot.pclpxl)               /* Checks X variables */
		code2 |= LEFT;
	else if (u->x2 > PL_Plot.pclpxh) 
		code2 |= RIGHT;

	if (u->y2 < PL_Plot.pclpyl)					/* Now check the Y var */
		code2 |= BELOW;
	else if (u->y2 > PL_Plot.pclpyh) 
		code2 |= ABOVE;

	if ( (code1 | code2) == 0) {
		return(TRUE);										/* Both OKAY => return */
	} else if (code1 == 0) {
		swap = FALSE;
	} else if (code2 == 0) {
		swap = TRUE;										/* Pretend second still out */
		tmp  = u->x1;
		u->x1 = u->x2;										/* Just switch coordinates */
		u->x2 = tmp;
		tmp  = u->y1;
		u->y1 = u->y2;
		u->y2 = tmp;
		code2 = code1;
		
/* ... Real pain if both initially out.  It may still intersect somewhere.
   ... If both high or low, no chance.  If a chance, try to clip the one
   ... point and see if we get anything valid.  Check with BOTH flag at end. */
	} else {													/* Here is bad stuff for */
		if ( (code1 & code2) != 0) return(FALSE);	/* Check if both out same area ==> no chance */
		both = TRUE;										/* Check after attempt clip */
	}

/* ... If we got here, second point is outside of the bounds, take care of it
   ... For speed, make it simple linear routine of all cases. */

	while (TRUE) {

		if (code2 & ABOVE) {								/* Above the axis - clip */
			if (u->y1 == u->y2) return(FALSE);		/* Added 7/1/86 LRD */
			u->x2 = u->x1 + (u->x2 - u->x1) * (u->y1 - PL_Plot.pclpyh) / (u->y1 - u->y2);
         u->y2 = PL_Plot.pclpyh;
		} else if (code2 & BELOW) {					/* Below the box - clip */
			if (u->y1 == u->y2) return(FALSE);		/* Added 7/1/86 LRD */
			u->x2 = u->x1 + (u->x2 - u->x1) * (u->y1 - PL_Plot.pclpyl) / (u->y1 - u->y2);
         u->y2 = PL_Plot.pclpyl;
		}

/* ... Now, check X coordinate.  It may have changed from above clip already. */
		if (u->x2 < PL_Plot.pclpxl) {							/* New code for point */
			if (u->x1 == u->x2) return(FALSE);			/* Added 7/1/86 LRD */
			u->y2 = u->y1 + (u->y2 - u->y1) * (u->x1 - PL_Plot.pclpxl) / (u->x1 - u->x2);
			u->x2 = PL_Plot.pclpxl;
		} else if (u->x2 > PL_Plot.pclpxh) {
			if (u->x1 == u->x2) return(FALSE);			/* Added 7/1/86 LRD */
			u->y2 = u->y1 + (u->y2 - u->y1) * (u->x1 - PL_Plot.pclpxh) / (u->x1 - u->x2);
         u->x2 = PL_Plot.pclpxh;
		}

/* Reverse order back if okay */
      if (swap || both) {									/* Restore original order */
         tmp  = u->x1;										/* Or switch for BOTH mode */
			u->x1 = u->x2;
			u->x2 = tmp;
         tmp  = u->y1;
         u->y1 = u->y2;
         u->y2 = tmp;
		}

/* Unless both were out, something is now valid and we can return */
		if (! both) return(TRUE);							/* We have succeeded */

/* ... For both, have to continue with the next now */
		if ((u->y1 < PL_Plot.pclpyl) || (u->y1 > PL_Plot.pclpyh)) return(FALSE);	/*  Not successful */
		swap = TRUE;										/* Switch points next time again */
		both = FALSE;
		code2 = code1;
	}

	panic; return(TRUE);									/* BETTER NOT HAPPEN! */
}

/* =============================================================================
-- Usage: CALL EXCLUDE
--
-- Inputs: common blocks
--
-- Output: common blocks
============================================================================= */
PRIVATE void exclude(VECTOR v) {

	INT		i,j,nclip;
	REAL		x,y,x0,xmin,xmax,ymin,ymax;
	REAL		h,m,b,v1,hcrit,dxpl,x1pl,x2pl;

	if (PlotWindow->usrnbl) {						/* Currently enabled?? */
		v.x1 = v.x1*PlotWindow->xfact + PlotWindow->xoff;
		v.x2 = v.x2*PlotWindow->xfact + PlotWindow->xoff;
		v.y1 = v.y1*PlotWindow->yfact + PlotWindow->yoff;
		v.y2 = v.y2*PlotWindow->yfact + PlotWindow->yoff;
	} 

	m    = (v.y2-v.y1)/(v.x2-v.x1);				/* Now have y = mx+b real */
	b    = v.y1-m*v.x1;
	v1   = 1+m*m;										/* Commonly used value */

	x =  (REAL) fabs(PlotExcludeRadius);
	xmin = min(v.x1,v.x2)-x;
	xmax = max(v.x1,v.x2)+x;
	ymin = min(v.y1,v.y2)-x;
	ymax = max(v.y1,v.y2)+x;
	hcrit = (REAL) (x*sqrt(v1));					/* Critical h value */

	nclip = 0;											/* No clips yet! */
	for (i=0; i<PlotExcludeCurve->npt; i++) {
		x = PlotExcludeCurve->x[i];
		y = PlotExcludeCurve->y[i];
		x = x*PlotWindow->xfact + PlotWindow->xoff;	/* Work in INCHES! */
		y = y*PlotWindow->yfact + PlotWindow->yoff;
		if ( (x<=xmin) || (x>=xmax) || (y<=ymin) || (y>=ymax) ) continue;
		h = y-m*x-b;										/* Part of distance calc */
		if (fabs(h) >= hcrit) continue;				/* This one okay also! */

		x2pl = m*h;
		dxpl = x2pl*x2pl+(PlotExcludeRadius*PlotExcludeRadius-h*h)*v1;	/* Discriminate */
		if (dxpl <= 0) continue;
		dxpl = (REAL) sqrt(dxpl);						/* +/- value */
		x1pl = (x2pl+dxpl) / v1;
		x2pl = (x2pl-dxpl) / v1;						/* And the other */
		x1pl = (x1pl-v.x1+x)/(v.x2-v.x1);			/* Fraction of line to draw! */
		x2pl = (x2pl-v.x1+x)/(v.x2-v.x1);
		nclip = min(20, nclip+1);						/* Element in ex_vlow (start 2) */
		ex_vlow[nclip]  = min(x1pl,x2pl);
		ex_vhigh[nclip] = max(x1pl,x2pl);
	}

/* ... Sort the clip list by increasing lower bound (trivial bubble sort) */

	i = 2;
	while (i <= nclip) {
		if (ex_vlow[i]   < ex_vlow[i-1]) {
			x0				  = ex_vlow[i-1];
			ex_vlow[i-1]  = ex_vlow[i];
			ex_vlow[i]    = x0;
			x0            = ex_vhigh[i-1];
			ex_vhigh[i-1] = ex_vhigh[i];
			ex_vhigh[i]   = x0;
			i = 2;											/* Resart with next one */
		} else
			i++;
	}

/* ... Now, convert into the plot vectors (invert the clip stuff) */
	i = 0;
	ex_vlow[0] = 0.0f;								/* Starting high end of clip	*/
	for (j=1; j<=nclip; j++) {
		if (ex_vlow[j] > ex_vlow[i]) {		/* Will terminate the vector	*/
			if (ex_vlow[j] >= 1.0f) break;		/* Exit?								*/
			ex_vhigh[i++] = ex_vlow[j];
		}
		ex_vlow[i] = max(ex_vlow[i],ex_vhigh[j]);
	}
		
	if (ex_vlow[i] < 1.0f) ex_vhigh[i++] = 1.0f;		/* Terminate the vector */
	ex_nvect = i;
	
/*
d      write (0,*) 'ex_nvect: ',ex_nvect
d      do 99 i=1,ex_nvect
d99       write (0,*) 'low high: ',ex_vlow(i),ex_vhigh(i)
*/
	return;
}

/* ===========================================================================
-- Usage:       
--
-- Description: 
--
-- Inputs:      
--
-- Outputs:      
--
-- Returns:     
--
-- Notes:       
=========================================================================== */
PRIVATE void Plot3DVector(VECTOR *u) {

	DspLinfnc line;

	line.x1 = (INTEGER) u->x1;				/* Normal vector for DSPTCH */
	line.y1 = (INTEGER) u->y1;
	line.x2 = (INTEGER) u->x2;
	line.y2 = (INTEGER) u->y2;
	(*DEVICE->dsptch)(LINFNC, DEVICE->DriverBlock, (DSP *) &line);	/* Dispatch and draw */
	return;
}

#ifdef JUNK

	static short *buf;
	static int bufsize=0;
	int x1,x2,y1,y2;

	if (PlotWindow->hidden_3d) {

	if (buf == NULL || bufsize < PL_Plot.xhigh) {
		if (buf != NULL) free(buf);
		bufsize = PL_Plot.xhigh;						/* One point for each pixel */
		if ( (buf = calloc(bufsize, sizeof(*buf)) == NULL) panic;
	}

	if (u->x1 > u->x2) {
		x1 = u->x1; x2 = u->x2;
		y1 = u->y1; y2 = u->y2;
	} else {
		x1 = u->x2; x2 = u->x1;
		y1 = u->y2; y2 = u->y1;
	}
	if (below) {y1 = -y1; y2 = -y2;}

	if (x1 == x2) {
		if (y1 < buf[x1] && y2 < buf[x1]) return(0);
		y1 = min(y1, buf[x1]);
		y2 = min(y2, buf[x1]);
		buf[x1] = max(y1, y2);
		line(x1,y1, x2,y2);
	} else {
		mode = OFF;
		dy = ((y2-y1)*16384 + 8191) / (x2-x1);
		y =  y1 * 16384;
		for (x=x1; x<=x2; x++,y+=dy) {
			iy = (dy+8191) / 16384;
			if (iy >= buf[x]) {
				buf[x1] = iy;
				if (mode = OFF) {
					xstart = x;
					ystart = iy;
					mode = ON;
				}
				continue;
			} else if (mode == ON) {
				line(xstart, ystart, x-1, ylast);
				line(x-1, ylast, x, buf[x]);
				mode == OFF;
			}
			ylast = iy;
		}
		if (mode == ON) line(xstart, ystart, x-1, ylast);
	}
	return;
}
#endif
