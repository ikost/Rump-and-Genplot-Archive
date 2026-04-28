/* save.c */

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
#include <unistd.h>

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
#define	panic					SysPanic(__FILE__, __LINE__)

typedef struct _SAVE SVTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
void sv_undo(void);					/* Only HCOPY and LEXP know this prototype */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE void svout(SVTYPE *SV);
PRIVATE void svflush(SVTYPE *SV);
PRIVATE LOGICAL svcsav(SVTYPE *SV);
PRIVATE LOGICAL svcrst(SVTYPE *SV);

PRIVATE int   svgetcmd(SVTYPE *SV);
PRIVATE int   svgetint(SVTYPE *SV);
PRIVATE REAL  svgetreal(SVTYPE *SV);
PRIVATE char *svgetstr(SVTYPE *SV, char *buf);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Routine: int SV_Request(KEY,UNIT,CODE)
--
-- Purpose: A multipurpose routine called to setup, turn on and off,
--          or save files in the hard_copy routines.
--    Note: The keys are executed in a heirarchy order as listed below, and
--          can thus be added for multiple actions in one call.
--          Note that one must be careful. Consider SV$SAV+SV$INI+SV$OFF, will
--          result in saving the currently active file buffer, followed by
--          initializing UNIT immediately and then turning off the save mode.
--
-- Inputs:  KEY - Parameter indicating operation to be performed
--                SV$RCM - Reread next stored block as common rewrite
--                SV$SAV - Output buffer, close and truncate = SV$OUT + SV$CLO
--                SV$OUT - Output buffer and truncate file
--                SV$CLO - Closes the current unit
--                SV$CLR - Clears save mode (turn off all units)
--                SV$INI - Initializes a specified unit for collecting commands
--                         note SV$INI => SV$UNT+SV$RST+SV$ON+SV$COM
--                SV$UNT - Specifies the active unit to be output to
--                SV$RST - Resets file to beginning and adds initial parameters
--                SV$ON  - Turns on command saving mode
--                SV$COM - Only save current common block data
--                SV$OFF - Turns off command saving mode
--                -7777  - Special code for SV$RST for call from ERASE
--         UNIT - Valid fortran unit opened for write privileges.
--                Must be sequential access, fixed records of SVBUFZ size,
--                and unformatted.
--                Only required for SV$INI or SV$UNT.  Other can = 0.
--         CODE - Return status giving error code of certain processes.
--                ES$BKY - Bad key
--                ES$BUN - Bad unit number (not open)
--                ES$NWR - File not opened for writing
--                ES$NCU - No unit currently active
--                ES$TNC - Unable to truncate file
--                ES$CLO - Unable to close file
--
-- Dec. 11, 1984 - MOT
--     Added version # to save of common block for later checking.
--
-- Dec. 28, 1986 - MOT
--     Added new commands and cleaned up
--
-- June 4, 1988 -MJU
--     Parameterized in terms of the size of an integer
============================================================================ */
int SV_Request(int key, FILE *stream) {

	int cmd,i,rcode;
	SVTYPE *SV;

	SV = &PlotWindow->Save;						/* So don't have to do every time */

/* ... Check validity of key */
	if (key >= SVKEY_MAX || key <= 0) {
		gen_err("Bad key in call (SV_Request)");
		return(SVERR_BADKEY);
	}

	if ( (key & (SVKEY_READ_COMMON | SVKEY_FLUSH)) && (SV->Unit == NULL)) {
		gen_err("No active unit (SAVE)");
		return(SVERR_NOUNIT);
	}

	if (key & SVKEY_READ_COMMON) {					/* Simple block read & XEQ */
		rcode = 0;											/* Assume valid return */
		SV->svptr = NULL;									/* Force a read */
		if ( (cmd=svgetcmd(SV)) == -1) {				/* Can I read a buffer */
			gen_err("Unable to read HCOPY buffer (SV_RCM)");
			rcode = SVERR_BADREAD;
		} else if (cmd != SV_INIT) {					/* Command not expected */
			gen_err("Expected INIT command which was not received (SV_RCM)");
			rcode = SVERR_BADREAD;
		} else if (! svcrst(SV)) {						/* Restore common block */
			gen_warn("Common block mismatch -- your problem!");
		}
		PlotConformDevice();								/* Force device to conform */
		PlotFixInternal();								/* And fix internal values */
		SV->svptr = SV->block;							/* Back at beginning of block */
		return(rcode);
	}

	if (key & SVKEY_FLUSH) {							/* Output buffer and truncate */
		svflush(SV);
	}

	if (key & SVKEY_CLOSE) {							/* Close down the unit */
		ftrunc(SV->Unit,-1L);							/* And truncate */
		fclose(SV->Unit);
		SV->On   = FALSE;
		SV->Unit = NULL;
	}

	if (key & SVKEY_CLEAR) {							/* Clear all modes */
		SV->On   = FALSE;
		SV->Unit = NULL;
	}

	if (key & SVKEY_SET_UNIT) {						/* Turn on this unit			*/
		SV->Unit   = stream;
		SV->On     = (SV->Unit != NULL);
		if (! SV->On) {									/* Is it invalid?				*/
			gen_err("Unit not open (SV_Request)");
			return(SVERR_NOUNIT);
		}
	}

	if (key & SVKEY_GET_END) {						/* Go to end of file			*/
		if (SV->Unit == NULL) {
			gen_err("No unit enabled (SV_Request)");
			return(SVERR_NOUNIT);
		}
		for (i=1;i<SVNSAVES;i++) SV->Marks[i].posn=-1;	/* Invalid positions */
		rewind(SV->Unit);									/* Rewind unit */
		SV->undo_pos = -1;								/* No undo possible */
		SV->curr_pos =  0;								/* At position 1 */
		SV->svptr    = NULL;
		while ( (cmd=svgetcmd(SV)) != -1) {
			if (cmd == SV_INIT) {
				svcrst(SV);
				for (i=SVNSAVES-1; i>=2; i--) SV->Marks[i] = SV->Marks[i-1];
				SV->Marks[1].posn = ftell(SV->Unit);
				strcpy(SV->Marks[1].savcur, SV->savcur);
				TTYprintf(" Position %8.8lx marked: %s\n", SV->Marks[1].posn, SV->Marks[1].savcur);
			}
			SV->svptr = NULL;
		}
		SV->Marks[0].posn = ftell(SV->Unit);
		strcpy(SV->Marks[0].savcur, "Reset");
		strcpy(SV->savcur, "Reset");
		SV->svptr = SV->block;
		SV->LastRestorePosn = -1;
		PlotConformDevice();
	}

	if (key & SVKEY_RESET) {							/* Truncate and reset		*/
		if (SV->Unit == NULL) {
			gen_err("No unit enabled (SV_Request)");
			return(SVERR_NOUNIT);
		}
		rewind(SV->Unit);											/* Rewind */
		ftrunc(SV->Unit,0L);										/* And truncate at 0 */
		SV->undo_pos = -1;										/* No undo possible */
		SV->curr_pos =  0;										/* At position 1 */
		SV->svptr    = SV->block;
		for (i=1;i<SVNSAVES;i++) SV->Marks[i].posn=-1;	/* Invalid positions */
		SV->Marks[0].posn = 0;									/* Initial one */
		strcpy(SV->Marks[0].savcur, "Start");
		strcpy(SV->savcur, "Start");
		SV->LastRestorePosn  = -1;
	}

	if (key & SVKEY_ON) {								/* Turn on save */
		if (! (SV->On = (SV->Unit != NULL))) {
			gen_err("No active file for saving! (SVON)");
			return(SVERR_NOUNIT);
		}
	}

	if (key & SVKEY_WRITE_COMMON) {					/* Initialize with new common */
		if (! SV->On) {
			gen_err("No file currently enabled! (SVCOM)");
			return(SVERR_NOUNIT);
		}
		svcsav(SV);											/* Save common block */
		ftrunc(SV->Unit,-1L);							/* And truncate */
	}

	if (key & SVKEY_OFF) {
		SV->On = FALSE;
	}

	return(0);
}

