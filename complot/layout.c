/* LAYOUT.F77  -- Routines controlling layout of the plot */

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

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
void sv_in0(int code);
void sv_in1(int code, int i1);
void sv_in2(int code, int i1, int i2);
void sv_rl1(int code, REAL x1);
void sv_rl2(int code, REAL x1, REAL x2);
void sv_rl4(int code, REAL x1, REAL x2, REAL x3, REAL x4);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* =============================================================================
--     USRMOD - Routine which allows user to explicitly set the value of the
--              USRNBL flag which controls the conversion of coordinates from
--              user scales to inches automatically.  If false, all coordinates
--              are assumed to be in inches.  If true, the coordinates will be
--              converted according to values set by SET or OFFSET.  These
--              routines automatically set the USRNBL flag when called.
--
--     Usage: LOG = USRMOD(FLAG)
--
--     Inputs: FLAG   - New status desired for USRNBL flag
--
--     Output: USRMOD - Previous status of USRNBL flag
--
--     Notes: .TRUE.  => Apply conversion PLOT = XSCALE*X + XOFF
--            .FALSE. => Ignore conversion
============================================================================= */
LOGICAL PlotSetUserMode(LOGICAL flag) {
	
	LOGICAL hold;

	sv_in1(SV_USRMOD, (int) flag);			/* Handle HCOPY */

	hold = PlotWindow->usrnbl;
	PlotWindow->usrnbl = flag;
	PlotFixInternal();
	return(hold);
}


/* =============================================================================
--     SUBROUTINE SET - Routine to establish scaling factors from the user
--                      coordinates to inches required in the original complot
--		       routines.  User oblivious to actual conversion values.
--
--     Usage:     LOG = SET(XMIN,XMAX,YMIN,YMAX)
--
--     Inputs:    XMIN  -  Users minimum x to be plotted
--                XMAX  -  Users maximum x to be plotted
--                YMIN  -  Users minimum y to be plotted
--                YMAX  -  Users maximum y to be plotted
--
--     Outputs:   SET   -  Status of USRNBL after this call
--                Sets the offset and factors to main plotting routines to
--                effect the desired transformation.
--
--     Notes: This routine takes XSIZE and YSIZE and scales the drawing routine
--            to fit the data to within a window with a margin of XMARG and
--            YMARG of that size.  Sets clipping to these regions also.
--            plot  = user*FACT + OFF
--
--     ERRORS: If XMIN=XMAX or YMIN=YMAX, routine does not change those parms.
--             If XMIN=YMIN=XMAX=XMIN=0, the status of USRNBL will be returned.
--             USRNBL identifies the status of plot using USER conversions.
--
-- Changes common block and takes effect immediately!
============================================================================= */
LOGICAL PlotSetRange(REAL xmin, REAL xmax, REAL ymin, REAL ymax) {

	PlotWindow->mode_3d = FALSE;

	if ( (xmin!=0.0) || (ymin!=0.0) || (xmax!=0.0) || (ymax!=0.0) ) {
		
/* Set the coordinate conversion */
		sv_rl4(SV_SET, xmin, xmax, ymin, ymax);		/* Handle HCOPY */

		PlotWindow->usrnbl = TRUE;
		if (xmin != xmax) {
			PlotWindow->xmin  = xmin;						/* Save values			*/
			PlotWindow->xmax  = xmax;
			PlotWindow->xfact = (PlotWindow->xsize-PlotWindow->xmarg[0]-PlotWindow->xmarg[1]) / (xmax-xmin);
			PlotWindow->xoff  = PlotWindow->xmarg[0] - xmin*PlotWindow->xfact;
		}
		if (ymin != ymax) {
			PlotWindow->ymin  = ymin;
			PlotWindow->ymax  = ymax;
			PlotWindow->yfact = (PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]) / (ymax-ymin);
			PlotWindow->yoff  = PlotWindow->ymarg[0] - ymin*PlotWindow->yfact;
		}
		PlotFixInternal();
	}
	
	return(PlotWindow->usrnbl);
}


/* =============================================================================
-- Subroutine ORIGIN
--     Routine to displace all subsequent plotting by a specific distance.
--     The distance is specified in absolute inches from origin of the
--     device.  Equivalent to call to PLOT with -n pen command, except
--     this relocation is absolute instead of relative.  The relocation
--     remains in effect until a call to PLTEND or to FRAME.
--
--     Usage: CALL ORIGIN(XPLOFF,YPLOFF)
--
--     Inputs: XPLOFF - Absolute X origin of subsequent plots (inches)
--             YPLOFF - Absolute Y origin
--                      < 0 has no effect on that origin
--
--     Output: Changes common block.  Takes effect immediately.
============================================================================= */
void PlotSetOrigin(REAL xnew, REAL ynew) {

	sv_rl2(SV_ORIGIN, xnew, ynew);				/* Handle HCOPY */
	if (xnew >= 0.0) PlotWindow->xorg = xnew;
	if (ynew >= 0.0) PlotWindow->yorg = ynew;
	PlotFixInternal();
	return;
}


