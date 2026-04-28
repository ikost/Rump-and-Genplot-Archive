/* plsystem.c */

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
#include <time.h>
#include <ctype.h>
#include <limits.h>
#ifdef NT
	#include <windows.h>
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	FLMAGIC	-2187304.0f					/* Magic # (exact for float cmps) */

#define  UNIMPLEMENTED -2
#define  NOMORE        -1
#define  OKAY           0

typedef struct _SYMBOLLOOKUP {
	char *name;
	int  symbol;
} SYMBOLLOOKUP;

typedef struct _CMTYPE {
	CHAR *command;
	INTEGER minlen;
	int (*fnc)(void);
} CMTYPE;

#define INITIALIZED	(DEVICE->Initialized)			/* Is device initialized */
#define HASCURSOR		(DEVICE->StatusBits & 0x01)	/* Does it have a cursor */
#define NUMBERPENS	(min(DEVICE->NumberPens,PlotWindow->palette->num_pens))				/* How many pens allowed */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int do_device(void);
static int do_erase(void);
static int do_newpage(void);
static int do_pagesplit(void);
static int do_color(void);
static int do_lw(void);
static int do_font(void);
static int do_title(void);
static int do_tstamp(void);
static int do_hcopy(void);
static int do_size(void);
static int do_offset(void);
static int do_shrink(void);
static int do_margins(void);
static int do_flipxy(void);
static int do_orient(void);
static int do_speed(void);
static int do_autoerase(void);
static int do_subplot(void);
static int do_visible(void);
static int do_vecsize(void);
static int do_sgraph(void);
static int do_fill(void);
static int do_encursor(void);
static int do_calc_autoscale(void);
static int do_calc_autologscale(void);
static int do_grprint(void);
static int do_grpage(void);
static int do_grflush(void);
static int do_colormap(void);
static int do_palette(void);
static int do_symbolmap(void);
static int do_clipsyms(void);
static int do_axesfillcolor(void);
static int do_pagefillcolor(void);
static int do_areafillcolor(void);

static int text_draw(REAL x, REAL y, REAL ts, CHAR *token);
static char *ColorNameFromValue(int color);

static void lex_xcursor(char *str, int len);
static void lex_ycursor(char *str, int len);
static void lex_xycursor(char *str, int len);
static void lex_boxcursor(char *str, int len);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static char	 DeviceName[VARNAME_STR_SIZE] = "AUTO";
static LOGICAL DoErase        = TRUE;
static LOGICAL CursorEnabled  = TRUE;
static int		 CurrentPen     = 1;
static int		 SelectPen      = 1;

static SYMBOLLOOKUP SymbolLookup[] = {
	{"OpenSquare",				S_OPENSQUARE},			{"Box",			S_OPENSQUARE},
	{"OpenCircle",				S_OPENCIRCLE},
	{"OpenTriangle",			S_OPENTRIANGLE},
	{"Cross",					S_CROSS},				{"Plus",			S_CROSS},
																{"+",				S_CROSS},
	{"X",							S_X},
	{"OpenDiamond",			S_OPENDIAMOND},		{"Diamond",		S_OPENDIAMOND},
	{"OpenStar",				S_OPENSTAR},
	{"FilledSquare",			S_FILLEDSQUARE},		{"Square",		S_FILLEDSQUARE},
	{"FilledBox",				S_FILLEDSQUARE},
	{"FilledCircle",			S_FILLEDCIRCLE},		{"Circle",		S_FILLEDCIRCLE},
	{"FilledTriangle",		S_FILLEDTRIANGLE},	{"Triangle",	S_FILLEDTRIANGLE},

	{"FilledLeftTriangle",	S_FILLEDLEFTTRIANGLE},
	{"LeftFilledTriangle",	S_FILLEDLEFTTRIANGLE},
	{"FilledRightTriangle",	S_FILLEDRIGHTTRIANGLE},
	{"RightFilledTriangle",	S_FILLEDRIGHTTRIANGLE},

	{"Asterisk",				S_ASTERISK},			{"*",				S_ASTERISK},
	{"Asterix",					S_ASTERISK},			{"Obelisk",		S_ASTERISK},
	{"FilledStar",				S_FILLEDSTAR},			{"Star",			S_FILLEDSTAR},
	{"StarOfDavid",			S_STAROFDAVID},		{"David",		S_STAROFDAVID},
	{NULL,						0}
};


