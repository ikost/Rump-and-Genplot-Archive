/* <routine name> */

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
-- Usage: PlotDrawErrorSymbol(int key, ERRORSYMBOL *buf)
--
-- Inputs: buf->widthx - length (inches) of cross bar on X error (y dir)
--         buf->widthy - length (inches) of cross bar on Y error (x dir)
--         buf->symsiz - size of the symbol (inches)
--         buf->isym   - symbol to plot
--         buf->x      - center of the symbol
--         buf->y      - center of the symbol
--         buf->errs  - (x_minus, x_plus, y_minus, y_plus)
--
-- Notes: (1) Prior to first call, structure should be initialized to 0
---       (2) Widths are given as the full width in inches
============================================================================ */
void PlotDrawErrorSymbol(ERRORSYMBOL *buf) {

	REAL x[5],y[5];										/* Origin, xm,xp,ym,yp posn */
	int i, isym;
	int clpmod_sav;										/* Save of the clipping mode */
	BOOL flag;

#define ERRXM buf->errs[0]								/* X Minus error		*/
#define ERRXP buf->errs[1]								/* X Plus error		*/
#define ERRYM buf->errs[2]								/* Y Minus error		*/
#define ERRYP buf->errs[3]								/* Y Plus error		*/

	isym = min(13, abs(buf->isym));
	
/* Check if size or symbol has changed since last call */
	if (buf->loadsym != isym || buf->loadsiz != buf->symsiz) {	
		if (isym != 0) {
			PlotQuerySymbolExtent(isym, buf->obuf);		/* Find size */
			for (i=0; i<4; i++) buf->obuf[i] *= buf->symsiz;
		} else {
			for (i=0; i<4; i++) buf->obuf[i]  = 0;
		}			
		buf->loadsym = isym;
		buf->loadsiz = buf->symsiz;
	}

/* Plot the symbol if necessary */
	if (buf->isym != 0) PlotSymbol(buf->x, buf->y, buf->symsiz, buf->isym);

/* Decide whether or not to turn off clipping */
	clpmod_sav = PlotWindow->clpmod;
	if (! PlotWindow->clipsymbols && PlotWindow->clpmod == 0) {
		if (! PlotInBounds(buf->x, buf->y, 0)) return;
		PlotWindow->clpmod = 1;
		PlotFixInternal();
	}

/* Convert positions to "inch" positions */
	PlotConvert2DScales(USER_TO_INCH, buf->x,       buf->y, &x[0], &y[0]);
	PlotConvert2DScales(USER_TO_INCH, buf->x-ERRXM, buf->y, &x[1], &y[1]);
	PlotConvert2DScales(USER_TO_INCH, buf->x+ERRXP, buf->y, &x[2], &y[2]);
	PlotConvert2DScales(USER_TO_INCH, buf->x, buf->y-ERRYM, &x[3], &y[3]);
	PlotConvert2DScales(USER_TO_INCH, buf->x, buf->y+ERRYP, &x[4], &y[4]);

/* Temporarily suspend user mode and go to inches */
	flag = PlotSetUserMode(FALSE);

/* Now, go through one at a time */
	if (ERRXM > 0.0) {
		if (buf->widthx != 0.0) {						/* Lower cross bar */
			PlotMove(x[1], y[1]-buf->widthx/2.0f, 3);
			PlotMove(x[1], y[1]+buf->widthx/2.0f, 2);
		}
		PlotMove(x[1], y[1], 3);						/* One end of bar		*/
		if (buf->isym != 0) {							/* Close on symbol	*/
			PlotMove(x[0]+buf->obuf[0], y[0], 2);
		} else if (ERRXP <= 0.0) {
			PlotMove(x[0], y[0], 2);
		}
	}

	if (ERRXP > 0.0) {
		if (buf->isym != 0) {							/* Start at symbol edge */
			PlotMove(x[0]+buf->obuf[1], y[0], 3);
		} else if (ERRXM <= 0.0) {						/* Or at point */
			PlotMove(x[0], y[0], 3);
		}
		PlotMove(x[2], y[2], 2);
		if (buf->widthx != 0.0) {						/* Upper cross bar */
			PlotMove(x[2], y[2]-buf->widthx/2.0f, 3);
			PlotMove(x[2], y[2]+buf->widthx/2.0f, 2);
		}
	}

	if (ERRYM > 0.0) {
		if (buf->widthy != 0.0) {						/* Lower cross bar */
			PlotMove(x[3]-buf->widthy/2.0f, y[3], 3);
			PlotMove(x[3]+buf->widthy/2.0f, y[3], 2);
		}
		PlotMove(x[3], y[3], 3);						/* One end of bar */
		if (buf->isym != 0) {							/* Close on symbol */
			PlotMove(x[0], y[0]+buf->obuf[2], 2);
		} else if (ERRYP <= 0.0) {						/* Or on point */
			PlotMove(x[0], y[0], 2);
		}
	}

	if (ERRYP > 0.0) {
		if (buf->isym != 0) {							/* Start at symbol edge */
			PlotMove(x[0], y[0]+buf->obuf[3], 3);
		} else if (ERRYM <= 0.0) {						/* Or at point */
			PlotMove(x[0], y[0], 3);
		}
		PlotMove(x[4], y[4], 2);
		if (buf->widthy != 0.0) {						/* Upper cross bar */
			PlotMove(x[4]-buf->widthy/2.0f, y[4], 3);
			PlotMove(x[4]+buf->widthy/2.0f, y[4], 2);
		}
	}

/* Restore to user mode */
	if (flag) PlotSetUserMode(TRUE);
	if (clpmod_sav != PlotWindow->clpmod) {
		PlotWindow->clpmod  = clpmod_sav;		/* Restore clipping mode */
		PlotFixInternal();
	}

	return;
}

/* =============================================================================
--     PlotText - Plot text at a specific location
--
--     Usage: PlotText(INTEGER row, INTEGER col, CHAR *text);
--
--     Inputs: row,col - row and column to plot
--             text    - character string
============================================================================= */
void PlotText(INTEGER row, INTEGER col, CHAR *token) {

	DspTxtfnc text;
	text.col = col;
	text.row = row;
	text.str = token;
	(*DEVICE->dsptch)(TXTFNC, DEVICE->DriverBlock, (DSP *) &text);
	return;
}

/* ============================================================================
--     CIRCLE - Routine to draw a circle
--
--     Usage: CALL CIRCLE(X,Y,RADIUS)
--
--     Inputs: X      - X coordinate of center
--             Y      - Y coordinate of center
--             RADIUS - Radius desired
--
============================================================================ */
#define dtheta   0.087266463f
#define pi       3.141592654f

void PlotCircle(REAL x, REAL y, REAL radius) {

	REAL theta=0.0f;
	REAL xp,yp;
	
	PlotMove(x+radius, y, 3);					/* Position pen to start */
	while ( (REAL) (theta+=dtheta) < (REAL) (2*pi+dtheta/2) ) {
		xp = (REAL) (x+radius*cos(theta));
		yp = (REAL) (y+radius*sin(theta));
		PlotMove(xp, yp, 2);
	}
	PlotMove(x+radius, y, 3);					/* Lift pen at end */
	return;
}
