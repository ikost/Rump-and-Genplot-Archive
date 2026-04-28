/* annote.c */

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
#include <ctype.h>

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

typedef enum _OPS1 {
   DO_VIEW,
	DO_HELP,			DO_PARMS,		DO_CURSOR,		DO_SIZE,			DO_ANGLE,	
	DO_SPACING,		DO_ORGMODE,		DO_CENTER,		DO_LABEL,		DO_MULTILINE,
	DO_DRAW,			DO_SAMPLE,		DO_COORMODE,	DO_HCOPY,		DO_ANNOTE,
	DO_RESET,		DO_RETURN,		DO_ARROW,		DO_ARC,			DO_PARC,
	DO_CIRCLE,		DO_RECT,			DO_LINE,			DO_CONN,			DO_POINT,
   DO_CCIRCLE,		DO_PCIRCLE,		DO_DARROW,
	DO_SETRANGE,	DO_FILLEDRECT,
/* ... */
	SQ_HELP,			SQ_TYPE,			SQ_CYCLES,		SQ_AMP,			SQ_INTCYCLE,
	SQ_INTAMP,		SQ_SIZE,			SQ_1DRAW,		SQ_2DRAW,		SQ_3DRAW,
	SQ_SOLID,		SQ_DOT,			SQ_WAVY,			SQ_CLOSE,		SQ_SAMPLE,
	SQ_DRAW,			SQ_ROT,			SQ_RETURN,		SQ_LABEL,
/* ... */
	DO_NULL
} OPS1;


typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1	rcode;
} CMTYPE;

#define	TWOPI		6.283185308f					/* two pi */
#define	ESC		0x1B								/* Escape character	*/
#define	CTRLC		0x03								/* Control C			*/

#define	FLMAGIC	-2187304.0						/* Magic # (exact for float cmps) */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL GetCoord(int key, REAL x[], REAL y[], int npt, int nextra, char *text);
PRIVATE LOGICAL PlotSquigg(LOGICAL autoflag);
PRIVATE void arc(int key);
PRIVATE void SimpleCircle(int key);
PRIVATE void SetOptions(int *lt, int *lw, int *pen, BOOL *clip);
PRIVATE void UnSetOptions(int *lt, int *lw, int *pen, BOOL *clip);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
PRIVATE const CMTYPE cmlist[] = {
#ifdef CSET2
			{"help",			-4, DO_VIEW},
#endif
			{"?",				 1, DO_HELP},		{"help",			 2, DO_HELP},
			{"parameters",	 3, DO_PARMS},		{"parms",		-5, DO_PARMS},
			{"cursor",		 3, DO_CURSOR},	{"size",			 2, DO_SIZE},
			{"angle",		 3, DO_ANGLE},		{"spacing",		 4, DO_SPACING},
			{"orgmode",		 3, DO_ORGMODE},	{"align",  		 5, DO_ORGMODE},
			{"center",		 3, DO_CENTER},
			{"label",		 3, DO_LABEL},		{"place",		 4, DO_LABEL},
			{"lable",		-5, DO_LABEL},		/* Lisa's misspelling */
			{"multiline",	 5, DO_MULTILINE},
			{"draw",			-2, DO_DRAW},		{"sample",		 4, DO_SAMPLE},
			{"coormode",	 5, DO_COORMODE},	{"hcopy",		 2, DO_HCOPY},
			{"annote:",		 3, DO_ANNOTE},	{"annotate:",	 6, DO_ANNOTE},
			{"setrange",	-8, DO_SETRANGE},
			{"reset",		 3, DO_RESET},		{"return",		 3, DO_RETURN},
			{"quit",			-1, DO_RETURN},
			{NULL,			 0, DO_NULL} };

PRIVATE const CMTYPE drlist[] = {
			{"arrow",		 3, DO_ARROW},		{"arc",			 3, DO_ARC},
			{"darrow",		 4, DO_DARROW},
			{"parc",			 3, DO_PARC},		{"circle",		 2, DO_CIRCLE},
			{"ccircle",     3, DO_CCIRCLE},
			{"pcircle",     3, DO_PCIRCLE},
			{"rectangle",	 3, DO_RECT},		{"line",			 2, DO_LINE},
			{"connected",	 3, DO_CONN},		{"point",		 2, DO_POINT},
			{"filled_rectangle", 4, DO_FILLEDRECT},
			{NULL,			 0, DO_NULL} };

/* Annote paramters */
PRIVATE int xorgmode, yorgmode;					/* Origins (left, center etc) */
PRIVATE REAL ilspac, ansize, anangl, andeg, xoff, yoff;
PRIVATE REAL arwlen = 0.0;			/* Must be legal #'s since SGRAPH may do */
PRIVATE REAL arwang = 0.0;			/* a fprint before first call to ANNOTE */
PRIVATE LOGICAL InchMode, usrsav;
PRIVATE REAL curx,cury;

/* Squigg parameters */
PRIVATE enum {S_LEFT,S_RIGHT} squig=S_RIGHT;	/* Right or left squiggle		*/
PRIVATE REAL	wave,									/* Magnitude of side wiggle	*/
					wiggle,								/* Number of periods in side	*/
					wave2,								/* Magnitude interface wave	*/
					cycles,								/* Number of interface cycles	*/
					sqxlen,sqylen,						/* Length of squiggles			*/
					sqxcen,sqycen,sqangl;			/* Center of sample face		*/

