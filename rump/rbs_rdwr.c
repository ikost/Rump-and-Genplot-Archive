/* rbs_rdwr.c */

/* ===========================================================================
-- There are relatively simple requirements for these routines.  However,
-- nothing will work if these are not met.
--
--  1. There must be typedef's for
--         INT16, UINT16, INT32 and UINT32 representing 2 and 4 byte integers
--         REAL32, REAL64                  representing 4 and 8 byte floats
--  2. sizeof(UINT32) and sizeof(REAL32) must be exactly the same size
--  3. sizeof(unsigned char) must be exactly 1 byte
--  4. If (REAL32) is not a 4 byte IEEE representation of a real number, then
--     #define CONVERT_IEEE and write the following routines (skeletons at
--     end of this file).  Bit ordering of REAL32 must correspond to standard.
--          (unsigned long) float_to_ieee(REAL32 x)
--          (REAL32)        ieee_to_float(unsigned long x)
--
--  Some of the code may look inefficient but will almos always properly handle
--  the conversion from byte swapped order on all machines.  Only the
--  floating point representation is questionable.
--
-- Except for the forced size constants, attempt to use standard REAL and
-- int throughout this code.
--
-- 8/10/94 - MOT - Version 1.1
--        Enabled the zero compression features which can compress about 30%
--        better than previous level.  Added RbsSetFileWriteVersion() to
--        allow specification of the compatibility mode.  Default is still
--        the old DOS version, but will change soon.
--   Reads versions  [1.0] through [1.1]
--   Writes versions [1.0] through [1.1]    - default is [1.0]
--
-- This code now includes special pipe reading ability.  Probably best not
-- to distribute this version.
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE					/* Always require POSIX standard */
#include "preload.h"

/* #define	CONVERT_IEEE	*/		/* Uncomment enables float->IEEE routines */
/* #define	DEBUG_MODE		*/		/* Rather verbose testing						*/
/*	#define	NO_PIPE_MODE	*/		/* Disable pipe mode read/writes				*/
/*	#define	NO_EA_MODE		*/		/* Disable extended attributes				*/
/*	#define	LOCAL_MODE		*/		/* If defined, ERRprintf/TTYprintf local	*/

/* ------------------------------------------------------------------------ */
/* -------------------- NO USER SERVICABLE PARTS BELOW -------------------- */
/* ---------------- REFER SERVICING TO QUALIFIED PERSONNEL ---------------- */
/* ------------------------------------------------------------------------ */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#ifndef LOCAL_MODE
	#include "extends.h"
#endif
#include "spectrum.h"

#if (defined OS2 && !defined NO_EA_MODE)	/* For setting extended attributes */
	#include <ea.h>
#endif

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#ifndef TRUE
	#define	TRUE	1
#endif
#ifndef FALSE
	#define	FALSE	0
#endif

#ifdef NO_PIPE_MODE								/* If pipes disabled, do by def's */
	#define	popen(filename,mode)	NULL
	#define	pclose(handle)
#endif

#define VERSION_ID "RUMP data via POSIX C [v 1.1 - 8/1/94 MOT]"

#define RUMP_ID          0x10211210L				/* Rump ID string       */
#define MAJOR_REV_LEVEL  0x0001                 /* Major revision level */
#define MINOR_REV_LEVEL  0x0001                 /* Minor revision level */

#define PROGRAM_ID_REC   0x00000000             /* Record types        */
#define COMMENT_REC      0x00000001             /* Comment             */
#define SILENT_REC       0x00000002             /* Silent comment      */
#define DATA_INIT_REC    0x00000010             /* Start data records  */
#define ARRAY_INIT_REC	 0x00000020             /* Start array records */
#define DATA_REC         0x00000011             /* Actual data records */
#define DATA_REC_0		 0x00000012					/* Data in compress 0  */
#define DATA_REC_1		 0x00000013					/* Data in compress 1  */
#define DATA_REC_2		 0x00000014					/* Data in compress 2  */
#define DATA_REC_3		 0x00000015					/* Data in compress 3  */
#define ID_REC           0x00000101             /* ID string record    */
#define LTCT_REC         0x00000102             /* LTCT string record  */
#define DATE_REC         0x00000103             /* DATE string record  */
#define CORR_REC         0x00000110             /* Correction factor   */
#define ACCEL_REC        0x00000111             /* Accelerator record  */
#define MCA_REC          0x00000112             /* MCA parameters      */
#define RBS_REC          0x00000120             /* RBS parameters      */
#define FRES_REC         0x00000121             /* FRES parameters     */
#define PIXE_REC         0x00000122             /* PIXE parameters     */
#define NREAC_REC        0x00000123             /* Nuclear reactions   */

/* Space needed for buffer to hold at least one block */
#define	BUFFER_SPACE_NEEDED	4*1027		/* 1024 plus checksum longs */
#define	BUFFER_WRITE_NEEDED	4*1027+7		/* Extra for compress overwrite */

#ifdef DEBUG_MODE
	static struct {
		int id;
		char *name;
	} types[] = {	{PROGRAM_ID_REC,	"ID record"},
						{COMMENT_REC,		"Comment record"},
						{SILENT_REC,		"Silent comment"},
						{DATA_INIT_REC,	"Data start record"},
						{ARRAY_INIT_REC,	"Array start record"},
						{DATA_REC,			"Data record"},
						{DATA_REC_0,		"Data record (compression 0)"},
						{DATA_REC_1,		"Data record (compression 1)"},
						{DATA_REC_2,		"Data record (compression 2)"},
						{DATA_REC_3,		"Data record (compression 3)"},
						{ID_REC,				"Ident record"},
						{LTCT_REC,			"LTCT record"},
						{DATE_REC,			"Date record"},
						{CORR_REC,			"Correction record"},
						{ACCEL_REC,			"Accelerator record"},
						{MCA_REC,			"MCA record"},
						{RBS_REC,			"RBS record"},
						{FRES_REC,			"FRES record"},
						{PIXE_REC,			"PIXE record"},
						{NREAC_REC,			"NUCL record"},
						{-1,					"Unknown record"} };

	static char *gettype(int type) {
		int i;
		for (i=0; types[i].id != -1; i++) {
			if (types[i].id == type) break;
		}
		return(types[i].name);
	}
