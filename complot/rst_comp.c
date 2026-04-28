/* rst_comp.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE					/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
	#define	NEED_COMPLOT_MUTEX_INFO
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	RST_COMP_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"							/* Needed for GV routines */
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
static void PlotLinkVars(int itype);
static BOOL PlotLoadColors(int flag);
static int Color_To_RGB(char *name);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
PLOTDRIVER Null_Driver;						/* Assume they have right structure */

/* ------------------------------------ */
/* Declare & initialization global vars */
/* ------------------------------------ */
EXPORT PLT_DEVICEINFO *DEVICE=NULL;				/* Device structure pointer */
EXPORT PLT_DEVICEINFO *TABLET=NULL;				/* Tablet structure pointer */
EXPORT PLT_WINDOWINFO *PlotWindow=NULL;		/* Complot Window pointer   */

EXPORT AXIS_PARMS	PL_Axis;
EXPORT FIXPARMS	PL_Plot;
EXPORT SYMPARMS	PL_Symbols;

SUBPAGE		PL_SubPage;

CURVE		  *PlotExcludeCurve=NULL;		/* Pointer to curve to exclude */
REAL			PlotExcludeRadius=-0.18f;	/* Radius to exclude around	 */

INTEGER PL_segind[NTYPE+1] = {0,2,6,8,14,20,24};	/* Index into seglns */
INTEGER PL_seglns[24] = {	60, 40,						/* Dashed			*/
									40, 30,  0, 30,			/* Dashed-dot		*/
									 0, 25,						/* Dotted			*/
									60, 30,  0, 30,  0, 30,	/* Dash-dot-dot	*/
									30, 30, 30, 30,  0, 30,	/* Dash-dash-dot	*/
									60, 30, 30, 30 };			/* Long-short dash */

#ifdef NT
	char		Complot_Mutex_Name[32] = "";
	HANDLE	Complot_Mutex_Handle = NULL;
#endif

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ===========================================================================
--  Routine to initialize internal parts of Complot prior to general use.  
--  Must be executed prior to executing any other routines
--
--  Usage: PlotInitialize();
============================================================================ */
void PlotInitialize(void) {

	static INTEGER done=FALSE;
	
	if (done) return;									/* If we are done, return	 */
	done = TRUE;										/* But we only do this once */

/* ---------------------------------------------------------------------------
	1. Initialize any parameters not initialized in declarations 
	2. Load character data for symbol and parameters
	3. Allocate a default structure for describing a plot window
   4. Allocate a default structure for describing a device
--------------------------------------------------------------------------- */

	PlotLoadColors(0);							/* Load color database */

/*	TTYprintf("INFO: Loading fonts\n"); */
	if (! PlotLoadFonts(0)) {
		ERRprintf("FATAL ERROR: Unable to load necessary character sets\n");
		exit(EXIT_FAILURE);
	}

/*	TTYprintf("INFO: Creating plot window\n"); */
	if (! PlotAllocateWindow((void **) &PlotWindow)) {
		ERRprintf("FATA ERROR: Unable to allocate default PlotWindow structure\n");
		exit(EXIT_FAILURE);
	}

/*	TTYprintf("INFO: Creating default device structure\n"); */
	if (! PlotAllocateDevice((void **) &DEVICE)) {
		ERRprintf("FATA ERROR: Unable to allocate default DEVICE structure\n");
		exit(EXIT_FAILURE);
	}

/* Create a MUTEX semaphone (and own it) to let drivers know program is alive */
#ifdef NT
	sprintf(Complot_Mutex_Name, "Local\\GPT_PID_%d", getpid());
	if ( (Complot_Mutex_Handle = CreateMutex(NULL, TRUE, Complot_Mutex_Name)) == NULL) {
		char msg[2048];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, msg, sizeof(msg), NULL);
		ERRprintf("ERROR: Creating initialization mutex (%s) failed - tell developers (%d)\n", Complot_Mutex_Name);
		ERRprintf("  %s", msg);
	}
#endif

	PlotReset(TRUE);
	return;
}

