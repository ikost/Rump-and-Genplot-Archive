/* <3d.c> - 3D setup routines */

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
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */

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
LOGICAL PlotSet3DMode(LOGICAL mode) {
	LOGICAL oldmode;

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_SET3DMODE,1,0,0);
		SV_PutInt(mode);
	}


	oldmode = PlotWindow->mode_3d;
	PlotWindow->mode_3d = mode;
	return(oldmode);
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
void PlotSet3DView(REAL r, REAL xd, REAL yd, REAL zd) {

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_SET3DVIEW,0,4,0);
		SV_PutReal(r);
		SV_PutReal(xd);
		SV_PutReal(yd);
		SV_PutReal(zd);
	}

	PlotWindow->view_d   = r;
	PlotWindow->rotate[0] = xd;
	PlotWindow->rotate[1] = yd;
	PlotWindow->rotate[2] = zd;

	PlotFixInternal();
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
void PlotSet3DRange(REAL xmin, REAL xmax, REAL ymin, REAL ymax, REAL zmin, REAL zmax) {

	if (PlotWindow->Save.On) {
		SV_PutCmd(SV_SET3DRANGE,0,6,0);
		SV_PutReal(xmin);
		SV_PutReal(xmax);
		SV_PutReal(ymin);
		SV_PutReal(ymax);
		SV_PutReal(zmin);
		SV_PutReal(zmax);
	}

	PlotWindow->xmin = xmin;
	PlotWindow->xmax = xmax;
	PlotWindow->ymin = ymin;
	PlotWindow->ymax = ymax;
	PlotWindow->zmin = zmin;
	PlotWindow->zmax = zmax;
	PlotWindow->usrnbl  = TRUE;		/* User mode enabled */
	PlotWindow->mode_3d = TRUE;		/* And in 3D mode		*/

	PlotFixInternal();
}