/* ============================================================================
-- Routine: SV_Plot(UNIT)
--
-- Purpose: Takes commands from the UNIT and translates them to plot commands
--          which are executed as necessary.  No plotter is enabled in the
--          routine and all erase functions must be executed before calling
--          the routine.
--
-- Usage: CALL SV_Plot(UNIT)
--
-- Inputs: UNIT - Valid Fortran unit number opened for reading.  Commands will
--                be taken from the current position.
--                If it is the same as the current active file for collecting
--                commands, the file is truncated and rewound before plotting.
--
-- Output: none
============================================================================ */
void SV_Plot(FILE *stream, LOGICAL NewFrame) {

	INTEGER i,j,n,cmd;
	REAL    x,y,z,x1,y1,height,theta,c[6];
	char	  charsp[LONG_STR_SIZE];
	LOGICAL doerase=TRUE;

	SVTYPE  *SV;
	FILE    *holdunit;
	LOGICAL	holdon;
	LOGICAL  WasBreak=FALSE, warn=TRUE;

/* ... Modifications to parameters in the HCOPY file */
	REAL	  ScalingFactor=1.0f;

	if (stream == NULL) {
		gen_err("Unit not open (SVPLOT)");
		return;
	}

	SV = &PlotWindow->Save;

	while (LexGetOption(charsp, sizeof(charsp))) {
		if (LexEqual(charsp,"-shrink", 2)) {
			x = LexGetReal(1.0f, "Multiplicative shrink (1.0): ");
			ScalingFactor = 1.0f / max(0.01f, x);
		} else if (LexEqual(charsp,"-noerase", 2)) {
			doerase = FALSE;
#ifdef JUNK
	REAL	  xoffset, yoffset;
		} else if (LexEqual(charsp,"-offset", 2)) {
			xoffset = LexGetReal(0.0f, "Additional offset in X: ");
			yoffset = LexGetReal(0.0f, "Additional offset in Y: ");
#endif
		} else {
			gen_err2("Unrecognized option", charsp);
		}
	}

	svflush(SV);							/* Flush if necessary */
	holdunit = SV->Unit;					/* Save current unit and on/off status */
	holdon   = SV->On;
	SV->On   = FALSE;						/* Temporarily turn off and change unit */
	SV->Unit = stream;
	rewind(SV->Unit);						/* Rewind the unit to beginning	*/

	if (NewFrame && doerase) {
		PlotNewPage(-1);
	}

/* .. Loop here while still have commands in buffer and no ^C interrupt */
	SV->svptr = NULL;						/* Nothing initially in buffer	*/
	while (TRUE) {

		WasBreak = SysChkBreak(TRUE);					/* Check on INTERRUPT	*/
		if (WasBreak) break;								/* Abort loop if true	*/

		if ((cmd = svgetcmd(SV)) == -1) break;		/* Get next command		*/

		switch (cmd) {
			case SV_INIT:					/* Initialize */
				if (! svcrst(SV) && warn) {
					gen_warn("Global areas different -- will continue trying");
					warn = FALSE;
				}
				PlotWindow->factr *= ScalingFactor;
				PlotConformDevice();					/* Force device to conform */
				PlotFixInternal();					/* And fix internal values */
				break;
			case SV_PLOT:					/* Plot line segment */
				x = svgetreal(SV);
				y = svgetreal(SV);
				i = svgetint(SV);
				PlotMove3D(x, y, 0.0f, i);
				break;
			case SV_PLOT3D:				/* 3-D plot request */
				x = svgetreal(SV);
				y = svgetreal(SV);
				z = svgetreal(SV);
				i = svgetint(SV);
				PlotMove3D(x,y,z,i);
				break;
			case SV_PLOTDOT:				/* Plot dot */
				x = svgetreal(SV);
				y = svgetreal(SV);
				PlotMove(x,y,4);
				break;
			case SV_FLUSH:					/* Flush */
				PlotFlush();
				break;
			case SV_FACTOR:				/* Factor */
				x = ScalingFactor * svgetreal(SV);
				PlotSetFactor(x);
				break;
			case SV_COLOR:					/* Color */
				i = svgetint(SV);
				PlotSelectPen(i);
				break;
			case SV_DASHES:				/* Dashes */
				i = svgetint(SV);
				x = svgetreal(SV);
				PlotSetLineType(i,x);
				break;
			case SV_USRMOD:				/* Usrmod */
				i = svgetint(SV);
				PlotSetUserMode(i);
				break;
			case SV_SPEED:					/* Speed */
				i = svgetint(SV);
				j = svgetint(SV);
				PlotSetPenSpeed(i,j);
				break;
			case SV_OFFSET:				/* Offset (coordinates values) */
				x  = svgetreal(SV);
				y  = svgetreal(SV);
				x1 = svgetreal(SV);
				y1 = svgetreal(SV);
				PlotSetScaling(x, y, x1, y1);
				break;
			case SV_SET:					/* Set */
				x  = svgetreal(SV);
				x1 = svgetreal(SV);
				y  = svgetreal(SV);
				y1 = svgetreal(SV);
				PlotSetRange(x,x1,y,y1);
				break;
			case SV_CLIP:					/* New clip routine */
				i = svgetint(SV);
				for (j=0; j<4; j++) c[j] = svgetreal(SV);
				PlotSetClip(&i, c);
				break;
			case SV_CLIPSYMBOLS:
				i = svgetint(SV);
				PlotSetSymbolClip(i);
				break;
			case SV_ORIGIN:				/* Origin */
				x = svgetreal(SV);
				y = svgetreal(SV);
				PlotSetOrigin(x,y);
				break;
			case SV_MARGIN:				/* Margin */
				x = svgetreal(SV);
				y = svgetreal(SV);
				x1 = svgetreal(SV);
				y1 = svgetreal(SV);
				PlotSetMargin(x, y, x1, y1);
				break;
			case SV_SIZE:					/* Size */
				x = svgetreal(SV);
				y = svgetreal(SV);
				PlotSetSize(x,y);
				break;
			case SV_ERASE:					/* Erase (obsolete) */
				PlotErase();
				break;
			case SV_NEWPAGE:
				i = svgetint(SV);
				PlotNewPage(i);
				break;
			case SV_FRAME:					/* Frame (obsolete) */
				PlotFrame();
				break;
			case SV_PANELON:				/* Panel on */
				break;
			case SV_PANELOFF:				/* Panel off */
				break;
			case SV_LINESTYLE:			/* Line style */
				i = svgetint(SV);
				PlotSetLineWidth(i);
				break;
			case SV_VISIBLE:				/* Visibility */
				i = svgetint(SV);
				PlotSetVisibility(i);
				break;
			case SV_REGERASE:				/* Region erase */
				x  = svgetreal(SV);
				y  = svgetreal(SV);
				x1 = svgetreal(SV);
				y1 = svgetreal(SV);
				PlotEraseRegion(x, y, x1, y1);
				break;
			case SV_FILL:					/* Fill */
				i = svgetint(SV);
				x = svgetreal(SV);
				y = svgetreal(SV);
				PlotFill(i, x,y);
				break;

			case SV_BEGINPATH:
				PlotBeginPath();
				break;
			case SV_ENDPATH:
				PlotEndPath();
				break;
			case SV_FILLPATH:
				i = svgetint(SV);
				j = svgetint(SV);
				PlotFillPath(i,j);
				break;
			case SV_AXESREGION:
				i = svgetint(SV);					/* Possible color */
				PlotInformAxesLimits(i);
				break;

			case SV_FILLRECTANGLE:
				x  = svgetreal(SV);
				y  = svgetreal(SV);
				x1 = svgetreal(SV);
				y1 = svgetreal(SV);
				i  = svgetint(SV);
				PlotFillRect(x,y, x1,y1, i);
				break;

			case SV_STROKEPATH:
				i = svgetint(SV);
				j = svgetint(SV);
				PlotStrokePath(i,j);
				break;

			case SV_FLIPXY:				/* Flip XY */
				i = svgetint(SV);
				PlotSetXYFlip(i);
				break;
			case SV_ORIENT:				/* Arbitrary orientation */
				i = svgetint(SV);
				PlotSetPageOrientation(i);
				break;
			case SV_SYMBOL:				/* Symbol routine */
				x = svgetreal(SV);
				y = svgetreal(SV);
				z = svgetreal(SV);
				height = svgetreal(SV);
				theta  = svgetreal(SV);
				n      = svgetint(SV);			/* N in symbol call */
				i      = PlotSelectFont(svgetint(SV));
				svgetstr(SV, charsp);
				Plot3DInchString(x,y,z, height, charsp, theta, n);
				PlotSelectFont(i);
				break;
			case SV_MARK:					/* Draw a single symbol at point */
				x = svgetreal(SV);
				y = svgetreal(SV);
				z = svgetreal(SV);
				height = svgetreal(SV);
				i = svgetint(SV);
				Plot3DSymbol(x, y, z, height, (CHAR) i);
				break;
			case SV_SET3DMODE:					/* Set 3D mode active	*/
				i = svgetint(SV);
				PlotSet3DMode(i);
				break;
			case SV_SET3DVIEW:					/* Orientation angles	*/
				x1 = svgetreal(SV);				/* Viewing distance		*/
				x  = svgetreal(SV);				/* Angles about x,y,z	*/
				y  = svgetreal(SV);
				z  = svgetreal(SV);
				PlotSet3DView(x1, x,y,z);
				break;
			case SV_SET3DRANGE:
				for (j=0; j<6; j++) c[j] = svgetreal(SV);
				PlotSet3DRange(c[0],c[1],c[2],c[3],c[4],c[5]);
				break;
			case SV_SET3DTRANSFORM:
				{ REAL gm[3][3];
				for (i=0; i<3; i++) for (j=0; j<3; j++) gm[i][j] = svgetreal(SV);
				Plot3DTransform(1, gm, NULL);	/* Force set new matrix */
				}
				break;
			case SV_RESET3DTRANSFORM:
				Plot3DTransform(0, NULL, NULL);	/* Force clear matrix */
				break;
			case SV_3DMULTIPLY:
				{ REAL gm[4][4]; int key;
				key = svgetint(SV);
				for (i=0; i<4; i++) for (j=0; j<4; j++) gm[i][j] = svgetreal(SV);
				Plot3DMultiply(key, gm);
				}
				break;
			default:
				PlotFlush();
				ERRprintf("WARNING: Unknown command %i (SVPLOT)\n", cmd);
				break;
		}
	}

/* ... Now, reset the information to its status before plot */
	PlotFlush();							/* Exit plot mode */
	if (WasBreak) {
		gen_warn("HCOPY plot aborted from keyboard.");
		while ( (cmd=svgetcmd(SV)) != -1) {
			if (cmd == SV_INIT) svcrst(SV);
			SV->svptr = NULL;
		}
		PlotConformDevice();
	}
	SV->Unit = holdunit;					/* Old unit */
	SV->On   = holdon;
	SV->svptr = SV->block;				/* As if nothing was there */
	return;

}