/* ===========================================================================
-- The PlotConformDevice forces the currently active device to conform in its
-- settings to the values saved in the *PlotWindow structure.  This function
-- should be called each time the active device or window is changed to force
-- initial compatibility
--
-- Usage:  PlotConformDevice(void);
--
-- Output: Uses settings of *PlotWindow structure and applies them to the
--         device specified by the *DEVICE structure
--
-- Notes: Currently set parameters are:
--        1) color palette
--        2) pen speeds
--        3) current pen color
--        4) current line visibility
=========================================================================== */
void PlotConformDevice(void) {
	DSP dsp;
	
	if (DEVICE != NULL && DEVICE->Initialized) {
		PlotSetPalette(PlotWindow->palette);		/* Set the palette first */
		dsp.spd.speed = PlotWindow->spdsav;			/* For speed function */
		dsp.spd.pens  = 0;								/* Mark all pens */
		(*DEVICE->dsptch)(SPDFNC, DEVICE->DriverBlock, &dsp);
		PlotSetBrush(PlotWindow->colour, &dsp.col.brush);
		dsp.col.pen = dsp.col.brush.closest_index;
		(*DEVICE->dsptch)(COLFNC, DEVICE->DriverBlock, &dsp);
		dsp.vis.visible = PlotWindow->visib;
		(*DEVICE->dsptch)(VISFNC, DEVICE->DriverBlock, &dsp);
		dsp.lw.linewidth = PlotWindow->jstyle;
		(*DEVICE->dsptch)(LWFNC,  DEVICE->DriverBlock, &dsp);
	}
	return;
}


/* ===========================================================================
-- Usage:  PlotReset(LOGICAL FullReset);
--
-- Inputs: FullReset - if TRUE, reset all parameters.  If FALSE, may leave
--                     some subset unchanged (such as pen mapping)
--
-- Output: Resets common block values
--	  Turns off plotter if active (PLTEND)
--	  Turns off HCOPY if active (HCEND)
========================================================================== */
void PlotReset(LOGICAL FullReset) {

	int i;

/* Reset all Window parameters by reinitializing the Window */
	if (PlotWindow == NULL) {
		PlotAllocateWindow((void **) &PlotWindow);	/* Allocate and reset */
	} else {
		PlotResetWindow(PlotWindow, FullReset);
	}

/* SubPage information */
	if (FullReset) {
		PL_SubPage.active = PL_SubPage.init = FALSE;
		PL_SubPage.page   = 0;
		PL_SubPage.nrows  = PL_SubPage.ncols = 1;
	}

/* Symbol parameters */
	if (FullReset) {									/* Only reset mapping on full */
		for (i=0; i<NUMBER_OF_SYMBOLS+1; i++) {
			SymbolMap[i] = i;
			SymbolScale[i] = 1.0f;
		}
		SymbolMap[NUMBER_OF_SYMBOLS] = 0;		/* Except last loops around */
	}

/* Axis parameters */
	PL_Axis.MajorTick    =  0.16f;				/* Major tick marks			*/
	PL_Axis.MinorTick    =  0.08f;				/* Minor tick marks			*/
	PL_Axis.MinLabelSize = -0.07f;				/* Minimum character size	*/
	PL_Axis.MaxLabelSize =  0.18f;				/* Maximum character size	*/
	PL_Axis.TitleSize    =  0.22f;				/* Title size */

	PL_Axis.TitleOffset.Bottom = 0.0f;			/* Offset of axis labels	*/
	PL_Axis.TitleOffset.Top    = 0.0f;
	PL_Axis.TitleOffset.Left   = 0.0f;
	PL_Axis.TitleOffset.Right  = 0.0f;

	PL_Axis.Color.Line     = 0;				/* AXIS same color pen 1 */
	PL_Axis.Color.Major    = 0;
	PL_Axis.Color.Minor    = 0;
	PL_Axis.Color.Labels   = 0;
	PL_Axis.Color.Title    = 0;
	PL_Axis.Color.Tertiary = 0;
		
	PL_Axis.Width.Line     =  1.7f;				/* Main line is wide */
	PL_Axis.Width.Major    =  1.0f;				/* Others lines same */
	PL_Axis.Width.Minor    =  1.0f;
	PL_Axis.Width.Labels   =  1.0f;
	PL_Axis.Width.Title    =  1.0f;
	PL_Axis.Width.Tertiary =  1.0f;

/* Superscript/subscripts parameters */
	PL_Symbols.SuperscriptOffset = 0.75f;		/* Offset for superscripts */
	PL_Symbols.SubscriptOffset   = 0.375f;		/* Offset for subscripts	*/
	PL_Symbols.ScriptSize		  = 0.65f;		/* Height ratio				*/

/* ... Exclude information */
	PlotExcludeRadius = -0.18f;						/* - ==> not set		*/
	PlotExcludeCurve  = NULL;							/* No exclude curve	*/

/* ... Publish variables */
	PlotLinkVars(0xFF);

	PlotSystem(U_RESET,NULL,NULL);			/* Reset plot support package */
	HardCopy  (U_RESET);							/* Reset HardCopy processor   */
	PlotResetAnnote(FullReset);				/* Consider ANNOTE part of me */
	PlotID(0, 0, 0, 0, "**RESET**");			/* Reset the IDS code */

/* Reset the current device to conform to these settings */
	PlotConformDevice();							/* Make device conform to settings */
	if (DEVICE != NULL) {
		if (DEVICE->Initialized) PlotNewPage(-1);	/* Frame the device */
		DEVICE->Autoflush = TRUE;					/* By default, always cleanup */
	}

	Plot3DTransform(0, NULL, NULL);			/* Clear temp transformation */
	PlotFixInternal();							/* And fix all else */

	return;
}


