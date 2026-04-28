/* HP-GL.F77 */

/* ---------------------------------------------------------------------------
-- Modification history.
--
-- 4/23/95 - MOT
--      Modified to incorporate new structure for rotated paper (orientation).
--      Also changed coordinates to SHORTS in code (since no need)
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"
#ifdef SLAVE
	#define HPGL_Driver SlaveDriver
#endif

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
#include "io_chan.h"
#include "complot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _DRVBLOCK {					/* These need to quasi-static */
	IO_BLOCK	*IO_Block;						/* Carry over to IO channel */

	LOGICAL	pltmod,							/* Device in plot mode?					*/
				CanDoFF,							/* Can I do FF								*/
				PaperCheck,						/* How about paper checks?				*/
				CanDoFill,						/* How about area fills?				*/
				IniHPGL,							/* Include initialization strings	*/
				EnqAck,XonXoff,				/* ENQ/ACK or XON/XOFF protocol?		*/
				TTYOut;							/* Output goes down terminal line	*/
	LOGICAL	dirty,							/* Is plot dirty now?					*/
				newpen,							/* Do we need the new pen?				*/
				pendown;							/* Is the pen down?						*/
	INTEGER	wpen,								/* Pen wanted								*/
				numpens,							/* Number of pens allowed				*/
				ixloc,iyloc,					/* Current pen position					*/
				ldev2;							/* Selected device						*/
	INTEGER	xl,yl,xh,yh;					/* Clipping limits						*/
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL HPGL_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL hppchk(void);
PRIVATE void hpanmd(void);
PRIVATE void hppmod(void);
PRIVATE LOGICAL DrawMarker(int isym, int x, int y, int size, int angle);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

#define NPERIN		1016			/* Number of points/inch					*/
#define FEED		0x01			/* Has ability to do page feed			*/
#define DETECT		0x02			/* Has ability to detect page in place */
#define FILL		0x04			/* Has polygon fill capability			*/
#define ONLINE		"\033.("
#define OFFLINE	"\033.)"
#define NDEV		7				/* Number of devices */
#define SIZESETS	3				/* Number of paper size sets */

PRIVATE IO_BLOCK *io;			/* Pointer to block for I/O process */
PRIVATE DRVBLOCK *blk;			/* Pointer to local block */

/*............................................................................
-- DEVPAR gives the capabilities of the particular plotters.
--    (1) => Bit specification of capabilities
--      bit 0 (1) --> Ability to do page feeds
--      bit 1 (2) --> Ability to do a paper in place test
--    (2) => Page set parameters to use
--    (3) => Maximum buffer size on plotter (operating style)
............................................................................ */


PRIVATE struct {
	INTEGER Flags;					/* Capability flags					*/
	INTEGER NumberPens;			/* Number of pens it holds			*/
	INTEGER PageSet;				/* Which page size set to use		*/
	INTEGER BufSize;				/* Maximum buffer size allowed	*/
} devpar[] = {
	{0,					8,	0,  928},		/* HP 9872S     no    no	*/
	{0,					8,	0,  928},		/* HP 7220AC    no    no	*/
	{FEED,				8,	0,  928},		/* HP 7220ST    yes   no	*/
	{0,					8,	0, 1024},		/* HP 7225A     no    no	*/
	{DETECT,				8,	1, 1024},		/* HP 7475A     no    yes	*/
	{DETECT | FEED | FILL,	8,	2, 1024},		/* HP 7550A     yes   yes	*/
	{0,					8,	0,   60}			/* HP 7440A     no    no!!	*/
};

/* Maximum sizes allowed for 4 paper sizes for different plotters */
typedef struct {
	INTEGER x;
	INTEGER y;
} PageSize;

PRIVATE PageSize PaperSize[][4] = {
/*     A size         B size        A4 size        B4 size         */
/*     8.5x11         11x17         210x297        297x420         */
  { {11176, 8636},	{16000,11176},	{11880, 8400},	{16000, 11400} },	 /* HP7220 */
  { {10365, 7962},	{16640,10365},	{11040, 7721},	{16158, 11040} },	 /* HP7475 */
  { {10870, 7600},	{15970,10870},	{10170, 7840},	{16450, 10170} }	 /* HP7550 */
} ;


/* Type for defining symbols in DrawMarker later */
typedef struct _COORD {
	short x,y;
} COORD;


