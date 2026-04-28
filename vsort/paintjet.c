/* paintjet.c */

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

#define DEFAULT_FILENAME	"paintjet.out"

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
#define	HPReset					"\033E"					/* Reset in HP mode		*/
#define	HPSetPlanes				"\033*b1M\033*r%iU"
#define	HPSetColor				"\033*v%2.2iA\033*v%2.2iB\033*v%2.2iC\033*v%2.2iI"
#define	HPSetResolution		"\033*t%iR\033*r0A"
#define	HPTmpFormFeed			"\033*rB\f\033*r0A"
#define	HPExitGraphics			"\033*rB"			/* Exit Graphics mode		*/
#define	HPFormFeed				"\f"					/* Formfeed						*/
#define	HPSendRaster			"\033*b%i%c"		/* Initial sequence			*/

/* ============================================================================
-- Subroutine to handle HP PaintJet output.  Uses run length encoding to
-- output full length of the map.  Colors use a lookup table to get pixel info
-- and must be properly set.
--
-- Usage:  logical = PaintJet(key)
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
-- HP PaintJet color printers
--	0 => 180x180 dpi  1 color  (white/black)
--	1 => 180x180 dpi  3 color  (white,black,red,green)
--	2 => 180x180 dpi  7 color  (wh,bk,rd,gn,yl,bl,magenta,cyan)
--	3 => 180x180 dpi 15 color  (repeat of colors for device 2)
--	4 =>  90x 90 dpi  1 color  (white/black)
--	5 =>  90x 90 dpi  3 color  (white,black,red,green)
--	6 =>  90x 90 dpi  7 color  (wh,bk,rd,gn,yl,bl,magenta,cyan)
--	7 =>  90x 90 dpi 15 color  (wh,bk,...lots...)
============================================================================ */
LOGICAL PaintJet(int key) {

	static struct {
		int red,green,blue;
	} NTSC_RGB[] = {	{90, 88, 85},		/* White    (0) */
							{04, 04, 06},		/* Black		(1) */
							{53,  8, 14},		/* Red      (2) */
							{03, 26, 22},		/* Green    (3) */
							{04, 04, 29},		/* Blue     (4) */
							{53, 05, 25},		/* Magenta  (5) */
							{02, 22, 64},		/* Cyan		(6) */
							{89, 83, 13}		/* Yellow	(7) */
	};
	#define	NUM_RGB	(sizeof(NTSC_RGB) / sizeof(NTSC_RGB[0]))

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	static	unsigned char *OutputBuffer;
	BITMAPQ	*PtrNext, *PtrNow, *Ptr;
	int		iband;
	int		i,iout;

	LOGICAL	IsBlank;

	switch (key) {
		
/* -----------------------------------------------------------------------
-- Initialize command
-- ... Initial setup.  Reverse bit ordering
--        (1) Reverse bit ordering for HP
--        (2) Set to allow 8 colors (3 planes)
--	       (3) Set up the printer internal to agree with this decision
-------------------------------------------------------------------------- */
		case 0:
			if (io == NULL) {
				io = IO_OpenChannel("VSORT", TransferInfo.IO_Chan, IOC_BINARY, IOF_NOFLOW);
				if (io == NULL) {
					io = IO_OpenChannel("VSORT", "file*" DEFAULT_FILENAME, IOC_BINARY, IOF_NOFLOW);
					gen_err("Had to try opening " DEFAULT_FILENAME " in current directory");
				}
			}
			ReverseBits  = TRUE;								/* Reverse bit ordering */
			Dirty        = FALSE;
			ColorMax = max(1,min(15,TransferInfo.numpens));	/* # valid pens */

			if      (ColorMax == 1) NumPlanes = 1;		/* Number of bands */
			else if (ColorMax <= 3) NumPlanes = 2;
			else if (ColorMax <= 7) NumPlanes = 3;
			else							NumPlanes = 4;

/*			ScansPerBand /= NumPlanes;		*/				/* Reduce so ~same memory use */

/* ----------------------------------------------------
--     <esc>*r3U    ==> 3 planes per pixel position
--     <esc>*v90A<esc>*v88B<esc>*v85C<esc>*v0I  ==> Set pallette 0 to white
--     <esc>*v4A<esc>*v4B<esc>*v6C<esc>*v7I     ==> Set pallette N to black
--     <esc>*b1M    ==> use run length encoding
---------------------------------------------------- */

			if (! NoReset) IO_fputs(HPReset, io);		/* Output reset */
			IO_fprintf(io, HPSetPlanes, NumPlanes);

/* ... Set the pallette colors */
			for (i=0; i<NUM_RGB; i++)
				IO_fprintf(io, HPSetColor, NTSC_RGB[i].red, NTSC_RGB[i].green, NTSC_RGB[i].blue, i);

/* ... Set resolution and enter graphics mode */
			IO_fprintf(io, HPSetResolution, TransferInfo.xperinch);
			OutputBuffer = (unsigned char *) malloc(2*MapWidth*sizeof(BITMAPQ));
			break;

/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) IO_fputs(HPTmpFormFeed, io);
			Dirty = FALSE;									/* No longer dirty */
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			IO_fputs(HPExitGraphics,io);				/* Formally exit graphics	*/
			if (Dirty && (! NoFormFeed)) IO_fputs(HPFormFeed,io);
			if (! NoReset) IO_fputs(HPReset,io);	/* Optional reset				*/
			IO_CloseChannel(io);	io = NULL;			/* Close the channel			*/
			free(OutputBuffer);							/* Free all memory used		*/
			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

			SWAB((char *)BitMap, (char *)BitMap, BitMapSize);

			PtrNext = BitMap;
			for (iband=0; iband<(ScansPerBand*NumPlanes); iband+=NumPlanes) {

				Ptr = PtrNow = PtrNext;					/* This scan position */
				PtrNext += MapWidth*NumPlanes;		/* And the next one */

/* -- Find first non-zero element (or determine line to be empty) */
				while (Ptr != PtrNext && *Ptr == 0) Ptr++;
				IsBlank = (Ptr == PtrNext);

				if (!Dirty && NoBlanks && IsBlank) continue;	/* Go to next band */

				Dirty = TRUE;									/* Page is now dirty */
				for (i=0; i< NumPlanes; i++) {
					iout = (int) rll_encode((unsigned char *) PtrNow, MapWidth*sizeof(BITMAPQ), OutputBuffer);
					IO_fprintf(io, HPSendRaster, iout, (i==(NumPlanes-1)) ? 'W' : 'V');
					IO_fwrite(OutputBuffer, iout, 1, io);
					PtrNow += MapWidth;
				}
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