/* ============================================================================
-- Usage Guide:
--
--     Subroutine for text processing of annotation for plots.
--     Basically, just requests the labelling of plots.
--
--     Usage: LOG = ANNOTE(AUTO)
--
--     Input: AUTO - Specifies if routine to automatically return to calling
--                   routine on an unrecognized command.
--                   .TRUE.  => Return
--                   .FALSE. => Stay in annote on errors
--
--     Output: ANNOTE - .FALSE. => Automatic return w/out explicit RET command
--                      .TRUE.  => Explicit return command
--
--  March 16, 1984 - MOT
--     Added call to CLPSET to allow plotting anywhere on the hard clip area
--
--  Dec. 1986 - MOT
--     Moved most of drawing to DRAW command, left labeling in ANNO.
--
--  Feb. 1986 - MOT
--     Discovered problem in HCOPY BACKUP algorithm.  Because of the automagic
--     CLPSET call after PLSUPP, it was effectively preventing multiple calls
--     to BACKUP.  This is now possible, but adds the potential problem of
--     losing the ability to draw outside of the normal clip boundary after a
--     backup to before the entrance to ANNOTE.  Tough Luck - can't please all
--     the people all the time.
--
--  Dec. 1986 - MOT
--     Changed calls to CLPSET to CLIP$
--
--  Feb. 1988 - MOT
--     Added ability to use different modes for specifying coordinates.
--     0 => Use absolute inches
--    +1 => Use user coordinates from SET
--    +2 => Include an offset to move position
============================================================================ */
LOGICAL PlotAnnote(LOGICAL autoflag) {

	static char *list[]={"Left", "Center", "Right", "Bottom", "Center", "Top"};
	static LOGICAL FirstTime = TRUE;

	int savek;											/* Save vector for CLIP$ */
	REAL save[4];

	char token[LONG_STR_SIZE], *aptr;
	int achr;
	LOGICAL rcode=TRUE;								/* Assume successful exit */
	int opt_lt, opt_lw, opt_pen;					/* Option parameters */
	BOOL opt_clip;

	REAL x[3], y[3], xmod, ymod, xtmp, lang;
	int i;
	LOGICAL flag, ltmp;
	CMTYPE *citem;
	FILE *lun;
	enum {SIMPLEFILE, PIPE} filetype;

/* -------------------------- */
	if (FirstTime) {PlotResetAnnote(TRUE); FirstTime = FALSE;}

/* ------------------------------- */
/* Set PLOT to desired environment */
/* ------------------------------- */
ReStart:
	usrsav = PlotSetUserMode(FALSE);					/* Leave user mode */
	savek  = -1;
	PlotSetClip(&savek,save);							/* Query current CLIP mode */
	i = 1;
	PlotSetClip(&i,save);								/* Set to maximum size */

/* ------------------------ */
/* Command processing		 */
/* ------------------------ */
	while (TRUE) {
		LexEscape(TRUE);											/* Clear <ESC> condition */
		while (! LexGetTokenP(token, sizeof(token), "ANNOTE: ")) continue;
		if (LexEscape(TRUE)) break;							/* Exit out now */
		if (LexAlias(token, sizeof(token))) continue;	/* Check for alias */

		citem = LexCmdl(token, cmlist, sizeof(CMTYPE));
		if (citem == NULL) citem = LexCmdl(token, drlist, sizeof(CMTYPE));
		if (citem == NULL) {
			if (LexSystem(0, token)) {						/* How about system command */
				continue;
			} else if (PlotSystem(-6, token, NULL)) {	/* Can PLSUPP handle? */
				PlotSetUserMode(usrsav);					/* Leave user mode */
				PlotSetClip(&savek, save);
				PlotSystem(0, token, NULL);
				goto ReStart;									/* Restore save values */
			} else if (autoflag) {							/* Automatic return? */
				LexBackup();
				rcode = FALSE;
				break;
			} else {
				gen_err2("Unrecognized or ambiguous command", token);
				LexFlush();
				continue;
			}
		} else switch (citem->rcode) {

#ifdef CSET2
			case DO_VIEW:
			{
				char path[PATH_MAX], cmdline[PATH_MAX];
				LexGetRest(token, sizeof(token));
				if (*token == '\0') strcpy(token, "annote");
				SysResolveDyntName(path, "genplot.inf", sizeof(path));
				sprintf(cmdline, "start view %s %s", path, token);
				SysSystem(cmdline);
				break;
			}
#endif

			case DO_HELP:
				LexCmdlPrint(cmlist, sizeof(CMTYPE), "Annote Commands:");
				LexCmdlPrint(drlist, sizeof(CMTYPE), "Draw Commands:");
				PlotSystem(U_HELP, NULL,NULL);
				LexSystem (U_HELP, NULL);
				break;

			case DO_PARMS:
				TTYputs("\nGraph labelling parameters:\n");
				TTYprintf("  Label Size: %-4.2f     Drawing Angle: %-6.1f\n", ansize, andeg);
				TTYprintf("  Interline spacing: %-4.2f\n", ilspac);
				TTYprintf("Origin for text specified at %s %s\n\n", list[xorgmode], list[yorgmode+3]);
				break;

			case DO_CURSOR:					/* Cursor command to return coordinate set */
				PlotSetUserMode(InchMode ? FALSE : usrsav);	/* Coordinates set */

				TTYputs("Returns absolute inches or user: <space> recycles\n");
				GVLinkReal("XCUR", GVF_HIDDEN, &curx);
				GVLinkReal("YCUR", GVF_HIDDEN, &cury);
				GVLinkReal("CURX", GVF_HIDDEN, &curx);
				GVLinkReal("CURY", GVF_HIDDEN, &cury);
				do {
					PlotCursor(&curx, &cury, &achr);
					TTYprintf("X: %-14.7g      Y: %-14.7g\n", curx, cury);
				} while (achr == '0' || achr == ' ');
				break;

			case DO_SIZE:
				xtmp = LexGetReal(ansize, "Size of text (no change): ");
				if (! LexEscape(TRUE)) ansize = xtmp;
				break;

			case DO_ANGLE:
				xtmp = LexGetReal(FLMAGIC, "Angle to draw (cursor): ");
				if (LexEscape(TRUE)) break;
				if (xtmp != FLMAGIC) {
					andeg  = xtmp;							/* In degrees */
					anangl = xtmp*0.01745329252f;		/* And in radians */
				} else{
					if (! GetCoord(1, x, y, 2, 0, "Points from start to end of angle: (CURSOR) ")) break;
					anangl = (REAL) atan2(y[1]-y[0], x[1]-x[0]);
					andeg  = anangl/0.01745329252f;
				}
				break;

			case DO_SPACING:
				xtmp = LexGetReal(2.0, "Interline spacing in char heights (2.0): ");
				if (! LexEscape(TRUE)) ilspac = xtmp;
				break;

			case DO_ORGMODE:
				ltmp = LexGetTokenP(token, sizeof(token), 
					"Origin point for labels [LB|lc|lt|cb|cc|ct|rb|rc|rt] ");
				if (LexEscape(TRUE)) break;
				if (! ltmp) {
					xorgmode = 0; yorgmode = 0;
				} else {
					token[3] = '\0';
					i = LexSelect(token, "LB LC LT CB CC CT RB RC RT");
					if (i <= 0) i = LexSelect(token, "BL CL TL BC CC TC BR CR TR");
					if (i <= 0) {
						gen_err2("Illegal label origin specified",token);
						LexFlush();
						break;
					}
					xorgmode = ((i-1)/3);					/* Will give 0,1,2 */
					yorgmode = ((i-1)%3);					/* Will give 0,1,2 */
				}
				break;

			case DO_CENTER:
				ltmp = LexOnOff(FALSE, "Centering mode (OFF): ");
				if (LexEscape(TRUE)) break;
				xorgmode = yorgmode = (ltmp ? 1 : 0);
				break;

			case DO_LABEL:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(1, x, y, 1, 0, "Coordinates of text: (CURSOR) ")) {
					do {
						if (! LexGetStrExpr(token, sizeof(token))) LexPromptStr(token, sizeof(token), "Text: ");
						if (LexEscape(TRUE)) break;
						i = (int) strnblen(token);
						flag = token[i-1] == '~';
						if (flag) token[i-1] = '\0';
						xmod = 0.5f*xorgmode*PlotQueryStringLength(ansize, token, 0);	/* 0=left,   1=center, 2=right */
						ymod = 0.5f*yorgmode*ansize;											/* 0=bottom, 1=center, 2=top	*/
						x[1] = (REAL) (x[0] - xmod*cos(anangl) + ymod*sin(anangl));
						y[1] = (REAL) (y[0] - ymod*cos(anangl) - xmod*sin(anangl));
						PlotString(x[1], y[1], ansize, token, andeg, 0);
						PlotFlush();
						x[0] += (REAL) (ansize*sin(anangl)*ilspac);
						y[0] -= (REAL) (ansize*cos(anangl)*ilspac);
					} while (flag);
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_MULTILINE:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(1, x, y, 1, 0, "Coordinates of text: (CURSOR) ")) {
					lun = NULL;												/* Not file mode	*/
					if (LexGetToken(token, sizeof(token))) {		/* Possible file	*/
						if (*token == '<') {								/* YES!!!			*/
							aptr = token+1;								/* Filename posn	*/
							if (*aptr == '\0') {							/* Form < file		*/
								if (! LexGetTokenP(token, sizeof(token), "Filename: ")) break;
								aptr = token;
							}
							if ( (lun = fopen(aptr, "r")) == NULL) {
								ERRprintf("ERROR: %s failed to open\n", aptr);
								break;
							}
							filetype = SIMPLEFILE;
						} else if (*token == '|') {					/* PIPE!!!			*/
							aptr = token+1;								/* Filename posn	*/
							if (*aptr == '\0') {							/* Form < file		*/
								if (! LexGetTokenP(token, sizeof(token), "Pipe command: ")) break;
								aptr = token;
							}
							if ( (lun = popen(aptr, "r")) == NULL) {
								ERRprintf("ERROR: %s failed to execute\n", aptr);
								break;
							}
							filetype = PIPE;
						} else {												/* Unrecognized	*/
							LexBackup();
						}
					}
					if (lun == NULL) 
						TTYputs("Enter lines of text.  End with <ESC> or string @END\n");
					while (TRUE) {
						if (lun == NULL) {
							LexPromptStr(token, sizeof(token), ": ");
							if (LexEscape(TRUE)) break;
						} else {
							if (fgets(token, sizeof(token), lun) == NULL) break;
							if ( (aptr = strchr(token, '\n')) != NULL) *aptr = '\0';
						}
						aptr = token; while (isspace(*aptr)) aptr++;
						if (stricmp(aptr, "@END") == 0) break;
						if (strnicmp(aptr, "sprintf(", 8) == 0) {							/* One exception handling */
							int ierr;
							aptr = GVEvalStrExpr(aptr, &ierr);
							if (aptr != NULL && ierr == 0) strscpy(token, aptr, sizeof(token));
							if (aptr != NULL) free(aptr);
						}
						i = (int) strnblen(token);
						xmod = 0.5f*xorgmode*PlotQueryStringLength(ansize, token, 0);	/* 0=left,   1=center, 2=right */
						ymod = 0.5f*yorgmode*ansize;											/* 0=bottom, 1=center, 2=top	*/
						x[1] = (REAL) (x[0] - xmod*cos(anangl) + ymod*sin(anangl));
						y[1] = (REAL) (y[0] - ymod*cos(anangl) - xmod*sin(anangl));
						PlotString(x[1], y[1], ansize, token, andeg, 0);
						PlotFlush();
						x[0] += (REAL) (ansize*sin(anangl)*ilspac);
						y[0] -= (REAL) (ansize*cos(anangl)*ilspac);
					}
					if (lun != NULL) {
						if (filetype == SIMPLEFILE) fclose(lun);
						if (filetype == PIPE)       pclose(lun);
					}
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_DRAW:					/* Now obsolete -- do nothing command */
				break;

			case DO_SAMPLE:
				PlotSetUserMode(InchMode ? FALSE : usrsav);	/* Coordinates set */
				if (! PlotSquigg(TRUE)) TTYputs("SAMPLE: Automatic return\n");
				break;

			case DO_COORMODE:
				ltmp = LexChoice(TRUE, "INCHES", "USER", "Coordinates specified as INCHES or USER (inches): ");
				if (LexEscape(TRUE)) break;
				InchMode = ltmp;
				xtmp = LexGetReal(xoff, "X offset: ");	if (LexEscape(TRUE)) break; 
				xoff = xtmp;
				xtmp = LexGetReal(xoff, "Y offset: ");	if (LexEscape(TRUE)) break;
				yoff = xtmp;
				break;

			case DO_HCOPY:
				if (! HardCopy(0)) LexFlush();
				break;

			case DO_ANNOTE:							/* Do nothing */
				break;

			case DO_SETRANGE:
				PlotSetUserMode(InchMode ? FALSE : usrsav);	/* Coordinates set */
				break;

			case DO_RESET:
				PlotResetAnnote(TRUE);
				break;

			case DO_RETURN:
				goto ExitAll;

			case DO_ARROW:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(1, x, y, 2, 0, "Start and end: (CURSOR) ")) {
					if (x[0] == x[1] && y[0] == y[1]) {
						gen_warn("Arrow aborted - zero length");
						break;
					}
					PlotMove(x[0],y[0],3);
					PlotMove(x[1],y[1],2);
					lang = (REAL) (atan2(y[0]-y[1],x[0]-x[1]));
					x[0] = (REAL) (x[1] + arwlen*cos(lang+arwang));
					y[0] = (REAL) (y[1] + arwlen*sin(lang+arwang));
					PlotMove(x[0],y[0],3);
					PlotMove(x[1],y[1],2);
					x[0] = (REAL) (x[1] + arwlen*cos(lang-arwang));
					y[0] = (REAL) (y[1] + arwlen*sin(lang-arwang));
					PlotMove(x[0],y[0],2);
					PlotFlush();
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_DARROW:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(1, x, y, 2, 0, "Start and end: (CURSOR) ")) {
					if (x[0] == x[1] && y[0] == y[1]) {
						gen_warn("Arrow aborted - zero length");
						break;
					}
					PlotMove(x[0],y[0],3);
					PlotMove(x[1],y[1],2);

					lang = (REAL) (atan2(y[0]-y[1],x[0]-x[1]));

					x[2] = (REAL) (x[1] + arwlen*cos(lang+arwang));
					y[2] = (REAL) (y[1] + arwlen*sin(lang+arwang));
					PlotMove(x[2],y[2],3);
					PlotMove(x[1],y[1],2);
					x[2] = (REAL) (x[1] + arwlen*cos(lang-arwang));
					y[2] = (REAL) (y[1] + arwlen*sin(lang-arwang));
					PlotMove(x[2],y[2],2);

					x[2] = (REAL) (x[0] - arwlen*cos(lang+arwang));
					y[2] = (REAL) (y[0] - arwlen*sin(lang+arwang));
					PlotMove(x[2],y[2],3);
					PlotMove(x[0],y[0],2);
					x[2] = (REAL) (x[0] - arwlen*cos(lang-arwang));
					y[2] = (REAL) (y[0] - arwlen*sin(lang-arwang));
					PlotMove(x[2],y[2],2);

					PlotFlush();
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;


			case DO_ARC:
				arc(0);
				break;

			case DO_PARC:
				arc(1);
				break;

			case DO_CCIRCLE:
				SimpleCircle(0);
				break;

			case DO_PCIRCLE:
				SimpleCircle(1);
				break;

			case DO_CIRCLE:
				arc(2);
				break;

			case DO_RECT:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(2, x, y, 2, 0, "Enter corners of rectangle (CURSOR): ")) {
					i = LexGetInt(1, "Line type to draw box (1 => solid): ");
					if (! LexEscape(TRUE)) {
						PlotSetLineType(i, 0.0);
						PlotMove(x[0], y[0], 3);
						PlotMove(x[0], y[1], 2);
						PlotMove(x[1], y[1], 2);
						PlotMove(x[1], y[0], 2);
						PlotMove(x[0], y[0], 2);
						PlotSetLineType(1, 0.0);
						PlotFlush();
					}
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_FILLEDRECT:
				if (! GetCoord(2, x, y, 2, 0, "Enter corners of rectangle (CURSOR): ")) break;
				if (! LexGetMathP(token, sizeof(token), "Color to fill (0 = background): ")) break;
				if (LexEscape(TRUE)) break;
				i = PlotMatchColor(token, 0);
				PlotFillRect(x[0],y[0], x[1],y[1], i);
				PlotFlush();
				break;

			case DO_LINE:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(3, x, y, 2, 0, "Beginning and end of line (CURSOR): ")) {
					i = LexGetInt(1, "Type of line (1 => solid): ");
					if (! LexEscape(TRUE)) {
						PlotSetLineType(i, 0.0);
						PlotMove(x[0], y[0], 3); PlotMove(x[1], y[1], 2);
						PlotSetLineType(1, 0.0);
						PlotFlush();
					}
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_POINT:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				if (GetCoord(1, x, y, 1, 0, "Coordinate of a point (CURSOR): ")) {
					i = LexGetInt(1, "Symbol number (0-13): ");
					if (! LexEscape(TRUE)) {
						PlotSymbol(x[0], y[0], ansize, (CHAR) i);
						PlotFlush();
					}
				}
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			case DO_CONN:
				SetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);		/* Unusual in done first */

				PlotSetUserMode(InchMode ? FALSE : usrsav);	/* Coordinates set */
				x[1] = LexGetReal(FLMAGIC, "Enter points (CURSOR): ");
				if (LexEscape(TRUE)) goto AbortConnected;
				if (x[1] == FLMAGIC) {
					TTYputs("Enter points, end with a non-0 or non-space char\n");
					PlotCursor(x, y, &achr);
					if (achr == ESC) goto AbortConnected;
				} else {
					x[0] = x[1];
					y[0] = LexGetReal(0.0, "And Y coord: ");
					if (LexEscape(TRUE)) goto AbortConnected;
				}
				PlotMove(x[0]+xoff, y[0]+yoff, 3);
				PlotFlush();

				achr = ' ';
				while (TRUE) {
					if (x[1] != FLMAGIC) {
						x[0] = LexGetReal(FLMAGIC, "Next point (quit): ");
						if (LexEscape(TRUE)) break;
						if (x[0] == FLMAGIC) break;
						y[0] = LexGetReal(*y, "Y val: ");
						if (LexEscape(TRUE)) break;
					} else {
						if (achr != ' ' && achr != '0') break;
						PlotCursor(x, y, &achr);
						if (achr == ESC) break;
					}
					PlotMove(x[0]+xoff, y[0]+yoff, 2);
					PlotFlush();
				}

AbortConnected:
				UnSetOptions(&opt_lt, &opt_lw, &opt_pen, &opt_clip);
				break;

			default:
				TTYputs("This is a real screwup -- never come here (ANNOTE)\n");
				break;
		}
	}