#endif

#ifdef LOCAL_MODE
	#define	ERRprintf	printf
	#define	TTYprintf	printf
	#define	strscpy		strncpy
	#define	stricmp		strcmp
	#define	DFLT_STR_SIZE	256
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static  int   read_record(int *item_cnt, int ShowErrors);
static  void  get_str(char *result, size_t len);
static  INT32 get_int32(void);
static  INT16 get_int16(void);
static  REAL  get_real(void);

static  int read_data_records(REAL *counts, int npt, int compress, char **errmsg);
static  int read_compress(REAL *counts,int npt,int *npt_read);

static  void wr_str(int record_type, char *value);
static  void wr_data(int npt, int nspectra, REAL counts[]);
static  void write_record(int record_type);
static  void put_byte(unsigned char value);
static  void put_int16(UINT16 value);
static  void put_int32(UINT32 value);
static  void put_real(REAL value);
static  void put_string(char *string, size_t length);

static  int check_c_reals(void);
static  UINT32 MakeCheckSum(int num_elem);

static int UnZeroCompress(void *data, int length);
static int TryZeroCompress(void *data, int length);

#if defined(CONVERT_IEEE)
 static  REAL32 ieee_to_float(UINT32 value);
 static  UINT32 float_to_ieee(REAL32 value);
#endif

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- These parameters define how this read/write routine is used by RUMP.
--
-- RbsRdwrID       - Simple token identifying this routine.  Can be used
--                   to force read using a specific file protocol.
-- RbsRdwrDesc     - Description of data format.  All loaded read modules 
--                   can be enumerated with the command "READ -CONFIG".
-- RbsRdwrExtList  - semicolon delimited list of extensions that are 
--                   defined to use this read routine.  Any number of
--                   extensions of any length may be specified.
-- RbsRdwrOptions  - Or'd list of options
--                      R_MAKE_DEFAULT - this module replaces RUMP default
--                             and will be used to read unknown file formats.
--                      R_HAS_WRITE  - module includes a routine to rewrite
--                             the data format.  Must then define the
--                             routine RbsWriteProc.  Bit will be
--                             turned off if routine not present.
--                      R_USE_BINARY - file handles passed to ReadProc() or
--                             WriteProc() will be opened in binary
--                             (as opposed to ASCII) mode.
--                      R_DONT_OPEN  - Tells RUMP not to open files before
--                             calling ReadProc() and WriteProc().  They 
--                             must be allowed to open the file.  This 
--                             should only be used for such modules as an
--                             MCA access routine.  Default is for RUMP
--                             to open the file, including handling
--                             pipes and compressed files - returning
--                             only a handle for user to read.  HANDLE
--                             will be NULL in calls to ReadProc().  Routine
--                             is responsible for its own cleanup.
--                      R_NO_PIPE    - ReadProc() cannot handle a pipe read.
--                             This should only be necessary if file
--                             repositioning is required.
-- RbsReadProc()   - Actual read routine.  Reads from open stream and fills
--                   in the spectral data.  Stream may be a file or pipe.
-- RbsWriteProc()  - If you want to handle writes (from limited RUMP data
--                   storage SPECTRUM format), define this routine.  Takes
--                   existing SPECTRUM structure and an open handle and 
--                   writes the data.  (Normally not defined in user code.)
=========================================================================== */

char *RbsRdwrID       = "RUMP";
char *RbsRdwrDesc     = "RUMP Interchange Data Format v. 1.1";
char *RbsRdwrExtList  = ".rump;.rbs;.frs;.fres;.pixe";

int   RbsRdwrOptions  = R_HAS_WRITE | R_USE_BINARY;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static FILE *handle = NULL;						/* Handle to open file      */
static unsigned char *buffer, *bufptr;			/* Buffer/pointer to buffer */
static int minor_rev_level = 0;					/* Default is compatibility */