/* ---------------------------------------------------------------------------
-- Routine: LOGICAL FUNCTION SVCMD$(CMD)
--
--     Usage: LOG = SVCMD(CMD)
--
--     Inputs: UNT - Unit number to read from
--
--     Output: CMD - INTEGER character command
--             svcmd$ - .FALSE. if at end of file
--------------------------------------------------------------------------- */
PRIVATE int svgetcmd(SVTYPE *SV) {

	int cmd;

	while (TRUE) {
		if (SV->svptr==NULL || SV->svptr>=SV->block+SVBUFSIZE) {
			if (fread(SV->block, sizeof(SV->block), 1, SV->Unit) != 1) {
				if (feof(SV->Unit)) {					/* Just clear an EOF */
					clearerr(SV->Unit);
				} else {										/* But print errors	*/
					gen_err2(strerror(errno), "SVCMD");
				}
				return(-1);
			}
			SV->svptr = SV->block;
		}
		cmd = *(SV->svptr++);
		if (cmd != SV_NULL) return(cmd);
		SV->svptr = NULL;								/* Force a new buffer read */
	}

	panic; return(cmd);								/* BETTER NOT HAPPEN! */
}


/* ============================================================================
--     Routine to extract a character string from SVBUF
--
--     Usage: CALL SVSTR$(buf)
--
--     Inputs: none
--
--     Output: fill buf with character string from the buffer
============================================================================ */
PRIVATE char *svgetstr(SVTYPE *SV, char *buf) {

	UINT i;

	i = *(SV->svptr++);						/* Get # of chars in string */
	memcpy(buf, SV->svptr, i);
	buf[i] = '\0';								/* And mark the EOS */
	i = (i+sizeof(int)-1)/sizeof(int);
	SV->svptr += i;
	return(buf);
}