ExitAll:
	PlotSetClip(&savek, save);
	PlotSetUserMode(usrsav);
	return(rcode);
}


/* ===========================================================================
-- Simple routine to scan for common options - pen/linetype/linewidth
=========================================================================== */
PRIVATE void SetOptions(int *lt, int *lw, int *pen, BOOL *clip) {

	char token[DFLT_STR_SIZE];
	int i;

/* Set default values */
	if (lt   != NULL) *lt  = -1;
	if (lw   != NULL) *lw  = -1;
	if (pen  != NULL) *pen = -1;
	if (clip != NULL) *clip = FALSE;

	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-linetype", 6) || LexEqual(token, "-ltype", 3)) {
			i = LexGetInt(1, "LType: ");
			if (lt != NULL) *lt = i;
		} else if (LexEqual(token, "-pen", 4) || LexEqual(token, "-color", 4)) {
			if (LexGetMathP(token, sizeof(token), "Pen color: ")) i = PlotMatchColor(token, -1);
			if (pen != NULL) *pen = i;
		} else if (LexEqual(token, "-linewidth", 5) || LexEqual(token, "-lw", 3)) {
			i = nint(7*LexGetReal(1.0f, "LWidth: "));
			if (lw != NULL) *lw = i;
		} else if (LexEqual(token, "-clip", 5)) {
			if (clip != NULL) *clip = TRUE;
		} else {
			LexBackup();								/* Treat as a number */
			break;
		}
	}

	if (pen  != NULL && *pen != -1) *pen = PlotSelectPen(*pen);
	if (lw   != NULL && *lw  != -1) *lw  = PlotSetLineWidth(*lw);
	if (lt   != NULL && *lt  != -1)        PlotSetLineType(*lt, 0.0f);
	if (clip != NULL && *clip)      { int i=0; PlotSetClip(&i, NULL); }

	return;
}
		

