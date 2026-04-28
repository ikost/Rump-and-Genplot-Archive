/* IDS.F77 */

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
#include "lexp.h"								/* Uses LexGetOption */
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	FLMAGIC	-2187304.0f					/* Magic # (exact for float cmps) */

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

typedef enum _OPS1 {
	DO_HELP, DO_PLACE, DO_SKIP, DO_UNSKIP, DO_CLOSE, DO_LS, DO_RESET,
	DO_NOMARK, DO_NOSYM, DO_NOLINE, DO_MSYM,
	DO_LTYPE, DO_SYM, DO_PEN, DO_LW
} OPS1;

typedef struct _CMTYPE {								/* Command structure		*/
	const char *command;
	int minlen;
	OPS1 rcode;
} CMTYPE;

static CMTYPE cmlist[] = {
	{"-?",      2, DO_HELP},   {"-HELP",     2, DO_HELP},
	{"-PLACE",	3, DO_PLACE}, 
	{"-SKIP",	3, DO_SKIP},	{"-UNSKIP",	  4, DO_UNSKIP},
	{"-CLOSE",	3, DO_CLOSE},
	{"-LTYPE",  3, DO_LTYPE},	{"-LINETYPE", 3, DO_LTYPE},
	{"-SYMBOL", 3, DO_SYM},		{"-MARKER",   5, DO_SYM},
	{"-L&S",		4, DO_LS},
	{"-MSYMBOL",3, DO_MSYM},	{"-MSYMS",	  6, DO_MSYM},
	{"-PEN",    2, DO_PEN},    {"-COLOR",    4, DO_PEN},
	{"-LWIDTH", 3, DO_LW},     {"-LINEWIDTH", 6, DO_LW},
	{"-NOMARK", 4, DO_NOMARK}, {"-NOSYMBOL", 4, DO_NOSYM}, 
										{"-NOLINE",   4, DO_NOLINE},
	{"-RESET",  2, DO_RESET},
	{NULL,		0, DO_RESET} };