/* ============================================================================
--     Routine to extract an integer number from SVBUF
--
--     Usage: int = SVINT$()
--
--     Inputs: none
--
--     Output: Next integer number out of the buffer
============================================================================ */
PRIVATE int svgetint(SVTYPE *SV) {

	return(*(SV->svptr++));
}

/* ============================================================================
--     Routine to extract a real number from SVBUF
--
--     Usage: real = SVREL$()
--
--     Inputs: none
--
--     Output: Next real number out of the buffer
============================================================================ */
PRIVATE REAL svgetreal(SVTYPE *SV) {

	REAL x;
	
	memcpy(&x, SV->svptr, sizeof(REAL));
	SV->svptr += sizeof(REAL)/sizeof(int);
	return(x);
}

/* ============================================================================
-- ... In current configuration, parms will be on line with command always
============================================================================ */

/* ============================================================================
-- Usage:  SV_PutCmd(cmd, nint,nreal,nchr);
--
-- Inputs: nint  - Number of integers extra needed
--         nreal - Number or real values needed
--         nchr  - Number of characters
--
-- Output: Makes sure there is sufficient space in buffer for requested output.
============================================================================ */
void SV_PutCmd(int key, UINT NumInts, UINT NumReals, UINT NumChars) {

	UINT i;
	SVTYPE *SV;
	
	SV = &PlotWindow->Save;

	if (SV->On) {										/* Anything to do	*/
		i = 1 + NumInts + NumReals*(sizeof(REAL)/sizeof(int));
		if (NumChars > 0) i += 1 + (NumChars+sizeof(int)-1)/sizeof(int);
		if (SV->svptr+i >= SV->block+SVBUFSIZE) svout(SV);
	}
	*(SV->svptr++) = key;
	return;
}