PRIVATE void UnSetOptions(int *lt, int *lw, int *pen, BOOL *clip) {

	if (pen  != NULL && *pen != -1) { PlotSelectPen(*pen); *pen = -1; }
	if (lw   != NULL && *lw  != -1) { PlotSetLineWidth(*lw); *lw = -1; }
	if (lt   != NULL && *lt  != -1) { PlotSetLineType(1, 0.0f); *lt = -1; }
	if (clip != NULL && *clip)      { int i=1; PlotSetClip(&i, NULL); }

	return;
}

/* ============================================================================
-- Simple Circle:
--
--     SimpleCircle:
============================================================================ */
PRIVATE void SimpleCircle(int key) {

	REAL x[2], y[2], radius, theta, d_theta, start, end;
	int i, numsegs;

	int lw, pen, lt;				/* Parameters for set/clear options */
	BOOL clip;

/* Check and set color/linewidth/linetype options */
	SetOptions(&lt, &lw, &pen, &clip);

	if (! GetCoord(0x10 | 0x01, x, y, 1, 1, "(x,y,r) Center and radius in inches (or radius point): (CURSOR) ")) goto Exit_Me;
	radius = x[1];
	
/* If partial arc, get the angles */
	if (key == 1) {
		start = TWOPI/360.0f*LexGetReal(0.0f, "Starting angle (counterclockwise from X axis): ");
		end   = TWOPI/360.0f*LexGetReal(360.0f, "Ending angle: ");
	} else {
		start = 0.0f;
		end   = TWOPI;
	}
	if (fabs(end-start) > TWOPI) {start = 0.0f; end = TWOPI;}

/* Check and set color/linewidth/linetype options */
	numsegs = nint(fabs(radius*(end-start)/0.04));	/* 1 mm maximum arc size	*/
	numsegs = min(361,max(21, numsegs));				/* But not less than 20		*/
	d_theta = (end-start)/(numsegs-1);					/* Increment size				*/
	for (i=0; i<numsegs; i++) {							/* Number of segments		*/
		theta = start + i*d_theta;
		PlotMove((REAL) (x[0]+radius*cos(theta)), (REAL) (y[0]+radius*sin(theta)), (i==0)?3:2);
	}
	PlotFlush();

/* Restore settings from options */
Exit_Me:
	UnSetOptions(&lt, &lw, &pen, &clip);
	return;
}


