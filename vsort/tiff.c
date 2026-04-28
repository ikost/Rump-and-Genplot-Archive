/* tiff.c */

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
#include <time.h>
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
#include "tiff.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define DEFAULT_FILENAME	"plot.tif"

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


/* ============================================================================
-- Subroutine to handle output in the Tiff structure.  Will utilize the
-- packbits compression if enabled by the user.
--
-- Usage:  LOGICAL TiffOut(int key);
--
-- Inputs: key  - operation requested
--                = 0 - Device initialize
--                = 1 - Page initialize
--                = 2 - Output one band
--                = 3 - Close and exit
--
-- Output: Various.  Opens file, writes bytes, and transcribes data.
--
-- Returns: Success of the operation.  TRUE --> successful.
============================================================================ */
LOGICAL TiffOut(int key) {

	static IO_BLOCK *io=NULL;					/* io block structure				*/
	static LOGICAL	 Dirty,						/* Has page been written to?		*/
						 Initialized;				/* Has TIFF file been init'd		*/

	static TIFF    *tf=NULL;					/* Open TIFF file unit				*/
	static int		StripMaxCount=0,			/* malloc() size of Bytes/Offset	*/
						StripCount=0;				/* Current # of entries				*/
	static USHORT	*StripBytes=NULL;			/* Count of bytes in each strip	*/
	static ULONG	*StripOffset=NULL;		/* Offset of each strip in file	*/
	static unsigned char *buf=NULL;			/* Work buffer for transcription	*/

	int iband, icnt;								/* Random counters					*/

	switch (key) {
		
/* -----------------------------------------
-- Initialize command
----------------------------------------- */
		case 0:
			if (io == NULL) {						/* Open the I/O channel as spec'd */
				io = IO_OpenChannel("VSORT", TransferInfo.IO_Chan, IOC_BINARY, IOF_NOFLOW);
				if (io == NULL) {
					io = IO_OpenChannel("VSORT", "file*" DEFAULT_FILENAME, IOC_BINARY, IOF_NOFLOW);
					ERRprintf("ERROR: Had to try opening " DEFAULT_FILENAME " in current directory\n");
				}
			}
			if (io == NULL) return(FALSE);

														/* And additionally open as TIFF file */
			if ( (tf=TiffOpenFile(NULL, io->funit)) == NULL) {
				ERRprintf("ERROR: Problem opening TIFF File, will fail badly\n");
				IO_CloseChannel(io); io=NULL;
				return(FALSE);
			}

			ReverseBits = TRUE;								/* Reverse bit ordering */
			ColorMax    = 1;									/* Single color		    */
			NumPlanes   = 1;									/* Single graphics plane */

			buf = NoCompress ? NULL : malloc(1+MapWidth+MapWidth/120) ;

			free(StripOffset); StripOffset = NULL;		/* Initialize internal */
			free(StripBytes) ; StripBytes  = NULL;
			StripCount  = StripMaxCount = 0;				/* No data yet				*/
			Dirty       = FALSE;								/* Page is not dirty		*/
			Initialized = FALSE;								/* File also not init'd */

			break;

/* -----------------------------------------------------
-- New page.  Do a FF on all but first page
---------------------------------------------------- */
		case 1:
			if (Dirty) {						/* If working on one, finish first */
				TiffPutShort     (tf, IMAGELENGTH,     (USHORT) (StripCount*ScansPerBand));
				TiffPutShortArray(tf, STRIPBYTECOUNTS, (USHORT) StripCount, StripBytes);
				TiffPutLongArray (tf, STRIPOFFSETS,    (USHORT) StripCount, StripOffset);

				TiffWriteIFD(tf);				/* Output the IFD							*/
				StripCount  = 0;				/* And reset counters					*/
				Dirty       = FALSE;			/* And mark no longer dirty			*/
				Initialized = FALSE;			/* And we are not initialized again */
			}

			if (! Initialized) {
				time_t tod;										/* Structures for time	*/
				char timestr[SHORT_STR_SIZE],*aptr;		/* date/time of output	*/

				time(&tod);										/* Get current time */
				strscpy(timestr, asctime(localtime(&tod)), sizeof(timestr));
				if ( (aptr = strchr(timestr,'\n')) != NULL) *aptr = '\0';

				TiffPutLong    (tf, NEWSUBFILETYPE,	0);		/* Trivial image	*/
				TiffPutString  (tf, SOFTWARE, "GENPLOT TIFF DRIVER 2.0");
				TiffPutString  (tf, ARTIST, "Copyright (c) CGS - 1994-95");
				TiffPutString  (tf, DATETIME,	timestr);
				TiffPutShort   (tf, BITSPERSAMPLE, 1);			/* 1 bit/sample	*/
				TiffPutShort   (tf, SAMPLESPERPIXEL, 1);		/* 1 sample/pixel	*/
				TiffPutRational(tf, XRESOLUTION, TransferInfo.xperinch, 1);
				TiffPutRational(tf, YRESOLUTION, TransferInfo.yperinch, 1);
				TiffPutShort   (tf, COMPRESSION, (USHORT) (NoCompress ? 1 : 32773));
				TiffPutShort   (tf, RESOLUTIONUNIT, 2);					/* Use inches		*/
				TiffPutShort   (tf, PHOTOMETRICINTERPRETATION, 0);		/* Black on white	*/
				TiffPutShort   (tf, IMAGEWIDTH, (USHORT) (MapWidth*BITS_PER_MAP));
				TiffPutLong		(tf, ROWSPERSTRIP, ScansPerBand);

				StripCount = 0;
				Initialized = TRUE;
			}
			break;

/* -------------------------------------------
-- End of plot - exit graphics
------------------------------------------- */
		case 3:
			if (Dirty) {						/* If working on one, finish first */
				TiffPutShort     (tf, IMAGELENGTH,     (USHORT) (StripCount*ScansPerBand));
				TiffPutShortArray(tf, STRIPBYTECOUNTS, (USHORT) StripCount, StripBytes);
				TiffPutLongArray (tf, STRIPOFFSETS,    (USHORT) StripCount, StripOffset);

				TiffWriteIFD(tf);				/* Output the IFD							*/
				StripCount  = 0;				/* And reset counters					*/
				Dirty       = FALSE;			/* And mark no longer dirty			*/
				Initialized = FALSE;			/* And we are not initialized again */
			}

			TiffCloseFile(tf);   tf = NULL;			/* Close the channel */
			IO_CloseChannel(io);	io = NULL;			/* Close the channel */

			free(buf);				buf = NULL;
			free(StripOffset);	StripOffset = NULL;
			free(StripBytes);		StripBytes = NULL;
			StripCount = StripMaxCount = 0;

			break;

/* --------------------------
-- Bit map transcription
-------------------------- */
		case 2:

			SWAB((char *)BitMap, (char *)BitMap, BitMapSize);

			if (NoBlanks && !Dirty) {					/* Entire band must be empty */
				for (icnt=0; icnt<(int) BitMapSize; icnt++) {
					if (BitMap[icnt] != 0) break;
				}
				if (icnt >= (int) BitMapSize) break;
			}

			Dirty = TRUE;									/* Will have something on page */

			if (StripCount >= StripMaxCount) {
				StripMaxCount += 100;
				StripBytes  = realloc(StripBytes,  StripMaxCount*sizeof(*StripBytes));
				StripOffset = realloc(StripOffset, StripMaxCount*sizeof(*StripOffset));
			}

/* Set the offset for the strip data once now, using full alignment */
			StripOffset[StripCount] = TiffWriteData(NULL, 0,0, tf, 0);
			if (NoCompress) {
				StripBytes[StripCount] = BitMapSize;
				TiffWriteData(BitMap, 1, BitMapSize, tf, TF_NOPREALIGN | TF_NOPOSTALIGN);
			} else {
				StripBytes[StripCount] = 0;
				for (iband=0; iband<ScansPerBand; iband++) {
					icnt = (int) tiff_encode(BitMap+MapWidth*iband, MapWidth, buf, 1);
					StripBytes[StripCount] += icnt;
					TiffWriteData(buf, 1, icnt, tf, TF_NOPREALIGN | TF_NOPOSTALIGN);
				}
			}
			StripCount++;
			break;

		default:
			return(FALSE);
	}
	return(TRUE);
}


