/* DSPTCH - Dispatch routine for all graphics functions */
/* ============================================================================
--     WPG_Driver - COMPLOT driver for WP version 5.0
--
--     Usage: LOG = WPG_Driver(cmd, PARMS)
--
--
--     Inputs: cmd   - Command - See driver.ins for definitions
--             PARMS - Variable dimensioned array with parameters for transfer
--                     Type and direction depend on command.
--
--     Return: TRUE - Successful operation
--             FALSE - Failed operation
============================================================================ */
#if ! (defined CSET2 || defined MSC60 || defined MSC70)

	#define _POSIX_SOURCE						/* Always require POSIX standard */
	#include "preload.h"

	#include <stdlib.h>
	#include <stdio.h>
	#include "mytypes.h"
	#include "extends.h"
	#include "complot.h"

	typedef struct _DRVBLOCK DRVBLOCK;

	LOGICAL WPG_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);
	
	LOGICAL WPG_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {
		if (key == INIFNC)
			ERRputs("ERROR: Word Perfect driver is not functional on this architecture\n");
		return(FALSE);
	}

#else 

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define WPG_Driver SlaveDriver
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

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

typedef struct _DRVBLOCK {						/* These need to quasi-static */
	void		*IO_Block;							/* Carry over to IO channel	*/
	INTEGER	linwid,								/* Line width parameter			*/
				numpens,								/* Number of pens configured	*/
				ipen,									/* Pen color						*/
				page_x, page_y,
				width, height;
	LOGICAL	running;								/* Are we actually running		*/
} DRVBLOCK;

#define	DPI					1200.0f			/* Pixel resolution/inch			*/
#define	PALETTE_SIZE		8					/* Number of elements in palette	*/

#define	DefaultLineWidth	7						/* Default linewidth				*/
#define	NumPaperSizes		4						/* Number of paper sizes 		*/
#define	MaxPenColor			(PALETTE_SIZE-1)	/* 0-7 colors possible			*/

#define	WPG_LINE_ATTRIBUTES		2	/* Word Perfect function definitions	*/
#define	WPG_MARKER_ATTRIBUTES	3
#define	WPG_DRAW_MARKER			4
#define	WPG_DRAW_VECTOR			5
#define	WPG_DRAW_POLYLINE			6
#define	WPG_COLOR_MAP				14
#define	WPG_START_DATA				15
#define	WPG_END_DATA				16
			
typedef struct _POINT {
	short x,y;
} POINT;

#pragma pack(1)						/* Force to byte boundaries */
typedef struct _RGB {
	unsigned char red,green,blue;
} RGB;

#pragma pack()

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL WPG_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static struct {REAL x,y;} PaperSize[] =
					{ {11.0f, 11.0f},				/* 11x11  paper */
					  {8.0f,  10.5f},				/* 8.5x11 paper */
					  {8.0f,  13.5f},				/* 8.5x14 paper */
					  {10.5f, 16.5f} };			/* 11x17  paper */

#pragma pack(1)						/* Force to byte boundaries */
	static struct _wpg_prefix {
		unsigned char Identifier[4];	/* 255, 'WPC'				*/
		long	Start_Adr;					/* This if first so 16	*/
		char	Product_Type;				/* Product type = 1		*/
		char	File_Type;					/* File type is 0x16		*/
		char	Major_Version;				/* Version is 1			*/
		char	Minor_Version;				/* Minor version is 0	*/
		short	Encrypt_Key;				/* No encryption			*/
		short	Reserved;					/* Reserved must be 0	*/
	} wpg_prefix = { {255, 'W', 'P', 'C'}, 0x10, 1, 0x16, 1, 0, 0, 0};

#define	DOTBUFSIZE	30
	static struct _dots {
		unsigned char type;
		unsigned char length;
		short	npt;							/* Number of points		*/
		POINT pt[DOTBUFSIZE];
	} *dots=NULL;
	static int dotcnt=0;

#ifdef UNUSED_FOR_NOW
#define	LINEBUFSIZE	30
	static struct _lines {
		unsigned char type;
		unsigned char length;
		short	npt;							/* Number of points		*/
		POINT pt[LINEBUFSIZE];
	} *lines=NULL;
	static int linecnt=0;
#endif

#pragma pack()													/* Return to default boundaries */

