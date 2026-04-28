/* DSPTCH - Dispatch routine for all graphics functions */
/* ============================================================================
--     Tektronix_Driver - COMPLOT driver for Tektronix format devices such as
--                        TEK4010, TEK4105, SELANAR SG200, etc.
--
--     Usage: LOG = Tektronix_Driver(cmd, PARMS)
--
--     Inputs: cmd   - Command - See driver.ins    for definitions
--             PARMS - Variable dimensioned array with parameters transfered
--                     Type and direction depend on command.
--
--     Output: DRVTKX - Success of operation
--
--     Notes: 1) Due to the use of FF capability on the VAX computers, screen
--               erase may not function properly (FF will be converted to a
--               series of vertical tabs.  To bypass this problem, the 4105
--               terminals can be programed with a macro which will clear the
--               screen.  The clear screen here, then, is a macro execute which
--               can be done as:
--               CALL PLCOUT(CHAR(27) // 'KXG+',5)      -- Execute macro
--               Alternately, the terminal controller can be reconfigured to
--               form feeds.
--
--            2) GSMODE and FSMODE are really just reminders to the programmer.
--               In actual use, the GSMODE is indicated by being in PLTMOD
--               and FSMODE not set.  GSMODE = PLTMOD .AND. (.NOT. FSMODE)
--
-- IBM-PC notes:  There are serious problems with the real Tektronix 4010 type
--                terminals.  PC runs too fast with nice interrupt driver.  It
--                may be necessary to add "CALL WAI232(IOUNIT)" code to flush
--                the buffer before continuing. (Note that PLINI returns the
--                IOUNIT on return)
--
--                Easiest fix is to modify the RS-232 card in PC to accept an
--                incoming clock from the TK4010.  This requires cutting the
--                run from 2 pins on a LS125 chip which generates the clock for
--                the UART.  Connect the pins to a switch which allows either
--                the internal clock or an external clock to be sent to the
--                last buffer driver before going to the UART.  The TEK 4010
--                can be configured to send a 16X it's incoming clock to pin 15
--                of the cable, which gets hooked into the UART.  The divisor
--                on the UART must be set to divide by 1 to handle this.  Nice
--                fix which really speeds things up!
--
-- August 4, 1986 - MOT
--     Added an offset to final output which randomly places the axis in the
--     range of 0 to +10 X and Y pixels.  Keeps us from burning the screen at
--     the same place everytime.  New position everytime erase done.
--
-- April 9, 1988 - MOT
--     Rewritten to support new device driver / io channel separation
--     Slow 4010  (needing sync chars) specified so by setting LDEV(3) to
--          9600/baud.  Ie. LDEV(3) = 8 => running at 1200 and need SYN's.
--
-- July 7, 1988 - MOT
--     Added KERMIT switch mode
============================================================================ */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define Tektronix_Driver SlaveDriver
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"

#include "complot.h"
#include "io_chan.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#define MaxPenColor			15					/* 0-15 colors possible			*/

