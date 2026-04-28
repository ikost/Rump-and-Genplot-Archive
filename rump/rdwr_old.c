/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ----------------- */
/*  ---------                                              ----------------- */
/*  ---------    The source code to RUMP may be freely     ----------------- */
/*  ---------  modified as long as this copyright notice   ----------------- */
/*  ---------          is included and unchanged.          ----------------- */
/*  ------------------------------------------------------------------------ */

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
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "spectrum.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	LOCAL_DEBUG(str)	/* ERRprintf(str) */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static BOOL RMBinaryMode(FILE *funit);
static int  RMReadRecord(void *buf, size_t len);
static void RMReverse(void *buffer, size_t width, INT num);

static void set_null(char *str, int len);
static void rbs_pack(REAL *cin, int npt, REAL *ctmp, int npt2);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static enum {NONE,RMFTN,UNIXLIKE} BinaryType;	/* Type of binary file		*/
static BOOL ReverseBytes=FALSE;				/* Must I reverse bytes?	*/
static FILE	*FileHandle=NULL;						/* Handle for the open file	*/


/* ===========================================================================
-- Routine to read old RMFortran type data structures for an RBS spectrum.
-- Converts to the internal SPECTRUM structure of use in RUMP. 
--
-- Usage: BOOL RbsRdFileOld(char *filename, SPECTRUM *buf);
--
-- Inputs: filename - file to opened and read.  .RBS is not assumed.
--         buf      - pointer to SPECTRUM structure to receive data
--
-- Output: *buf     - fills in all elements of the structure
--
-- Return: TRUE  - successful read
--         FALSE - some error.
=========================================================================== */
static struct {
	REAL m;
	int z,cbeam;
} old_beam[] = {
	{4.00150586f, 2, 2},		/*  He++	*/
	{4.00150586f, 2, 1},		/*  He+	*/
	{1.00727648f, 1, 1},		/*  H+	*/
	{3.01603f   , 2, 1},		/*  He3+ */
	{2.01355326f, 1, 1}		/*  D+	*/
};