/* ============================================================================
--     HP-GL - COMPLOT low level driver for HP plotters utilizing HPGL language
--
--     Usage: LOG = DRVHPG(cmd, PARMS)
--
--     Inputs: cmd   - Command - See COMPLOT.INS for definitions
--             PARMS - Variable dimensioned array with parameters for transfer
--                     Type and direction depend on command.
--
--     Output: DRVHPG - Success of operation
--
-- Nov. 22, 1984 - MOT
--     Modified cursor routine so pen is not automatically put down on request.
--
-- Sep. 7, 1985 - MOT
--     Added equipment flag for pinch rollers, etc.
--
-- April 9, 1988 - MOT
--     Rewritten to support new device driver / io channel separation
--
-- July 17, 1988 - MOT
--     Added separation of I/O channel to check for local options
--
-- Notes: 1) <esc>.( and <esc>.) are used to bring plotter on and off line
--           since these seem to be the old default.  Newer <esc>.Y and <esc>.Z
--           may be implemented someday, but not now.
============================================================================ */
LOGICAL HPGL_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	INTEGER	i;
	INTEGER	bufmax,							/* Maximum possible buffer	*/
				bufsiz;							/* Buffer size (from PLINI) */
	INTEGER	SizeSet,SizeIndex;
	REAL		sizex,sizey;					/* Temp variables */
	CHAR		line[20], token[DFLT_STR_SIZE];

/* -------------------------------------------------------------
   Handle first part of initialization separate.  Allocate memory 
   and start up the I/O channel process with its own junk
-------------------------------------------------------------- */
	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("HP-GL", dsp->ini.IO_Channel, IOC_TEXT | IOC_CRLF, IOF_NOFLOW);
		if (blk->IO_Block == NULL) return(FALSE);
		io = blk->IO_Block;
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
			blk->ldev2 = abs(dsp->ini.SubDevice);
			if ((blk->ldev2 == 0) || (blk->ldev2 > NDEV)) return(FALSE);
			blk->ldev2--;											/* Decrement to 0 based */
			blk->IniHPGL = dsp->ini.SubDevice > 0;			/* Use initialization? */

			blk->numpens  = dsp->ini.NumberPens;
			if (blk->numpens < 1) blk->numpens = 1;
			if (blk->numpens > devpar[blk->ldev2].NumberPens) blk->numpens = devpar[blk->ldev2].NumberPens;
			dsp->ini.NumberPens = blk->numpens;

			dsp->ini.xperinch = NPERIN;						/* Points/inch			*/
			dsp->ini.yperinch = NPERIN;						/* Points/inch			*/
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS |	/* Supports graphs	*/
											DEV_CAP_CURSOR   |	/* Can do cursors		*/
											DEV_CAP_MARKERS  |	/* Can do some markers */
											DEV_CAP_CLIP;			/* Will get clip msgs */

			SizeSet = devpar[blk->ldev2].PageSet;			/* Which page set?	*/
			SizeIndex = abs(dsp->ini.Options);
			SizeIndex = max(0, min(SizeIndex, 4));
			if (SizeIndex == 0) {
				UserInput("Page size X,Y in inches (no checks!): ", token, sizeof(token));
				sscanf(token,"%f %f",&sizex,&sizey);
				dsp->ini.xmax = (INTEGER) sizex*NPERIN;
				dsp->ini.ymax = (INTEGER) sizey*NPERIN;
			} else {
				SizeIndex--;													/* Decrement to 0 based */
				dsp->ini.xmax = PaperSize[SizeSet][SizeIndex].x;	/* Pixels in X */
				dsp->ini.ymax = PaperSize[SizeSet][SizeIndex].y;	/* Pixels in Y */
			}

			bufmax = devpar[blk->ldev2].BufSize;				/* Max allowed		*/
			bufsiz = io->BufSize;									/* Current size	*/
			if (bufsiz>=bufmax) IO_SetBuffer(io,bufmax-8);	/* Modify?			*/
			bufsiz = min(io->BufSize, bufmax);					/* And result		*/

			blk->TTYOut   = (io->Abilities & IOA_TERMINAL) != 0;	/* Output to TTY? */
			blk->EnqAck   = (io->FlowControl == IOF_ENQ);	/* ENQ/ACK */
			blk->XonXoff  = (io->FlowControl == IOF_XON);	/* XON/XOFF */

