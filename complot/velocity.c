/* VELOCITY.F77 - Program for plotting velocity vectors on x,y coordinates */

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
void velplt( REAL x[], REAL y[], REAL vx[], REAL vy[], INTEGER npt, INTEGER inc);
void velscl(REAL vx[], REAL vy[], INTEGER npt, REAL length);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* =============================================================================
--     SUBROUTINE for plotting velocity vectors at specified x and y
--     coordinates.
--     CALLED USING:
--           CALL VELPLT(X,Y,VX,VY,NPT,INC)
--
--     INPUTS: X  - X Coordinates of data points
--             Y  - Y Coordinates
--             VX - X component of the velocity
--             VY - Y component of the velocity
--             NPT - number of data points in vectors
--             INC - increments of points (2 = every other)
--
-- Dec. 12, 1982 - MOT
--    Original coding
============================================================================= */
REAL vscale;

void velplt( REAL x[], REAL y[], REAL vx[], REAL vy[], INTEGER npt, INTEGER inc) {

	REAL x_1,y_1,dx,dy,ax,ay,theta;
	INTEGER i;
	LOGICAL usrsav;

#define arx 0.0866f
#define ary 0.05f

	usrsav = PlotSetUserMode(FALSE);				/* Off user elements */
	for (i=0; i<npt; i+=inc) {
		x_1 = x[i];										/* No user so easy */
		y_1 = y[i];
		dx = vx[i]*vscale;
		dy = vy[i]*vscale;
		if (usrsav) {
			x_1 = (x_1-PlotWindow->xoff)/PlotWindow->xfact;				/* Absolute inches */
			y_1 = (y_1-PlotWindow->yoff)/PlotWindow->yfact;				/* Absolute inches */
			dx = (REAL) (dx*sqrt(fabs(PlotWindow->yfact/PlotWindow->xfact))); /* And vector length */
			dy = (REAL) (dy*sqrt(fabs(PlotWindow->xfact/PlotWindow->yfact))); /* Y value */
		}
		theta = (REAL) atan2(dx,dy);								/* Angle it makes */
		PlotMove(x_1,y_1,3);											/* Draw vector */
		PlotMove(x_1+dx,y_1+dy,2);									/* To the tip */
		ax = (REAL) (-arx*cos(theta) - ary*sin(theta));		/* Movements for arrow */
		ay = (REAL) (ary*cos(theta) +  arx*sin(theta));		/* And Y value */
		PlotMove(x_1+dx+ax,y_1+dy+ay,2);							/* Plot to it */
		ax = (REAL) (-arx*cos(theta) +  ary*sin(theta));	/* Other side */
		ay = (REAL) (-ary*cos(theta) -  arx*sin(theta));	/* And again */
		PlotMove(x_1+dx,y_1+dy,3);									/* Do it */
		PlotMove(x_1+dx+ax,y_1+dy+ay,2);							/* And pick up pen */
	}

	if (DEVICE->Autoflush) PlotFlush();				/* Clear buffers */
	if (usrsav) PlotSetUserMode(TRUE);
	return;
}


/* =============================================================================
--    Subroutine to set the scale parameters
--
--     Usage:  VELSCL(VX,VY,NPT)
--
--     Inputs: VX - velocity data
--             VY - velocity data
--             NPT - total number of points
============================================================================= */
void velscl(REAL vx[], REAL vy[], INTEGER npt, REAL length) {

	REAL r,rhold=0.0f;
	INTEGER i;

	for (i=0; i<npt; i++) {
		r = vx[i]*vx[i] + vy[i]*vy[i];
		rhold = max(r,rhold);
	}
	
	vscale = length / (REAL) sqrt(rhold);
	return;
}