/* ----------------------------------------------------------------------------
--  Routine to allocate and initialize a WindowInfo structure 
--
-- Usage: LOGICAL PlotAllocateWindow(WINDOWINFO **Window, LOGICAL FullReset);
--
-- Output: Returns pointer to window pointer (if necesary) filled appropriately
--
--	If the Window is already allocated and valid, will just be returned to the
-- initialized state.
---------------------------------------------------------------------------- */
LOGICAL PlotAllocateWindow(void **Window) {

	static int sequence=0;							/* Random sequence number */
	PLT_WINDOWINFO *LocWin;
	LOGICAL NewWindow=FALSE;

/* If Window does not appear to be correctly allocated, release it and redo */
	if (*Window != NULL) {
		LocWin = *((PLT_WINDOWINFO**) Window);
		if (LocWin->identifier != COMPLOT_VERSION) {
			free(*Window);
			*Window = NULL;
		}
	}

	if (*Window == NULL) {							/* Are we to allocate?			*/
		if ( (*Window = calloc(sizeof(PLT_WINDOWINFO), 1)) == NULL) return(FALSE);
		NewWindow = TRUE;								/* And we are now new window	*/
	}
	LocWin = *((PLT_WINDOWINFO **) Window);
	
	LocWin->identifier = COMPLOT_VERSION;		/* Version number as ID */
	LocWin->sequence   = ++sequence;
	LocWin->LastWindow = NULL;

/* Handle the save unit for commands */
	if (NewWindow) {
		LocWin->Save.On     = FALSE;						/* Not on enabled	*/
		LocWin->Save.Unit   = NULL;						/* No valid unit	*/
		*LocWin->Save.Name  = '\0';						/* Name is null */
		LocWin->Save.svptr  = LocWin->Save.block;		/* This is valid */
	}

	PlotResetWindow(LocWin, TRUE);						/* Full reset of window	*/

	return(TRUE);
}