static RGB	colormap[PALETTE_SIZE] = {
			{0xFF, 0xFF, 0xFF},								/* 0 - white	*/
			{   0,    0,    0},								/* 1 - black	*/
			{0xFF,    0,    0},								/* 2 - red		*/
			{   0, 0xFF,    0},								/* 3 - green	*/
			{   0,    0, 0xFF},								/* 4 - blue		*/
			{0xFF,    0, 0xFF},								/* 5 - magenta	*/
			{   0, 0xFF, 0xFF},								/* 6 - cyan   	*/
			{0xFF, 0xFF,    0} };							/* 7 - yellow	*/


static void FlushDot(IO_BLOCK *io, DRVBLOCK *blk) {

#pragma pack(1)												/* Force to byte boundaries */

	struct {
		unsigned char type;
		unsigned char length;
		char	marker_style;									/* Nominally 1 always				*/
		char	color;
		short	marker_width;
	} rec;

#pragma pack()													/* Return to default boundaries */

	if (dots != NULL && dotcnt != 0) {

		rec.type   = WPG_MARKER_ATTRIBUTES;				/* Starting marker attributes */
		rec.length = 4;										/* char+char+short				*/
		rec.marker_style = 3;								/* Asterisk							*/
		rec.color        = blk->ipen;
		rec.marker_width = blk->linwid;
		IO_fwrite(&rec, rec.length+2, 1, io);

		dots->npt    = dotcnt;
		dots->type   = WPG_DRAW_MARKER;
		dots->length = 4*dotcnt + 2;						/* 2 + 2*2*NPT */
		IO_fwrite(dots, dots->length+2, 1, io);

		rec.marker_style = 5;								/* Square					*/
		IO_fwrite(&rec, rec.length+2, 1, io);

		IO_fwrite(dots, dots->length+2, 1, io);		/* And draw again */
	}
	dotcnt = 0;
	return;
}


/* ----------------------------------------------------------------------- */
LOGICAL WPG_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	DRVBLOCK	  *blk;
	IO_BLOCK	  *io;
	int i;

#pragma pack(1)						/* Force to byte boundaries */

	struct {
		unsigned char		type;
		unsigned char		length;
		union {
			struct _line_attributes {		/* Rec 2/4 - Line_Attributes		*/
				char	line_style;				/* Nominally 1 always				*/
				char	color;
				short	line_width;
			} lt;
			struct _marker_attributes {	/* Rec 3/4 - poly_Attributes		*/
				char	marker_style;			/* Nominally 1 always				*/
				char	color;
				short	marker_width;
			} mt;
			struct _draw_marker {			/* Rec 4/8 - draw_vector			*/
				short	npt;						/* Number of points (hope 1)		*/
				short x,y;						/* Coordinate							*/
			} mk;	
			struct _draw_vector {			/* Rec 5/8 - draw_vector			*/
				short x1,y1;					/* Starting coordinate				*/
				short x2,y2;					/* Ending coordinates				*/
			} dr;	
			struct _color_map {
				short	startindex;
				short	numindex;
				RGB	colors[PALETTE_SIZE];
			} cm;
			struct _wpg_start_data {		/* Rec 15/6 - wpg_start_data		*/
				char	version;					/* Must be 1							*/
				char	flags;					/* Normally 0							*/
				short	width;					/* Width of figure					*/
				short	height;					/* Height of figure					*/
			} st;
			struct _wpg_end_data {			/* Rec 16/0 - wpg_end_data			*/
				char	none;						/* No actual data in record		*/
			} end;
		} data;
	} rec;

#pragma pack()							/* Return to default boundaries */


/* -------------------------------------------------------------
   Handle first part of initialization separate.  Allocate memory 
   and start up the I/O channel process with its own junk
-------------------------------------------------------------- */
	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("WORDPERFECT", dsp->ini.IO_Channel, IOC_BINARY, IOF_NOFLOW);
		if ( (io = blk->IO_Block) == NULL) return(FALSE);
	} else {
		blk = DriverBlock;						/* My local copy (so can change) */
		io  = blk->IO_Block;
		if ( (! blk->running) && (key != ENDFNC)) return(FALSE);
		if (IO_ferror(io) != 0) {
			ERRputs("ERROR: WORDPERFECT output device is in error -- did the pipe break?\n"
					  "       Shutting down device.  Reinitialize plot device before continuing\n");
			blk->running = FALSE;				/* No longer running */
			return(FALSE);
		}
	}