/* ============================================================================
--     Routine to insert a integer number into the INTEGER buffer SVBUF
--
--     Usage: CALL SV_PutInt(I)
--
--     Inputs: I - Integer value to be insert in buffer
--
--     Note: Assumes the space exists, no checking done.
============================================================================ */
void SV_PutInt(int ival) {

	if (PlotWindow->Save.On) {
		*(PlotWindow->Save.svptr++) = ival;
	}
	return;
}

/* ============================================================================
--     Routine to insert a real number into the INTEGER buffer SVBUF
--
--     Usage: CALL SVREAL(X)
--
--     Inputs: X - Real variable to be insert in buffer
--
--     Note: Assumes the space exists, no checking done.
============================================================================ */
void SV_PutReal(REAL rval) {

	if (PlotWindow->Save.On) {
		memcpy(PlotWindow->Save.svptr, &rval, sizeof(REAL));
		PlotWindow->Save.svptr += sizeof(REAL)/sizeof(int);
	}
	return;
}

/* ============================================================================
--     Routine to insert a character string into the INTEGER buffer SVBUF
--
--     Usage: CALL SV_PutStr(str,nn)
--
--     Inputs: str - character string to be put in buffer
--             nn  - number of characters to be inserted
--
--     Note: Assumes the space exists, no checking done.
============================================================================ */
void SV_PutStr(char *str, UINT NumCharss) {

	UINT i;
	if (PlotWindow->Save.On) {
		i = 1 + (NumCharss+sizeof(int)-1)/sizeof(int);
		*PlotWindow->Save.svptr = (int) NumCharss;
		memcpy(PlotWindow->Save.svptr+1, str, NumCharss);
		PlotWindow->Save.svptr += i;
	}
	return;
}