/* ... Determine if the plotter has a chance to do page feeds or paper test */
			blk->CanDoFF    = (devpar[blk->ldev2].Flags & FEED)   ? TRUE : FALSE ;
			blk->PaperCheck = (devpar[blk->ldev2].Flags & DETECT) ? TRUE : FALSE ;
			blk->CanDoFill  = (devpar[blk->ldev2].Flags & FILL)   ? TRUE : FALSE ;
			if ( ! (io->Abilities & IOA_READ)) blk->PaperCheck = FALSE;

/* ... Initialize the specific devices */
			blk->pltmod  = TRUE;								/* Not in plot mode now */
			blk->dirty   = FALSE;							/* No vectors drawn yet */
			blk->newpen  = TRUE;								/* New value now */
			blk->pendown = FALSE;							/* And pen is not down now! */
			blk->xl = blk->yl = 0;							/* Clipping limits	*/
			blk->xh = blk->yh = 32000;
			blk->wpen    = 1;									/* Start with pen 1 */
			blk->ixloc   = -1;								/* Pen location unknown */

/*-----------------------------------------------------------------------------
-- ... We treat all devices as if they will be using XON/XOFF unless specified
-- ... otherwise.  Allows for files, but assumes whatever spools the file can
-- ... handle it.  GPIB will ignore any such commands anyway.
--        <esc>.(	   => Bring the plotter online
--        <esc>.J	   => Abort Device Control
--        <esc>.K	   => Abort any graphics currently in progress
--	 <esc>.T	   => Reset memory allocations to poweron default
--			      (not done since I may not see it finished)
--        <esc>.M:          => Set <CR> as output terminator.  
--		              No turnaround delay     No output trigger char
--		              No echo terminate char  No output initiator char
----------------------------------------------------------------------------- */
			
			if (blk->IniHPGL) {							/* Initialize at all? */
				IO_fputs(ONLINE,io);						/* Plotter on */
				IO_fputs("\033.J",io);					/* Abort device control */
				IO_fputs("\033.K",io);					/* Abort graphics in progress */
/*				IO_fputs("\033.T",io);	*/				/* Reset memory allocations */
				IO_fputs("\033.M:",io);					/* Set output mode */
				if (blk->XonXoff) {						/* Use default XON/XOFF mode? */
/*-----------------------------------------------------------------------------
--  <esc>.@;2:        => Logical I/O buffer = physical I/O buffer size
--		                   Disable CTS/DSR
--	 <esc>.N;19:       => Disable delay, ^S becomes XOFF character.
--  <esc>.I<buf>;;17: => XOFF goes <buf> below logical size. ;; specifies
--		                   XON/XOFF.  17 => Use ^Q as XON character.
----------------------------------------------------------------------------- */
					IO_fputs("\033.@;2:",io);			/* Configure plotter */
					IO_fputs("\033.N;19:",io);			/* Establish ^S as XOFF */
					if (bufsiz > 200) 					/* Can I deal w/ 81 character? */
						IO_fputs("\033.I81;;17:",io);	/* 81 (PRIME) level for XOFF */
					else
						IO_fputs("\033.I20;;17:",io);	/* 20 otherwise */
				} else if (blk->EnqAck) {						/* ENQ/ACK mode */
/*-----------------------------------------------------------------------------
--  <esc>.@;2:        => Logical I/O buffer = physical I/O buffer size
--                       Disable CTS/DSR
--	 <esc>.N:          => Disable delay, No immediate response char
--  <esc>.I<buf>;5;6: => Set BLOCK SIZE = <buf> (bufsiz+64 overwrite)
--                       Specify <5> and <6> as ENQ/ACK string
----------------------------------------------------------------------------- */
					i = min(bufsiz+64, bufmax-4);			/* Size to send */
					IO_fputs("\033.@;2:",io);				/* Configure plotter */
					IO_fputs("\033.N:",io);					/* Disable XOFF, delay */
					IO_fprintf(io, "\033.I%i;5;6:", i);

				} else {										/* Default hardware mode */
/*-----------------------------------------------------------------------------
--  <esc>.@;3         => Logical I/O buffer = physical I/O buffer size
--		                   Enable DTR, disable CTS/DSR
--	 <esc>.N:          => Disable delay, No immediate response char
----------------------------------------------------------------------------- */
					IO_fputs("\033.@;3:",io);			/* Configure plotter */
					IO_fputs("\033.N:",io);				/* Disable XOFF, delay */
				}
				IO_fflush(io);								/* FORCE ALL THIS OUT!!! */

				if (blk->CanDoFF) IO_fputs("PG;",io);	/* Feed a new page */
				if (! hppchk()) {							/* Check for pinch rollers */
					hpanmd();								/* Go offline */
					IO_CloseChannel(io);					/* Close down device */
					return(FALSE);
				}
				blk->pltmod = TRUE;						/* We are in plot mode now */
			}

			IO_fputs("IN;FT2;PU;",io);					/* Initialize with pen up */