/* ===========================================================================
-- Function to read a binary RBS file from disk.  Modifies all entries in
-- the structure that are defined in the file.
--
--  Usage:  SPECTRUM *RbsReadProc(file *funit, CHAR *path, SPECTRUM *spectra, int *err);
--
--  Inputs: funit    - stream opened for binary reads
--          path     - (unused) Path of opened file (or to be opened)
--          spectra  - pointer to a current spectrum structure.  If this
--                     entry is NULL, a new structure will allocated,
--                     initialized with default values, and then filled.
--
--  Output: err - If not NULL, will receive the error code
--                0 - successful read of data
--                1 - unable to open file
--                2 - unable to allocate buffer for reading
--               -3 - unable to allocate buffers space for counts
--               -2 - bad file format from the beginning
--               -1 - bad file format somewhere during reading (after headers)
--
--  Returns: pointer to spectrum (either spectra or the malloc'd space).  On
--           error, will return NULL.
--
--  Notes:  The spectra->counts pointer is handled differently depending
--          on its initial value.  If spectra->counts is initially NULL
--          pointer, then sufficient space is allocated to hold the
--          incoming data.  However, if not, the data is loaded into the
--          existing buffer if spectra->nptmax is adequate.  Otherwise, the
--          existing buffer if free()'d, and new space allocated.
=========================================================================== */
SPECTRUM *RbsReadProc(FILE *funit, CHAR *path, SPECTRUM *spectra, int *err) {

	struct {												/* Version numbers */
		int major;
		int minor;
	} vers;

	int npt, compression, record_type, item_cnt;
	char result[DFLT_STR_SIZE];
	char *errmsg;

	check_c_reals();									/* Check format #'s	*/

/* Copy over the file handle to global copy */
	handle = funit;

/* Allocate buffer space */
	if ( (buffer = malloc(BUFFER_SPACE_NEEDED)) == NULL) {
		if (err != NULL) *err = 2;
		return(NULL);
	} else if (spectra == NULL && (spectra=RbsAllocateSpectrum(NULL, 0)) == NULL) {
		free(buffer);
		if (err != NULL) *err = 2;
		return(NULL);
	}

/* Check for valid RUMP format */
	if ((record_type = read_record(NULL, TRUE)) < 0 || 
		  record_type != PROGRAM_ID_REC        ||
		  get_int32()  != RUMP_ID) {
		free(buffer);
		if (err != NULL) *err = -2;				/* Maybe try old if not a pipe */
		return(NULL);									/* Probably not us */
	}

	vers.major = get_int16();
	vers.minor = get_int16();
	if (vers.major != MAJOR_REV_LEVEL) {
		ERRprintf("WARNING: Major revision level for file is different\n");
	} else if (vers.minor > MINOR_REV_LEVEL) {
		ERRprintf("WARNING: Minor revision is beyond this read code\n");
	}
/*	printf("RBS file header.  [v. %d.%2.2d]\n", vers.major, vers.minor); */

/* Read records until we get the DATA record */
	while (TRUE) {
		if ((record_type = read_record(&item_cnt, TRUE)) < 0) {
			errmsg = NULL;															/* Error message already printed */
			goto bad_data_file;
		}
		switch(record_type) {
			case PROGRAM_ID_REC:				/* program specifier record			*/
				if (get_int32() != RUMP_ID) {
					errmsg = "Invalid RUMP ID record\n"; 
					goto bad_data_file;
				}
				break;
			case COMMENT_REC:					/* printed comment record				*/
				get_str(result, sizeof(result));
				TTYprintf("%s\n",result);
				break;
			case SILENT_REC:					/* unprinted comment record			*/
				break;
			case ID_REC:						/* id string record						*/
				get_str(spectra->id, sizeof(spectra->id));
				break;
			case LTCT_REC:						/* live time/clock time record		*/
				get_str(spectra->ltct, sizeof(spectra->ltct));
				break;
			case DATE_REC:						/* date record								*/
				get_str(spectra->date, sizeof(spectra->date));
				break;
			case CORR_REC:						/* correction factor record			*/
				spectra->corr = get_real();			/* Correction value			*/
				break;
			case ACCEL_REC:					/* accelerator parms record			*/
				spectra->e0   =       get_real();	/* Beam Energy (MeV)			*/
				spectra->zbeam= (int) get_int32();	/* Z of incident beam		*/
				spectra->mbeam=       get_real();	/* Mass of incident beam	*/
				spectra->cbeam= (int) get_int32();	/* Charge of incident beam	*/
				spectra->q    =       get_real();	/* Total integrated charge	*/
				spectra->current =    get_real();	/* Beam current				*/
				break;
			case MCA_REC:						/* mca parms record						*/
				spectra->kevch   =  get_real();		/* keV/channel on MCA		*/
				spectra->kev0    =  get_real();		/* keV of channel 0			*/
				spectra->first   =  get_real();		/* Starting channel on MCA	*/
				spectra->fwhm    =  get_real();		/* Detector resolution		*/
				if (item_cnt > 4)
					spectra->tau  =  get_real();		/* MCA shaping constant		*/
				break;
			case RBS_REC:						/* Normal RBS scattering record		*/
				spectra->type  = RBS;					/* Now sure we are RBS     */
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle) */
				spectra->phi   = get_real();			/* (180-scattering angle)  */
				spectra->psi   = get_real();			/* (exit target angle)     */
				spectra->omega = get_real();			/* Detector solid angle    */
				break;
			case FRES_REC:						/* forward recoil record				*/
				spectra->type  = FRES;					/* Now sure we are FRES		*/
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle)	*/
				spectra->phi   = get_real();			/* (180-scattering angle)	*/
				spectra->psi   = get_real();			/* (exit target angle)		*/
				spectra->omega = get_real();			/* Detector solid angle		*/
				break;
			case PIXE_REC:							/* xray emission record				*/
				spectra->type  = PIXE;					/* Now sure we are PIXE		*/
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle)	*/
				spectra->phi   = get_real();			/* (180-scattering angle)	*/
				spectra->psi   = get_real();			/* (exit target angle)		*/
				spectra->omega = get_real();			/* Detector solid angle		*/
				break;
			case DATA_INIT_REC:					/* data initialization record		*/
			case ARRAY_INIT_REC:					/* array initialization record	*/
				compression  = (int) get_int32();		/* Type compression	*/
				if (compression < 0 || compression > 3) {
					errmsg = "Unrecognized data compression format\n";
					goto bad_data_file;
				}

/* Get the number of points per spectrum and # of spectrum stored */
				spectra->npt      = (int) get_int32();
				spectra->nspectra = (record_type == ARRAY_INIT_REC) ? get_int32() : 1 ;

				npt = spectra->npt * spectra->nspectra;		/* Number needed in data */

				if (spectra->counts == NULL || npt > spectra->nptmax) { 
					if (spectra->counts != NULL) free(spectra->counts);
					spectra->nptmax = 0;
					spectra->counts = calloc(npt, sizeof(*spectra->counts));
					if (spectra->counts == NULL) {
						ERRprintf("HOLY SHIT: I can't believe I'm out of memory!\n");
						free(buffer);
						if (err != NULL) *err = -3;
						return(NULL);
					}
					spectra->nptmax = npt;
				}

				if (read_data_records(spectra->counts, npt, compression, &errmsg) != 0)
					goto bad_data_file;
				
				spectra->dirty  = FALSE;			/* Now clean					*/
				spectra->modify = FALSE;			/* Parameters unchanged		*/
				free(buffer);							/* Close down and go!		*/
				if (err != NULL) *err = 0;
				return(spectra);

			case DATA_REC:							/* Data record (not allowed!!)	*/
			case DATA_REC_0:
			case DATA_REC_1:
			case DATA_REC_2:
			case DATA_REC_3:
				errmsg = "Data record read before initialized\n";
				goto bad_data_file;

			case NREAC_REC:						/* nuclear reaction record			*/
				spectra->type = NUCLEAR;
				ERRprintf("Nuclear reaction records type not implemented yet\n");
				break;

			default:									/* don't recognize					*/
				ERRprintf("Unimplemented record type: %8.8lx\n",record_type);
		}
	}