/* ============================================================================
--     Subroutine to save the data in the CPLTX3 common block.  Has to be kept
--     to allow HCOPY MARK and start of save. Also saves SAVCUR alpha text from
--     the SVKEYS.INS common block.  This will be text identifying the common
--     block saved.
--
--     Usage:  CALL SVCSAV
--
--     Inputs: none
--
--     Output: Puts common block into SVBUF and outputs it.
============================================================================ */
PRIVATE LOGICAL svcsav(SVTYPE *SV) {

	UINT len;
	
	if (! SV->On) return(TRUE);					/* Nothing to do */

	svout(SV);

	*(SV->svptr++) = SV_INIT;							/* Must be first */
	len = (int) ( ( (char *) &PlotWindow->Save) - ( (char *) PlotWindow) );
	memcpy(SV->svptr, PlotWindow, len);							/* Save COMMON */
	SV->svptr += (len+sizeof(int)-1)/sizeof(int);
	memcpy(SV->svptr, SV->savcur, sizeof(SV->savcur));		/* Save SAVCUR */
	SV->svptr += sizeof(SV->savcur);

	svout(SV);

	return(TRUE);
}


/* ============================================================================
--     Subroutine to restore the data in the CPLTX3 common block.
--
--     Usage: logical = svcrst()
--
--     Inputs: none
--
--     Output: svcrst - .TRUE.  -> Same version stored as currently running
--                      .FALSE. -> Version of CPLTX3 block different (error?)
--             Refills common block CPLTX3
--             Makes appropriate calls to reset plotter status
--
-- ... If block version # different, tries to patch up.
============================================================================ */
PRIVATE LOGICAL svcrst(SVTYPE *SV) {

	UINT len;
	
	SV->svptr = SV->block;
	if (*(SV->svptr++) != SV_INIT) return(FALSE);	/* Must be first */
	
	len = (int) ( ( (char *) &PlotWindow->Save) - ( (char *) PlotWindow) );
	memcpy(PlotWindow, SV->svptr, len);							/* Restore COMMON */
	SV->svptr += (len+sizeof(int)-1)/sizeof(int);
	memcpy(SV->savcur, SV->svptr, sizeof(SV->savcur));		/* Restore SAVCUR */
	SV->svptr += sizeof(SV->savcur);
	return(TRUE);
}