/* =============================================================================
--    SUBROUTINE IDS(LTYPE,SYMBOL,LINEWIDTH,TEXT)
--
-- Usage: LOGICAL PlotID(int ltype, int sym, int ipen, int lwidth, CHAR *id);
--
-- Inputs: ltype  - linetype of identifier (0 for symbols)
--         sym    - symbol (if ltype == 0)
--         ipen   - if > 0, specific pen color
--         lwidth - if > 0, specific line width
--         id     - text to draw
--
--    Output:   Graphics only
--
-- Special mode: If the char string is blank and the apparent option is not
--               recognized, then the first entry is taken as the string. 
--               This allows "identify "-5V" -nomark" to work.
============================================================================= */
LOGICAL PlotID(INTEGER ltype, INTEGER sym, INTEGER ipen, INTEGER lwidth, CHAR *id) {

	INTEGER	savek;									/* Save vector for CLIP */
	REAL		save[4];
	INTEGER	i,imark,lwidth_hold, ipen_hold;
	INTEGER  syms[20],nsyms=0;
	CHAR		token[LONG_STR_SIZE], *aptr, local_id[LONG_STR_SIZE];
	BOOL		logtmp,close_legend,UseCursor;
	REAL		x,y,xp,yp,lw,xstart;					/* Coordinate for plotting */
	REAL		OldPatSize;
	CMTYPE *citem;										/* Item found in search */

/* -- Code begin -- */
	syms[0] = sym; nsyms = 1;

	if (strcmp(id, "**RESET**") == 0) {				/* Reset plot after erase etc. */
		PlotWindow->ID.yp = PlotWindow->ysize - PlotWindow->ymarg[1] - PlotWindow->ID.topskip;	/* Start down from top */
		PlotWindow->ID.len_max = 0.0f;
		return(TRUE);
	}

	close_legend = FALSE;
	xp = PlotWindow->xmarg[0] + PlotWindow->ID.leftskip;	/* And set the X value */

	imark = 0;														/* Identify nothing */
	while (LexGetOption(token, sizeof(token))) {			/* Any tokens to check */
		if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) == NULL) {
			if (*id == '\0') {									/* We haven't specified a string yet */
				strcpy(local_id, token);
				id = local_id;
			} else {
				ERRprintf("ERROR: %s unrecognized option\n", token);
				return(FALSE);
			}
		} else switch (citem->rcode) {
			case DO_HELP:
				TTYprintf(
                      " Usage: identify [text] [options]\n"
                      "\n"
                      "    -PLACE { / | <x> <y> }    - Starting legend position in inches (cursor)\n"
                      "    -SKIP                     - Skip one space before the legend entry\n"
                      "    -CLOSE                    - Close the legend box after this entry\n"
                      "    -L&S                      - Do both lines and symbols on legend\n"
                      "    -SYMbol <sym>             - Override and use this symbol type\n"
							 "    -MSYMbol <sym1> <sym2> /  - Multiple symbols on single line\n"
                      "    -LType  <lt>              - Override and use this linetype\n"
                      "    -PEN    <color>           - Override and use this color\n"
                      "    -LWidth <linewidth>       - Override and use this linewidth\n"
							 "    -NOMARK -NOSYM -NOLINE    - Draw text only, or turn off each\n"
                      "    -RESET                    - Reset to top of legend on new plot (internal)\n"
                      "\n");
				return(TRUE);

			case DO_PLACE:							/* New placement for start */
				xp = LexGetReal(FLMAGIC,"Legend starting posn in inches (cursor)");
				UseCursor = FALSE;
				if (xp == FLMAGIC) {
					UseCursor = TRUE;
					logtmp = PlotSetUserMode(FALSE);			/* Go into inches mode only */
					PlotCursor(&xp,&yp,NULL);
					PlotSetUserMode(logtmp);					/* Back to what it was */
				} else {
					yp = LexGetReal(PlotWindow->ID.yp,"And Y posn: ");
				}
				PlotWindow->ID.yp       = yp;					/* Copies straight over */
				PlotWindow->ID.leftskip = xp - PlotWindow->xmarg[0];		/* Correct to screwy units */
				PlotWindow->ID.topskip  = PlotWindow->ysize - PlotWindow->ymarg[1] - PlotWindow->ID.yp;
				if (UseCursor) TTYprintf("Legend start position set: %g %g\n", xp, yp);
				break;

			case DO_SKIP:											/* Skip a line position */
				PlotWindow->ID.yp -=  PlotWindow->ID.spacing*PlotWindow->ID.size;
				break;

			case DO_UNSKIP:										/* Un-skip a line position */
				PlotWindow->ID.yp +=  PlotWindow->ID.spacing*PlotWindow->ID.size;
				break;

			case DO_CLOSE:											/* Close */
				close_legend = TRUE;
				break;
			case DO_RESET:
				PlotWindow->ID.yp = PlotWindow->ysize - PlotWindow->ymarg[1] - PlotWindow->ID.topskip;	/* Start down from top */
				PlotWindow->ID.len_max = 0.0f;
				return(TRUE);

			case DO_PEN:
				if (LexGetMathP(token, sizeof(token), "Pen color: ")) 
					ipen = PlotMatchColor(token, ipen);
				break;
			case DO_LW:
				lw = LexGetReal(-1.0, "Line width: ");
				lwidth = (lw > 0) ? max(1,nint(7*lw)) : -1 ;
				break;

			case DO_MSYM:
				for (i=0; i<20; i++) {
					syms[i] = -1;
					if (! LexGetTokenP(token, sizeof(token), "Symbol number (end): ")) break;
					if (strcmp(token, "/") == 0) break;
					syms[i] = PlotMatchSymbol(token, 1);
				}
				nsyms = i;
				imark |= 0x0002;
				break;
				
			case DO_SYM:
				if (LexGetTokenP(token, sizeof(token), "Symbol number: "))
					syms[0] = PlotMatchSymbol(token, syms[0]);
				nsyms = 1;
				imark |= 0x0002;
				break;
			case DO_LTYPE:
				imark |= 0x0001;
				ltype = LexGetInt(ltype, "Line type: ");
				break;
			case DO_LS:												/* Put both marks on plot */
				imark |= 0x0003;
				break;
			case DO_NOMARK:										/* Don't put mark on plot */
				imark |= 0x00F0;
				break;
			case DO_NOSYM:
				imark |= 0x0020;									/* Disables symbol */
				break;
			case DO_NOLINE:
				imark |= 0x0010;									/* Disables symbol */
				break;
		}
	}

/* If no explicit selection of line or symbol, choose default */
	if ( (imark & 0x000F) == 0) imark |= (ltype==0 && sym!=0) ? 0x0002 : 0x0001;
	if (imark & 0x0010) imark &= 0xFFFE;
	if (imark & 0x0020) imark &= 0xFFFD;