bad_data_file:
	if (errmsg != NULL) ERRprintf("ERROR: %s\n", errmsg);
	free(buffer);
	if (err != NULL) *err = -1;
	return(NULL);

}

/* ===========================================================================
-- Function to set the revision level for RbsWriteProc().  The default
-- mode is to write to the 1.0 revision level.  This call will enable writing
-- compressed data to the 1.1 specification.
--
--  Usage:  int RbsSetFileWriteVersion(int minor_level);
--
--  Inputs: minor_level - level of writes tolerated
--                        0 => limit to version 1.0 format
--                        1 => limit to version 1.1 format
--                       <0 => no change, report level only
--
--  Returns: previous minor_level enabled
=========================================================================== */
int RbsSetFileWriteVersion(int minor_level) {

	int rcode;

	rcode = minor_rev_level;						/* Old level */

	if (minor_level > 0 && minor_level <= MINOR_REV_LEVEL) {
		minor_rev_level = minor_level;
	} else if (minor_level > 0) {
		ERRprintf("WARNING: Revision level %d for RbsWriteVersion is invalid\n",
			minor_level);
	}

	return(rcode);
}


/* ===========================================================================
-- Function to write a binary RBS file to disk.  The filename is taken from
-- the SPECTRUM structure.  This is a standard write that stores all parameters
-- currently relevent to RBS analysis.
--
--  Usage:  int RbsWriteProc(FILE *funit, CHAR *path, SPECTRUM *spectra)
--
--  Inputs: funit   - points to an open file in "wb" mode for writing
--          path    - (unused) Path of opened file (or to be opened)
--          spectra - pointer to a spectrum structure containing all of
--                    the current parameters and data.
--
--  Output: Outputs record as specified
--
--  Returns:  0 - successful write
--           -1 - unable to allocate buffer space for writing
=========================================================================== */
int RbsWriteProc(FILE *funit, CHAR *path, SPECTRUM *spectra) {

	INT32 rev_level;
	char fileID[80];								/* Can be 80 since known usage */

	check_c_reals();								/* Check format #'s */
	handle = funit;

	if ( (buffer = malloc(BUFFER_WRITE_NEEDED)) == NULL) return(-1);

	bufptr = buffer+8;							/* Establish start point */
	put_int32(RUMP_ID);							/* RUMP ID value    */
	rev_level = (MAJOR_REV_LEVEL << 16) | (minor_rev_level & 0xFFFF);
	put_int32(rev_level);							/* Set version numbers	*/
	write_record(PROGRAM_ID_REC);

/* Write initial header strings to disk */
	sprintf(fileID, "PC-RUMP data file [v %1d.%1d]", MAJOR_REV_LEVEL, minor_rev_level);
	wr_str(SILENT_REC, fileID);
	wr_str(SILENT_REC, VERSION_ID);
	wr_str(ID_REC  , spectra->id);
	wr_str(LTCT_REC, spectra->ltct);
	wr_str(DATE_REC, spectra->date);

/* Write accelerator record */
	put_real(spectra->e0);
	put_int32(spectra->zbeam);
	put_real(spectra->mbeam);
	put_int32(spectra->cbeam);
	put_real(spectra->q);
	put_real(spectra->current);
	write_record(ACCEL_REC);

/* Write MCA record */
	put_real(spectra->kevch);
	put_real(spectra->kev0);
	put_real(spectra->first);
	put_real(spectra->fwhm);
	put_real(spectra->tau);
	write_record(MCA_REC);

/* Write RBS record */
	put_int32((INT32) spectra->geom);
	put_real(spectra->theta);
	put_real(spectra->phi);
	put_real(spectra->psi);
	put_real(spectra->omega);
	switch (spectra->type) {
		case RBS:
			write_record(RBS_REC); break;
		case FRES:
			write_record(FRES_REC); break;
		case PIXE:
			write_record(PIXE_REC); break;
		case NUCLEAR:						/* nuclear reaction record			*/
			write_record(RBS_REC); break;
		case OTHER:							/* Don't know how to handle		*/
			break;
	}			

/* Write CORR record */
	put_real(spectra->corr);
	write_record(CORR_REC);

/* Write the DATA */
	wr_data(spectra->npt, spectra->nspectra, spectra->counts);

/* Free temporary space */
	free(buffer);                   /* Free temporary space */

#if (defined OS2 && !defined NO_EA_MODE)
	EA_SetAscii(spectra->filename, ".TYPE", "RBS Spectrum");
#endif

	return(0);
}

/* ===========================================================================
-- Function to read the next record off disk into rump_record
--
-- Usage:  int = read_record(int *item_cnt, int ShowErrors)
--
-- Inputs: ShowErrors - if TRUE, print out error messages indicating fault 
--
-- Output: item_cnt - If not NULL, number of 4-byte data items in record
--
-- Returns: read_record - +n ==> Record type N
--                        -1 ==> Bad byte count
--                        -2 ==> Bad checksum
--
-- Action:  Reads next record from open disk file into buffer.
=========================================================================== */
static int read_record(int *item_cnt, int ShowErrors) {

	unsigned long CheckSum=0;								/* Check sum counter    */
	size_t NumRead, NumWant;								/* Number read, want    */
	int RecordLength, RecordType;							/* Record length (read) */
	int rcode=0;												/* Return code				*/

/* Read record length and type */
	if ( (NumRead = fread(buffer, 4, 2, handle)) != 2) {
		if (ShowErrors) ERRprintf("read_record: Failed to read the first two elements (8 bytes) of the record\n");
		return(-1);
	}
	bufptr  = buffer;											/* Set pointer for get_xxxx */
	RecordLength = (int) get_int32();						/* First element is length  */
	RecordType   = (int) get_int32();						/* Followed by record type  */
	CheckSum     = MakeCheckSum(2);						/* And start the check sum  */

/* Record length must be between 3 and 1027 to be valid */
	if ( (RecordLength < 3) || (RecordLength > 1027)) {
		if (ShowErrors) ERRprintf("read_record: Record length (%d) invalid [3,1027].  May be old RBS format.\n", RecordLength);
		return(-2);
	}

	NumWant = RecordLength-2;								/* # elements left to read  */
	if ( (NumRead = fread(buffer, 4, NumWant, handle)) != NumWant) {
		if (ShowErrors) ERRprintf("read_record: NumRead (%d) mismatch with NumWant (%d)\n", NumRead, NumWant);
		rcode = -3;
	}
	if (rcode == 0) {
		CheckSum = (CheckSum + MakeCheckSum((int) NumWant)) & 0xFFFFFFFFL ;
		if (CheckSum != 0) {
			if (ShowErrors) ERRprintf("read_record: Checksum mismatch (%x)\n", CheckSum);
			rcode = -4;
		}
	}

	bufptr = buffer;													/* Set pointer for get_xxxx */
	if (rcode == 0) rcode = RecordType;							/* No errors, return type   */
	if (item_cnt != NULL) *item_cnt = (int) NumRead-1;		/* If user wants #, don't count the checksum */

#ifdef DEBUG_MODE
	TTYprintf("read_record: type=%s  size=%d\n", gettype(rcode), NumRead-1);
#endif

	return(rcode);
}