/* ============================================================================
-- Usage Guide:
--
--     ARC - Draw a portion of an arc on plot
--
--      mode 0: - arc
--      mode 1: - parc
--      mode 2: - circle
============================================================================ */
PRIVATE void arc(int mode) {

	REAL x[3], y[3], cx, cy, radius, theta, t1,t2,t3, beg,mid,end, dx;
	int i, lw, pen, lt, clip;

/* Check and set color/linewidth/linetype options */
	SetOptions(&lt, &lw, &pen, &clip);

	if (! GetCoord(1, x, y, 3, 0, "(x,y) Start End Mid: (CURSOR) ")) goto Exit_Me;

	t1 = (x[0]-x[2])*(x[0]-x[2]) + (y[0]-y[2])*(y[0]-y[2]);
	t2 = (x[1]-x[2])*(x[1]-x[2]) + (y[1]-y[2])*(y[1]-y[2]);
	t3 = (x[0]-x[2])*(y[1]-y[2]) - (x[1]-x[2])*(y[0]-y[2]);
	if (t3 == 0.0) {
		gen_warn("Points colinear - Line only drawn");
		PlotMove(x[0],y[0],3);
		PlotMove(x[1],y[1],2);
	} else {
		cx = x[2] + 0.5f*(t1*(y[1]-y[2]) - t2*(y[0]-y[2]))/t3;	/* Find actual cntr */
		cy = y[2] - 0.5f*(t1*(x[1]-x[2]) - t2*(x[0]-x[2]))/t3;	/* Through points */
		radius = (x[0]-cx)*(x[0]-cx) + (y[0]-cy)*(y[0]-cy);
		radius = (REAL) sqrt(radius);										/* The radius */
		beg = (REAL) atan2(y[0]-cy,x[0]-cx);							/* Angles from cntr */
		end = (REAL) atan2(y[1]-cy,x[1]-cx);
		mid = (REAL) atan2(y[2]-cy,x[2]-cx);

		if (beg < mid) {					/* Make sure we have the right directions */
			if (end < beg) {
				end += TWOPI;
			} else if (end < mid) {
				beg += TWOPI;
			}
		} else {
			if (end > beg) {
				end -= TWOPI;
			} else if (end > mid) {
				beg -= TWOPI;
			}
		}

		if (mode == 2) end = beg + TWOPI;			/* Handle circles as 2*pi	*/
		i = nint(fabs(radius*(end-beg)/0.04));		/* 1 mm maximum arc size	*/
		i = max(20, i);									/* But not less than 20		*/
		dx = (end-beg)/i;									/* Increment size				*/
		theta = beg;										/* Starting from beginning	*/
		PlotMove(x[0],y[0],3);							/* And move to start			*/
		while (i--) {
			theta += dx;
			x[0] = (REAL) (cx + radius*cos(theta));
			y[0] = (REAL) (cy + radius*sin(theta));
			PlotMove(x[0],y[0],2);
		}

		t1 = arwlen/2.0f/radius;
		if (t1 <= 1.0 && mode == 0) {
			t1 = (REAL) (asin(t1) + TWOPI/4);
			if (beg < end) 
				theta -= t1;
			else
				theta += t1;
			x[0] = (REAL) (x[1] + arwlen*cos(theta+arwang));
			y[0] = (REAL) (y[1] + arwlen*sin(theta+arwang));
			PlotMove(x[0],y[0],3);
			PlotMove(x[1],y[1],2);
			x[0] = (REAL) (x[1] + arwlen*cos(theta-arwang));
			y[0] = (REAL) (y[1] + arwlen*sin(theta-arwang));
			PlotMove(x[0],y[0],2);
		}
	}
	PlotFlush();

/* Restore settings from options */
Exit_Me:
	UnSetOptions(&lt, &lw, &pen, &clip);

	return;
}