typedef struct _DRVBLOCK {						/* These need to quasi-static */
	void		*IO_Block;							/* Carry over to IO channel	*/
	int		ldev2;								/* Secondary type					*/
	int		ibaud;								/* 9600/baud for syn chars		*/
	LOGICAL	pltmod;								/* Are we in plot mode?			*/
	char		*toansi;								/* Code to switch to terminal mode */
	char		*tograph;							/* Code to switch to graph mode */
	int		ncurs;								/* Number of chars returned by cursor */
	int		ixloc, iyloc;						/* Current pen location			*/
	int		tkxoff, tkyoff;					/* Random offset for screen	*/
	int		numpens;								/* Maximum number of pens		*/
	LOGICAL	fsmode;								/* Are we in a point mode		*/
	LOGICAL	panels;								/* Are we starting in panel	*/
	LOGICAL	hasfs;								/* Unit supports FS mode?		*/
	LOGICAL	mouse;								/* Is this equiped with a mouse */
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL Tektronix_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE void tkanmd(void);
PRIVATE void tkxyou(int x, int y, LOGICAL gslast);
PRIVATE void tkdela(int nchar);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

#define	TK4010			1				/* Tek 4010 terminal */
#define	TK4014			2				/* Tek 4014 capabilities */
#define	TK4105			3				/* Tek 4105 color terminal */
#define	SG200				4				/* Selanar SG200 for VT100 */
#define	VI550				5				/* Visual 550 */
#define	HIREZ				6				/* Selanar HI-REZ */
#define	DETAB				7				/* Digital Engineering TAB */
#define	VT340				8				/* VT340/TEK => kermit */
#define	TE4105			9				/* Tektronix 4105 Edit Mode */
#define	XTERM				10				/* XTerm vt102 & tek4014 modes */
#define	NDEV				10				/* Total number */

/* ... And now some special characters - General 4014 codes */
#define	ESCFF					"\033\f"			/* <esc><FF>  */
#define	ESCETB				"\033\027"		/* <esc><ETB> */
#define	ESCSUB				"\033\032"		/* <esc><SUB> */
#define	ASCIFS				 0x1C				/* <FS> */
#define	ASCIGS				 0x1D				/* <GS> */
#define	ASCIUS				 0x1F				/* <US> */
#define	ASCSYN				 0x16				/* <SYN> */
#define	BYPASSCANCEL		 0x07				/* <bell> */

/* ---------------------------------------------------------------------------
-- ... Bit definitions for flags
-- ...       0 - Has FSMODE capability
-- ...		 1 - Cursor report has 6 characters rather than 5
-- ...       2 - Mouse attached, translate 'l' to 0 and 'r' to 1 on mouse
--------------------------------------------------------------------------- */
PRIVATE struct _DEVPAR {
	int xmax, ymax;
	int flags;
	char *tograph, *toansi;
} table[] = {	
	{1023, 767, 0, "",			""},					/* Tektronix 4010					*/
	{1023, 782,	1, "",			""},					/* Tektronix 4014					*/
	{1023, 782, 1, "\033%!0",	"\033%!1"},			/* Tektronix 4105 color			*/
	{1023, 802, 1,	"\035",		"\035\033~0T"},	/* Selanar SG200 for VT100		*/
	{1023, 779, 3,	"\035",		"\035\030"},		/* VI550 Emulator/Retrogr		*/
	{1023, 779, 3,	"\0331",		"\0332"},			/* HIREZ 100						*/
	{1023, 767, 3,	"\035",		"\035\"0"},			/* Digital Engineering TAB		*/
	{1023, 767, 3,	"\033[?38h","\033[?38l"},		/* VT 340/Tek						*/
	{1023, 782, 1,	"\033%!0",	"\033%!2"},			/* Tektronix 4105 color Edit	*/
	{1023, 782, 5,	"\033[?38h","\033\003"}			/* XTerm vt102/tek4014 modes	*/
};

PRIVATE DRVBLOCK  *blk;								/* Make these global */
PRIVATE IO_BLOCK  *io;								/* Make these global */

/* ----------------------------------------------------------------------- */
LOGICAL Tektronix_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	int i;
	char buf[8];								/* Space for cursor response */

/* -------------------------------------------------------------
   Handle first part of initialization separate.  Allocate memory 
   and start up the I/O channel process with its own junk
-------------------------------------------------------------- */
	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("TEKTRONIX", dsp->ini.IO_Channel, IOC_BINARY, IOF_NOFLOW);
		if ( (io = blk->IO_Block) == NULL) return(FALSE);
	} else {
		blk = DriverBlock;						/* My local copy (so can change) */
		io  = blk->IO_Block;
	}