/* ===========================================================================
-- Function to retrieve packed string buffer
--
-- Usage:  void get_str(char *result, int len)
--
-- Inputs: bufptr      - pointer to next character in r/w buffer
--
-- Output: bufptr      - pointer to next character in r/w buffer
--         get_int32    - unpacked long integer
=========================================================================== */
static void get_str(char *result, size_t maxlen) {

	size_t i,j;

	i = (size_t) get_int32();					/* Read string length */
	j = (size_t) min(i, maxlen-1);
	memcpy(result, bufptr, j);
	result[j] = '\0';
	bufptr += (i+3)/4;							/* Skip over elements */
	return;
}

/* ===========================================================================
--  Function to retrieve next packed long integer from buffer
--
--  Usage:  long = get_int32()
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_int32     - unpacked long integer
=========================================================================== */
static INT32 get_int32(void) {

	register INT32 val;

	val = (((long) bufptr[0]) << 24) | (((long) bufptr[1]) << 16) |
			(((long) bufptr[2]) <<  8) |  ((long) bufptr[3]) ;
	bufptr += 4;
	return (val);
}

/* ===========================================================================
--  Function to convert packed short integer in buffer to 2 byte int
--
--  Usage:  short = get_int16(unsigned char buffer[], int *ptr)
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_int16     - unpacked short integer
=========================================================================== */
static INT16 get_int16(void) {

	register INT16 val;

	val = (((INT16) bufptr[0]) << 8) | bufptr[1];
	bufptr += 2;
	return (val);
}

/* ===========================================================================
--  Function to convert packed IEEE 4 byte real number in the read
--  buffer into the default REAL representation of the machine
--
--  Usage:  REAL = get_real()
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_real     - unpacked real number
=========================================================================== */
static REAL get_real(void) {

	union {
		REAL32 result;
		UINT32 val;
	} tmp;

	tmp.val = get_int32();

#if defined(CONVERT_IEEE)
	tmp.result = ieee_to_float(tmp.val);
#endif

	return (tmp.result);
}

/* ===========================================================================
--  Function to read sequence of data records
--
--  Usage:  int = read_data_records(REAL data[], int npt, int compress, char **errmsg);
--
--  Inputs: data      - buffer to store read data (better be big enough!)
--          npt       - number of data points we will eventually read
--          compress  - default compression method
--
--  Output: *errmsg   - pointer to error message if problems.  Will be
--                      set to NULL if the error has already been printed to screen.
--
--  Returns:  0 if successful
--           -1 on errors
=========================================================================== */
static int read_data_records(REAL *counts, int npt, int compress, char **errmsg) {

	int i, item_cnt, npt_read, record_type;

	npt_read = 0;
	while (npt_read != npt) {
		if ((record_type = read_record(&item_cnt, TRUE)) < 0) {
			if (errmsg != NULL) *errmsg = NULL;								/* Error already identified */
			return(-1);
		}

		if (record_type == DATA_REC) {		/* Convert to specific */
			if      (compress == 0)	record_type = DATA_REC_0;
			else if (compress == 1) record_type = DATA_REC_1;
			else if (compress == 2) record_type = DATA_REC_2;
			else if (compress == 3) record_type = DATA_REC_3;
		}

		switch (record_type) {
			case DATA_REC_0:
				for (i=min(1024,npt-npt_read) ; i ; i--) 
					counts[npt_read++] = get_real();
				break;
			case DATA_REC_1:
				for (i=min(1024,npt-npt_read) ; i ; i--) 
					counts[npt_read++] = (REAL) get_int32();
				break;
			case DATA_REC_2:
			case DATA_REC_3:
				if (record_type == DATA_REC_3) UnZeroCompress(bufptr, 4*item_cnt);
				if (read_compress(counts, npt, &npt_read)) {
					if (errmsg != NULL) *errmsg = "Bad data compression\n";
					return(-1);
				}
				break;
			default:
				if (errmsg != NULL) *errmsg = "Holy shit!  Record was not data as expected\n";
				return(-1);
		}
	}
	return(0);
}

/* ===========================================================================
--  Function to uncompress "differential" integral format
--
--  Usage:  int = read_compress(REAL data[], int npt, int *npt_read)
--
--  Inputs: npt       - number of data points we will eventually read
--          *npt_read - number of data points read so far
--
--  Output: data[]    - data buffer with uncompressed real data points
--          bufptr    - pointer to next character in r/w buffer
--          read_compress  - success (0 ==> okay)
=========================================================================== */
static int read_compress(REAL counts[], int npt, int *npt_read) {
        
	signed char  byte;
	signed short word;
	signed long  last;
	int i=1;

	last  = get_int32();										/* Read first value */
	counts[(*npt_read)++] = (REAL) last;

	while ( (*npt_read < npt) && (i++ != 1024) ) {
		byte = *(bufptr++);
		if (((unsigned char) byte) != 0x80) {        /* Avoid sign error */
			last += byte;										/* problems in test */
		} else {
			word = get_int16();
			if (((unsigned short) word) != 0x8000) {  /* Avoid sign error */
				last += word;	                        /* problems in test */
			} else {
				last = get_int32();
			}
		}
		counts[(*npt_read)++] = (REAL) last;
	}
	return(0);
}