/* ============================================================================
--     Subroutine to input coordinates either cursor or entry
--
--     CALL GETCOORD(key, X,Y,NPT,NEXTRA,TEXT)
--
--     Inputs: key  - 0x0F - flag for type of cursor
--                       1 -> use normal cursor
--                       2 -> use rectangle (box) cursor
--                       3 -> use line cursor
--                    0x10 - Special mode for getting radius of circle
--             NPT  - Number of points to get
--             NEXTRA - Special other number of points
--             TEXT - Initial prompt text
--
--     Output: X,Y  - Array of the points (absolute inches always)
--
-- ... Uses either absolute inches or user coordinates.  Transforms to inches
-- ... before returning to calling program.
--
-- Routine always returns with plot scaling set in absolute inches also.
============================================================================ */
PRIVATE LOGICAL GetCoord(int key, REAL x[], REAL y[], int npt, int nextra, char *token) {

	LOGICAL CursorMode = FALSE;
	int achr, CursorType;
	int i, igot, ipts;

	PlotSetUserMode(InchMode ? FALSE : usrsav);	/* Coordinates set */

	i = 0;
	npt = abs(npt);

	CursorType = key & 0x0F;
	while (i < npt) {
		igot = 1;
		if (CursorMode) {
			if (CursorType != 2 || (i==npt-1)) {
				PlotCursor(x+i, y+i, &achr);
			} else {
				PlotBoxCursor(x+i, y+i, x+i+1, y+i+1, &achr);
				igot = 2;
			}
			if (achr == ESC) {
				CursorMode = FALSE;
				continue;
			} else if (achr == CTRLC) {							/* Break */
				gen_warn("Cursor entry aborted");
				LexFlush();
				PlotSetUserMode(FALSE);
				return(FALSE);
			}
		} else {
			x[i] = LexGetReal(FLMAGIC, (i==0) ? token : "Next point: ");
			if (LexEscape(TRUE)) {
				PlotSetUserMode(FALSE);
				return(FALSE);
			}
			if (x[i] == FLMAGIC) {
				CursorMode = TRUE;
				continue;
			}
			y[i] = LexGetReal(0.0,"And Y coord: ");
			if (LexEscape(TRUE)) {
				PlotSetUserMode(FALSE);
				return(FALSE);
			}
		}
		i += igot;
	}

/* Special mode */
	i = 0;
	while (key & 0x10 && i < nextra) {							/* Request for radius from CCIRCLE */
		if (CursorMode) {
			PlotCursor(x+npt+i, y+npt+i, &achr);
			if (achr == ESC) {
				CursorMode = FALSE;
				continue;
			} else if (achr == CTRLC) {							/* Break */
				gen_warn("Cursor entry aborted");
				LexFlush();
				PlotSetUserMode(FALSE);
				return(FALSE);
			}
		} else {
			x[npt+i] = (REAL) fabs(LexGetReal(1.0, "Radius (in inches only): "));
			y[npt+i] = 0;
		}
		i++;
	}
	PlotSetUserMode(FALSE);							/* Be in inches */

/* Output the values for uses */
	if (CursorMode) {
		TTYputs(" Coordinates:");
		for (i=0; i<npt; i++) TTYprintf("  %.4g %.4g", x[i], y[i]);
		TTYputs("\n");
	}

/* Number of points to be converted depends on modes */
	ipts = npt;
	if (CursorMode && (key & 0x10)) ipts += nextra;

/* Transform to add the offset from user specifications */
	for (i=0; i<ipts; i++) {				/* And transform to inches */
		x[i] += xoff;									/* Offsets */
		y[i] += yoff;
	}

	if (! InchMode && usrsav) {
		PlotSetUserMode(usrsav);
		for (i=0; i<ipts; i++) {			/* And transform to inches */
			PlotConvert2DScales(USER_TO_INCH, x[i], y[i], &x[i], &y[i]);
		}
	}

/* And if a radius request, convert to radius instead of offset by last point */
	if (key & 0x10 && CursorMode) {				/* Radius requests */
		for (i=0; i<nextra; i++) {
			x[npt+i] = (REAL) sqrt( pow(x[npt+i]-x[npt-1],2) + pow(y[npt+i]-y[npt-1],2));
			y[npt+i] = 0;
			TTYprintf("      Radius:  %.4g\n", x[npt+i]);
		}
	}

	PlotSetUserMode(FALSE);
	return(TRUE);
}


/* ============================================================================
-- Usage Guide:
--
-- Usage: CALL ANN$SET(key)
--
-- Inputs: key - 0 => List out parameters to Terminal
--              -1 => Initialize if appropriate
--
-- Output: Lists internal variables out to the I/O terminal
============================================================================ */
void PlotAnnoteSet(int key) {
	
	static LOGICAL First=TRUE;

	if (First) {
		First = FALSE;
		GVLinkReal("$ARWLEN",  GVF_HIDDEN, &arwlen);
		GVLinkReal("$ARWANG",  GVF_HIDDEN, &arwang);
	}
	if (key == 0) {
		TTYputs("ANNOTE parameters\n");
		TTYprintf(" ARWLEN: %-10.4g ARWANG: %-10.4g\n", arwlen, arwang);
	}
	return;
}

