/* VSORT.F77 */

/* ---------------------------------------------------------------------------
-- Modification history.
--
-- 4/23/95 - MOT
--      Memory leak tests revealed double free(vecbuf) both in DoScan() and
--      in code where allocated/DoScan called.  Removed from DoScan()
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

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

#define	CR						0x0D
#define	ENDPLT				32766			/* End of plot flag */
#define	ENDBLK				32765			/* End of block flag */

#define	MAX_INMEM			4				/* Max # of Records in memory	*/
#define	MAX_NUM_BLOCKS		10000			/* Max # of Records on disk	*/
#define	MAX_BAND_SIZE		24				/* Max # of scan lines			*/
#define	BLOCK_SIZE			2048u			/* Bytes/block on disk			*/

#define	VectorsPerRecord	(BLOCK_SIZE/sizeof(VECTOR))

typedef struct _vector {
	short	x ,y;									/* X,Y value			*/
	short	dx,dy;								/* Movement				*/
	short fl;									/* Color & linewidth */
} VECTOR;

typedef struct _DEVICELIST {
	int	Class;
	LOGICAL (*driver)(int key);
} DEVICELIST;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE	int		SortFile(CHAR *filename);
PRIVATE	LOGICAL	SortBlocks(int *NumChains);
PRIVATE	void		DoScan(void);
PRIVATE	LOGICAL	ScribeBand(int Band,int *NumActive,int *NumBlocks,int *NumDone);
PRIVATE	int		CompressBlock(void);
PRIVATE	void		CompressAndWrite(void);
PRIVATE	void		CompressAndRead(void);
PRIVATE	LOGICAL	rdblk(unsigned int block, VECTOR *array);
PRIVATE	void		wrblk(unsigned int block, VECTOR *array);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
TRANSFERINFO TransferInfo;				/* Information from driver				*/
LOGICAL		 ReverseBits;				/* Reverse bit order						*/
BITMAPQ		*BitMap;						/* Output bitmap (2-dimensional!)	*/
unsigned int BitMapSize;				/* Size of BitMap in bytes				*/
int			 MapWidth;					/* Width of band (in words)			*/
int			 ScansPerBand;				/* # of scans in each band output	*/
int			 NumPlanes;					/* Number of planes in output map	*/
int			 ColorMax;					/* Maximum color allowed by device	*/
int			 ColorMap[16];				/* Map of color --> bit pattern		*/

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static int		FileHndl;				/* Unit to read from */
static VECTOR  *outblk;					/* Array of VECTORS */
static VECTOR  *inblk[MAX_INMEM];	/* Array of pointers to VECTOR blocks */
static VECTOR  *vecbuf, *vecptr;		/* Vector pointers in DoScan */
static int	  *BlkLinkLst;				/* Link list of block # to next block # */
static int	  *ChainStart;				/* Chain starting points */
static LOGICAL (*dmdriv)(int key);	/* Is a routine call */
static LOGICAL	verbose=TRUE;			/* Are we verbose reporting? */

int		NumVectors;						/* Number of vectors in plot			*/
long		BlockOffset;					/* Block on dist where plot starts	*/
int		maxy;								/* Maximum Y value						*/