/* ===========================================================================
-- Function to write a string record to disk
--
--  Usage:  void wr_str(int code,char *string)
--
--  Inputs: code     - What type of string record
--          *string  - Character string to output
--
--  Output: Outputs appropriate string record
=========================================================================== */
static void wr_str(int code, char *string) {

	put_string(string,strlen(string));
	write_record(code);
	return;
}

/* ===========================================================================
-- Routines to handle zero compression/uncompression of byte stream (after
-- delta compression).  Multiple zeros in stream are compressed to a pair
-- marker <FLAG><count>.  <count> can be 0x01-0xFF.  The special case
-- <FLAG><00> indicates to literally include <FLAG> in the stream.
--
-- Compression is indicated in the stream by an initial byte of 0x80 followed
-- by the <FLAG> byte.  Default is for the <FLAG> to be 0x81 since -127 should
-- not be a common delta.  
--
--  Usage:  int TryZeroCompress(void *data, int length);
--          int UnZeroCOmpress(void *data, int length);
--
--  Inputs: data   - pointer to the current data (will be overwritten)
--          length - number of bytes in the stream initially
--
--  Output: data  - replaced with compressed stream if fewer (compress)
--                  or replaced by the expanded stream (uncompress).
--
--  Returns: New number of bytes in the stream.
=========================================================================== */
#define	ZERO_COMPRESS_FLAG	0x80			/* Flag indicating compress data */
#define	DEFAULT_REPEAT_BYTE	0x81			/* Byte indicating repeat			*/
#define	MAX_REPEAT_COUNT		0xFF			/* Maximum # of 00 to compress	*/

static int UnZeroCompress(void *data, int length) {

	unsigned char achr, repeat_byte, *iptr, *optr, tmp[BUFFER_SPACE_NEEDED];
	int len;
	
	iptr = data;									/* Local copies */
	len  = length;

	if (*iptr == ZERO_COMPRESS_FLAG) {		/* Flag indicating zero compressed */
		optr = tmp;

		repeat_byte = iptr[1];					/* Second byte is repeat marker */
		iptr += 2; len -= 2;						/* First 2 have been used */

		while (len > 0) {							/* Walk through all elements */
			achr = *iptr++; len--;
			if (achr != repeat_byte || len == 0) {
				*optr++ = achr;
			} else {
				achr = *iptr++; len--;
				if (achr == 0x00) {				/* Stream really had the byte */
					*optr++ = repeat_byte;
				} else {
					while (achr--) *optr++ = 0;
				}
			}
		}
		length = (int) (optr-tmp);				/* New stream length */
		memcpy(data, tmp, length);
	}

	return(length);
}

static int TryZeroCompress(void *data, int length) {

	int i,numzero;
	unsigned char *optr, *iptr, achr, tmp[BUFFER_WRITE_NEEDED];

/* Go ahead and assume compression will work */
	iptr = data;
	optr = tmp;
	memset(tmp, sizeof(tmp), 0);				/* Zero fill so no strange bytes */
	*optr++ = ZERO_COMPRESS_FLAG;				/* Flag to indicate compression */
	*optr++ = DEFAULT_REPEAT_BYTE;			/* And indicate my repeat byte  */

	for (numzero=0,i=0; i<length; i++) {
		achr = *iptr++;
		if (achr != 0) {							/* Non-zero, just store */
			if (numzero > 0) {
				if (numzero < 3) {
					while (numzero--) *optr++ = 0;
				} else {
					*optr++ = DEFAULT_REPEAT_BYTE;
					*optr++ = numzero;
				}
				numzero = 0;
			}
			*optr++ = achr;
			if (achr == DEFAULT_REPEAT_BYTE) *optr++ = 00;
		} else {
			if (++numzero == MAX_REPEAT_COUNT) {
				*optr++ = DEFAULT_REPEAT_BYTE;
				*optr++ = MAX_REPEAT_COUNT;
				numzero = 0;
			}
		}
		if (optr-tmp > length) goto ExitNow;
	}

/* Fill in any pending zeros, and blank pad buffer */
	if (numzero > 0) {
		if (numzero < 3) {
			while (numzero--) *optr++ = 0;
		} else {
			*optr++ = DEFAULT_REPEAT_BYTE;
			*optr++ = numzero;
		}
	}

ExitNow:
	iptr = data;												/* Restore this to check */
	if (optr-tmp < length) {								/* And now decide */
		length = (int) (optr-tmp);
		memcpy(data, tmp, length);
	} else if (*iptr == ZERO_COMPRESS_FLAG) {			/* Flag/data conflict? */
		(*iptr)++;												/* Avoid read crash!   */
		ERRprintf("ERROR: 1 in a 100,000,000 chance problem.  Data will not reread correctly\n");
	}
	return(length);
}


/* ===========================================================================
--  Function to write the data records to disk
--
--  Usage:  void wr_data(int npt, int nspectra, REAL counts[])
--
--  Inputs: npt      - Number of data points to output              (#)
--          nspectra - number of spectra to write                   (#)
--          counts[] - data set                                    (cnt)
--
--  Output: Outputs DATA_INITIALIZE and DATA_RECS for data set
=========================================================================== */
#define COMPRESS_MAX  1.0E9f				/* Output full long integer format */
													/* if any value larger than this   */
#define MAX_BYTE_SAVE 127					/* Max value stored as byte offset */
#define MAX_WORD_SAVE 32767				/* Max value stored as word offset */