/* ===========================================================================
-- Routine to reset a window structure to some standard initial state.
--
-- Reset can be full or partial - some parameters such as color mapping
-- remain unchanged across a partial reset.
=========================================================================== */
void PlotResetWindow(void *Window, LOGICAL FullReset) {
	
	int i;
	PLT_WINDOWINFO *LocWin;

#define	DEFAULT_PALETTE_SIZE	16
	static INT32 DefaultPalette[DEFAULT_PALETTE_SIZE] = {
		C_WHITE,			C_BLACK,			C_RED,		C_GREEN,			C_BLUE, 
		C_MAGENTA,		C_CYAN,			C_YELLOW,	C_DARKRED,		C_DARKGREEN,
		C_DARKBLUE,		C_DARKMAGENTA, C_DARKCYAN,	C_DARKYELLOW,	C_DARKGRAY,
		C_PALEGRAY };
	
	LocWin = (PLT_WINDOWINFO *) Window;

	LocWin->orient  = LANDSCAPE;					/* Normal plotting		*/
	LocWin->visib   = 1;
	LocWin->colour  = 1;								/* Pen 1 initially		*/
	LocWin->PageFillColor = -1;					/* No page fill			*/
	LocWin->AreaFillColor = -1;					/* No area fill			*/
	LocWin->AxesFillColor = -1;					/* No axes fill			*/
	LocWin->spdsav  = 18;							/* Default speed			*/
	LocWin->clipsymbols = TRUE;					/* Symbols are clipped	*/

	LocWin->xloc    = 0.0f;							/* Current location to 0,0 */
	LocWin->yloc    = 0.0f;

	if (FullReset) {
		free(LocWin->palette);
		LocWin->palette = malloc(sizeof(PLOT_PALETTE) + DEFAULT_PALETTE_SIZE*sizeof(INT32));
		LocWin->palette->num_entries = DEFAULT_PALETTE_SIZE;
		LocWin->palette->num_pens    = DEFAULT_PALETTE_SIZE-1;
		for (i=0; i<DEFAULT_PALETTE_SIZE; i++) {
			LocWin->palette->rgb[i] = DefaultPalette[i];
		}
		GVLinkIntArray("$PALETTE", GVF_INTERNAL | GVF_HIDDEN, PlotWindow->palette->rgb, PlotWindow->palette->num_entries, NULL);
	}
	
/* Reset the plot size etc. */
	LocWin->mode_3d = FALSE;						/* Not in 3D mode to begin */
	LocWin->clpmod  = 0;								/* Use BOX mode by default */
	LocWin->factr   = 1.0f;							/* Shrink factor */
	LocWin->xsize   = 9.0f;							/* Default size */
	LocWin->ysize   = 7.45f;
	LocWin->xorg    = 0.35f;						/* Starting origin */
	LocWin->yorg    = 0.0f;
	LocWin->xmarg[0] = 1.00f;						/* Default margin sizes */
	LocWin->xmarg[1] = 0.85f;
	LocWin->ymarg[0] = 0.85f;						/* Default margin sizes */
	LocWin->ymarg[1] = 0.85f;

/* Reset the plot scales etc. */
	LocWin->xmin      = 0.0f;
	LocWin->xmax      = 1.0f;
	LocWin->ymin      = 0.0f;
	LocWin->ymax      = 1.0f;
	LocWin->zmin      = 0.0f;
	LocWin->zmax      = 1.0f;
	LocWin->rotate[0] = 30.0f;						/* Rotation (in degrees)	*/
	LocWin->rotate[1] = 30.0f;						/* Tilt     (in degrees)	*/
	LocWin->rotate[2] = 0.0f;						/* Skew		(in degrees)	*/
	LocWin->view_d    = 100.0f;					/* Viewing distance			*/

/* ... Turn off user scaling */
	LocWin->usrnbl  = FALSE;						/* User coordinates disabled */
	LocWin->xfact   = 1.0f;							/* No user changes yet */
	LocWin->xoff    = 0.0f;
	LocWin->yfact   = 1.0f;							/* Same in Y */
	LocWin->yoff    = 0.0f;

/* Reset line drawing and symbol parameters */
	LocWin->jstyle = 7;									/* Line sytle 7 ==> 1/150 dpi */
	LocWin->lintyp = 1;									/* Initial linetype */
	LocWin->patsiz = 0.003f;							/* Pattern size */
	LocWin->chrmap = PL_Symbols.DefaultCharMap;	/* Character map (default) */
   LocWin->zforce = 0.3f;								/* For AUTOSCALE routine */

/* Default conditions for IDS routine */
	LocWin->ID.leftskip = 0.20f;						/* Space from  left axis line */
	LocWin->ID.topskip  = 0.40f;						/* Space under top  axis line */
	LocWin->ID.size     = 0.15f;						/* Height of chars (inches) */
	LocWin->ID.spacing  = 1.50f;						/* Line spacing (id.size units) */
	LocWin->ID.linesize = 0.65f;						/* Length of example line */
	LocWin->ID.vecsize  = 0.003f;

/* Time stamping information */
	LocWin->TimeStamp.X      = 0.01f;
	LocWin->TimeStamp.Y      = 0.01f;
	LocWin->TimeStamp.Size   = 0.10f;
	strscpy(LocWin->TimeStamp.Encode,"%H:%M:%S - %A %B %d, %Y",sizeof(LocWin->TimeStamp.Encode));

	return;
}

