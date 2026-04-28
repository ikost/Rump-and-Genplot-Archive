/* subroutines from main genplot */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"

#include "helper.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	int  notmask;
	int   ormask;
} CMTYPE;

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


/* ===========================================================================
-- Routine to set box coordinates into linked variables (used from fit etc.)
=========================================================================== */
void gpt_SetBoxCursorCoords(REAL xl, REAL yl, REAL xr, REAL yr) {

	static REAL xbox[2], ybox[2];

	xbox[0] = xl; xbox[1] = xr;
	ybox[0] = yl; ybox[1] = yr;

	GVLinkArray("xbox$", GVF_GLOBAL, xbox, 2, NULL);			/* Link variables */
	GVLinkArray("ybox$", GVF_GLOBAL, ybox, 2, NULL);

	return;
}

/* ============================================================================
-- ... Handle the cursor functions
============================================================================ */
static char CursorHelp[]=
"The cursor command allows values on the screen to be measured.  There are two\n" 
"types of cursor, the convention cross and a box.  The cursor may be made to\n"
"freely range over the graph, or to follow a specific data set.  The final\n"
"cursor position is reported back as variables.\n"
"\n"
"Usage: CURSor [<curve>] [-options]\n"
"\n"
"If an optional curve is specified after the cursor command, those data will\n"
"be used for any tracking functions.\n"
"\n"
"Options:\n"
"   -help | -?        Prints this help message\n"
"\n"
"   -normal           Default cross hair free on the screen (default)\n"
"   -tracking         Follows data points of specified curve or main curve\n"
"      -point         Specifies point index for initial cursor position\n"
"   -box              Switches to a box cursor (area defining)\n"
"   -single           Returns immediately on any key/button press\n"
"   -silent           Prints no messages, but returns variables\n"
"\n"
"Output: The following values will be set by the cursor command.\n"
"   XCUR, YCUR  - position of the final X and Y coordinates of the cursor\n"
"   CCUR        - character pressed to terminate the cursor command\n"
"   ICUR        - point number of the last data point in -track mode\n"
"   CURX, CURY  - linked to XCUR,YCUR for the dyslexic among us\n"
"   XBOX$,YBOX$ - arrays linked/defined for the box cursor\n"
"\n"
"Notes:\n"
"  (1) The cursor normally reports its position each time the left mouse button\n"
"      (LMB) is pressed and remains active.  It returns on RMB or any key.\n"
"  (2) The -single option causes the cursor to return on any button or key\n"
"  (3) If possible, cursor positions are displayed on the graphic device.\n"
"  (4) For the box cursor, the LMB will toggle the corner moved by the mouse\n"
"  (5) The tracking cursor will normally follow data points in order. Holding\n"
"      the LMB will free the cursor and jump to the nearest point when released.\n"
"  (6) The cursor obeys the PLX and PLY settings.  All values are reported to\n"
"      screen, but variables are set only to the active corrdinates.\n"
"\n"
" Examples: cursor                     /* Basic cursor command\n"
"           cursor -track -point 17    /* Tracking cursor starting at 17th point\n"
"           echo Identify the crossing\n"
"           cursor -single -silent     /* To request a single point for analysis\n"
"           setv xcross = xcur\n"
;

int gpt_do_cursor(void) {

	static INT LastPoint=1;

	int ichr;
	char token[DFLT_STR_SIZE];
	enum {NORMAL, TRACKING, BOX} type = NORMAL;			/* Type of cursor */
	BOOL SingleMode = FALSE;									/* Single request mode */
	BOOL VerboseMode = TRUE;
	REAL x,y, xb,xt, yl,yr, xc,yc, xb1,xb2, xt1,xt2, yl1,yl2, yr1,yr2 ;

	static REAL xcur, ycur;								/* For linkage */
	static INT  ccur, icur;

/* Check if option is for help */
	if (LexCheckHelp("Cursor", CursorHelp, NULL)) return(OKAY);

	if (! PlotSystem(3, NULL, NULL)) {
		ERRprintf("ERROR: Look again fellow! Current device does not support a cursor\n");
      return(NOMORE);
	}

	GVLinkReal("XCUR", GVF_GLOBAL, &xcur);						/* Link variables */
	GVLinkReal("YCUR", GVF_GLOBAL, &ycur);						/* Possible again */
	GVLinkInt ("CCUR", GVF_GLOBAL, &ccur);
	GVLinkInt ("ICUR", GVF_GLOBAL, &icur);
	GVLinkReal("CURX", GVF_HIDDEN | GVF_GLOBAL, &xcur);
	GVLinkReal("CURY", GVF_HIDDEN | GVF_GLOBAL, &ycur);

	GptSetRange();													/* Set the range */

/* Scan for options (type and control) */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-normal", 2)) {
			type = NORMAL;
		} else if (LexEqual(token, "-tracking", 2)) {
			type = TRACKING;
		} else if (LexEqual(token, "-box", 2)) {
			type = BOX;
		} else if (LexEqual(token, "-point", 2)) {
			LastPoint = LexGetInt(LastPoint, "Starting point (same as last): ");
		} else if (LexEqual(token, "-single", 2)) {
			SingleMode = TRUE;
		} else if (LexEqual(token, "-silent", 4) || LexEqual(token, "-quiet", 2)) {
			VerboseMode = FALSE;
		} else {
			ERRprintf("ERROR: %s is an invalid cursor option\n", token);
			return(NOMORE);
		}
	}

	switch (type) {

		case NORMAL:
			if (VerboseMode) {
				TTYputs(SingleMode ? "Cursor: All keys return\n" : "Cursor: <0> or space recycles\n");
				TTYflush();
			}
			do {
				PlotCursor(&x, &y, &ichr);
				x = (x-Gpt->xmin) / (Gpt->xmax-Gpt->xmin);	/* To range [0,1] */
				y = (y-Gpt->ymin) / (Gpt->ymax-Gpt->ymin);
				xcur = xb = xminb + (xmaxb-xminb)*x;		/* Bottom/left */
				ycur = yl = yminl + (ymaxl-yminl)*y;
				xt = (Gpt->xtop   == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("bottom_to_top(xcur)", NULL)) : xmint + (xmaxt-xmint)*x;
				yr = (Gpt->yright == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("left_to_right(ycur)", NULL)) : yminr + (ymaxr-yminr)*y;
				sprintf(token, "Cursor: XB = %-14.7g  YL = %-14.7g", xb, yl);
				PlotText(1,1,token);
				PlotFlush();
				if (VerboseMode) {
					TTYprintf("%s\n", token);
					if (Gpt->BoxMode && ((Gpt->xtop != IS_OFF) || (Gpt->yright != IS_OFF))) 
						TTYprintf("        XT = %-14.7g  YR = %-14.7g\n", xt,yr);
					TTYflush();
				}
			} while (! SingleMode && ichr == '0');

			if (Gpt->BoxMode && Gpt->plx != 0) xcur = xt;		/* Change def? */
			if (Gpt->BoxMode && Gpt->ply != 0) ycur = yr;
			ccur = ichr;
			break;

		case TRACKING:
			do {
				if (GptCurve->npt <= 0) {
					ERRprintf("ERROR: No data to track.  Think again ...\n");
					return(NOMORE);
				} else if (! PlotTrackingCursor(GptCurve->x, GptCurve->y, GptCurve->npt, &LastPoint, &ichr)) {
					ERRprintf("ERROR: Tracking CURSOR not available on this device\n");
					return(NOMORE);
				}
				if (VerboseMode) {
					TTYprintf("I:%4i  X: %-14.7g  Y: %-14.7g\n",
								 LastPoint, GptCurve->x[LastPoint], GptCurve->y[LastPoint]);
					TTYflush();
				}
				icur = LastPoint;								/* Return point number	*/
			} while (! SingleMode && ichr == '0');
			xcur = GptCurve->x[LastPoint];				/* And set values			*/
			ycur = GptCurve->y[LastPoint];
			ccur = ichr;
			break;

		case BOX:
			if (VerboseMode) {
				TTYputs(SingleMode ? "Box cursor: All keys return\n" : "Box cursor: <0> (sorry only a 0) recycles\n");
				TTYflush();
			}

			do {
				if (! PlotBoxCursor(&x, &y, &xc, &yc, &ichr)) {
					ERRprintf("ERROR: (Huh?) Can't emulate box cursor - sorry\n");
					return(NOMORE);
				}
				x = (x-Gpt->xmin) / (Gpt->xmax-Gpt->xmin);	/* To range [0,1] */
				y = (y-Gpt->ymin) / (Gpt->ymax-Gpt->ymin);

				xcur = xb1 = xminb + (xmaxb-xminb)*x;	/* Bottom/left */
				ycur = yl1 = yminl + (ymaxl-yminl)*y;
				xt1 = (Gpt->xtop   == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("bottom_to_top(xcur)", NULL)) : xmint + (xmaxt-xmint)*x;
				yr1 = (Gpt->yright == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("left_to_right(ycur)", NULL)) : yminr + (ymaxr-yminr)*y;

				x = (xc-Gpt->xmin) / (Gpt->xmax-Gpt->xmin);	/* To range [0,1] */
				y = (yc-Gpt->ymin) / (Gpt->ymax-Gpt->ymin);
				xcur = xb2 = xminb + (xmaxb-xminb)*x;	/* Bottom/left */
				ycur = yl2 = yminl + (ymaxl-yminl)*y;
				xt2 = (Gpt->xtop   == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("bottom_to_top(xcur)", NULL)) : xmint + (xmaxt-xmint)*x;
				yr2 = (Gpt->yright == IS_NONLINEAR) ? GVTrimToReal(GVEvalExpr("left_to_right(ycur)", NULL)) : yminr + (ymaxr-yminr)*y;

				if (VerboseMode) {
					TTYprintf("Box: XB = (%14.7g,%14.7g)  YL = (%14.7g,%14.7g)\n", xb1,xb2, yl1,yl2);
					if (Gpt->BoxMode && ((Gpt->xtop != IS_OFF) || (Gpt->yright != IS_OFF))) 
						TTYprintf("     XT = (%14.7g,%14.7g)  YR = (%14.7g,%14.7g)\n", xt1,xt2, yr1,yr2);
					TTYflush();
				}
			} while (! SingleMode && ichr == '0');

			x  = (Gpt->BoxMode && Gpt->plx != 0) ? xt1 : xb1 ;
			xc = (Gpt->BoxMode && Gpt->plx != 0) ? xt2 : xb2 ;
			y  = (Gpt->BoxMode && Gpt->ply != 0) ? yr1 : yl1 ;
			yc = (Gpt->BoxMode && Gpt->ply != 0) ? yr2 : yl2 ;
			gpt_SetBoxCursorCoords(x,y,xc,yc);

			ccur = ichr;
			break;
	}

	return(OKAY);
}


