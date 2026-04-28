/* FSETUP.F77 */

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
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#define	mkx(x)	((char )((x) ? 'X' : ' '))
#define	rdx(x)	(((x)!=' ' && (x)!='n' && (x)!='N') ? TRUE : FALSE)
#define	InsCursor(mode)

typedef struct _VAR {
	int row,col;
	int length;
	char *str;
} VAR;

typedef struct _DSP {
	int row,col;
	char *str;
} DSP;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL fcheck(int ip, char *str, int *n1, int *n2);
PRIVATE LOGICAL ScreenData(DSP *Dsp, VAR *Val, int *myip, 
									LOGICAL (*check)(int ip, char *str, int *n1, int *n2));
PRIVATE void PutData(VAR *ValPtr, int attrib);
PRIVATE void PutModData(VAR *ValPtr, int attrib);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
LABELS:
 Bottom:   _________________________________________________________________
 Left:     _________________________________________________________________
 Top:      _________________________________________________________________
 Right:    _________________________________________________________________

                                   Auto    Log           Status         Plot
            Minimum     Maximum    Scale   Mode   Off   On  Copy User   Using
Bottom:    __________  __________    _      _           X                 _
Left:      __________  __________    _      _           X                 _
Top:       __________  __________    _      _      _    _    _    _       _
Right:     __________  __________    _      _      _    _    _    _       _

PEn: ___    LType: __    SYMbol: __    SYMSIZ: ______    NPOint: __

Force Scales: _    Auto-axis: _     Auto-IDS: _     Box-Mode: _
Minor Ticks:  _    Ticks In:  _    
---------------------------------------------------------------------------- */

PRIVATE char	title[4][66], minimum[4][11], maximum[4][11], autoscale[4], 
					logmode[4], isactive[4], status[2][4],
					PenColor[4], LineType[3], SymbolType[4], SymbolSize[6], 
					Npoint[4];
PRIVATE char	Force, AutoAxes, AutoIDs, BoxMode, MinorTick, InTicks;

VAR FillIn[] = {
		{ 2,13,  65, title[0]},						/* Four titles (0) */
		{ 3,13,  65, title[1]},
		{ 4,13,  65, title[2]},
		{ 5,13,  65, title[3]},
		{ 9,13,  10, minimum[0]},					/* Minimum (4) */
		{ 9,25,  10, maximum[0]},					/* Maximum */
		{ 9,39,   1, &autoscale[0]},				/* Autoscale */
		{ 9,46,   1, &logmode[0]},					/* Log Mode */
		{ 9,76,   1, &isactive[0]},				/* Active */

		{10,13,  10, minimum[1]},					/* Minimum (9) */
		{10,25,  10, maximum[1]},					/* Maximum */
		{10,39,   1, &autoscale[1]},				/* Autoscale */
		{10,46,   1, &logmode[1]},					/* Log Mode */
		{10,76,   1, &isactive[1]},				/* Active */

		{11,13,  10, minimum[2]},					/* Minimum (14) */
		{11,25,  10, maximum[2]},					/* Maximum */
		{11,39,   1, &autoscale[2]},				/* Autoscale */
		{11,46,   1, &logmode[2]},					/* Log Mode */
		{11,53,	 1, &status[0][0]},				/* Status */
		{11,58,	 1, &status[0][1]},				/* Status */
		{11,63,	 1, &status[0][2]},				/* Status */
		{11,68,	 1, &status[0][3]},				/* Status */
		{11,76,   1, &isactive[2]},				/* Active */

		{12,13,  10, minimum[3]},					/* Minimum (23) */
		{12,25,  10, maximum[3]},					/* Maximum */
		{12,39,   1, &autoscale[3]},				/* Autoscale */
		{12,46,   1, &logmode[3]},					/* Log Mode */
		{12,53,	 1, &status[1][0]},				/* Status */
		{12,58,	 1, &status[1][1]},				/* Status */
		{12,63,	 1, &status[1][2]},				/* Status */
		{12,68,	 1, &status[1][3]},				/* Status */
		{12,76,   1, &isactive[3]},				/* Active */

		{14,07,   3, PenColor},						/* Pen color  (32) */
		{14,21,   2, LineType},						/* LTYPE */
		{14,35,   3, SymbolType},					/* Symbol */
		{14,49,   5, SymbolSize},					/* Symsiz */
		{14,67,   3, Npoint},						/* NPOINT  (36) */

		{16,16,   1, &Force},						/* Force  (37) */
		{16,32,   1, &AutoAxes},					/* Autoaxis */
		{16,48,   1, &AutoIDs},						/* Auto-ids */
		{16,64,   1, &BoxMode},						/* Box-Mode */
		{17,16,   1, &MinorTick},					/* Minor-ticks */
		{17,32,   1, &InTicks},						/* Ticks-in (42) */
		{0,0,0, NULL} };


