/* pin24.c */

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
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "io_chan.h"
#include "vsort.h"
#include "xtrn.h"

#include "drvclass.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define DEFAULT_FILENAME	"pin24.out"

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
#define	FORMFEED		"\f"
#define	LINEFEED		"\x0D\033J\030"		/* Line feed = ^[J^X = 24 ministeps */

#define	SET180LPI	"\033[\\" "\x4\0\0\0\x4\xB"
#define	SET216LPI	"\033[\\" "\x4\0\0\0\x8\xD"

/* ============================================================================
-- Subroutine to handle 24 pin printers, including EPSON, NEC and IBM
--
-- Usage:  logical = pin24(key)
--
-- Inputs: key    - operation requested
--           = 00 - Initialize
--           = 01 - Initialize
--           = 02 - Output raster information
--           = 03 - Shut down
--
-- Output: bytes via PLCOUT and PLIOUT
--         pin24 - success
--
-- ... LQ-500    24 pin driver
-- ... NEC P2200 24 pin driver
-- ... IBM X24E  24 pin driver
============================================================================ */
LOGICAL Pin24(int key) {

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	static	unsigned char *OutputBuffer=NULL;
	static	size_t OutputBufferSize;
	static	char GraphStart[5]="huh?";
	int		i,j,k,l;
	BITMAPQ	*Ptr, pattern;
	unsigned	char ibit, *obuf;

	switch (key) {

/* -----------------------------------------
-- Initialize command
----------------------------------------- */
		case 0:
			if (io == NULL) {
				io = IO_OpenChannel("VSORT", TransferInfo.IO_Chan, IOC_BINARY, IOF_NOFLOW);
				if (io == NULL) {
					io = IO_OpenChannel("VSORT", "file*" DEFAULT_FILENAME, IOC_BINARY, IOF_NOFLOW);
					gen_err("Had to try opening " DEFAULT_FILENAME " in current directory");
				}
			}

			ScansPerBand = 24;				/* Bands must be 24 bits wide */

			if (TransferInfo.Class == LQ500 || TransferInfo.Class == LQ800) {
				strcpy(GraphStart, "\033* ");
				if (TransferInfo.yperinch == 60) {
					GraphStart[2] = 32;
				} else if (TransferInfo.yperinch == 120) {
					GraphStart[2] = 33;
				} else {
					GraphStart[2] = 39;
				}
			} else if (TransferInfo.Class == IBM_X24E) {		/* Brain dead IBM */
				IO_fwrite(SET180LPI, 9,1, io);					/* Set vertical units */
				strcpy(GraphStart, "\033[g ");
				if (TransferInfo.yperinch == 60) {				/* 60 dpi */
					GraphStart[3] = 8;
				} else if (TransferInfo.yperinch == 120) {	/* 120 dpi */
					GraphStart[3] = 9;
				} else if (TransferInfo.yperinch == 180) {	/* 180 dpi */
					GraphStart[3] = 11;
				}
			}
			Dirty       = FALSE;

			OutputBufferSize = 3*MapWidth*BITS_PER_MAP;
			OutputBuffer = malloc(OutputBufferSize);
			break;
		
/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) IO_fputs(FORMFEED, io);
			Dirty = FALSE;									/* No longer dirty */
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			if (TransferInfo.Class == IBM_X24E) 		/* Brain dead IBM */
				IO_fwrite(SET216LPI, 9,1, io);			/* Set vertical units */
			if (Dirty && (! NoFormFeed)) IO_fputs(FORMFEED, io);
			IO_CloseChannel(io);	io = NULL;				/* Close the channel */
			free(OutputBuffer);
			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

/* ---------------------------------------------------------------------------
-- Must switch the map from across page to down page
--
-- Data is spread across 3 bytes going down page
--
-- BitMap[00] - word 00 pin 0x80
-- Bitmap[01] - word 00 pin 0x40
-- ...
-- BitMap[07] - word 00 pin 0x01
-- Bitmap[08] - word 01 pin 0x80
-- BitMap[09] - word 01 pin 0x40
-- .....
-- BitMap[15] - word 01 pin 0x01
-- BitMap[16] - word 02 pin 0x80
-- BitMap[17] - word 02 pin 0x40
-- ....
-- BitMap[22] - word 02 pin 0x02
-- BitMap[23] - word 02 pin 0x01
--------------------------------------------------------------------------- */
			memset(OutputBuffer, 0, OutputBufferSize);
			for (i=0; i<3; i++) {							/* Treat as 3 sets of 8	*/
				for (j=0,ibit=0x80; j<8; j++,ibit>>=1) {	/* 24 pins = 3*8		*/
					Ptr = BitMap + (8*i+j)*MapWidth;		/* First word of pin		*/
					obuf = OutputBuffer+i;					/* fill in output buf	*/
					for (k=0; k<MapWidth; k++) {			/* All elements			*/
						pattern = Ptr[k];						/* Next n pixels across */
						for (l=0; l<BITS_PER_MAP; l++) {	/* Do bits in pattern	*/
							if (pattern & 0x01) *obuf |= ibit;
							obuf += 3; pattern >>= 1;		/* Next positions			*/
						}
					}
				}
			}

/* Count number of non-zero output bytes */
			j = MapWidth*BITS_PER_MAP*3;
			while (j>0 && OutputBuffer[j-1] == 0) j--;

/* And output data and/or linefeed as necessary */
			if (j > 0) {
				j = ((j+2)/3) * 3;							/* Nearest 3 block! */
				i = (TransferInfo.Class == IBM_X24E) ? j+1 : j/3;	/* # of "words" */
				fflush(stdout);
				IO_fputs(GraphStart, io);					/* Enter graphic block */
				IO_fputc((i&0xFF), io);
				IO_fputc((i>>8),   io);
				IO_fwrite(OutputBuffer, j, 1, io);		/* Output the data */
			}

			if (j != 0 || Dirty || ! NoBlanks) {	/* Should we put a LF out? */
				Dirty = TRUE;
				IO_fputs(LINEFEED, io);
			}
			return(TRUE);


		default:
			return(FALSE);
	}
	return(TRUE);
}
