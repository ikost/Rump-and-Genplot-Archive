/* deskjetc.c */

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

#define DEFAULT_FILENAME	"deskjetc.out"

typedef enum _COMPRESSION {NONE=0, RLL=1, TIFF=2, DELTA=3} COMPRESSION;

#define	HPReset					"\033E"					/* Reset in HP mode		*/
#define	HPInitialize			"\033&l0O\033&l0L"	/* Portrait/No perf		*/
#define	HPSetPrintMode			"\033*p%iN"				/* Print mode				*/
#define	HPSetShingling			"\033*o%iQ"				/* 0,1,2 shingling		*/
#define	HPSetQuality			"\033*r%iQ"				/* 0,1,2 dflt/draft/high */
#define	HPSetResolution		"\033*t%iR"				/* Set the resolution	*/
#define	HPSetPlanes				"\033*r%iU"				/* # of graphic planes	*/
#define	HPExitGraphics			"\033*rB"				/* Exit Graphics mode	*/
#define	HPEnterGraphics		"\033*r0A"				/* Enter graphics mode	*/
#define	HPSendCompressRaster	"\033*b%im%i%c"		/* Send compress + data	*/
#define	HPSendRasterData		"\033*b%i%c"			/* Send raster data		*/
#define	HPFormFeed				"\f"						/* Formfeed					*/

#define	MAXPLANES		4									/* Max # of graph planes */

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
/* Colors should be White, Black, Red, Green, Blue, Magenta, Cyan, Yellow */
static int  CMY_ColorMap[8] = {0,7,6,5,3,2,1,4};
static int KCMY_ColorMap[8] = {0,1,12,10,6,4,2,8};

/* ============================================================================
-- Subroutine to handle HP DeskJet output.  Utilizes as much as
-- intelligence as exists in the Laser Jet to reduce output size.
--
-- Usage:  logical = deskjetc(key)
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
LOGICAL DeskJetC(int key) {

	static	IO_BLOCK	*io=NULL;
	static	LOGICAL	Dirty;
	BITMAPQ	*PtrNext, *PtrNow, *Ptr;
	int		iband;
	LOGICAL	IsBlank;
	int		i, achr;

	COMPRESSION icmp;
	static COMPRESSION Compression = NONE;					/* Type compress used */
	static	unsigned char *LastMap[MAXPLANES],			/* Last row buffer */
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
			ColorMax    = TransferInfo.numpens;
			ColorMax    = max(1,min(7,ColorMax));		/* # valid pens		*/
			if      (ColorMax == 1)	NumPlanes = 1;		/* Number of bands	*/
			else							NumPlanes = 3;
			if (NumPlanes == 3 && TransferInfo.Class == DJET_550C) {
				NumPlanes = 4;									/* Go to KCMY format	*/
				memcpy(ColorMap, KCMY_ColorMap, sizeof(KCMY_ColorMap));
			} else {
				memcpy(ColorMap, CMY_ColorMap, sizeof(CMY_ColorMap));
			}

			Compression = NONE;								/* No compression		*/
			RLLBuffer   = NULL;								/* Empty buffers		*/
			TIFFBuffer  = NULL;
			DeltaBuffer = NULL;
			for (i=0; i<MAXPLANES; i++) LastMap[i] = NULL;

			if (! NoCompress) {								/* Need buffers			*/
				RLLBuffer    = malloc(2*MapWidth);		/* RLL encoding			*/
				TIFFBuffer   = malloc(MapWidth+MapWidth/120);
				DeltaBuffer  = malloc(MapWidth+MapWidth/8);
				for (i=0; i<NumPlanes; i++) {
					LastMap[i] = malloc(MapWidth);		/* Last row of dots		*/
					memset(LastMap[i], 0, MapWidth);
				}
			}

/* ---------------------------------------------------------------------------
--  Send a reset, set resolution set, and possible # of planes 
--         <esc>E            - reset
--         <esc>*t300R       - set to 300 PPI resolution
--         <esc>*r1U         - set for 1 pixel plane in CMY format
--         <esc>*rB          - exit graphics mode and reset compression
--         <esc>*r0A         - enter graphics and prepare for rows
--------------------------------------------------------------------------- */
			if (! NoReset) {
				IO_fputs(HPReset, io);						/* Output reset			*/
				IO_fputs(HPInitialize, io);				/* Portrait/No perf		*/
			}

			IO_fputs(HPExitGraphics, io);					/* Clears all formats	*/
			IO_fprintf(io, HPSetPlanes, -NumPlanes);	/* Number of CMY planes	*/
			IO_fprintf(io, HPSetResolution, TransferInfo.xperinch);

			if (! NoReset) {
				IO_fprintf(io, HPSetPrintMode, 4);		/* Smart Bi-directional */
				IO_fprintf(io, HPSetQuality, 0);			/* Keypad setting			*/
				IO_fprintf(io, HPSetShingling, 0);		/* no shingling			*/
			}

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
			for (i=0; i<MAXPLANES; i++) {
				if (LastMap[i] != NULL) memset(LastMap[i], 0, MapWidth);
			}
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
			for (i=0; i<MAXPLANES; i++) {
				if (LastMap[i] != NULL) free(LastMap[i]);
			}
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
			for (iband=0; iband<(ScansPerBand*NumPlanes); iband+=NumPlanes) {

				Ptr = PtrNow = PtrNext;					/* This scan position */
				PtrNext += MapWidth*NumPlanes;		/* And the next one */

/* -- Find first non-zero element (or determine line to be empty) */
				while (Ptr != PtrNext && *Ptr == 0) Ptr++;
				IsBlank = (Ptr == PtrNext);

				if (!Dirty && NoBlanks && IsBlank) continue;	/* Go to next band */

				Dirty = TRUE;									/* Page is now dirty */
				for (i=0; i<NumPlanes; i++,PtrNow+=MapWidth) {
					if (NoCompress) {							/* No compression easy	*/
						iout = MapWidth;						/* Use full width			*/
						ibuf = PtrNow;							/* And real data			*/
						icmp = NONE;							/* And no compression	*/
					} else {
						irll   = (int)  rll_encode(PtrNow, MapWidth, RLLBuffer);
						itiff  = (int) tiff_encode(PtrNow, MapWidth, TIFFBuffer, 0);
						idelta = (int) delta_encode(PtrNow, MapWidth, DeltaBuffer, LastMap[i]);
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
					}

					achr = (i==(NumPlanes-1)) ? 'W' : 'V';
					if (Compression != icmp) {
						IO_fprintf(io, HPSendCompressRaster, icmp, iout, achr);
						Compression = icmp;
					} else {
						IO_fprintf(io, HPSendRasterData, iout, achr);
					}
					IO_fwrite(ibuf, iout, 1, io);
					if (LastMap[i] != NULL) memcpy(LastMap[i], PtrNow, MapWidth);
				}
			}
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}