/* ============================================================================
-- Usage:  CALL RST$ANNO
--
-- Inputs: none
--
-- Output: Resets internal common block variables to initial values
============================================================================ */
void PlotResetAnnote(LOGICAL FullReset) {

/* Annote parameters */
	ilspac   = 2.0;								/* Interline space for LABEL */
	xorgmode = 0;									/* Left */
	yorgmode = 0;									/* Bottom */
	ansize   = 0.20f;
	anangl   = 0.0;
	andeg    = 0.0;
	arwlen   = 0.15f;
	arwang   = 0.52f;
	InchMode = TRUE;
	xoff     = 0.0;
	yoff     = 0.0;

/* Squigg parameters */
	wave   = 0.1f;					/* Main squigg amplitude */
	wiggle = 1.0f;					/* Number of wiggles		*/
	wave2  = 0.02f;				/* Inside wiggle amplitude */
	cycles = 10.0f;				/* Cycles on inside wiggles */
	sqxlen = 2.0f;					/* Length of squigg */
	sqylen = 1.0f;
	sqxcen = sqycen = 1.0f;		/* Center of new style sample drawing */
	sqangl = 0.0f;					/* Draw at 0 degrees initially */
	
	GVLinkReal("$ARWLEN",  GVF_HIDDEN, &arwlen);
	GVLinkReal("$ARWANG",  GVF_HIDDEN, &arwang);
	return;
}

PRIVATE const CMTYPE sqlist[] = {
			{"?",					1, SQ_HELP},		{"help",			2, SQ_HELP},
			{"type",				3, SQ_TYPE},		{"cycles",		3, SQ_CYCLES},
			{"amplitude",		3, SQ_AMP},			{"intcycle",	4, SQ_INTCYCLE},
			{"intamplitude",	4, SQ_INTAMP},		{"size",			2, SQ_SIZE},
			{"1draw",			1, SQ_1DRAW},		{"2draw",		1, SQ_2DRAW},
			{"3draw",			1, SQ_3DRAW},		{"draw",			4, SQ_DRAW},
			{"rotsample",		3, SQ_ROT},			{"solid",		3, SQ_SOLID},
			{"dotted",			4, SQ_DOT},			{"label",		3, SQ_LABEL},
			{"wavy",				3, SQ_WAVY},		{"wave",		  -4, SQ_WAVY},
			{"close",			2, SQ_CLOSE},		{"sample",		3, SQ_SAMPLE},
			{"return",			3, SQ_RETURN},
			{NULL,				0, DO_NULL} };

#define	PLOTMOVE(x,y,m)	PlotMove((REAL) (sqxcen+(x)*cc*sqxlen-(y)*ss*sqylen), \
				                        (REAL) (sqycen+(y)*cc*sqylen+(x)*ss*sqxlen), m)
static void draw_squigg(void) {

	double ss,cc;
	int i;

	if (sqxlen == 0) sqxlen = 1;			/* Avoid problems */
	if (sqylen == 0) sqylen = 1;
	
	ss = sin(sqangl*0.017453292519943);
	cc = cos(sqangl*0.017453292519943);

	PlotSetLineType(1,0.0);
	PLOTMOVE(0, -0.5, 3);
	PLOTMOVE(0, +0.5, 2);
	for (i=0; i<100; i++)
		PLOTMOVE(i/99.0, 0.5+wave*sin(TWOPI*wiggle*i/99.0), 2);
	PLOTMOVE(0, -0.5, 3);
	for (i=0; i<100; i++)
		PLOTMOVE(i/99.0,-0.5+wave*sin(TWOPI*wiggle*i/99.0), 2);
	PlotFlush();
	return;
}


