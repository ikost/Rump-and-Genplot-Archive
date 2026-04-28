/* scope.f77 */

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
void PlotDrawScopeFace(void);

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
--     Routine to draw a simulated oscilloscope graticule on the graph.
--
--     Usage: CALL SCOPE
--
--     Note: Uses an internal size 10.0 x 8.0 for the trace.  Call to
--           SIZE with these values should be made to create the proper
--           aspect ratio.  Routine is mainly for fun only.
============================================================================= */
#define ts 0.17f

void PlotDrawScopeFace(void) {

	REAL		x,y,xsav[4];
	REAL		clp[4];
	REAL		x_1,x_2,dx,y_1,y_2,dy;
	REAL		ts1,ts2;
	INTEGER	i,j,k,savek;
	LOGICAL	usrsav;

	usrsav = PlotWindow->usrnbl;						/* Want to go back at end */
	if (usrsav) {
		xsav[0] = PlotWindow->xoff;
		xsav[1] = PlotWindow->xfact;
		xsav[2] = PlotWindow->yoff;
		xsav[3] = PlotWindow->yfact;
	}
	savek = -1;
	PlotSetClip(&savek,clp);
	i = 0;
	PlotSetClip(&i, clp);
	PlotSetRange(0.0f, 10.0f, 0.0f, 8.0f);			/* My size - default */
	i = 1;
	PlotSetClip(&i,clp);									/* Now allow all clipping! */

	ts1 = (REAL) (ts/8.0f*(PlotWindow->ysize-PlotWindow->ymarg[0]-PlotWindow->ymarg[1]));	/* symbol size */
	ts2 = (REAL) (ts/1.5f);

/* Draw the vertical lines */
	for (i=0; i<=10; i++) {
		x = (REAL) i;
		if (i==0) {											/* Percentage markers here */
			for (j=1; j<=2; j++) {
				PlotMove(0.0f,0.0f            ,3);		/* Many strange lines */
				PlotMove(0.0f,(REAL) (1.5f-ts),2);
				PlotMove(0.0f,(REAL) (1.5f+ts),3);
				PlotMove(0.0f,(REAL) (2.0f-ts),2);
				PlotMove(0.0f,(REAL) (2.0f+ts),3);
				PlotMove(0.0f,(REAL) (6.0f-ts),2);
				PlotMove(0.0f,(REAL) (6.0f+ts),3);
				PlotMove(0.0f,(REAL) (6.5f-ts),2);
				PlotMove(0.0f,(REAL) (6.5f+ts),3);
				PlotMove(0.0f,8.0f            ,2);
			}
			for (y = 6.5f; y>=2.4f; y--) {
				PlotMove(0.0f, y, 3);							/* % markers */
				PlotMove(0.2f, y, 2);
			}
			k = PlotSelectFont(1);							/* Always with set 1 */
			PlotString((REAL) (-0.9f*ts) ,(REAL) (1.5f-.7f*ts2) , ts1, "0%" ,0.0f,2);
			PlotString((REAL) (-0.9f*ts) ,(REAL) (2.0f-.7f*ts2) , ts1, "10" ,0.0f,2);
			PlotString((REAL) (-0.9f*ts) ,(REAL) (6.0f-.7f*ts2) , ts1, "90" ,0.0f,2);
			PlotString((REAL) (-1.35f*ts),(REAL) (6.5f-.7f*ts2) , ts1,"100" ,0.0f,3);
			PlotSelectFont(k);								/* Back to what it was */
		} else {
			PlotMove(x,0.0f,3);
			PlotMove(x,8.0f,2);								/* The actual line */
			if (i==10) {
				PlotMove(x,0.0f,2);							/* Double thickness */
			} else {
				if (i==5)									/* Size of tick marks */
					dx = .1f;								/* Center line big */
				else
					dx = .02f;								/* Others smaller */
				x_1 = x - dx;
				x_2 = x + dx;
				for (j=1; j<=8; j++) {
					for (k=1; k<=4; k++) {
						y = 9.0f-j-0.2f*k;
						PlotMove(x_1, y, 3);
						PlotMove(x_2, y, 2);
					}
				}
			}
		}
	}

/* Horizontal lines */
	for (i=0; i<=8; i++) {
		y = (REAL) i;
		if ( (i==2) || (i==6) )
			PlotMove(0.2f, y, 3);
		else
			PlotMove(0.0f, y, 3);
		PlotMove(10.0f, y, 2);
		if ( (i==0) || (i==8) ) {
			PlotMove(0.0f, y, 2);							/* Darken the line */
		} else {
			dy = 0.02f;
			if (i==4) dy = .1f;
			y_1 = y - dy;
			y_2 = y + dy;
			for (j=1; j<=10; j++) {
				for (k=1; k<=4; k++) {
					x = 11 - j - 0.2f*k;
					PlotMove(x, y_1, 3);
					PlotMove(x, y_2, 2);
				}
			}
		}
	}

/* Add the small dots at 0 and 100% values */
	for (y = 1.5f; y<10 ; y+=5) {
		for (i=0; i<10; i++) {
			for (j=1; j<=4; j++) {
				x = i + 0.2f*j;
				if (i+j != 1) PlotMove(x,y,4);					/* All but first dot! */
			}
		}
	}

	PlotSetClip(&savek,clp);
	if (usrsav) 
		PlotSetScaling(xsav[0],xsav[1],xsav[2],xsav[3]);
	else
		PlotSetUserMode(FALSE);
	if (DEVICE->Autoflush) PlotFlush();
	return;
}