/* ============================================================================
--     In-place heapsort sort of data into ascending order X.  Carries Y
--     along with it simultaneously unless requested not.
--
--     Usage: CALL MY_SORT(x,y,npt,nptmax,ids)
--
--     Inputs: X,Y - X and Y pairs of current data
--             NPT - Number of valid points
--
--     Output: X,Y - Sorted data
--
--     Note: Options: -REVERSE | -NORMAL : Ascending or descending X values
--		     -STRICT  | -DELETE : Delete duplicate values
============================================================================ */
static char SortHelp[]=
"\n"
" Command to sort data into ascending, descending, or random lists.  The\n"
" options allow for selection of type and order of sort.  If no curve is\n"
" specified, then the data in the main curve is assumed.\n"
"\n"
"   SORT [<curve>|<array>|<string_array>] [-options]\n"
"   SORT -curve   <name> [-options]\n"
"   SORT -array   <name> [-options]\n"
"   SORT -strings <name> [-options]\n"
"\n"
" Notes: Sort is one of a very few commands that can accept either a curve\n"							  
"        or an array as the first argument.  By default, sort operates on the\n"
"        main curve.  Arrays can be of either REAL or STRING type, and only a\n"
"        subset of the options make any sense.\n"
"\n"
" Options:\n"
"   -help | -?        Prints this help message\n"
"\n"
"   -normal           Sorts in ascending order (default)\n"
"   -reverse          Sorts in descending order\n"
"   -random           Randomize instead of sort\n"
"   -nosort           Does not do a sort\n"
"   -strict           Sort strictly (remove duplicate values)\n"
"    -delete          Delete duplicate points keeping Y of first only (default)\n"
"    -average | -avg  Delete duplicate points, but average Y values\n"
"    -sum             Sum Y values on duplications\n"
"   -xonly            Sorts on X and modify X only - Y and Z unchanged\n"
"   -yonly            Sorts on Y and modify Y only - X and Z unchanged\n"
"   -zonly            Sorts on Z and modify Z only - X and Y unchanged\n"
"   -x | -y | -z      Specify sorting based on X, Y or Z values\n"
"   -xy               Sort on X and then Y in a 3D curve (default)\n"
"   -silent | -quiet  Turns off all error messages\n"
"   -nocase           Ignore case on string compares\n"
"\n"
" Examples: sort                /* Data will be in ascending X values\n"
"           sort -y -reverse    /* Data will be in descending Y value order\n"
"           sort c1:y -reverse  /* Reverse order sort the array\n"
"           sort -strict        /* Often used before a spline fit\n"
"           sort -xonly -random /* Randomly redistributes X values\n";

static int SortCurve(char *name);
static int SortArray(char *name);
static int SortStrings(char *name);