/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {

/* --------------- Initialize ----------------------- */
		case INIFNC:

			if (dsp->ini.SubDevice > NDEV) return(FALSE);	/* Too big a number */

			blk->ldev2			= dsp->ini.SubDevice;	/* Secondary device */
			blk->ibaud			= dsp->ini.Options;		/* Store options as ibaud */
			dsp->ini.xperinch = 100;						/* 100 dpi average for me */
			dsp->ini.yperinch = 100;

			i = blk->ldev2-1;
			dsp->ini.xmax     = table[i].xmax;
			dsp->ini.ymax     = table[i].ymax;
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS |	/* Supports graphs	*/
											DEV_CAP_CURSOR;		/* Can do cursors		*/

			blk->tograph      = table[i].tograph;
			blk->toansi       = table[i].toansi;
			blk->hasfs =  table[i].flags & 0x01;	/* Device has point mode	*/
			blk->ncurs = (table[i].flags & 0x02) ? 6 : 5 ;	/* # chars from cursor */
			blk->mouse =  table[i].flags & 0x04;	/* Device has mouse cursor	*/
			blk->ixloc  = -1;								/* Location undefined		*/
			blk->iyloc  = -1;
			blk->pltmod = FALSE;							/* Start out of plot mode	*/
			blk->fsmode = FALSE;							/* Ensure FS mode off		*/
			blk->panels = FALSE;							/* Ensure panels stay away	*/

			blk->numpens  = dsp->ini.NumberPens;
			if (blk->numpens < 1) blk->numpens = 1;
			if (blk->numpens > MaxPenColor) blk->numpens = MaxPenColor;
			dsp->ini.NumberPens = blk->numpens;

/* ----------------------------------
-- ... Special initializations ... 
------------------------------------ */
/* ... Tektronix 4105 in edit mode only has different TOANSI sequence */
			if (blk->ldev2 == TE4105) blk->ldev2 = TK4105;

/* ... Switch operating modes on selanar SG200 */
			if (blk->ldev2 == SG200) {			/* Initializing devices				*/
				IO_fputs(
					"\033[?5h"						/* <esc>[?5h - Turn screen white	*/
					"\0331"							/* <esc>1    - Go SELANAR mode	*/
					"\033\f"							/* Clear the screen					*/
					"\033~3T"						/* <esc>~3T  - Go to SG200 mode	*/
					"\033\""							/* <esc>"    - SELANAR ON			*/
					"\033~7T",io);					/* <esc>~7T  - Go to TEK mode		*/
				blk->pltmod = TRUE;				/* unfortunately now in PLTMOD	*/
				tkanmd();							/* Switch back							*/
			}

/* --- Now, set starting poisitin and we are done --- */
			srand((int) time(NULL));				/* Set random generator	*/
			blk->tkxoff = rand() % 10;				/* Give random movement	*/
			blk->tkyoff = rand() % 7;				/* Give random movement	*/
			break;

/* -------------------- draw vector ---------------------------------------- */
		case LINFNC:												/* Draw line */
			if (blk->panels) {
				if (! blk->pltmod) IO_fputs(blk->tograph, io);
				IO_fputs("\033LP", io);							/* <esc>LP - begin panel */
				tkxyou(dsp->line.x1, dsp->line.y1, TRUE);	/* At last point */
				IO_fputc('0', io);								/* Kludge for panel */
				IO_fputc(ASCIGS, io);							/* Ready for first vector */
				blk->pltmod = TRUE;
			} else if (blk->ixloc != dsp->line.x1 || blk->iyloc != dsp->line.y1 || !blk->pltmod || blk->fsmode) {
				if (! blk->pltmod) IO_fputs(blk->tograph, io);
				if (  blk->fsmode) IO_fputc(ASCIUS, io);	/* Get from points to lines */
				IO_fputc(ASCIGS, io);							/* GS to get to graphics */
				tkxyou(dsp->line.x1, dsp->line.y1, TRUE);	/* At last point */
				blk->fsmode = FALSE;								/* Not in FSMODE anymore */
				blk->pltmod = TRUE;								/* Definitely in plot mode */
			}
			blk->ixloc = dsp->line.x2; blk->iyloc = dsp->line.y2;
			tkxyou(blk->ixloc, blk->iyloc, FALSE);			/* And draw the vector */
			break;

/* -------------------- erase -------------------- */
/* ... See special notes at top for VAX changes possible on TEK4105 */
		case ERSFNC:					/* Erase screen */
			if (! blk->pltmod) IO_fputs(blk->tograph,io);
			blk->pltmod = TRUE;
			if (blk->ldev2 ==  DETAB)
				IO_fputs("\033\"g!ERA G\015", io);	/* Talk about obscure */
			else
				IO_fputs(ESCFF, io);					/* <esc>FF clear screen */
			tkdela(960);									/* Erase delay */
			blk->tkxoff = rand() % 10;					/* New random movement	*/
			blk->tkyoff = rand() % 7;					/* New random movement	*/
			tkanmd();
			break;

/* -------------------- flush -------------------- implicit anmode -------- */
		case FLSFNC:					/* Flush all buffers */
			tkanmd();
			break;

/* -------------------- frame -------------------- */
		case FRMFNC:					/* End of frame */
			tkanmd();
			break;

/* -------------------- end ----------------------- */
		case ENDFNC:					/* End of plot */
			tkanmd();
			if (blk->ldev2 == SG200) {			/* End VT100 Selanar SG200 */
				IO_fputs("\033[?5l", io);		/* <esc>[?5l - Turn screen black	*/
				IO_fflush(io);
			}
			IO_CloseChannel(io);
			free(blk);										/* Free my memory usage */	
			break;

/* ---------------------------- change color -------------------- */
/* -- NOTE, TEKTRX uses the pen number instead of the color index */
/* -------------------------------------------------------------- */
		case COLFNC:								/* Set color */
			i = min(blk->numpens, max(0, dsp->col.pen));
			if (blk->ldev2 != TK4105) return(FALSE);			/* Only 4105 implements */
			if (! blk->pltmod) IO_fputs(blk->tograph,io);	/* Into tek mode			*/
			IO_fprintf(io, "\033ML%d", i);						/* Color select #			*/
			if (! blk->pltmod) IO_fputs(blk->toansi,io);		/* Return if necessary	*/
			break;

/* -------------------- alphanumeric mode -------------------- */
		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
			tkanmd();
			break;

/* -------------------- change speed -------------------- */
		case SPDFNC:					/* Set pen speed */
			break;

/* -------------------- set visibility -------------------- */
		case VISFNC:					/* Set visibility (light, dark, complement) */
			break;

/* -------------------- read cursor -------------------- */
		case CURFNC:					/* Read cursor function */
			if (! blk->pltmod) IO_fputs(blk->tograph, io);	/* Go to TEK mode */
			blk->pltmod = TRUE;										/* Definitely there */
			tkdela(4);													/* Let vector finish */

/* ... Read TEK with <esc><sub> to enable cursor. 5 or 6 ASCII chars back */
			if (IO_GetInput(io, buf, blk->ncurs, -1, ESCSUB) <= 0) {	/* Fail */
				tkanmd();
				return(FALSE);
			}
			if (blk->ldev2 == TK4105) IO_fputc(BYPASSCANCEL, io);
			tkanmd();
			dsp->cur.x = 32*(buf[1] & 0x1F) + (buf[2] & 0x1F) - blk->tkxoff;
			dsp->cur.y = 32*(buf[3] & 0x1F) + (buf[4] & 0x1F) - blk->tkyoff;
			dsp->cur.achr = (buf[0] & 0x7F);
			if (blk->mouse) {
				if (dsp->cur.achr == 'l') dsp->cur.achr = '0';
				if (dsp->cur.achr == 'r') dsp->cur.achr = '1';
			}
			break;

/* -------------------- point plot -------------------- */
		case PNTFNC:												/* Plot a single point */
			if (! blk->pltmod) IO_fputs(blk->tograph, io);
			if (blk->hasfs) {
				if (! blk->pltmod || ! blk->fsmode) {
					IO_fputc(ASCIFS, io);						/* Get into point mode	*/
					blk->pltmod = TRUE;							/* Now in plot mode		*/
					blk->fsmode = TRUE;							/* Now point FSMODE		*/
					blk->panels = FALSE;							/* In case of aborting	*/
					tkxyou(dsp->point.x, dsp->point.y, TRUE);
				} else {
					tkxyou(dsp->point.x, dsp->point.y, FALSE);
				}
			} else {
				IO_fputc(ASCIGS, io);							/* May be duplicate */
				tkxyou(dsp->point.x, dsp->point.y, TRUE);
				tkxyou(dsp->point.x, dsp->point.y, FALSE);
			}
			blk->ixloc = dsp->point.x;
			blk->iyloc = dsp->point.y;
			blk->pltmod = TRUE;
			break;

/* -------------------- Begin panel --------------------  */
		case PANFNC:					/* Panel function */
			if (blk->ldev2 != TK4105) return(FALSE);		/* Only good on 4105 */
			if (! blk->pltmod) IO_fputs(blk->tograph, io);	
			IO_fputs("\033MP", io);								/* Select Fill pattern */
			IO_fputc(0 | 0x20, io);								/* Pattern number */
			blk->panels = TRUE;									/* Needs (x,y),0 after */
			if (! blk->pltmod) IO_fputs(blk->toansi, io);
			break;
			
/* -------------------- End panel --------------------  */
		case POFFNC:					/* Panel off function */
			if (blk->ldev2 != TK4105) return(FALSE);
			IO_fputs("\033LE", io);					/* <esc>LE    - End panel code */
			break;

/* -------------------- Set line linewidth --------------------  */
		case LWFNC:										/* Line width function */
			break;

/* -------------------- Character output -----------------
 ... <length> (string) <angle> <x> <y> label
	chrset    - character set                       x  - x position (start)
	size      - nominal character size (pixels)     y  - y position (start)
	angle     - angle to draw
	pixel_len - length in pixels (X)
	*string   - pointer to string
 ------------------------------------------------------- */
		case DRAWCHAR:					/* Text drawing functions */
			return(FALSE);

/* ------------------- Graphics Print ------------------- */
		case GRPFNC:					/* Graphics screen dump function */
			if (! blk->pltmod) IO_fputs(blk->tograph, io);
			IO_fputs(ESCETB, io);						/* Hard copy request */
			blk->pltmod = TRUE;
			tkanmd();										/* Back to terminal mode */
			break;

		default: 
			return(FALSE);
	}
	return(TRUE);
}


