/* laserjet.c */

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

#define DEFAULT_FILENAME	"laser.out"

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
const char SetHPPD[] = {0x1B, '[', 'K', 15,0,		/* Length of strings			*/
								0x00, '1',						/* Mode = keep/delete/user	*/
								0x02,								/* Select HP Emulation		*/
								0,2,0,0,0,0,0,0,0,0,0,0};	/* DSOPT1-12					*/
const char SetPPDS[] = {0x1B, '[', 'K', 15,0,		/* Length of strings			*/
								0x00, '1',						/* Mode = keep/delete/user	*/
								0x01,								/* Select IBM PPDS mode		*/
								0,0,2,0,0,0,0,0,0,0,0,0};	/* DSOPT1-12					*/

#define HPReset					"\033E"				/* Reset in HP mode			*/
#define HPSetResolution			"\033*t%iR"			/* Set the resolution		*/

#define HPRasterAtLeft			"\033*r0A"			/* Enter graphics mode		*/
#define HPRasterAtCursor		"\033*r1A"			/* Start graphic at cursor	*/
#define HPSendRaster				"\033*b%iW"			/* Initial sequence			*/
#define HPExitGraphics			"\033*rB"			/* Exit Graphics mode		*/

#define HPFormFeed				"\f"					/* Formfeed						*/
#define HPSkipYPixels			"\033*p+%iY"		/* Skip pixels initial		*/
char    HPSkipOnThisLine[] =	"\033*p-1y%iX";	/* Skip on this line			*/
#define HPSkipOnNextLine		"\033*p%iX"			/* Skip X position			*/

/* ============================================================================
-- Subroutine to handle HP Laser Jet II output.  Utilizes as much as
-- intelligence as exists in the Laser Jet to reduce output size.
--
-- Usage:  logical = laserjet(key)
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
LOGICAL LaserJet(int key) {

#define NUM_ZERO		14						/* Number for compress breakeven */

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	static	int		BlankLines,Pixels;
	BITMAPQ	*PtrNext, *PtrNow, *Ptr, *StartPtr, *EndPtr;
	int		iband;
	int		iout,zerocnt;

	LOGICAL	First=FALSE,
				IsBlank;

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
				if (TransferInfo.Class == IBM4019) 
					IO_fwrite(SetHPPD, sizeof(SetHPPD), 1, io);
			}
			ReverseBits = TRUE;								/* Reverse bit ordering */
			Dirty       = FALSE;
			Pixels      = 300/TransferInfo.xperinch;	/* Dots/per/pixel	*/
			HPSkipOnThisLine[4] = (char)(Pixels+'0');	/* Encode into reverse lf */

			if (! NoReset) IO_fputs(HPReset, io);		/* Output reset */
			IO_fprintf(io, HPSetResolution, TransferInfo.xperinch);
			if (NoCompress) IO_fputs(HPRasterAtLeft,io); /* Enter graphics mode	*/
			break;
		
/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) {
				if (! NoCompress)
					IO_fputs(HPExitGraphics HPFormFeed HPRasterAtLeft, io);
				else
					IO_fputs(HPFormFeed,io);		/* FF output if was dirty */
			}
			Dirty = FALSE;									/* No longer dirty */
			BlankLines = 0;								/* No blank lines pending */
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			if (NoCompress) IO_fputs(HPExitGraphics,io);	/* Formally exit graphics */
			if (Dirty && (! NoFormFeed)) IO_fputs(HPFormFeed,io);
			if (! NoReset) IO_fputs(HPReset,io);			/* Optional reset */
			if (TransferInfo.Class == IBM4019)
				IO_fwrite(SetPPDS, sizeof(SetPPDS), 1, io);
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

				if (! Dirty && NoBlanks && IsBlank) continue;	/* Go to next band */
				Dirty = TRUE;												/* Page now dirty */

				if (NoCompress) {
					IO_fprintf(io, HPSendRaster, MapWidth*sizeof(BITMAPQ));
					IO_fwrite(PtrNow, MapWidth*sizeof(BITMAPQ), 1, io);
					continue;
				}
				
				First = TRUE;								/* First output on this line */
				if (IsBlank) { BlankLines++; continue; }

				if (BlankLines != 0) {				/* Output blanks as necessary */
					IO_fprintf(io, HPSkipYPixels, Pixels*BlankLines);
					BlankLines=0;
				}

				while (Ptr < PtrNext) {
					StartPtr = Ptr++;							/* Starting point */
					EndPtr   = NULL;
					zerocnt  = 0;								/* No blanks seen yet */
					while (Ptr < PtrNext) {					/* Should be non-zero */
						if (*Ptr == 0) {
							if (EndPtr == NULL) EndPtr = Ptr;
							zerocnt++;
						} else if (EndPtr != NULL) {
							if (zerocnt >= NUM_ZERO) break;
							EndPtr = NULL;
							zerocnt = 0;
						}
						Ptr++;
					}
					if (EndPtr == NULL) EndPtr = PtrNext;
					
					iout = BITS_PER_MAP*Pixels*((int) (StartPtr-PtrNow));
					if (First)											/* Skip horizontally */
						IO_fprintf(io, HPSkipOnNextLine, iout);
					else
						IO_fprintf(io, HPSkipOnThisLine, iout);
					iout = sizeof(BITMAPQ)*((int) (EndPtr-StartPtr));
					IO_fputs(HPRasterAtCursor, io);
					IO_fprintf(io, HPSendRaster, iout);
					IO_fwrite(StartPtr, iout, 1, io);
					IO_fputs(HPExitGraphics, io);
					First = FALSE;
				}
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