/* ============================================================================
-- Usage Guide:
--
--     Routine to draw a sample box on the screen
--
--     Usage: Command oriented processor.  Accepts commands until it finds an
--            unrecognized one, or a return command.
--            LOG = SQUIGG(AUTO)
--
--     Input: AUTO - .TRUE.  => Return automatically on errors to caller
--                   .FALSE. => Stay with this program and process errors
--
--     Output: SQUIGG - .TRUE.  => Explicit return to caller
--                      .FALSE. => Automatic return
============================================================================ */
PRIVATE LOGICAL PlotSquigg(LOGICAL autoflag) {

	LOGICAL rcode = TRUE;								/* Assume successful */
	LOGICAL ltmp, LocalUsrSave;
	char token[LONG_STR_SIZE];
	int  i;
	REAL xi[3],yi[3], xdraw,ydraw, xtmp,ytmp;
	double ss,cc;

	CMTYPE *citem;

/* ... Code begins */
	LocalUsrSave = PlotSetUserMode(FALSE);						/* Dump the mode */

	while (TRUE) {

		LexEscape(TRUE);											/* Clear <ESC> condition */
		while (! LexGetTokenP(token, sizeof(token), "SAMPLE: ")) continue;
		if (LexEscape(TRUE)) break;							/* Exit out now */
		if (LexAlias(token, sizeof(token))) continue;	/* Check for alias */
		
		citem = LexCmdl(token, sqlist, sizeof(CMTYPE));
		if (citem == NULL) {
			if (LexSystem(0, token)) {						/* How about system command */
				continue;
			} else if (PlotSystem(0, token, NULL)) {
				continue;
			} else if (autoflag) {							/* Automatic return? */
				LexBackup();
				rcode = FALSE;
				break;
			} else {
				gen_err2("Unrecognized or ambiguous command",token);
				LexFlush();
				continue;
			}
		} else switch (citem->rcode) {
			case SQ_HELP:
				LexCmdlPrint(sqlist, sizeof(CMTYPE), "Squigg commands:");
				break;

			case SQ_TYPE:
				ltmp = LexChoice(TRUE, "RIGHT", "LEFT", "Side to close (RIGHT): ");
				squig = ltmp ? S_RIGHT : S_LEFT ;
				break;

			case SQ_CYCLES:
				wiggle = LexGetReal(1.0, "Number of wiggles in side (1): ");
				break;

			case SQ_AMP:
				wave = LexGetReal(0.1f, "Magnitude of wave (0.1): ");
				break;

			case SQ_INTCYCLE:
				cycles = LexGetReal(10.0, "Number of cycles in interface (10): ");
				break;

			case SQ_INTAMP:
				wave2 = LexGetReal(0.02f, "Magnitude of interface wave (0.02): ");
				break;

			case SQ_SIZE:
				sqxlen = LexGetReal(2.0, "Length of squiggle edge (2): ");
				sqylen = LexGetReal(1.0, "Width of squiggle face (1): ");
				break;

			case SQ_DRAW:
				if (! GetCoord(1, &sqxcen, &sqycen, 1, 0, "Center of surface face (CURSOR): ")) break;
				sqangl = LexGetReal(0.0, "Angle (degrees): ");
				draw_squigg();
				break;

			case SQ_1DRAW:
				if (! GetCoord(1, &sqxcen, &sqycen, 1, 0, "Bottom left corner (CURSOR): ")) break;
				sqycen += sqylen/2.0f;
				if (squig == S_RIGHT) sqxcen += sqxlen;
				sqangl = (squig == S_RIGHT) ? 180.0f : 0.0f ;
				draw_squigg();
				break;

			case SQ_2DRAW:
				if (! GetCoord(1, xi, yi, 2, 0, "Coordinates of extremes (CURSOR): ")) break;
				sqxlen = (REAL) fabs(xi[1]-xi[0]);
				sqylen = (REAL) fabs(yi[1]-yi[0]);
				sqxcen = (squig == S_RIGHT) ? max(xi[0],xi[1]) : min(xi[0],xi[1]);
				sqycen = (yi[0]+yi[1])/2;
				sqangl = (squig == S_RIGHT) ? 180.0f : 0.0f ;
				draw_squigg();
				break;

			case SQ_3DRAW:
				if (! GetCoord(1, xi, yi, 3, 0, "Face corners (2 pts), back edge (CURSOR): ")) break;
				sqxcen = (xi[0]+xi[1])/2.0f;
				sqycen = (yi[0]+yi[1])/2.0f;
				sqylen = (REAL) sqrt( pow(xi[1]-xi[0],2)+pow(yi[1]-yi[0],2) );
				if (xi[0] != xi[1]) {
					sqangl = (REAL) (90.0+atan((yi[1]-yi[0])/(xi[1]-xi[0])) / 0.017453292519943);
				} else {
					sqangl = 0;
				}
				ss = sin(sqangl*0.017453292519943);
				cc = cos(sqangl*0.017453292519943);
				sqxlen = (REAL) (cc*(xi[2]-sqxcen) + ss*(yi[2]-sqycen));
				if (sqxlen < 0) {sqxlen = -sqxlen; sqangl += 180;}
				if (sqangl > 360) sqangl -= 360;
				if (sqangl < 0)   sqangl += 360;
				TTYprintf("Info: angle = %f degrees\n", sqangl);
				draw_squigg();
				break;

			case SQ_DOT:
			case SQ_WAVY:
			case SQ_SOLID:
				if (citem->rcode == SQ_DOT) {
					i = LexGetInt(1, "Type of line (1): ");
					if (LexEscape(TRUE)) break;
					PlotSetLineType(i, 0.0);
				}
				ss = sin(sqangl*0.017453292519943);
				cc = cos(sqangl*0.017453292519943);

				xdraw = LexGetReal(FLMAGIC, "Fraction from surface (CURSOR): ");
				if (LexEscape(TRUE)) break;
				if (xdraw == FLMAGIC) {
					PlotCursor(&xdraw, &ydraw, NULL);
					xdraw = (REAL) ((cc*(xdraw-sqxcen) + ss*(ydraw-sqycen)) / sqxlen);
				}

				ydraw = (REAL) (-0.5+wave*sin(TWOPI*wiggle*xdraw));
				PLOTMOVE(xdraw, ydraw, 3);
				if (citem->rcode == SQ_WAVY) {
					for (ytmp=0; ytmp<=1; ytmp+=1/100.0f) {
						xtmp = (REAL) (wave2*sin(cycles*TWOPI*ytmp));
						PLOTMOVE(xdraw+xtmp, ydraw+ytmp, 2);
					}
				} else {
					PLOTMOVE(xdraw, ydraw+1, 2);
				}
				PlotSetLineType(1, 0.0);
				PlotFlush();
				break;

			case SQ_LABEL:
				ss = sin(sqangl*0.017453292519943);
				cc = cos(sqangl*0.017453292519943);

				xdraw = LexGetReal(FLMAGIC, "Fraction from surface (CURSOR): ");
				if (LexEscape(TRUE)) break;
				if (xdraw == FLMAGIC) {
					PlotCursor(&xdraw, &ydraw, NULL);
					xdraw = (REAL) ((cc*(xdraw-sqxcen) + ss*(ydraw-sqycen)) / sqxlen);
				}
				ydraw = (REAL) (wave*sin(TWOPI*wiggle*xdraw));

				if (! LexGetToken(token, sizeof(token)))
					LexPromptStr(token, sizeof(token), "Layer ID: ");
				if (LexEscape(TRUE)) break;
				i = (int) strnblen(token);
				PLOTMOVE(xdraw, ydraw, 3);					/* Go there */
				PlotQueryPosn(&xdraw, &ydraw, NULL);	/* And query posn */
				
				xtmp = 0.5f*PlotQueryStringLength(ansize,token,0);
				ytmp = 0.5f*ansize;											/* 0=bottom, 1=center, 2=top	*/
				xdraw = (REAL) (xdraw + ytmp*cc + xtmp*ss);
				ydraw = (REAL) (ydraw - xtmp*cc + ytmp*ss);
				PlotString(xdraw, ydraw, ansize, token, sqangl+90.0f, 0);
				PlotFlush();
				break;

			case SQ_CLOSE:
				ss = sin(sqangl*0.017453292519943);
				cc = cos(sqangl*0.017453292519943);
				ydraw = (REAL) (wave*sin(TWOPI*wiggle));
				PLOTMOVE(1.00, ydraw+0.50, 3);
				PLOTMOVE(0.96, ydraw+0.25, 2);
				PLOTMOVE(1.04, ydraw+0.00, 2);
				PLOTMOVE(0.96, ydraw-0.25, 2);
				PLOTMOVE(1.0,  ydraw-0.50, 2);
				PlotFlush();
				break;

			case SQ_ROT:				/* ... Rotated sample for Horowitz */
				ERRprintf("ERROR: Has been removed.  Use DRAW with angle = 90.\n");
				LexFlush();
				break;

			case SQ_SAMPLE:
				break;

			case SQ_RETURN:
				goto ExitAll;

			default:
				TTYputs("This is a real screwup -- never come here (SQUIGG)\n");
				break;
		}
	}

ExitAll:
	PlotSetUserMode(LocalUsrSave);
	return(rcode);
}
