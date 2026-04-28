/* gptaxis.c */

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
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	LEXESCAPE		if (LexEscape(TRUE)) return(OKAY)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static LOGICAL YouDraw(int key, REAL xp, REAL yp, REAL xlp, int ixm);
void make_label_structure(GPT_AXIS_LABELS *user, PLOT_AXIS_LABELS *labels);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
void GptSetRange(void);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
-- Routine to draw the axes assuming that GptCurve and all parameters are
-- properly initialized.
--
-- Inputs: none
--
-- Output: none
--------------------------------------------------------------------------- */
void GptDrawAxes(void) {
	
	LOGICAL ytype, independent;
	int key, key2, colhld, i,j, mx, ixm, ichk;
	INTEGER color;
	REAL t1,t2,dx,dx2,xlp,xp,yp;
	REAL xmin,xmax,ymin,ymax, zmin,zmax;
	REAL *x,*y, umin,umax;
	CHAR title[LONG_STR_SIZE];
	PLOT_AXIS_LABELS *paxis_labels, axis_labels;

	if (Gpt->mode_3d) {
		GptDraw3DAxes();
		return;
	}

	PlotInformAxesLimits(-1);				/* Possibly color inner region */

	xmin = ymin = zmin = 0;
	xmax = ymax = zmax = 1;
	if (GptCurve != NULL) {								/* Working with curve */
		ArrayMinMax(GptCurve->x, GptCurve->npt, &xmin, &xmax);	/* Okay? */
		ArrayMinMax(GptCurve->y, GptCurve->npt, &ymin, &ymax);
		if (GptCurve->z != NULL) 
			ArrayMinMax(GptCurve->z, GptCurve->npt, &zmin, &zmax);
	} else if (GptSurface != NULL) {
		ArrayMinMax(GptSurface->x, GptSurface->ncol, &xmin, &xmax);
		ArrayMinMax(GptSurface->y, GptSurface->nrow, &ymin, &ymax);
		ArrayMinMax(GptSurface->z, GptSurface->npt,  &zmin, &zmax);
	}

	PlotID(0, 0, 0, 0, "**RESET**");					/* Reset IDS command */
	Gpt->linetype = Gpt->linetypestart;				/* Reset line type */
	Gpt->symtype  = Gpt->symtypestart;				/* Reset symbol type */
	colhld   = PlotSelectPen(1);						/* Use pen 1 here		*/

/* Step 0 - autoscale Z if necessary (to address bitmap) */
	if (GptSurface != NULL || GptCurve->z != NULL) {
		key = ZAXIS;

		if ((Gpt->AutoFlag) & (0x001<<key)) {			/* Use autoscaled values */
			Gpt->rmins[key] = zmin;
			Gpt->rmaxs[key] = zmax;
		} else {
			zmin = Gpt->rmins[key];
			zmax = Gpt->rmaxs[key];
		}

		if (! (Gpt->ForceRegions & (0x01<<key))) {	/* If not forced, make "nice" values */
			if (Gpt->logtype[key]) {
				PlotAutoLogScale(zmin, zmax, &t1, &t2, &dx, &dx2, &mx);
			} else {
				PlotAutoScale(zmin, zmax, &t1, &t2, &dx, &dx2, &mx);
			}
			Gpt->rmins[key] = t1;
			Gpt->rmaxs[key] = t2;
		}
	}

/* Scan 1 -- Deal with any axis potentially in "Window" mode */
	for (key=0; key<4; key++) {						/* Loop through axes looking only for windowed */
		if (! (Gpt->AutoFlag & (0x100<<key)) ) continue;
		if ( (key == TOP)   && (Gpt->xtop   != IS_ON) ) continue;
		if ( (key == RIGHT) && (Gpt->yright != IS_ON) ) continue;
		ytype = (key == LEFT) || (key == RIGHT);

		key2 = ytype ? ((Gpt->BoxMode) ? 2*Gpt->plx : BOTTOM) : ((Gpt->BoxMode) ? 2*Gpt->ply+1 : LEFT) ;
		if ( (key2 == TOP)   && (Gpt->xtop != IS_ON) ) key2 = BOTTOM;
		if ( (key2 == RIGHT) && (Gpt->xtop != IS_ON) ) key2 = LEFT;

		if (Gpt->AutoFlag & (0x001<<key2)) {		/* Is other axis autoscaled */
			t1 = ytype ? xmin : ymin;
			t2 = ytype ? xmax : ymax;
		} else {
			t1 = Gpt->rmins[key2];
			t2 = Gpt->rmaxs[key2];
		}
		if (!(Gpt->ForceRegions & (0x01<<key2))) { /* If not forced, make nice */
			if (Gpt->logtype[key2])
				PlotAutoLogScale(t1, t2, &t1, &t2, &dx, &dx2, &mx);
			else
				PlotAutoScale(t1, t2, &t1, &t2, &dx, &dx2, &mx);
		}

		if (t1 > t2) {dx=t1; t1=t2; t2=dx;}
		if (GptCurve != NULL) {
			x = ytype ? GptCurve->x : GptCurve->y;
			y = ytype ? GptCurve->y : GptCurve->x;
			umin = +REAL_MAX; umax = -REAL_MAX;
			for (i=0; i<GptCurve->npt; i++) {
				if (x[i] >= t1 && x[i] <= t2) {
					if (y[i] < umin) umin = y[i];
					if (y[i] > umax) umax = y[i];
				}
			}
		} else {
			umin = ytype ? ymin : xmin;
			umax = ytype ? ymax : xmax;
		}

		if (umin > umax) {										/* This is essentially no data */
			Gpt->rmins[key] = ytype ? ymin : xmin;
			Gpt->rmaxs[key] = ytype ? ymax : xmax;
		} else {														/* Normal condition */
			Gpt->rmins[key] = umin;
			Gpt->rmaxs[key] = umax;
		}
	}

/* Scan 2, now do the axes for real -- Limits on WINDOW mode already set */
	for (key=0; key<4; key++) {						/* Loop through all axes */
		ytype = (key == LEFT) || (key == RIGHT);
		key2  = key;
		if ( (key == TOP)   && (Gpt->xtop   != IS_ON) ) key2 = BOTTOM;
		if ( (key == RIGHT) && (Gpt->yright != IS_ON) ) key2 = LEFT;
		independent = (key == key2);

		/* If 0, will use defaults */
		color = Gpt->color[key];						/* Set the color to use */

/* ... If autoscaling, set values in place */
		if (Gpt->AutoFlag & (0x001<<key) ) {
			if (ytype) {
				Gpt->rmins[key] = ymin;
				Gpt->rmaxs[key] = ymax;
			} else {
				Gpt->rmins[key] = xmin;
				Gpt->rmaxs[key] = xmax;
			}
		}
		/* Determine the tick mark and labeling values */
		if (Gpt->logtype[key2]) { 							/* Logarithmic? */
			PlotAutoLogScale(Gpt->rmins[key2], Gpt->rmaxs[key2], &t1, &t2, &dx, &dx2, &mx);
		} else {
			PlotAutoScale(Gpt->rmins[key2], Gpt->rmaxs[key2], &t1, &t2, &dx, &dx2, &mx);
		}

		if (independent) {								/* Independent axis?			*/
			if (Gpt->ForceRegions & (0x01<<key)) { /* Forced or nice				*/
				t1 = Gpt->rmins[key];					/* Forced, use exact			*/
				t2 = Gpt->rmaxs[key];
			} else {
				Gpt->rmins[key] = t1;					/* Reset to be nice values */
				Gpt->rmaxs[key] = t2;
			}
		} else {												/* Copy if not */
			Gpt->rmins[key] = t1 = Gpt->rmins[key2];	/* Use previous values */
			Gpt->rmaxs[key] = t2 = Gpt->rmaxs[key2];	/* Use previous values */
		}

		if (Gpt->udx[key]  != 0.0f) {					/* Overrides? */
			dx  = Gpt->udx[key];
		} else if (Gpt->udx[key2] != 0.0f) {		/* Or on other axis? */
			dx  = Gpt->udx[key2];
		}

		if (Gpt->udx2[key] != 0.0f) {					/* Minor tick spacing */
			dx2 = Gpt->udx2[key];
		} else if (Gpt->udx2[key2] != 0.0f) {
			dx2 = Gpt->udx2[key2];
		}

		if (Gpt->umx[key]  != 0) {						/* And labelling modes */
			mx  = Gpt->umx[key];
		} else if (Gpt->umx[key2] != 0) {
			mx  = Gpt->umx[key2];
		}

		if (t1 == t2) {
			if (t1 < 0)			{t1 = 2*t1; t2 = 0;}
			else if (t1 == 0)	{t1 = -1.0; t2 = 1.0;}
			else					{t1 = 0.0;  t2 = 2*t2;}
		}
		PlotSetRange(t1, t2, t1, t2);					/* Both sets same for now */
		xp = t1 + (t2-t1)*Gpt->uxs[key];				/* Starting point		*/
		yp = t1 + (t2-t1)*Gpt->uys[key];				/* And Y value			*/
		xlp = Gpt->uxl[key]*(t2-t1);					/* Length of axis		*/
		if (! Gpt->BoxMode) {							/* Old crossing axes	*/
			PlotSetRange(Gpt->rmins[0], Gpt->rmaxs[0], Gpt->rmins[1], Gpt->rmaxs[1]);
			if (ytype) { 
				xp = 0.0f;
			} else {
				yp = 0.0f;
			}
		}

/* Now, continue to get the titles, etc. */
		strscpy(title, Gpt->titles[key2], sizeof(title));
		j = max(1, (int) strnblen(title));
		
		ixm = Gpt->axmode[key] & ~(AXIS_Y_TYPE);		/* Axis mode info */
		if (ytype) ixm |= AXIS_Y_TYPE;					/* And make X,Y type okay */

		if (key == TOP || key == RIGHT) {				/* Special cases */
			ichk = (key == TOP) ? Gpt->xtop : Gpt->yright;
			if (! Gpt->BoxMode) {
				continue;
			} else if (ichk == IS_NONLINEAR) {
				YouDraw(key, xp,yp, xlp, ixm);
				continue;
			} else if (ichk == IS_OFF) {
				*title = '\0';
				j = -1;
				ixm |= AXIS_NO_TICK_LABELS;
			} else {
				j = -j;										/* Put title on other side */
			}
		}

/* Handle possible user labels declarations */
		if (Gpt->do_user_labels[key2]) {
			paxis_labels = &axis_labels;
			make_label_structure(&Gpt->user_labels[key2], paxis_labels);
		} else {
			paxis_labels = NULL;
		}

/* And finally just do the axis */
		if (Gpt->logtype[key2]) {
			PlotLogAxis(xp, yp, ixm, xlp, dx, dx2, title, j, mx, color, paxis_labels);
		} else {
			PlotAxis(xp, yp, ixm, xlp, dx, dx2, title, j, mx, color, paxis_labels);
		}
	}											/* end for loop */

	PlotFlush();							/* Flush graphics modes */
	PlotSelectPen(colhld);				/* Restore pen				*/
	return;
}