/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {

/* --------------- Initialize ----------------------- */
		case INIFNC:

			i = max(1, min(NumPaperSizes, dsp->ini.Options));			/* Paper Index */
			blk->page_x = (INTEGER) (DPI * PaperSize[i-1].x + 0.5f);	/* Pixels in X */
			blk->page_y = (INTEGER) (DPI * PaperSize[i-1].y + 0.5f);	/* Pixels in Y */

			dsp->ini.xperinch = (INTEGER) DPI;		/* 1200 pixels/inch */
			dsp->ini.yperinch = (INTEGER) DPI;		/* 1200 pixels/inch */
			dsp->ini.xmax = blk->page_x;
			dsp->ini.ymax = blk->page_y;
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS;	/* Supports graphs	*/

			blk->linwid  = DefaultLineWidth;				/* Reset on frame */
			blk->numpens = max(1, min(dsp->ini.NumberPens, MaxPenColor));
			blk->ipen    = 1;
			blk->running = FALSE;							/* We are not up/running */

			blk->ipen   = 1;									/* Color 1 */
			blk->linwid = 8;									/* Linewidth 0.007 */
			blk->height = 0;									/* Height of plot (unknown) */
			blk->width  = 0;									/* Width of plot (unknown) */

			IO_fwrite(&wpg_prefix, sizeof(wpg_prefix), 1, io);

			rec.type   = WPG_START_DATA;				/* WPG_START_DATA record */
			rec.length = sizeof(rec.data.st);
			rec.data.st.version = 1;
			rec.data.st.flags   = 0;
			rec.data.st.width   = 0;					/* Don't know yet			 */
			rec.data.st.height  = 0;					/* Nor him at this point */
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_LINE_ATTRIBUTES;		/* Starting line attributes */
			rec.length = sizeof(rec.data.lt);
			rec.data.lt.line_style = 1;
			rec.data.lt.color      = blk->ipen;
			rec.data.lt.line_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_MARKER_ATTRIBUTES;		/* Starting marker attributes */
			rec.length = sizeof(rec.data.mt);
			rec.data.mt.marker_style = 1;
			rec.data.mt.color        = blk->ipen;
			rec.data.mt.marker_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_COLOR_MAP;
			rec.length = sizeof(rec.data.cm);
			rec.data.cm.startindex = 0;
			rec.data.cm.numindex   = PALETTE_SIZE;
			for (i=0; i<PALETTE_SIZE; i++) {
				rec.data.cm.colors[i].red   = colormap[i].red;
				rec.data.cm.colors[i].green = colormap[i].green;
				rec.data.cm.colors[i].blue  = colormap[i].blue;
			}
			IO_fwrite(&rec, rec.length+2, 1, io);

			blk->running = TRUE;
			break;

/* ----------------------- point plot ----------------------------------------- */
/* -------------------- draw vector ---------------------------------------- */
		case PNTFNC:												/* Plot a single point */
		case LINFNC:												/* Draw line */

			if (key==PNTFNC || (dsp->line.x1==dsp->line.x2 &&
									  dsp->line.y1==dsp->line.y2) ) {
				if (dots == NULL) {
					dots = malloc(sizeof(*dots));
					dotcnt = 0;
				}
				if (key == PNTFNC) {
					dots->pt[dotcnt].x = dsp->point.x;
					dots->pt[dotcnt].y = dsp->point.y;
				} else {
					dots->pt[dotcnt].x = dsp->line.x1;
					dots->pt[dotcnt].y = dsp->line.y1;
				}
				blk->width  = max(blk->width,  dots->pt[dotcnt].x);	/* Track extent of box */
				blk->height = max(blk->height, dots->pt[dotcnt].y);

				if (++dotcnt >= DOTBUFSIZE) FlushDot(io, blk);

			} else {
				rec.type   = WPG_DRAW_VECTOR;
				rec.length = sizeof(rec.data.dr);
				rec.data.dr.x1 = dsp->line.x1;
				rec.data.dr.y1 = dsp->line.y1;
				rec.data.dr.x2 = dsp->line.x2;
				rec.data.dr.y2 = dsp->line.y2;

				/* Track extent of box */
				blk->width  = max(blk->width,max(rec.data.dr.x1,rec.data.dr.x2));
				blk->height = max(blk->height,max(rec.data.dr.y1,rec.data.dr.y2));

				IO_fwrite(&rec, rec.length+2, 1, io);
			}
			break;

/* -------------------- erase -------------------- */
		case ERSFNC:					/* Erase screen */
			blk->width = blk->height = 0;
			IO_fseek(io, 24, SEEK_SET);				/* Set file position at 24 */

			rec.type   = WPG_LINE_ATTRIBUTES;		/* Starting line attributes */
			rec.length = sizeof(rec.data.lt);
			rec.data.lt.line_style = 1;
			rec.data.lt.color      = blk->ipen;
			rec.data.lt.line_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_MARKER_ATTRIBUTES;		/* Starting marker attributes */
			rec.length = sizeof(rec.data.mt);
			rec.data.mt.marker_style = 1;
			rec.data.mt.color        = blk->ipen;
			rec.data.mt.marker_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_COLOR_MAP;
			rec.length = sizeof(rec.data.cm);
			rec.data.cm.startindex = 0;
			rec.data.cm.numindex   = PALETTE_SIZE;
			for (i=0; i<PALETTE_SIZE; i++) {
				rec.data.cm.colors[i].red   = colormap[i].red;
				rec.data.cm.colors[i].green = colormap[i].green;
				rec.data.cm.colors[i].blue  = colormap[i].blue;
			}
			IO_fwrite(&rec, rec.length+2, 1, io);

			break;

/* -------------------- flush -------------- implicit anmode -------- */
		case FLSFNC:					/* Flush all buffers */
			break;

/* -------------------- frame -------------------- */
		case FRMFNC:					/* End of frame */
			break;

/* -------------------- end ---------------------- */
		case ENDFNC:										/* End of plot */

			if (dots != NULL) {							/* Any dots drawn? */
				FlushDot(io, blk);						/* Flush them		 */
				free(dots);									/* And free the memory */
				dots = NULL;
			}

			rec.type = WPG_END_DATA;					/* Mark end of data */
			rec.length = 0;
			IO_fwrite(&rec, rec.length+2, 1, io);

			IO_fseek(io, 16, SEEK_SET);				/* Go back to beginning */
			rec.type   = WPG_START_DATA;				/* WPG_START_DATA record */
			rec.length = sizeof(rec.data.st);
			rec.data.st.version = 1;
			rec.data.st.flags   = 0;
			rec.data.st.width   = blk->width;		/* Now known */
			rec.data.st.height  = blk->height;		/* Now known */
			IO_fwrite(&rec, rec.length+2, 1, io);

			IO_CloseChannel(io);
			free(blk);										/* Free my memory usage */	
			break;

/* -------------------- change color --------------------------- */
/* -------------------- Set line linewidth --------------------- */
		case LWFNC:									/* Line width function */
		case COLFNC:								/* Set color function  */

			FlushDot(io, blk);

			if (key == LWFNC) {
				blk->linwid = min(255,max(0,dsp->lw.linewidth));
				blk->linwid = (6*blk->linwid)/5;
			} else if (key == COLFNC) {
				blk->ipen = min(blk->numpens, max(0,dsp->col.brush.closest_index));
			}

			rec.type   = WPG_LINE_ATTRIBUTES;		/* Starting line attributes */
			rec.length = sizeof(rec.data.lt);
			rec.data.lt.line_style = 1;
			rec.data.lt.color      = blk->ipen;
			rec.data.lt.line_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			rec.type   = WPG_MARKER_ATTRIBUTES;		/* Starting marker attributes */
			rec.length = sizeof(rec.data.mt);
			rec.data.mt.marker_style = 1;
			rec.data.mt.color        = blk->ipen;
			rec.data.mt.marker_width = blk->linwid;
			IO_fwrite(&rec, rec.length+2, 1, io);

			break;

/* -------------------- alphanumeric mode -------------------- */
		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
			break;

/* -------------------- change speed -------------------- */
		case SPDFNC:					/* Set pen speed */
			break;

/* -------------------- set visibility -------------------- */
		case VISFNC:					/* Set visibility (light, dark, complement) */
			return(FALSE);

/* -------------------- read cursor -------------------- */
		case CURFNC:					/* Read cursor function */
			return(FALSE);

/* -------------------- Begin panel --------------------  */
		case PANFNC:					/* Panel function */
			return(FALSE);

/* -------------------- End panel --------------------  */
		case POFFNC:					/* Panel off function */
			return(FALSE);

		case DRAWCHAR:					/* Text drawing functions */
			return(FALSE);
			
		default: 
			return(FALSE);
	}

	return(TRUE);

}

#endif /* ! (defined CSET2 || defined MSC60) */
