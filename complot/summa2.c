/* **************************************************************** */
/* summagraphics bit pad 2 driver                                   */
/*                                                                  */
/* Mike Thompson - 11/7/91                                          */
/* Computer Graphics Service                                        */
/* Ithaca NY                                                        */
/*                                                                  */
/* Original Coding: Mike Uttormark                                  */
/*                                                                  */
/* To avoid real confusion one needs to have handy                  */
/*    1) Summagraphics Bit Pad Two Data Tablet Technical Reference  */
/* **************************************************************** */

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
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "complot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define XLEN        11								/* Length in X direction */
#define YLEN        11								/* Length in Y direction */
#define XPERINCH   200								/* Resolution in X */
#define YPERINCH   200								/* Resolution in X */
#define MAXX       XPERINCH*XLEN					/* Maximum # pixels in X */
#define MAXY       YPERINCH*YLEN					/* Maximum # pixels in Y */

#define RESETPAD 		"kQ"							/* Command to reset tablet */
#define SENDPOSIMM 	"ST"							/* Send an immediate posn */
#define POINTMODE		"P"							/* Put in point mode */

typedef struct _DriverBlock {						/* These need to quasi-static */
   int iunit;
	int ounit;
	enum {UNKNOWN, POINT, STREAM, SWITCHED} mode;
	char name[PATH_MAX];
	char PointMode;
	char SwitchMode;
	char StreamMode;
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL Summa2_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE void cvrt_pos(int iunit, int *x, int *y, int *achr) ;

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

PRIVATE DRVBLOCK *blk;									/* Pointer to local block */

LOGICAL Summa2_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	static char SwitchSpeeds[] = "P@ABCDEFG";		/* Options 0 - 8 rates */
	static char StreamSpeeds[] = "MMMMMMMMM";		/* Options 0 - 8 rates */
	int i;

	if (key == TABINIFNC) {
		if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) 
		   return(FALSE);
		strcpy(blk->name, dsp->ini.IO_Channel);	/* Name of device to open	*/
	} else {
		blk = DriverBlock;								/* local copy (changeable) */
	}

/* -------------------------------------------------------------
   Rest of module is one huge case statement;
-------------------------------------------------------------- */
	switch (key) {

/* --------------- Initialize ----------------------- */
		case TABINIFNC:

			i = max(0, min(8, dsp->ini.Options));		/* Get in range */
			blk->PointMode  = 'P';
			blk->SwitchMode = SwitchSpeeds[i];
			blk->StreamMode = StreamSpeeds[i];

	  		dsp->ini.xperinch = XPERINCH;
			dsp->ini.yperinch = YPERINCH;
			dsp->ini.xmax     = MAXX;
			dsp->ini.ymax     = MAXY;
			dsp->ini.Capabilities = DEV_CAP_TABLET;

			if ( (blk->iunit=open(blk->name,O_RDONLY|O_NOCTTY)) == -1) {
				free(blk);
			   return(FALSE);
			} else if ( (blk->ounit=open(blk->name,O_WRONLY|O_NOCTTY)) == -1) {
				close(blk->iunit);
				free(blk);
				return(FALSE);
			}

			write(blk->ounit,RESETPAD,2);					/* Reset pad now	 */
			tcdrain(blk->ounit);								/* And be sure out */
			sleep(1);											/* Sleep init time */
			tcflush(blk->iunit, TCIFLUSH);				/* Clear all input */
			blk->mode = UNKNOWN;								/* Not in any mode */
			break;
			
		case TABENDFNC:
			close(blk->ounit);								/* close files */
			close(blk->iunit);
			free(blk);
			break;

		case TABGETPOSN:										/* Immediate posn		*/
			tcflush(blk->iunit, TCIFLUSH);				/* Clear any pending */
			write(blk->ounit, SENDPOSIMM, 2);			/* Set for immediate */
			tcdrain(blk->ounit);								/* Wait to be output */
			cvrt_pos(blk->iunit, &dsp->cur.x, &dsp->cur.y, &dsp->cur.achr);
		   blk->mode = UNKNOWN;
			break;

		case TABCLEARMODE:						/* Clear input modes and flush */
			if (blk->mode != POINT) {
				tcflush(blk->iunit, TCIFLUSH);
				write(blk->ounit, &blk->PointMode, 1);
				tcdrain(blk->ounit);
				blk->mode = POINT;
			}
			tcflush(blk->iunit, TCIFLUSH);
			break;
			
	   case TABGETPOINT:							/* Return on user input */
			if (blk->mode != POINT) {
			   tcflush(blk->iunit, TCIFLUSH);
				write(blk->ounit, &blk->PointMode, 1);
				tcdrain(blk->ounit);
				blk->mode = POINT;
			}
			cvrt_pos(blk->iunit, &dsp->cur.x, &dsp->cur.y, &dsp->cur.achr);
			break;

	   case TABGETSWITCH:							/* Return on user input */
			if (blk->mode != SWITCHED) {
			   tcflush(blk->iunit, TCIFLUSH);
				write(blk->ounit, &blk->SwitchMode, 1);
				tcdrain(blk->ounit);
				blk->mode = SWITCHED;
			}
			cvrt_pos(blk->iunit, &dsp->cur.x, &dsp->cur.y, &dsp->cur.achr);
			break;

	   case TABGETSTREAM:							/* Return on user input */
			if (blk->mode != STREAM) {
			   tcflush(blk->iunit, TCIFLUSH);
				write(blk->ounit, &blk->StreamMode, 1);
				tcdrain(blk->ounit);
				blk->mode = STREAM;
			}
			cvrt_pos(blk->iunit, &dsp->cur.x, &dsp->cur.y, &dsp->cur.achr);
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Routine to read characters from iunit and then translate into appropriate
-- coordinates.  From the digitizer, we get a message of the form:
--
--   0124,2248,3<CR><LF>
--
-- Usage:  void cvrt_pos(int iunit, int *x, int *y, int *achr);
--
-- Inputs: iunit - fildes open for reading (tablet hopefully)
--
-- Output: *x, *y, *achr - interpreted string from digitizer
--------------------------------------------------------------------------- */
PRIVATE void cvrt_pos(int iunit, int *x, int *y, int *achr) {

	char response[20], *ptr=response;

	while (ptr-response < sizeof(response)) {
		read (iunit, ptr, 1);					/* Get a character */
		if (*ptr == '\n') break;				/* Take <LF> as end of message */
		if (*ptr != '\r') ptr++;				/* But ignore <CR> */
	}

	*x    = (int) strtol(response, &ptr, 10);		/* Interpret X coordinate */
	*y    = (int) strtol(ptr+1,    &ptr, 10);		/* Interpret Y coordinate */
	*achr = (int) strtol(ptr+1,    NULL, 10);		/* And interpret key press */
	return;
}