int RbsRdFileOld( char *filen, SPECTRUM *buf) {

	char version[12];
	int partcl, ivers, npt2, rc;
	REAL *rdbuf;
	FILE *lun;

#if (defined CSET2 || defined GNU_C || defined MSC70)
	#pragma pack(2)
#endif
	#define REC_1_LENGTH	(3*IDCHR+11*4+3*2)	/* RM FORTRAN size of record 1 */
	struct {
		char id[IDCHR], date[IDCHR];
		REAL32 e0, q, kevch, kev0, theta, phi, omega, corr;
		char ltct[IDCHR];
		REAL32 first, fwhm, current;
		INT16 geom, partcl, npt;
	} rec_1;
	#define REC_2_LENGTH	(2*2+2*4)				/* RM FORTRAN size of record 2 */
	struct {
		INT16 zbeam; REAL32 mbeam;
		INT16 cbeam; REAL32 psi;
	} rec_2;
#if (defined CSET2 || defined GNU_C || defined MSC70)
	#pragma pack()
#endif

/* Check that structures have correct packing to read directly */ 
	if (( lun = fopen(filen,"rb")) == NULL) {
		ERRprintf("ERROR: %s failed to open\n", filen);
		return(FALSE);
	} else if (! RMBinaryMode(lun)) {
		fclose(lun);
		LOCAL_DEBUG("ERROR: File is not in old RUMP RM/FORTRAN format\n");
      return(FALSE);
	} else if (sizeof(rec_1) != REC_1_LENGTH || sizeof(rec_2) != REC_2_LENGTH) {
		ERRprintf("ERROR: This machine can't read ancient RUMP format due to packing restrictions\n");
		fclose(lun);
		return(FALSE);
	}

/* Assume tmp SPECTRUM buffer is properly initialized (via alloc earlier) */
	SysQualifyPath(buf->filename, filen, sizeof(buf->filename));

   if (RMReadRecord(version,11) != 0) goto ReadErr;	/* Read version */
	version[11] = '\0';											/* Terminate it */
	if (strnicmp(version,"VERSION 1.2", 11) == 0) {
		ivers = 12;
	} else if (strnicmp(version, "RUMP (v1.3)", 11) == 0) {
		ivers = 13;
	} else {										/* Don't handle this old */
		LOCAL_DEBUG("ERROR: RMFORT version was not recognized\n");
		goto ReadErr;
	}

	if (RMReadRecord(&rec_1, sizeof(rec_1)) != 0) {
		LOCAL_DEBUG("ERROR: Unable to read first record\n");
		goto ReadErr;
	}

	if (ReverseBytes) {
		RMReverse(&rec_1.e0,    sizeof(REAL32), 8);	/* e0 --> corr			*/
		RMReverse(&rec_1.first, sizeof(REAL32), 3);	/* first --> current	*/
		RMReverse(&rec_1.geom,  sizeof(INT16),3);	/* geom  --> npt		*/
	}
		 
	strscpy(buf->id,   rec_1.id,   sizeof(buf->id));   set_null(buf->id,   sizeof(buf->id));
	strscpy(buf->date, rec_1.date, sizeof(buf->date)); set_null(buf->date, sizeof(buf->date));
	strscpy(buf->ltct, rec_1.ltct, sizeof(buf->ltct)); set_null(buf->ltct, sizeof(buf->ltct));

	buf->e0      = rec_1.e0;			buf->q      = rec_1.q;
	buf->kevch   = rec_1.kevch;		buf->kev0   = rec_1.kev0;
	buf->theta   = rec_1.theta;		buf->phi    = rec_1.phi;
	buf->omega   = rec_1.omega;		buf->corr   = rec_1.corr;
	buf->first   = rec_1.first;		buf->fwhm   = rec_1.fwhm;
	buf->current = rec_1.current;
	buf->geom    = (GEOMETRY_TYPE) rec_1.geom;
	buf->npt     = rec_1.npt;

	if (ivers >= 13) {						/* New versions	*/
		LOCAL_DEBUG("MSG: Reading versions beyond 1.3\n");
		if (RMReadRecord(&rec_2, sizeof(rec_2)) != 0) {
			LOCAL_DEBUG("ERROR: Record read within 1.3 failed\n");
			goto ReadErr;
		}
		if (ReverseBytes) {
			RMReverse(&rec_2.zbeam, sizeof(INT16), 1);
			RMReverse(&rec_2.mbeam, sizeof(REAL32),  1);
			RMReverse(&rec_2.cbeam, sizeof(INT16), 1);
			RMReverse(&rec_2.psi,   sizeof(REAL32),  1);
		}
		buf->mbeam = rec_2.mbeam;
		buf->cbeam = rec_2.cbeam;
		buf->psi   = rec_2.psi;

		if (RMReadRecord(&npt2, sizeof(npt2)) != 0) {
			LOCAL_DEBUG("ERROR: npt2 read within 1.3 failed\n");
			goto ReadErr;
		}
		if (ReverseBytes) RMReverse(&npt2, sizeof(INT16), 1);

		rdbuf = malloc(npt2 * sizeof(REAL32));
		if (RMReadRecord(rdbuf, npt2*sizeof(REAL32)) != 0) {
			LOCAL_DEBUG("ERROR: data read within 1.3 failed\n");
			goto ReadErr;
		}
		if (ReverseBytes) RMReverse(rdbuf, sizeof(REAL32), npt2);
		rbs_pack(buf->counts, buf->npt, rdbuf, npt2);
		free(rdbuf);

	} else {
		LOCAL_DEBUG("MSG: Reading versions 1.2 and earlier\n");
		buf->psi = 0.0;
		if (buf->geom == GENERAL)
			ERRprintf("WARNING: PSI value lost in file I/O\n");
		partcl = max(0, min(rec_1.partcl, 4));
		buf->zbeam = old_beam[partcl].z;
		buf->mbeam = old_beam[partcl].m;
		buf->cbeam = old_beam[partcl].cbeam;
		if ((rc = RMReadRecord(buf->counts, buf->npt*sizeof(REAL32))) != 0) {
			LOCAL_DEBUG("ERROR: data read within 1.2 failed\n");
			goto ReadErr;
		}
		if (ReverseBytes) RMReverse(buf->counts, sizeof(REAL32), buf->npt);
	}

	if (buf->kevch == 0.0) buf->kevch = 1.0;
	buf->dirty  = FALSE;						/* Now clean					*/
	buf->modify = FALSE;						/* Parameters unchanged		*/
	fclose(lun);
	return(TRUE);

/* --------------------------------------------------------------------- */
ReadErr:									/* Here if something wrong with reading */
	fclose (lun);
	buf->npt = 0;						/* Still can't do it */
	ERRprintf("ERROR: RBS Data format in %s not recognized\n", filen);
	return(FALSE);
}