/* ---------------------------------------------------------------------------
-- Some old stuff for setting the PA1 and PA2 points on plotter.
--			p1x = (xmarg+xorg)*factr
--			p1y = (ymarg+yorg)*factr
--			p2x = (xsize-xmarg+xorg)*factr
--			p2y = (ysize-ymarg+yorg)*factr
--       if (p1x>0&&p1y>0&&p2x>0&&p2y>0) then
--          call hpip(p1x,p1y,p2x,p2y)		      ! Set PA1 and PA2
--          call hpsc(p1x,p1y,p2x,p2y)		      ! Lock coordinates
--       endif
----------------------------------------------------------------------------- */
			hpanmd();										/* May be necessary */
			break;
			
		case LINFNC:										/* Draw line */

/* -------------------- draw vector ----------------------------------------*/
			if (! blk->pltmod) hppmod();
			if (blk->newpen) {
				if (blk->pendown) IO_fputs("PU;",io);
				IO_fprintf(io, "SP%i;", blk->wpen);			/* Pen color change */
				blk->pendown = FALSE;
				blk->newpen  = FALSE;							/* Current pen now installed */
			}

			if ( (blk->ixloc != dsp->line.x1)  ||  (blk->iyloc != dsp->line.y1) ) {
				if (blk->pendown) IO_fputs("PU;",io);		/* Force pen up */
				IO_fprintf(io, "PA%i,%i;", dsp->line.x1, dsp->line.y1);
				blk->pendown = FALSE;
			}
			if (! blk->pendown) IO_fputs("PD;",io);			/* Pen down */
			blk->ixloc = dsp->line.x2;
			blk->iyloc = dsp->line.y2;
			IO_fprintf(io, "PA%i,%i;", dsp->line.x2, dsp->line.y2);
			blk->pendown = TRUE;
			blk->dirty   = TRUE;
			break;

/* -------------------- erase -------------------- */
		case ERSFNC:					/* Erase screen */
			break;						/* Impossible   */

/* -------------------- alphanumeric mode --------------------------------- */
/* -------------------- frame --------------------------------------------- */
/* -------------------- flush -------------------- implicit anmode -------- */
		case FRMFNC:					/* End of frame */
			if (blk->dirty && blk->CanDoFF) IO_fputs("PG;",io);	/* Get the next page */
			blk->dirty = FALSE;
			if (blk->pltmod) {
				if (blk->pendown) IO_fputs("PU;",io);		/* Lift pen */
				blk->pendown = FALSE;							/* Remind for next time */
				hpanmd();											/* Clear buffer */
			}
			break;

		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
		case FLSFNC:					/* Flush all buffers */
			if (blk->pltmod) {
				if (blk->pendown) IO_fputs("PU;",io);		/* Lift pen */
				blk->pendown = FALSE;							/* Remind for next time */
				hpanmd();											/* Clear buffer */
			}
			break;

/* -------------------- end ------------------------------------------------- */
		case ENDFNC:					/* End of plot */
			if (! blk->pltmod) hppmod();
			IO_fputs("SP;PU;PA0,0;",io);					/* Pen in stall, lower left */
			blk->ixloc = -1;									/* Location moved */
			blk->pendown = FALSE;							/* Pen is up */

			if (blk->IniHPGL) IO_fputs(OFFLINE,io);	/* Clean up - Take off-line */
			blk->pltmod = FALSE;								/* Far out of plot mode */
			IO_fflush(io);										/* Empty buffer */
			IO_CloseChannel(io);
			free(blk);											/* Free my memory usage */
			break;

/* -------------------- change color -------------------------- */
/* -- NOTE, HPGL uses the pen number instead of the color index */
/* ------------------------------------------------------------ */
		case COLFNC:								/* Set color */

			i = max(1, min(dsp->col.pen, blk->numpens));		/* Pen desired */
			blk->newpen = blk->newpen || (i != blk->wpen);
			blk->wpen = i;
			break;

