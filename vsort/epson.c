/* epson.c */

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

#define DEFAULT_FILENAME	"epson.out"

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

/* ============================================================================
-- Subroutine to handle EPSON MX-80 type printers (including GEMINI)
--
-- Usage:  logical = Epson(key)
--
-- Inputs: key    - operation requested
--           = 00 - Initialize
--           = 01 - Initialize
--           = 02 - Output raster information
--           = 03 - Shut down
--
-- Output: bytes via PLCOUT and PLIOUT
--         dmdriv - success
============================================================================ */
LOGICAL Epson(int key) {

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	static	unsigned char *OutputBuffer=NULL;
	static	size_t OutputBufferSize;
	int		i,j,k,l;
	BITMAPQ	*Ptr, pattern;
	unsigned	char ibit, *obuf;

	static char *esck,										/* Set horizontal DPI=60 */
					MajorLineFeed[5]="\033J\001\015",	/* Line Feed (full size) */
					MinorLineFeed[5]="\033J\001\015";	/* Partial pixel line feed */
	static int  InterleaveFreq;

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
			InterleaveFreq = TransferInfo.xperinch/72;
			ScansPerBand = 8*InterleaveFreq;
			esck = (TransferInfo.yperinch != 60) ? "\033L" : "\033K" ;

/* Gemini is 16/144 pixels/LF, Epson is 24/216 pixels/LF */
			i = (TransferInfo.Class == GEMINI) ? 16 : 24 ;
			MajorLineFeed[2] = (char) (i-(InterleaveFreq-1)); /* 24/216 pixels/lf or 16/144 */
			Dirty = FALSE;									/* Don't need a FF yet */

			OutputBufferSize = MapWidth*BITS_PER_MAP;
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
			if (Dirty && (! NoFormFeed)) IO_fputs(FORMFEED, io);
			IO_CloseChannel(io);	io = NULL;			/* Close the channel			*/
			free(OutputBuffer);
			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

/* First see if we should just ignore this line scan */
			if (! Dirty && NoBlanks) {
				i = MapWidth*ScansPerBand;
				while (i>0 && BitMap[i-1] == 0) i--;
				if (i == 0) return(TRUE);
			}
			Dirty = TRUE;

/* ---------------------------------------------------------------------------
-- Must switch the map from across page to down page
--
-- Data is interleaved (possibly) to deal with only 8 pins but 24 pin
-- resolution.
--
-- BitMap[00] - interleave 0  pin 0x80
-- Bitmap[01] - interleave 1  pin 0x80
-- BitMap[02] - interleave 2  pin 0x80
-- BitMap[03] - interleave 0  pin 0x40
-- Bitmap[04] - interleave 1  pin 0x40
-- BitMap[05] - interleave 2  pin 0x40
-- .....
-- BitMap[21] - interleave 0  pin 0x01
-- Bitmap[22] - interleave 1  pin 0x01
-- BitMap[23] - interleave 2  pin 0x01
--------------------------------------------------------------------------- */
			for (i=0; i<InterleaveFreq; i++) {
				memset(OutputBuffer, 0, OutputBufferSize);
				for (j=0,ibit=0x80; j<8; j++,ibit>>=1) {	/* Start at top wire &	*/
					Ptr = BitMap + (i + j*InterleaveFreq)*MapWidth;
					obuf = OutputBuffer;						/* fill in output buf	*/
					for (k=0; k<MapWidth; k++) {			/* All elements			*/
						pattern = Ptr[k];						/* Next n pixels across */
						for (l=0; l<BITS_PER_MAP; l++) {	/* Do bits in pattern	*/
							if (pattern & 0x01) *obuf |= ibit;
							obuf++; pattern >>= 1;			/* Next positions			*/
						}
					}
				}

/* Count number of non-zero output bytes */
				j = (int) OutputBufferSize;
				while (j > 0 && OutputBuffer[j-1] == 0) j--;

				if (j > 0) {									/* Number to output		*/
					IO_fputs(esck, io);
					IO_fputc( (j&0xFF), io);
					IO_fputc( (j>>8),   io);
					IO_fwrite(OutputBuffer, j, 1, io);
				}
				IO_fputs( (i==(InterleaveFreq-1)) ? MajorLineFeed : MinorLineFeed , io);
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