/* ===========================================================================
==============================================================================
==============================================================================
------ TIFF ROUTINES -- TIFF ROUTINES -- TIFF ROUTINES -- TIFF ROUTINES ------
==============================================================================
==============================================================================
=========================================================================== */

/* ===========================================================================
-- Routine to add an IFD entry with a single LONG parameter
--
-- Usage:  int TiffPutLong(TIFF *tf, USHORT tag, ULONG value);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         value - Value of the single entry
--
-- Output: none
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutLong(TIFF *tf, USHORT tag, ULONG value) {

	TIFF_ENTRY *entry;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);
	entry = &tf->entries[tf->ifd_count++];

	entry->tag    = tag;
	entry->type   = T_LONG;
	entry->count  = 1;
	entry->value.lval = value;
	return(0);
}

/* ===========================================================================
-- Routine to add an IFD entry with an associated array of SHORTS
--
-- Usage:  int TiffPutShortArray(TIFF *tf, USHORT tag, USHORT count, USHORT *buf);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         count - Number of array elements to output
--         buf   - pointer to the SHORT array 
--
-- Output: Writes array to disk (if necessary) and adds directory entry
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutShortArray(TIFF *tf, USHORT tag, USHORT count, USHORT *buf) {

	TIFF_ENTRY *entry;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);

	if (count <= 1) {
		return TiffPutShort(tf, tag, *buf);
	} else if (count == 2) {
		return TiffPut2Short(tf, tag, buf[0], buf[1]);
	} else {
		entry = &tf->entries[tf->ifd_count++];
		entry->tag    = tag;
		entry->type   = T_SHORT;
		entry->count  = count;
		entry->value.offset = TiffWriteData(buf, 2, count, tf, 0);
	}
	return(0);
}

/* ===========================================================================
-- Routine to add an IFD entry with an associated array of LONGS
--
-- Usage:  int TiffPutLongArray(TIFF *tf, USHORT tag, USHORT count, ULONG *buf);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         count - Number of array elements to output
--         buf   - pointer to the LONG array 
--
-- Output: Writes array to disk (if necessary) and adds directory entry
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutLongArray(TIFF *tf, USHORT tag, USHORT count, ULONG *buf) {
	TIFF_ENTRY *entry;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);

	if (count <= 1) {
		return TiffPutLong(tf, tag, *buf);
	} else {
		entry = &tf->entries[tf->ifd_count++];
		entry->tag    = tag;
		entry->type   = T_LONG;
		entry->count  = count;
		entry->value.offset = TiffWriteData(buf, 4, count, tf, 0);
	}
	return(0);
}


/* ===========================================================================
-- Routine to add an IFD entry with a single SHORT parameter
--
-- Usage:  int TiffPutShort(TIFF *tf, USHORT tag, USHORT value);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         value - Value of the single entry
--
-- Output: none
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutShort(TIFF *tf, USHORT tag, USHORT value) {

	TIFF_ENTRY *entry;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);
	entry = &tf->entries[tf->ifd_count++];
	
	entry->tag    = tag;
	entry->type   = T_SHORT;
	entry->count  = 1;
	entry->value.sval[0] = value;
	entry->value.sval[1] = 0;
	return(0);
}

/* ===========================================================================
-- Routine to add an IFD entry with two SHORT parameter
--
-- Usage:  int TiffPut2Short(TIFF *tf, USHORT tag, USHORT val1, USHORT val2);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         val1,val2 - Value of the two parameters
--
-- Output: none
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPut2Short(TIFF *tf, USHORT tag, USHORT val1, USHORT val2) {

	TIFF_ENTRY *entry;
	
	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);
	entry = &tf->entries[tf->ifd_count++];

	entry->tag    = tag;
	entry->type   = T_SHORT;
	entry->count  = 1;
	entry->value.sval[0] = val1;
	entry->value.sval[1] = val2;
	return(0);
}

/* ===========================================================================
-- Routine to add an IFD entry with ASCII text
--
-- Usage:  int TiffPutString(TIFF *tf, USHORT tag, char *string);
--
-- Inputs: tf     - open TIFF file unit
--         tag    - TIFF tag for the directory entry
--         string - ASCII string to write (including terminating NULL)
--
-- Output: none
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutString(TIFF *tf, USHORT tag, char *string) {

	TIFF_ENTRY *entry;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);
	entry = &tf->entries[tf->ifd_count++];

	entry->tag    = tag;
	entry->type   = T_ASCII;
	entry->count  = (int) strlen(string)+1;
	entry->value.offset = TiffWriteData(string, 1, strlen(string)+1, tf, 0);
	return(0);
}

/* ===========================================================================
-- Routine to add an IFD entry with a single RATIONAL parameter
--
-- Usage:  int TiffPutRational(TIFF *tf, USHORT tag, ULONG num, ULONG denom);
--
-- Inputs: tf    - open TIFF file unit
--         tag   - TIFF tag for the directory entry
--         num   - Integer representing numerator of the fraction
--         denom - Integer representing denominator of the fraction
--
-- Output: none
--
-- Returns: 0 on success, -1 if tf is not valid
=========================================================================== */
int TiffPutRational(TIFF *tf, USHORT tag, ULONG num, ULONG denom) {

	TIFF_ENTRY *entry;

	struct {
		ULONG numerator;
		ULONG denominator;
	} rational;

	if (tf==NULL || tf->magic!=TIFF_COOKIE || tf->ifd_count>=MAX_ENTRIES) return(-1);
	entry = &tf->entries[tf->ifd_count++];

	rational.numerator   = num;
	rational.denominator = denom;
	
	entry->tag    = tag;
	entry->type   = T_RATIONAL;
	entry->count  = 1;						/* Single rational value */
	entry->value.offset = TiffWriteData(&rational, sizeof(rational), 1, tf, 0);
	return(0);
}

