/* DSPTCH - Dispatch routine for all graphics functions */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define Debug_Driver SlaveDriver
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

#include "complot.h"
#include "io_chan.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _DRVBLOCK {						/* These need to quasi-static */
	void		*IO_Block;							/* Carry over to IO channel */
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL Debug_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ---------------------------------------------------------------------------
-- DebugDriver - Null driver for testing software
--
-- Usage: LOGICAL Debug_Driver
--
-- Inputs: cmd   - Command - See COMPLOT.INS for definitions
--         PARMS - Variable dimensioned array with parameters for transfer
--                 Type and direction depend on command.
--
-- Output: DRVNUL - Success of operation
--
-- NOTE: DRVNUL is a character stream device.  Output channels may be
--       selected in DEVICES.DAT.  The RS-232 line format is enabled
--       with CR/LF following each line output.
--------------------------------------------------------------------------- */
LOGICAL Debug_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

/* Device parms */
	DRVBLOCK	  *blk;
	IO_BLOCK	  *io;

	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("POSTSCRIPT", dsp->ini.IO_Channel, IOC_TEXT, IOF_NOFLOW);
		if ( (io = blk->IO_Block) == NULL) return(FALSE);
	} else {
		blk = DriverBlock;						/* My local copy (so can change) */
		io  = blk->IO_Block;
	}