/* -------------------- change speed -------------------- */
		case SPDFNC:					/* Set pen speed */
			if (! blk->pltmod) hppmod();
			if (dsp->spd.pens == 0)				/* Speed command w/o pen select */
				IO_fprintf(io, "VS%i;", dsp->spd.speed);		
			else										/* Speed command w/ pen select */
				IO_fprintf(io, "VS%i,%i;", dsp->spd.speed, dsp->spd.pens);
			hpanmd();
			break;

/* -------------------- set visibility -------------------- */
		case VISFNC:					/* Set visibility (light, dark, complement) */
			break;

/* -------------------- read cursor -------------------- */
		case CURFNC:					/* Read cursor function */
			if (! blk->pltmod) hppmod();
			blk->ixloc = -1;										/* Assume we move */
			dsp->cur.achr = '\0';								/* And set default response */
			if (IO_GetInput(io, line, sizeof(line),13,"OS;") <= 0) {		/* Cursor available? */
				hpanmd();
				return(FALSE);
			}
			IO_fputs("DP;",io);							/* Turn on ENTER light */
			IO_fflush(io);									/* Force out */

/* ... Wait until user has pushed 'enter' button */
			do {
				MilliSleep(100);							/* Sleep 0.1 seconds */
				IO_GetInput(io,line,sizeof(line),13,"OS;");	/* Request status */
				sscanf(line, "%i", &i);					/* Interpret the status */
			} while ( (i & 0x04) == 0);				/* Wait for bit 2 to be set */

			IO_GetInput(io,line,sizeof(line),13,"OD;");	/* Request digitizer output */
			sscanf(line, "%i %i %i", &dsp->cur.x, &dsp->cur.y, &i);
			if (i != 0) 
				dsp->cur.achr = '0';
			else
				dsp->cur.achr = '3';
			break;

/* -------------------- point plot ----------------------------------------- */
		case PNTFNC:												/* Plot a single point */
			if (! blk->pltmod) hppmod();
			if (blk->pendown) IO_fputs("PU;",io);			/* Make sure pen is up */
			if (blk->newpen)	IO_fprintf(io,"SP%i;",blk->wpen);	/* Pen color change */
			if ( (blk->ixloc != dsp->point.x) || (blk->iyloc != dsp->point.y) ) {
				IO_fprintf(io, "PA%i,%i;", dsp->point.x, dsp->point.y);
				blk->ixloc = dsp->point.x;							/* New pen location */
				blk->iyloc = dsp->point.y;
			}
			IO_fputs("PD;PU;",io);					/* Pen down and up */

			blk->newpen  = FALSE;					/* Current pen now installed */
			blk->pendown = FALSE;
			blk->dirty   = TRUE;
			break;

/* -------------------- Draw a symbol marker ------------------ */
		case DRAWMARKER:
			return(DrawMarker(dsp->mark.isym, dsp->mark.x, dsp->mark.y, dsp->mark.size, dsp->mark.angle));

/* -------------------- Set IOCTL flags -----------------------  */
		case IOCTL:						/* Transfer of information only */
			break;

/* -------------------- Receive the current clipping ---------- */
		case TELLCLIP:						/* Just save clip limits */
			i = (blk->xl != dsp->clip.xl) || (blk->xh != dsp->clip.xh) ||
				 (blk->yl != dsp->clip.yl) || (blk->yh != dsp->clip.yh);
			blk->xl = dsp->clip.xl;
			blk->yl = dsp->clip.yl;
			blk->xh = dsp->clip.xh;
			blk->yh = dsp->clip.yh;
			if (i) IO_fprintf(io, "IW%i,%i,%i,%i\n", blk->xl, blk->yl, blk->xh, blk->yh);
			break;

		default:
			return(FALSE);
			
	}
	return(TRUE);
}


/* ============================================================================
-- Usage Guide:
--
--     DrawMarker - possibly draw one of the markers (closed one important)
--
--     Usage: LOGICAL DrawMarker(isym, x,y, size, angle)
--
--     Inputs: isym - symbol (Use logical defines MARK_SQUARE, ...)
--             x,y  - center position
--             size - size (in device coordinates)
--             angle - angle relative to landscape baseline (degrees 0,90,180,270)
--
--     Output: TRUE  - successfully drawn
--             FALSE - not drawn - try other methods
============================================================================ */
/* (asterisk, cross, x, stardavid) not done because require multiple stroke */
/* Coordinates are in .001" units for simplicity */
#if 0				/* Square/Filled Square handled separately */
static COORD f_square[]  = {	{-190,  190}, {-190, -190},
										{ 190, -190}, { 190,  190},
										{0,0}	};