static void wr_data(int npoint, int nspectra, REAL *counts) { 

	int npt, i,compress,sum, istart,override;		/* Random loop counter */
	INT32 current,last=0,delta;

	npt = npoint * nspectra;
	compress = (minor_rev_level >= 1) ? 3 : 2 ;	/* Assume compression level */

	for (i=0; i<npt; i++) {
		if ( fabs(counts[i]) > COMPRESS_MAX ) {
			compress = 0;
			break;
		}
		current = (INT32) counts[i];
		if ((REAL) current != counts[i]) {
			compress = 0;
			break;
		}
		if (i%1024 == 0) {					/* Starting new block? */
			sum = 4;								/* 1st takes 4 bytes   */
		} else {
			delta = current-last;
			if (delta < 0) delta = -delta;
			if (delta <= MAX_BYTE_SAVE) {
				sum += 1;
			} else if (delta <= MAX_WORD_SAVE) {
				sum += 3;
			} else {
				sum += 7;
			}
			if (sum >= 4096 && compress == 2) compress = 1;
		}
		last = current;
	}

#ifdef DEBUG_MODE
	TTYprintf("write_data: compress=%d  npt=%d\n", compress, npt);
#endif

	if (nspectra == 1 || minor_rev_level < 1) {
		put_int32(compress);							/* Compression style   */
		put_int32(npt);
		write_record(DATA_INIT_REC);
	} else {
		put_int32(compress);							/* Compression style */
		put_int32(npoint);								/* Number of points */
		put_int32(nspectra);							/* Number of spectra */
		write_record(ARRAY_INIT_REC);
	}
        
	if (compress == 0 || compress == 1) {	/* Just simple float format */
		for (i=0; i<npt; i++) {
			if (compress == 0) {
				put_real(counts[i]);
			} else {
				put_int32((INT32) counts[i]);
			}
			if (i%1024 == 1023) write_record(DATA_REC);
		}
		if (i%1024 != 0) write_record(DATA_REC);
	} else {
		for (i=0; i<npt; i++) {
			if (i%1024 == 0) {					/* New record block */
				put_int32((INT32) counts[i]);
				sum      = 4;						/* And reset the values */
				istart   = i;
				override = FALSE;
			} else if (override) {				/* Using override block */
				put_int32((INT32) counts[i]);
			} else {
				delta = (INT32) (counts[i]-last);
				if (labs(delta) <= MAX_BYTE_SAVE) {
					put_byte((char) delta);
					sum += 1;
				} else if (labs(delta) <= MAX_WORD_SAVE) {
					put_byte(0x80);
					put_int16((INT16) delta); 
					sum += 3;
				} else {  
					put_byte(0x80);
					put_int16(0x8000);
					put_int32((INT32) counts[i]);
					sum += 7;
				}
				if (sum >= 4096 && compress == 3) {
					bufptr = buffer+8;
					while (istart <= i) put_int32((INT32) counts[istart++]);
					override = TRUE;
				}
			}
			last = (UINT32) counts[i];
			if (i%1024 == 1023) {
				if (compress == 3 && ! override)
					bufptr = buffer+8 + TryZeroCompress(buffer+8, (int) (bufptr-(buffer+8)));
				write_record(override ? DATA_REC_1 : DATA_REC);
			}
		}
		if (i%1024 != 0) {
			if (compress == 3 && ! override)
				bufptr = buffer+8 + TryZeroCompress(buffer+8, (int) (bufptr-(buffer+8)));
			write_record(override ? DATA_REC_1 : DATA_REC);
		}
	}
	return;
}



/* ===========================================================================
--  Function to place an integer into the output buffer
--
--  Usage: void put_byte(unsigned char value)
--
--  Inputs: value - Integer number to packed into building record
--
--  Output: bufptr - pointer to next character in r/w buffer
=========================================================================== */
static void put_byte(unsigned char value) {

	*(bufptr++) = value;
	return;
}

/* ===========================================================================
--  Function to place an integer into the output buffer
--
--  Usage:  void put_int16(UINT16 value)
--
--  Inputs: value - Integer number to packed into building record
--
--  Output: bufptr - pointer to next character in r/w buffer
=========================================================================== */
static void put_int16(UINT16 value) {

	*(bufptr++) = (unsigned char) ((value>>8) & 0xFF);
	*(bufptr++) = (unsigned char) ( value     & 0xFF);
	return; 
}

/* ===========================================================================
--  Function to place an integer into the output buffer
--
--  Usage:  void put_int32(UINT32 value)
--
--  Inputs: value - Integer number to packed into building record
--
--  Output: bufptr - pointer to next character in r/w buffer
=========================================================================== */
static void put_int32(UINT32 value) {

	*(bufptr++) = (unsigned char) ((value>>24) & 0xFF);
	*(bufptr++) = (unsigned char) ((value>>16) & 0xFF);
	*(bufptr++) = (unsigned char) ((value>> 8) & 0xFF);
	*(bufptr++) = (unsigned char) ( value      & 0xFF);
	return; 
}


/* ===========================================================================
--  Function to place a real number into the output buffer with
--  IEEE format.
--
--  Usage:  void put_real(REAL value)
--
--  Inputs: value - Real number to packed as IEEE into  record
--
--  Output: bufptr - pointer to next character in r/w buffer
=========================================================================== */
static void put_real(REAL value) {

	union {
		REAL32 real;
		UINT32 dummy;
	} tmp;

#if defined(CONVERT_IEEE)
	tmp.dummy = float_to_ieee(value);
#else
	tmp.real = (REAL32) value;
#endif

	put_int32(tmp.dummy);
	return;
}

/* ===========================================================================
--  Function to pack string into output buffer
--
--  Usage:  void put_string(char *string, int len)
--
--  Inputs: *string - string to copy into buffer
--          len     - number of valid characters
--          bufptr - pointer to next character in r/w buffer
--
--  Output: bufptr - pointer to next character in r/w buffer
=========================================================================== */
static void put_string(char *string, size_t len) {

	put_int32((int) len);						/* Output length of string  */
	memcpy(bufptr,string,len);
	bufptr += 4*((len+3)/4);				/* Pointer to 32 bit word boundary */
	return;
}