int gpt_do_sort(void) {

	char token[VARNAME_STR_SIZE];
	int itype;

/* Check if option is for help */
	if (LexCheckHelp("Sort", SortHelp, NULL)) return(OKAY);

/* Check for an argument specifying an alternate curve or array to use */
	if (LexChkToken(token, sizeof(token))) {
		if (LexEqual(token, "-CURVE", 3)) {
			LexGetToken(token, sizeof(token));				/* Strip the -CURVE */
			if (! LexGetTokenP(token, sizeof(token), "Curve name (MAIN): ")) return(OKAY);
			return SortCurve(token);
		} else if (LexEqual(token, "-ARRAY", 6)) {
			LexGetToken(token, sizeof(token));				/* Strip the -ARRAY */
			if (! LexGetTokenP(token, sizeof(token), "Array name (abort): ")) return(OKAY);
			return SortArray(token);
		} else if (LexEqual(token, "-STRINGS", 7)) {
			LexGetToken(token, sizeof(token));				/* String the -STRINGS */
			if (! LexGetTokenP(token, sizeof(token), "String array name (abort): ")) return(OKAY);
			return SortStrings(token);
		} else if (GVGetInfo(token, &itype, NULL)) {		/* Allow implied curves and arrays */
			if (itype == GV_2DCURVE || itype == GV_3DCURVE) {
				LexGetToken(token, sizeof(token));			/* Really get it now */
				return SortCurve(token);
			} else if (itype == GV_ARRAY || itype == GV_ARRAY_LINK) {
				LexGetToken(token, sizeof(token));			/* Really get it now */
				return SortArray(token);
			} else if (itype == GV_STRING_ARRAY) {
				LexGetToken(token, sizeof(token));			/* Really get it now */
				return SortStrings(token);
			}
		}
	}
	return SortCurve(NULL);
}

static int SortCurve(char *name) {

	REAL xtmp, *x,*x_1,*x_2, *y,*y_1,*y_2, *z,*z_1,*z_2;
	int  i,j,istore, isame, npt;
	unsigned long itmp;
	char token[OPTION_STR_SIZE];
	int SortFlag;										/* What kind of sort to run? */
	BOOL UseZ;
	CURVE *Curve;

	CMTYPE *citem;
	static const CMTYPE cmlist[] =	{	
		{"-normal",		2,  SORT_REVERSE, 0},
		{"-reverse",	3,	 0,				SORT_REVERSE},
		{"-nosort",		4,  SORT_SORT,    0},
		{"-random",		4,	 SORT_SORT,		SORT_RANDOM},
		{"-strict",		3,	 0,				SORT_STRICT},	
		{"-delete",		4,	 SORT_AVERAGE,	SORT_STRICT},
		{"-average",	4,	 0,				SORT_STRICT | SORT_AVERAGE},
		{"-avg",			4,	 0,				SORT_STRICT | SORT_AVERAGE},
		{"-sum",			4,  0,				SORT_STRICT | SORT_SUM},
		{"-xonly",		4,	 SORT_ON_MASK|SORT_FOR_MASK, SORT_ON_X | SORT_DOX},
		{"-yonly",		4,	 SORT_ON_MASK|SORT_FOR_MASK, SORT_ON_Y | SORT_DOY},
		{"-zonly",		4,  SORT_ON_MASK|SORT_FOR_MASK, SORT_ON_Z | SORT_DOZ},
		{"-x",			2,	 SORT_ON_MASK, SORT_ON_X | SORT_DOX},
		{"-y",			2,	 SORT_ON_MASK, SORT_ON_Y | SORT_DOY},
		{"-z",			2,	 SORT_ON_MASK, SORT_ON_Z | SORT_DOZ},
		{"-xy",			3,  SORT_ON_MASK, SORT_ON_XY | SORT_DOX | SORT_DOY},
		{"-silent",		4,  0,				SORT_SILENT},
		{"-quiet",		2,  0,				SORT_SILENT},
		{NULL,			0,	 0, 0} };

/* Sort on a curve, set the proper curve */
	Curve = GptCurve;												/* Default is to use the Genplot curve */
	if (name != NULL) {
		int itype;
		CURVE **aptr=NULL;
		if (! GVGetInfo(name, &itype, (void **) &aptr) || (itype != GV_2DCURVE && itype != GV_3DCURVE)) {
			ERRprintf("ERROR: Curve %s doesn't exist or isn't of expected type (SortCurve)\n", name);
			return(NOMORE);
		}
		Curve = *aptr;
	}

	if (Curve->z == NULL) {
		SortFlag = SORT_ON_X  | SORT_SORT | SORT_DOX | SORT_DOY;					/* Default options */
	} else {
		SortFlag = SORT_ON_XY | SORT_SORT | SORT_DOX | SORT_DOY | SORT_DOZ;	/* Default options */		
	}

/* Parse for other options */
	while (LexGetOption(token, sizeof(token))) {
		if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL) {
			SortFlag &= ~citem->notmask;
			SortFlag |=  citem->ormask;
		} else {
			ERRprintf("ERROR: Unrecognized SORT option (%s)\n", token);
			return(NOMORE);
		}
	}

/* Get the actual pointers to the data from the curve information */	
	npt = Curve->npt;
	x = Curve->x; 
	y = Curve->y; 
	z = Curve->z;
	if (npt <= 1) return(OKAY);								/* Anything we do is okay */

/* Allow for possible sort on other variables */
	if (x == NULL) SortFlag &= ~(SORT_DOX | SORT_ON_X);
	if (y == NULL) SortFlag &= ~(SORT_DOY | SORT_ON_Y);
	if (z == NULL) SortFlag &= ~(SORT_DOZ | SORT_ON_Z);
	if ((SortFlag & SORT_ON_MASK) == 0) {
		ERRprintf("ERROR: Specified sort variable does not exist\n");
		return(NOMORE);
	}
	
/* ---------------------------
... Randomize the data ...
--------------------------- */
	if (SortFlag & SORT_RANDOM) {							/* Randomize order?	*/
		srand((int) time(NULL));							/* Randomize random	*/
		for (i=0; i<npt; i++) {								/* Choose each from n-i possible */
			itmp = rand(); itmp = itmp*(npt-i); j = (INT) (itmp/( ((unsigned long)RAND_MAX)+1)); j = j+i;
			if (j == i) continue;
			if (SortFlag & SORT_DOX) {xtmp = x[j]; x[j] = x[i]; x[i] = xtmp;}
			if (SortFlag & SORT_DOY) {xtmp = y[j]; y[j] = y[i]; y[i] = xtmp;}
			if (SortFlag & SORT_DOZ) {xtmp = z[j]; z[j] = z[i]; z[i] = xtmp;}
		}
		return(OKAY);
	} 

/* ... Sort the data ... */
	if (SortFlag & SORT_SORT) heap_sort(x, y, z, npt, SortFlag);

