/* quietjet.c */

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
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stddef.h>

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

#define DEFAULT_FILENAME	"quietjet.out"

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
#define HPSelect					"\033%A"				/* Put in HP mode				*/
#define HPDeselect				"\033%@"				/* Return to default mode	*/
#define HPReset					"\033E"				/* Reset w/in HP mode		*/
#define HPSetResolution			"\033*t%iR"			/* Set the resolution		*/
#define HPEnterGraphics			"\033*rA"			/* Enter graphics mode		*/
#define HPSendRaster				"\033*b%iW"			/* Initial sequence			*/
#define HPExitGraphics			"\033*rB"			/* Exit Graphics mode		*/
#define HPFormFeed				"\f"					/* Formfeed						*/

/* ============================================================================
-- Subroutine to handle HP Laser Jet II output.  Utilizes as much as
-- intelligence as exists in the Laser Jet to reduce output size.
--
-- Usage:  logical = quietjet(key)
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
LOGICAL QuietJet(int key) {

	static		IO_BLOCK	*io=NULL;
	static		LOGICAL	Dirty;
	BITMAPQ		*PtrNext, *PtrNow, *Ptr;
	int			iband;
	ptrdiff_t	icnt;

	LOGICAL	IsBlank;

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
			ReverseBits = TRUE;								/* Reverse bit ordering */
			Dirty       = FALSE;

			if (! NoReset) {
				IO_fputs(HPSelect, io);						/* Select HP mode			*/
				IO_fputs(HPReset, io);						/* And reset printer		*/
			}
			IO_fprintf(io, HPSetResolution, TransferInfo.xperinch);
			IO_fputs(HPEnterGraphics,io);					/* Enter graphics mode	*/
			break;
		
/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) {
				IO_fputs(HPExitGraphics, io);				/* Exit graphics mode	*/
				IO_fputs(HPFormFeed, io);					/* FF output if dirty	*/
				IO_fputs(HPEnterGraphics, io);			/* Re-enter graphics		*/
			}
			Dirty = FALSE;										/* No longer dirty		*/
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			IO_fputs(HPExitGraphics,io);					/* Formally exit graphics */
			if (Dirty && (! NoFormFeed)) IO_fputs(HPFormFeed,io);
			if (! NoReset) {
				IO_fputs(HPReset, io);						/* Optional reset			*/
				IO_fputs(HPDeselect, io);					/* And reset to default	*/
			}
			IO_CloseChannel(io);
			io = NULL;
			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

			SWAB((char *)BitMap, (char *)BitMap, BitMapSize);

			PtrNext = BitMap;
			for (iband=0; iband<ScansPerBand; iband++) {

				Ptr = PtrNow = PtrNext;					/* This scan position */
				PtrNext += MapWidth;						/* And the next one */

/* -- Find first non-zero element (or determine line to be empty) */
				while (Ptr != PtrNext && *Ptr == 0) Ptr++;
				IsBlank = (Ptr == PtrNext);

				if (!Dirty && NoBlanks && IsBlank) continue;	/* Go to next band */
				Dirty = TRUE;								/* Page is now dirty */

				if (IsBlank) {
					IO_fprintf(io, HPSendRaster, 0);
				} else {										/* Find end of data */
					Ptr = PtrNext-1;
					while (*Ptr == 0) Ptr--;
					icnt = Ptr - PtrNow + 1;			/* Number of units output */
					IO_fprintf(io, HPSendRaster, icnt*sizeof(BITMAPQ));
					IO_fwrite(PtrNow, icnt*sizeof(BITMAPQ), 1, io);
				}
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