/* =============================================================================
--     SUBROUTINE SIZE - Set the physical size of plot (inches)
--
--     Usage: CALL SIZE(XSIZE,YSIZE)
--
--     Inputs: XSIZE - NEW X DIMENSION (INCHES) OF PLOTTER AREA
--                     IF <= 0, LEFT UNCHANGED
--             YSIZE - NEW Y DIMENSION
--
--     Output: Changes common block.  Only affected if user selects his
--             coordinates with SET.
--
--     NOTES: No action is taken on the new plotter size
--            until the set1 routine is subsequently called.
============================================================================= */
void PlotSetSize(REAL xnew, REAL ynew) {

	sv_rl2(SV_SIZE, xnew, ynew);						/* Handle HCOPY */
	if (xnew > 0.0) PlotWindow->xsize = xnew;
	if (ynew > 0.0) PlotWindow->ysize = ynew;
	return;
}


/* =============================================================================
--  SUBROUTINE MARGIN
--     When using user coordinates with SET command, a margin is left
--     around the plot for AXIS and titles.  This routine sets the value
--     of this margin in the X and Y direction.  The margin command has
--     no effect on the clipping unless user coordinates are enabled by
--     a call to SET.
--
--     Usage:     CALL MARGIN(XMARGL, YMARGL, XMARGH, YMARGH)
--
--     Inputs:    XMARGL - Margin in the lower X direction 
--                XMARGH - Margin in the upper X direction
--                YMARGL - Margin in the left  Y direction
--                YMARGH - Margin in the right Y direction
--                < 0 has no effect on that particular margin
--
--     Output:    Sets variables in the common block.
============================================================================= */
void PlotSetMargin(REAL xl, REAL yl, REAL xh, REAL yh) {

	sv_rl4(SV_MARGIN, xl, yl, xh, yh);					/* Handle HCOPY */
	if (xl >= 0.0) PlotWindow->xmarg[0] = xl;
	if (yl >= 0.0) PlotWindow->ymarg[0] = yl;
	if (xh >= 0.0) PlotWindow->xmarg[1] = xh;
	if (yh >= 0.0) PlotWindow->ymarg[1] = yh;
	return;
}


/* =============================================================================
--  void PlotInformAxesLimits();
--     Tell driver the region corresponding to the "interior" of the plot
--     region.  The driver is free to fill with a color if desired.
--
--     Usage:     PlotInformAxesLimits(int color);
--
--     Inputs:    Color - will be passed to driver, but may be unused.
--                        Specify as negative to use defaults.
--
--     Output:    Sends information message to driver
============================================================================= */
void PlotInformAxesLimits(int color) {

	DspAxesLimit dsp;
	REAL  x1,y1,x2,y2;

	sv_in1(SV_AXESREGION, color);

	PlotConvert2DScales(INCH_TO_PIXEL, PlotWindow->xmarg[0], PlotWindow->ymarg[0],
							  &x1, &y1);
	PlotConvert2DScales(INCH_TO_PIXEL, PlotWindow->xsize-PlotWindow->xmarg[1],
							  PlotWindow->ysize-PlotWindow->ymarg[1], &x2, &y2);

	dsp.axes_x1 = (int) (min(x1, x2)+1.5);			/* Make them one less */
	dsp.axes_x2 = (int) (max(x1, x2)+1.5);
	dsp.axes_y1 = (int) (min(y1, y2)-0.5);			/* Make these one less */
	dsp.axes_y2 = (int) (max(y1, y2)-0.5);

/* Change color in case we want to fill the axes with this color */
	if (color < 0) color = PlotWindow->AxesFillColor;
	if (color < 0) color = CLR_NOMARK;
	PlotSetBrush(color, &dsp.axes_brush);

/* And now the information on the full area */
	PlotConvert2DScales(INCH_TO_PIXEL, 0, 0, &x1, &y1);
	PlotConvert2DScales(INCH_TO_PIXEL, PlotWindow->xsize, PlotWindow->ysize, &x2, &y2);
	dsp.area_x1 = (int) (min(x1, x2)+1.5);			/* Make them one less */
	dsp.area_x2 = (int) (max(x1, x2)+1.5);
	dsp.area_y1 = (int) (min(y1, y2)-0.5);			/* Make these one less */
	dsp.area_y2 = (int) (max(y1, y2)-0.5);

/* Change color in case we want to fill the area with this color */
	color = PlotWindow->AreaFillColor;
	if (color < 0) color = CLR_NOMARK;
	PlotSetBrush(color, &dsp.area_brush);

	(*DEVICE->dsptch)(AXESLIMIT, DEVICE->DriverBlock, (DSP *) &dsp);

	return;
}