/* ===========================================================================
-- TiffWriteData outputs bytes to the open TIFF file and returns the
-- offset of the data block from the beginning of the file.
--
-- Usage: size_t TiffWriteData(void *buf, size_t size, size_t num, TIFF *tf, int flags);
--
-- Inputs: buf   - pointer to buffer to write
--         size  - Size of each item
--         num   - Number of items to write
--         tf    - pointer to open TIFF file structure
--         flags - or'd behavior modification flags.  0 for normal usage.
--            TF_NOPREALIGN  - don't prealign file to an even byte boundary
--            TF_NOPOSTALIGN - don't postalign file to an even byte boundary
--
-- Output: none
--
-- Returns: File position where data begins (relative to file beginning)
--
-- Note: (1) If flags is zero, the file is positioned on an even byte
--           boundary before the write begins, and is padded so the next
--           write would naturally occur on an even boundary.
--       (2) Size or num may be zero.  Only the alignment is done and the
--           current file position is returned.
=========================================================================== */
ULONG TiffWriteData(void *buf, size_t size, size_t num, TIFF *tf, int flags) {

	size_t ilen, rcode;

	if (tf==NULL || tf->magic!=TIFF_COOKIE) return (ULONG) -1;

/* Unless flags has TF_NOPREALIGN set, make sure we are on an even byte */
	if ( (!(flags & TF_NOPREALIGN)) && (ftell(tf->funit)%2 != 0) )
		fputc('\0', tf->funit);

/* Determine current file position and write */
	rcode = ftell(tf->funit);				/* Return will be current position */
	ilen = size*num;							/* Total # of bytes */
	if (ilen != 0) fwrite(buf, ilen, 1, tf->funit);

/* Unless flags has TF_NOPOSTALIGN set, make sure we are on an even byte */
	if ( (!(flags & TF_NOPOSTALIGN)) && (ftell(tf->funit)%2 != 0) )
		fputc('\0', tf->funit);

	return (ULONG) rcode;
}