/* ===========================================================================
=========================================================================== */
void GptDraw3DAxes(void) {
	
	int key, colhld, i,j, mx, ixm;
	static REAL zxy[3][3] = { {0,1,0}, {0,0,1}, {1,0,0} };
	static REAL zyx[3][3] = { {0,0,1}, {0,1,0}, {1,0,0} };
	static REAL yxz[3][3] = { {0,1,0}, {1,0,0}, {0,0,1} };
	INTEGER color;
	REAL t1,t2,dx,dx2,xlp,xp, yp,zp;
	REAL xp0,yp0, xp1,yp1, xp2,yp2, xp3,yp3;
	REAL xmin,xmax, ymin,ymax, zmin,zmax;
	CHAR title[LONG_STR_SIZE];

	xmin = ymin = zmin = 0;
	xmax = ymax = zmax = 1;
	if (GptCurve != NULL) {								/* Working with curve */
		ArrayMinMax(GptCurve->x, GptCurve->npt, &xmin, &xmax);	/* Okay? */
		ArrayMinMax(GptCurve->y, GptCurve->npt, &ymin, &ymax);
		if (GptCurve->z != NULL) 
			ArrayMinMax(GptCurve->z, GptCurve->npt, &zmin, &zmax);
	} else if (GptSurface != NULL) {
		ArrayMinMax(GptSurface->x, GptSurface->ncol, &xmin, &xmax);
		ArrayMinMax(GptSurface->y, GptSurface->nrow, &ymin, &ymax);
		ArrayMinMax(GptSurface->z, GptSurface->npt,  &zmin, &zmax);
	}

	PlotID(0, 0, 0, 0, "**RESET**");					/* Reset IDS command */
	Gpt->linetype = Gpt->linetypestart;				/* Reset line type */
	Gpt->symtype  = Gpt->symtypestart;				/* Reset symbol type */
	colhld   = PlotSelectPen(1);						/* Use pen 1 here		*/

	PlotSet3DView(Gpt->view_d, Gpt->tilt, Gpt->skew, Gpt->rotate);

/* Autoscale if necessary */
	for (i=0; i<3; i++) {								/* The three axes */
		if (i == 0) {
			key = BOTTOM;
			xmin = xmin; xmax = xmax;
		} else if (i == 1) {
			key = LEFT;
			xmin = ymin; xmax = ymax;
		} else {
			key = ZAXIS;
			xmin = zmin; xmax = zmax;
		}

		/* If 0, will use defaults */
		color = Gpt->color[key];							/* Set the color to use */

		if ((Gpt->AutoFlag) & (0x001<<key)) {			/* Use autoscaled values */
			Gpt->rmins[key] = xmin;
			Gpt->rmaxs[key] = xmax;
		} else {
			xmin = Gpt->rmins[key];
			xmax = Gpt->rmaxs[key];
		}

		if (Gpt->logtype[key]) {
			PlotAutoLogScale(xmin, xmax, &t1, &t2, &dx, &dx2, &mx);
		} else {
			PlotAutoScale(xmin, xmax, &t1, &t2, &dx, &dx2, &mx);
		}

		if (Gpt->ForceRegions & (0x01<<key)) {
			xmin = t1 = Gpt->rmins[key];
			xmax = t2 = Gpt->rmaxs[key];
		} else {
			Gpt->rmins[key] = xmin = t1;
			Gpt->rmaxs[key] = xmax = t2;
		}

		if (Gpt->udx[key]  != 0.0)	dx  = Gpt->udx[key];		/* Overrrides */
		if (Gpt->udx2[key] != 0.0)	dx2 = Gpt->udx2[key];
		if (Gpt->umx[key]  != 0)	mx  = Gpt->umx[key];

		if (key == LEFT) {
			Plot3DTransform(1, yxz, NULL);
		} else if (key == ZAXIS) {
			xp0 = (REAL) fabs(Gpt->rotate);	while (xp0 > 90) xp0 -= 180;
			Plot3DTransform(1, (fabs(xp0) < 45) ? zxy : zyx, NULL);
		}
		PlotSet3DRange(t1, t2, 0.0, 1.0, 0.0, 1.0);	/* 3D mode with 0-1 range */

/* Decide which of the 4 possible axes should be drawn - always do "bottom"
-- for X and Y, and "left" for the Z-axis */
		yp = zp = 0;
		PlotConvert3DScales(USER_TO_GRID, t1, 0.0, 0.0, &xp0, &yp0, NULL);
		PlotConvert3DScales(USER_TO_GRID, t1, 1.0, 0.0, &xp1, &yp1, NULL);
		PlotConvert3DScales(USER_TO_GRID, t1, 0.0, 1.0, &xp2, &yp2, NULL);
		PlotConvert3DScales(USER_TO_GRID, t1, 1.0, 1.0, &xp3, &yp3, NULL);
		if (key != ZAXIS) {
			if (yp3 <= yp2 && yp3 <= yp1 && yp3 <= yp0) {
				yp = zp = 1;
			} else if (yp2 <= yp3 && yp2 <= yp1 && yp2 <= yp0) {
				zp = 1;
			} else if (yp1 <= yp3 && yp1 <= yp2 && yp1 <= yp0) {
				yp = 1;
			}
		} else {
			if (xp3 <= xp2 && xp3 <= xp1 && xp3 <= xp0) {
				yp = zp = 1;
			} else if (xp2 <= xp3 && xp2 <= xp1 && xp2 <= xp0) {
				zp = 1;
			} else if (xp1 <= xp3 && xp1 <= xp2 && xp1 <= xp0) {
				yp = 1;
			}
		}
		if (zp == 1) PlotSet3DRange(t1, t2, 0.0, 1.0, 1.0, 0.0);

		xp  = t1 + (t2-t1)*Gpt->uxs[key];			/* Starting point */
		xlp = Gpt->uxl[key]*(t2-t1);					/* Length of axis */

		strscpy(title, Gpt->titles[key], sizeof(title));
		j = max(1, (int) strnblen(title));
		
		if (key == LEFT) {							/* Axis mode info */
			ixm = (Gpt->axmode[LEFT]   & ~(AXIS_Y_TYPE | AXIS_VERTICAL)) |
				   (Gpt->axmode[BOTTOM] & (AXIS_VERTICAL)) ;
		} else if (key == BOTTOM) {
			ixm = Gpt->axmode[key] & ~(AXIS_Y_TYPE);
		} else {
			ixm = Gpt->axmode[key] | 0x800;
		}

		if (Gpt->logtype[key]) {
			PlotLogAxis(xp, yp, ixm, xlp, dx, dx2, title, j, mx, color, NULL);
		} else {
			PlotAxis(xp, yp, ixm, xlp, dx, dx2, title, j, mx, color, NULL);
		}
	}											/* end for loop */

	Plot3DTransform(0, NULL, NULL);
	PlotFlush();							/* Flush graphics modes */
	PlotSelectPen(colhld);				/* Restore pen				*/
	return;
}