/* ===========================================================================
--  Function to write the next record into the disk file
--
--  Usage:  void write_record(int record_type)
--
--  Inputs: record_type - Record Type to write (see record.def)
--
--  Output: Fills in the remainder of the buffer structure with
--          record type, length and finally checksum.
--          Outputs it to open disk file.
=========================================================================== */
static void write_record(int RecordType) {

	INT32     CheckSum;
	size_t    RecordLength;
	ptrdiff_t BytesToWrite;

/*----------------------------------------------------------------------------
   1. Round off pointer to next even 4 byte boundary
   2. # of records = pointer/4 (next write position) + 1 for checksum
   3. Reset point to 00 and output the record length and type to buffer
   4. Make the check sum of buffer to here, negate and output checksum
----------------------------------------------------------------------------*/
	BytesToWrite = (bufptr-buffer);				/* Number bytes written now */
	RecordLength = 1+ (BytesToWrite+3)/4;		/* Add space for checksum   */
	bufptr  = buffer;									/* Write record type/length */
	put_int32((int) RecordLength);					/* Length of record!        */
	put_int32(RecordType);							/* Record type              */
	bufptr = buffer + (4*RecordLength-4);		/* Position to checksum     */
	CheckSum = - ((INT32) MakeCheckSum((int) RecordLength-1));
	put_int32(CheckSum);

/*----------------------------------------------------------------------------
   1. Output record to disk w/ error message if appropriate
   2. Reset pointer to 8 leaving space type record length and type next time
----------------------------------------------------------------------------*/
	if (fwrite(buffer, 4, RecordLength, handle) != RecordLength)
		ERRprintf("ERROR: Bad write to disk file\n");
#ifdef DEBUG_MODE
	TTYprintf("write_record: type=%s  length=%d\n", gettype(RecordType), RecordLength);
#endif

	bufptr  = buffer + 8;							/* Point to start of data */
	return;												/* for subsequent writes  */
}

/* ===========================================================================
--  Function to generate the checksum of a given number of elements
--  from the buffer area.
--
--  Usage:  UINT32 = MakeCheckSum(int num_elem)
--
--  Inputs: num_elem - number of elements to sum at this time
--
--  Output: check sum
=========================================================================== */
static UINT32 MakeCheckSum(int num_elem) {

	register unsigned char *ptr;
	UINT32 result=0;

	ptr = buffer;
	while (num_elem--) {
		result += (((unsigned long) ptr[0]) << 24) | (((unsigned long) ptr[1]) << 16) |
					 (((unsigned long) ptr[2]) <<  8) |  ((unsigned long) ptr[3]) ;
		ptr += 4;
	}
	return(result);
}

/* ===========================================================================
--  Routine to check if number representations in this version of C are
--  correct.  Will generate error message on either of the following:
--     1. sizeof(UINT32) != sizeof(REAL32)
--     2. (REAL32) 93.375 not IEEE format (should be exact representation)
--
--  Usage:  int check_c_reals()
--
--  Inputs: none
--
--  Output: check_c_reals 0 ==> all okay
--                        1 ==> size of (long != float) or (byte != 1)
--                        2 ==> not IEEE format
--
--  Note: If CONVERT_IEEE is defined, the conversion routines will be
--        exercised as well.  Error is type 2 for these as well.
=========================================================================== */
#define CHK_NUMBER_REAL 93.375f                 /* Equivalent value of real */
#define CHK_NUMBER_HEX  0x42BAC000L             /* and long in IEEE format  */

static int check_c_reals(void) {
	union {													/* Equivalence the code */
		UINT32 dummy_long;
		REAL32 dummy_float;
	} tmp;
        
	if ((sizeof(REAL32) != sizeof(UINT32)) || (sizeof(char) != 1)) {
		ERRprintf("WARNING: sizeof(REAL32) != sizeof(UINT32) or sizeof(char) != 1\n");
		return(1);
	}

#if defined(CONVERT_IEEE)
	if ((float_to_ieee(CHK_NUMBER_REAL) != CHK_NUMBER_HEX) ||
		(ieee_to_float(CHK_NUMBER_HEX)  != CHK_NUMBER_REAL)) {
		ERRprintf("WARNING: Conversion routines for internal (float) to IEEE fail\n");
		return(2);
	}
#else
	tmp.dummy_float = CHK_NUMBER_REAL;              /* Exact value */
	if (tmp.dummy_long != CHK_NUMBER_HEX) {
		ERRprintf("WARNING: (REAL32) is not IEEE format.  Internal value %8.8lX vs %8.8lX\n",
			tmp.dummy_long,CHK_NUMBER_HEX);
		return(2);
	}
#endif

	return(0);
}

#if defined(CONVERT_IEEE)

/* ===========================================================================
--  Function to convert a "long" 4 byte IEEE format real number into
--  the internal "REAL32" representation.  Bit swapping and ordering is
--  your problem!
--
--  Usage:  REAL32 = ieee_to_float(long ieee_dummy)
--
--  Inputs: ieee_dummy - 4 byte IEEE real number packed in long
--
--  Output: ieee_to_float - internal representaion of same #
=========================================================================== */
static REAL32 ieee_to_float(UINT32 value) {
	union {
		REAL32 dummy_real;
		UINT32 dummy_long;
	} tmp;
	tmp.dummy_long = value;
	return (tmp.dummy_real);
}

/* ===========================================================================
--  Function to convert a "REAL32" internal format real number into the
--  equivalent IEEE real number packed into a 4 byte long integer
--  Functional inverse of routine above.
--
--  Usage:  unsigned long = float_to_ieee(REAL32 dummy)
--
--  Inputs: dummy - real number in internal representation
--
--  Output: float_to_ieee - IEEE representation of real number
=========================================================================== */
static UINT32 float_to_ieee(REAL32 value) {
	union {
		REAL32 dummy_real;
		UINT32 dummy_long;
	} tmp;
	tmp.dummy_real = value;
	return (tmp.dummy_long);
}

#endif