/* =============================================================================
--     XYFLIP - Subroutine to flip X-Y coordinates on plot page
--
--     Usage:     CALL XYFLIP(flag)
--
--     Inputs:    flag - .TRUE.  => Flip coordinates set
--			.FALSE. => Return coordinates to normal
--
--     Output: Does an automatic re-initialize of the plot screen.
--	      Equivalent to "FRAME" without doing an erase.
============================================================================= */
LOGICAL PlotSetXYFlip(LOGICAL flag) {

	LOGICAL logtmp;

/* Save for return value */
	logtmp = (PlotWindow->orient == PORTRAIT || PlotWindow->orient == INV_PORTRAIT);
	sv_in1(SV_FLIPXY, (int) flag);
	PlotWindow->orient = flag ? PORTRAIT : LANDSCAPE ;
	PlotFixInternal();
	return(logtmp);
}


/* =============================================================================
--     PlotSetPageOrientation - Subroutine to choose Portrait/Landscape set
--
--     Usage: int PlotSetPageOrientation(int mode)
--
--     Inputs: mode - desired mode (if valid value)
--               1 => Landscape
--               2 => Portrait
--               3 => Inverted Landscape
--               4 => Inverted Portrait
--
--     Output: Does an automatic re-initialize of the plot screen.
--
--     Returns: Previous value of the screen orientation
============================================================================= */
int PlotSetPageOrientation(int mode) {

	int itmp;
	static ORIENTATION list[4] = {LANDSCAPE, PORTRAIT, INV_LANDSCAPE, INV_PORTRAIT};

/* Determine current setting */
	for (itmp=0; itmp<4; itmp++) {
		if (PlotWindow->orient == list[itmp]) break;
	}
	itmp++;
	if (itmp>4) itmp=1;

/* Maybe modify */
	if (mode >= 1 && mode <= 4 && mode != itmp) {
		sv_in1(SV_ORIENT, mode);
		PlotWindow->orient = list[mode-1];
		PlotFixInternal();
	}
	return(itmp);
}


/* =============================================================================
-- PlotSetSymbolClip - Should we clip symbols to area inside plot?
--
-- Usage:  BOOL PlotSetClip(BOOL key);
--
-- Inputs: key TRUE/FALSE - should symbols be clipped (normally true)
--
-- Output: sets internal variables
--
-- Return: previous setting
============================================================================= */
BOOL PlotSetSymbolClip(BOOL key) {					/* Set symbol clip	*/
	BOOL lval;

	sv_in1(SV_CLIPSYMBOLS, (int) key);
	
	lval = PlotWindow->clipsymbols;
	PlotWindow->clipsymbols = key;
	return(lval);
}


/* =============================================================================
--     PlotSetClip - Specifically set the clipping boundary
--
--     Usage:  PlotSetClip(key,parms)
--
--     Inputs: key -1 => Return mode in key, clip region in parms!
--	                 0 => Use box mode specified by size and margins
--		              1 => Use max mode - full screen
--		              2 => Use specified values in parms(1-4)
--	      parms  => For key=2, has xlow,xhigh, ylow,yhigh values
--
--     Output: key,parms for key = -1
--
--     Note: All values are specified as absolute inches - no dealing with 
--           origin or otherwise.
============================================================================= */
void PlotSetClip(INTEGER *key, REAL parms[]) {

	int i;

	if (*key == -1) {
		*key = PlotWindow->clpmod;				/* Return previous value */
		if (parms != NULL) {
			*(parms++) = PlotWindow->clpxl;
			*(parms++) = PlotWindow->clpxh;
			*(parms++) = PlotWindow->clpyl;
			*(parms++) = PlotWindow->clpyh;
		}
		return;
	}

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_CLIP, 1,4,0);				/* Need room for values */
		SV_PutInt(*key);
		for (i=0;i<4;i++) SV_PutReal( (parms==NULL) ? 0.0f : parms[i] );
	}

	PlotWindow->clpmod = *key;
	if (*key == 2) {
		PlotWindow->clpxl = *(parms++);
		PlotWindow->clpxh = *(parms++);
		PlotWindow->clpyl = *(parms++);
		PlotWindow->clpyh = *(parms++);
	}
	PlotFixInternal();
	return;
}


