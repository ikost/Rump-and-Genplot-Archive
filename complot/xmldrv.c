/* DSPTCH - Dispatch routine for all graphics functions */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifdef SLAVE
	#define XML_Driver SlaveDriver
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

#define	XDPI		(1000)
#define	YDPI		(1000)
#define	XMAX		(11000)
#define	YMAX		(8500)
#define	SCALE		(0.10f)						/* Coordinates multiplied by this value to give # for file */

typedef struct _DRVBLOCK {						/* These need to quasi-static */
	void		*IO_Block;							/* Carry over to IO channel */
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL XML_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

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
-- XML_Diver - Null driver for testing software
--
-- Usage: LOGICAL XML_Driver
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
LOGICAL XML_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

/* Device parms */
	DRVBLOCK	  *blk;
	IO_BLOCK	  *io;
	static int r=0,g=0,b=0;					/* RGB colors */

	if (key == INIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) return(FALSE);
		blk->IO_Block = IO_OpenChannel("XML", dsp->ini.IO_Channel, IOC_TEXT, IOF_NOFLOW);
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
			break;
		case INIFNC:					/* Initialize */
			dsp->ini.xperinch     = XDPI;
			dsp->ini.yperinch     = YDPI;
			dsp->ini.xmax         = XMAX;
			dsp->ini.ymax         = YMAX;
			dsp->ini.Capabilities = 0;							/* No capabilities */
			IO_fprintf(io, 
						  "<?xml version=\"1.0\"?>\n"
						  "<?xml-stylesheet type=\"text/xsl\" href=\"#stylesheet\"?>\n"
						  "<!DOCTYPE doc [\n"
						  "<!ATTLIST xsl:stylesheet\n"
						  "  id	ID	#REQUIRED>\n"
						  "]>\n"
						  "<graph>\n"
						  "<xsl:stylesheet id=\"stylesheet\"\n"
						  "                version=\"1.0\"\n"
						  "                xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\"\n"
						  "                xmlns=\"http://www.w3.org/2000/svg\">\n"
						  "   <xsl:output method=\"xml\" version=\"1.0\" encoding=\"UTF-8\" indent=\"yes\" media-type=\"image/svg\" />\n"
						  "   <!-- any xsl:import elements -->\n"
						  "  <xsl:template match=\"xsl:stylesheet\" />\n"
						  "  <xsl:template match='/'>\n"
						  "    <svg width=\"100%%\" height=\"100%%\" version=\"1.1\"\n"
						  "         xmlns=\"http://www.w3.org/2000/svg\">\n"
						  "    <xsl:apply-templates />\n"
						  "    </svg>\n"
						  "  </xsl:template>\n"
						  "  <xsl:template match='line'>\n"
						  "    <xsl:param name='x1' select='@x1' />\n"
						  "    <xsl:param name='y1' select='@y1' />\n"
						  "    <xsl:param name='x2' select='@x2' />\n"
						  "    <xsl:param name='y2' select='@y2' />\n"
						  "    <xsl:param name='r' select='@r' />\n"
						  "    <xsl:param name='g' select='@b' />\n"
						  "    <xsl:param name='b' select='@g' />\n"
						  "    <line x1='{$x1}' y1='{$y1}' x2='{$x2}' y2='{$y2}' stroke='rgb({$r},{$g},{$b})' stroke-width='2' />\n"
						  "  </xsl:template>\n"
						  "\n"
						  "</xsl:stylesheet>\n"
						 );
			break;
		case LINFNC:					/* Draw line */
			IO_fprintf(io,"<line x1='%f' y1='%f' x2='%f' y2='%f' r='%d' g='%d' b='%d' />\n",
				SCALE*dsp->line.x1, SCALE*(YMAX-dsp->line.y1), SCALE*dsp->line.x2, SCALE*(YMAX-dsp->line.y2), r,g,b);
			break;
		case ERSFNC:					/* Erase screen */
			break;
		case FLSFNC:					/* Flush all buffers */
			break;
		case FRMFNC:					/* End of frame */
			break;
		case ENDFNC:					/* End of plot */
			IO_fprintf(io, "</graph>");
			IO_CloseChannel(io);
			free(blk);
			break;
		case COLFNC:					/* Set color */
			r = (dsp->col.brush.rgb & 0x00ff0000) >> 16;
			g = (dsp->col.brush.rgb & 0x0000ff00) >> 8;
			b = (dsp->col.brush.rgb & 0x000000ff);
			break;
		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
			break;
		case SPDFNC:					/* Set pen speed */
			break;
		case VISFNC:					/* Set visibility (light, dark, complement) */
			break;
		case CURFNC:					/* Read cursor function */
			break;
		case PNTFNC:					/* Plot a single point */
			break;
		case PANFNC:					/* Panel function */
			break;
		case POFFNC:					/* Panel off function */
			break;
		case LWFNC:										/* Line width function */
			break;
		case GRPFNC:					/* Graphics screen dump function */
			break;
		case PAGFNC:					/* Select page function */
			break;
		case REGFNC:					/* Region erase function */
			break;
		case BEGINPATH:
			break;
		case ENDPATH:
			break;
		case FILLPATH:
			break;
		case STROKEPATH:
			break;

		case FILFNC:					/* Region fill function */
			break;
		case TXTFNC:					/* Text drawing functions */
			break;
		case IOCTL:						/* Transfer of information only */
			break;
		case CURTRK:					/* Tracking cursor */
			break;
		case CURBOX:					/* Box cursor */
			break;
		case DRAWCHAR:
			break;
		case DRAWMARKER:
			break;
		case TELLCLIP:
			break;
		default: 
			break;
	}
	return(TRUE);
}