/* ========================================================================== */
	logtmp = PlotSetUserMode(FALSE);					/* Go into inches mode only */
	savek  = -1;											/* Clear clipping mode */
	PlotSetClip(&savek,save);							/* Query current CLIP mode */
	i = 1;
	PlotSetClip(&i,save);								/* Set maximum size */

	if (id != NULL && *id != '\0') {					/* Something to draw? */

/* Select pen and move down to first position */
		if (ipen > 0) ipen_hold = PlotSelectPen(ipen);
		PlotWindow->ID.yp -= PlotWindow->ID.spacing*PlotWindow->ID.size;							/* Next position */

/* ... Do the symbols/lines first, using specified linewidth if necessary */
		if (lwidth > 0) lwidth_hold = PlotSetLineWidth(lwidth);

/* ... Give example of symbol */
		if (imark & 0x01) {									/* Draw line? */
			PlotQueryLineType(NULL, &OldPatSize);
			if (ltype == 0) {									/* Get the linetype set */
				PlotSetLineType(4, PlotWindow->ID.vecsize);
			} else {
				PlotSetLineType(abs(ltype), PlotWindow->ID.vecsize);
			}
/* ... Give example of linetype */
			if (imark & 0x02) {								/* Split if doing sym also */
				PlotMove(xp+PlotWindow->ID.linesize/2-PlotWindow->ID.size/2,
							PlotWindow->ID.yp+PlotWindow->ID.size/2, 3);
				PlotMove(xp, PlotWindow->ID.yp+PlotWindow->ID.size/2, 2);
				PlotMove(xp+PlotWindow->ID.linesize, PlotWindow->ID.yp+PlotWindow->ID.size/2, 3);	/* Give example of linetype */
				PlotMove(xp+PlotWindow->ID.linesize/2+PlotWindow->ID.size/2,
							PlotWindow->ID.yp+PlotWindow->ID.size/2, 2);
			} else {
				PlotMove(xp, PlotWindow->ID.yp+PlotWindow->ID.size/2, 3);
				PlotMove(xp+PlotWindow->ID.linesize, PlotWindow->ID.yp+PlotWindow->ID.size/2, 2);
			}
			PlotSetLineType(1, OldPatSize);				/* Reset ltype to 1 (normal) */
		}
		if (imark & 0x02) {
			xstart = xp + PlotWindow->ID.linesize/2.0f - (nsyms-1)/2.0f*1.2f*PlotWindow->ID.size;
			for (i=0; i<nsyms; i++) {
				PlotSymbol(xstart, PlotWindow->ID.yp+PlotWindow->ID.size/2, 1.4f*PlotWindow->ID.size,(CHAR) abs(syms[i]));
				xstart += 1.2f*PlotWindow->ID.size;
			}
		}

		if (lwidth > 0) PlotSetLineWidth(lwidth_hold);	/* Reset Linewidth */

/* ... Now do the ID, multiple lines if necessary */
		xp += PlotWindow->ID.size/2;				/* Shift everything by spacing */
		while (*id != '\0') {
			for (aptr=token; *id!='\0'; id++) {
				if (*id != '\\') {
					*(aptr++) = *id;
				} else if (id[1] == '\\') {
					*(aptr++) = *(id++);
				} else if (id[1] != 'n') {
					*(aptr++) = *id;
				} else {
					id += 2;
					break;
				}
			}
			*aptr = '\0';
			PlotWindow->ID.len_max = max(PlotWindow->ID.len_max, PlotQueryStringLength(PlotWindow->ID.size,token,0)+PlotWindow->ID.linesize+0.05f);
			PlotString(xp+PlotWindow->ID.linesize+0.05f,PlotWindow->ID.yp,PlotWindow->ID.size,token,0.0f,0);
			if (*id != '\0') PlotWindow->ID.yp -= PlotWindow->ID.spacing*PlotWindow->ID.size;							/* Next position */
		}

/* And restore the pen if necessary */
		if (ipen   > 0) PlotSelectPen(ipen_hold);
	}

	if (close_legend) {
		int parms[2];
		PlotSystem(6, NULL, parms);
		if (parms[0] < 0) PlotSelectPen(1);					/* Switch to black if autocolors */
		PlotSetLineType(1, 0.0f);								/* Reset ltype to 1 (normal) */
		x = PlotWindow->xmarg[0]+PlotWindow->ID.leftskip;	/* Starting point */
		y = PlotWindow->ysize-PlotWindow->ymarg[1]-PlotWindow->ID.topskip;	/* Start down from top */
		PlotWindow->ID.yp -= PlotWindow->ID.size/2;								/* Shift down for next one */
		PlotMove(x,y,3);											/* Starting point */
		PlotMove(x+PlotWindow->ID.len_max+PlotWindow->ID.size,y,2);
		PlotMove(x+PlotWindow->ID.len_max+PlotWindow->ID.size,PlotWindow->ID.yp,2);
		PlotMove(x,PlotWindow->ID.yp,2);
		PlotMove(x,y,2);
	}

	PlotSetClip(&savek,save);							/* Return status */
	if (DEVICE->Autoflush) PlotFlush();				/* Clear all buffers */
	PlotSetUserMode(logtmp);							/* Turn back on if necessary */
	return(TRUE);
}