/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {
		case INFFNC:					/* Return information on device */
			IO_fprintf(io,"(DEBUG) Request for information\n");
			break;
		case INIFNC:					/* Initialize */
			IO_fprintf(io,"(DEBUG) Driver initialized w/ channel %s\n",dsp->ini.IO_Channel);
			IO_fprintf(io,"(DEBUG) Driver requests %d pens\n", dsp->ini.NumberPens);
			dsp->ini.xperinch     = 1000;
			dsp->ini.yperinch     = 1000;
			dsp->ini.xmax         = 11000;
			dsp->ini.ymax         = 8500;
			dsp->ini.Capabilities = DEV_CAP_GRAPHICS  |	/* Supports graphs	*/
											DEV_CAP_CURSOR    |	/* Can do cursors		*/
											DEV_CAP_MARKERS   |	/* Can do markers    */
											DEV_CAP_FONTS	   |	/* Can do characters  */
											DEV_CAP_GREEKFONT |	/* Can do greek characters */
											DEV_CAP_CLIP;			/* Will get clip msgs */
			break;
		case LINFNC:					/* Draw line */
			IO_fprintf(io,"(DEBUG) Vector requested (%7.3f,%7.4f) to (%7.3f,%7.3f)\n",
				dsp->line.x1/1000.f, dsp->line.y1/1000.f, dsp->line.x2/1000.f, dsp->line.y2/1000.f);
			break;
		case ERSFNC:					/* Erase screen */
			IO_fprintf(io,"(DEBUG) Screen erase\n");
			break;
		case FLSFNC:					/* Flush all buffers */
			IO_fprintf(io,"(DEBUG) Flush\n");
			break;
		case FRMFNC:					/* End of frame */
			IO_fprintf(io,"(DEBUG) Frame: %d orientation\n", dsp->frame.orient);
			break;
		case ENDFNC:					/* End of plot */
			IO_fprintf(io,"(DEBUG) End\n");
			IO_CloseChannel(io);
			free(blk);
			break;
		case COLFNC:					/* Set color */
			IO_fprintf(io,"(DEBUG) Color/pen change to %i: brush %i %i %8.8x\n",
				dsp->col.pen, dsp->col.brush.index, dsp->col.brush.closest_index, dsp->col.brush.rgb);
			break;
		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
			IO_fprintf(io,"(DEBUG) Alphanumeric mode\n");
			break;
		case SPDFNC:					/* Set pen speed */
			IO_fprintf(io,"(DEBUG) Speed change to: %i for pens %i\n",dsp->spd.speed, dsp->spd.pens);
			break;
		case VISFNC:					/* Set visibility (light, dark, complement) */
			IO_fprintf(io,"(DEBUG) Set visibility: %i\n",dsp->vis.visible);
			break;
		case CURFNC:					/* Read cursor function */
			IO_fprintf(io,"(DEBUG) Cursor read operation with display function %p\n",
					 dsp->cur.display);
			break;
		case PNTFNC:					/* Plot a single point */
			IO_fprintf(io,"(DEBUG) Point at    (%7.3f,%7.3f)\n",
				dsp->point.x/1000.f, dsp->point.y/1000.f);
			break;
		case PANFNC:					/* Panel function */
			IO_fprintf(io,"(DEBUG) Begin panel\n");
			break;
		case POFFNC:					/* Panel off function */
			IO_fprintf(io,"(DEBUG) End panel\n");
			break;
		case LWFNC:										/* Line width function */
			IO_fprintf(io,"(DEBUG) Linestyle change: %i\n",dsp->lw.linewidth);
			break;
		case GRPFNC:					/* Graphics screen dump function */
			IO_fprintf(io,"(DEBUG) Request screen dump\n");
			break;
		case PAGFNC:					/* Select page function */
			IO_fprintf(io,"(DEBUG) Request page change: %i %i\n",dsp->page.UsePage, dsp->page.ShowPage);
			break;
		case REGFNC:					/* Region erase function */
			IO_fprintf(io,"(DEBUG) Erase region command\n");
			break;
		case BEGINPATH:
			IO_fprintf(io,"(DEBUG) Begin path command\n");
			break;
		case ENDPATH:
			IO_fprintf(io,"(DEBUG) End path command\n");
			break;
		case FILLPATH:
			IO_fprintf(io,"(DEBUG) Fill path command: color %d  mode %d\n", dsp->fillpath.color, dsp->fillpath.mode);
			break;
		case STROKEPATH:
			IO_fprintf(io,"(DEBUG) Stroke path command: color %d  linewidth %d\n", dsp->strokepath.color, dsp->strokepath.linewidth);
			break;

		case FILFNC:					/* Region fill function */
			IO_fprintf(io,"(DEBUG) Fill region request\n");
			break;
		case TXTFNC:					/* Text drawing functions */
			IO_fprintf(io,"(DEBUG) Draw text function\n");
			break;
		case IOCTL:						/* Transfer of information only */
			IO_fprintf(io,"(DEBUG) IOCTL control transfer: %i\n",dsp->ioctl.ioctl);
			break;
		case CURTRK:					/* Tracking cursor */
			IO_fprintf(io,"(DEBUG) Engage tracking cursor\n");
			break;
		case CURBOX:					/* Box cursor */
			IO_fprintf(io,"(DEBUG) Engage box cursor\n");
			break;
		case DRAWCHAR:
			IO_fprintf(io,"(DEBUG) Draw char '%c' at %f %f, angle %d and size %d\n",
						  dsp->drwchr.chr,
						  dsp->drwchr.x/1000.0, dsp->drwchr.y/1000.0, dsp->drwchr.angle, 
						  dsp->drwchr.size);
			break;
		case DRAWMARKER:
			IO_fprintf(io,"(DEBUG) Draw marker %i at %f %f angle %f of height %i\n", 
						  dsp->mark.isym, 
						  dsp->mark.x/1000.0, dsp->mark.y/1000.0, 
						  dsp->mark.angle, dsp->mark.size);
			break;
		case TELLCLIP:
			IO_fprintf(io,"(DEBUG) Clip margins set: (%f,%f) (%f,%f)\n",
						  dsp->clip.xl/1000.0, dsp->clip.xh/1000.0, 
						  dsp->clip.yl/1000.0, dsp->clip.yh/1000.0);
			break;
		default: 
			IO_fprintf(io,"(DEBUG) Unknown key passed: %i\n",key);
	}
	return(TRUE);
}