static COORD square[]    = {	{-238, -238}, {-238,  238},
										{ 238,  238}, { 238, -238},
										{-238, -238},
										{0,0}	};
#endif
static COORD f_trian[]   = {	{   0,  214}, {-238, -214},
										{ 238, -214}, {   0,  214},
										{0,0}	};
static COORD f_l_trian[] = {	{-214,    0}, { 214, -238},
										{ 214,  238}, {-214,    0},
										{0,0}	};
static COORD f_r_trian[] = {	{ 214,    0}, {-214,  238},
										{-214, -238}, { 214,    0},
										{0,0}	};
static COORD f_star[]    = {	{   0,  286}, {-190, -238},
										{ 286,   95}, {-286,   95},
										{ 190, -238}, {   0,  286},
										{0,0}	};
static COORD triangle[]  =	{	{   0,  286}, {-333, -286},
										{ 333, -286}, {   0,  286},
										{0,0}	};
static COORD diamond[]   = {	{   0,  476}, {-286,    0},
										{   0, -476}, { 286,    0},
										{   0,  476},
										{0,0}	};
static COORD star[]      = {	{   0,  429}, { -95,  143},
										{-381,  143}, {-143,  -48},
										{-238, -333}, {   0, -143},
										{ 238, -333}, { 143,  -48},
										{ 381,  143}, {  95,  143},
										{   0,  429},
	 									{0,0}	};