/* ===========================================================================
--  Subroutine to compress/uncompress input RBS record
--
--  Usage: CALL RBS_PACK(CIN,NPT, CTMP,NPT2)
--
--  Inputs: CIN,NPT  - uncompressed data and number of points
--          CTMP,NPT -   compressed data and number of points
--
--  Output: Which ever is requested
=========================================================================== */
static void rbs_pack(REAL *cin, int npt, REAL *ctmp, int npt2) {

	int i, i_start, i_npt, i_ld;

	if (npt == npt2) {							/* Simple copy */
		for (i=0; i<npt2; i++) cin[i] = ctmp[i];
	} else {
		for (i=0; i<npt; i++) cin[i] = 0.0;		/* Pre-blank fill */

		i_ld = 0;
		while (i_ld < npt2) {						/* Are we done! */
			i_start = (int) (ctmp[i_ld] - 1);	/* C arrays		 */
			i_npt   = (int) (ctmp[i_ld+1]);
			for (i=0; i<i_npt; i++) cin[i_start+i] = ctmp[i_ld+2+i];
			i_ld = i_ld+i_npt+2;
		}
	}

	return;
}


/* ===========================================================================
=========================================================================== */
static void set_null(char *str, int len) {

	char *aptr;

	for (aptr=str+len-1; aptr>=str; aptr--) {
		if (*aptr != '\0' && ! isspace(*aptr)) break;
	}
	*(++aptr) = '\0';
	return;
}


/* ------ Routine to determine the mode of the current binary file -------- */
#define	RMBUFSIZE		4096

#if BYTE_ENDIAN_ORDER == LITTLE_ENDIAN	/* PC format machines read it correct */
	#define	RMRecLen		0x00000013
	#define	RMRMRev		FALSE				/* No reversal needed */
#else												/* Others read it byte reversed */
	#define	RMRecLen		0x13000000
	#define	RMRMRev		TRUE				/* Byte reversal required */
#endif


/* ---------------------------------------------------------------------------
-- Painful routine to check the format of a binary data file and properly
-- handle the reads of both types.  For writing, we use the default local.
--------------------------------------------------------------------------- */
static BOOL RMBinaryMode(FILE *funit) {
	
	UINT32 reclen;											/* Must be a 4 byte integer */

	if (fread(&reclen, sizeof(reclen), 1, funit) != 1) return(FALSE);
	fseek(funit, 0L, SEEK_SET);					/* Rewind */
	FileHandle = funit;

	if (reclen != RMRecLen) return(FALSE);		/* Is it expected pattern? */
	BinaryType   = RMFTN;
	ReverseBytes = RMRMRev;
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static void RMReverse(void *buffer, size_t width, INT num) {

	char *buf=buffer;
	char tmpbuf[8], *aptr;
	register size_t i;

	while (num--) {
		memcpy(tmpbuf, buf, width);
		aptr = tmpbuf+width;
		for (i=0; i<width; i++) *(buf++) = *(--aptr);
	}
	return;
}

/* -------------------------------------------------------------------------
-- Routine to read Ryan-McFarland style records from disk to fill len bytes.
-- Will read multiple records from known maximum size RMBUFSIZE
--
-- Usage: int RMReadRecord(void *buf, size_t len);
--
-- Inputs: len - number of bytes we want to read (ignoring blocking bytes)
--
-- Output: Fill buf with the bytes read (guarenteed to read exactly len)
--
-- Return:  0 - everything succeeded
--         -1 - read failure.  Unable to read or mismatch of block bytes
--         -2 - record was not of expected size.  File is rewound to the
--              start of record length to allow multiple attempts
--------------------------------------------------------------------------- */
static int RMReadRecord(void *buf, size_t len) {

	UINT32 reclen;
	size_t iread;
	size_t extra;

	extra = (BinaryType==RMFTN) ? 8 : 0;		/* Extra count on record length */
	while (len) {
		iread = min(len, RMBUFSIZE);
/*		TTYprintf("Extra: %d  iread: %d  len: %d\n", extra, iread, len); */
		if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) return(-1);
		if (ReverseBytes) RMReverse(&reclen, sizeof(reclen), 1);
/*		TTYprintf("record length: %d versus expected %d\n", reclen, iread+extra); */
		if (reclen != iread+extra) {
			fseek(FileHandle, -((long) sizeof(reclen)), SEEK_CUR);
			return(-2);
		}
		if (fread(buf, iread, 1, FileHandle) != 1) return(-1);
		if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) return(-1);
		if (ReverseBytes) RMReverse(&reclen, sizeof(reclen), 1);
		if (reclen != iread+extra) return(-1);
		len -= iread;
		buf = (void *) (((char *)buf) + iread);
	}
	return(0);
}