/* ===========================================================================
-- Routine to open a TIFF file for input/output.
--
-- Usage:  TIFF *TiffOpenFile(char *filename, FILE *funit);
--
-- Inputs: name  - character string filename to be opened if funit is NULL.
--                 File will be opened for binary write and truncated.
--         funit - either NULL indicating file should be opened, or an open
--                 file stream opened for binary write.  funit will take
--                 precedence.  If opened as funit, unit will not be closed
--                 on TiffCloseFile, but only left at the EOF.
--
-- Output: none
--
-- Returns: (FILE *) like pointer to open TIFF file.  Pointer must be
--          passed to other routines writing TIFF structure.
=========================================================================== */
TIFF *TiffOpenFile(char *name, FILE *funit) {

	TIFF *tf;

	struct {
		USHORT id_vers[2];			/* Identifier & version (packed!!!!) */
		ULONG  ifd_offset;
	} header;

	if ( (tf = malloc(sizeof(TIFF))) == NULL) {
		fprintf(stderr, "ERROR: Unable to allocate space for TIFF structures\n");
		return(NULL);
	} else if (funit != NULL) {								/* Passed opened unit? */
		tf->type = LOCAL_FILE;
	} else if ( (funit=fopen(name,"wb")) != NULL) {		/* Open in binary mode */
		tf->type = PASSED_FILE;
	} else {
		fprintf(stderr, "ERROR: %s failed to open\n", name);
		free(tf);
		return(NULL);
	}

	tf->funit       = funit;
	tf->ifd_pointer = 4;				/* First one at byte 4 */
	tf->ifd_count   = 0;
	tf->magic       = TIFF_COOKIE;

	/* First word is II for Intel byte order and MM for Motorola order */
	header.id_vers[0] = (BYTE_ENDIAN_ORDER==LITTLE_ENDIAN) ? 0x4949 : 0x4D4D;
	header.id_vers[1] = 0x002A;
	header.ifd_offset = 0;
	fwrite(&header, sizeof(header),1, tf->funit);

	return(tf);
}