/* ===========================================================================

=========================================================================== */
int gpt_do_setup(void) {

	char token[80];
	int	 i, mylt, mysym, mypen[2];
	static int ip=0;						/* Position to start at */

	DSP Display[] = {
		{1,2, "LABELS: "},
		{2,3,  "Bottom: "},	
		{3,3,  "Left: "},
		{4,3,  "Top: "},
		{5,3,  "Right: "},

		{7,37,                                   "Auto    Log           Status         Plot"},
		{8,14,            "Minimum     Maximum    Scale   Mode   Off   On  Copy User   Using"},
		{9,3,  "Bottom:                                                X"},
		{10,3, "Left:                                                  X"},
		{11,3, "Top: "},
		{12,3, "Right: "},

		{14,2,"PEn:        LType:       SYMbol:       SYMSIZ:           NPOint: "},
		{16,2,"Force Scales:      Auto-axis:       Auto-IDS:       Box-Mode: "},
		{17,2,"Minor Ticks:  _    Ticks In:  _"},
		{18,2,"===== <ESC> - exit ============================= <^G> - abort ====="},
		{0,0, NULL} };

/* ... Copy current values into the work buffers */
	for (i=0; i<4; i++) {
		strscpy(title[i], Gpt->titles[i], sizeof(title[i]));
		sprintf(minimum[i], "%g", Gpt->rmins[i]);
		sprintf(maximum[i], "%g", Gpt->rmaxs[i]);
		autoscale[i] = mkx(Gpt->AutoFlag & (1<<i));
		logmode[i]   = mkx(Gpt->logtype[i]);
	}
	isactive[0] = mkx(Gpt->plx == 0);
	isactive[2] = mkx(Gpt->plx == 1);
	isactive[1] = mkx(Gpt->ply == 0);
	isactive[3] = mkx(Gpt->ply == 1);

	status[0][0] = mkx(Gpt->xtop == IS_OFF);
	status[0][1] = mkx(Gpt->xtop == IS_ON);
	status[0][2] = mkx(Gpt->xtop == IS_COPY);
	status[0][3] = mkx(Gpt->xtop == IS_NONLINEAR);

	status[1][0] = mkx(Gpt->yright == IS_OFF);
	status[1][1] = mkx(Gpt->yright == IS_ON);
	status[1][2] = mkx(Gpt->yright == IS_COPY);
	status[1][3] = mkx(Gpt->yright == IS_NONLINEAR);

	mylt  = (! Gpt->AutoLineType) ? Gpt->linetypestart : -( (Gpt->linetypestart%7) + 1) ;
	mysym = (! Gpt->AutoSymbols)  ? Gpt->symtypestart  : -( (Gpt->symtypestart%13) + 1) ;

	PlotSystem(6, NULL, mypen);						/* Query pen color & number */
	sprintf(PenColor,   "%3i",   mypen[0]);
	sprintf(LineType,   "%2i",   mylt);
	sprintf(SymbolType, "%3i",   mysym);
	sprintf(SymbolSize, "%5.3f", Gpt->symsiz);
	sprintf(Npoint,     "%3i",   Gpt->npoint);

	Force     = mkx( (Gpt->ForceRegions != 0) );
	AutoAxes  = mkx(Gpt->AutoAxes);
	AutoIDs   = mkx(Gpt->AutoIDs);
	BoxMode   = mkx(Gpt->BoxMode);
	MinorTick = mkx(Gpt->MinorTicks);
	InTicks   = mkx(Gpt->InTicks);

/* ... This is all setup, now call to routine to allow the input as wanted */
	if (! ScreenData(Display, FillIn, &ip, fcheck)) return(OKAY);

	Gpt->AutoFlag = 0;
	for (i=0; i<4; i++) {
		strcpy(Gpt->titles[i], title[i]);
		Gpt->rmins[i] = (REAL) atof(minimum[i]);
		Gpt->rmaxs[i] = (REAL) atof(maximum[i]);
		if (rdx(autoscale[i])) Gpt->AutoFlag |= (1<<i);
		Gpt->logtype[i] = rdx(logmode[i]);
	}
	Gpt->plx = rdx(isactive[0]) ? 0 : 1;
	Gpt->ply = rdx(isactive[1]) ? 0 : 1;
	if (rdx(status[0][0])) 
		Gpt->xtop = IS_OFF;
	else if (rdx(status[0][1]))
		Gpt->xtop = IS_ON;
	else if (rdx(status[0][2]))
		Gpt->xtop = IS_COPY;
	else if (rdx(status[0][3]))
		Gpt->xtop = IS_NONLINEAR;
	if (rdx(status[1][0])) 
		Gpt->yright = IS_OFF;
	else if (rdx(status[1][1]))
		Gpt->yright = IS_ON;
	else if (rdx(status[1][2]))
		Gpt->yright = IS_COPY;
	else if (rdx(status[1][3]))
		Gpt->yright = IS_NONLINEAR;
			
	Gpt->symsiz       = (REAL) atof(SymbolSize);
	Gpt->npoint       = (int)  atol(Npoint);
	Gpt->ForceRegions = (rdx(Force)) ? -1 : 0 ;
	Gpt->AutoAxes     = rdx(AutoAxes);
	Gpt->AutoIDs      = rdx(AutoIDs);
	Gpt->BoxMode      = rdx(BoxMode);

/* ... More complex changes can be handled by the main processor */
	if (atol(PenColor) != mypen[0]) {
		sprintf(token, "PEN %s", PenColor); 
		LexInsText(token);
	}
	if (atol(LineType) != mylt) {
		sprintf(token, "ltype %s", LineType);
		LexInsText(token);
	}
	if (atol(SymbolType) != mylt) {
		sprintf(token, "ltype %s", SymbolType);
		LexInsText(token);
	}
	if (rdx(MinorTick) != Gpt->MinorTicks) {
		sprintf(token, "subticks %s", rdx(MinorTick) ? "ON" : "OFF");
		LexInsText(token);
	}
	if (rdx(InTicks) != Gpt->InTicks) {
		sprintf(token, "inticks %s", rdx(InTicks) ? "ON" : "OFF");
		LexInsText(token);
	}
	return(OKAY);
}