/* ============================================================================
-- Usage:  CALL YOUDRAW(key,xp,yp,xlp,ixm)
--
-- Inputs: key   - 3 => X axis, 4 => Y axis
--	        xp,yp - coordinates to start drawing at
--         xlp   - length of appropriate axis
--         xlow  - lower limit to draw is either xp or yp depending on key
--         xhigh - is xp or yp + xlp
--	        ixm   - axis labelling type
--
-- Output: Draws a non-linear axis
--
-- Note: Compilers are getting too bright.  They optimize out the apparently
--       redundant settings of the variable XME before calls to RDRTOK. 
--       Handled by specifying XME as a common variable (what a waste of space)
--       but avoids the failure.  Variable is tagged on end of the AXGEN common
--       block which serves no other purpose.
============================================================================ */
PRIVATE char *formula=NULL;
PRIVATE REAL xme=0.0f;
PRIVATE char *eqns[4]= {"bottom_to_top(x_t)",	/* For evaluating */
								"top_to_bottom(x_t)", 
								"left_to_right(x_t)", 
								"right_to_left(x_t)" };
PRIVATE char *fnc[4] = {"bottom_to_top",			/* For checking existence */
								"top_to_bottom", 
								"left_to_right", 
								"right_to_left" };
PRIVATE char *dflt[2]= {"(1000/x_t)-273",			/* For setting */
								"1000/(x_t+273)"};