/* ===========================================================================
-- Routine to write out the current TIFF IFD directory and reset to be ready
-- to start a new one.  Must now output all directory structures and mark
-- the previous pointer with the actual address.
--
-- Usage: int TiffWriteIFD(TIFF *tf);
--
-- Inputs: tf - pointer to open TIFF file structure
--
-- Output: Writes directory (if any entries present) to file
--
-- Returns: 0 if successful, -1 otherwise.
=========================================================================== */
int TiffWriteIFD(TIFF *tf) {

	ULONG ifd_offset, posn;

	if (tf==NULL || tf->magic!=TIFF_COOKIE) return(-1);
	if (tf->ifd_count == 0) return(0);
	
	ifd_offset = TiffWriteData(NULL, 0,0, tf, 0);	/* Put on even boundary */

/* -------------------------------------------------------------------
-- Write directory structure consisting of:
--   (1) A USHORT count - number of directory entries
--   (2) The count directory entries (each 12 bytes)
--   (3) A ULONG pointer to the next directory, initially set to 0
------------------------------------------------------------------- */
	fwrite(&tf->ifd_count, 2, 1, tf->funit);
	fwrite( tf->entries, sizeof(*tf->entries), tf->ifd_count, tf->funit);
	posn = 0;
	fwrite(&posn, 4, 1, tf->funit);

/* Rewind to last ifd_pointer, write actual IFD address, and reset position */
	posn = ftell(tf->funit);
	fseek(tf->funit, tf->ifd_pointer, SEEK_SET);
	fwrite(&ifd_offset, 1, 4, tf->funit);
	fseek(tf->funit, posn, SEEK_SET);
	tf->ifd_pointer = posn-4;

	tf->ifd_count   = 0;										/* And mark as written */
	return(0);
}


/* ===========================================================================
-- Routine to close an opened TIFF file.  Will output any pending IFD
-- directory, and then delete internal structures.  If TiffOpenFile() had
-- actually opened the file, it will be closed at this time also.
--
-- Usage:  int TiffCloseFile(TIFF *tf);
--
-- Inputs: tf - pointer to open TIFF file structure
--
-- Output: Potentially writes directory and closes TIFF structures and file.
--
-- Returns: 0 if successful, -1 otherwise.
--
-- Note: If TIFF was opened by name, then file will be closed.  If opened
--       as a FILE * pass, then file will be left open for user to close.
=========================================================================== */
int TiffCloseFile(TIFF *tf) {

	if (tf==NULL || tf->magic!=TIFF_COOKIE) return(-1);

	TiffWriteIFD(tf);										/* Write any partial IFD */

/* Maybe close file, unset MagicCookie and free structure */
	if (tf->type == LOCAL_FILE) fclose(tf->funit);
	tf->magic = 0;											/* Mark as invalid */
	free(tf);

	return(0);
}