/* ... Eliminate Duplicates ... */
/* This works for the -XY mode also since Z will not be NULL (UseZ) */
	if (SortFlag & SORT_STRICT) {						/* Find and elimate dups?	*/
		if (SortFlag & SORT_ON_X) {					/* This holds for -X and -XY */
			x_1=x_2=x; y_1=y_2=y; z_1=z_2=z;			/* Last stored value */
		} else if (SortFlag & SORT_ON_Y) {			/* -Y */
			x_1=x_2=y; y_1=y_2=x; z_1=z_2=z;			/* Last stored value */
		} else {												/* -Z */
			x_1=x_2=z; y_1=y_2=x; z_1=z_2=y;			/* Last stored value */
		}

		UseZ = (z_1 != NULL);							/* Include Z in process	*/
		istore = isame  = 1;								/* Single point the same	*/
		for (i=1; i<npt; i++) {							/* Search forward */
			x_2++; y_2++; z_2++;							/* Next point		*/
			if (*x_2==*x_1 && (! UseZ || *y_2==*y_1)) {	/* Equal values?	*/
				if (SortFlag & SORT_AVERAGE) {
					if (! UseZ) {
						*y_1 = (isame*(*y_1)+(*y_2)) / (isame+1);
					} else {
						*z_1 = (isame*(*z_1)+(*z_2)) / (isame+1);
					}
				} else if (SortFlag & SORT_SUM) {
					if (! UseZ) {
						*y_1 += *y_2;
					} else {
						*z_1 += *z_2;
					}
				}
				isame++;
			} else {
				*(++x_1) = *x_2;							/* Store in next slot */
				*(++y_1) = *y_2;
				if (UseZ) *(++z_1) = *z_2;
				isame = 1;
				istore++;
			}
		}
		if (npt != istore && ! (SortFlag & SORT_SILENT)) {
			TTYprintf("SORT: %i points deleted\n", npt-istore);
		}
		Curve->npt = istore;
	}

/* ... Reverse the order if requested */
	if (SortFlag & SORT_REVERSE) {				/* Invert the data? */
		for (i=0,j=npt-1; i<npt/2; i++,j--) {
			if (SortFlag & SORT_DOX) {xtmp = x[i]; x[i] = x[j]; x[j] = xtmp;}
			if (SortFlag & SORT_DOY) {xtmp = y[i]; y[i] = y[j]; y[j] = xtmp;}
			if (SortFlag & SORT_DOZ) {xtmp = y[i]; y[i] = y[j]; y[j] = xtmp;}
		}
	}
	return(OKAY);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
static int SortArray(char *name) {

	REAL xtmp, *x;
	int  i,j, npt;
	unsigned long itmp;
	char token[OPTION_STR_SIZE];
	int SortFlag = SORT_SORT;		/* Default options */
	int itype;
	ARRAY **aptr=NULL;
	ARRAY *array;

	CMTYPE *citem;
	static const CMTYPE cmlist[] =	{	
		{"-normal",		2,  SORT_REVERSE, 0},
		{"-reverse",	3,	 0,				SORT_REVERSE},
		{"-nosort",		4,  SORT_SORT,    0},
		{"-random",		4,	 SORT_SORT,		SORT_RANDOM},
		{"-strict",		4,	 0,				SORT_STRICT},
		{NULL,			0,	 0, 0} };

/* Sort on a curve, set the proper curve */
	if (! GVGetInfo(name, &itype, (void **) &aptr) || (itype != GV_ARRAY && itype != GV_ARRAY_LINK)) {
		ERRprintf("ERROR: Array %s doesn't exist or isn't of expected type (SortArray)\n", name);
		return(NOMORE);
	}
	array = *aptr;

/* Parse for other options */
	while (LexGetOption(token, sizeof(token))) {
		if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL) {
			SortFlag &= ~citem->notmask;
			SortFlag |=  citem->ormask;
		} else {
			ERRprintf("ERROR: Unrecognized SORT option (%s)\n", token);
			return(NOMORE);
		}
	}

/* Get the actual pointers to the data from the curve information */	
	npt = *array->size;
	x   = array->x;
	if (npt <= 1) return(OKAY);								/* Anything we do is okay */

/* ---------------------------
... Randomize the data ...
--------------------------- */
	if (SortFlag & SORT_RANDOM) {							/* Randomize order?	*/
		srand((int) time(NULL));							/* Randomize random	*/
		for (i=0; i<npt; i++) {								/* Choose each from n-i possible */
			itmp = rand(); itmp = itmp*(npt-i); j = (INT) (itmp/( ((unsigned long)RAND_MAX)+1)); j = j+i;
			if (j != i) {xtmp = x[j]; x[j] = x[i]; x[i] = xtmp;}
		}
		return(OKAY);
	} 

/* ... Sort the data ... */
	if (SortFlag & SORT_SORT) heap_sort(x, NULL, NULL, npt, SORT_ON_X | SORT_DOX);

/* ... Reverse the order if requested */
	if (SortFlag & SORT_REVERSE) {				/* Invert the data? */
		for (i=0,j=npt-1; i<npt/2; i++,j--) { xtmp = x[i]; x[i] = x[j]; x[j] = xtmp; }
	}

/* ... If desired, remove duplicates */
	if (SortFlag & SORT_STRICT) {
		for (i=1,j=0; i<npt; i++) {						/* Check against previous entry */
			if (x[i] == x[j]) continue;					/* If same, don't include in final */
			if (++j != i) x[j] = x[i];						/* Otherwise copy over as needed */
		}
		*array->size = npt = j+1;							/* And reset the size */
	}
				  
	return(OKAY);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
static int mycmp(const void *c1, const void *c2) {
	return strcmp(*((char **)c1), *((char **)c2));
}
static int myicmp(const void *c1, const void *c2) {
	return stricmp(*((char **)c1), *((char **)c2));
}


static int SortStrings(char *name) {

	char *xtmp, **x;
	int  i,j, npt;
	unsigned long itmp;
	char token[OPTION_STR_SIZE];
	int SortFlag = SORT_SORT;					/* Default options */
	int itype;
	STRING_ARRAY **aptr=NULL;
	STRING_ARRAY *array;
	
	CMTYPE *citem;
	static const CMTYPE cmlist[] =	{	
		{"-normal",		2,  SORT_REVERSE, 0},
		{"-reverse",	3,	 0,				SORT_REVERSE},
		{"-nosort",		4,  SORT_SORT,    0},
		{"-random",		4,	 SORT_SORT,		SORT_RANDOM},
		{"-nocase",		3,  0,				SORT_NOCASE},
		{"-strict",		4,	 0,				SORT_STRICT},
		{NULL,			0,	 0, 0} };
		
/* Sort on a curve, set the proper curve */
	if (! GVGetInfo(name, &itype, (void **) &aptr) || (itype != GV_STRING_ARRAY && itype != GV_STRING_ARRAY_LINK)) {
		ERRprintf("ERROR: Array %s doesn't exist or isn't of expected type (SortStrings)\n", name);
		return(NOMORE);
	}
	array = *aptr;
	
/* Parse for other options */
	while (LexGetOption(token, sizeof(token))) {
		if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL) {
			SortFlag &= ~citem->notmask;
			SortFlag |=  citem->ormask;
		} else {
			ERRprintf("ERROR: Unrecognized SORT option (%s)\n", token);
			return(NOMORE);
		}
	}

/* Get the actual pointers to the data from the curve information */	
	npt = *array->size;
	x   = array->sval;
	if (npt <= 1) return(OKAY);								/* Anything we do is okay */

/* ---------------------------
... Randomize the data ...
--------------------------- */
	if (SortFlag & SORT_RANDOM) {							/* Randomize order?	*/
		srand((int) time(NULL));							/* Randomize random	*/
		for (i=0; i<npt; i++) {								/* Choose each from n-i possible */
			itmp = rand(); itmp = itmp*(npt-i); j = (INT) (itmp/( ((unsigned long)RAND_MAX)+1)); j = j+i;
			if (j != i) {xtmp = x[j]; x[j] = x[i]; x[i] = xtmp;}	/* Just exchange pointers */
		}
		return(OKAY);
	} 