/* ========================================================================= */
PRIVATE LOGICAL fcheck(int ip, char *str, int *n1, int *n2) {

	LOGICAL lval;
	int i;

	lval = rdx(*str);
	*n1 = ip;									/* Default assumption */
	*n2 = -1;									/* Assume only 1 */

	switch (ip) {
		case 8:									/* PLX Bottom or Top */
		case 22:
			if (ip == 22) lval = !lval;
			isactive[0] = mkx(lval);
			isactive[2] = mkx(!lval);
			*n1 = 8; *n2 = 22;
			return(TRUE);
		case 13:									/* PLY left or right */
		case 31:
			if (ip == 31) lval = !lval;
			isactive[1] = mkx(lval);
			isactive[3] = mkx(!lval);
			*n1 = 13; *n2 = 31;
			return(TRUE);

		case 7:									/* Log mode for all axes */
		case 12:
		case 17:
		case 26:
			*str = mkx(lval);
			return(TRUE);

		case 4:									/* Setting range ==> dump autoscale */
		case 5:
			if (autoscale[0] == ' ') return(FALSE);
			autoscale[0] = ' ';      
			*n1 = 6;								/* Redraw autoscaling */
			return(TRUE);
		case 6:
			if (! lval) {*str = ' '; return(FALSE);}
			sprintf(minimum[0], "%g", Gpt->rmins[0]);
			sprintf(maximum[0], "%g", Gpt->rmaxs[0]);
			*n1 = -4; *n2 = -6;
			*str = 'X';
			return(TRUE);
		case 9:									/* Setting range ==> dump autoscale */
		case 10:
			if (autoscale[1] == ' ') return(FALSE);
			autoscale[1] = ' ';
			*n1 = 11;							/* Redraw autoscaling */
			return(TRUE);
		case 11:
			if (! lval) {*str = ' '; return(FALSE);}
			sprintf(minimum[1], "%g", Gpt->rmins[1]);
			sprintf(maximum[1], "%g", Gpt->rmaxs[1]);
			*n1 = -9; *n2 = -11;
			*str = 'X';
			return(TRUE);
		case 14:									/* Setting range ==> dump autoscale */
		case 15:
			if (autoscale[2] == ' ') return(FALSE);
			autoscale[2] = ' ';
			*n1 = 16;							/* Redraw autoscaling */
			return(TRUE);
		case 16:
			if (! lval) {*str = ' '; return(FALSE);}
			sprintf(minimum[2], "%g", Gpt->rmins[2]);
			sprintf(maximum[2], "%g", Gpt->rmaxs[2]);
			*n1 = -14; *n2 = -16;
			*str = 'X';
			return(TRUE);
		case 23:									/* Setting range ==> dump autoscale */
		case 24:
			if (autoscale[3] == ' ') return(FALSE);
			autoscale[3] = ' ';
			*n1 = 25;							/* Redraw autoscaling */
			return(TRUE);
		case 25:
			if (! lval) {*str = ' '; return(FALSE);}
			sprintf(minimum[3], "%g", Gpt->rmins[3]);
			sprintf(maximum[3], "%g", Gpt->rmaxs[3]);
			*n1 = -23; *n2 = -25;
			*str = 'X';
			return(TRUE);
			
		case 18:									/* XTOP mode change */
		case 19:
		case 20:
		case 21:
			*n1 = -18;
			*n2 = -21;
			if (lval) {										/* One set, erase others */
				for (i=0; i<4; i++) status[0][i] = mkx(i==(ip-18));
			} else {
				for (i=0; i<4; i++) if (rdx(status[0][i])) return(FALSE);
				if (ip != 18) 
					status[0][0] = 'X';
				else
					status[0][1] = 'X';
			}
			return(TRUE);

		case 27:									/* YRIGHT mode change */
		case 28:
		case 29:
		case 30:
			*n1 = -27;
			*n2 = -30;
			if (lval) {										/* One set, erase others */
				for (i=0; i<4; i++) status[1][i] = mkx(i==(ip-27));
			} else {
				for (i=0; i<4; i++) if (rdx(status[1][i])) return(FALSE);
				if (ip != 27) 
					status[1][0] = 'X';
				else
					status[1][1] = 'X';
			}
			return(TRUE);

		case 37:
		case 38:
		case 39:
		case 40:
		case 41:
		case 42:
			*str = mkx(lval);
			return(TRUE);

		default:
         return(FALSE);
	}
}