/* ============================================================================
--     Routine to output buffer to the disk.  Resets SVPTR to 1.
--
--     Usage: CALL SVOUT
--            Counts total number of blocks output
============================================================================ */
PRIVATE void svout(SVTYPE *SV) {

	if (SV->Unit == NULL) {
		SV->On = FALSE;
	} else if (SV->svptr != SV->block) {
		if (SV->svptr < SV->block+SVBUFSIZE) *(SV->svptr) = SV_NULL;
		fseek(SV->Unit, 0L, SEEK_CUR);	/* A read must be followed by a seek */
													/* before a write.  This ensures ok. */
		if (fwrite(SV->block, sizeof(SV->block), 1, SV->Unit) != 1) {
			PlotFlush();
			ERRprintf("(SVOUT) Write failure (%s).\n"
						 "        HCOPY file %s will be closed and deleted.\n",
				strerror(errno), SV->Name);
			clearerr(SV->Unit);					/* May be necessary to close	*/
			fclose(SV->Unit);						/* Close the puppy				*/
			if (remove(SV->Name) != 0)			/* And unlink it					*/
				ERRprintf("(SVOUT) Unable to delete bad file\n");
			SV->On = FALSE;
			SV->Unit = NULL;
			return;
		}
	}
	SV->svptr = SV->block;
	return;
}

/* ============================================================================
--     Routine to flush buffer to the disk.  Makes sure everything is written
--     and truncates the file.
--
--     Usage: svflush(SVTYPE *SV);
============================================================================ */
PRIVATE void svflush(SVTYPE *SV) {
	
	if (SV->Unit != NULL) {
		if (SV->On) svout(SV);							/* Output pending block	*/
		ftrunc(SV->Unit,-1L);							/* And truncate			*/
	}
	return;
}


/* ============================================================================
--     SV_UNDO
--
-- Routine called from LEXP each time a command line is read from the terminal.
-- We keep track of where pointer was at the beginning of the line and hence
-- try to make the whole game backupable.  Must load COMPLOT before LEXP to
-- get this properly loaded.
--
-- Usage:  call sv_undo
--
-- Inputs: none
--
-- Output: internal only
============================================================================= */
void sv_undo(void) {
	
	SVTYPE *SV;
	long i4;

	SV = &PlotWindow->Save;

	if (SV->On) {
		svout(SV);											/* Flush current buffer */
		i4 = ftell(SV->Unit);							/* Where are we? */
		if (SV->curr_pos != i4) {						/* Okay, we have done something */
			SV->undo_pos = SV->curr_pos;
			SV->curr_pos = i4;
		}
	}
	return;
}
