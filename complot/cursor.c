/* CURSOR.F77 */

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
PRIVATE void CursorString(INTEGER ix, INTEGER iy, CHAR **str);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* =============================================================================
--     CURSOR - Read the cursor from equipped devices
--
--     Usage: CALL CURSOR(X, Y, CHAR)
--
--     Output: X    - X coordinate of cursor
--             Y    - Y coordinate of cursor
--             CHAR - Character returned with cursor button (CHARACTER*1)
--
============================================================================= */
void PlotCursor(REAL *x, REAL *y, INTEGER *achr) {

	DspCurfnc cur;											/* To send to DSPTCH */
	
/* ... Go read the cursor */
	cur.display = CursorString;
	(*DEVICE->dsptch)(CURFNC, DEVICE->DriverBlock, (DSP *) &cur);
	PlotConvert2DScales(PIXEL_TO_USER, (REAL) cur.x, (REAL) cur.y, x, y);	/* Convert to X,Y */
	if (cur.achr == ' ') cur.achr = '0';			/* Convert ' ' to 0 */
	if (achr != NULL) *achr = cur.achr;				/* Copy character back */
	return;
}

PRIVATE void CursorString(INTEGER ix, INTEGER iy, CHAR **str) {

	REAL x,y;
	static CHAR token[SHORT_STR_SIZE];

	PlotConvert2DScales(PIXEL_TO_USER, (REAL) ix, (REAL) iy, &x, &y);
	sprintf(token,"X: %12.5f  Y: %12.5f", x,y);
	*str = token;
	return;
}

/* ============================================================================
-- Follows data points on screen
--
-- Usage:  LOGICAL = CUR$TRAK(x,y,npt,inow)
--
-- Inputs:  X,Y  - X,Y arrays of data points
--          NPT  - Number of data points
--	   INOW - Point to start on
--
-- Outputs: INOW     - Last point on
--	   CUR$TRAK - Did device have the capability?
--	   IRET     - Integer equivalent of key terminating
============================================================================ */
LOGICAL PlotTrackingCursor(REAL x[],REAL y[], INTEGER npt, INT *inow, INTEGER *iret) {

	INT JumpSize, NextUp, NextDn;				/* Random variables */
	INT i,j;
	DspTxtfnc text;
	DspCurfnc cur;
	REAL x2, y2, d, dist;
	CHAR token[SHORT_STR_SIZE];

	if (npt == 0) return(FALSE);				/* Can't do anything! */
	i = min(npt-1, max(0, *inow)) ;			/* Starting point */
	cur.display = NULL;							/* Avoid display from cursor values */

	while (TRUE) {

		sprintf(token, "I: %4.4i  X: %13.6g  Y: %13.6g",i, x[i], y[i]);
		text.col = text.row = 1;
		text.str = token;
		if (! (*DEVICE->dsptch)(TXTFNC, DEVICE->DriverBlock, (DSP *) &text)) {
			CONputs(token);
			CONputc(0x0D);
			CONflush();
		}

		PlotConvert2DScales(USER_TO_PIXEL, x[i], y[i], &x2, &y2);
		cur.x = (int) x2; cur.y = (int) y2;
		cur.x = (int) min(PL_Plot.pclpxh, max(PL_Plot.pclpxl, cur.x));
		cur.y = (int) min(PL_Plot.pclpyh, max(PL_Plot.pclpyl, cur.y));
		if (! (*DEVICE->dsptch)(CURTRK, DEVICE->DriverBlock, (DSP *) &cur))
			return(FALSE);

/* ... See if we should continue */
/*		TTYprintf("Returned with code: %4.4x\n", cur.achr); */
		if ((cur.achr & 0x00FF) == 0x00FF) {	/* Is it special code	*/
         dist = +1.0E37f;
			for (j=0; j<npt; j++) {
				PlotConvert2DScales(USER_TO_PIXEL, x[j],y[j], &x2,&y2);
				d = (x2-cur.x)*(x2-cur.x) + (y2-cur.y)*(y2-cur.y);
				if (d < dist) {
					dist = d;
					i = j;
				}
			}
		} else if (cur.achr & 0x100) {			/* Is it a virtual key? */
			JumpSize = (cur.achr & 0x200) ? 10 : 1 ;
			cur.achr &= 0x1FF;						/* Leave only virtual codes */
			NextUp = min(i+JumpSize, npt-1);		/* Possible end-points */
			NextDn = max(0, i-JumpSize);			/* Possible end point */
			switch (cur.achr) {						/* Switch on character */
				case VIRTUAL_END:
					i = npt-1;
					break;
				case VIRTUAL_HOME:
					i = 0;
					break;
				case VIRTUAL_LEFT:
					i = NextDn;							/* Left arrow and equivalent */
					break;
				case VIRTUAL_PAGEDOWN:
				case VIRTUAL_RIGHT:
				case VIRTUAL_PAGEUP:
					i = NextUp;							/* Right arrow and equivalent */
					break;
				case VIRTUAL_UP:
					if (y[NextUp] > y[i])
						i = NextUp;
					else if (y[NextDn] > y[i]) 
						i = NextDn;
					break;
				case VIRTUAL_DOWN:
					if (y[NextDn] < y[i]) 
						i = NextDn;
					if (y[NextUp] < y[i]) 
						i = NextUp;
					break;
			}
		} else {
			*inow = i;									/* Last position */
         *iret = cur.achr;							/* And the character */
			if (*iret == ' ') *iret = '0';		/* Convert ' ' to 0 */
         return(TRUE);
		}
	}

	panic; return(TRUE);								/* BETTER NOT HAPPEN! */
}