/* ===========================================================================
--     Subroutine to handle the screen functions.  Quite simple, but not
--     necessarily nice.
--
--     Usage:  LOGICAL scr$data(I$P,I$T,N1,D$P,D$T,N2,IP,CHECK)
--
--     Inputs: I$P - (2xn1) array containg row/col position of info text
--	            I$T - text array for information
--	            N1  - Number of information elements
--	            D$P - (3xn2) array containing row/col/length for input info
--	            D$T - input and output array of text
--	            N2  - number of inputtable elements
--	            IP  - Element within the changable place to start
--	            CHECK - External logical function which is called for each char!
--
--     Output: D$T - Modified as necessary
-- 	         IP  - Last element we were engaged on
--	            scr$data - .TRUE.  => Exit with return (Mac-okay)
--			                 .FALSE. => Exit with <ESC>  (Mac-cancel)
============================================================================ */
#define	CTRL_A			0x01
#define	CTRL_B			0x02
#define	CTRL_C			0x03
#define	CTRL_D			0x04
#define	CTRL_E			0x05
#define	CTRL_F			0x06
#define	CTRL_K			0x0B
#define	CTRL_L			0x0C
#define	CTRL_N			0x0E
#define	CTRL_P			0x10
#define	CTRL_Z			0x1A
#define	ESC				0x1B
#define	ABORT				0x07
#define	BACKSPACE		0x08
#define	TAB				0x09

#define	TEXT				0x11									/* Text color */
#define	DATA				0x13									/* Data color */
#define	MY_LINE			0x14									/* Default line */
#define	MY_CHAR			0x18									/* And char color */
#define	LastRow			24