REAL invt(REAL x);										/* Inversion function */

/* ------------------------------------------------------------------------- */
static LOGICAL YouDraw(int key, REAL xp, REAL yp, REAL xlp, int ixm) {

	REAL xlow,xhigh,x1,x2,dx,dx2;
	int i,mx,ik,ierr;
	char title[LONG_STR_SIZE];
	INTEGER color;
	PLOT_AXIS_LABELS *paxis_labels, axis_labels;

/* ---------------------------------------------------------
-- Set up so X1 = user coordinate corresponding to XLOW
--           X2 = user coordinate corresponding to XHIGH
--------------------------------------------------------- */
	if (key == TOP) {								/* XTOP axis */
		xlow  = xp;
		xhigh = xp+xlp;
	} else if (key == RIGHT) {					/* YRIGHT axis */
		xlow = yp;
		xhigh = yp+xlp;
	} else {
		return(FALSE);
	}

	/* If 0, will use defaults */
	color = Gpt->color[key];							/* Set the color to use */

	ik = 2*(key-TOP);								/* 0 ==> X, 2 ==> Y */
	if (! GVLinkReal("X_T", GVF_USER, &xme)) return(FALSE);

	if (! GVGetInfo(fnc[ik], &mx, NULL)) {
		if (!GVAllocFnc(eqns[ik], GVF_USER, dflt[0]) || !GVAllocFnc(eqns[ik+1], GVF_USER, dflt[1]))
			goto UnableToDefine;
	} else if (mx != GV_FUNCTION) {
		goto NotAFunction;
	} else if (! GVGetInfo(fnc[ik+1], &mx, NULL)) {
		goto NotAFunction;
	} else if (mx != GV_FUNCTION) {
		goto NotAFunction;
	}

	xme = xhigh;
	x1  = GVTrimToReal(GVEvalExpr(eqns[ik], &ierr));	/* Upper limit */
	if (ierr != 0) return(FALSE);
	xme = xlow;
	x2  = GVTrimToReal(GVEvalExpr(eqns[ik], &ierr));	/* Lower limit */
	if (ierr != 0) return(FALSE);
	xme = x1;														/* Inversion formula */
	xme = GVTrimToReal(GVEvalExpr(eqns[ik+1], &ierr));
	if (ierr != 0) return(FALSE);
	formula = eqns[ik+1];										/* And for use in invt */
	
	xlow  = min(x1,x2);											/* Order them */
	xhigh = max(x1,x2);											/* High to low */
	if (key == TOP)												/* X axis */
		xp = xlow;
	else																/* Y axis */
		yp = xlow;
	
	PlotAutoScale(xlow,xhigh, &x1,&x2, &dx,&dx2, &mx);	/* Corresponding ticks */
	if (Gpt->udx[key]  != 0.0f) dx  = Gpt->udx[key];		/* Overrides? */
	if (Gpt->udx2[key] != 0.0f) dx2 = Gpt->udx2[key];
	if (Gpt->umx[key]  != 0   ) mx  = Gpt->umx[key];
	strscpy(title, Gpt->titles[key], sizeof(title));	/* Copy over title */
	i = (int) strnblen(title);									/* Nominal length	 */
	i = min(-i,-1);												/* Make sure drawn */

/* Handle possible user labels declarations */
	if (Gpt->do_user_labels[key]) {
		paxis_labels = &axis_labels;
		make_label_structure(&Gpt->user_labels[key], paxis_labels);
	} else {
		paxis_labels = NULL;
	}

	PlotNonLinearAxis(xp, yp, ixm, xhigh-xlow, dx, dx2, title, i, mx, color, paxis_labels, invt);

	GVDeallocate("X_T");
	return(TRUE);

/* --- Errors here --- */
UnableToDefine:
	gen_err("Unable to make the default definitions for nonlinear axes");
	return(FALSE);
NotAFunction:
	gen_err("Bad definition of BOTTOM_TO_TOP(X) or TOP_TO_BOTTOM(X), or Y equivalents");
	return(FALSE);
}

