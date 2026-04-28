/* okidata.c */

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

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define DEFAULT_FILENAME	"okidata.out"

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
#define	EnterGraphics		"\034\003"			/* 12 cpi, APA graphics			*/
#define	ExitGraphics		"\003\002"			/* Exit graphics					*/
#define	ResetPrinter		"\030"				/* Cancel ==> reset all			*/
#define	FormFeed				"\003\002\f\003"	/* Exit APA, <FF>, enter APA	*/
#define	LineFeed				"\003\016"			/* <3><14> Graphics <LF>		*/

/* ============================================================================
-- Subroutine to handle OKIDATA printers.
--
-- Usage:  logical = okidata(key)
--
-- Inputs: key    - operation requested
--           = 00 - Initialize
--           = 01 - Initialize
--           = 02 - Output raster information
--           = 03 - Shut down
--
-- Output: bytes via PLCOUT and PLIOUT
--         dmdriv - success
--
-- Handles Okidata printers Microline 92/93
============================================================================ */
LOGICAL Okidata(int key) {

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	static	unsigned char *OutputBuffer;
	static	size_t OutputBufferSize;
	int		i,j;
	BITMAPQ	*Ptr, pattern;
	unsigned char ibit, *obuf, *eptr;

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
			ScansPerBand = 7;								/* Need 7 wires each time */
			IO_fputs(EnterGraphics, io);				/* 12 cpi graphics <28><3> */
			Dirty = FALSE;									/* Don't need a FF yet */

			OutputBufferSize = MapWidth*BITS_PER_MAP;
			OutputBuffer = (unsigned char *) malloc(OutputBufferSize);
			break;

/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) IO_fputs(FormFeed, io);
			Dirty = FALSE;									/* No longer dirty */
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			IO_fputs(ExitGraphics, io);				/* Exit graphics mode */
			if (Dirty && (! NoFormFeed)) IO_fputc('\f', io);
			if (! NoReset) IO_fputs(ResetPrinter, io);
			IO_CloseChannel(io);	io = NULL;			/* Close the channel			*/
			free(OutputBuffer);
			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

/* ... Now, switch the map from across page to down page */
			Ptr = BitMap;										/* Scan position */
			memset(OutputBuffer, 0, OutputBufferSize);
			for (ibit=0x01; ibit!=0x80; ibit <<= 1) {	/* Start at top wire */
				obuf = OutputBuffer;						/* From beginning of buffer */
				for (i=MapWidth; i; i--) {
					pattern = *(Ptr++);
					for (j=BITS_PER_MAP; j; j--) {	/* Do the bits in pattern */
						if (pattern & 0x01) *obuf |= ibit;
						obuf++;
						pattern >>= 1;
					}
				}
			}

/* ---------------------------------------------
Now, three steps
   1. Look how far anything is being output
	2. Convert 0x03 chars into 0x03/0x03 pairs
	3. Output chars to plotter plus a line feed
------------------------------------------------ */
			obuf = OutputBuffer+OutputBufferSize-1;			/* End of the band */
			while (*obuf==0 && obuf!=OutputBuffer) obuf--;
			if (obuf != OutputBuffer) {
				Dirty = TRUE;
				i = (int) (obuf-OutputBuffer) + 1;				/* Max number used */
				obuf = OutputBuffer;
				while ( i && ( (eptr=memchr(obuf,0x03,i)) != NULL) ) {
					j = (int) (eptr-obuf) + 1;					/* How far to <3>		*/
					IO_fwrite(obuf, j, 1, io);					/* Output to <3>		*/
					IO_fputc(0x03, io);							/* Output second <3.	*/
					i -= j;											/* Reduce char count	*/
					obuf = eptr+1;									/* And new pointer	*/
				}
				if (i) IO_fwrite(obuf, i, 1, io);			/* Anything left?		*/
				IO_fputs(LineFeed, io);
			} else if (Dirty || ! NoBlanks) {				/* Output empty line	*/
				Dirty = TRUE;
				IO_fputs(LineFeed, io);
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