/*=============================================================================
--     CUR$BOX - Read a box cursor from equipped devices
--               If none exists, switch to normal cursor and read two points.
--
--     Usage: LOGICAL = CUR$BOX(X1,Y1, X2,Y2, CHAR)
--
--     Output: X1,Y1   - Lower left coordinate
--             X2,Y2   - Upper left coordinate
--             CHAR    - Character returned with cursor button (CHARACTER*1)
--	      cur$box - Did device have this capability
============================================================================= */
LOGICAL PlotBoxCursor(REAL *x1, REAL *y1, REAL *x2, REAL *y2, INTEGER *achr) {
	
	DspCurbox curbox;						/* For dsptch */
	DspCurfnc cur;							/* For dsptch */
	REAL xt;
	
	if ((*DEVICE->dsptch)(CURBOX , DEVICE->DriverBlock, (DSP *) &curbox)) {
		PlotConvert2DScales(PIXEL_TO_USER, (REAL) curbox.x1, (REAL) curbox.y1, x1, y1);
		PlotConvert2DScales(PIXEL_TO_USER, (REAL) curbox.x2, (REAL) curbox.y2, x2, y2);
		if (achr != NULL) *achr = curbox.achr;		/* Copy character back */
	} else {
		TTYputs("Enter lower left and upper right corners of box: ");
		TTYflush();
		if (! (*DEVICE->dsptch)(CURFNC, DEVICE->DriverBlock, (DSP *) &cur)) 
			return(FALSE);
		PlotConvert2DScales(PIXEL_TO_USER, (REAL) cur.x, (REAL) cur.y, x1, y1);
		if (! (*DEVICE->dsptch)(CURFNC, DEVICE->DriverBlock, (DSP *) &cur)) 
			return(FALSE);
		PlotConvert2DScales(PIXEL_TO_USER, (REAL) cur.x, (REAL) cur.y, x2, y2);
		if (achr != NULL) *achr = cur.achr;	/* Copy character back */
	}

	if (achr != NULL && *achr == ' ') *achr = '0';	/* Convert <sp> to 0 */

	if (*x1 > *x2) {								/* Are they reversed */
		xt  = *x1;
		*x1 = *x2;
		*x2 =  xt;
	}
	if (*y1 > *y2) {								/* Are they reversed */
		xt  = *y1;
		*y1 = *y2;
		*y2 =  xt;
	}

	return(TRUE);
}