/* ============================================================================
-- Usage Guide:
--
--     LOGICAL FUNCTION PLSUPP(KEY, TOKE, PARMS)
--
--    Logical PLSUPP does commands which are strictly plotting in nature.
--
--    INPUTS:   KEY      Chooses the operation:
--                  6 = Info request: parms(1) = jpen
--                                    parms(2) = penmax
--                                    parms(3-5) reserved
--                  5 = Select next pen PARMS(1) > 0 autochange, =<0 no change
--                  4 = Query pen validity PARMS(1)= pen
--                  3 = Query cursor availablility
--                  2 = Is plotter "turned on"?
--                  1 = Prepare new page/screen
--                  0 = Process command
--                 -1 = Initialize
--                 -2 = Reset
--                 -3 = List Commands
--                 -4 = Display Parameters
--                 -5 = Turn Off
--                 -6 = Return TRUE if command is recognized.  No action!
--
--              TOKE     is the command (Character string) for the case KEY = 0
--
--              PARMS    Parameter list for special keys (dependent)
--
--    OUTPUT:   LOG      (Function value) True for Key = 0 and the command
--                       was found and action was attempted.  Also True for
--                       Key = 1 and the screen was sucessfully prepared.
--                       Keys > 1 ask a question, Function value is answer.
--                       Otherwise False.
--
--    COMMON BLOCKS:     None
--    CALLS:             Complot
--
-- ... Note: Changed to use internal COMPLOT common blocks for values
============================================================================ */
LOGICAL PlotSystem(int key, char *token, int *parms) {

	int rcode, statme;
	CMTYPE *citem;										/* For the match */
	static const CMTYPE cmlist[] = {
			{"DEVICE",		2,		do_device},
			{"ERASE",		2,		do_erase},
			{"NEWPAGE",		4,		do_newpage},
			{"PAGESPLIT",	4,		do_pagesplit},	{"SPLITPAGE", -5,		do_pagesplit},
			{"PEN",			2,		do_color},		{"COLOR",		-3,	do_color},
			{"LINEWIDTH",	5,		do_lw},			{"LW",			-2,	do_lw},	
															{"LINESTYLE",	-5,	do_lw},
			{"CHRSET",		3,		do_font},		{"FONT",			-4,	do_font},
															{"CHARACTER",	-5,	do_font},
			{"TITLE",		3,		do_title},
			{"TIMESTAMP",	5,		do_tstamp},
			{"HCOPY",		2,		do_hcopy},
			{"SIZE",			3,		do_size},
			{"OFFSET",		2,		do_offset},
			{"SHRINK",		2,		do_shrink},
			{"MARGINS",		4,		do_margins},
			{"FLIPXY",		6,		do_flipxy},
			{"ORIENTATION",6,		do_orient},
			{"PAPERCOLOR",	  -6, do_pagefillcolor},
			{"PAGECOLOR",	  -5, do_pagefillcolor},
			{"PAGEFILLCOLOR", 8, do_pagefillcolor},
			{"AREAFILLCOLOR", 8, do_areafillcolor},
			{"AXESFILLCOLOR", 8, do_axesfillcolor},
			{"AXISFILLCOLOR",-8, do_axesfillcolor},
			{"SPEED",		2,		do_speed},
			{"AUTOERASE",	5,		do_autoerase},
			{"SUB_PLOT",	5,		do_subplot},	{"SUBPLOT", -4,	do_subplot},
			{"VISIBILITY",	3,		do_visible},
			{"VECSIZE",		2,		do_vecsize},
			{"SGRAPH",		4,		do_sgraph},		
			{"FILL",			4,		do_fill},
			{"ENCURSOR",	2,		do_encursor},
			{"CALC_AUTOSCALE",		-10, do_calc_autoscale},
			{"CALC_AUTOLOGSCALE",	-10, do_calc_autologscale},
			{"PALETTE",	   3,		do_palette},
			{"COLORMAP",	6,		do_colormap},
			{"SYMBOLMAP",	7,		do_symbolmap},
			{"CLIPSYMBOLS",7,		do_clipsyms},
			{"CLIPSYMS",  -8,		do_clipsyms},
			{"GRPRINT",		4,		do_grprint},		/* Command graph window to print */
			{"GRPAGE",		4,		do_grpage},			/* Set new graph window */
			{"GRFLUSH",		4,		do_grflush},		/* Flush the graphics buffer */
			{NULL,			0,		NULL} };

/* --- Okay -- what do they want us to do? */
	switch (key) {
		case -6:
		case 0:
			citem = LexCmdl(token, cmlist, sizeof(CMTYPE));
			if (key == -6)	
				return(citem != NULL);
			else if (citem == NULL) 
				return(FALSE);
			else if ( (rcode = (*citem->fnc)()) < 0) {
				if (rcode == UNIMPLEMENTED)
					ERRprintf("ERROR: Sorry -- that function is not implemented yet\n");
				else
					ERRprintf("ERROR: Problem executing command %s\n", token);
				LexFlush();
			}
			return(TRUE);

		case 1:				/* FRAME   - Generate a new page/screen for output	*/
			if (! INITIALIZED) {
				if (PlotSelectDevice(DeviceName, &statme) != 0) return(FALSE);
				PlotSetPenSpeed(PlotWindow->spdsav, 0);	/* Reset the speed	*/
				PlotNewPage(-1);
			}
			if (DoErase) PlotNewPage(0);
			CurrentPen = SelectPen;						/* Reset CurrentPen if < 0 */
			PlotSelectPen(abs(CurrentPen));			/* Set CurrentPen in place */
			return(TRUE);									/* All done						*/

		case 2:				/* QUERIES - Is there a device initialized */
			return(INITIALIZED);
		
		case 3:				/* QUERIES - Is device ready and able for cursor */
			return(INITIALIZED && HASCURSOR && CursorEnabled);
			
		case 4:				/* QUERIES - Ask if pen is okay */
			return(*parms <= NUMBERPENS);
			
		case 5:				/* CHGPEN  - Change pen with possible option */
			PlotSelectPen(abs(CurrentPen));
			if ((*parms > 0) && (CurrentPen < 0)) 
				CurrentPen = (CurrentPen % NUMBERPENS) - 1;
			return(TRUE);
			
		case 6:				/* QUERIES - Info Query */
			*(parms++) = CurrentPen;
			*(parms++) = NUMBERPENS;
			return(TRUE);

		case U_INIT:		/* Initialize and reset */
			LexAddAmpEntry("&xcursor", lex_xcursor);		/* LEXP cursor fncs */
			LexAddAmpEntry("&ycursor", lex_ycursor);
			LexAddAmpEntry("&xycursor", lex_xycursor);
			LexAddAmpEntry("&boxcursor", lex_boxcursor);
			return(TRUE);

		case U_RESET:		/* Reset parameters */
			CursorEnabled = TRUE;
			DoErase       = TRUE;
			CurrentPen    = 1;
			SelectPen     = 1;
			return(TRUE);

		case U_HELP:	/* Out portion of the help list */
			LexCmdlPrint(cmlist, sizeof(CMTYPE), "Plotting Support Commands:");
			return(TRUE);
				
/* ----------------------------------------------------------------------------
-- Plot device: AUTO
-- Size:    +xx.xx by +xx.xx           Shrink: xx.xxx
-- Offset:  +xx.xx by +xx.xx           Pen:    iii of maximum iii
-- Margin:  +xx.xx by +xx.xx           Speed:  iii
-- Cursor is disabled
-- Auto-erase is disabled
---------------------------------------------------------------------------- */
		case U_PARM:	/* Our portion of the parameter list */
			TTYprintf(" Plot device: %s\n", DeviceName);
			TTYprintf(" Size:  %8.2f by %7.2f               Shrink: %7.3f\n", 
				PlotWindow->xsize, PlotWindow->ysize, 1.0/PlotWindow->factr);
			TTYprintf(" Offset:%8.2f by %7.2f               Pen:    %3i of max %3i\n", 
				PlotWindow->xorg,  PlotWindow->yorg,  CurrentPen, NUMBERPENS);
			TTYprintf(" Margin:%8.2f (bottom)%8.2f (left)  Speed:  %3i\n",
				PlotWindow->ymarg[0], PlotWindow->xmarg[0], PlotWindow->spdsav);
			TTYprintf("        %8.2f (upper) %8.2f (right) Linewidth:  %5.2f\n", PlotWindow->ymarg[1], PlotWindow->xmarg[1], PlotSetLineWidth(-1)/7.0f);
			TTYprintf("\n");
			if (! CursorEnabled)					TTYputs(" Cursor is disabled.\n");
			if (CursorEnabled && HASCURSOR)	TTYputs(" Cursor available.\n");
			if (! DoErase)							TTYputs(" Auto-erase is disabled.\n");
			HardCopy(U_PARM);
			return(TRUE);

		case U_QUIT:	/* Out shutdown responsibility */
			PlotShutDown();
			return(TRUE);
	}
	return(TRUE);
}


