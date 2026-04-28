/* SYMBOL.C - Plot package symbol plotter */

/* ===========================================================================
-- Modification history
--
-- MOT 12/17/94 - Change to permit full code page (256) in character sets
-- MOT 1/7/95   - Modified to use device driver drawing of symbols where ok
-- MOT 1/8/95   - Massive modification of character sets to character font
--                and attributes.  Much cleaner flow to handle Times-Roman
--                and Helvetica fonts on known devices.
--  Need to implement and deal with a font metric.
=========================================================================== */

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
#include <limits.h>
#include <ctype.h>
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	SYMBOL_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic			SysPanic(__FILE__, __LINE__)
#define	NOLDS			20
#define	CHRSET_FILE	"hdata.chr"

typedef struct _SYMLOC {					/* Global information on string */
	LOGICAL  DoDrawing;						/* Are we to do the drawing	*/
	REAL		height;							/* Requested height				*/
	REAL		theta;							/* Just so we have				*/
	REAL		costh, sinth;					/* Cos/Sin of angle				*/
	REAL		xxstart,yystart, zzstart;	/* Start coordinates of line	*/
	REAL		xx[NOLDS],yy[NOLDS];			/* Used coordinates				*/
} SYMLOC;

typedef struct _SYMMODS {
	INTEGER family;						/* Font to be using							*/
	INTEGER cp;								/* Code page to be using					*/
	INTEGER attrib;						/* Attributes of font						*/
	INTEGER mypen;							/* Current pen color							*/
	REAL		height;						/* Height of characters						*/
	REAL		yscrpt;						/* Offset for Sub/superscripts			*/
	INTEGER	lastbr;						/* Control for last brace					*/
	LOGICAL  textsym;						/* Special handle of symbols				*/
	REAL		leftshift;					/* Fractional Left shift in cell (0.0)	*/
	REAL		hscale;						/* Baseline shift scaling (1.0)			*/
	struct _SYMMODS *last;				/* Last symbol structure					*/
} SYMMODS;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE void    Draw(int ich, SYMLOC *sym, SYMMODS *mods);
PRIVATE SYMMODS *superscript(SYMMODS *tmp);
PRIVATE SYMMODS *subscript(SYMMODS *tmp);
PRIVATE void    subscript_old  (SYMMODS *mods);
PRIVATE void    superscript_old(SYMMODS *mods);
PRIVATE LOGICAL unscript_old   (SYMMODS *mods);
PRIVATE SYMMODS *unwind(SYMLOC *sym, SYMMODS *mods, int type);
PRIVATE char   *DrawSpecial(char *aptr, SYMLOC *sym, SYMMODS *mods);
PRIVATE void	 MapFontSelection(int number, int *family, int *cp, int *attrib);
PRIVATE int		 SelectHersheyMap(int family, int cp, int attrib);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
extern REAL Plot_Z_Default;					/* Default Z value on 2D plot	 */
extern REAL Plot_Z_Inch_Default;				/* Private in plot/axis/symbol */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
EXPORT int	SymbolMap[NUMBER_OF_SYMBOLS+1]    = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14, 0};
EXPORT REAL SymbolScale[NUMBER_OF_SYMBOLS+1] = {1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
EXPORT const char *SymbolNames[NUMBER_OF_SYMBOLS] = {
	"open square",					"open circle",					"open triangle",
	"cross",							"x",								"open diamond",
	"open star",					"filled square",				"filled circle",
	"filled triangle",			"filled left triangle",		"asterisk",
	"filled right triangle",	"filled star",					"Star of David"
};

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
#define	NCHARS			256			/* # of symbols per font index */
#define	INDEX_OFFSET	0				/* Offset from ASCII symbol to map index */
	PRIVATE int ChrLoaded=FALSE;		/* Execute only once */
	PRIVATE unsigned short NumFonts;	/* Number of sets	*/
	PRIVATE short *ChrNames;			/* Letter of set	 ChrNames[NumFonts]    */
	PRIVATE short *ChrIndex;			/* Index of stroke ChrIndex[NumFonts,NCHARS] */
	PRIVATE short *ChrStroke;			/* Actual strokes	 ChrStroke[huge]       */
	PRIVATE REAL xlen;					/* X-length of drawing (for TLEN)		  */

#define UPSIDEDOWN_QM	168			/* Codepage 850 symbol for inverted ? */
#define UPSIDEDOWN_EXCL	173			/* Codepage 850 symbol for inverted ! */

#define ASCHAT				'^'			/* Special command mode		*/
#define SUPERSCRIPT		'~'			/* Superscript command		*/
#define UNSUPERSCRIPT	'+'			/* Unsuperscript command	*/
#define SUBSCRIPT			'`'			/* Subscript command			*/
#define UNSUBSCRIPT		'='			/* Unsubscript command		*/
#define ASCDOT				'.'			/* Special characters		*/
#define SPACE				' '			/* character					*/
#define BACKUP				'-'			/* backspace character		*/
#define LEFTBRACE			'{'			/* left brace					*/
#define RIGHTBRACE		'}'			/* right brace					*/
#define UNDERSCORE		'_'			/* underscore character		*/
#define BACKSLASH			'\\'			/* (quote) character			*/
#define FONTSELECT1		'F'			/* for font changes			*/
#define FONTSELECT2		'f'
#define PENSELECT1		'P'			/* for pen changes			*/
#define PENSELECT2		'p'

#define DO_BACKSPACE		   0x70FE	/* Use upper levels for commands		*/
#define DO_THINSPACE			0x70FD	/* Special command for \,				*/

#define DEGREE_TO_RAD  0.0174532925f
#define TLENXVALUE		 203498.0f		/* Kludge to figure out if SYMBOL */
#define TLENYVALUE		 123978.0f		/* called from TLEN */


/* ===========================================================================
--  PlotLoadFonts - Loads stroke data for characters
--
--  Usage: LOGICAL PlotLoadFonts(INTEGER key)
--
--  Inputs: none
--
--  Output: On first entry, loads character set data from unformatted file
=========================================================================== */
LOGICAL PlotLoadFonts(INTEGER key) {

	CHAR file[PATH_MAX];						/* Filename				*/
	FILE *unt=NULL;							/* File stream unit	*/
	CHAR *errstr;
	unsigned short i2;						/* Must be 2 byte character */

	if (key == -1) {									/* Free all memory when exiting */
		if (ChrNames  != NULL) free(ChrNames);
		if (ChrIndex  != NULL) free(ChrIndex);
		if (ChrStroke != NULL) free(ChrStroke);
		ChrNames = ChrIndex = ChrStroke = NULL;
		ChrLoaded = FALSE;
		return(TRUE);
	} else if (ChrLoaded) {							/* Are we already loaded? */
		return(TRUE);
	}

	SysResolveDyntName(file, CHRSET_FILE, sizeof(file));		/* Open file */

LoadFile:
	if ( (unt=fopen(file, "rb")) == NULL) goto LoadError;

	if (fread(&NumFonts,sizeof(NumFonts),1,unt) != 1) 
		goto ReadError;												/* # of sets in file */
	ChrNames = malloc(sizeof(*ChrNames)*NumFonts);			/* Allocate space */
	ChrIndex = malloc(sizeof(*ChrIndex)*NumFonts*NCHARS);	/* Allocate space */
	if (fread(ChrNames,sizeof(*ChrNames),NumFonts,unt) != NumFonts) 
		goto ReadError;												/* Read Labelling data */
	if ((int) fread(ChrIndex,sizeof(*ChrIndex),NCHARS*NumFonts,unt) != (NCHARS*NumFonts))
		goto ReadError;												/* Read Index data	  */

	if (fread(&i2, 2, 1,unt) != 1) goto ReadError;			/* # of JOT words */
	ChrStroke = malloc(sizeof(*ChrStroke)*i2);				/* Allocate stroke space */
	if (fread(ChrStroke,2,i2,unt) != i2) goto ReadError;	/* Jot data */

	fclose(unt);

	PL_Symbols.DefaultCharMap = DSP_FAMILY_TMSRM | DSP_CP_ASCII | DSP_ATTRIB_NORMAL ;
	ChrLoaded = TRUE;										/* And don't load again	*/

	goto AllExit;

/* ----------------------------------------------------- */
LoadError:
	errstr = "Missing/invalid character data: %s\n";
	goto HandleError;
ReadError:
	errstr = "Error reading character data: %s\n";
	goto HandleError;
/* ----------------------------------------------------- */
HandleError:
	ERRprintf(errstr,file);
	if (unt != NULL) fclose(unt);
	UserInput("Enter pathname to " CHRSET_FILE " (abort): ",file, sizeof(file));
	if (*file == '\0') return(FALSE);
	goto LoadFile;
/* ----------------------------------------------------- */

AllExit:											/* To avoid compiler warning */
	return(TRUE);
}

/* ============================================================================
--  Routine to return and set a specific map for the character sets.
--
--  Usage: OLDMAP = PlotSelectFont(NEWMAP)
--
--  Inputs: newmap - font indicator.  If family == FAMILY_MASK, then
--                   request to map old 0-10 font numbers to new style
--                 -1 is a request for return of current map only.
--
--  Returns: oldmap - new style font selector
============================================================================ */
INTEGER PlotSelectFont(INTEGER ich) {

	INTEGER rcode;
	int family, cp, attrib;						/* Separate components */

/* Decide what to return first */
	rcode = PlotWindow->chrmap;				/* And possibly query only */
	if (ich == -1) return(rcode);

/* Now, separate and create font/attrib components */
	if ((ich & DSP_FAMILY_MASK) == DSP_FAMILY_MASK) {	/* Special condition */
		family = rcode & DSP_FAMILY_MASK;
		cp     = rcode & DSP_CP_MASK;
		attrib = rcode & DSP_ATTRIB_MASK;
		MapFontSelection(ich, &family, &cp, &attrib);
	} else {
		family = ich & DSP_FAMILY_MASK;
		cp     = ich & DSP_CP_MASK;
		attrib = ich & DSP_ATTRIB_MASK;
		if (family == 0) family = rcode & DSP_FAMILY_MASK;	/* Keep old font	*/
		if (cp     == 0) cp     = rcode & DSP_CP_MASK;		/* Keep old mask	*/
	}

	PlotWindow->chrmap = family | cp | attrib;			/* And recombine	*/
	return(rcode);
}

/* ============================================================================
--  Routine to place a symbol at a specified user coordinate position
--
--  Usage: PlotSymbol(x, y, height, ich);
--
--  Inputs: x,y    - position (user space)
--          height - size of character (in inches)
--          ich    - symbol to draw
--
--  Output: none
--
--  Returns: void
============================================================================ */
void PlotSymbol(REAL x, REAL y, REAL height, CHAR ich) {
	Plot3DSymbol(x, y, Plot_Z_Default, height, ich);
	return;
}

void Plot3DSymbol(REAL x, REAL y, REAL z, REAL height, CHAR ich) {

	int		clpmod_sav;									/* Save of the clipping mode */
	LOGICAL	saveon_sav;									/* State of SAVEON variable */
	INTEGER	savlin;										/* Save old linetype		*/
	SYMLOC	sym;											/* Symbol parameters		*/
	SYMMODS	mods;											/* And modifiable parms	*/
	REAL     xt,yt,zt;
	
/* Copy user parameters to temporary variables */
	sym.DoDrawing = TRUE;								/* Yes we are drawing it!	*/
	sym.height    = height;								/* Actual height				*/
	sym.costh     = 1;									/* No rotation */
	sym.sinth     = 0;

	mods.family   = PlotWindow->chrmap & DSP_FAMILY_MASK;	/* Family component */
	mods.attrib   = PlotWindow->chrmap & DSP_ATTRIB_MASK;	/* Attribute part	*/
	mods.cp       = DSP_CP_SYMBOL;					/* But use symbols			*/
	mods.height   = height;								/* Height absolute			*/
	mods.yscrpt   = 0;									/* Init sup/Sub offset		*/
	mods.textsym  = FALSE;
	mods.leftshift = 0.0;
	mods.hscale   = 1.0;

	if (PlotWindow->usrnbl) {
		PlotConvert3DScales(USER_TO_INCH, x,y,z, &xt, &yt, &zt);
	} else {
		xt = x; yt = y; zt = z;
	}

/* During symbols, turn off line-type function and user coordinates */
	savlin = 1;										/* If not otherwise, linetype 1 */
	saveon_sav = PlotWindow->Save.On;		/* Save status of saveon */
	clpmod_sav = PlotWindow->clpmod;
	if (! PlotWindow->clipsymbols && PlotWindow->clpmod == 0) {
		if (! PlotInBounds(x,y,z)) return;
		PlotWindow->clpmod = 1;					/* Expand to no clipping */
		PlotFixInternal();
	}

	if (PlotWindow->Save.On && (PlotExcludeCurve == NULL)) {	/* Handle HCOPY? */
		SV_PutCmd(SV_MARK, 1,3,0);			/* Make sure enough room! */
		SV_PutReal(x);
		SV_PutReal(y);
		SV_PutReal(z);
		SV_PutReal(height);
		SV_PutInt(ich);
		PlotWindow->Save.On = FALSE;
	}

/* --------------------------------------------------------
--	translate symbol if necessary (along with size)
----------------------------------------------------------- */
	if (ich < 0) ich = -ich;
	ich = ich % NUMBER_OF_SYMBOLS ;			/* Limit it to valid range			  */
	height *= SymbolScale[(int) ich];		/* And symbol scaling factor		  */
	ich     = SymbolMap[(int) ich];			/* Switch the symbol ordering		  */

/* --------------------------------------------------------
-- First, try to draw using device driver implementation.
-- Limited to 2D and when not excluding segments
----------------------------------------------------------- */
	if ( (DEVICE->Capabilities & DEV_CAP_MARKERS) &&
		! PlotWindow->mode_3d && (PlotExcludeCurve == NULL) ) {
		DspDrawMarker mk;
		REAL px, py;
		PlotConvert2DScales(USER_TO_PIXEL, x,y, &px, &py);
		mk.x     = (int) px;
		mk.y     = (int) py;
		mk.isym  = ich;
		mk.size  = (int) (height * PL_Plot.factr * PL_Plot.yscale);
		mk.angle = PL_Plot.orient_angle;
		if ((*DEVICE->dsptch)(DRAWMARKER, DEVICE->DriverBlock, (DSP *) &mk))
			goto AllExit;
	}

/* -------------------------------------------------------- */
/* Failed, draw directly instead										*/
/* -------------------------------------------------------- */

/* Go to solid lines */
	if (PlotWindow->lintyp != 1) savlin = PlotSetLineType(1,0.0f);

/* ... Now, go and actually plot the characters. */
	sym.xx[0] = sym.xxstart = xt;			/* Position in inches */
	sym.yy[0] = sym.yystart = yt;
	            sym.zzstart = zt;
	Draw(ich, &sym, &mods);

/* Leave the plotter in position for more characters.  Reset parms */
	PlotMove3DInch(sym.xx[0], sym.yy[0], sym.zzstart, 3);		/* Pen up at final location */
	if (savlin != 1) PlotSetLineType(savlin,0.0f);

AllExit:
	if (clpmod_sav != PlotWindow->clpmod) {
		PlotWindow->clpmod  = clpmod_sav;		/* Restore clipping mode */
		PlotFixInternal();
	}
	PlotWindow->Save.On = saveon_sav;			/* Restore save mode		 */
	return;
}


/* ===========================================================================
--  PlotQueryStringLength - Calculate the length (inches) of a string of characters
--
--  Usage: REAL = PlotQueryStringLength(HEIGHT, STR, N)
--
--  Inputs: See SYMBOL above
--
--  Output: The actual length of string in plotted inches
--
--  Notes: This routine actually runs through SYMBOL with a flag telling it
--         not to plot anything.  Should be accurate for centering, etc.
========================================================================== */
REAL PlotQueryStringLength(REAL height, char *str, INTEGER n) {

	PlotInchString(TLENXVALUE, TLENYVALUE, height,str, 0.0f, abs(n));	/* Go fake it */
	return(xlen);											/* Hopefully set!!!		 */
}


/* ============================================================================
--  PlotString     - Plots symbols and characters (user coordinates)
--  PlotInchString - Plots symbols and characters (absolute inch coordinates)
--
--  Usage: PlotString(X, Y, HEIGHT, STR, THETA, N)
--         PlotInchString(X, Y, HEIGHT, STR, THETA, N)
--
--  Inputs: X,Y    - Coordinates for starting left corner (REAL)
--          HEIGHT - Symbol height in absolute inches (REAL)
--          STR    - Characters to be drawn (CHARACTER)
--          THETA  - Angle to drawn at
--          N      - Number of symbols in STR
--             < 0 => Pen down before moving to X,Y
--             = 0 => Use length (AS DEFINED) of STR for length
--
--  Note: The N=0 will plot the defined length of the string, not just
--        the effective length.  CHARACTER*132 STR always plots 132 chars.
--
--Oct. 20, 1984 - MOT, LRD
--    Implemented subscript size, offsets as variables rather than constants.
--
--Mar. 3, 1985 - MOT
--    Rewrote SYMBOl, SYMBOL and TLEN by implementing multiple entry points
--    to a single subroutine.
--1Q 1987 - LRD
--    Rewrote inner loop for greater speed
--June 16, 1987 - LRD
--    Changed Max Jots to 200
--June 16, 1987 - LRD
--    Added handler for left-extending characters, used for chrset 8 and 9.
--    The left offset is coded in upper half (i.e. jot/64) of the character
--    width field.  Rick Cochran special.
=========================================================================== */
void PlotString(REAL x, REAL y, REAL height, char *str, REAL theta, INTEGER n) {
	Plot3DString(x, y, Plot_Z_Default, height, str, theta, n);
	return;
}

void Plot3DString(REAL x, REAL y, REAL z, REAL height, char *str, REAL theta, INTEGER n) {
	REAL xm,ym,zm;
	PlotConvert3DScales(USER_TO_INCH, x,y,z, &xm, &ym, &zm);
	Plot3DInchString(xm, ym, zm, height, str, theta, n);
	return;
}

void PlotInchString(REAL x, REAL y, REAL height, char *str, REAL theta, INTEGER n) {
	Plot3DInchString(x, y, Plot_Z_Inch_Default, height, str, theta, n);
	return;
}

/* ----------------------------------------------------------------------------
	real x      - X coordinate to start drawing
	real y;		- Y coordinate to start drawing
	real height - Physical height of characters
	char *str   - String to draw
	real theta  - Angle for drawing
	int  n		- Number of characters
---------------------------------------------------------------------------- */
void Plot3DInchString(REAL x, REAL y, REAL z, REAL height, char *str, REAL theta, INTEGER n) {

	LOGICAL	saveon_sav;						/* State of SAVEON variable */
	INTEGER	savlin,							/* Save old linetype */
				savcol;							/* Save old pen color */
	int		ich,next,i;						/* Decimal equivalent of ASCII char */
	UINT		nn;
	char		*aptr;
	SYMLOC	sym;								/* Symbol parameters */
	SYMMODS	*mods, *tmp;
	
/* Copy user parameters to temporary variables */
	nn = abs(n);
	if (nn == 0) nn = (int) strlen(str); /* Intrinsic length */
	if (nn == 0) return;						/* Return if no characters */

	mods = (SYMMODS *) malloc(sizeof(SYMMODS));	/* Allocate first one and mark as so */
	mods->last = NULL;

/* What a kludge -- were we called by TLEN, in user mode, or in inches? */
	sym.DoDrawing = (x != TLENXVALUE || y != TLENYVALUE);
	if (! sym.DoDrawing) {										/* Start at (0,0) */
		sym.yystart = sym.xxstart = sym.zzstart = 0.0;
	} else {															/* Inch mapping	*/
		sym.xxstart = x;
		sym.yystart = y;
		sym.zzstart = z;
	}

/* During symbols, turn off line-type function and user coordinates */
	saveon_sav = PlotWindow->Save.On;			/* Save status of saveon */
	savcol     = PlotWindow->colour;				/* Save old pen color */
	savlin = 1;

	if (sym.DoDrawing) {
		if (saveon_sav && (PlotExcludeCurve == NULL)) {	/* Handle HCOPY? */
			SV_PutCmd(SV_SYMBOL, 2,4,nn);		/* Put command and size */
			SV_PutReal(x);
			SV_PutReal(y);
			SV_PutReal(z);
			SV_PutReal(height);
			SV_PutReal(theta);
			SV_PutInt(n);							/* Passed value of n */
			SV_PutInt(PlotWindow->chrmap);	/* Character set */
			SV_PutStr(str,nn);					/* String to write */
			PlotWindow->Save.On = FALSE;
		}
		if (PlotWindow->lintyp!=1)								/* Go to solid lines */
			savlin=PlotSetLineType(1,0.0f);	
		if (n < 0)													/* Draw line to symbol */
			PlotMove3DInch(sym.xxstart, sym.yystart, sym.zzstart, 2);
	}

	sym.height = height;										/* Actual height	 */
	sym.theta  = theta;										/* Copy so have	 */
	sym.costh  = (REAL) cos(theta*DEGREE_TO_RAD);	/* Rotation factor */
	sym.sinth  = (REAL) sin(theta*DEGREE_TO_RAD);	/* Rotation factor */
	for (i=0; i<NOLDS; i++) {
		sym.xx[i] = sym.xxstart; 
		sym.yy[i] = sym.yystart;
	}

	mods->height     = height;							/* Height always absolute	*/
	mods->yscrpt     = 0;								/* Init sup/Sub offset		*/
	mods->textsym    = FALSE;
	mods->mypen      = PlotWindow->colour;				/* My starting color	*/
	mods->family     = PlotWindow->chrmap & DSP_FAMILY_MASK;	/* Family component	*/
	mods->cp         = PlotWindow->chrmap & DSP_CP_MASK;		/* Codepage component */
	mods->attrib     = PlotWindow->chrmap & DSP_ATTRIB_MASK;	/* Attribute part	*/
	mods->leftshift  = 0.0;
	mods->hscale     = 1.0;

	mods->lastbr     = 0;								/* Last brace did not exist */

/* ... Now, go and actually plot the characters. */
	aptr = str;
	while (*aptr != '\0' && nn--) {
		ich    = *(aptr++) & 0xFF;					/* Get character (w/o parity) */
		next   = *aptr     & 0xFF;					/* And next character */

		switch (ich) {

			case LEFTBRACE:							/* Substring spec */
				tmp = mods;
				mods = (SYMMODS *) malloc(sizeof(SYMMODS));	/* Allocate new one and mark */
				memcpy(mods, tmp, sizeof(SYMMODS));
				mods->last = tmp;
				mods->lastbr = 0;
				break;

			case RIGHTBRACE:							/* End of a string or sub/sup */
				if (mods->lastbr == 0 && (tmp=mods->last) != NULL) {
					mods = unwind(&sym, mods, 0);
				} else if (! unscript_old(mods)) {
					Draw(ich, &sym, mods);
				}
				break;
				
			case ASCHAT:								/* Special commands */
				if (next == '\0') {
					Draw(ich, &sym, mods);
					break;
				}
				aptr++;									/* Be gone with ^ character */
				switch (next) {						/* Really this character now */
					case LEFTBRACE:					/* ^{} or ^+ are superscript cmds */
						mods = superscript(mods);
						break;
					case SUPERSCRIPT:
						superscript_old(mods);
						break;
					case SUBSCRIPT:					/* ^_ is old subscript mode */
						subscript_old(mods);
						break;
					case UNSUPERSCRIPT:				/* ^+ or ^= are old unmode */
					case UNSUBSCRIPT:
						unscript_old(mods);
						break;
					case ASCHAT:						/* Old form of quoting ^ char */
						Draw(ich, &sym, mods);
						break;
					case BACKUP:
						Draw(DO_BACKSPACE, &sym, mods);
						break;
					case PENSELECT1:
					case PENSELECT2:					/* Next value is pen color */
						mods->mypen = *(aptr++)-'0';
						if (sym.DoDrawing) PlotSelectPen(mods->mypen);
						break;
					case FONTSELECT1:
					case FONTSELECT2:					/* Next value is font select */
						next = *(aptr++);				/* Next value is font index */
						if (isdigit(next)) MapFontSelection(next, &mods->family, &mods->cp, &mods->attrib);
						break;
					default:
						if (isdigit(next)) MapFontSelection(next, &mods->family, &mods->cp, &mods->attrib);
						break;
					}
				break;
			case BACKSLASH:									/* Backslash==>quoted char */
				aptr = DrawSpecial(aptr, &sym, mods);
				break;
			case UNDERSCORE:
				if (next == LEFTBRACE) {			/* _{} ==> start subscript */
					mods = subscript(mods);
					aptr++;
				} else {
					Draw(ich, &sym, mods);
				}
				break;
			case '?':									
			case '!':
				if (next == '`') {				/* Special code page request */
					Draw( (ich=='!') ? UPSIDEDOWN_EXCL : UPSIDEDOWN_QM, &sym, mods);
					aptr++;
				} else {
					Draw(ich, &sym, mods);
				}
				break;
			default:
				Draw(ich, &sym, mods);
		}
	}

/* Leave the plotter in position for more characters.  Reset parms */
/* Note -- TLEN reads internal value of XLEN to get the length -- be careful */
	if (sym.DoDrawing) {
		PlotMove3DInch(sym.xx[0], sym.yy[0], sym.zzstart, 3);		/* Pen up at final location */
		if (savlin != 1) PlotSetLineType(savlin,0.0f);
		PlotSelectPen(savcol);
	}

	PlotWindow->Save.On = saveon_sav;
	xlen = sym.xx[0];									/* Give it to XLEN for TLEN */

	while (mods != NULL) {mods=(tmp=mods)->last; free(tmp);}
	return;
}

/* ---------------------------------------------------------------------------
-- Lowest level routine to actually stroke the character after everything
-- is set up.  This routine may call the driver (Postscript) to do the
-- drawing instead if everything else appears okay-dokay and symbol is a
-- relatively normal ascii character.
--
-- Usage: void Draw (INTEGER ich, SYMLOC *sym, SYMMODS *mods);
--
-- Inputs: ich  - symbol to draw. May also be one of special commands such
--                as DO_BACKSPACE.
--         sym  - SYMLOC structure giving position of the character
--         mods - characteristics of the symbol (map, etc.)
--
-- Output: sym  - modified position on page (for next marker)
--         output to driver for actual strokes if necessary
--
-- Returns: void
--------------------------------------------------------------------------- */
/* FontMetric for Times New Roman font (100=height, scaled to 150): */
typedef int METRIC[256];

typedef struct _FONTINFO {
	char name[VARNAME_STR_SIZE];
	int hasfont[4];
	double scaling;
	int font[4];
	METRIC metric[4];
} FONTINFO;

/* FontMetric for Times New Roman font */
static FONTINFO TmsRmn = {
	"Times New Roman", {TRUE, TRUE, TRUE, TRUE}, 1.50,
	{DSP_FAMILY_TMSRM | DSP_CP_ASCII | DSP_ATTRIB_NORMAL,
	 DSP_FAMILY_TMSRM | DSP_CP_ASCII | DSP_ATTRIB_ITAL, 
	 DSP_FAMILY_TMSRM | DSP_CP_ASCII | DSP_ATTRIB_BOLD,
	 DSP_FAMILY_TMSRM | DSP_CP_ASCII | DSP_ATTRIB_ITAL | DSP_ATTRIB_BOLD},
	{
		{25,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,54,45,50,60,60,60,60,99,99,56,99,60,60,
			25,33,41,50,50,83,78,18,33,33,50,56,25,33,25,28,
			50,50,50,50,50,50,50,50,50,50,28,28,56,56,56,44,
			92,72,67,67,72,61,56,72,72,33,39,72,61,89,72,72,
			56,72,67,56,61,72,72,94,72,72,61,33,28,33,47,50,
			33,44,50,44,50,44,33,50,50,28,28,50,28,78,50,50,
			50,50,33,39,28,50,50,72,50,50,44,48,20,48,54,60,
			67,50,44,44,44,44,44,44,44,44,44,28,28,28,72,72,
			61,67,89,50,50,50,50,50,50,72,72,50,50,72,56,50,
			44,28,50,50,50,72,28,31,44,76,56,75,75,33,50,50,
			60,60,60,60,60,72,72,72,76,60,60,60,60,50,50,60,
			60,60,60,60,60,60,44,72,60,60,60,60,60,60,60,50,
			50,72,61,61,61,28,33,33,33,60,60,60,60,20,33,60,
			72,50,72,72,50,72,50,50,56,72,72,72,50,72,50,33,
			33,56,50,75,45,50,56,33,40,33,25,30,30,30,60,25
		},
		{25,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,54,52,50,60,60,60,60,99,99,67,99,60,60,
			25,33,42,50,50,83,78,21,33,33,50,67,25,33,25,28,
			50,50,50,50,50,50,50,50,50,50,33,33,67,67,67,50,
			92,61,61,67,72,61,61,72,72,33,44,67,56,83,67,72,
			61,72,61,50,56,72,61,83,61,56,56,39,28,39,42,50,
			33,50,50,44,50,44,28,50,50,28,28,44,28,72,50,50,
			50,50,39,39,28,50,44,67,44,44,39,40,27,40,54,60,
			67,50,44,50,50,50,50,44,44,44,44,28,28,28,61,61,
			61,67,89,50,50,50,50,50,44,72,72,50,50,72,67,50,
			50,28,50,50,50,67,28,31,50,76,67,75,75,39,50,50,
			60,60,60,60,60,61,61,61,76,60,60,60,60,50,50,60,
			60,60,60,60,60,60,50,61,60,60,60,60,60,60,60,50,
			50,72,61,61,61,28,33,33,33,60,60,60,60,27,33,60,
			72,50,72,72,50,72,50,50,61,72,72,72,44,56,50,33,
			33,67,50,75,52,50,67,33,40,33,25,30,30,30,60,25
		},
		{25,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,58,54,50,60,60,60,60,99,99,57,99,60,60,
			25,33,55,50,50,100,83,28,33,33,50,57,25,33,25,28,
			50,50,50,50,50,50,50,50,50,50,33,33,57,57,57,50,
			93,72,67,72,72,67,61,78,78,39,50,78,67,94,72,78,
			61,78,72,56,67,72,72,100,72,72,67,33,28,33,58,50,
			33,50,56,44,56,44,33,50,56,28,33,56,28,83,56,50,
			56,56,44,39,33,56,50,72,50,50,44,39,22,39,52,60,
			72,56,44,50,50,50,50,44,44,44,44,28,28,28,72,72,
			67,72,100,50,50,50,56,56,50,78,72,50,50,78,57,50,
			50,28,50,56,56,72,30,33,50,75,57,75,75,33,50,50,
			60,60,60,60,60,72,72,72,75,60,60,60,60,50,50,60,
			60,60,60,60,60,60,50,72,60,60,60,60,60,60,60,50,
			50,72,67,67,67,28,39,39,39,60,60,60,60,22,39,60,
			78,56,78,78,50,78,56,56,61,72,72,72,50,72,50,33,
			33,57,50,75,54,50,57,33,40,33,25,30,30,30,60,25
		},
		{25,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,58,50,50,60,60,60,60,99,99,61,99,60,60,
			25,39,55,50,50,83,78,28,33,33,50,57,25,33,25,28,
			50,50,50,50,50,50,50,50,50,50,33,33,57,57,57,50,
			83,67,67,67,72,67,67,72,78,39,50,67,61,89,72,72,
			61,72,67,56,61,72,67,89,67,61,61,33,28,33,57,50,
			33,50,50,44,50,44,33,50,56,28,28,50,28,78,56,50,
			50,50,39,39,28,56,44,67,50,44,39,35,22,35,57,60,
			67,56,44,50,50,50,50,44,44,44,44,28,28,28,67,67,
			67,72,94,50,50,50,56,56,44,72,72,50,50,72,57,50,
			50,28,50,56,56,72,27,30,50,75,61,75,75,39,50,50,
			60,60,60,60,60,67,67,67,75,60,60,60,60,50,50,60,
			60,60,60,60,60,60,50,67,60,60,60,60,60,60,60,50,
			50,72,67,67,67,28,39,39,39,60,60,60,60,22,39,60,
			72,50,72,72,50,72,58,50,61,72,72,72,44,61,50,33,
			33,57,50,75,50,50,57,33,40,33,25,30,30,30,60,25
		}
	}
};

/* FontMetric for Helvetica font */
static FONTINFO Helv = {
	"Helvetica", {TRUE, TRUE, TRUE, TRUE}, 1.40,
	{DSP_FAMILY_HELV | DSP_CP_ASCII | DSP_ATTRIB_NORMAL,
	 DSP_FAMILY_HELV | DSP_CP_ASCII | DSP_ATTRIB_ITAL, 
	 DSP_FAMILY_HELV | DSP_CP_ASCII | DSP_ATTRIB_BOLD,
	 DSP_FAMILY_HELV | DSP_CP_ASCII | DSP_ATTRIB_ITAL | DSP_ATTRIB_BOLD},
	{
		{28,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,56,54,56,60,60,60,60,99,99,58,99,60,60,
			28,28,35,56,56,89,67,19,33,33,39,58,28,33,28,28,
			56,56,56,56,56,56,56,56,56,56,28,28,58,58,58,56,
			101,67,67,72,72,67,61,78,72,28,50,67,56,83,72,78,
			67,78,72,67,61,72,67,94,67,67,61,28,28,28,47,56,
			33,56,56,50,56,56,28,56,56,22,22,50,22,83,56,56,
			56,56,33,50,28,56,50,72,50,50,50,33,26,33,58,60,
			72,56,56,56,56,56,56,50,56,56,56,28,28,28,67,67,
			67,89,100,56,56,56,56,56,50,78,72,61,56,78,58,56,
			56,28,56,56,56,72,37,36,61,74,58,83,83,33,56,56,
			60,60,60,60,60,67,67,67,74,60,60,60,60,56,56,60,
			60,60,60,60,60,60,56,67,60,60,60,60,60,60,60,56,
			56,72,67,67,67,28,28,28,28,60,60,60,60,26,28,60,
			78,61,78,78,56,78,56,56,67,72,72,72,50,67,56,33,
			33,58,56,83,54,56,58,33,40,33,28,33,33,33,60,28
		},
		{28,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,55,54,56,60,60,60,60,99,99,58,99,60,60,
			28,28,35,56,56,89,67,19,33,33,39,58,28,33,28,28,
			56,56,56,56,56,56,56,56,56,56,28,28,58,58,58,56,
			101,67,67,72,72,67,61,78,72,28,50,67,56,83,72,78,
			67,78,72,67,61,72,67,94,67,67,61,28,28,28,47,56,
			33,56,56,50,56,56,28,56,56,22,22,50,22,83,56,56,
			56,56,33,50,28,56,50,72,50,50,50,33,26,33,58,60,
			72,56,56,56,56,56,56,50,56,56,56,28,28,28,67,67,
			67,89,100,56,56,56,56,56,50,78,72,61,56,78,58,56,
			56,28,56,56,56,72,37,36,61,74,58,83,83,33,56,56,
			60,60,60,60,60,67,67,67,74,60,60,60,60,56,56,60,
			60,60,60,60,60,60,56,67,60,60,60,60,60,60,60,56,
			56,72,67,67,67,28,28,28,28,60,60,60,60,26,28,60,
			78,61,78,78,56,78,56,56,67,72,72,72,50,67,56,33,
			33,58,56,83,54,56,58,33,40,33,28,33,33,33,60,28
		},
		{28,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,67,56,56,60,60,60,60,99,99,58,99,60,60,
			28,33,47,56,56,89,72,24,33,33,39,58,28,33,28,28,
			56,56,56,56,56,56,56,56,56,56,33,33,58,58,58,61,
			97,72,72,72,72,67,61,78,72,28,56,72,61,83,72,78,
			67,78,72,67,61,72,67,94,67,67,61,33,28,33,58,56,
			33,56,61,56,61,56,33,61,61,28,28,56,28,89,61,61,
			61,61,39,56,33,61,56,78,56,56,50,39,28,39,58,60,
			72,61,56,56,56,56,56,56,56,56,56,28,28,28,72,72,
			67,89,100,61,61,61,61,61,56,78,72,61,56,78,58,56,
			56,28,61,61,61,72,37,36,61,74,58,83,83,33,56,56,
			60,60,60,60,60,72,72,72,74,60,60,60,60,56,56,60,
			60,60,60,60,60,60,56,72,60,60,60,60,60,60,60,56,
			61,72,67,67,67,28,28,28,28,60,60,60,60,28,28,60,
			78,61,78,78,61,78,61,61,67,72,72,72,56,67,56,33,
			33,58,56,83,56,56,58,33,40,33,28,33,33,33,60,28
		},
		{28,60,60,60,60,60,60,35,60,60,60,60,60,60,60,60,
			60,60,60,67,56,56,60,60,60,60,99,99,58,99,60,60,
			28,33,47,56,56,89,72,24,33,33,39,58,28,33,28,28,
			56,56,56,56,56,56,56,56,56,56,33,33,58,58,58,61,
			97,72,72,72,72,67,61,78,72,28,56,72,61,83,72,78,
			67,78,72,67,61,72,67,94,67,67,61,33,28,33,58,56,
			33,56,61,56,61,56,33,61,61,28,28,56,28,89,61,61,
			61,61,39,56,33,61,56,78,56,56,50,39,28,39,58,60,
			72,61,56,56,56,56,56,56,56,56,56,28,28,28,72,72,
			67,89,100,61,61,61,61,61,56,78,72,61,56,78,58,56,
			56,28,61,61,61,72,37,36,61,74,58,83,83,33,56,56,
			60,60,60,60,60,72,72,72,74,60,60,60,60,56,56,60,
			60,60,60,60,60,60,56,72,60,60,60,60,60,60,60,56,
			61,72,67,67,67,28,28,28,28,60,60,60,60,28,28,60,
			78,61,78,78,61,78,61,61,67,72,72,72,56,67,56,33,
			33,58,56,83,56,56,58,33,40,33,28,33,33,33,60,28
		}
	}
};
	
/* FontMetric for Times New Roman font */
static FONTINFO Greek = {
	"Symbol Set", {TRUE, FALSE, FALSE, FALSE}, 1.50,
	{DSP_FAMILY_TMSRM | DSP_CP_MATH | DSP_ATTRIB_NORMAL, 0, 0, 0},
	{
		{	25,60,60,60,60,60,60,25,60,60,60,60,60,60,60,60,
			60,60,60,60,25,25,60,60,60,60,60,60,60,60,60,60,
			25,33,71,50,55,83,78,44,33,33,50,55,25,55,25,28,
			50,50,50,50,50,50,50,50,50,50,28,28,55,55,55,44,
			55,72,67,72,61,61,76,60,72,33,63,72,69,89,72,72,
			77,74,56,59,61,69,44,77,64,79,61,33,86,33,66,50,
			50,63,55,55,49,44,52,41,60,33,60,55,55,58,52,55,
			55,52,55,60,44,58,71,69,49,69,49,48,20,48,55,60,
			25,25,25,25,25,25,25,25,25,25,25,25,25,25,258,25,
			25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
			25,62,25,55,17,71,50,75,75,75,75,104,99,60,99,60,
			40,55,41,55,55,71,49,46,55,55,55,55,100,60,100,66,
			82,69,79,99,77,77,82,77,77,71,71,71,71,71,71,71,
			77,71,79,79,89,82,55,25,71,60,60,104,99,60,99,60,
			49,33,79,79,79,71,38,38,38,38,38,38,49,49,49,49,
			55,33,27,69,69,69,38,38,38,38,38,38,49,49,49,33
		}
	}
};


	
/* Following Hershey definitions are measured in dots */
#define	SPACE_AMOUNT			0.9523f	/* Fraction of character full height (20/21) */
#define	SPACE_PS_AMOUNT		0.42f		/* Fraction for Postscript fonts */
#define	THINSPACE_AMOUNT		0.476f	/* Fraction of character full height (10/21) */
#define	THINSPACE_PS_AMOUNT	0.21f		/* Fraction for Postscript fonts */
#define	CHARACTER_HEIGHT		21.0f		/* Character height in dots			*/
#define	Y_CENTER_VALUE			21			/* Character center y coordinate		*/
#define	Y_DESCEND_VALUE		12			/* Y extent of character descenders	*/

PRIVATE void Draw(int ich, SYMLOC *sym, SYMMODS *mods) {

	int	family,cp,attrib;						/* Attributes of selected font */
	REAL	c1,s1;									/* Scaled SIN/COS values */
	REAL	xtmp, metric, concat, hershey_metric;
	FONTINFO *psfont;
	int   i, fontindex;							/* Which font index in family */

/* Hershey parameters */
	int nmap,										/* Hershey font # corresponding		*/
		use_hershey,								/* Are the hershey to be used?		*/
		jptr,											/* Jot pointer for stroke generator */
		jot,											/* Actual stroke storage				*/
		ipen,											/* Pen mode for stroke (up or down) */
		icx,icy,										/*  Dot pen coordinate					*/
		ixoff,iyoff,iyscrp;						/* Offsets for character center		*/

   if (ich == DO_BACKSPACE) {					/* Simple backup command */
		for (i=0; i<NOLDS-1; i++) {
			sym->xx[i] = sym->xx[i+1];
			sym->yy[i] = sym->yy[i+1];
		}
		return;
	}

	c1 = sym->costh*mods->height;					/* Scaled SIN/COS		*/
	s1 = sym->sinth*mods->height;
	for (i=NOLDS-1; i; i--) {						/* Push position for backup */
		sym->xx[i] = sym->xx[i-1];
		sym->yy[i] = sym->yy[i-1];
	}

	family = mods->family;							/* Local copies for mods */
	cp     = mods->cp;
	attrib = mods->attrib;

/* Deal with special cases */
	if (ich == SPACE && cp != DSP_CP_SYMBOL) {	/* SYMBOL codepage has a character at space (box) */
		if (family == DSP_FAMILY_HELV || family == DSP_FAMILY_TMSRM) {
			sym->xx[0] += SPACE_PS_AMOUNT * c1 * mods->hscale;
			sym->yy[0] += SPACE_PS_AMOUNT * s1 * mods->hscale;
		} else {
			sym->xx[0] += SPACE_AMOUNT * c1 * mods->hscale;
			sym->yy[0] += SPACE_AMOUNT * s1 * mods->hscale;
		}
		return;
	} else if (ich == DO_THINSPACE) {
		if (family == DSP_FAMILY_HELV || family == DSP_FAMILY_TMSRM) {
			sym->xx[0] += THINSPACE_PS_AMOUNT * c1 * mods->hscale;
			sym->yy[0] += THINSPACE_PS_AMOUNT * s1 * mods->hscale;
		} else {
			sym->xx[0] += THINSPACE_AMOUNT * c1 * mods->hscale;
			sym->yy[0] += THINSPACE_AMOUNT * s1 * mods->hscale;
		}
		return;
	} else if (ich > 0x100) {						/* Upper now mapped -> lower	*/
		ich &= 0xFF;									/* Just strip upper bits		*/
	} else if (ich < 0x10) {						/* Define first 16 as symbols	*/
		family = DSP_FAMILY_HERSHEY;				/* Always do as HERSHEY			*/
		cp     = DSP_CP_SYMBOL;						/* And with SYMBOL chrset		*/
		attrib = DSP_ATTRIB_NORMAL;				/* And no choice here either	*/
		ich = ich + SPACE;							/* And shift to char in set	*/
	}

/* ------------------------------------------------------------------------- 
 ...  ChrStroke(jptr)   = isize    (Width of character in dots)
 ...  ChrStroke(jptr+1) = strokes  63 => Pen up, 4095 => done
   ------------------------------------------------------------------------- */
	if (mods->textsym)											 family = DSP_FAMILY_HERSHEY;
	if (cp == DSP_CP_ODDGREEK || cp == DSP_CP_SYMBOL)   family = DSP_FAMILY_HERSHEY;
	nmap = SelectHersheyMap(family, cp, attrib);
	jptr = ChrIndex[NCHARS*nmap+ich-INDEX_OFFSET];	/* Index into stroke array */
	use_hershey = (family != DSP_FAMILY_HELV) && (family != DSP_FAMILY_TMSRM);
	if (jptr == 0 && use_hershey) return;				/* Nothing there - ignore	*/

/* ... Calculate concat length and width (metric) - dealing with centered */
/* ... jptr points to first stroke, so metric info is back one */
	if (jptr != 0) {
		jot = ChrStroke[jptr-1] & 0x3F;
		hershey_metric = jot / CHARACTER_HEIGHT;	/* Relative width of hershey	*/
	} else {
		jot = 0;											/* Don't know what really should be here */
		hershey_metric = 1.0f;
	}

	psfont = NULL;										/* Not a PS font					*/
	fontindex = 0;										/* To avoid problems ever		*/
	if (mods->textsym) {								/* If text symbol					*/
		metric = hershey_metric;
		concat = 1.4f * metric;						/* Need extra space				*/
	} else if (ich<=ASCDOT && nmap==0) {		/* Centered characters			*/
		metric = hershey_metric;
		concat = 0;										/* No shift in position			*/
	} else if (use_hershey) {
		concat = metric = hershey_metric;
	} else {
		psfont = (family == DSP_FAMILY_TMSRM) ? &TmsRmn : &Helv;
		if (cp == DSP_CP_MATH) psfont = &Greek;
		fontindex = 0;
		if (attrib & DSP_ATTRIB_ITAL) fontindex += 1;
		if (attrib & DSP_ATTRIB_BOLD) fontindex += 2;
		while (! psfont->hasfont[fontindex] && fontindex != 0) fontindex--;
		concat = metric = (REAL) (psfont->scaling * psfont->metric[fontindex][ich] / 100.0);
	}
	concat *= mods->hscale;							/* We may reduce by choice */
	sym->xx[0] += concat*c1;
	sym->yy[0] += concat*s1;

/* ... Plot the characters only if we are not doing this just for length */
	if (sym->DoDrawing) {
		REAL xx,yy;
		/* Start position with potential shift by modification */
		xx = sym->xx[1] - c1*metric*mods->leftshift;
		yy = sym->yy[1] - s1*metric*mods->leftshift;
		
		if (psfont != NULL &&							/* Try driver first? */
         (! PlotWindow->mode_3d || DEVICE->Capabilities & DEV_CAP_3DFONTS) &&
			( (DEVICE->Capabilities & DEV_CAP_FONTS     && cp == DSP_CP_ASCII) ||
			  (DEVICE->Capabilities & DEV_CAP_GREEKFONT && cp == DSP_CP_MATH)) ) {
			DspDrawChar dr;
			REAL px,py;

			dr.size   = (int) (mods->height * PL_Plot.factr * PL_Plot.yscale);
			dr.size   = (int) (psfont->scaling * dr.size);
			dr.font   = psfont->font[fontindex];		/* Specific one */
			dr.chr    = ich;

			if (! PlotWindow->mode_3d) {
				PlotConvert2DScales(INCH_TO_PIXEL, 
										  xx-sym->sinth*mods->yscrpt,yy+sym->costh*mods->yscrpt, &px, &py);

				dr.x = (int) (px+0.5); dr.y = (int) (py+0.5);
				dr.angle  = ( (int) (sym->theta + PL_Plot.orient_angle + 0.5)) % 360;
				if ((*DEVICE->dsptch)(DRAWCHAR, DEVICE->DriverBlock, (DSP *) &dr))
					return;
			} else {
				REAL x00,y00,x01,y01,x10,y10;
				PlotConvert3DScales(INCH_TO_PIXEL, 
										  xx-sym->sinth*mods->yscrpt, yy+sym->costh*mods->yscrpt,sym->zzstart,
										  &px, &py, NULL);
				dr.x = (int) (px+0.5); dr.y = (int) (py+0.5);
         /* -------------------------------------------------------------------
         -- The transformation for character skew will be handled by giving
         -- the driver a CTM transformation for the draw motions (ala
			-- Postscript).  The matrix elements are defined via
			--  <x> =  ([0] [1] 0 )  (x)
			--  <y> =  ([2] [3] 0 )  (y)
			--  <1> =  ( 0   0  1 )  (1)
         -- The elements are found by taking 1 inch square and distorting it
			-- to the pixel positions.  This handles all rotation except specific
			-- request for additional angle.
         -- Since the character shift is handled before the transform, only
			-- the four elements are needed (t1 and t2 are zero).
         ------------------------------------------------------------------ */
				dr.angle  = ( (int) (sym->theta + 0.5)) % 360;
				PlotConvert3DScales(INCH_TO_PIXEL, 0.0, 0.0, sym->zzstart, &x00, &y00, NULL);
				PlotConvert3DScales(INCH_TO_PIXEL, 1.0, 0.0, sym->zzstart, &x01, &y01, NULL);
				PlotConvert3DScales(INCH_TO_PIXEL, 0.0, 1.0, sym->zzstart, &x10, &y10, NULL);
				dr.matrix[0] = (x01-x00) / PL_Plot.xscale / PL_Plot.factr;
				dr.matrix[1] = (y01-y00) / PL_Plot.yscale / PL_Plot.factr;
				dr.matrix[2] = (x10-x00) / PL_Plot.xscale / PL_Plot.factr;
				dr.matrix[3] = (y10-y00) / PL_Plot.yscale / PL_Plot.factr;
				if ((*DEVICE->dsptch)(DRAW3DCHAR, DEVICE->DriverBlock, (DSP *) &dr))
					return;
			}
		}

		if (jptr == 0) return;							/* Nothing there - ignore	*/
		if (mods->textsym) {								/* If text symbol					*/
			ixoff = -jot/5;								/* 10% X offset					*/
			iyoff = Y_DESCEND_VALUE;					/* Worry about descending chr	*/
		} else if (ich<=ASCDOT && nmap==0) {		/* Centered characters			*/
			ixoff = jot / 2;								/* Offset X so it goes			*/
			iyoff = Y_CENTER_VALUE;						/* Y default, final unmoved	*/
		} else {
			ixoff = 0;										/* No X offset						*/
			iyoff = Y_DESCEND_VALUE;					/* Worry about descending chr	*/
		}

		iyscrp = (int) ((mods->yscrpt/mods->height)*CHARACTER_HEIGHT+0.5);	/* And pixel subscript */
		ixoff = ixoff + ChrStroke[jptr-1]/64;		/* For script chars		*/
		iyoff = iyscrp - iyoff;							/* Script pixel move		*/
		ipen  = 3;											/* Pen up first			*/
		xtmp  = metric / hershey_metric;				/* X-scaling correction */
		while ( (jot = ChrStroke[jptr++]) != 4095) {
			if (jot == 63)	{								/* Pen up command */
				ipen = 3;
			} else {
				icx = (jot      & 0x3F) - ixoff;		/* Incremental X */
				icy = ((jot>>6) & 0X3F) + iyoff;		/* Incremental Y */
				icx = (icx>0) ? (int) (icx*xtmp+0.5) : (int) (icx*xtmp-0.5); /* Modified width */
				PlotMove3DInch( xx+(icx*c1-icy*s1)/CHARACTER_HEIGHT, 
								    yy+(icx*s1+icy*c1)/CHARACTER_HEIGHT,
								    sym->zzstart, ipen);
				ipen = 2;										/* Pen down hereout */
			}
		}
	}

	return;
}


/* ---------------------------------------------------------------------------
   Routine to initialize subsequent characters for superscripts
--------------------------------------------------------------------------- */
PRIVATE SYMMODS *superscript(SYMMODS *tmp) {
	SYMMODS *mods;
	mods = malloc(sizeof(SYMMODS));
	memcpy(mods, tmp, sizeof(SYMMODS));
	mods->last = tmp;
	mods->yscrpt = mods->yscrpt + mods->height * PL_Symbols.SuperscriptOffset;
	mods->height =                mods->height * PL_Symbols.ScriptSize;
	mods->lastbr = 0;
	return(mods);
}

/* ---------------------------------------------------------------------------
   Routine to initialize subsequent characters for subscripts
--------------------------------------------------------------------------- */
PRIVATE SYMMODS *subscript(SYMMODS *tmp) {
	SYMMODS *mods;
	mods = malloc(sizeof(SYMMODS));
	memcpy(mods, tmp, sizeof(SYMMODS));
	mods->last = tmp;
	mods->yscrpt = mods->yscrpt - mods->height * PL_Symbols.SubscriptOffset;
	mods->height =                mods->height * PL_Symbols.ScriptSize;
	mods->lastbr = 0;
	return(mods);
}


/* ---------------------------------------------------------------------------
   Routine to initialize subsequent characters for subscripts
--------------------------------------------------------------------------- */
PRIVATE void subscript_old(SYMMODS *mods) {
	mods->yscrpt = mods->yscrpt - mods->height * PL_Symbols.SubscriptOffset;
	mods->height =                mods->height * PL_Symbols.ScriptSize;
	mods->lastbr = 3*mods->lastbr+2;
	return;
}


/* ---------------------------------------------------------------------------
   Routine to initialize subsequent characters for superscripts
--------------------------------------------------------------------------- */
PRIVATE void superscript_old(SYMMODS *mods) {
	mods->yscrpt = mods->yscrpt + mods->height * PL_Symbols.SuperscriptOffset;
	mods->height =                mods->height * PL_Symbols.ScriptSize;
	mods->lastbr = 3*mods->lastbr+1;
	return;
}


/* ---------------------------------------------------------------------------
   Routine to remove last executed superscript or subscript
--------------------------------------------------------------------------- */
PRIVATE LOGICAL unscript_old(SYMMODS *mods) {
	INTEGER i;

	if ( (i = mods->lastbr % 3) == 0) return(FALSE);
	mods->lastbr = mods->lastbr/3;

	mods->height = mods->height / PL_Symbols.ScriptSize;
	if (i == 1)											/* Unsuperscript */
		mods->yscrpt = mods->yscrpt - mods->height * PL_Symbols.SuperscriptOffset;
	else if (i == 2)									/* Unsubscript  */
		mods->yscrpt = mods->yscrpt + mods->height * PL_Symbols.SubscriptOffset;
	return(TRUE);
}


/* ============================================================================
--  Subroutine to determine the extent of a symbol in order to just abut it
--  in either X or Y.
--
--  Usage: CALL CHR$SIZE(ich,obuf)
--
--  Inputs: ich - character to check (< 14)
--
--  Output: obuf(1) - minimum X value (per inch of symsize)
--          obuf(2) - maximum X value
--          obuf(3) - minimum Y value
--          obuf(4) - maximum Y value
--
--  Note: The values returned should be multiplied by the symsize to get the
--        actual extent.  Any invalid character will return 0 for all elements.
c=========================================================================== */
void PlotQuerySymbolExtent(INTEGER ich, REAL obuf[]) {

	int family,cp,attrib,nmap;					/* Attributes of selected font */
	int jptr, jot, ipen, icx, icy, ixoff, iyoff;
	int icx2=0, icy2=0;							/* Init to avoid compiler warn */
	REAL	xmin=0.0f,
		   xmax=0.0f,
			ymin=0.0f,
			ymax=0.0f;
	REAL	x;

	if (abs(ich) > 14) {
/*		TTYprintf("ERROR: Requested extent of symbol outside [0,14]\n"); */
		goto BadSymbol;
	}

/* Map it correctly, assuming it is a symbol request */
	family = DSP_FAMILY_HERSHEY;				/* Always do as HERSHEY			*/
	cp     = DSP_CP_SYMBOL;						/* And with SYMBOL chrset		*/
	attrib = DSP_ATTRIB_NORMAL;				/* And no choice here either	*/
	ich    = abs(ich) + SPACE;					/* And shift to char in set	*/
	nmap   = SelectHersheyMap(family, cp, attrib);
	jptr   = ChrIndex[NCHARS*nmap+ich-INDEX_OFFSET];	/* Index into stroke array */

	if (jptr == 0) {
/*		TTYprintf("ERROR: No stroke data for symbol index specified\n"); */
		goto BadSymbol;
	}

/* ... Calculate offsets for centered characters */
	ixoff = (ChrStroke[--jptr] & 0x3F)/2;			/* Center in X */
	iyoff = Y_CENTER_VALUE;								/* Y is default */

	ipen  = 3;												/* Pen up first */
	while ( (jot = ChrStroke[++jptr]) != 4095) {
		if (jot == 63)										/* Pen up command */
			ipen = 3;
		else {
			icx = (jot      & 0x3F) - ixoff;			/* Incremental X */
			icy = ((jot>>6) & 0X3F) - iyoff;			/* Incremental Y */
			if (ipen == 2) {
				if (icy*icy2 <= 0) {						/* Crosses X axis */
					if (icy == icy2) {					/* Are both on the axis? */
						xmin = min(min(xmin, (REAL) icx), (REAL) icx2);
						xmax = max(max(xmax, (REAL) icx), (REAL) icx2);
					} else {
						x = icx2 + ( ((0.0f-icy2)/(icy-icy2)) * (icx-icx2) );
						xmin = min(xmin,x);
						xmax = max(xmax,x);
					}
				}
				if (icx*icx2 <= 0) {						/* Crosses Y axis */
					if (icx == icx2) {					/* Are both on the axis? */
						ymin = min(min(ymin, (REAL) icy), (REAL) icy2);
						ymax = max(max(ymax, (REAL) icy), (REAL) icy2);
					} else {
						x = icy2 + ( ((0.0f-icx2)/(icx-icx2)) * (icy-icy2) );
						ymin = min(ymin,x);
						ymax = max(ymax,x);
					}
				}
			}
			ipen = 2;										/* Pen down hereout */
			icx2 = icx;										/* Save last value */
			icy2 = icy;
		}
	}

BadSymbol:
   obuf[0] = xmin/CHARACTER_HEIGHT;			/* Per inch of character size */
	obuf[1] = xmax/CHARACTER_HEIGHT;
	obuf[2] = ymin/CHARACTER_HEIGHT;
	obuf[3] = ymax/CHARACTER_HEIGHT;
/*	TTYprintf("sym: %d values: %f %f %f %f\n", ich, obuf[0],obuf[1],obuf[2],obuf[3]); */
	return;
}


/* ---------------------------------------------------------------------------
-- Routine to interpret any \xxxx macro specifications in a string
--
-- Usage:  (char *) aptr = DrawSpecial(char *aptr, SYMLOC *sym);
--
-- Inputs: aptr - pointer to current position in string to be interpreted
--         sym  - current structure describing drawing characteristics
--
-- Returns: aptr - new position after interpretation of commands
----------------------------------------------------------------------------- */
typedef struct {
	char *name;
	int	sym;						/* Modified symbol to use	*/
	int	cp;						/* Command or codepage		*/
} SPECIALS;

typedef struct {					/* Structure for single char conversions		*/
	char	chr;
	int	sym;
} FOREIGN;

#define	SP_PLAIN			1
#define	SP_BOLD			2
#define	SP_ITAL			3
#define	SP_HELVETICA	4
#define	SP_TIMESROMAN	5
#define	SP_HERSHEY		6
#define	SP_ROMAN			7
#define	SP_GREEK			8
#define	SP_GOTHIC		9
#define	SP_SCRIPT		10
#define	SP_PEN			11
#define	SP_NEWLINE		12
#define	SP_CR				13
#define	SP_BACKSPACE	14
#define	SP_DEGREE		15			/* Degree symbol (define as ^{o}) */

PRIVATE FOREIGN quoted[] = {
	{'a',	132},	{'A',	142},			/* Convert to upper codepage 850 */
	{'e',	137},	{'E',	211},
	{'i',	139},	{'I',	216},
	{'o',	148},	{'O',	153},
	{'u',	129},	{'U',	154},
	{'y',	152},
	{'\0',0} };

PRIVATE FOREIGN squoted[] = {
	{'a',	160},	{'A',	181},			/* Convert to upper codepage 850 */
	{'e',	130},	{'E',	144},
	{'i',	161},	{'I',	214},
	{'o',	162},	{'O',	224},
	{'u',	163},	{'U',	233},
	{'y',	236},	{'Y',	237},
	{'\0',0} };

PRIVATE FOREIGN hatted[] = {
	{'a',	131},	{'A',	182},			/* Convert to upper codepage 850 */
	{'e',	136},	{'E',	210},
	{'i',	140},	{'I',	215},
	{'o',	147},	{'O',	226},
	{'u',	150},	{'U',	234},
	{'\0',0} };

PRIVATE FOREIGN bquoted[] = {
	{'a',	133},	{'A',	183},			/* Convert to upper codepage 850 */
	{'e',	138},	{'E',	212},
	{'i',	141},	{'I',	222},
	{'o',	149},	{'O',	227},
	{'u',	151},	{'U',	235},
	{'\0',0} };

PRIVATE FOREIGN tilded[] = {
	{'a',	198},	{'A',	199},			/* Convert to upper codepage 850 */
	{'o',	228},	{'O',	229},
	{'n',	164},	{'N',	165},
	{'\0',0} };

PRIVATE SPECIALS speclist[] = {
	{"rm",		0, SP_PLAIN},
	{"bf",		0,	SP_BOLD},
	{"it",		0,	SP_ITAL},

	{"norm",		0, SP_PLAIN},
	{"bold",		0, SP_BOLD},		{"ital",		0,	SP_ITAL},

	{"helv",		0, SP_HELVETICA},
	{"tmsrm",	0, SP_TIMESROMAN},
	{"hershey",	0, SP_HERSHEY},
	{"gothic",	0,	SP_GOTHIC},
	{"script",	0, SP_SCRIPT},

	{"roman",	0,	SP_ROMAN},
	{"greek",	0,	SP_GREEK},

	{"pen",		0, SP_PEN},			{"color",	0, SP_PEN},
	{"n",			0, SP_NEWLINE},	{"r",			0, SP_CR},
	{"b",			0, SP_BACKSPACE},

	{"deg",		0,	SP_DEGREE},
	
/* Upper code page symbols - See Wiki on Code Page 850 */
	{"ss",			0xE1,	0},												/* 225 */
	{"aa",			0x86,	0},		{"AA",		0x8F,	0},			/* 134 & 143 */
	{"ae",			0x91,  0},		{"AE",		0x92,	0},			/* 145 & 146 */
	{"o",				0x9B,	0},		{"O",			0x9D,	0},			/* 155 & 157 */
	{"thorn",		0xe7,	0},		{"Thorn",	0xe8, 0},			/* 231 & 232 */
	{"eth",			0xd0, 0},		{"Eth",		0xd1,	0},			/* 207 & 208 */
	{"micro",		0xe6, 0},												/* 230 */
/*	{"registered",	0xa9, 0}, */
/*	{"copyright",	0xb8,	0}, */
	{"paragraph",	0xf4,	0},
	{"section",		0xf5,	0},
	{"plusminus",	0xf1, 0},
	{"divide",		0xf6,	0},
	{"multiply",	0x9e,	0},
/*	{"degree",		0xf8,	0}, */
	{"yen",			0xbe,	0},
	{"currency",	0xcf,	0},
	{"sterling",	0x9c,	0},
	{"cent",			0xbd,	0},
	{"florin",		0x9f,	0},

	{"alpha",	'a', DSP_CP_MATH},		{"Alpha",	'A',	DSP_CP_MATH},
	{"beta",		'b', DSP_CP_MATH},		{"Beta",		'B',	DSP_CP_MATH},
	{"chi",		'c', DSP_CP_MATH},		{"Chi",		'C',	DSP_CP_MATH},
	{"delta",	'd', DSP_CP_MATH},		{"Delta",	'D',	DSP_CP_MATH},
	{"epsilon",	'e', DSP_CP_MATH},		{"Epsilon",	'E',	DSP_CP_MATH},
	{"phi",		'f', DSP_CP_MATH},		{"Phi",		'F',	DSP_CP_MATH},
	{"gamma",	'g', DSP_CP_MATH},		{"Gamma",	'G',	DSP_CP_MATH},
	{"eta",		'h', DSP_CP_MATH},		{"Eta",		'H',	DSP_CP_MATH},
	{"iota",		'i', DSP_CP_MATH},		{"Iota",		'I',	DSP_CP_MATH},
	{"kappa",	'k', DSP_CP_MATH},		{"Kappa",	'K',	DSP_CP_MATH},
	{"lambda",	'l', DSP_CP_MATH},		{"Lambda",	'L',	DSP_CP_MATH},
	{"mu",		'm', DSP_CP_MATH},		{"Mu",		'M',	DSP_CP_MATH},
	{"nu",		'n', DSP_CP_MATH},		{"Nu",		'N',	DSP_CP_MATH},
	{"omicron",	'o', DSP_CP_MATH},		{"Omicron",	'O',	DSP_CP_MATH},
	{"pi",		'p', DSP_CP_MATH},		{"Pi",		'P',	DSP_CP_MATH},
	{"theta",	'q', DSP_CP_MATH},		{"Theta",	'Q',	DSP_CP_MATH},
	{"rho",		'r', DSP_CP_MATH},		{"Rho",		'R',	DSP_CP_MATH},
	{"sigma",	's', DSP_CP_MATH},		{"Sigma",	'S',	DSP_CP_MATH},
	{"tau",		't', DSP_CP_MATH},		{"Tau",		'T',	DSP_CP_MATH},
	{"upsilon",	'u', DSP_CP_MATH},		{"Upsilon",	0xA1,	DSP_CP_MATH},
	{"omega",	'w', DSP_CP_MATH},		{"Omega",	'W',	DSP_CP_MATH},
	{"xi",		'x', DSP_CP_MATH},		{"Xi",		'X',	DSP_CP_MATH},
	{"psi",		'y', DSP_CP_MATH},		{"Psi",		'Y',	DSP_CP_MATH},
	{"zeta",		'z', DSP_CP_MATH},		{"Zeta",		'Z',	DSP_CP_MATH},

	{"Ohm",			'W',	DSP_CP_MATH},	/* Ohm to Omega */
	{"ohm",			'W',	DSP_CP_MATH},	/* Ohm to Omega */
	{"Mho",			'S',	0},				/* Siemans */
	{"mho",			'S',	0},				/* Siemans */

	{"varepsilon",	'V',	DSP_CP_MATH},
	{"vartheta",	'J',	DSP_CP_MATH},
	{"varphi",		'j',	DSP_CP_MATH},
	{"varUpsilon",	'U',	DSP_CP_MATH},

	{"minus",	'-',	DSP_CP_MATH},		/* Was '/' in symbols (okay) */
	{"plus",		'+',	DSP_CP_MATH},		/* Was '0' in symbols (okay) */
	{"eq",		'=',	DSP_CP_MATH},		/* Was '6' in symbols (okay) */
	{"lt",		'<',	DSP_CP_MATH},		/* Was '9' in symbols (2241) replace */ 
	{"gt",		'>',	DSP_CP_MATH},		/* Was ':' in symbols (2242) replace */


/* See ASCII Font tables for Symbol */
	{"forall",			0x22, DSP_CP_MATH},
	{"exists",			0x24,	DSP_CP_MATH},
	{"owns",				0x27,	DSP_CP_MATH},
	{"therefore",		0x5C,	DSP_CP_MATH},
	{"perp",				0x5E, DSP_CP_MATH},		
	{"sim",				0x7E,	DSP_CP_MATH},
	{"le",				0xA3,	DSP_CP_MATH},
	{"infty",			0xA5,	DSP_CP_MATH},
	{"clubsuit",		0xA7,	DSP_CP_MATH},
	{"diamondsuit",	0xA8,	DSP_CP_MATH},
	{"heartsuit",		0xA9,	DSP_CP_MATH},
	{"spadesuit",		0xAA,	DSP_CP_MATH},
	{"leftrightarrow",0xAB,	DSP_CP_MATH},
	{"leftarrow",		0xAC,	DSP_CP_MATH},
	{"uparrow",			0xAD,	DSP_CP_MATH},
	{"rightarrow",		0xAE,	DSP_CP_MATH},
	{"downarrow",		0xAF,	DSP_CP_MATH},
	{"degree",			0xB0,	DSP_CP_MATH},
	{"pm",				0xB1,	DSP_CP_MATH},
	{"ge",				0xB3,	DSP_CP_MATH},
	{"times",			0xB4,	DSP_CP_MATH},
	{"propto",			0xB5,	DSP_CP_MATH},
	{"partial",			0xB6,	DSP_CP_MATH},
	{"bullet",			0xB7,	DSP_CP_MATH},
	{"div",				0xB8,	DSP_CP_MATH},
	{"ne",				0xB9,	DSP_CP_MATH},
	{"equiv",			0xBA,	DSP_CP_MATH},
	{"approx",			0xBB,	DSP_CP_MATH},
	{"dots",				0xBC,	DSP_CP_MATH},
	{"aleph",			0xC0,	DSP_CP_MATH},
	{"Re",				0xC2,	DSP_CP_MATH},
	{"Im",				0xC1,	DSP_CP_MATH},
	{"wp",				0xC2,	DSP_CP_MATH},
	{"oplus",			0xC5,	DSP_CP_MATH},
	{"otimes",			0xC4,	DSP_CP_MATH},
	{"oslash",			0xC6,	DSP_CP_MATH},
	{"cap",				0xC7,	DSP_CP_MATH},
	{"cup",				0xC8,	DSP_CP_MATH},
	{"supset",			0xC9,	DSP_CP_MATH},
	{"supseteq",		0xCA,	DSP_CP_MATH},
	{"subset",			0xCC,	DSP_CP_MATH},
	{"subseteq",		0xCD,	DSP_CP_MATH},
	{"in",				0xCE,	DSP_CP_MATH},
	{"nin",				0xCF,	DSP_CP_MATH},
	{"angle",			0xD0,	DSP_CP_MATH},
	{"nabla",			0xD1,	DSP_CP_MATH},
	{"grad",				0xD1,	DSP_CP_MATH},
	{"Registered",		0xD2,	DSP_CP_MATH},
	{"Copyright",		0xD3,	DSP_CP_MATH},
	{"Trademark",		0xD4,	DSP_CP_MATH},
	{"prod",				0xD5,	DSP_CP_MATH},
	{"sqrt",				0xD6,	DSP_CP_MATH},
	{"surd",				0xD6,	DSP_CP_MATH},
	{"cdot",				0xD7,	DSP_CP_MATH},		/* Was '4' in symbols (2236) */
	{"neg",				0xD8,	DSP_CP_MATH},
	{"wedge",			0xD9,	DSP_CP_MATH},
	{"vee",				0xDA,	DSP_CP_MATH},
	{"Leftrightarrow",0xDB,	DSP_CP_MATH},
	{"Leftarrow",		0xDC,	DSP_CP_MATH},
	{"Uparrow",			0xDD,	DSP_CP_MATH},
	{"Rightarrow",		0xDE,	DSP_CP_MATH},
	{"Downarrow",		0xDF,	DSP_CP_MATH},
	{"Diamond",			0xE0,	DSP_CP_MATH},
	{"langle",			0xE1,	DSP_CP_MATH},
	{"registered",		0xE2,	DSP_CP_MATH},
	{"copyright",		0xE3,	DSP_CP_MATH},
	{"trademark",		0xE4,	DSP_CP_MATH},
	{"sum",				0xE5,	DSP_CP_MATH},
	{"lceil",			0xE9,	DSP_CP_MATH},
	{"lfloor",			0xEB,	DSP_CP_MATH},
	{"rangle",			0xF1,	DSP_CP_MATH},
	{"int",				0xF2,	DSP_CP_MATH},
	{"rceil",			0xF9,	DSP_CP_MATH},
	{"rfloor",			0xFB,	DSP_CP_MATH},

/*	{"degree",	0xB0,	DSP_CP_MATH},	*/	/* Was 'J' in symbols (718) */
	{"oint",		'C',	DSP_CP_SYMBOL},	/* Was 'C' in symbols (2269) non-PS */
	{"mp",		'2',	DSP_CP_SYMBOL},	/* Was '2' in symbols (2234) non-PS */

	{"approx",	'O',	DSP_CP_SYMBOL},	{"Approx",	'P',	DSP_CP_SYMBOL},
	{"bra",		'X',	DSP_CP_SYMBOL},	{"ket",		'Y',	DSP_CP_SYMBOL},

	{"box",		' ',	DSP_CP_SYMBOL},	{"Box",		'\'',	DSP_CP_SYMBOL},
	{"circ",		'!',	DSP_CP_SYMBOL},	{"Circ",		'(',	DSP_CP_SYMBOL},
	{"triangle",'"',	DSP_CP_SYMBOL},	{"Triangle",')',	DSP_CP_SYMBOL},
	{"diamond",	'%',	DSP_CP_SYMBOL},
	{"star",		'&',	DSP_CP_SYMBOL},	{"Star",		'-',	DSP_CP_SYMBOL},

	{NULL, 0, 0} };

PRIVATE char *DrawSpecial(char *aptr, SYMLOC *sym, SYMMODS *mods) {

	char str[DFLT_STR_SIZE], *myptr, *oldaptr;
	int  i,isym, cp_hold;
	FOREIGN *flist;
	SPECIALS *slist;

	if (*aptr == '\0') {
		Draw('\\', sym, mods);
	} else if (*aptr == ',') {					/* Thin space \, */
	   Draw(DO_THINSPACE, sym, mods);
		aptr++;
	} else if (isdigit(*aptr)) {				/* 0-9 become symbols 0-13 */
		mods->textsym = TRUE;
		isym = *(aptr++)-'0';					/* Symbol value */
		if (isym <= 1 && isdigit(*aptr)) isym = 10*isym + (*(aptr++)-'0');
		Draw(isym, sym, mods);					/* Draw it */
		if (isspace(*aptr)) aptr++;			/* Skip first trailing blank */
		mods->textsym = FALSE;
	} else if (strchr("'\"`^~", *aptr) != NULL) { /* Foreign accent character */
		if      (*aptr == '\'') flist = squoted;
		else if (*aptr == '"')  flist = quoted;
		else if (*aptr == '`')  flist = bquoted;
		else if (*aptr == '~')  flist = tilded;
		else if (*aptr == '^')  flist = hatted;
		aptr++;
		while ( (flist->chr != '\0') && (flist->chr != *aptr) ) flist++;
		if (flist->chr != '\0') {				/* Valid special character */
			Draw(flist->sym, sym, mods);		/* Draw upper code page character */
			aptr++;
		} else {
			Draw(*(aptr-1), sym, mods);		/* Draw simple quote */
		}
	} else if (! isalpha(*aptr)) {
		Draw(*aptr++, sym, mods);
	} else {
		slist  = speclist;						/* Search the speclist	*/
		oldaptr = aptr;							/* Save old pointer		*/
		myptr   = str;								/* Store into str			*/
		while (isalpha(*aptr)) *myptr++ = *aptr++;
		*myptr = '\0';								/* NULL terminate			*/
		while (slist->name!=NULL && strcmp(slist->name,str)!=0) slist++;
		if (slist->name == NULL) {
			Draw('\\', sym, mods);				/* Draw as specified		*/
			aptr = oldaptr;						/* Restore old pointer	*/
		} else {
			if (slist->cp == 0) {				/* Simple translation	*/
				Draw(slist->sym, sym, mods);
			} else if (slist->sym != 0) {		/* Special character		*/
				cp_hold = mods->cp;				/* Save current map		*/
				mods->cp = slist->cp;			/* Insert new map			*/
				mods->textsym = (slist->cp == DSP_CP_SYMBOL);
				Draw(slist->sym, sym, mods);	/* Draw it					*/
				mods->cp = cp_hold;				/* Restore original map */
				mods->textsym = FALSE;			/* Back to normal syms	*/
			} else switch(slist->cp) {
				case SP_PEN:
					while (isspace(*aptr)) aptr++;	/* Skip spaces */
					mods->mypen = 0;
					while (isdigit(*aptr)) mods->mypen = mods->mypen*10+(*aptr++-'0');
					if (sym->DoDrawing) PlotSelectPen(mods->mypen);
					break;
				case SP_NEWLINE:						/* Fall through to SP_CR also */
					sym->xxstart += 1.2f*sym->sinth*sym->height;
					sym->yystart -= 1.2f*sym->costh*sym->height;
					mods = unwind(sym, mods, 1);
					break;
				case SP_CR:
					mods = unwind(sym, mods, 1);
					break;
				case SP_BACKSPACE:
					for (i=0; i<NOLDS-1; i++) {
						sym->xx[i] = sym->xx[i+1];
						sym->yy[i] = sym->yy[i+1];
					}
					break;

				case SP_DEGREE:							/* Degree symbol */
					mods = superscript(mods);
					mods->leftshift = 0.15f;			/* Shift over 0.15 units */
					mods->hscale    = 0.8f;				/* And reduce total width */
					Draw('o', sym, mods);
					mods = unwind(sym, mods, 0);
					break;
					
				case SP_HELVETICA:
					mods->family = DSP_FAMILY_HELV;			break;
				case SP_TIMESROMAN:
					mods->family = DSP_FAMILY_TMSRM;			break;
				case SP_HERSHEY:
					mods->family = DSP_FAMILY_HERSHEY;		break;
				case SP_GOTHIC:
					mods->family = DSP_FAMILY_GOTHIC;		break;
				case SP_SCRIPT:
					mods->family = DSP_FAMILY_SCRIPT;		break;

				case SP_ROMAN:
					mods->cp     = DSP_CP_ASCII; break;
				case SP_GREEK:
					mods->cp     = DSP_CP_MATH; break;

				case SP_PLAIN:
					mods->attrib = 0;								break;
				case SP_BOLD:
					mods->attrib |= DSP_ATTRIB_BOLD;			break;
				case SP_ITAL:
					mods->attrib |= DSP_ATTRIB_ITAL;			break;

			}
		}
		if (isspace(*aptr)) aptr++;
	}
	return(aptr);
}


/*--------------------------------------------------------------------------- 
-- Routine to unwind the modifiable paramater table and possibly reset to the
-- original values from unmodifiable set
--
-- Inputs: sym  - local variables
--         mods - pointer to modifiable local parameters
--
-- Returns: New pointer to valid mods local parameter block
--------------------------------------------------------------------------- */
PRIVATE SYMMODS *unwind(SYMLOC *sym, SYMMODS *mods, int type) {

	SYMMODS *tmp;
	int cpen;
	int i;

	cpen = mods->mypen;						/* Hold pen for later comparison	*/
	i = (type == 0) ? 1 : 9999;			/* Unwind once, or inifinitely	*/

	while ( (tmp=mods->last) != NULL && i--) {
		free(mods);
		mods = tmp;
	}

/* ... If pen color has changed, and we are drawing, select correct pen now */
	if (mods->mypen != cpen && sym->DoDrawing) PlotSelectPen(mods->mypen);

	if (type != 0) {								/* Restore initial parameters */
		mods->height = sym->height;
		mods->yscrpt = 0;
		for (i=0; i<NOLDS; i++) {
			sym->xx[i] = sym->xxstart;
			sym->yy[i] = sym->yystart;
		}
	}

	return(mods);
}

/* ---------------------------------------------------------------------------
-- Map the old 0-10 style fonts into a font/attrib pair for uniform use.
--
-- Usage: attrib = MapFontSelection(int old_map_num);
--------------------------------------------------------------------------- */
PRIVATE void MapFontSelection(int old_map, int *m_family, int *m_cp, int *m_attrib) {

	int family, cp, attrib;

	old_map &= 0x0F;										/* Limit range */

	family = *m_family;									/* Try to keep the same */
	cp     = DSP_CP_ASCII;
	attrib = DSP_ATTRIB_NORMAL;

	if (family == DSP_FAMILY_GOTHIC || family == DSP_FAMILY_SCRIPT) 
		family = DSP_FAMILY_HERSHEY;

	switch (old_map) {
		case 0:
			cp = DSP_CP_SYMBOL; break;
		case 2:
		case 4:
			family = DSP_FAMILY_HERSHEY;
			cp     = DSP_CP_ODDGREEK;
			break;
		case 6:
		case 7:
			cp = DSP_CP_MATH;
			break;
		case 5:
			family = DSP_FAMILY_GOTHIC;
			break;
		case 8:
		case 9:
			family = DSP_FAMILY_SCRIPT;
			break;
	}

	if (old_map==3 || old_map==4 || old_map==7 || old_map==9 || old_map==10)
		attrib |= DSP_ATTRIB_BOLD;

	*m_family = family;
	*m_cp     = cp;
	*m_attrib = attrib;
	return;
}

/* ===========================================================================
-- Take font/attribute pair and select appropriate hershey font.
=========================================================================== */
PRIVATE int SelectHersheyMap(int family, int cp, int attrib) {

	int rcode=1;

	if (cp == DSP_CP_SYMBOL) {									/* No other choice */
		rcode = 0;
	} else if (cp == DSP_CP_MATH) {
		rcode = (attrib & DSP_ATTRIB_BOLD) ? 7 : 6 ;
	} else if (cp == DSP_CP_ODDGREEK) {
		rcode = (attrib & DSP_ATTRIB_BOLD) ? 4 : 2 ;
	} else if (family == DSP_FAMILY_GOTHIC) {				/* No other choice */
		rcode = 5;
	} else if (family == DSP_FAMILY_SCRIPT || attrib & DSP_ATTRIB_ITAL) {
		rcode = (attrib & DSP_ATTRIB_BOLD) ? 9 : 8 ;
	} else {
		rcode = (attrib & DSP_ATTRIB_BOLD) ? 3 : 1 ;
	}

	return(rcode);
}
