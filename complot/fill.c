/* FILL.F77 */

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

/* =============================================================================
-- PlotBeginPath - Tells driver that following vectors will constitute a path.
-- PlotEndPath   - Tells driver that path is finished.  Driver need implement
--                 only a single path structure, path should not be cleared
--                 until next PlotBeginPath
-- PlotFillPath  - causes path to be filled with specified color, and maybe
--                 outlined as specified.
--
--     Usage: PlotBeginPath()
--            PlotEndPath()
--            PlotFillPath(int color, int mode);
--            PlotStrokePath(int color, int linewidth)
--
--     Inputs: fill_pen    - If >=0, color to fill region
--             outline_pen - If >0,  color to outline region
--             linewidth   - Linewidth for the outline (if > 0)
--
--     Output: Tells driver to expect following draw commands to give a
--             set of region specifications.  Fill will actually conclude
--             with 
============================================================================= */
int PlotBeginPath(void) {

	if (PlotWindow->Save.On) SV_PutCmd(SV_BEGINPATH, 0,0,0);
	(*DEVICE->dsptch)(BEGINPATH, DEVICE->DriverBlock, NULL);
	return(0);
}

int PlotEndPath(void) {
	if (PlotWindow->Save.On) SV_PutCmd(SV_ENDPATH, 0,0,0);
	(*DEVICE->dsptch)(ENDPATH, DEVICE->DriverBlock, NULL);
	return(0);
}

int PlotFillPath(int color, int mode) {

	DspFillPath dsp;

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_FILLPATH, 2,0,0);					/* Fill command */
		SV_PutInt(color);
		SV_PutInt(mode);
	}

	dsp.color = color;
	dsp.mode  = mode;
	(*DEVICE->dsptch)(FILLPATH, DEVICE->DriverBlock, (DSP *) &dsp);
	return(0);
}

int PlotStrokePath(int color, int linewidth) {

	DspStrokePath dsp;
	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_STROKEPATH, 2,0,0);					/* Fill command */
		SV_PutInt(color);
		SV_PutInt(linewidth);
	}
	dsp.color = color;
	dsp.linewidth = linewidth;
	(*DEVICE->dsptch)(STROKEPATH, DEVICE->DriverBlock, (DSP *) &dsp);
	return(0);
}

/* =============================================================================
--     FILL - Keyed routine to start, end or fill a specified region of plot
--
--     Usage: INTEGER FILL(key,x,y)
--
--     Inputs: key - 1 => Start a panel operation
--		    2 => End   a panel operation
--		    3 => Fill at the coordinates specified by COORD
--		         Causes a fill operation to start at coord and fill
--		         to the first boundary.
--	      coord - X,Y coordinates for fill (only need be specified key 3)
--					  If X = -1, Coordinates will be obtained from cursor
--					  Function will return key pressed for cursor
============================================================================= */
INTEGER PlotFill(INTEGER key, REAL x, REAL y) {
	
	REAL    xp,yp;
	INTEGER rcode=0;
	
	DspCurfnc cur;
	DspFilfnc fill;
	
	switch (key) {
		case 1:										/* Panel start */
			(*DEVICE->dsptch)(PANFNC, DEVICE->DriverBlock, NULL);
			break;
		case 2:										/* Panel end */
			(*DEVICE->dsptch)(POFFNC, DEVICE->DriverBlock, NULL);
			break;
		case 3:										/* Fill area */
         if (x == -1.0f) {						/* Get from cursor */
				cur.display = NULL;				/* No conversion routines */
				(*DEVICE->dsptch)(CURFNC, DEVICE->DriverBlock, (DSP *) &cur);
				PlotConvert2DScales(PIXEL_TO_USER, (REAL) cur.x, (REAL) cur.y, &xp, &yp);
				rcode = (INTEGER) cur.achr;
			} else {
				xp = x;
				yp = y;
			}

			PlotConvert2DScales(USER_TO_PIXEL, xp, yp, &xp, &yp);
			fill.x = (int) xp; fill.y = (int) yp;
			(*DEVICE->dsptch)(FILFNC, DEVICE->DriverBlock, (DSP *) &fill);
			break;
	}

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_FILL, 1,2,0);					/* Fill command */
		SV_PutInt(key);
		SV_PutReal(xp);
		SV_PutReal(yp);
	}

	return(rcode);
}