static int DefaultColorMap[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

/* ------------------------------------------------------------------------ */
DEVICELIST KnownDevices[] = {
							{LJET_II,			LaserJet},		/* II  uses PCL-4 only */
							{IBM4019,			LaserJet},		/* Nearly same if LJet */
							{LJET_III,			LaserJet4},		/* III uses PCL-5 also */
							{LJET_IV,			LaserJet4},		/* IV  uses PCL-5 */
/*							{DJET,				DeskJet}, */
							{DJET_PLUS,			DeskJet},
							{DJET_500,			DeskJet},
							{DJET_500C,			DeskJetC},
							{DJET_550C,			DeskJetC},
							{PAINTJET,			PaintJet},
							{QUIETJET,			QuietJet},
							{MX80,				Epson},
							{GEMINI,				Epson},
							{OKIDATA,			Okidata},
							{LQ500,				Pin24},
							{LQ800,				Pin24},
							{IBM_X24E,			Pin24},

							{DJET,				TiffOut},
							{TIFF_B,				TiffOut},
							{-1,					NULL} };

int main(int argc, char *argv[]) {

	char filename[PATH_MAX], *aptr;
	FILE *funit;
	int i;

/* For diagnostics, print some basic information as we start */
	fprintf(stderr, "vsort slave application started.  argc=%d\n", argc);
	for (i=0; i<argc; i++) fprintf(stderr, "  argv[%d] = %s\n", i, argv[i]);
	fflush(stderr);

/* Parse command line options */
	argc--; argv++;									/* Skip program name */
	if (argc && (strcmp(*argv, "-silent") == 0) ) { argc--; argv++; verbose = TRUE; }

/* For NT, parse command line parameters as a pipe name for communication */
#ifdef NT
	if (! argc) {											/* Pipe name? */
		fprintf(stderr, "WARNING: Will try <stdin>.  But NT seems to need named pipes.\n");
		funit = stdin;
	} else {
		HANDLE API_handle;
		int iunit;
		fprintf(stderr, "Connecting to named pipe: %s\n", *argv);
		if ( (API_handle = CreateFile(*argv, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "ERROR: Unable to open the pipe %s (error=%d)\n", *argv, GetLastError());
			sleep(2);
			return(3);
		} else if ( (iunit = _open_osfhandle((long) API_handle, _O_BINARY)) == -1) {
			fprintf(stderr, "ERROR: Unable to create C handle (error=%d)\n", GetLastError());
			sleep(2);
			return(3);
		} else if ( (funit = fdopen(iunit, "rb")) == NULL) {
			fprintf(stderr, "ERROR: Unable to convert C handle into FILE * pointer\n");
			sleep(2);
			return(3);
		}
	}
#else
	funit = stdin;
#endif
	
#ifdef OS2
/*	if (! isatty(fileno(stdin)))  freopen("CON", "r", stdin); */
	if (! isatty(fileno(stdout))) freopen("CON", "w", stdout);
/*	if (! isatty(fileno(stderr))) freopen("CON", "w", stderr); */
#endif
	
	while (fgets(filename, sizeof(filename), funit) != NULL) {
		if ( (aptr = strchr(filename, '\n')) != NULL) *aptr = '\0';
		if ( (aptr = strchr(filename, '\r')) != NULL) *aptr = '\0';
		if (*filename == '\0') continue;
		if (strcmp(filename, "QUIT") == 0) break;
		SortFile(filename);
	}

	if (verbose) printf("Exiting - Last received string: %s\n", filename);
	sleep(2);
	return(EXIT_SUCCESS);
}


/* ============================================================================
-- SortFile() takes a file containing vectors and print the result in dot
-- matrix form.  Should be modifiable for use with any raster oriented device.
--
-- Usage: int SortFile(char *filename)
--
-- Inputs: filename -- file containing unsorted vector list
--
-- Output:  return code  0 => everything okay
--                       8 => Version of header does not match
--                       9 => Unable to open input  file
--                      10 => Unable to read input  file
--                      11 => Unable to open output file
--                      12 => Unable to allocate temporary memory
--
--  LDEV:       Raster device type - see DMDRIV for list
--  IO_CHAN:    I/O channel as normally specified for a device
--  IO_FLAG:    bit wise flags concerning the output 
--               0 ==> Single dot, rather than 5 dots, on SYM 0 LT 0 draws
--               1 ==> Suppress final FF when closing
--               2 ==> Suppress reset sequences on open/close
--               3 ==> Suppress linewidth control (use single pixel)
--               4 ==> Suppress compression (if possible)
--               5 ==> Suppress blank lines at top of scan
--               6 ==> Generate plot at FRAME command each time
-- ----------------------------------------------------------------------------
-- ... File format:
-- ...   Record 1 - Marker information about device.  DPI, dev type, etc.
-- ...   Record 2 - Start of plot information.  There are VectorsPerRecord
-- ...              vectors per record.  The end of the plot is marked with an
-- ...              X start of ENDPLT.  The record containing this vector has
-- ...              the last vector of the RECORD also marked with ENDPLT to
-- ...              make finding it easier.
-- ----------------------------------------------------------------------------
--
============================================================================ */
PRIVATE int SortFile(CHAR *filename) {

	int		i;
	int		errcode;
	int		NumChains, NumBlocks;
	BITMAPQ	qb,qb1;
	DEVICELIST *DeviceList = KnownDevices;

	FileHndl		= -1;										/* No file opened yet	*/
	outblk		= NULL;									/* No memory allocated	*/
	BlkLinkLst	= NULL;									/* No memory allocated	*/
	ChainStart	= NULL;									/* No memory allocated	*/
	for (i=0; i<MAX_INMEM; i++) inblk[i] = NULL;	/* No memory allocated	*/

/* First, test something simple */
	qb  = (BITMAPQ) (0x01 << (BITS_PER_MAP-1));	/* Hope this is right	*/
	qb1 = (BITMAPQ) (qb >> 1);
	if (qb & qb1) {
		gen_err("ERROR: Hardware fails to handle shift correctly");
		errcode = 8; goto AllReturn;
	}

/* ---------------------------------------------------------------------------
-- Open file and read first info - DeviceName,SubDevice,IO_Flag,IO_Chan
---------------------------------------------------------------------------- */
	if ( (FileHndl = open(filename, O_RDWR|O_BINARY)) == -1) {
		gen_err2("File does not exist or failed to open", filename);
		errcode = 9; goto AllReturn;
	}

	if (read (FileHndl, (char *) &TransferInfo, sizeof(TransferInfo)) != sizeof (TransferInfo)) {
		gen_err("So sorry Charlie - Unable to read vector file");
		errcode = 10; goto AllReturn;
	}
	if (verbose)
		printf("Driver reports: \n"
				 "       Version: %4.4x\n"
				 "         Class: %i\n"
				 "       IO_Chan: %s\n"
				 "     SubDevice: %i\n"
				 "       IO_Flag: %i\n"
				 "   x,y perinch: %i,%i\n"
				 "       x,y max: %i,%i\n"
				 "       numpens: %i\n",
				 	 TransferInfo.Version,
					 TransferInfo.Class,		 TransferInfo.IO_Chan,
					 TransferInfo.SubDevice, TransferInfo.IO_Flag, TransferInfo.xperinch, TransferInfo.yperinch,
					 TransferInfo.xmax, TransferInfo.ymax,
					 TransferInfo.numpens);

	if (TransferInfo.Version != TRANSFERINFO_VERSION) {
		ERRprintf("ERROR: Version of vector file does not match this program.\n");
		errcode = 8; goto AllReturn;
	}

	while (DeviceList->Class != -1) {
		if (TransferInfo.Class == DeviceList->Class) break;
		DeviceList++;
	}
	if (DeviceList->Class == -1 || (dmdriv=DeviceList->driver) == NULL) {
		ERRprintf("ERROR: No driver exists for device class %d\n", TransferInfo.Class);
		errcode = 11; goto AllReturn;
	}
	BlockOffset = 1;									/* Starting block of vectors	*/

/* ---------------------------------------------------------------------------
 ... Set up initial defaults.  IO_FLAG and IO_CHAN have been set before call.
---------------------------------------------------------------------------- */
	maxy			 = TransferInfo.ymax;					/* Pixels in Y */
	ScansPerBand = MAX_BAND_SIZE;							/* # scan at a time	*/
	ReverseBits	 = FALSE;									/* Normal bit order	*/
	NumPlanes	 = 1;											/* Assume one color plane		*/
	ColorMax		 = 1;											/* Assume one color allowed	*/

/* ---------------------------------------------------------------------------
 ... Output driver routine DMDRIV may modify MAXY, MapWidth, ScansPerBand and
     REVERSE_BITS as necessary.  DMDRIV must open an  output file, modify
     the parameters if necessary and return status.

     1. Give them the current guess of MapWidth, BitMapSize for information
     2. Modify the values to actual values after potential changes
---------------------------------------------------------------------------- */
	MapWidth	  = (maxy+BITS_PER_MAP-1)/BITS_PER_MAP;	/* Words in one scan */
	BitMapSize = sizeof(*BitMap)*MapWidth*NumPlanes*ScansPerBand;
	memcpy(ColorMap, DefaultColorMap, sizeof(DefaultColorMap));
	if (! (*dmdriv)(0)) {									/* Turn driver on */
		gen_err("I/O channel initialize failed on RASTER output");
		errcode = 11; goto AllReturn;
	}
	MapWidth	  = (maxy+BITS_PER_MAP-1)/BITS_PER_MAP;	/* Words in one scan */
	BitMapSize = sizeof(*BitMap)*MapWidth*NumPlanes*ScansPerBand;

/* ----------------------------------------------------------------------------
 ... Start here on a new plot frame - new page, new merging
---------------------------------------------------------------------------- */
	while (TRUE) {
		
/* ---------------------------------------------------------
-- ... Allocate memory for working blocks (lots of them!) --
------------------------------------------------------------ */
		if ( (outblk = (VECTOR *) malloc(BLOCK_SIZE)) == NULL) goto AllocErr;

/* ---------------------------------------------------------------------------
 ... Initially, we have some number of chains, each of which is only 1 long.
 ... We simply mark that fact and let the merge happily go it's way
---------------------------------------------------------------------------- */
		NumChains = 0;
		while (TRUE) {
			if (! rdblk(NumChains, outblk)) {							/* Can I read */
				if (NumChains == 0) goto ProbableEnd;
				goto ReadError;
			}
			if (outblk[VectorsPerRecord-1].x == ENDPLT) break;		/* Found end? */
			if (NumChains++ == MAX_NUM_BLOCKS) {
				gen_err("RIDICULOUS # OF VECTORS - PLOT REFUSED!");
				errcode = 99; goto AllReturn;
			}
		}

		NumBlocks  = NumChains+1;									/* Number of blocks */
		NumVectors = outblk[VectorsPerRecord-1].y;

		if ((BlkLinkLst = (int *) malloc(NumBlocks*sizeof(*BlkLinkLst))) == NULL) goto AllocErr;
		if ((ChainStart = (int *) malloc(NumBlocks*sizeof(*ChainStart))) == NULL) goto AllocErr;
		for (i=0; i<MAX_INMEM; i++) {
			if ((inblk[i] = (VECTOR *) malloc(BLOCK_SIZE))==NULL) goto AllocErr;
		}

		for (i=0; i<=NumChains; i++) {
			BlkLinkLst[i]  = -1;								/* Next element is not */
			ChainStart[i]  = i;								/* Chain starts here */
		}

/*----------------------------------------------------------------------
 ... If # chains is >1, load in first block of each and perform merge
---------------------------------------------------------------------- */
		while (NumChains) {										/* 0 ==> single chain */
			if (verbose) {
				char token[80];
				sprintf(token,"Merging %3i chains ...", NumChains); 
				CONputs(token);
			}
			if (! SortBlocks(&NumChains)) goto ReadError;	/* Sort the blocks	  */
			if (verbose) CONputc(CR);
		}

		free(outblk); outblk = NULL;				/* Free this memory */
		for (i=0; i<MAX_INMEM; i++) { free(inblk[i]); inblk[i] = NULL; }

/* -------------------------------------------------------------------
 ... Okay, single chain on disk now.  Time to just output it!
------------------------------------------------------------------- */
		(*dmdriv)(1);									/* Frame the driver */

		if ((BitMap = (BITMAPQ *) malloc(BitMapSize))   == NULL) goto AllocErr;
		if ((vecbuf = (VECTOR  *) malloc(2*BLOCK_SIZE+sizeof(vecbuf->x))) == NULL) goto AllocErr;
		DoScan();
		free(BitMap); BitMap = NULL;
		free(vecbuf); vecbuf = NULL;

		free(BlkLinkLst);	BlkLinkLst = NULL;
		free(ChainStart);	ChainStart = NULL;
		putchar('\n');
		BlockOffset += NumBlocks;					/* Update BlockOffset for next */
	}

/* -------------------------------------------------------------------------
 ... EOF or error on input file - Can't tell which occurred at this time.
 ... Just abort out irrelevant of reason for coming here.  No message
------------------------------------------------------------------------- */
ReadError:
	gen_err("Unexpected error reading the file");
ProbableEnd:
	(*dmdriv)(3);								/* Close output w/ end of plot */
	errcode = 0; goto AllReturn;


AllocErr:	
	gen_err("Unable to allocate memory for vector blocks (RASTER)");
	errcode = 97; goto AllReturn;

/* -- All returns go through here to clean up memory allocation etc. -- */
AllReturn:
	if (FileHndl != -1) close(FileHndl);			/* Close the file			*/
	sleep(1);												/* Let close() succeed	*/
																/* OS/2 problem!			*/
	if (BlkLinkLst != NULL) free(BlkLinkLst);		/* Free Memory				*/
	if (ChainStart != NULL) free(ChainStart);
	if (outblk     != NULL) free(outblk);
	for (i=0; i<MAX_INMEM; i++) 
		if (inblk[i] != NULL) free(inblk[i]);

	if (errcode == 0 && unlink(filename) != 0)
		printf("ERROR: Unlink of %s failed (errno = %i)\n", filename, errno);

	fflush(stdout);
	return(errcode);
}


/* ============================================================================
-- LOGICAL SortBlocks(int *NumChains)
--
-- We do all the work here to sort once through a set of chains.  The result
-- of the sort goes back to disk.  The pointers will be all set up so that the
-- result itself is one long chain.
--
-- On entry to this subroutine, the following must be preset in /RASTR/:
--  A           -  Filled with the first records in each chain
--  CURRENT_REC -  DECKPT pointers to which records are loaded in A
--
-- We build up the sorted string of vectors in the common block array VB1
-- which is written to disk.
============================================================================ */
PRIVATE LOGICAL SortBlocks(int *NumChains) {

	VECTOR  *outptr;								/* Pointer for output list			*/
	VECTOR  *inptr[MAX_INMEM];					/* Pointer to input lists			*/
	int count[MAX_INMEM];						/* Count of vectors used in each */
	int NextBlkInChain[MAX_INMEM];
	int EmptyBlk[MAX_INMEM+1];					/* List of empty blocks on disk */
														/* Need 1 extra because of read/write order */
	int *EmptyBlkPtr=EmptyBlk;					/* Pointer to where can put EmptyList */

	int		LastBlkWritten=-1,				/* Most recently written record	*/
				NumBlksValid=0;					/* Number valid blocks in memory	*/
	int		block;
	int		lowvalue,lowindex;
	int		OutCnt,i;							/* Counters */
	
/* -- Load in first block from up to MAX_INMEM chains -- */
	for (i=0; i<MAX_INMEM; i++) {					/* Fill INMEM internal blocks	*/
		block = ChainStart[*NumChains];			/* Get start of last chain		*/
		NextBlkInChain[i] = BlkLinkLst[block];	/* Next block in this Chain	*/
		if (! rdblk(block,inblk[i]) ) return(FALSE);
		inptr[i] = inblk[i];							/* In-chain pointer = filled	*/
		count[i] = 0;									/* No vectors read from him	*/
		*(EmptyBlkPtr++)  = block;					/* Record now empty				*/
		NumBlksValid++;								/* Number of blocks valid up	*/
		if (--(*NumChains) < 0) break;			/* One fewer -- any left?		*/
	}
		
	(*NumChains)++;									/* We will be a new chain		*/
	for (i=*NumChains; i; i--)						/* Open as 0 so last used		*/
		ChainStart[i] = ChainStart[i-1];			/* In subsequent sorts			*/

/* ----------------------------------------------------------------------------
 ... We loop through the following code as long as necessary to walk through
 ... all the chains we have.  Generate VectorsPerRecord sorted vectors and
 ... then output them.
---------------------------------------------------------------------------- */
	while (TRUE) {

		outptr = outblk;									/* Reset output pointer			*/
		for (OutCnt=0; OutCnt<VectorsPerRecord; OutCnt++) {
			lowindex = 0;									/* Find chain with the lowest	*/
			lowvalue = inptr[0]->x;						/* starting X value -- this	*/
			for (i=1; i<NumBlksValid; i++) {			/* vector gets inserted into	*/
				if (inptr[i]->x < lowvalue) {			/* multiply sorted list next	*/
					lowvalue = inptr[i]->x;				/* Keep value for interest		*/
					lowindex = i;							/* And index of its chain		*/
				}
			}

         if (lowvalue == ENDPLT) {					/* END of all chains?			*/
				if (OutCnt) {								/* Any left to save to disk?	*/
					outptr->x = ENDPLT;
					block = *(--EmptyBlkPtr);			/* Get an empty block			*/
					wrblk(block, outblk);				/* Send it onward!				*/
					if (LastBlkWritten >= 0)			/* Install in chain or start	*/
						BlkLinkLst[LastBlkWritten] = block;	
					else
						ChainStart[0] = block;			
					BlkLinkLst[block] = -1;				/* Mark this as end of chain	*/
				} else if (LastBlkWritten >= 0) {	
					BlkLinkLst[LastBlkWritten] = -1;	/* Mark this as end of chain	*/
				}
				return(TRUE);
			}

			*(outptr++) = *(inptr[lowindex]++);		/* Insert vector/increment	*/

			if (++count[lowindex] == VectorsPerRecord) {	/* More in record? */
				block = NextBlkInChain[lowindex];		/* Next to read in */
				inptr[lowindex] = inblk[lowindex];		/* Incoming list			*/
				count[lowindex] = 0;							/* Haven't used any yet */
				if (block == -1) {							/* Is this end of chain	*/
					inptr[lowindex]->x = ENDPLT;			/* Flag as plot end		*/
				} else {											
					rdblk(block, inblk[lowindex]);		/* Read next and mark	*/
					*(EmptyBlkPtr++) = block;				/* block as free			*/
					NextBlkInChain[lowindex] = BlkLinkLst[block];
				}
			}
		}

/* ... Write output buffer to disk */
		block = *(--EmptyBlkPtr);							/* Get a free block */
		wrblk(block,outblk);
		if (LastBlkWritten >= 0) 
			BlkLinkLst[LastBlkWritten] = block;			/* Install in chain */
		else
			ChainStart[0] = block;							/* Start of chain here	*/
		LastBlkWritten   = block;							/* Keep for information	*/
	}																/* Loop for ever			*/
	panic; return(TRUE);							/* BETTER NOT HAPPEN */
}


/* ============================================================================
--     subroutine DoScan
--
-- Inputs:   blocks are linked in a chain as specified by dchain().
--           ChainStart[0] - Starting block of chain
--           BlkLinkLst[i] - Link list from block to block
--        
-- The vectors are processed as segments, calculating all intersections over
-- all possible Y values within the given X coordinate.  Any vector which which
-- lies outside region of interest, or terminates, is marked with a -1.
--
-- When we are done with a row (actually a set of rows, MAX_BAND_SIZE in 
-- number), pass control to the device driver DMDRIV, which will convert the
-- dot image (stored in array MAP) to the printer codes.
============================================================================ */
LOGICAL	InMemory;									/* Are remaining vectors in memory	*/
int		NextRead, FirstRead, LastWrite;		/* Next block which can be read		*/

/* Band: xxx    Active: xxxxx (xxxxx)    Done: xxxxxx (xxxxxx)    Links: xx (xx) */
char format[] = 
	"\rBand: %3.3i    Active: %5i (%5i)    Done: %6i (%6i)    Links: %2i (%2i)";

PRIVATE void DoScan(void) {

	int	Band=0,					/* Which band is being drawn		*/
			NumActive,				/* Number of vectors active		*/
			MaxActive=0,			/* Maximum # of active vectors	*/
			NumBlocks=0,			/* Number of blocks current used */
			MaxBlocks=0,			/* Maximum # of blocks used		*/
			NumDone=0;				/* How many vectors are done		*/

	NextRead  = ChainStart[0];			/* Next block which we should read	*/
	vecbuf->x = ENDBLK;

	while (ScribeBand(Band, &NumActive, &NumBlocks, &NumDone)) {
		(*dmdriv)(2);												/* Output the result */
		if (verbose) {												/* User information?	*/
			char token[80];
			MaxActive = max(MaxActive, NumActive);			/* Follow maximum */
			MaxBlocks = max(MaxBlocks, NumBlocks);			/* Follow maximum */
			sprintf(token, format, 
				Band, NumActive,MaxActive, NumDone,NumVectors, NumBlocks,MaxBlocks);
			CONputs(token);
			fflush(NULL);
		}
		Band++;
	}
	
	return;
}

/* ============================================================================
--     logical function ScribeBand
--
-- We have a job to do which is easier said than done.  Given a list of active
-- vectors, and the limits of a band, compute the dot image of those vectors
-- within the band.  While at it, delete vectors which become inactive.
============================================================================ */
PRIVATE LOGICAL ScribeBand(int Band,int *NumActive,int *NumBlocks,int *NumDone) {

	int		j,k;
	REAL		x,yl,yh,
				dp,									/* Maximum excursion of Y	*/
				m,										/* Slope of the vector		*/
				r2,									/* Radius of end (lw/2**2)	*/
				c2;									/* Random constants			*/
	LOGICAL	lweven;
	int		ix,lw,icol,
				ixl,ixh,								/* Lower and upper allowed X */
				xs,ys,								/* Real starting point			*/
				dx,dy,								/* Displacement of vector		*/
				FirstX,								/* Current origin of Bit map	*/
				iyl,iyh;

	BITMAPQ	qb;									/* Bit position into BitMap	*/
	BITMAPQ	bitplane;
	BITMAPQ *BitPtr, *BitPtr2;

	memset(BitMap, 0, BitMapSize);			/* Clear the bit map */
	FirstX = Band*ScansPerBand;				/* First X value in this band		*/
	*NumActive = 0;								/* None active this band yet		*/
	*NumBlocks = 0;								/* Haven't used any blocks yet	*/
	vecptr = vecbuf;								/* Starting position					*/
	InMemory = TRUE;								/* And assume we have in memory	*/

	while (TRUE) {

		if (vecptr->x == ENDBLK) CompressAndRead();	/* Fill as needed */
		xs = vecptr->x;										/* X coordinate */
		if ((xs==ENDPLT)||(xs>FirstX+ScansPerBand)) {	/* Finished with band? */
			if (! InMemory) {
				CompressAndWrite();							/* Compress & write rest */
				BlkLinkLst[LastWrite] = NextRead;
				NextRead = FirstRead;
			}
			return( (xs != ENDPLT) || (*NumActive != 0) );

		} else if (xs != -1) {
			ys   = vecptr->y;								/* Y starting point */
			dx   = vecptr->dx;							/* X movement */
         dy   = vecptr->dy;							/* Y movement */
			lw   = vecptr->fl&0xFF;						/* Low 8 bits */
			icol = vecptr->fl>>8;						/* Requested color */
			icol = ColorMap[min(ColorMax,icol)];	/* And limit to valid # */

         if (xs+dx+lw < FirstX) {					/* Does vector lie in band? */
            vecptr->x = -1;							/* Mark as no longer used */
				(*NumDone)++;
				continue;
			}

			(*NumActive)++;								/* Increment # active */
			xs  = xs + lw/2;								/* Actual starting point */
			lweven =  ( (lw & 0x01) == 0);			/* Is lw even? */

         ixl = max(-lw/2, FirstX-xs);				/* Lower level to look */
			ixh = dx+lw/2;									/* Upper range */
         if (lweven) ixh--;							/* Look one less each time */

			if (ixh+xs <= FirstX+ScansPerBand-1) {	/* Does vector end in this band */
				vecptr->x = -1;							/* Flag as deleted */
				(*NumDone)++;								/* And keep track of done */
			} else											/* No, keep for next time */
            ixh = (FirstX+ScansPerBand-1)-xs;

         r2  = lw*lw/4.0f;								/* Allowed radius squared */
         m   = dy/(dx+.00001f);						/* slope of the line */
         c2  = dx*(1.0f+m*m);							/* Ending range */
         dp  = (REAL) ((lw/2.0f)*sqrt(1.0f+m*m)); /* Perpendicular size */

         x = ixl - 1.0f;								/* X starts back one */
         if (lweven) x += 0.5f;						/* Move to center point */

			for (ix=ixl; ix<=ixh; ix++) {				/* Loop through points */
				x += 1.0f;									/* Next X point */
				yl = m*x - dp;								/* Lower Y value */
            yh = m*x + dp;								/* Upper Y value */
            if (m*yl+x < 0.0f)						/* Before 1st point? */
               yl = (REAL) (-sqrt(r2-x*x));
            else if (m*yl+x > c2)					/* After 2nd point? */
               yl = (REAL) (dy - sqrt(r2-(x-dx)*(x-dx)));
            if (m*yh+x > c2)							/* After 2nd point? */
               yh = (REAL) (dy + sqrt(r2-(x-dx)*(x-dx)));
            else if (m*yh+x < 0.0f)					/* Before 1st point? */
               yh = (REAL) (+sqrt(r2-x*x));
				iyl = max(0,(int)(yl+0.1f+ys+0.99f));	/* Real start point */
            iyh = min(maxy,(int) (yh+0.1f+ys));		/* Ending position */

				BitPtr = BitMap +										/* Start of BitMap */
							(ix+xs-FirstX)*NumPlanes*MapWidth +	/* Start of row */
									+iyl/BITS_PER_MAP;				/* Specific word */
				if (ReverseBits) {									/* Reverse ordering */
					qb = (BITMAPQ) (0x01 << ((BITS_PER_MAP-1)- iyl%BITS_PER_MAP)); 
				} else {													/* Normal ordering */
					qb = (BITMAPQ) (0x01 << (iyl % BITS_PER_MAP));
				}

				for (j=iyl; j<=iyh; j++) {
					bitplane = 0x01;						/* First bit plane */
					BitPtr2  = BitPtr;
					for (k=0; k<NumPlanes; k++) {		/* Loop through color planes */
						if (icol & bitplane) *BitPtr2 |= qb;
						bitplane <<= 1;					/* Shift over to next */
						BitPtr2  += MapWidth;			/* To next plane */
					}
					if (ReverseBits) {					/* Next pixel position */
						qb >>= 1;
						if (qb == 0) {
							qb = 0x01 << (BITS_PER_MAP-1);
							BitPtr++;
						}
					} else {
						qb <<= 1;
						if (qb == 0) {
							qb = 0x01;
							BitPtr++;
						}
					}
				}												/* End of bit set loop */
			}													/* End of x range of vector */
		}
		vecptr++;											/* Next vector! */
	}															/* Do forever! */
	panic; return(FALSE);								/* BETTER NOT HAPPEN! */
}

/* --------------------------------------------------------------------------
-- Routine to find a block that has been erased
--
-- Because I should not never need more blocks than I start with, I should
-- be safe searching for the -2 marker of an empty block.
---------------------------------------------------------------------------- */
PRIVATE int FindFree(void) {
	int i=0;
	while (BlkLinkLst[i] != -2) i++;
	return(i);
}
		

/* --------------------------------------------------------------------------
-- Routine to compress block and read next one into space
---------------------------------------------------------------------------- */
PRIVATE void CompressAndRead(void) {

	int i,nvecs;

	nvecs = CompressBlock();

	if (nvecs >= VectorsPerRecord) {
		i = FindFree();									/* Find a disk block		*/
		wrblk(i, vecbuf);									/* Write out this block */
		if (InMemory) {									/* Is this first save?	*/
			FirstRead = i;									/* Keep track of start	*/
		} else {
			BlkLinkLst[LastWrite] = i;					/* Start maintaining link */
		}
		LastWrite     = i;								/* And track last block	*/
		BlkLinkLst[i] = -1;								/* No longer empty!		*/
		InMemory = FALSE;
		vecptr -= VectorsPerRecord;					/* Keep at ENDBLK still */
		nvecs  -= VectorsPerRecord;					/* Leaving fewer in space */
		memmove(vecbuf, &vecbuf[VectorsPerRecord], sizeof(VECTOR)*(nvecs+1));
	}

	while (vecptr->x == ENDBLK) {
		if (NextRead == -1) {
			vecptr->x = ENDPLT;
		} else {
			rdblk(NextRead, vecptr);
			i = NextRead;
			NextRead = BlkLinkLst[NextRead];			/* Next to read */
			BlkLinkLst[i] = -2;							/* This one unused now */
			vecptr[VectorsPerRecord].x = ENDBLK;
		}
	}
	return;
}

/* --------------------------------------------------------------------------
-- Routine to compress block and read next one into space
---------------------------------------------------------------------------- */
PRIVATE void CompressAndWrite(void) {

	int i,nvecs;

	nvecs = CompressBlock();
	while (nvecs > 0) {
		i = FindFree();
		wrblk(i, vecbuf);
		nvecs -= VectorsPerRecord;					/* Leaving fewer in space */
		if (nvecs >= 0) memmove(vecbuf, &vecbuf[VectorsPerRecord], sizeof(VECTOR)*(nvecs+1));
		BlkLinkLst[LastWrite] = i;					/* Put me in chain from last	*/
		LastWrite = i;									/* And make me the last now	*/
		BlkLinkLst[i] = -1;							/* But no longer unused			*/
	}
	vecbuf->x = ENDBLK;									/* Nothing in buffer now */
	return;
}

/* --------------------------------------------------------------------------
-- Routine to compress block into smallest available space
--
-- vecptr is left pointing at the ENDBLK at end of the list
---------------------------------------------------------------------------- */
PRIVATE int CompressBlock(void) {

	VECTOR *rdptr;
	int nvecs=0;

	vecptr = rdptr = vecbuf;							/* Initialize */
	while (TRUE) {
		if (rdptr->x == ENDBLK) break;
		if (rdptr->x == ENDPLT) break;
		if (rdptr->x != -1) {							/* Is this one valid? */
			*(vecptr++) = *rdptr;						/* Move across			 */
			nvecs++;
		}
		rdptr++;
	}
	vecptr->x = ENDBLK;									/* And mark last one as EOB */
	return(nvecs);
}


/* ============================================================================
-- Function to write a vector block to the file
--
-- Usage:  call wrblk(block, array)
--
-- Inputs: block - block to read (relative to start point BlockOffset)
--
-- Output: none
============================================================================ */
PRIVATE void wrblk(unsigned int block, VECTOR *array) {
	
	lseek(FileHndl, (BlockOffset+block) * BLOCK_SIZE, SEEK_SET);
	write(FileHndl, (char *) array, BLOCK_SIZE);
	return;

}

/* ============================================================================
-- Function to read a vector block from the file
--
-- Usage:  LOGICAL = rdblk(block, array)
--
-- Inputs: block - block to read (relative to start point BlockOffset)
--
-- Output: Fills in array
--         rdblk - .TRUE.  ==> successful read
--               - .FALSE. ==> unable to read the requested block 
============================================================================ */
PRIVATE LOGICAL rdblk(unsigned int block, VECTOR *array) {

	if (lseek(FileHndl, (BlockOffset+block) * BLOCK_SIZE, SEEK_SET) == -1)
		return(FALSE);
	if ( read(FileHndl, (char *) array, BLOCK_SIZE) != BLOCK_SIZE)
		return(FALSE);
	return(TRUE);
}
