/* TABLET.C - Routines for handling digitizing tablets */

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
#include <limits.h>

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

typedef struct _DEVPAIR {
	CHAR *name;
	PLOTDRIVER *pointer;
} DEVPAIR;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
PLOTDRIVER Pipes_Driver;
PLOTDRIVER Null_Driver;
PLOTDRIVER Debug_Driver;
PLOTDRIVER Summa2_Driver;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
PRIVATE DEVPAIR TabList[] = 
						  { {"DEBUG",		Debug_Driver}		/* Null driver			*/
							,{"NULDRV",		Null_Driver}		/* Null driver			*/
							,{"SUMMA2",		Summa2_Driver}		/* Summa Tablet		*/
							,{"\0",			NULL}					/* End of list			*/
						};

/* ============================================================================
-- Open a channel corresponding to a tablet device
-- 
-- Usage:   PlotOpenTablet(void)
-- 
-- Inputs:  none
-- 
-- Output:  none
-- 
-- Returns: none
=========================================================================== */
LOGICAL PlotOpenTablet(void) {

	DspInifnc dsp;
	DEVPAIR *TabPtr=TabList;
	
	if (TABLET == NULL) return(FALSE);
	TABLET->Initialized = FALSE;				/* Mark tablet as not initialized */
	TABLET->dsptch      = NULL;				/* Mark it unavailable */

/* Look up the driver (if internal) or handle piped drivers	*/
	if (*TABLET->DriverName == '!') {		/* Requesting pipe driver */
		TABLET->dsptch = Pipes_Driver;
	} else do {
		if (stricmp(TABLET->DriverName, TabPtr->name) == 0) {
			TABLET->dsptch = TabPtr->pointer;
			break;
		}
	} while ( (TabPtr++)->pointer != NULL);
	
	if (TABLET->dsptch == NULL) {						/* No match? */
		gen_err2("Device driver not recognized", TABLET->DriverName);
		return(FALSE);
	}

	memset(&dsp, 0, sizeof(dsp));					/* Zero out everything			*/
	dsp.Driver      = TABLET->DriverName;
	dsp.Class       = TABLET->Class;
	dsp.SubDevice   = TABLET->SubDevice;
	dsp.Options     = TABLET->Options;
	dsp.IO_Channel  = TABLET->IO_Channel;
	dsp.NumberPens  = 1;
	dsp.DriverBlock = &TABLET->DriverBlock;
	TABLET->Capabilities = DEV_CAP_TABLET;

	if (! (*TABLET->dsptch)(TABINIFNC, TABLET->DriverBlock, (DSP *) &dsp)) {
		gen_err2("Unable to initialize device",TABLET->DriverName);
		return(FALSE);
	}

	TABLET->xperinch    = dsp.xperinch;
	TABLET->yperinch    = dsp.yperinch;
	TABLET->xmax		  = dsp.xmax;
	TABLET->ymax        = dsp.ymax;
	TABLET->Capabilities = dsp.Capabilities;
	TABLET->ypmax       = dsp.ymax;					/* Give COMPLOT Y max pixels */
	TABLET->Initialized = TRUE;						/* Mark device as initialized */
	return(TRUE);

}


/* ============================================================================
-- Close a channel corresponding to a tablet device
-- 
-- Usage:   PlotCloseTablet(void)
-- 
-- Inputs:  none
-- 
-- Output:  none
-- 
-- Returns: none
=========================================================================== */
void PlotCloseTablet(void) {

	if (TABLET != NULL && TABLET->Initialized) {					/* Only do as necessary */
		(*TABLET->dsptch)(TABENDFNC, TABLET->DriverBlock, NULL);
		TABLET->Initialized = FALSE;
	}
	return;
}


/* ============================================================================
-- Close a channel corresponding to a tablet device
-- 
-- Usage:   PlotQueryTablet()
-- 
-- Inputs:  key
-- 
-- Output:  none
-- 
-- Returns: none
=========================================================================== */
LOGICAL PlotQueryTablet(int key, int *x, int *y, int *achr) {
	
	DspCurfnc cur;
	int mykey;
	
	if (TABLET == NULL || !TABLET->Initialized) return(FALSE);

	if (key == 0)								/* Get a single isolated point */
		mykey = TABGETPOINT;
	else if (key == 1)						/* Get one point from switch stream */
		mykey = TABGETSWITCH;
	else if (key == 2)						/* Get a point from full stream mode */
		mykey = TABGETSTREAM;
	else if (key == -1) {					/* Request to clear a stream mode */
		(*TABLET->dsptch)(TABCLEARMODE, TABLET->DriverBlock, NULL);
		return(TRUE);
	}
	else
		return(FALSE);
	
	cur.display = NULL;
	(*TABLET->dsptch)(mykey, TABLET->DriverBlock, (DSP *) &cur);
	if (x    != NULL) *x = cur.x;
	if (y    != NULL) *y = cur.y;
	if (achr != NULL) *achr = cur.achr;
	return(TRUE);
}