PRIVATE LOGICAL DrawMarker(int isym, int x, int y, int size, int angle) {

	int i, dx, dy, dx2, dy2, edge, theta;
	char *fmt;
	COORD *path;
	enum {PATH, FILLED, SQUARE, CIRCLE, F_SQUARE, F_CIRCLE} itype;

	angle = (angle+45)/90;							/* Convert angle to 0...3		*/
	if (angle < 0 || angle > 3) angle = 0;

	switch (isym) {
		case MARK_CIRCLE:
			itype = CIRCLE;								break;
		case MARK_FILLED_CIRCLE:
			itype = F_CIRCLE;								break;
		case MARK_FILLED_SQUARE:
			itype = F_SQUARE;								break;
		case MARK_SQUARE:
			itype = SQUARE;								break;
		case MARK_TRIANGLE:
			itype = PATH;		path = triangle;		break;
		case MARK_DIAMOND:
			itype = PATH;		path = diamond;		break;
		case MARK_STAR:
			itype = PATH;		path = star;			break;
		case MARK_FILLED_TRIANGLE:
			itype = FILLED;	path = f_trian;		break;
		case MARK_FILLED_LEFTTRIANGLE:
			itype = FILLED;	path = f_l_trian;		break;
		case MARK_FILLED_RIGHTTRIANGLE:
			itype = FILLED;	path = f_r_trian;		break;
		case MARK_FILLED_STAR:
			itype = FILLED;	path = f_star;			break;
		case MARK_CROSS:
		case MARK_X:
		case MARK_ASTERISK:
		case MARK_STAROFDAVID:
		default:
			return(FALSE);
	}
	if (itype==FILLED && !blk->CanDoFill) return(FALSE);

	if (! blk->pltmod) hppmod();					/* Have paper						*/
	if (blk->pendown) IO_fputs("PU;", io);		/* Start pen up					*/
	if (blk->newpen) {								/* And chnage pen if needed	*/
		IO_fprintf(io, "SP%i;", blk->wpen);		/* Pen color change				*/
		blk->newpen  = FALSE;						/* Current pen now installed	*/
	}

	switch (itype) {
		case SQUARE:
			edge = max(3, (int) ( 0.476 * size + 0.5)) ;
			dx   = x - edge/2;
			dy   = y - edge/2;
			dx2  = x + (edge+1)/2;
			dy2  = y + (edge+1)/2;
			IO_fprintf(io, "PA%i,%i;EA%i,%i;PU;\n",
				dx, dy, dx2, dy2);
			break;

		case F_SQUARE:					/* Filled block 0.380*size on edge */
			edge = max(3, (int) ( 0.38 * size + 0.5)) ;
			dx   = x - edge/2;
			dy   = y - edge/2;
			dx2  = x + (edge+1)/2;
			dy2  = y + (edge+1)/2;
			IO_fprintf(io, "PA%i,%i;RA%i,%i;EA%i,%i;PU;\n",
				dx, dy, dx2, dy2, dx2, dy2);
			break;

		case CIRCLE:					/* draw circle w/ radius 0.34*size */
			edge = (int) (size*0.34+0.5);
			if (edge >= NPERIN)
				theta = 5;
			else if (edge >= (NPERIN+2)/4)
				theta = 10;
			else if (edge >= (NPERIN+5)/9)
				theta = 15;
			else if (edge >= (NPERIN+8)/16)
				theta = 20;
			else if (edge >= (NPERIN+18)/36)
				theta = 30;
			else if (edge >= (NPERIN+40)/80)
				theta = 45;
			IO_fprintf(io, "PA%i,%i;CI%i,%i;PU;\n", x,y, edge,theta);
			break;

		case F_CIRCLE:					/* draw circle w/ radius 0.20*size */
			edge = (int) (size*0.20+0.5);
			if (edge >= NPERIN)
				theta = 5;
			else if (edge >= (NPERIN+2)/4)
				theta = 10;
			else if (edge >= (NPERIN+5)/9)
				theta = 15;
			else if (edge >= (NPERIN+8)/16)
				theta = 20;
			else if (edge >= (NPERIN+18)/36)
				theta = 30;
			else if (edge >= (NPERIN+40)/80)
				theta = 45;
			IO_fprintf(io, "PA%i,%i;WG%i,0,360,%i;CI%i,%i;PU;\n",
				x,y, edge,theta, edge,theta);
			break;

		case PATH:
			i = 0;
			while (path->x != 0 || path->y != 0) {
				if (i == 0) fmt = "PA%i,%i;";
				if (i == 1) fmt = "PD%i,%i";
				if (i == 2) fmt = ",%i,%i";
				if (angle == 0) {									/* Deal with angles */
					dx = path->x;
					dy = path->y;
				} else if (angle == 3) {
					dx =  path->y;
					dy = -path->x;
				} else if (angle == 2) {
					dx = -path->x;
					dy = -path->y;
				} else if (angle == 1) {
					dx = -path->y;
					dy =  path->x;
				}
				dx = (int) (dx*size/1000.0 + x + 0.5);
				dy = (int) (dy*size/1000.0 + y + 0.5);
				IO_fprintf(io, fmt, dx,dy);
				path++; i++;
			}
			IO_fputs(";PU;\n", io);
			break;

		case FILLED:
			i = 0;
			while (path->x != 0 || path->y != 0) {
				if (i == 0) fmt = "PA%i,%i;PM0;";
				if (i == 1) fmt = "PD%i,%i";
				if (i == 2) fmt = ",%i,%i";
				if (angle == 0) {									/* Deal with angles */
					dx = path->x;
					dy = path->y;
				} else if (angle == 3) {
					dx =  path->y;
					dy = -path->x;
				} else if (angle == 2) {
					dx = -path->x;
					dy = -path->y;
				} else if (angle == 1) {
					dx = -path->y;
					dy =  path->x;
				}
				dx = (int) (dx*size/1000.0 + x + 0.5);
				dy = (int) (dy*size/1000.0 + y + 0.5);
				IO_fprintf(io, fmt, dx,dy);
				path++; i++;
			}
			IO_fputs(";PM2;FP;EP;PU;\n", io);
			break;

	}

	blk->ixloc = blk->iyloc = -1;						/* Pen out of position */
	blk->pendown = FALSE;
	blk->dirty   = TRUE;
	return(TRUE);
}


/* ============================================================================
-- Usage Guide:
--
--     HPPCHK - Check the status of the pinch rollers
--
--     Usage: LOGICAL = HPPCHK ()
--
--     Inputs: common block only
--
--     Output: HPPCHK - TRUE  if pinch rollers are down,
--                      FALSE otherwise.
--
--     Note: Will not check unless either RS232 or IEEE488 type line
============================================================================ */
PRIVATE LOGICAL hppchk(void) {

	CHAR		buff[11];
	INTEGER	statme,errnum;

	if (! blk->PaperCheck) return(TRUE);						/* Can"t do it */

/* ... Step 1 - Check status and clear any pending errors */
	do {
		if (IO_GetInput(io,buff,sizeof(buff),13,"OS;") <= 0) return(FALSE);
		sscanf(buff, "%i", &statme);						/* Read Status */
		if ( (statme & 0x20) == 0) break;				/* Clear errors */
		IO_GetInput(io,buff,sizeof(buff),13,"OE;");	/* Read error number */
		sscanf(buff, "%i", &errnum);
	} while (errnum != 1);									/* Loop till okay */


/* ... Step 2 - Check paper status and continue when ready */
	if (statme & 0x10) return(TRUE);						/* "READY FOR DATA"? */
	UserInput("Plotter not ready.  Press <CR> when ok ...",buff,sizeof(buff));
	IO_GetInput(io, buff,sizeof(buff),13,"OS;");		/* Read status again */
	sscanf(buff, "%i", &statme);							/* Status */
	return ( (statme & 0x10) ? TRUE : FALSE );
}