/* ... Sort the data ... */
	if (SortFlag & SORT_SORT) qsort((void *) x, npt, sizeof(*x), (SortFlag & SORT_NOCASE) ? myicmp : mycmp);

/* ... Reverse the order if requested */
	if (SortFlag & SORT_REVERSE) {				/* Invert the data? */
		for (i=0,j=npt-1; i<npt/2; i++,j--) { xtmp = x[i]; x[i] = x[j]; x[j] = xtmp; }
	}

/* ... If desired, remove duplicates */
	if (SortFlag & SORT_STRICT) {
		for (i=1,j=0; i<npt; i++) {								/* Check against previous entry		*/
			if (strcmp(x[i], x[j]) == 0) {						/* Are same, drop entry					*/
				if (itype == GV_STRING_ARRAY) {					/* Internal, free unneeded string	*/
					free(x[i]);
					x[i] = NULL;
				}
				continue;												/* Link, will just overwrite later	*/
			}
			j++;															/* Different ==> need to store it	*/
			if (j != i) {												/* Don't bother if just same place	*/
				xtmp = x[j]; x[j] = x[i]; x[i] = xtmp;			/* Exchange the pointers				*/
			}
		}
		*array->size = npt = j+1;									/* And reset the size */
	}

	return OKAY;
}

/* --------------------------------------------------- */
/* ... ZOOM in on a section of the plot                */
/* ... UNZOOM the entire plot (AUX BOTH AUY BOTH PLOT) */
/* --------------------------------------------------- */
PRIVATE REAL ZoomRminHold[4], ZoomRmaxHold[4];
PRIVATE int ZoomAutoFlagHold=-1;
PRIVATE LOGICAL HaveZoomed=FALSE;

int gpt_do_zoom(void) {

	int i, ix=BOTTOM, iy=LEFT;						/* Index for X, index for Y */
	REAL t1,t2,t3,t4;
	int ichr;

	GptSetRange();										/* Set range */
	if (! PlotBoxCursor(&t1, &t2, &t3, &t4, &ichr)) {
		gen_err("ZOOM not implemented for this device");
		return(NOMORE);
	} else if (ichr == 0x03 || ichr == 0x1B) {
		TTYputs("ZOOM aborted\n");
		return(NOMORE);
	}

	if (! HaveZoomed) {							/* Have we already stored anything */
		for (i=0; i<4; i++) {
			ZoomRminHold[i] = Gpt->rmins[i];
			ZoomRmaxHold[i] = Gpt->rmaxs[i];
		}
		ZoomAutoFlagHold = Gpt->AutoFlag;
	}
	if (Gpt->BoxMode) {									/* Which vars? */
		ix = 2*Gpt->plx;
		iy = 2*Gpt->ply + 1;
	} 
	Gpt->rmins[ix] = t1;
	Gpt->rmins[iy] = t2;
	Gpt->rmaxs[ix] = t3;
	Gpt->rmaxs[iy] = t4;
	LexInsText("plot");
	Gpt->AutoFlag &= ~((0x101<<ix) | (0x101<<iy));	/* No longer automatic bits! */
	HaveZoomed = TRUE;
	return(OKAY);
}

/* ------------------------- */
int gpt_do_unzoom(void) {

	INT i;
	char token[OPTION_STR_SIZE];

	if (HaveZoomed) {							/* Is there something to restore? */
		for (i=0; i<4; i++) {
			Gpt->rmins[i] = ZoomRminHold[i];
			Gpt->rmaxs[i] = ZoomRmaxHold[i];
		}
		Gpt->AutoFlag = ZoomAutoFlagHold;
	}
	HaveZoomed = FALSE;
	if (! LexGetOption(token, sizeof(token))) LexInsText("plot");
	return(OKAY);
}


/* ============================================================================
-- FIX_GRID - Fix a specified grid to the data (implicit sort) 
--
-- Options:  -POINTS | -NPT         -- Number of values
--           -RANGE <xlow> <xhigh>  -- range of data
--           -FROM <xlow>           -- partial ranges
--           -TO   <xhigh>
--           -BY   <increment>      -- Instead of number of points
--
--           -MATCH <curve>         -- assign to X-coordinates of given curve
--           -SPLINE                -- use spline fit instead of linear
--
-- Usage:  gpt_do_fixgrid()
--
-- Inputs: x,y,npt,nptmax,ids - curve to work with
--
-- Output: x,y,npt - Modified to have requested number of points!
--
-- Note: Mode 1 - data interpolated on a uniformly spaced grid.

-- Notes: (1) DATA is implicitly sorted -STRICT -AVERAGE before interpolation
--        (2) Unlike DOS version, default range is always the extent of the
--            current data if -RANGE is not specified with current NPT
--        (3) -SPLINE is handled as a series of calls and error messages may
--            appear unusual.
============================================================================ */
static char FixGridHelp[] = 
"\n"
" Command to establish a known X-axis grid on an experimental data set.  Uses\n"
" either linear or spline interpolation with user choice of extrapolation by\n"
" constant extension or zero extension.  Can establish a fixed spaced grid, or\n"
" match the grid of an existing curve.\n"
"\n"
"   Fix_Grid [ [-CURVE] <curve>] [-opts]\n"
"\n"
" An optional curve may be specified.  If not given, will operate on the main\n"
" curve currently active.\n"
"\n"									 
" Options:\n"
"   -help         | -?                       Prints this help message\n"
"   -points <npt> | -npt <npt> | -pts <npt>  Number of points in final data set\n"
" 	 -from <low>   | -to <high>               Lower/upper limit of X range\n"
"   -range <low> <high>                      Lower/upper limit of X range\n"
"   -by <inc>                                Alternate to -points\n"
"   -match <curve>                           Set on same X-grid as existing curve\n"
"   {-linear | -spline}                      Spline or linear inter/extrapolation\n"
"   {-zero | -constant}                      Zero or constant extend out of range\n"
"\n"
" Defaults:\n"
"   Same range and number of points as existing curve.\n"
"   Linear interpolation between points\n"
"   Constant extend from last known data point to out of range values\n"
"\n"
" Examples: fix_grid\n"
"           fix_grid -range 0 100 -by 1\n"
"           fix_grid -match c1 -spline -zero\n"
"           fix_grid -spline -range -100 100 -points 2000\n";