PRIVATE LOGICAL ScreenData(DSP *Dsp, VAR *Val, int *myip, 
			LOGICAL (*check)(int ip, char *str, int *n1, int *n2)) {

	int	i,j,k,ii,jj, rowmax;
	int	ip,ipl,											/* Current,Last IP value */
			ic,icwant,LastCol,							/* Current column in field */
			irs,ics,ils,									/* Current row, column, length */
			NumVals,ival;
	LOGICAL	insmode,										/* Are we in insert mode */
				StringHasChanged,							/* Has string changed */
				l2;
	VAR *ValPtr;
	DSP *DspPtr;
	char tmp[2];

/* ----------------------------------------
-- ... Draw the screen
---------------------------------------- */
	ip = *myip;

ReDraw:
	icwant = LastCol = ic = 0;							/* Reset these values */
	ipl = -1;												/* Nothing displayed	*/
	l2  = FALSE;											/* And not "unmoved" */

	ScrClearAttrib(TEXT);									/* Clear screen */
	insmode = FALSE;
	InsCursor(insmode);										/* Turn off insert cursor */

	DspPtr = Dsp;
	rowmax = 0;
	while (DspPtr->str != NULL) {
		rowmax = max(rowmax, DspPtr->row);
		DspPtr++;
	}
	ValPtr = Val;
	NumVals = 0;
	while (ValPtr->str != NULL) {
		NumVals++;
		rowmax = max(rowmax, ValPtr->row);
		ValPtr++;
	}

/*	ScrEraseRegionAttrib(0, 1, imax, TEXT); */		/* Erase the region */

	DspPtr = (DSP *) Dsp;
	while (DspPtr->str != NULL) {
		ScrPutString(DspPtr->row, DspPtr->col, DspPtr->str, TEXT);
		DspPtr++;
	}

	ValPtr = Val;
	while (ValPtr->str != NULL) PutData(ValPtr++, DATA);


/* ----------------------------------------
-- ... Start reading from the Keyboard
---------------------------------------- */
	while (TRUE) {
		if (ip != ipl) {									/* Have we moved? */
			if (ipl >= 0) PutData(ValPtr, DATA);
			ValPtr = Val+ip;								/* And the actual address */
			irs = ValPtr->row;							/* Starting row */
			ics = ValPtr->col;							/* Starting column */
			ils = ValPtr->length;
			PutData(ValPtr, MY_LINE);
			ipl = ip;										/* Keep last one */
		}
		if (ic >= ils) ic = 0;							/* Bring pointer back */
      StringHasChanged = FALSE;						/* String unmodified */
		if (l2) LastCol = ics+ic;
      l2  = TRUE;											/* Moved! */

		if (ic <= (int) strlen(ValPtr->str)) {		/* Any valid char there */
			tmp[0] = ValPtr->str[ic]; 
		} else {
			tmp[0] = ' ';
		}
		tmp[1] = '\0';
		ScrPutString(irs, ics+ic, tmp, MY_CHAR);		/* Single character */
		ScrSetPosn(irs, ics+ic, D_BOLD);					/* Put cursor there */
		ival = CONgetc();										/* Get a character */
		ScrPutString(irs, ics+ic, tmp, MY_LINE);		/* Single character */

		switch (ival) {
			case CTRL_L:											/* ^L => Repaint */
				goto ReDraw;
			case VIRTUAL_HOME:								/* Go home? */
				ip = 0; ic = 0;
				break;
			case VIRTUAL_INSERT:								/* Insert key */
				insmode = ! insmode;
				InsCursor(insmode);
				break;
			case VIRTUAL_TAB:									/* Forward field */
			case TAB:
			case '\n':
			case '\r':
				if (++ip >= NumVals) ip = 0;
				ic = 0;
				break;
			case VIRTUAL_BACKTAB:
				if (--ip < 0) ip = NumVals-1;
				ic = 0;
				break;
			case CTRL_A:
				ic = 0;
				break;
			case CTRL_E:
				ic = (int) strlen(ValPtr->str);
				break;
			case VIRTUAL_LEFT:
			case CTRL_B:
				if (ic) 
					ic--;
				else {
					if (--ip < 0) ip = NumVals-1;
					ic = Val[ip].length-1;
				}
				break;
			case CTRL_F:
			case VIRTUAL_RIGHT:
				if (++ic >= ils) {
					ic = 0;
					if (++ip >= NumVals) ip=0;
				}
				break;
			case CTRL_D:
			case VIRTUAL_DELETE:
				if (ic < (int)strlen(ValPtr->str)) {
					for (i=ic; i<ValPtr->length; i++) ValPtr->str[i] = ValPtr->str[i+1];
					StringHasChanged = TRUE;
				}
				break;
			case BACKSPACE:
				if (ic > 0) {
					if (ic-- <= (int) strlen(ValPtr->str)) {
						for (i=ic; i<ValPtr->length; i++) ValPtr->str[i] = ValPtr->str[i+1];
						StringHasChanged = TRUE;
					}
				}
				break;
			case CTRL_K:									/* ^K => Blank remainder line */
				if (ic < (int) strlen(ValPtr->str)) {
					ValPtr->str[ic] = '\0';
					StringHasChanged = TRUE;
				}
				break;
			case ESC:
			case ABORT:
			case CTRL_C:
				PutData(ValPtr, DATA);
				ScrSetPosn(min(LastRow,rowmax+1), 1, D_NORMAL);
				InsCursor(FALSE);
				if (ival != ESC) return(FALSE);
				*myip = ip;
				return(TRUE);

			case VIRTUAL_UP:
			case CTRL_P:
			case CTRL_Z:
				while (ip && Val[ip-1].row == Val[ip].row) ip--; /* Get to next row */
				if (--ip < 0) ip = NumVals-1;
				while (ip && Val[ip-1].row == Val[ip].row && Val[ip].col > LastCol) ip--;
				ic = icwant;
				l2 = FALSE;											/* Not moved! */
				break;

			case VIRTUAL_DOWN:
			case CTRL_N:
				while (Val[ip+1].row == Val[ip].row) ip++;
				if (++ip >= NumVals) ip=0;
				while (Val[ip+1].row == Val[ip].row && Val[ip+1].col <= LastCol) ip++;
				ic = icwant;
				l2 = FALSE;											/* Not moved! */
				break;

			default:
				if (ival < ' ' || ival > 0x7F) {
					RingBell();
				} else {
					if (ic > (int) strlen(ValPtr->str)) {		/* Blank fill */
						for (i = (int) strlen(ValPtr->str); i<ic; i++) ValPtr->str[i] = ' ';
						ValPtr->str[ic] = '\0';
					}
					if (insmode) {
						for (i=ValPtr->length-1; i>ic; i--) ValPtr->str[i] = ValPtr->str[i-1];
						ValPtr->str[ic] = (char) ival;
						if (ic < ValPtr->length-1) ic++;
					} else {
						if (ValPtr->str[ic] == '\0') ValPtr->str[ic+1] = '\0';
						ValPtr->str[ic] = (char) ival;
						if (ic < ValPtr->length-1) ic++;
					}
					StringHasChanged = TRUE;
				}
		}
		if (StringHasChanged) PutModData(ValPtr, MY_LINE);

		icwant = ic;
		i = j = -1;
      if (StringHasChanged && (*check)(ip, ValPtr->str, &i, &j)) {
			if (i >= 0 && j!=i) {
				k = (i==ip) ? MY_LINE : DATA;
				PutData(&Val[i], k);
			}
			if (j >= 0) {
				k = (j==ip) ? MY_LINE : DATA;
				PutData(&Val[j], k);
			}
			if (i <= 0 && j < 0 && j!=i) {				/* Multiple entries */
				ii = min(-i,-j); jj = max(-i,-j);
				for (i=ii; i<=jj; i++) {
					k = (j==ip) ? MY_LINE : DATA;
					PutData(&Val[i], k);
				}
			}
		}
	}
	panic; return(FALSE);						/* BETTER NOT HAPPEN! */
}