/* ============================================================================
-- Usage Guide:
--
--     HPPMOD - Set HP into plot mode (for plotters attached to terminals)
--
--     Usage: CALL HPPMOD
--
--     Inputs: (none) Taken from common blocks
--
--     Output: (none) Only to terminal
============================================================================ */
PRIVATE void hppmod(void) {

	if ( (! blk->pltmod) && blk->TTYOut && blk->IniHPGL) {				/* TTY line? */
		IO_fputs(ONLINE,io);										/* Online now! */
		IO_fflush(io);												/* Force out! */
	}
	blk->pltmod = TRUE;
	return;
}


/* ============================================================================
-- Usage Guide:
--
--     HPANMD - Return plotter to terminal mode (for online terminals)
--
--     Usage: CALL HPANMD
--
--     Inputs: (none) Taken from common blocks
--
--     Output: (none) Only output to terminals
============================================================================ */
PRIVATE void hpanmd(void) {

	if (blk->TTYOut && blk->pltmod && blk->IniHPGL) {				/* Only  for blk->TTYOut */
		IO_fputs(OFFLINE,io);								/* Take offline */
		blk->pltmod = FALSE;									/* Now out of plotmode */
	}
	IO_fflush(io);												/* Flush buffers */
	return;
}

#ifdef OLDFORTRAN
c ============================================================================
c Usage Guide:
c
c     Subroutine HPIP - Set plotter points PA1 and PA2
c
c     Sets PA1 and PA2 points on plotter to specific coordinates on the
c     bed.  Should be called after each initialization of plotter.
c     Use HPSC to tie coordinates to PA1 and PA2 also which allows user
c     to set the frame boundaries by changing PA1 and PA2
c
c     Usage: CALL HPIP(P1X,P1Y,P2X,P2Y)
c
c     Inputs: P1X,P1Y - Coordinates of PA1 plotter point (in inches)
c             P2X,P2Y - Coordinates of PA2 plotter point (in inches)
c ============================================================================
      subroutine hpip(p1x,p1y,p2x,p2y)
      implicit none
      real p1x,p1y,p2x,p2y
c
      external plcout,plnout,plout
      intrinsic int
c
      call plcout("IP",2)
      call plnout(int(p1x*1016))             /* Coordinates of PA1 point */
      call plcout(",",1)
      call plnout(int(p1y*1016))             /* Y value */
      call plcout(",",1)
      call plnout(int(p2x*1016))             /* Coordinates of PA2 point */
      call plcout(",",1)
      call plnout(int(p2y*1016))             /* Y value */
      call plcout(";",1)
      call plout
      return
      end
c
c ============================================================================
c Usage Guide:
c
c     Subroutine HPSC - Tie coordinates to PA1 and PA2 points on HP plotter
c
c     Accompanying routine for HPIP to tie coordinates to PA1 and PA2. All
c     future references will be assumed using PA1 and PA2 as these values
c     nomatter where they lie on paper.
c     The points PA1 and PA2 should be tied to real inches before this call
c     via a call to HPIP.
c
c     Usage: CALL HPSC(P1X,P1Y,P2X,P2Y)
c
c     Inputs: P1X,P1Y - Coordinates of PA1 plotter point (in inches)
c             P2X,P2Y - Coordinates of PA2 plotter point (in inches)
c ============================================================================
      subroutine hpsc(p1x,p1y,p2x,p2y)
      implicit none
      real*4 p1x,p1y,p2x,p2y
c
      external plcout,plnout,plout
      intrinsic int
c
      call plcout("SC",2)
      call plnout(int(p1x*1016))             /* Coordinates of PA1 point */
      call plcout(",",1)
      call plnout(int(p2x*1016))             /* Coordinates of PA2 point */
      call plcout(",",1)
      call plnout(int(p1y*1016))             /* Y value */
      call plcout(",",1)
      call plnout(int(p2y*1016))             /* Y value */
      call plcout(";",1)
      call plout
      return
      end

#endif