int gpt_do_fixgrid(void) {

	char token[OPTION_STR_SIZE];
	int i, itype, npt, nptnew, isize, isame,istore, imatch;
	void *aptr,							/* For looking up addresses					*/
		  *spl;							/* Work area for spline							*/
	double dx;							/* Higher precision so clean					*/
	BOOL by_set;						/* Number of points or by						*/
	REAL xmin, xmax,					/* Range for the fit								*/
		  by,								/* Increment range								*/
		  *x, *y,						/* Original data									*/
		  *xgrid=NULL,					/* X-grid if specified from a curve			*/
		  *ynew,							/* New y data										*/						
		  *x_1,*x_2,*y_1,*y_2,		/* For deleting duplicates on sort			*/
		  xpt;						
	BOOL spline=FALSE,				/* Spline instead of linear interpolation	*/
		  zero_extend=FALSE;			/* Zero extend outside actual data range	*/

/* First, look for a help request */
	if (LexCheckHelp("FixGrid", FixGridHelp, NULL)) return(OKAY);

/* Curve components for local use */
	npt = GptCurve->npt;
	x   = GptCurve->x;
	y   = GptCurve->y;

/* Set default number, etc */
	if (npt < 2) {
		ERRprintf("ERROR: Kind of tough to interpolate with only 1 point (FIX_GRID)\n");
		return(NOMORE);
	} else if (GptCurve->z != NULL) {
		ERRprintf("ERROR: Well, you got me.  What is a fixed grid in 3D? (FIX_GRID)\n");
		return(NOMORE);
	}

/* And set initial paramters */
	nptnew = npt;								/* Default same # points */
	ArrayMinMax(x, npt, &xmin, &xmax);

/* Scan for options */
	by_set = FALSE;
	by = 0;										/* Default is by number of points */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-points", 3) || LexEqual(token, "-npt", 4) || LexEqual(token, "-pts", 4)) {
			nptnew = LexGetInt(nptnew, "Number of points (NPT): ");
		} else if (LexEqual(token, "-by", 3)) {
			by = LexGetReal(by, "Increment on data set (no change): ");
			by_set = (by != 0);
		} else if (LexEqual(token, "-from", 3)) {
			xmin = LexGetReal(xmin, "Minimum X value (full): ");
		} else if (LexEqual(token, "-to", 3)) {
			xmax = LexGetReal(xmax, "Maximum X value (full): ");
		} else if (LexEqual(token, "-range", 3)) {
			xmin = LexGetReal(xmin, "Minimum X value (full): ");
			xmax = LexGetReal(xmax, "Maximum X value (full): ");
		} else if (LexEqual(token, "-spline", 3)) {
			spline = TRUE;
		} else if (LexEqual(token, "-zero", 2)) {
			zero_extend = TRUE;
		} else if (LexEqual(token, "-constant", 4)) {
			zero_extend = FALSE;
		} else if (LexEqual(token, "-match", 3)) {
			if (LexGetTokenP(token, sizeof(token), "Array/Curve to match X-grid (none): ")) {
				xgrid = NULL;
				if (GVGetInfo(token, &itype, &aptr)) {
					if (itype == GV_2DCURVE || itype == GV_3DCURVE) {
						xgrid =  (*((CURVE **) aptr))->x;
						isize =  (*((CURVE **) aptr))->npt;
					} else if (itype == GV_ARRAY || itype == GV_ARRAY_LINK) {
						xgrid =  (*((ARRAY **) aptr))->x;
						isize = *(*((ARRAY **) aptr))->size;
					}
				}
				if (xgrid == NULL) {
					ERRprintf("ERROR: %s does not exist as a curve or array (FIX_GRID)\n", token);
					return(NOMORE);
				}
			}
		} else {
			ERRprintf("ERROR: %s unrecognized as an option (FIX_GRID)\n", token);
			return(NOMORE);
		} 
	}


/* If matching, set nptnew now and turn off by */
	if (xgrid != NULL) { nptnew = isize; by_set = FALSE; }			/* Must be! */

/* If -by, determine number of points now */
	if (by_set) nptnew = (int) ((fabs(xmax-xmin)+0.01*fabs(by))/fabs(by)) + 1;
	
/* Keep everything in range */
	if (nptnew > GVI_MAX_LENGTH) nptnew = GVI_MAX_LENGTH;
	if (xgrid == NULL && (xmax == xmin || nptnew < 2) ) {
		ERRprintf("ERROR: Invalid range (%g %g) or points (%d) <= 1 (FIX_GRID)\n", xmin,xmax, nptnew);
		return(NOMORE);
	}

/* Sort -STRICT & -AVERAGE the curve so easy to search */
	heap_sort(x,y,NULL, npt, SORT_ON_X | SORT_DOX | SORT_DOY);
	istore = isame  = 1;								/* Single point the same	*/
	x_1=x_2=x; y_1=y_2=y;							/* Last stored value */
	for (i=1; i<npt; i++) {							/* Search forward */
		x_2++; y_2++;									/* Next point		*/
		if (*x_2==*x_1) {								/* Equal values?	*/
			*y_1 = (isame*(*y_1)+(*y_2)) / (isame+1);
			isame++;
		} else {
			*(++x_1) = *x_2;							/* Store in next slot */
			*(++y_1) = *y_2;
			isame = 1;
			istore++;
		}
	}
	npt = GptCurve->npt = istore;

/* If spline, do the spline fit now */
	if (spline) {
		if ( (spl = GVFitSpline(NULL, x,y,npt,0)) == NULL) {
			ERRprintf("ERROR: Unable to spline fit data (probably npt < 3) (FIX_GRID)\n");
			return(NOMORE);
		}
	}

/* Create an array to receive interpolations and get ready to go */
	ynew = malloc(nptnew*sizeof(*ynew));
	if (xgrid == NULL) dx = (xmax-xmin) / (nptnew-1.0);

/* Constaint will be x[imatch] <= xpt <= x[imatch+1] */
	for (i=0,imatch=0; i<nptnew; i++) {						/* Do it */
		xpt = (xgrid != NULL) ? xgrid[i] : (REAL) (xmin + i*dx);
		if (xpt < x[0]) {
			ynew[i] = zero_extend ? 0.0f : y[0] ;
		} else if (xpt > x[npt-1]) {
			ynew[i] = zero_extend ? 0.0f : y[npt-1] ;
		} else if (spline) {
			ynew[i] = GVEvalSpline(spl, xpt);
		} else {
			while (imatch > 0     && x[imatch]   > xpt) imatch--;
			while (imatch < npt-2 && x[imatch+1] < xpt) imatch++;
			ynew[i] = y[imatch] + (y[imatch+1]-y[imatch])/(x[imatch+1]-x[imatch])*(xpt-x[imatch]);
		}
	}
	
/* Now, copy back to curve, resizing if necessary */
	if (nptnew > GptCurve->nptmax) {				/* Do I need to resize? */
		GVResize(GptUseCurve, nptnew);
		GptLinkXYZ(GptUseCurve);
		x = GptCurve->x;
		y = GptCurve->y;
	}
	for (i=0; i<nptnew; i++) {
		x[i] = (xgrid != NULL) ? xgrid[i] : (REAL) (xmin + i*dx);
		y[i] = ynew[i];
	}
	GptCurve->npt = nptnew;

/* Deallocate my work space */
	if (spline) free(spl);
	free(ynew);

	return(OKAY);
}