/* =============================================================================
--     FACTOR - Change factor by which all plotting coordinates are multiplied.
--
--     Usage: REAL = FACTOR(F)
--
--     Inputs: F      - New multiplicative factor
--                    - If <= 0.0, has no effect.
--
--     Output: FACTOR - Old multiplicative factor
--             Sets FACTR in COMPLOT common block
============================================================================= */
REAL PlotSetFactor(REAL f) {
	
	REAL hold;
	
	sv_rl1(SV_FACTOR, f);								/* Handle HCOPY */
	hold = PlotWindow->factr;
	if (f > 0.0) {
		PlotWindow->factr = f;
		PlotFixInternal();
	}
	return(hold);
}


/* =============================================================================
--     OFFSET - Apply a linear transformation to subsequent plot coordinates.
--
--     Usage: CALL OFFSET(XOFF,XFACT,YOFF,YFACT)
--
--     Inputs: XOFF,XFACT - Offset and factor for X coordinates
--             YOFF,YFACT - Offset and factor for Y coordinates
--
--     Output: XOFF, YOFF, XFACT, YFACT set in COMPLOT common
--
--     Notes: X(inches) = XOFF + XFACT * X(user)
--            Y(inches) = YOFF + YFACT * Y(user)
============================================================================= */
void PlotSetScaling(REAL x0, REAL xf, REAL y0, REAL yf) {

	sv_rl4(SV_OFFSET, x0, xf, y0, yf);				/* Handle HCOPY */

	PlotWindow->mode_3d = FALSE;

	PlotWindow->usrnbl = TRUE;
	PlotWindow->xfact  = xf;
	PlotWindow->xoff   = x0;
	PlotWindow->yfact  = yf;
	PlotWindow->yoff   = y0;

	PlotWindow->xmin = (PlotWindow->xmarg[0]-PlotWindow->xoff) / PlotWindow->xfact;
	PlotWindow->xmax =  PlotWindow->xmin + (PlotWindow->xsize-2*PlotWindow->xmarg[1]) / PlotWindow->xfact;
	PlotWindow->ymin = (PlotWindow->ymarg[0]-PlotWindow->yoff) / PlotWindow->yfact;
	PlotWindow->ymax =  PlotWindow->ymin + (PlotWindow->ysize-2*PlotWindow->ymarg[1]) / PlotWindow->yfact;

	PlotFixInternal();
	return;
}


/* =============================================================================
--     PlotQueryPosn - Return current coordinates and factor
--
--     Usage: CALL PlotQueryPosn(X, Y, F)
--
--     Inputs: COMPLOT common block
--
--     Output: X - Current X coordinate
--             Y - Current Y coordinate
--             F - Current plotting factor (see FACTOR)
--
--     Notes: Converts from the pseudo-inches stored in COMMON to user units.
============================================================================= */
void PlotQueryPosn(REAL *x, REAL *y, REAL *f) {

/* Superceeded, but still nice not to have to know xloc, yloc stuff	*/
/* x or y being NULL is handled by PlotConvert2DScales itself			*/
	PlotConvert2DScales(PIXEL_TO_USER, PlotWindow->xloc, PlotWindow->yloc, x, y);
	if (f != NULL) *f = PlotWindow->factr;				/* Current factor */

	return;
}


/* =============================================================================
--     Returns values from the common block.
--
--     Usage:     CALL PLOTQ(ARRAY,N)
--
--     Output:    ARRAY  REAL array with at least N elements.  They
--                       are set, up to a maximum of N, as follows:
--                       1   XORG
--                       2   YORG
============================================================================= */
#if 0                                        /* Routine no longer used */
void PlotQueryInfo(REAL array[], INTEGER n) {

	if (n == -4) {										/* Query XFACT,YFACT etc. */
		if (PlotWindow->usrnbl) {
			array[0] = PlotWindow->xfact;
			array[1] = PlotWindow->yfact;
			array[2] = PlotWindow->xoff;
			array[3] = PlotWindow->yoff;
		} else {
			array[0] = 1.0;
			array[1] = 1.0;
			array[2] = 0.0;
			array[3] = 0.0;
		}
	}
	if (n <= 0 || n > 4) return;
	if (n >= 4) array[3] = PlotWindow->ymarg;
	if (n >= 3) array[2] = PlotWindow->xmarg;
	if (n >= 2)	array[1] = PlotWindow->yorg;
	if (n >= 1) array[0] = PlotWindow->xorg;
	return;
}
#endif