/* ------------------------------------------------------------
--     DEVICE CODE TO SPECIFY ANY DEVICE
------------------------------------------------------------ */
static int do_device(void) {
	
	char token[DFLT_STR_SIZE];
	int  ierr,statme;

	if (! LexGetTokenP(token, sizeof(token), "Plotting device or cmd option: (no change) "))
		return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);
	
	if (*token == '-') {									/* Is it an option */
		DspIOCTL dsp={NULL,0};
		switch (LexSelect(token, "-? -OPTION -TEXT -GRAPH -NOGRAPH -NOTEXT -CHECK -FILENAME -QUERY -REPAINT -REDRAW")) {
			case 1:
				TTYprintf("\n"
" Usage: DEVice [ dev_name | -option ]\n\n"
" Options are identified by a leading hyphen and may or may not accept arguments\n"
"       -?               - quick help\n"
"       -OPTION <token>  - token passed to driver as driver specific command\n"
"       -TEXT            - request driver to display text window\n"
"       -GRAPH           - request driver to display graph window\n"
"       -NOGRAPH         - request driver to turn off graph window\n"
"       -NOTEXT          - request driver to turn off text window\n"
"       -REPAINT         - repaints the device screen\n"
"       -REDRAW          - synonymous with repaint\n"
"       -CHECK           - if a device has been initialized, do nothing.\n"
"                          Otherwise do a \"device auto\"\n"
"       -FILENAME <file> - change the internal name of devices.dat\n"
"       -QUERY           - prints out current device and device.dat filename\n"
" For the device name, the name \"auto\" causes the GTERM variable to be \n"
" scanned and used -- or uses a system default device.\n\n");
				return(OKAY);
			case 2:
				if (! LexGetTokenP(token, sizeof(token), "Driver option string: ") || LexEscape(TRUE)) return(OKAY);
				dsp.ioctl = 0;							/* Command string option */
				dsp.str = token;
				break;
			case 3:
				dsp.ioctl = +2;
				break;
			case 4:
				dsp.ioctl = +3;
				break;
			case 5:
				dsp.ioctl = -3;
				break;
			case 6:
				dsp.ioctl = -2;
				break;
			case 7:										/* Initialize if not already */
				if (INITIALIZED) return(OKAY);
				strcpy(token, "auto");
				goto DO_DEVICE_NAME;
			case 8:
				if (LexGetFileP(token, sizeof(token), "Replacement devices.dat filename (no change): ")) {
					strscpy(PlotDeviceDatFilename, token, sizeof(PlotDeviceDatFilename));
				}
				return(OKAY);
			case 9:
				TTYprintf("  device.dat file: %s\n  current device name: %s\n", 
							 PlotDeviceDatFilename, DeviceName);
				return(OKAY);
			case 10:
			case 11:
				dsp.ioctl = +4;
				break;
			default:
				ERRprintf("ERROR: Unrecognized device option (%s)\n", token);
				return(NOMORE);
		}
#ifdef NT
/* Because of XP changes, activation of text window must occur from the
-- graph screen if active, and graph activated from text screen if active */
		if       (SysGraphWindowHwnd != NULL && (dsp.ioctl == 3 || dsp.ioctl == -2) ) {
			if (IsIconic(SysGraphWindowHwnd)) ShowWindow(SysGraphWindowHwnd, SW_RESTORE);
			SetForegroundWindow(SysGraphWindowHwnd);
		} else {
			(*DEVICE->dsptch)(IOCTL, DEVICE->DriverBlock, (DSP *) &dsp);
		}
#else
		(*DEVICE->dsptch)(IOCTL, DEVICE->DriverBlock, (DSP *) &dsp);
#endif

		return(OKAY);
	}

DO_DEVICE_NAME:
	strscpy(DeviceName, token, sizeof(DeviceName));		/* And keep track for me */
	if ( (ierr = PlotSelectDevice(token, &statme)) != 0) 
		return( (ierr<0) ? NOMORE : OKAY);
	PlotNewPage(-1);									/* Get a full new page */
	PlotSetPenSpeed(PlotWindow->spdsav, 0);
	CurrentPen = SelectPen;							/* Reset CurrentPen if < 0 */
	PlotSelectPen(abs(CurrentPen));				/* Set CurrentPen in place */
	return(OKAY);
}