/* ============================================================================
--     TKANMD - Routine to return TEKTRONIX type terminal to alpha-numeric mode
--
--     Usage: CALL TKANMD
--
--     Notes: Does a flush of the internal buffer and then switches back to
--            Alphanumerics.  The SG200 is the only difficulty which has to
--            KLUDGE back to VT100 mode.
============================================================================ */
PRIVATE void tkanmd(void) {
	
	if (blk->pltmod) {
		tkdela(4);											/* Give time to draw vector */
		IO_fputc(ASCIUS, io);							/* Out of GRAPHICS plane */
		IO_fputs(blk->toansi, io);						/* All have code */
	}
	blk->pltmod = FALSE;									/* Out of plot mode now */
	IO_fflush(io);
	return;
}

/* ============================================================================
--     TKXYOU - Encode X,Y coordinates and send to display
--
--     Usage: CALL TKXYOU(X, Y, GSLAST)
--
--     Inputs: X,Y    - Coordinates to be output (INTEGER)
--             GSLAST - Do I need to send all 4 bytes?
--                      .TRUE.  => Force send all bytes
--                      .FALSE. => Compress data if possible
--
--     Notes:
--     The TEK convention sends binary packed data 5 bits/character with
--     unique identifers for each type. Form is
--           01xxxxx - High Y
--           11xxxxx - Low  Y
--           01xxxxx - High X
--           10xxxxx - Low  X
--     To speed communication, only the bytes that change must be resent,
--     the exceptions - 1) All must be sent after a GS or FS
--                      2) If High X is sent, so must Low Y
--                      3) Low X must always be sent
--                      4) At 9600 baud with TEK4010, need to send at least 2
--                         to allow time for it to interpret them - pain.
--
============================================================================ */
PRIVATE void tkxyou(int x, int y, LOGICAL gslast) {

	int i;
	unsigned char h[5], h2[5];
	static unsigned char hold[5]="";					/* Last coordinates */

	x = min(1023,x+blk->tkxoff);					   /* Conversion for offset	*/
	y = min(1023,y+blk->tkyoff);

	h[0] = (char) ( ((y>>5) & 0x1F) | 0x20);		/* Generate high y - :040 */
	h[1] = (char) ( ( y     & 0x1F) | 0x60);		/* Generate low  y - :140 */
	h[2] = (char) ( ((x>>5) & 0x1F) | 0x20);		/* Generate high x - :040 */
	h[3] = (char) ( ( x     & 0x1F) | 0x40);		/* Generate low  x - :100 */
	h[4] = '\0';

	if (gslast) {
		IO_fwrite(h, 4, 1, io);							/* Send all of the bits */
	} else {													/* otherwise minimize char # */
		i = 0;
		if (h[0] != hold[0]) h2[i++] = h[0];		/* Send high Y if necessary */
		if (h[2] != hold[2]) {							/* If high X changes, send */
			h2[i++] = h[1];								/* low Y, high X, low X		*/
			h2[i++] = h[2];
			h2[i++] = h[3];
		} else {
			if (h[1] != hold[1]) h2[i++] = h[1];	/* Need low Y? */
			h2[i++] = h[3];								/* Send Low X */
		}
		h2[i] = '\0';
		IO_fwrite(h2, i, 1, io);						/* Send the characters! */
	}

	for (i=0; i<4; i++) hold[i] = h[i];		/* Save a copy for next time */
	return;
}

/* ============================================================================
--     TKDELA - Routine to create a delay by sending no-op to TEK terminals.
--              Real pain since device does not support Xon/Xoff.  Only the
--              TEK4010 bothers with this delay, all others just return
--
--     Usage: CALL TKDELA(NCHAR)
--
--     Input: NCHAR - Number of SYN characters to send (at 9600 baud)
============================================================================ */
PRIVATE void tkdela(int nchar) {

	int i;
	if (blk->ibaud != 0) {								/* Send SYN characters? */
		for (i=nchar/blk->ibaud; i--; ) IO_fputc(ASCSYN, io);
	}
	return;
}