/* ----------------------------------------------------------------------------
--   Routine to allocate and initialize a DeviceInfo structure
--
--
---------------------------------------------------------------------------- */
LOGICAL PlotAllocateDevice(void **RequestDevice) {

	if (*RequestDevice != NULL) free(*RequestDevice);

	if ( (*RequestDevice = malloc(sizeof(PLT_DEVICEINFO))) == NULL) 
		return(FALSE);

	PlotResetDevice(*RequestDevice, TRUE);
	return(TRUE);
}

/* ===========================================================================
-- Routine to reset a device structure to some standard initial state.
--
-- Reset can be full or partial - some parameters may remain unchanged.
=========================================================================== */
void PlotResetDevice(void *Device, LOGICAL FullReset) {

	PLT_DEVICEINFO *LocDev;							/* Local device pointer */

	LocDev = (PLT_DEVICEINFO *) Device;
	
	LocDev->Initialized    = FALSE;				/* Device not initialized */
	LocDev->dsptch         = Null_Driver;		/* Pointer to device handler */
	LocDev->DriverBlock    = NULL;				/* Driver block blank for now */
	LocDev->Class          = 0;					/* Device is unknown */
	LocDev->SubDevice      = 0;					/* Secondary device # unknown */
	LocDev->Options        = 0;					/* No known options  */
	LocDev->xperinch		  = 100;					/* Device parameters */
	LocDev->yperinch		  = 100;
	LocDev->xmax			  = 1000;
	LocDev->ymax			  = 800;
	LocDev->Capabilities   = DEV_CAP_GRAPHICS;
	LocDev->Autoflush      = TRUE;				/* Autoflush on device enabled */

	return;
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
LOGICAL	PlotPushState(void) {

	PLT_WINDOWINFO *LocWin;
	struct _SAVE *SV, *NV;
	void *tmpspace;

	if (PlotWindow == NULL) 					/* If nothing, create one now	*/
		return(PlotAllocateWindow((void **) &PlotWindow));

	if ( (LocWin = (PLT_WINDOWINFO *) malloc(sizeof(PLT_WINDOWINFO))) == NULL) return(FALSE);

	*LocWin = *PlotWindow;						/* Duplicate everything */
	LocWin->LastWindow = PlotWindow;			/* Almost! */

	if (PlotWindow->Save.Unit != NULL) {	/* Have to deal with HCOPY? */
		SV = &PlotWindow->Save;
		NV = &LocWin->Save;
		NV->svptr = NV->block + (SV->svptr-SV->block);
		if ( (NV->Unit  = SysTmpFile(NV->Name, NULL, ".hcp", "w+b")) == NULL) {
			gen_err("Unable to duplicate HCOPY file in system push");
			NV->On = FALSE;
		} else {
			tmpspace = malloc(sizeof(SV->block));
			SysQualifyPath(NV->Name, NV->Name, sizeof(NV->Name));
			fflush(SV->Unit); rewind(SV->Unit);
			while (fread(tmpspace, sizeof(SV->block), 1, SV->Unit) == 1)
				fwrite(tmpspace, sizeof(SV->block), 1, NV->Unit);
			free(tmpspace);
		}
	}
	PlotWindow = LocWin;							/* And install it */
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
LOGICAL	PlotPopState(void) {

	if (PlotWindow == NULL || PlotWindow->LastWindow == NULL) return(FALSE);
	HardCopyCloseSave();
	PlotWindow = PlotWindow->LastWindow;
	PlotLinkVars(0x01);
	PlotConformDevice();
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
static void PlotLinkVars(int itype) {
	
	if (itype & 0x01) {						/* PlotWindow vars */
		GVLinkReal  ("$SCALING",GVF_INTERNAL|GVF_HIDDEN|GVF_CONSTANT, &PlotWindow->factr);
		GVLinkArray ("$SIZE",   GVF_INTERNAL|GVF_HIDDEN|GVF_CONSTANT, &PlotWindow->xsize, 2, NULL);
		GVLinkArray ("$MARGIN", GVF_INTERNAL|GVF_HIDDEN|GVF_CONSTANT,  PlotWindow->xmarg, 4, NULL);
		GVLinkArray ("$OFFSET", GVF_INTERNAL|GVF_HIDDEN|GVF_CONSTANT, &PlotWindow->xorg,  2, NULL);

		GVLinkReal  ("$ZFORCE",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->zforce);
		GVLinkReal  ("$IDLSKP",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.leftskip);
		GVLinkReal  ("$IDTSKP",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.topskip);
		GVLinkReal  ("$IDSIZE",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.size);
		GVLinkReal  ("$IDSPAC",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.spacing);
		GVLinkReal  ("$IDLSIZ",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.linesize);
		GVLinkReal  ("$IDVSIZ",		GVF_INTERNAL | GVF_HIDDEN, &PlotWindow->ID.vecsize);

		GVLinkArray ("$TSTAMP",		GVF_INTERNAL | GVF_HIDDEN, (REAL *) &PlotWindow->TimeStamp, 3, NULL);	/* Array */
		GVLinkString("$TSTAMP:FORMAT", GVF_INTERNAL | GVF_HIDDEN, PlotWindow->TimeStamp.Encode, sizeof(PlotWindow->TimeStamp.Encode));
	}
	if (itype & 0x02) {						/* PL_Axis vars */
		GVLinkReal  ("$TICK",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.MajorTick);
		GVLinkReal  ("$TICK2",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.MinorTick);
		GVLinkReal  ("$CSMIN",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.MinLabelSize);
		GVLinkReal  ("$CSMAX",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.MaxLabelSize);
		GVLinkReal  ("$TSIZE",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.TitleSize);
		GVLinkReal  ("$TBOFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.TitleOffset.Bottom);
		GVLinkReal  ("$TTOFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.TitleOffset.Top);
		GVLinkReal  ("$TLOFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.TitleOffset.Left);
		GVLinkReal  ("$TROFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Axis.TitleOffset.Right);
		GVLinkIntArray("$AXCOLR",	GVF_INTERNAL | GVF_HIDDEN, (INTEGER *) &PL_Axis.Color, 6, NULL);	/* Array */
		GVLinkArray ("$AXWIDTH",	GVF_INTERNAL | GVF_HIDDEN, (REAL *) &PL_Axis.Width, 6, NULL);	/* Array */
	}
	if (itype & 0x04) {
		GVLinkReal  ("$SUSIZ",		GVF_INTERNAL | GVF_HIDDEN, &PL_Symbols.ScriptSize);
		GVLinkReal  ("$SUBOFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Symbols.SubscriptOffset);
		GVLinkReal  ("$SUPOFF",		GVF_INTERNAL | GVF_HIDDEN, &PL_Symbols.SuperscriptOffset);
	}
	if (itype & 0x08) {
		GVLinkReal  ("EX$RADIUS",	GVF_INTERNAL | GVF_HIDDEN, &PlotExcludeRadius);	/* And the radius	*/
	}
	return;
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
void PlotShutDown(void) {

	PlotCloseDevice();									/* Close device */
	while (PlotWindow != NULL) {						/* Pop through memory */
		HardCopyCloseSave();
		PlotWindow = PlotWindow->LastWindow;
	}

/* Release the COMPLOT mutex so everyone can exit */
#ifdef NT
	if (Complot_Mutex_Handle != NULL) ReleaseMutex(Complot_Mutex_Handle);
	Complot_Mutex_Handle = NULL;
	*Complot_Mutex_Name = '\0';
#endif

	return;
}



/* ===========================================================================
--  PlotLoadColors - Loads colorname database
--
--  Usage: BOOL PlotLoadColors(int flag)
--
--  Inputs: flag - what to do
--                -1 ==> free the colortable in expectation of program exit
--                 1 ==> release the colortable and reload
--             other ==> just load
--
--  Output: On first entry, loads character set data from unformatted file
=========================================================================== */
static BOOL PlotLoadColors(int flag) {

	CHAR file[PATH_MAX];						/* Filename				*/
	FILE *unt=NULL;							/* File stream unit	*/
	int bufspace, tabledim, tablesize, lineno, r,g,b;
	char name[255], line[1024], *namebuf, *aptr;
	BOOL errors;
	
/* Is this a request for reload, or exit unload? */
	if (flag == -1 || flag == 1) {		/* Free known memory for exit */
		if (ColorTable != NULL) {
			free(ColorTable[1].name);		/* At least the first block */
			free(ColorTable);
			ColorTable = NULL;
			if (flag == -1) return(TRUE);
		}
	}

/* If already loaded, don't repeat */
	if (ColorTable != NULL) return(TRUE);

/* Load the file from standard locations */
	SysResolveDyntName(file, "colormap.dat", sizeof(file));		/* Open file */

	if ( (unt=fopen(file, "rb")) == NULL) {
		ERRprintf("WARNING: Unable to open color mapping file (%s)\n", file);
		return(FALSE);
	}

/* Okay, initialize and start reading */
	bufspace   = 0;
	tabledim   = 0;
	tablesize  = 0;
	lineno = 0;
	errors = FALSE;
	while (fgets(line, sizeof(line), unt)) {					/* Read a line */
		lineno++;
		if ( (aptr = strstr(line, "/*")) != NULL) *aptr = '\0';
		if ( (aptr = strchr(line, '#')) != NULL) *aptr = '\0';
		aptr = line;
		while (isspace(*aptr)) aptr++;
		if (*aptr == '\0') continue;

		if (sscanf(aptr, " %d %d %d %s", &r, &g, &b, name) != 4) {
			ERRprintf("ERROR: colormap.dat line %d did not parse 4 objects\n\t%s\n", lineno, aptr);
			errors = TRUE;
			continue;
		} else if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
			ERRprintf("ERROR: colormap.dat line %d contains invalid RGB specification\n\t%s\n", lineno, aptr);
			errors = TRUE;
			continue;
		}

/* Increase size of ColorTable if needed */
		if (tabledim < tablesize+2) {
			tabledim += 400;										/* Add 400 color names */
			ColorTable = realloc(ColorTable, sizeof(*ColorTable)*tabledim);
		}
/* Have big buffer space for the name allocations */
		if (bufspace < (int) (strlen(name)+1)) {
			namebuf = malloc(4096);
			bufspace = 4096;
		}

		ColorTable[tablesize].color = MY_RGB(r,g,b);
		ColorTable[tablesize].name  = namebuf;
		tablesize++;
		ColorTable[tablesize].name = NULL;					/* Stop marker */

		strcpy(namebuf, name);
		namebuf += strlen(name)+1;
		bufspace -= (int) strlen(name)+1;
	}
	fclose(unt);
	
/* Link the inverse process into LEXP for handling RGB_Color() */
	GVLinkFnc("RGB_Color", 0, 1, (EXT_FNC_LINK *) &Color_To_RGB);

	TTYprintf("INFO: Loaded %d colors from %s\n", tablesize, file);
	if (errors) sleep(3);
	return(TRUE);
}

static int Color_To_RGB(char *name) {
	int i;

	if (ColorTable != NULL) {
		for (i=0; ColorTable[i].name != NULL; i++) {
			if (stricmp(name, ColorTable[i].name) == 0) return(ColorTable[i].color);
		}
	}
	return(0);
}
