/* devini.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef OS2
	#define INCL_PM
	#include <os2.h>
#elif defined NT
	#include <windows.h>
	#define	NEED_COMPLOT_MUTEX_INFO
#endif

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
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- dev_ini - Routine to load and set the specified device as active
-- 
-- Usage: LOGICAL PlotOpenDevice();
-- 
-- Inputs: Information about driver from partially completed device structure.
-- 
-- Output: Initializes device and fills in rest of the device structure.  Returns
--         operating parameters such as pixels per inch etc.
=========================================================================== */
LOGICAL PlotOpenDevice(void) {

	DspInifnc dsp;
	static int Graph_Num = 0;					/* Graph number for identifying windows */
	
	if (DEVICE == NULL) return(FALSE);
	DEVICE->Initialized = FALSE;				/* Mark device as not initialized */
	DEVICE->dsptch      = NULL;				/* Mark it unavailable */

/* Look up the driver (if internal) or handle piped drivers */
	if (PlotFindDriver(DEVICE->DriverName, &DEVICE->dsptch, &DEVICE->Class) != 0) {
		ERRprintf("ERROR: Device driver %s not recognized\n", DEVICE->DriverName);
		return(FALSE);
	}

	memset(&dsp, 0, sizeof(dsp));					/* Zero out everything			*/
	dsp.IO_Channel  = DEVICE->IO_Channel;		/* I/O channel string (info)	*/
	dsp.DriverBlock = &DEVICE->DriverBlock;	/* Driver data block pointer	*/
	dsp.Driver      = DEVICE->DriverName;		/* Actual driver name (info)	*/
	dsp.Class       = DEVICE->Class;				/* Class of driver				*/
	dsp.SubDevice   = DEVICE->SubDevice;		/* Sub-device within driver	*/
	dsp.Options     = DEVICE->Options;			/* Options parameter				*/
	dsp.NumberPens  = DEVICE->NumberPens;		/* Number of pens requested	*/
	dsp.Capabilities = DEV_CAP_GRAPHICS;		/* Default for devices			*/
	
/* If running OS/2 or NT, try to also pass handle of parent window for info */
#ifdef OS2
	dsp.hwnd_Parent = WinQueryActiveWindow(HWND_DESKTOP);
	dsp.hwnd_Focus  = WinQueryFocus(HWND_DESKTOP);
#elif defined NT
	if (SysMainWindowHwnd != NULL) {
		dsp.hwnd_Parent = dsp.hwnd_Focus = SysMainWindowHwnd;
	} else {
		dsp.hwnd_Parent = GetForegroundWindow();
		dsp.hwnd_Focus  = GetFocus();
	}
	sprintf(dsp.Window_Title, "Graph [%d]", Graph_Num);
	strcpy(dsp.Active_Mutex, Complot_Mutex_Name);
#endif
/*	TTYprintf("Handles are Parent: %p  Focus: %p\n", dsp.hwnd_Parent, dsp.hwnd_Focus); */

	if (! (*DEVICE->dsptch)(INIFNC, DEVICE->DriverBlock, (DSP *) &dsp)) {
		ERRprintf("ERROR: Unable to initialize %s device\n", DEVICE->DriverName);
		return(FALSE);
	}

/* Transfer back the information obtained from the driver initialization */
	DEVICE->NumberPens  = dsp.NumberPens;			/* Possibly revised */
	DEVICE->xperinch    = dsp.xperinch;
	DEVICE->yperinch    = dsp.yperinch;
	DEVICE->xmax		  = dsp.xmax;
	DEVICE->ymax        = dsp.ymax;
	DEVICE->Capabilities = dsp.Capabilities;
	DEVICE->ypmax       = dsp.ymax;					/* Give COMPLOT Y max pixels */
	DEVICE->Initialized = TRUE;						/* Mark device as initialized */
#ifdef NT
	SysGraphWindowHwnd  = dsp.hwnd_Graph;			/* Get the window if valid */
#endif

	PlotWindow->xloc  = 0.0f;							/* Current location to 0.0 */
	PlotWindow->yloc  = 0.0f;							/* Current location to 0.0 */
	PlotWindow->visib = 1;								/* Visible lines */

	PlotFixInternal();									/* Set up conversion scales */
	PlotConformDevice();									/* Conform the device	*/

	Graph_Num++;
	return(TRUE);
}

/* ============================================================================
--     PlotCloseDevice - Terminate all plotting on this device
--
--     Usage: PlotCloseDevice(void)
--
--     Notes: This is last routine called during a plotting session. Its effect
--            depends implicitly on the type device.  The common block is
--            marked as no device initialized (DEVINI).
============================================================================= */
void PlotCloseDevice(void) {

	if (DEVICE != NULL && DEVICE->Initialized) {					/* Only do as necessary */
		(*DEVICE->dsptch)(ENDFNC, DEVICE->DriverBlock, NULL);	/* Close down device    */
		DEVICE->Initialized = FALSE;									/* No longer initialized */
#ifdef NT
		SysGraphWindowHwnd  = NULL;									/* No more window */
#endif

	}
	return;
}
