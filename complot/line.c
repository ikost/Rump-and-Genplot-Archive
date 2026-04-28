/* LINE.F77 */

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

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
--     YPLOT - Routine to plot line with even X spacing.
--
--     Usage:     CALL YPLOT(YARRAY,NPT,XSTART,DX,LTYPE1,SYM,SYMSIZ,IPT,MODE)
--
--     Inputs:    YARRAY  -  Array of Y values to be plotted
--                NPT     -  Number of points in YARRAY
--                XSTART  -  X value of first element in YARRAY
--                DX      -  X spacing between elements in YARRAY
--                LTYPE1  -  Type of line - see PLOT2
--                SYM     -  Symbol to be drawn if LTYPE1 = 0
--                SYMSIZ  -  Size of symbols (inches)
--                IPT     -  Every IPT-th point plotter. (2 is every other)
--                MODE    -  Code specifying plot mode
--                           0 = Linear mode
--                           1 = Log mode (base 10)
--                           2 = Square root mode
--
-- OLD CALLING MODE!!     -  ASCII (4 characters) specifying plot mode
--                           'LIN ' = Linear mode
--                           'LOG ' = Log mode (base 10)
--                           'SQRT' = Square root mode
--
--     Outputs:   None
============================================================================ */
void PlotYPlot(REAL yarray[], INTEGER npt, REAL xstart, REAL dx, INTEGER ltype, INTEGER sym, REAL symsiz, INTEGER ipt, INTEGER mode) {

	REAL x,y;
	INTEGER i, linsav, ipen;
	LOGICAL symplt;
	CHAR ich;

/* ... Do we want symbols or lines */
	if (ltype == 0) {
		ich = (CHAR) abs(sym);							/* Convert INT to character */
		symplt = TRUE;
	} else {
		symplt = FALSE;
		linsav = PlotSetLineType(abs(ltype),0.0f);	/* Set up and save old value */
		ipen = 3;												/* Pen up first time */
	}

	if (ipt <= 0) ipt = 1;								/* Don't get screwed up! */

	for (i=0; i<npt; i+=ipt) {
		x = xstart + dx*i;
		y = yarray[i];

		if (mode == 1) {									/* Want LOG mode? */
			if (y > 0.0f)
				y = (REAL) log10(y);
			else {
				ipen = 3;									/* Lift pen for next point */
				continue;									/* And skip drawing */
			}
		} else if (mode == 2) {							/* How about Square root */
			if (y < 0.0f) y = -y;
			y = (REAL) sqrt(y);							/* Must use abs value */
			if (yarray[i] < 0.0f) y = -y;				/* Check sign */
		} else if (mode != 0) {
			gen_warn("Check YPLOT call sequence");
		}
		if (symplt) {
			if (sym == 0)
				PlotMove(x,y,4);
			else
				PlotSymbol(x,y, symsiz, ich);			/* Draw symbol */
		} else {
			PlotMove(x,y,ipen);							/* Draw vector */
			ipen = 2;										/* Subsequent vectors down */
		}
	}

	if (! symplt) {										/* Restore line type */
		PlotSetLineType(linsav,0.0f);
		PlotMove(x,y,3);									/* Pen up */
	}
	return;
}

/* =============================================================================
--     LINE2 - Subroutine to plot several different type lines
--     The type of line is defined in the LENGTH, SEGMAX
--     arrays.  SEGMAX specifies the number of segments in the
--     line definition.  LENGTH specifies the length of each
--     of the segments.  Pen up/down is alternated for each segment.
--     Initially, the pen is put down for segment 1.
--
--     usage:     call line2(x,y,npt,ltype)
--		 call line3(x,y,npt,npoint,ltype)
--
--     INPUTS:    X       -  ARRAY OF X VALUES TO BE PLOTTED
--                Y       -  ARRAY OF Y VALUES TO BE PLOTTED
--                NPT     -  NUMBER OF POINTS IN X AND Y
--		 NPOINT  -  Only every NPOINT point is drawn
--                LTYPE   -  Type of line desired
--                        -  0 => points only
--                       not 0 => line type
--                           Negative causes LTYPE to be automatically changed
--                           after line is drawn.
--
--     OUTPUTS:   NONE
============================================================================= */
void PlotLine2(REAL x[], REAL y[], INTEGER npt, INTEGER *ltype1) {
	
	PlotLine3(x, y, npt, 1, ltype1);
	return;
}

void PlotLine3(REAL x[], REAL y[], INTEGER npt, INTEGER npoint, INTEGER *ltype1) {
	
	INTEGER ltype,i;
	
/* ... Reset pointer and check validity of ltype */
	ltype = abs(*ltype1);

/* Either loop for points only, or go to line command. */
	if (ltype > 0) {
		i = PlotSetLineType(ltype, 0.0);				/* Save old and set linetype */
		PlotLine1(x,y,npt,npoint, 0,0,0.0);			/* Just a fancy line command */
		PlotSetLineType(i, 0.0);						/* Reset linetype				  */
		if (*ltype1 < 0) {								/* Autochange linetype?		  */
			if (*ltype1 >= -NTYPE)
				*ltype1 = *ltype1 - 1;
			else
				*ltype1 = -1;								/* Reset to loop through later */
		}
	} else {
		for (i=0; i<npt; i+=npoint) PlotMove(x[i], y[i], 4);
	}
	return;
}


/* =============================================================================
--     LINE1 - Routine to simulate the line subroutine with user coordinates.
--     Note that lines extending outside the plotsize will be discarded.
--
--     Usage:     CALL LINE1(x,y, npt,ipt, itype, isym, symsiz)
--
--     Inputs:    X      -  pointer to x coordinates
--                Y      -  pointer to y coordinates
--                npt    -  number of points in x,y
--                ipt    -  every ipt-th point in x and y will be plotted
--                          ie. a total of npt/ipt points will be sent
--                itype  -  Line type:  
--                                  negative - symbols only every j-th point
--                                  zero     - line only
--                                  positive - line and symbols
--                isym   -  Number of symbol to be drawn in symbol mode
--                symsiz - Size of symbols
--
--     Outputs:   Plot vectors
============================================================================= */
void PlotLine1(REAL x[], REAL y[], INTEGER npt, INTEGER ipt, INTEGER itype, INTEGER isym, REAL symsiz) {

	int i;

/* Set some defaults */
	if (symsiz <= 0) symsiz = 0.10f;								/* Default size */
	if (ipt <= 0)     ipt = 1;										/* Every point */

/* Draw lines if we are type 0 or positive */
	if (itype >= 0) {													/* Line here, draw first */
		PlotMove(x[0], y[0], 3);									/* Draw and lift pen */
		for (i=0; i<npt; i+=ipt) PlotMove(x[i], y[i], 2);	/* Draw to line */
		PlotMove(x[npt-1],y[npt-1],3);							/* Lift pen at the end */
	}

/* Draw symbols at every |itype| points if itype != 0 */
	if (itype != 0) {													/* Points also? */
		for (i=0; i<npt; i+=abs(itype)) 
			PlotSymbol(x[i], y[i], symsiz, (CHAR) isym);
	}

	return;
}