/* ============================================================================
-- . . . . GRID command 
--
-- ... Subroutine to draw a grid on the plot
--
--     Usage: mkgrid$ [-COL n] [-LTYPE n] [-VECSIZE n]
--
--     Inputs: none
============================================================================ */
int gpt_do_grid(void) {

	static REAL vecsiz=0.01f;							/* Vector size default	*/
	static int  icol=1, iline=4;						/* Default color, line	*/
	char token[OPTION_STR_SIZE];
	int   PenSave;
	REAL  VecSave;

	REAL	x,dxtest,xmin,xmax,dx,dx2;
	int ix=BOTTOM, iy=LEFT, ix2,iy2;			/* Index for X, index for Y */

	if (! PlotSystem(2, NULL, NULL)) return(NOPLOTTER);

/* --------------------------------------------------------------- */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-PEN", 4) || LexEqual(token, "-COLOR", 3)) {
			icol = LexGetInt(icol, "Pen color: ");
		} else if (LexEqual(token, "-LTYPE", 3)) {
			iline = LexGetInt(iline, "Line type: ");
			iline = max(1,min(iline,7));
		} else if (LexEqual(token, "-VECSIZE", 4)) {
			vecsiz = LexGetReal(vecsiz, "Repeat size for LTYPE: ");
		} else {
			ERRprintf("ERROR: %s is an unrecognized GRID option\n", token);
			return(NOMORE);
		}
	}

	GptSetRange();												/* Set range */
	PenSave = PlotSelectPen(icol);						/* Select color	 */
	PlotQueryLineType(NULL, &VecSave);					/* Get dashes mode */
	PlotSetLineType(iline, vecsiz);
	
	if (Gpt->BoxMode) {										/* Determine actual IX */
		ix = 2*Gpt->plx;
		iy = 2*Gpt->ply + 1;
	} 
	ix2 = ix;
	iy2 = iy;
	if ( (ix == TOP)   && (Gpt->xtop   != IS_ON) ) ix2 = BOTTOM;
	if ( (iy == RIGHT) && (Gpt->yright != IS_ON) ) iy2 = LEFT;

	if (Gpt->logtype[ix2]) {
		PlotAutoLogScale(Gpt->rmins[ix2], Gpt->rmaxs[ix2], NULL, NULL, &dx, &dx2, NULL);
	} else {
		PlotAutoScale(Gpt->rmins[ix2], Gpt->rmaxs[ix2], NULL, NULL, &dx, &dx2, NULL);
	}

	if (Gpt->udx[ix]  != 0.0f) {						/* Overrides? */
		dx  = Gpt->udx[ix];
	} else if (Gpt->udx[ix2] != 0.0f) {				/* Or on other axis? */
		dx  = Gpt->udx[ix2];
	}

	dxtest = (REAL) fabs(dx/10);
	xmin = min(Gpt->xmin, Gpt->xmax);
	xmax = max(Gpt->xmin, Gpt->xmax);
	x = (REAL) (dx*floor(xmin/dx));	if (fabs(xmin-x) < dxtest) x += dx;
	while (x <= xmax-dxtest) {
		PlotMove(x, Gpt->ymin, 3);
		PlotMove(x, Gpt->ymax, 2);
		x += dx;
	}

	if (Gpt->logtype[iy2]) {
		PlotAutoLogScale(Gpt->rmins[iy2], Gpt->rmaxs[iy2], NULL, NULL, &dx, &dx2, NULL);
	} else {
		PlotAutoScale(Gpt->rmins[iy2], Gpt->rmaxs[iy2], NULL, NULL, &dx, &dx2, NULL);
	}
	if (Gpt->udx[iy]  != 0.0f) {					/* Overrides? */
		dx  = Gpt->udx[iy];
	} else if (Gpt->udx[iy2] != 0.0f) {			/* Or on other axis? */
		dx  = Gpt->udx[iy2];
	}

	dxtest = (REAL) fabs(dx/10);
	xmin = min(Gpt->ymin, Gpt->ymax);
	xmax = max(Gpt->ymin, Gpt->ymax);
	x = (REAL) (dx*floor(xmin/dx));	if (fabs(xmin-x) < dxtest) x += dx;
	while (x <= xmax-dxtest) {
		PlotMove(Gpt->xmin, x, 3);
		PlotMove(Gpt->xmax, x, 2);
		x += dx;
	}

	PlotSelectPen(PenSave);								/* Restore pen				*/
	PlotSetLineType(1, VecSave);						/* Restore line types	*/
	PlotFlush();											/* And flush output		*/
	return(OKAY);
}

/* ============================================================================
--  ... EDIT_DATA command to delete points from data set
--
--     Function to allow some rudimentary editing of data set
--
--     Usage:  LOGICAL EDTPNT$(X,Y,NPT,NPTMAX)
--
--     Inputs: X,Y - Buffers of X and Y coordinates
--	      NPT - Number of data points valid
--	      NPTMAX - Maximum number of allowed points
--
--     Output: Modified X and Y buffers
--	      EDTPNT$ = Success  .FALSE. ==> Not in acceptable graphics mode
--
--     Note: Only currently implemented for devices with a cursor
============================================================================ */
int gpt_do_editdata(void) {

	REAL t1,t2,t3,dist, *x,*y;
	INT i, ipnt=0, npt;
	int ichr;
	char token[OPTION_STR_SIZE];
	LOGICAL track=FALSE;

	track = LexGetOption(token, sizeof(token));	/* Any option ==> tracking */

	if (! PlotSystem(3, NULL, NULL)) {
		gen_err("EDIT_DATA requires cursor capable device");
		return(NOMORE);
	}

	GptSetRange();
	TTYputs("[D-del | A-add | T-track | <sp>-show | other->EXIT]\n");
	TTYflush();

	x = GptCurve->x; y = GptCurve->y; npt = GptCurve->npt;

	while (TRUE) {
		if (track) {
			if (! PlotTrackingCursor(x,y,npt, &ipnt, &ichr)) {
				gen_err("No tracking cursor available");
				TTYflush();
				track = FALSE;
				continue;
			}
			t1 = x[ipnt];
			t2 = y[ipnt];
			dist = 0.0f;
		} else {
			PlotCursor(&t1, &t2, &ichr);
			ipnt = 0;
			dist = 1.0e37f;
			for (i=0; i<npt; i++) {
				t3 = (x[i]-t1)*(x[i]-t1)/(Gpt->xmax-Gpt->xmin)/(Gpt->xmax-Gpt->xmin)
					+ (y[i]-t2)*(y[i]-t2)/(Gpt->ymax-Gpt->ymin)/(Gpt->ymax-Gpt->ymin);
				if (t3 < dist) {
					dist = t3; ipnt = i;
				}
			}
		}

		switch (ichr) {
			case 'x':											/* Delete */
			case 'X':											/* Delete */
			case 'd':
			case 'D':
				if (dist >= 0.002f || npt < 1) {
					gen_err("No point within reasonable distance");
					TTYflush();
					break;
				}
				t1 = x[ipnt];								/* Make sure exact */
				t2 = y[ipnt];
				npt--;										/* Eliminate point */
				for (i=ipnt; i<npt; i++) {
					x[i] = x[i+1]; y[i] = y[i+1];
				}
				i = PlotSelectPen(0);					/* Get background color */
				if (Gpt->symtype != 0) 
					PlotSymbol(t1, t2, Gpt->symsiz, (char) abs(Gpt->symtype));
				else 
					PlotMove(t1,t2,4);
				PlotSelectPen(i);
				PlotFlush();
				TTYprintf(" Point #%i deleted: %g %g\n", ipnt, t1, t2);
				TTYflush();
				break;
			case 'M':											/* Move */
			case 'm':
				if (dist >= 0.002f || npt < 1) {
					gen_err("No point within reasonable distance");
					TTYflush();
					break;
				}
				t1 = x[ipnt];								/* Make sure exact */
				t2 = y[ipnt];
				i = PlotSelectPen(0);					/* Get background color */
				if (Gpt->symtype != 0) {
					PlotSymbol(t1, t2, Gpt->symsiz, (char) abs(Gpt->symtype));
				} else {
					PlotMove(t1,t2,4);
				}
				PlotSelectPen(i);
				PlotFlush();
				PlotCursor(&t1, &t2, &ichr);
				if (ichr != 0x1B) {						/* If not escape, move it */
					x[ipnt] = t1;
					y[ipnt] = t2;
					if (Gpt->symtype != 0) {
						PlotSymbol(t1, t2, Gpt->symsiz, (char) abs(Gpt->symtype));
					} else {
						PlotMove(t1,t2,4);
					}
					PlotFlush();
					TTYprintf(" Point #%i moved to: %g %g\n", ipnt, t1, t2);
					TTYflush();
				}
				break;
			case 'a':								/* A ==> ADD */
			case 'A':								/* A ==> ADD */
				if (track) {
					gen_err("Cannot add points w/ tracking cursor");
				} else if (npt >= GptCurve->nptmax) {
					gen_err("No room to add points to curve");
				} else {
					x[npt] = t1; y[npt] = t2;
					if (Gpt->symtype != 0) {
						PlotSymbol(t1, t2, Gpt->symsiz, (char) abs(Gpt->symtype));
					} else {
						PlotMove(t1, t2, 4);
					}
					PlotFlush();
					TTYprintf(" Point #%i added: %g %g\n", npt, t1, t2);
					npt++;
				}
				TTYflush();
				break;
			case '0':										/* Show value */
			case ' ':
				TTYprintf(" Cursor at: %g %g\n", t1,t2);
				TTYflush();
				break;
			case 't':
			case 'T':
				track = !track;
				break;
			default:
				GptCurve->npt = npt;
				return(OKAY);
		}
	}
	panic; return(OKAY);							/* BETTER NOT HAPPEN! */
}