/* ===========================================================================
-- Routine to output the data to screen -- uses full defined length
=========================================================================== */
PRIVATE char LastOutput[80];

PRIVATE void PutData(VAR *ValPtr, int attrib) {

	char *iptr, *optr;

	memset(LastOutput,' ',80); 
	LastOutput[ValPtr->length] = '\0';
	iptr = ValPtr->str; optr = LastOutput;
	if (ValPtr->length == 1 && *iptr) {
		*(optr++) = *iptr; *optr = '\0';
	} else  {
		while (*iptr) *(optr++) = *(iptr++);
	}
	ScrPutString(ValPtr->row, ValPtr->col, LastOutput, attrib);
	return;
}


/* ===========================================================================
-- Routine to output changes to data to screen -- uses full defined length
=========================================================================== */
PRIVATE void PutModData(VAR *ValPtr, int attrib) {

	char token[80], *iptr, *optr, *aptr;
	int i;
	
	memset(token,' ',80); 
	token[ValPtr->length] = '\0';
	iptr = ValPtr->str; optr = token;
	if (ValPtr->length == 1 && *iptr) {
		*(optr++) = *iptr; *optr = '\0';
	} else  {
		while (*iptr) *(optr++) = *(iptr++);
	}
/* ... Okay, now determine where the strings start to differ */
	optr = token; iptr = LastOutput; i=0;
	while (*optr && (*optr == *iptr)) {i++; optr++; iptr++;}
	if (! *optr) return;
	aptr = token+ValPtr->length; iptr = LastOutput+ValPtr->length;
	while (*aptr == *iptr) {aptr--; iptr--;}
	strcpy(LastOutput, token);
	*(++aptr) = '\0';
	ScrPutString(ValPtr->row, ValPtr->col+i, optr, attrib);
	return;
}
