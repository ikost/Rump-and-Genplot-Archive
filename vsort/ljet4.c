/* laserjet4.c */

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

#define DEFAULT_FILENAME	"ljet4.out"

typedef enum _COMPRESSION {NONE=0, RLL=1, TIFF=2, DELTA=3} COMPRESSION;

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
#define	HPReset					"\033%-12345X\033E"	/* Reset in HP mode		*/
#define	HPSetResolution		"\033*t%iR"				/* Set the resolution	*/
#define	HPSetPlanes				"\033*r%iU"				/* # of graphic planes	*/
#define	HPExitGraphics			"\033*rB"				/* Exit Graphics mode	*/
#define	HPEnterGraphics		"\033*r0A"				/* Enter graphics mode	*/
#define	HPSendCompressRaster	"\033*b%im%i%c"		/* Send compress + data	*/
#define	HPSendRasterData		"\033*b%i%c"			/* Send raster data		*/
#define	HPFormFeed				"\f"						/* Formfeed					*/

/* ============================================================================
-- Subroutine to handle HP Laser Jet IV output.  Utilizes as much as
-- intelligence as exists in the Laser Jet to reduce output size.
--
-- Usage:  logical = laserjet4(key)
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
LOGICAL LaserJet4(int key) {

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	BITMAPQ	*PtrNext, *PtrNow, *Ptr;
	int		iband;
	LOGICAL	IsBlank;

	COMPRESSION icmp;
	static COMPRESSION Compression = NONE;					/* Type compress used */
	static	unsigned char *LastMap,							/* Last row buffer */
								  *RLLBuffer,						/* RLL buffer		 */
								  *TIFFBuffer,						/* TIFF buffer		 */
								  *DeltaBuffer;					/* Delta buffer	 */
	unsigned char *ibuf;
	int		irll, itiff, idelta, iout;


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
			ColorMax    = 1;									/* Single color		    */
			NumPlanes   = 1;									/* Single graphics plane */

			Compression = NONE;								/* No compression		*/
			RLLBuffer   = NULL;								/* Empty buffers		*/
			TIFFBuffer  = NULL;
			DeltaBuffer = NULL;
			LastMap     = NULL;

			if (! NoCompress) {								/* Need buffers			*/
				RLLBuffer    = malloc(2*MapWidth);		/* RLL encoding			*/
				TIFFBuffer   = malloc(MapWidth+MapWidth/120);
				DeltaBuffer = malloc(MapWidth+MapWidth/8);
				LastMap     = malloc(MapWidth);			/* Last row of dots		*/
				memset(LastMap, 0, MapWidth);
			}

/* ---------------------------------------------------------------------------
--  Send a reset, set resolution set, and possible # of planes 
--         <esc>E            - reset
--         <esc>*t300R       - set to 300 PPI resolution
--         <esc>*r1U         - set for 1 pixel plane in CMY format
--         <esc>*rB          - exit graphics mode and reset compression
--         <esc>*r0A         - enter graphics and prepare for rows
--------------------------------------------------------------------------- */
			if (! NoReset) IO_fputs(HPReset, io);		/* Output reset			*/

			IO_fprintf(io, HPSetResolution, TransferInfo.xperinch);
			IO_fputs(HPExitGraphics,  io);				/* Clears all formats	*/
			IO_fputs(HPEnterGraphics, io);
			break;

/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) {
				IO_fputs(HPExitGraphics, io);
				IO_fputs(HPFormFeed, io);
				IO_fputs(HPEnterGraphics, io);
				Dirty = FALSE;									/* No longer dirty */
			}
			if (LastMap != NULL) memset(LastMap, 0, MapWidth);
			Compression = NONE;
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			IO_fputs(HPExitGraphics,io);				/* Formally exit graphics	*/
			if (Dirty && (! NoFormFeed)) IO_fputs(HPFormFeed,io);
			if (! NoReset) IO_fputs(HPReset,io);	/* Optional reset				*/
			IO_CloseChannel(io);	io = NULL;			/* Close the channel			*/
			if (LastMap     != NULL) free(LastMap);
			if (RLLBuffer   != NULL) free(RLLBuffer);
			if (TIFFBuffer  != NULL) free(TIFFBuffer);
			if (DeltaBuffer != NULL) free(DeltaBuffer);
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

				Dirty = TRUE;									/* Page is now dirty */
				if (NoCompress) {								/* Are we to compress?	*/
					IO_fprintf(io, HPSendRasterData, MapWidth, 'W');
					IO_fwrite(PtrNow, MapWidth, 1, io);
				} else {
					irll   = (int)  rll_encode(PtrNow, MapWidth, RLLBuffer);
					itiff  = (int) tiff_encode(PtrNow, MapWidth, TIFFBuffer, 0);
					idelta = (int) delta_encode(PtrNow, MapWidth, DeltaBuffer, LastMap);
					if (idelta < itiff && idelta < irll) {
						iout = idelta;
						icmp = DELTA;
						ibuf = DeltaBuffer;
					} else if (itiff < irll) {
						iout = itiff;
						icmp = TIFF;
						ibuf = TIFFBuffer;
					} else {
						iout = irll;
						icmp = RLL;
						ibuf = RLLBuffer;
					}
					if (Compression != icmp) {
						IO_fprintf(io, HPSendCompressRaster, icmp, iout, 'W');
						Compression = icmp;
					} else {
						IO_fprintf(io, HPSendRasterData, iout, 'W');
					}
					IO_fwrite(ibuf, iout, 1, io);
					if (LastMap != NULL) memcpy(LastMap, PtrNow, MapWidth);
				}
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