/* ----------------------------------------------------------------
-- NEWPAGE - Forces next graph to appear on a new page (SubPage only)
-- ERASE - ERASE THE SCREEN OR DOES A FRAME TO DEVICE ACTIVE 
----------------------------------------------------------------- */
static int do_pagesplit(void) {

	char token[DFLT_STR_SIZE];
	int row,col, ipage;

	if (! LexGetTokenP(token, sizeof(token), "PageSplit (? for help):" ))
		 return(OKAY);

	if (LexEqual(token, "-?", 2) || LexEqual(token, "?", 1) || LexEqual(token, "help", 1)) {
		TTYprintf("PageSplit { set <rows> <cols> | cancel | select <row> <col> }\n");
		return(OKAY);
	} else if (LexEqual(token,"reset",3) || LexEqual(token,"quit",1) || LexEqual(token,"cancel",3)) {
		PL_SubPage.active = FALSE;
	} else if (LexEqual(token, "set", 3)) {
		row = LexGetInt(1, "Number of rows (1): ");
		col = LexGetInt(1, "Number of columns (1): ");
		PL_SubPage.nrows = max(1, row);
		PL_SubPage.ncols = max(1, col);
		PL_SubPage.active = (PL_SubPage.nrows*PL_SubPage.ncols > 1);
		PL_SubPage.page = 0;
		PL_SubPage.need_erase = TRUE;
		PL_SubPage.page_set = TRUE;
		PL_SubPage.init = FALSE;
		PlotNewPage(-1);
	} else if (LexEqual(token, "select", 3)) {
		row = LexGetInt(1, "Row (1): ");
		col = LexGetInt(1, "Column (1): ");
		row = (max(row,1)-1)%PL_SubPage.nrows;
		col = (max(col,1)-1)%PL_SubPage.ncols;
		ipage = 1 + row*PL_SubPage.ncols + col;
		PlotNewPage(ipage);
	} else {
		ERRprintf("ERROR: %s is an unrecognized PageSplit command\n", token);
		return(NOMORE);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------
-- NEWPAGE - Forces next graph to appear on a new page (SubPage only)
-- ERASE - ERASE THE SCREEN OR DOES A FRAME TO DEVICE ACTIVE 
----------------------------------------------------------------- */
static int do_newpage(void) {
	PlotNewPage(-1);
	return(OKAY);
}

static int do_erase(void) {
	if (INITIALIZED) {
		PlotNewPage(0);
		PlotFlush();
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     Set the current pen
---------------------------------------------------------------------- */
static int do_color(void) {

	char token[SHORT_STR_SIZE];

	if (! LexGetMathP(token, sizeof(token), "New pen color: ")) return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);

	SelectPen = CurrentPen = PlotMatchColor(token, SelectPen);
	PlotSelectPen(abs(CurrentPen));

	return(OKAY);
}

/* ----------------------------------------------------------------------
--     LINEWIDTH - Change the linewidth on plotters so equiped
---------------------------------------------------------------------- */
static int do_lw(void) {
	REAL x;
	x = LexGetReal(1.0f, "Line width (1=.007\"): ");
	if (! LexEscape(TRUE)) {
		x *= 7;														/* Go to okay units */
		if (x < 1) x = 1;
		PlotSetLineWidth((int) x);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     CHRSET - SELECT CHARACTER SET IF HERSHEY ENABLED
---------------------------------------------------------------------- */
static int do_font(void) {

	typedef struct _FAMILY {
		char *name;
		int minlen;
		int family, cp, attrib_mask, attrib;
	} FAMILY;

	char token[SHORT_STR_SIZE], *aptr;
	int family, cp, attrib, font;

/* Elements (FAMILY, CP) which are not set are retained from current font */
	static FAMILY cmlist[] = {
		{"TimesRoman",		5, DSP_FAMILY_TMSRM,		DSP_CP_ASCII,	0, 0},
		{"Times-Roman",	6, DSP_FAMILY_TMSRM,		DSP_CP_ASCII,	0, 0},
		{"TmsRmn",			3, DSP_FAMILY_TMSRM,		DSP_CP_ASCII,	0, 0},
		{"Helvetica",		4, DSP_FAMILY_HELV,		DSP_CP_ASCII,	0, 0},
		{"Hershey",			3, DSP_FAMILY_HERSHEY,	DSP_CP_ASCII,	0, 0},
		{"Script",			3,	DSP_FAMILY_SCRIPT,	DSP_CP_ASCII,	0, 0},
		{"Gothic",			4,	DSP_FAMILY_GOTHIC,	DSP_CP_ASCII,	0, 0},
		{"Roman",			5,	0,							DSP_CP_ASCII,	0xFFFF, 0},
		{"ASCII",			5,	0,							DSP_CP_ASCII,	0xFFFF, 0},
		{"Greek/Math",		5,	0,							DSP_CP_MATH,	0xFFFF, 0},
		{"Math/Greek",		4,	0,							DSP_CP_MATH,	0xFFFF, 0},
		{"Symbols",			3,	0,							DSP_CP_SYMBOL,	0xFFFF, 0},
		{"Normal",			4,	0,							0,					0,      0},
		{"Bold",				3,	0,							0,					0xFFFF, DSP_ATTRIB_BOLD},
		{"Italics",			2,	0,							0,					0xFFFF, DSP_ATTRIB_ITAL},
		{"BoldItalics",	5,	0,							0,					0xFFFF, DSP_ATTRIB_ITAL|DSP_ATTRIB_BOLD},
		{"BoldScript",		5,	DSP_FAMILY_SCRIPT,	DSP_CP_ASCII,	0xFFFF, DSP_ATTRIB_BOLD},
		{"BoldGothic",		5,	DSP_FAMILY_GOTHIC,	DSP_CP_ASCII,	0xFFFF, DSP_ATTRIB_BOLD},
		{"BoldGreek",		5,	0,							DSP_CP_MATH,	0xFFFF, DSP_ATTRIB_BOLD},
		{NULL,				0, 0,                   0,             0,      0}
	};
	FAMILY *entry;

	if (! LexGetTokenP(token, sizeof(token),
		"Character set [TimesRoman, Helvetica, 1,2,3,...] (no change): "))
		return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);

	if (*token == '?' || stricmp(token, "-?") == 0) {
		TTYprintf("Recognized font families and font attributes:\n");
		for (entry=cmlist; entry->name != NULL; entry++) {
			if (entry->minlen > 0) {
				if (entry->family != 0 && entry->cp != 0) {
					aptr = "(Sets family, codepage and possibly attributes)";
				} else if (entry->cp != 0) {
					aptr = "(Sets code page and possibly attributes)";
				} else {
					aptr = "(Sets attributes only)";
				}
				TTYprintf("  %-15s  %s\n", entry->name, aptr);
			}
		}
		return(OKAY);
	} else if ( (entry = LexCmdl(token, cmlist, sizeof(FAMILY))) != NULL) {
		font = PlotSelectFont(-1);
		family = (entry->family != 0) ? entry->family : font & DSP_FAMILY_MASK ;
		cp     = (entry->cp     != 0) ? entry->cp     : font & DSP_CP_MASK ;
		attrib = ((font & DSP_ATTRIB_MASK) & entry->attrib_mask) | entry->attrib;
		font   = family | cp | attrib;
	} else {
		font = strtol(token, &aptr, 10);
		if (*aptr != '\0' || font < 0 || font > 10) {
			ERRprintf("ERROR: I'll allow font names, but not %s\n", token);
			return(NOMORE);
		}
		font |= DSP_FAMILY_MASK;			/* Special code to use old method */
	}

	PlotSelectFont(font);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     HCOPY - SCREEN COPY PROCESSOR
---------------------------------------------------------------------- */
static int do_hcopy(void) {
	return ( HardCopy(0) ? OKAY : NOMORE);
}

/* ----------------------------------------------------------------------
--     SIZE  - CHANGE SIZE OF PLOTTING AREA
---------------------------------------------------------------------- */
static int do_size(void) {
	REAL x,y;
	x = LexGetReal(-1.0f, "XSIZE of plotting area (unchanged): ");
	if (LexEscape(TRUE)) return(OKAY);
	y = LexGetReal(-1.0f, "YSIZE of plotting area (unchanged): ");
	if (! LexEscape(TRUE)) 
		PlotSetSize(x,y);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     OFFSET  - NEW OFFSET TO BE SENT TO PLOTTER
---------------------------------------------------------------------- */
static int do_offset(void) {
	REAL x,y;
	x = LexGetReal(-1.0f, "New X offset (unchanged): ");
	if (LexEscape(TRUE)) return(OKAY);
	y = LexGetReal(-1.0f, "Y offset (unchanged): ");
	if (! LexEscape(TRUE)) 
		PlotSetOrigin(x,y);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     SHRINK  - OVERALL SHRINK PLOT BY GIVEN FACTOR. RELATIVE SIZES SAME
---------------------------------------------------------------------- */
static int do_shrink(void) {
	REAL x;
	x = LexGetReal(1.0f, "Reduce by a factor of _? (1) ");
	if (LexEscape(TRUE)) return(OKAY);
	if (x <= 0) {
		gen_err("Reduction factor of 0?  Are you on drugs?");
		return(NOMORE);
	}		
	PlotSetFactor(1.0f/x);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     MARGIN  - Reset the side margins
---------------------------------------------------------------------- */
static int do_margins(void) {

	char token[OPTION_STR_SIZE];
	int  i;
	REAL xt[4]={-1.0f, -1.0f, -1.0f, -1.0f};

	if (! LexGetTokenP(token,sizeof(token), 
		 "Margin: {Bottom | Left | Top | Right} <val>] (abort): ")) 
	  	return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);

	if ( (i = LexSelect(token, "LEFT BOTTOM RIGHT TOP UPPER")) > 0) {
		i--;
		if (i>3) i=3;							/* Top and upper synonymous */
		xt[i] = LexGetReal(-1.0f, "Margin value (no change): ");
		if (! LexEscape(TRUE) && xt[i] >= 0.0) 
			PlotSetMargin(xt[0], xt[1], xt[2], xt[3]);
	} else {												/* Old compatibility mode */
		LexBackup();
		xt[0] = LexGetReal(-1.0f, "New X margin (no change): ");
		if (LexEscape(TRUE)) return(OKAY);
		xt[1] = LexGetReal(-1.0f, "Y margin (no change): ");
		if (! LexEscape(TRUE)) 
			PlotSetMargin(xt[0],xt[1], xt[0],xt[1]);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     FLIPXY - Flip coordinate system
--     ORIENT - Set to portrait/landscape format
---------------------------------------------------------------------- */
static int do_flipxy(void) {

	char token[SHORT_STR_SIZE];

	if (! LexGetTokenP(token, sizeof(token), "Rotate plot 90 degrees on page (yes|NO): "))
		return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);
	
	if (LexEqual(token, "YES", 1) || LexEqual(token, "PORTRAIT", 1)) {
		PlotSetXYFlip(TRUE);
	} else if (LexEqual(token, "NO", 1) || LexEqual(token, "LANDSCAPE", 1)) {
		PlotSetXYFlip(FALSE);
	} else {
		ERRprintf("ERROR: %s invalid response for FLIPXY - expecting YES or NO\n", token);
		return(NOMORE);
	}
	return(OKAY);
}

static int do_axesfillcolor(void) {
	char token[SHORT_STR_SIZE];
	if (! LexGetMathP(token, sizeof(token), "Fill color inside axes (none): ")) {
		PlotWindow->AxesFillColor = -1;
	} else {
		PlotWindow->AxesFillColor = PlotMatchColor(token, -1);
	}
	return(OKAY);
}

static int do_pagefillcolor(void) {
	char token[SHORT_STR_SIZE];
	if (! LexGetMathP(token, sizeof(token), "Flood fill of entire paper color (none): ")) {
		PlotWindow->PageFillColor = -1;
	} else {
		PlotWindow->PageFillColor = PlotMatchColor(token, -1);
	}
	return(OKAY);
}

static int do_areafillcolor(void) {
	char token[SHORT_STR_SIZE];
	if (! LexGetMathP(token, sizeof(token), "Fill color of full graph area (none): ")) {
		PlotWindow->AreaFillColor = -1;
	} else {
		PlotWindow->AreaFillColor = PlotMatchColor(token, -1);
	}
	return(OKAY);
}


static int do_orient(void) {
	char token[SHORT_STR_SIZE];
	int i;

	if (! LexGetTokenP(token, sizeof(token), "Page orientation [LANDSCAPE | Portrait | Inv_Landscape | Inv_Portrait]: "))
		return(OKAY);
	if (LexEscape(TRUE)) return(OKAY);
	
	i = LexSelect(token, "Landscape Portrait Inv_Landscape Inv_Portrait");
	if (i <= 0) {
		ERRprintf("ERROR: Invalid response (%s)\n", token);
		return(NOMORE);
	}
	PlotSetPageOrientation(i);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     SPEED  -  CHANGE SPEED OF PEN
---------------------------------------------------------------------- */
static int do_speed(void) {
	int i;
	i = LexGetInt(18, "New pen speed in cm/sec (18): ");
	if (! LexEscape(TRUE)) {
		if ( i < 0 || i > 36) gen_warn("Starnge value (cm/sec) requested for pen speed.");
		PlotSetPenSpeed(i, 0);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     AUTOERASE - SETS AUTOERASE ON OR OFF AS DESIRED
---------------------------------------------------------------------- */
static int do_autoerase(void) {
	LOGICAL ltmp;
	ltmp = LexOnOff(TRUE, "Auto-erase [ON|off]? ");
	if (! LexEscape(TRUE)) 
		DoErase = ltmp;
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     VECSIZE - SET THE REPEAT DISTANCE FOR DASHED-LINES
---------------------------------------------------------------------- */
static int do_vecsize(void) {
	REAL x;
	x = LexGetReal(0.003f, "Repeat spacing on dashed lines (0.003\"): ");
	if (! LexEscape(TRUE)) {
		if (x <= 0) {
			gen_err("Illegal VECSIZE value");
			return(NOMORE);
		}
		PlotSetLineType(1, x);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     ENCURSOR - COMMAND TO ENABLE USE OF CURSOR TO ENTER DATA WHEN
--                USING THE TEKTRONIX SCREEN (AS IN INTEGRAL OR BLOWUP)
---------------------------------------------------------------------- */
static int do_encursor(void) {
	LOGICAL ltmp;
	ltmp = LexYesNo(TRUE, "Enable the cursor (YES|no): ");
	if (! LexEscape(TRUE)) 
		CursorEnabled = ltmp;
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     Autoscaling - internal type routine to calc the autoscale
--                   parameters for use by macros.
---------------------------------------------------------------------- */
static int do_calc_autoscales(int mode) {
	static REAL rmin, rmax, dx, dx2;
	static INTEGER mx;

	rmin = LexGetReal(0.0f, "Lower band limit (0): ");
	rmax = LexGetReal(1.0f, "Upper band limit (1): ");
	if (mode == 0) PlotAutoScale   (rmin, rmax, &rmin, &rmax, &dx, &dx2, &mx);
	if (mode == 1) PlotAutoLogScale(rmin, rmax, &rmin, &rmax, &dx, &dx2, &mx);
	GVLinkReal("RMIN$", GVF_INTERNAL, &rmin);
	GVLinkReal("RMAX$", GVF_INTERNAL, &rmax);
	GVLinkReal("DX$",   GVF_INTERNAL, &dx);
	GVLinkReal("DX2$",  GVF_INTERNAL, &dx2);
	GVLinkInt ("MX$",   GVF_INTERNAL, &mx);
	return(OKAY);
}
static int do_calc_autoscale(void) {
	return do_calc_autoscales(0);
}
static int do_calc_autologscale(void) {
	return do_calc_autoscales(1);
}

/* ----------------------------------------------------------------------
--    TITLE - Title the plot
---------------------------------------------------------------------- */
static int do_title(void) {

	REAL ts, x, y;
	LOGICAL ltmp;
	char token[LONG_STR_SIZE];
	static char title[LONG_STR_SIZE]="Guess What (?) Plot";

	ltmp = LexGetStrExpr(token, sizeof(token));
	if (LexEscape(TRUE)) return(OKAY);
	if (! ltmp) {
		LexPromptStr(token, sizeof(token), "Plot Title (/ for previous): ");
		if (LexEscape(TRUE)) return(OKAY);
	}
	if (strnblen(token) == 0) return(OKAY);
	if (strcmp(token,"/") != 0) strcpy(title, token);
	ts = PL_Axis.TitleSize;
	x = PlotWindow->xsize/2 - PlotQueryStringLength(ts,title,0)/2;	/* Center it */
	y = PlotWindow->ysize - PlotWindow->ymarg[1] + 1.5f*ts;			/* And slightly up */
	return(text_draw(x, y, ts, title));
}

/* -------------------------------------------------------------------------
--     TIMESTAMP:      Dec 03 1988  22:45:88
------------------------------------------------------------------------- */
static int do_tstamp(void) {

	char token[DFLT_STR_SIZE];
	time_t ltime;
	struct tm *tstruct;

	time(&ltime);
	tstruct = localtime(&ltime);
	strftime(token, sizeof(token), PlotWindow->TimeStamp.Encode, tstruct);
	return(text_draw(PlotWindow->TimeStamp.X, PlotWindow->TimeStamp.Y, PlotWindow->TimeStamp.Size, token));
}

/* ---------------------------------------------------------------------------
-- Common routine needed by TITLE and TIMESTAMP
--------------------------------------------------------------------------- */
static int text_draw(REAL x, REAL y, REAL ts, char *token) {
	REAL save[4];												/* For CLIP saving */
	INTEGER i,savek;

	if (! INITIALIZED) {
		ERRprintf("ERROR: Plotter not enabled - unable to draw text\n");
		return(NOMORE);
	}

	PlotSelectPen( (CurrentPen > 0) ? CurrentPen : 1);
	savek = -1;	PlotSetClip(&savek, save);			/* Query current clip	*/
	i = 1;		PlotSetClip(&i, NULL);				/* Set to maximum			*/

	PlotInchString(x, y, ts, token, 0.0f, 0);		/* Draw string				*/
	PlotFlush();											/* Flush						*/
	PlotSetClip(&savek, save);							/* Restore clip			*/
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     Graphics print - Attempt to make a hard copy of graphics screen
---------------------------------------------------------------------- */
static int do_grprint(void) {
	return( PlotRequestHardCopy() ? OKAY : NOMORE);
}

/* ----------------------------------------------------------------------
--     VISIBL - Set the drawing visibility
---------------------------------------------------------------------- */
static int do_visible(void) {
	int i;
	i = LexGetInt(1, "Line visibility (1=on, 0=erase, -1=xor): ");
	if (! LexEscape(TRUE)) 
		PlotSetVisibility(i);
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     PAGE - Selects a specific page if such option available
---------------------------------------------------------------------- */
static int do_grpage(void) {
	int i;
	DspPagfnc dsp;

	i = LexGetInt(1, "Graphics plotting page (1): ");
	if (! LexEscape(TRUE)) {
		dsp.UsePage  = i;
		dsp.ShowPage = 0;
		(*DEVICE->dsptch)(PAGFNC, DEVICE->DriverBlock, (DSP *) &dsp);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     FLUSH - Call graphics flush routine
---------------------------------------------------------------------- */
static int do_grflush(void) {

	PlotFlush();
	return(OKAY);
}

/* ----------------------------------------------------------------------
--     SGRAPH - Modify graphics parameters
---------------------------------------------------------------------- */
static int do_sgraph(void) {

	char		token[VARNAME_STR_SIZE];
	LOGICAL	ltmp;
	REAL		x=0;

	ltmp = LexGetTokenP(token, sizeof(token), "Internal variable to change: (list) ");
	if (LexEscape(TRUE)) return(OKAY);

	if (ltmp) {
		if ( (stricmp(token, "list") != 0) && *token != '/' && *token != '?') {
			x = LexGetReal(FLMAGIC, "Value to use (abort): ");
			if (LexEscape(TRUE) || x == FLMAGIC) return(OKAY);
		} else {
			*token = '\0';
		}
	} else {
		*token = '\0';					
	}

	if (! PlotSgraph(token, x)) {
		gen_err2("Unable to set SGRAPH value", token);
		return(NOMORE);
	}
	return(OKAY);
}

/* ----------------------------------------------------------------------
-- SUB_PLOT operation (possible key -CANCEL)
--
-- Subroutine to redefine the plot area based on a cursor input.  Allows the
-- first values to be regained through the use of a second key input
--
-- Usage:  LOG = SUB$PLOT(key)
--
-- Input:  key = 1 ==> Put up a box on screen and reset all coordinates so plot
--                     falls within this box
--         key = 2 ==> Restore saved values to return to original plot size
--
-- Output: Effectively does a SHRINK, OFFSET and SIZE command to force the plot
--         to fit within the requested boundaries.
---------------------------------------------------------------------- */
static char SubPlotHelp[] = 
"\n"
" Creates a inset plot within an existing plot.  Resets the page size\n"
" and offset to match a defined area.\n"
"\n"
"   Sub_Plot [-options]\n"
"\n"
" Options:\n"
"   -enter <x,y> <x,y>    Specifies corner of the subplot rectangle\n"
"   -cancel               Clears the sub-plot and returns to normal\n"
"   -silent | -quiet      Don't report the corners to the screen\n"
"   -erase                Erase the sub_plot rectangle\n"
"   -fill <color>         Fill sub_plot rectangle with specified color\n"
"\n"
" With no options, a box-cursor will be displayed to define the rectangle.\n"
" Values are reported for copying later to a macro.  The sub-plot remains\n"
" in effect until explicitely cleared.  This turns off autoerase.\n";

static int do_subplot(void) {

	int key,rcode,color;
	char token[SHORT_STR_SIZE];
	BOOL erase_now=FALSE;

	BOOL logtmp, silent;
	REAL dx,dy,t1,x_1,x_2,y_1,y_2;
	int tok1;

	static REAL xshold,yshold,fchold,xohold,yohold;
	static REAL saved=FALSE;

/* First, look for a help request */
	if (LexCheckHelp("SubPlot", SubPlotHelp, NULL)) return(OKAY);

/* Check for options on the command line */
	key = 1;
	silent = FALSE;
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-cancel", 2)) {
			key = 2;
		} else if (LexEqual(token, "-silent", 2) || 	LexEqual(token, "-quiet", 2)) {
			silent = TRUE;
		} else if (LexEqual(token, "-enter", 2)) {
			key = 0;
			x_1 = LexGetReal(0.0, "(x,y) (x,y) Coordinates of box corners (inches): ");
			if (LexEscape(TRUE)) return(NOMORE);
			y_1 = LexGetReal(0.0, "y of first corner: ");
			x_2 = LexGetReal(1.0, "x of second corner: ");
			y_2 = LexGetReal(1.0, "y or second corner: ");
			if (LexEscape(TRUE)) return(NOMORE);
		} else if (LexEqual(token, "-erase", 2)) {
			erase_now = TRUE;
			color = 0;
		} else if (LexEqual(token, "-fill", 2)) {
			erase_now = TRUE;
			if (! LexGetMathP(token, sizeof(token), "Color to fill background of area (0): ")) return(OKAY);
			color = PlotMatchColor(token, 0);
		} else {
			ERRprintf("ERROR: %s is not a valid SUB_PLOT option\n", token);
			return(NOMORE);
		}
	}

	if (saved) {									/* Restore old values */
		PlotSetSize(xshold, yshold);
		PlotSetFactor(fchold);
		PlotSetOrigin(xohold, yohold);
	}

	if (key == 1) {
		logtmp = PlotSetUserMode(FALSE);
		rcode = PlotBoxCursor(&x_1,&y_1,&x_2,&y_2,&tok1);
		if (tok1 == 0x1B) return(NOMORE);	/* Escape command */
		PlotSetUserMode(logtmp);
		if (! rcode) return(NOMORE);
		if (! silent) TTYprintf("SUB_PLOT coordinates: %7.3f %7.3f   %7.3f %7.3f\n",
					 x_1, y_1, x_2, y_2);

	} else if (key == 2) {		/* ... Here to restore the sub-plot values */
		DoErase = TRUE;
		saved  = FALSE;
		return(OKAY);
	}

	x_1 = (x_1+PlotWindow->xorg)*PlotWindow->factr;	/* Go to true inches */
	x_2 = (x_2+PlotWindow->xorg)*PlotWindow->factr;	/* Go to true inches */
	y_1 = (y_1+PlotWindow->yorg)*PlotWindow->factr;	/* Go to true inches */
	y_2 = (y_2+PlotWindow->yorg)*PlotWindow->factr;	/* Go to true inches */

	xshold = PlotWindow->xsize;							/* Keep old values */
	yshold = PlotWindow->ysize;
	fchold = PlotWindow->factr;
	xohold = PlotWindow->xorg;
	yohold = PlotWindow->yorg;

	dx  = (REAL) fabs(x_2-x_1);							/* X Distance */
	dy  = (REAL) fabs(y_2-y_1);							/* Y Distance */
	t1  = (REAL) sqrt(dx/PlotWindow->xsize*dy/PlotWindow->ysize);                  /* New shrink factor */
	PlotSetFactor(t1);										/* Geometric mean */
	PlotSetSize(dx/t1,dy/t1);								/* New size */
	PlotSetOrigin(min(x_1,x_2)/t1,min(y_1,y_2)/t1);	/* Origin now! */

	saved  = TRUE;
	DoErase = FALSE;											/* Turn off erase   */

	if (erase_now) {											/* Now, do we erase */
		logtmp = PlotSetUserMode(FALSE);
		PlotFillRect(0,0, PlotWindow->xsize, PlotWindow->ysize, color);
		PlotSetUserMode(logtmp);
		PlotFlush();
	}

	return(OKAY);
}

/* ----------------------------------------------------------------------
-- FILL - Cause fill to occur about current point
---------------------------------------------------------------------- */
static int do_fill(void) {
	REAL x, y;
	int  rcode;

	x = LexGetReal(-1.0f, "Enter coordinates for fill (cur): ");
	if (LexEscape(TRUE)) return(OKAY);
	if (x != -1.0f) y = LexGetReal(0.0f, "Enter Y coordinate: ");
	if (LexEscape(TRUE)) return(OKAY);
	do {
		rcode = PlotFill(3, x, y);
	} while (x == -1.0f && rcode == 48);
	return(OKAY);
}

/* ----------------------------------------------------------------------
-- clip symbols - Should symbols within plot area be clipped
---------------------------------------------------------------------- */
static int do_clipsyms(void) {

	BOOL ltmp;
	ltmp = LexYesNo(TRUE, "Clip symbols inside plot area (YES|no)? ");
	if (! LexEscape(TRUE)) PlotSetSymbolClip(ltmp);
	return(OKAY);
}


/* ----------------------------------------------------------------------
-- palette - Handle the palettes
---------------------------------------------------------------------- */
static int do_palette(void) {

	int i, ipen, NumEntries;
	PLOT_PALETTE *palette;
	char token[DFLT_STR_SIZE], path[PATH_MAX], prompt[48];
	FILE *unt;

	if (! LexGetTokenP(token, sizeof(token), "Palette command: ")) return(OKAY);

/* Help format */
	if (LexEqual(token, "?", 1) || LexEqual(token, "help", 1)) {
		TTYprintf(
"   Usage:  PALETTE [ COLORLIST | LOAD <name> | SAVE <name> | DEFINE <num> <num> | RAINBOW <type> | HELP ]\n"
"\n"
"   This function loads a palette from the given name using a default extension\n"
"   of .pal.  The palette file must be found in the current directory or along\n"
"   the search path.\n");
		return(OKAY);
	}

/* List color mode */
	if (LexEqual(token, "COLORLIST", 6)) {
		if (ColorTable == NULL) {
			TTYprintf("  No colortable was loaded.  Check that the appropriate database file exists\n");
			return(OKAY);
		}
		for (i=0; ColorTable[i].name != NULL; i++) {
			if (i%4 == 3) {
				TTYprintf("%s\n", ColorTable[i].name);
			} else {
				TTYprintf("%-18s  ", ColorTable[i].name);
			}
		}
		if (i%4 != 0) TTYputc('\n');
		return(OKAY);

	} else if (LexEqual(token, "RAINBOW", 4)) {
		GVP_CONTINUUMPALETTE rc;
		if (! LexGetTokenP(token, sizeof(token), "Rainbow choice (? => list): ")) return(OKAY);
		if ( (rc = GVSelectContinuumPalette(token, GVContinuumPalette)) == GV_PAL_ERROR) return(NOMORE);
		GVContinuumPalette = rc;
		return(OKAY);

	} else if (LexEqual(token, "DEFINE", 3)) {
		NumEntries = LexGetInt(-1, "Number of palette entries (abort): ");
		if (LexEscape(TRUE) || NumEntries <= 0) return(OKAY);
		if (NumEntries < 2) {
			ERRprintf("ERROR: Palette must define at least two pens");
			return(NOMORE);
		}

		palette = malloc(sizeof(PLOT_PALETTE) + NumEntries*sizeof(*palette->rgb));
		palette->num_entries = NumEntries;

		palette->num_pens = LexGetInt(-1, "Number of pen values (NumEntries-1): ");
		if (LexEscape(TRUE)) {free(palette); return(OKAY);}
		if (palette->num_pens < 0 || palette->num_pens > palette->num_entries-1) 
			palette->num_pens = palette->num_entries-1;

		for (i=0; i<palette->num_entries; i++) {
			sprintf(prompt, "Color for index %d: (black): ", i);
			if (LexGetMathP(token, sizeof(token), prompt)) {
				palette->rgb[i] = PlotMatchColor(token, MY_RGB(0,0,0));
				if (IS_PEN(palette->rgb[i])) {
					if (palette->rgb[i] >= i) {
						ERRprintf("ERROR: Invalid color specified - set to black (%s)\n", token);
						palette->rgb[i] = MY_RGB(0,0,0);
					} else {
						palette->rgb[i] = palette->rgb[(int) palette->rgb[i]];
					}
				}
			} else if (LexEscape(TRUE)) {
				free(palette);
				return(OKAY);
			} else {
				palette->rgb[i] = MY_RGB(0,0,0);
			}
		}

/* Put it into the PlotWindow structures */
		PlotSetPalette(palette);					/* Send the palette now */
		free(palette);
		PlotWindow->colour = -1;					/* Make sure it takes		*/
		PlotSelectPen(abs(CurrentPen));			/* And set current color	*/
		return(OKAY);
		
	} else if (LexEqual(token, "map", 3)) {
		while ( (ipen = LexGetInt(-1, "Pen # to set (quit): ")) >= 0) {
			if (ipen >= PlotWindow->palette->num_entries) {
				ERRprintf("Pen %i is beyond the end of the colormap table\n", ipen);
				return(NOMORE);
			}
			if (LexGetMathP(token, sizeof(token), "Color to set (no change): ")) {
				i = PlotMatchColor(token, 1);
				if (IS_PEN(i)) {							/* Very strange operation */
					if (i < 0 || i >= PlotWindow->palette->num_entries) i = PlotWindow->palette->num_entries-1;
					i = PlotWindow->palette->rgb[i];
				}
				PlotWindow->palette->rgb[ipen] = i;
			}
		}
		
		PlotSetPalette(PlotWindow->palette);	/* Resend the current palette */
		PlotWindow->colour = -1;					/* Make sure it takes		*/
		PlotSelectPen(abs(CurrentPen));			/* And set current color	*/
		return(OKAY);
		
	} else if (LexEqual(token, "save", 4)) {
		if (PlotWindow->palette == NULL) {
			ERRprintf("ERROR: No palette exists at the moment (huh?)\n");
			return(NOMORE);
		}
		if (! LexGetFileP(path, sizeof(path), "Save palette definition as filename (.pal): ")) return(OKAY);
		SysAddExt(path, ".pal");
		if ( (unt = fopen(path, "w")) == NULL) {
			ERRprintf("ERROR: Unable to open file %s\n", token);
			return(NOMORE);
		}
		fprintf(unt, "PAGEFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->PageFillColor));
		fprintf(unt, "AREAFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->AreaFillColor));
		fprintf(unt, "AXESFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->AxesFillColor));
		fprintf(unt, "PALETTE DEFINE %d %d\n", PlotWindow->palette->num_entries, PlotWindow->palette->num_pens);
		for (i=0; i<PlotWindow->palette->num_entries; i++) {
			fprintf(unt, "  %s\n", ColorNameFromValue(PlotWindow->palette->rgb[i]));
		}
		fclose(unt);
		return(OKAY);
		
	} else if (LexEqual(token, "show", 4) || LexEqual(token, "list", 4)) {
		if (PlotWindow->palette == NULL) {
			TTYprintf("INFO: No palette exists at the moment (huh?)\n");
		} else {
			TTYprintf("PAGEFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->PageFillColor));
			TTYprintf("AREAFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->AreaFillColor));
			TTYprintf("AXESFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->AxesFillColor));
			TTYprintf("AXESFILLCOLOR %s\n", ColorNameFromValue(PlotWindow->AxesFillColor));
			TTYprintf("PALETTE DEFINE %d %d\n", PlotWindow->palette->num_entries, PlotWindow->palette->num_pens);
			for (i=0; i<PlotWindow->palette->num_entries; i++) {
				TTYprintf("  %s\n", ColorNameFromValue(PlotWindow->palette->rgb[i]));
			}
		}
		return(OKAY);
		
	} else if (LexEqual(token, "load", 4) || LexEqual(token, "read", 4)) {
		
		if (! LexGetFileP(token, sizeof(token), "Palette definition file (.pal): ")) return(OKAY);
		if (! SysResolveDyntName(path, token, sizeof(path))) {
			SysAddExt(token, ".pal");
			if (! SysResolveDyntName(path, token, sizeof(path))) {
				ERRprintf("Unable to locate file %s\n", token);
				return(NOMORE);
			}
		}
		
/* And just execute as a macro */
		LexExecFile(path);						/* Execute the file */
		LexSetLocalNoEcho(TRUE);				/* And disable noise	*/
		return(OKAY);
	}

	TTYprintf("ERROR: Command %s not recognized\n", token);
	return(NOMORE);
}


/* ----------------------------------------------------------------------
-- colormap - Modify the plotwindow colormap table
---------------------------------------------------------------------- */
static int do_colormap(void) {
	LexInsText("palette map");
	return(TRUE);
}

/* ----------------------------------------------------------------------
-- symbolmap - Modify the symbol mapping table
---------------------------------------------------------------------- */
static int do_symbolmap(void) {

	int i, j;
	char token[SHORT_STR_SIZE], *aptr;
	SYMBOLLOOKUP *Table;

	while ( (i = LexGetInt(-1, "Symbol # to set (quit): ")) >= 0) {
		if (i >= NUMBER_OF_SYMBOLS) {
			ERRprintf("WAKEUP! Only %d symbols exist (0-%d), no %d\n", NUMBER_OF_SYMBOLS, NUMBER_OF_SYMBOLS-1, i);
			return(NOMORE);
		}
		if (LexGetTokenP(token, sizeof(token), "Symbol to use (no change): ")) {
			for (Table=SymbolLookup; Table->name!=NULL; Table++) {
				if (LexEqual(token, Table->name, -5)) break;
			}
			if (Table->name != NULL) {					/* Try as a number */
				SymbolMap[i] = Table->symbol;
			} else {
				j = strtol(token, &aptr, 10);
				if (*aptr == '\0' && j >= 0 && j < NUMBER_OF_SYMBOLS) {
					SymbolMap[i] = j;
				} else {
					ERRprintf("WARNING: Symbol %s not recognized as name or #. Symbol %d unchanged.\n", token, i);
					return(NOMORE);
				}
			}
		}
		SymbolScale[i] = LexGetReal(1.0, "Relative scaling of symbol (1.0): ");
	}

	return(OKAY);
}


/* =============================================================================
--     PlotMatchColor - Return index in current colormap for specified color.
--                      Color may be specified directly as an integer, or as
--                      color from known set.
--
--     Usage: INTEGER = PlotMatchColor(char *token, INTEGER dflt);
--
--     Inputs: token - string request
--             dflt  - default color index to return
--
--     Output: PlotMatchColor - index into current color map
============================================================================ */
INTEGER PlotMatchColor(char *token, INTEGER dflt) {
	
	TMPREAL dval;
	int i,ierr;
	int index;
	char *aptr, my_token[SHORT_STR_SIZE];

	if (strcmp(token, "/") == 0) return(dflt);

/* Go through a few of the special cases */
	index = -1;
	if (LexEqual(token, "background", 4)) {				/* Pen 0 specification	*/
		index = 0;
	} else if (LexEqual(token, "foreground", 4)) {		/* Primary color			*/
		index = 1;
	} else if (LexEqual(token, "none", 4)) {				/* No pen selection		*/
		index = CLR_NOMARK;
	} else if (LexEqual(token, "transparent", 4)) {		/* Erase to transparent	*/
		index = CLR_TRANSPARENT;
	}
	if (index >= 0) return(index);
	
/* Look in the user database first - require exact match */
	strscpy(my_token, token, sizeof(my_token));	/* So can modify */
	strlwr(my_token);
	if (ColorTable != NULL) {
		if ( (aptr = strstr(my_token, "grey")) != NULL) aptr[2] = 'a';
		for (i=0; ColorTable[i].name != NULL; i++) {
			if (stricmp(ColorTable[i].name, my_token) == 0) return(ColorTable[i].color);
		}
	}

/* Finally, try to interpret as a simple integer value */
	dval = GVEvalExpr(token, &ierr);
	if (ierr != 0 || dval < INT_MIN || dval > INT_MAX) {
		ERRprintf("ERROR: Illegal pen number expression (%s)\n", token);
		return(dflt);
	}

	return( (int) ((dval>0) ? dval+0.5 : dval-0.5) );
}

static char *ColorNameFromValue(int color) {
	static char token[32];						/* Limit names to 32 chars */
	int i;

	if (color == -1) {
		return("NONE");
	} else if (IS_PEN(color)) {
		sprintf(token, "%d", color);
		return(token);
	} else if (IS_TRANSPARENT(color)) {
		return("TRANSPARENT");
	} else if (IS_NOMARK(color)) {
		return("NONE");
	}

	for (i=0; ColorTable[i].name != NULL; i++) {
		if (ColorTable[i].color == color) return(ColorTable[i].name);
	}
	sprintf(token, "RGB(%d,%d,%d)", R_FROM_RGB(color), G_FROM_RGB(color), B_FROM_RGB(color));
	return(token);
}


/* =============================================================================
--     PlotMatchSymbol - Return index in current symbolmap for specified symbol.
--                      Symbol may be specified directly as an integer, or as
--                      symbol from known set.
--
--     Usage: INTEGER = PlotMatchSymbol(char *token, INTEGER dflt);
--
--     Inputs: token - string request
--             dflt  - default symbol index to return
--
--     Output: PlotMatchSymbol - index into current color map
============================================================================ */
INTEGER PlotMatchSymbol(char *token, INTEGER dflt) {
	
	TMPREAL rval;
	int i,ierr;

	SYMBOLLOOKUP *Table;

	if (strcmp(token, "/") == 0) return(dflt);
	for (Table=SymbolLookup; Table->name!=NULL; Table++) {
		if (LexEqual(token, Table->name, -5)) {
			for (i=1; i<NUMBER_OF_SYMBOLS; i++) {
				if (SymbolMap[i] == Table->symbol) return(i);
			}
			ERRprintf("WARNING: %s in not in the current symbol map\n", token);
			return(dflt);
		}
	}

	rval = GVEvalExpr(token, &ierr);
	if (ierr != 0 || rval < INT_MIN || rval > INT_MAX) {
		ERRprintf("ERROR: Illegal symbol number expression (%s)\n", token);
		return(dflt);
	}
	return( (int) ((rval>0) ? rval+0.5 : rval-0.5) );
}


/* ============================================================================
-- ... Link cursor functions into LEXP for ease of use
============================================================================ */
static void lex_boxcursor(char *str, int len) {

	REAL t,xx=0,yy=0,xxx=1,yyy=1;

	if (INITIALIZED && HASCURSOR && CursorEnabled) PlotBoxCursor(&xx,&yy, &xxx,&yyy, NULL);

	if (xx > xxx) t = xx, xx = xxx, xxx = t;
	if (yy > yyy) t = yy, yy = yyy, yyy = t;
	sprintf(str, "%g,%g,%g,%g", xx,yy,xxx,yyy);

	return;
}

static void lex_crfncs(int type, char *str, int len) {

	REAL x=0,y=0;

	if (INITIALIZED && HASCURSOR && CursorEnabled) PlotCursor(&x, &y, NULL);

	if (type == 2) {
		sprintf(str, "%g,%g", x,y);
	} else {
		sprintf(str, "%g", (type == 0) ? x : y);
	}
	return;
}

static void lex_xcursor(char *str, int len) {
	lex_crfncs(0, str, len);
	return;
}
static void lex_ycursor(char *str, int len) {
	lex_crfncs(1, str, len);
	return;
}
static void lex_xycursor(char *str, int len) {
	lex_crfncs(2, str, len);
	return;
}