/* . . . . Enter 3-D mode control . . . . */
int gpt_do_2d(void) {
	if (! GVModifyCurve(GptUseCurve, GV_2DCURVE)) return(NOMORE);
/*	TTYputs("Curve now 2-dimensional\n");	*/
	Gpt->mode_3d = FALSE;
	return(OKAY);
}

int gpt_do_3d(void) {
	if (! GVModifyCurve(GptUseCurve, GV_3DCURVE)) return(NOMORE);
/*	TTYputs("Curve now 3-dimensional\n");	*/
	Gpt->mode_3d = TRUE;
	return(OKAY);
}

/* . . . . Load or execute an external user routine process . . . . */
static char UserHelp[]=
	"The user command is a feature allowing external code to be integrated into\n" 
	"GENPLOT.  These must be written in C and linked with specific modules and\n"
	"function names.  Default extension on the module is .usr.  Examples are\n"
	"provided in the source directories.\n"
	"\n"
	"Usage: USER [-? | -help | -load | -unload | -free | -read | -write]\n"
	"\n"
	"Options:\n"
	"   -help | -?        Prints this help message\n"
	"\n"
	"   -load <module>    Loads the specified .usr module.  Implicitly frees any\n"
	"                     currently loaded user module\n"
   "   -free | -unload   Releases the current .usr module.\n"
   "   -read <filename>  Calls the read user module to read specified file\n"
   "   -write <filename> Calls the write user module to write current data\n"
	"\n"
	"Notes:\n"
	"  (1) This command was primarily provided to be able to read and write\n"
	"      non-standard data files, or to process data with very application\n"
	"      specific code.  User modules can also define new functions, but this\n"
   "      is better handled with the LOAD command as the module can be persistent.\n"
   "  (2) With no options, USER command executes the default routine in the module.\n"
   "  (3) -READ and -WRITE call the corresponding routines in the user module\n"
   "\n"
   " Examples: user -load gpib            /* Load GPIB control module\n"
   "           user -read                 /* Reads via the GPIB\n"
	;

int gpt_do_user(void) {

	char token[OPTION_STR_SIZE], pathname[PATH_MAX];

/* Check if option is for help */
	if (LexCheckHelp("User", UserHelp, NULL)) return(OKAY);

	if (LexGetOption(token, sizeof(token))) {					/* Check for reload */
		if (LexEqual(token, "-LOAD", 2)) {
			if (! LexGetFileP(pathname, sizeof(pathname), "User module filename: ")) return(OKAY);
			return(GptLoadUserDLL(pathname));
		} else if (LexEqual(token, "-UNLOAD", 4) || LexEqual(token, "-FREE", 2)) {
			GptFreeUserDLL(TRUE);
			return(OKAY);
		} else if (LexEqual(token, "-READ", 2)) {
			if (GptUserRead == NULL) {
				gen_err("No user read routine loaded");
				return(NOMORE);
			} else if (! LexGetFileP(pathname, sizeof(pathname), "File to read: ")) {
				return(OKAY);
			}
			return( ((*GptUserRead)(pathname, GptUseCurve) == 0) ? OKAY : NOMORE);
		} else if (LexEqual(token, "-WRITE", 2)) {
			if (GptUserWrite == NULL) {
				gen_err("No user write routine loaded");
				return(NOMORE);
			} else if (! LexGetFileP(pathname, sizeof(pathname), "File to write: ")) {
				return(OKAY);
			}
			return( ((*GptUserWrite)(pathname, GptUseCurve) == 0) ? OKAY : NOMORE);
		} else {
			LexBackup();
		}
	}
	return ( ((*GptUserFnc)(GptUseCurve) == 0) ? OKAY : NOMORE );
}

/* . . . . Load and initialize an external user module . . . . */
/* . . . . Load or execute an external user routine process . . . . */
static char LoadHelp[]=
	"The LOAD command is a feature allowing external code to be integrated into\n" 
	"GENPLOT.  These must be written in C and linked with specific modules and\n"
	"function names.  Default extension on the module is .mdl.  In contrast to .usr\n"
   "files (loaded with ther user -load command), .mdl module are persistent until\n"
   "Genplot exits or the module deactivates itself.  Multiple .mdl modules may be\n"
   "loaded in a given session.\n"
	"\n"
	"Usage: LOAD [-? | -help] <module>\n"
	"\n"
	"Options:\n"
	"   -help | -?        Prints this help message\n"
	"\n"
	" Examples: load pearson          /* Load Pearson V math functions\n"
	;
int gpt_do_load(void) {
	char pathname[PATH_MAX];

/* Check if option is for help */
	if (LexCheckHelp("Load", LoadHelp, NULL)) return(OKAY);

	if (! LexGetFileP(pathname, sizeof(pathname), "User module filename: ")) return(OKAY);
	return(GptLoadUserMDL(pathname));
}