/* ========================================================================= */
REAL invt(REAL x) {
	xme = x;
	return( GVTrimToReal(GVEvalExpr(formula, NULL)) );
}

/* ========================================================================= */
void make_label_structure(GPT_AXIS_LABELS *user, PLOT_AXIS_LABELS *labels) {
	int type, ilen;
	void *ptr;

	labels->major = labels->minor = NULL;
	labels->nmajor = labels->nminor = 0;
	labels->labels = NULL;
	labels->csize = 0.0;
	
	if (GVGetAdrInfo(user->major, &type, &ptr, &ilen) && (type == GV_ARRAY || type == GV_ARRAY_LINK)) {
		labels->major = (REAL *) ptr;
		labels->nmajor = ilen;
	}
	if (GVGetAdrInfo(user->minor, &type, &ptr, &ilen) && (type == GV_ARRAY || type == GV_ARRAY_LINK)) {
		labels->minor = (REAL *) ptr;
		labels->nminor = ilen;
	}
	if (GVGetAdrInfo(user->text, &type, &ptr, &ilen) && (type == GV_STRING_ARRAY || type == GV_STRING_ARRAY_LINK)) {
		labels->labels = (CHAR **) ptr;
		labels->nmajor = min(labels->nmajor, ilen);
	}
	labels->csize = (REAL) GVEvalExpr(user->csize, NULL);
	return;
}

#if 0
PLOT_AXIS_LABELS labels;
REAL major[6]={0,1,2,3,4,5}, minor[5]={0.5,1.5,2.5,3.5,4.5};
char *text[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
labels.nmajor = 6;
labels.nminor = 5;
labels.csize  = 0;
labels.major  = major;
labels.minor  = minor;
labels.labels = text;
#endif
