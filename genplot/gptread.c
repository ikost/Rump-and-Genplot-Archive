/* GptRead.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* #define	NO_PIPE_MODE	*/		/* Disable pipe mode read/writes				*/
/* #define	NO_EA_MODE		*/		/* Disable extended attributes				*/

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if defined MSC70
	#include <windows.h>								/* Standard windows headers */
	#include <vfw.h>									/* Need Video-for-Windows headers */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"				/* For GPTUserRead and Write */

#if (defined OS2 && !defined NO_EA_MODE)	/* For setting extended attributes */
	#include <ea.h>
#endif

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef NO_PIPE_MODE								/* If pipes disabled, do by def's */
	#define	popen(filename,mode)	NULL
	#define	pclose(handle)
#endif

#if (defined OS2 || defined NT)
	#define	PROMPTFORTEXT	"Enter comment lines.  End with EOF (^Z) or string @END\n"
	#define	PROMPTFORDATA	"Enter data points. End with EOF (^Z) or string @END\n"
	#define	CONSOLE_FILE_NAME		"CON:"
#else
	#define	PROMPTFORTEXT	"Enter lines of text.  End with EOF (^D) or string @END\n"
	#define	PROMPTFORDATA	"Enter data points. End with EOF (^D) or string @END\n"
	#define	CONSOLE_FILE_NAME		"/dev/tty"
#endif

#define	LINEBUFSIZE		1024

/* Parameters for GptReadMatrix */
#define	MAXWARN					20					/* # warnings before become quiet */
#define	MAXERRORS				1000				/* # errors before it read aborts */

static int MaxReadWarn   = MAXWARN;
static int MaxReadErrors = MAXERRORS;
static BOOL VarsLinked   = FALSE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* Routines used to help with file reads */
static void TrimWhiteSpace(char *buf);

/* Routines for dealing with RMFORT record types */
static BOOL RMBinaryMode(FILE *funit);
static BOOL RMCheckFormat80(UINT32 reclen);
static int  RMReadRecord(void *buf, size_t size, size_t count);
static int  RMReadAltRecords(void *buf1, size_t size1, size_t count1, void *buf2, size_t size2, size_t count2);
static BOOL RMReadText(char *token);
static BOOL RMReadScale(REAL *, REAL *, REAL *, REAL *);
static BOOL RMRead2Scale(REAL *, REAL *);
static BOOL RMReadInt(INT *npt);
static BOOL RMRead2Int(INT *npt, INT *ncol);
static BOOL RMReadArray(REAL *x, int len);
static BOOL RMDumpReals(int len);
static BOOL RMWriteText(char *token);
static BOOL RMWriteShorts(int npt, int ncol);
static BOOL RMWriteLongs(int npt, int ncol);
static BOOL RMWriteArray(REAL *x, int len);
static BOOL RMWriteRecord(void *buf, size_t size, size_t count);
static void RMReverse(void *buffer, size_t width, size_t num);

/* Routines for digitizing data */
PRIVATE int DigitizeCurve(char *UseCurve);
PRIVATE int CursorizeCurve(char *UseCurve);
PRIVATE BOOL GetPoint(int key, REAL *x, REAL *y, int *cpar);
PRIVATE BOOL ScanTime(char *token, double *value);

SURFACE *GptMatrixRead(FILE *funit, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int flags);
SURFACE *GptCameraRead(FILE *funit, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int flags);
SURFACE *GptBitmapRead(FILE *funit, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int flags);
SURFACE *GptAVIRead(char *FileName, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int flags, int iframe);
SURFACE *GptFLIRRead(char *FileName, FILE *funit, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int flags, int iframe);
SURFACE *GptAFMRead(char *fname, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int rd_flags, int iframe);

int GptMatrixWrite(FILE *funit, SURFACE *surface, BOOL silent);
int GptBitmapWrite(FILE *funit, SURFACE *surface, BOOL silent);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/*============================================================================
--     Function to read data files for genplt
--
--     Usage: int GptRead(char *PassedCurve);
--
--     Inputs: PassedCurve - name of the curve presently linked as GptCurve
--
--     Output: Fills various curve/array structures as requested
--
--     Unversioned:
--         Imbedded header, ASCII pairs
--     VERSION 0.0: Formatted, minor controlled ASCII
--         Header text
--         <cntr>Z
--         ASCII X,Y pairs
--     VERSION 1.1: Formatted, controlled ASCII
--         VERSION 1.1
--         Identifier text
--         <cntrl>Z
--         Additional parameters
--         <cntrl>Z
--         XFACT,XOFF,YFACT,YOFF
--         NPT
--         data - Xdata, Ydata ASCII pairs
--		 VERSION 3.1: 
--         All headers and data in RMFORT unformatted records
--		 VERSION 4.0:
--         All headers/data in RMFORT unformatted records
--
--     XPLOT = Xread*XFACT + XOFF
--     YPLOT = Yread*YFACT + YOFF
--
-- ... Feb. 1986 - MOT
--     Changed the ASCII read to parse into tokens and let RDARG interpret
--
-- ... March 1987 - MOT
--     New version 3.1, which is unformatted completely.
--
-- ... Sep 1986 - MOT
--     Added ability to specify -COLUMN n1 n2  to use multiple column data
--     Only works on the ASCII version of data
--
-- ... Oct 1987 - MOT
--     Created binary version 4.0 ("VERSION GENPLT 4.0") for storage of data
--     in binary records.  Generalized to allow for multiple columns of data
--     stored in a single file.
--
-- ... Nov. 1987 - MOT
--     Added code to handle a -LIST option which specifies arrays and columns
--     for multiple reads.  READ <filename> -LIST xdata 1 ydata 2 ydata_new 3
--
-- ... Apr. 1988 - MOT
--     Added digitize option
--
-- ... Apr. 1989 - MOT
--     Added better handling of ASCII/BINARY/UNKNOWN modes
-- 
-- ... Convex puts record length as first few bytes.  It reads length
-- ... then allocates a buffer of appropriate size.  Non-binary files
-- ... may request huge buffers so check for this first.
--
-- ... Sep. 2013 - MOT
--     Modified so can do -append read of -LIST components
============================================================================ */
PRIVATE	FILE	*FileHandle=NULL;				/* Handle for the open file		*/

#define	CLZ			0x01						/* End of record marker				*/
#define	NVAR			128						/* Maximum # of reads allowed		*/
#define	UPDATE_SCREEN_COUNT	100			/* Numbers going by on ASCII rd	*/

/* RD_Flags definitions */
#define	RD_SILENT			0x01				/* Completely silent operation	*/
#define	RD_DEBUG				0x02				/* Debug mode requested				*/
#define	RD_NOWARN			0x04				/* No warning mode					*/
#define	RD_RED_ONLY			0x100				/* Extract red element only (bitmaps)		*/
#define	RD_GREEN_ONLY		0x200				/* Extract green element only (bitmaps)	*/
#define	RD_BLUE_ONLY		0x400				/* Extract blue element only (bitmaps)		*/
#define	RD_CHROMA_ONLY		0x800				/* Extract chroma (4th) element only (bitmaps)	*/

static char ReadHelp[]=
"\n"
" Primary command to read user data files - either in ASCII format or binary.\n"
"\n"
"   READ <filename> [-options]\n"
"   READ -surface <name> <filename> [-opts]   Read a 3D surface\n"
"        -3D      <name> <filename> [-opts]   Read a 3D surface\n"
"   READ %digitize [-curve -append            Read from digitizing tablet\n"
"                   -hold -tablet -bell -nobell]\n"
"   READ %cursor   [-curve -append]           Digitize from on-screen cursor\n"
"\n"
" Options:\n"
"   -help | -?           Prints this help message\n"
"   -dummy               No read - but initializes internal variables\n"
"   -debug               Sometimes debugging info\n"
"\n"
"   -name <str_expr>     Specify name, allowing string expressions (not default)\n"
"   -file <str_expr>     synonymous with -name\n"
"   -ascii               Force read as 2D/3D ASCII mode (default is auto-detect)\n"
"   -csv                 Force read as 2D/3D ASCII mode (identical to -ASCII)\n"
"   -binary              Force read as 2D/3D BINARY mode\n"							  
"   -append              Append data to existing curve or list of arrays\n"
"   -list <var> <col> <var> <col> /\n"
"                        Reads into a list of array variable arbitrary columns\n"
"                        Variables may also be string arrays storing text\n"
"\n"
"   -curve <name>        Read into specified curve (created if necessary)\n"
"   -surface <surf>      Read to create a 3D surface matrix structure\n"
"   -matrix <surf>       Synonymous with -surface\n"
"   -bitmap <surf>       Handle as a Windows bitmap file, returning a surface\n"
"   -BMP <surf>          Handle as a Windows bitmap file, returning a surface\n"
"   -AVI <surf>          Handle as a frame from an AVI file (accepts -FRAME)\n"
"   -FLIR <surf>         Handle as a FLIR camera image (may accept -FRAME)\n"
"   -AFM <surf>          Read the first image from an AFM data file\n"
"\n"
"   -rows <start> <end>  Read only lines <start> through <end> (first = 1)\n"
"   -lines <start> <end> Read only lines <start> through <end> (first = 1)\n"
"   -begin <text>        Skip all lines until <text> found at start\n"
"   -end <text>          Quit on first line with <text> at beginning\n"
"   -cols <x> <y> [<z>]  Specify data columns for X,Y,Z entries.  0 is index\n"
"   -cols <start> <end>  For -SURFACE, limits columns read\n"
"\n"
"   -frame <iframe>      Which frame from a sequence (AVI, AMF and FLIR modes)\n"
"   -blue                Extract the first (blue) element from a bitmap\n"
"   -green               Extract the second (green) element from a bitmap\n"
"   -red                 Extract the third (red) element from a bitmap\n"
"   -chroma              Extract the fourth (reserved) element from a bitmap\n"
"\n"
"   -user                Read using alternate loaded USER module\n"
"\n"
"   -silent | -quiet     Don't print any statistics about curve read\n"
"   -expressions         Force ASCII read to accept expressions (default is auto-detect)\n"
"   -strict              Force ASCII to strictly number mode - ignore all extensions\n"
"   -delimiters \"list\"   List of column delimiters.  Use \"\\t\" for tab\n"
"   -nowarning           Don't print warnings during read\n"
"\n"
" Filenames may be specified as piped, compressed, or inline format.  gzip'd\n"
" files (extension .gz) are automatically recognized and read as via the pipe.\n"
" The .dat extension is assumed, but not required.\n"
"     <<eof             Reads current stream (macro file) until text 'eof' seen\n"
"     <<                From console, reads until @end or @eof entered.\n"
"     | gzip -dc file   Takes input from the specified pipe function\n"
"     lots::file1       file1 read from zip archive lots.zip\n"
"\n"
" Examples: read simple.dat\n"
"           read simple.dat -curve c1 -col 1 3\n"
"           read simple.dat -surface s1 -rows 1 24 -cols 2 84\n"
"           read %digitize\n";

typedef enum {						/* File type */
	UNSPECIFIED,					/* Hasn't been determined yet (normally no name given yet) */
	SIMPLEFILE,						/* Simple file opened as fopen/fclose */
	CONSOLE,							/* Input from console requested */
	STREAM,							/* Stream from existing file (for macros) */
	PIPE,								/* Pipe from command */
	DIGITIZE,						/* Digitize command (handle immediately) */
	CURSOR_DIGITIZE,				/* Cursor digitize */
	WAS_ERROR						/* File type or name invalid, only possible return is error */
} FILETYPE;
typedef enum {						/* File mode */
	UNKNWN,							/* Type currently unknown */
	BINARY,							/* GENPLOT binary format */
	ASCII,							/* Normal ASCII text format */
	AFM_IMAGE,						/* AFM image file */
	BIT_MAP,							/* Windows BITMAP format */
	AVI_MAP,							/* Windows AVI format */
	CAMERA,							/* Some strange format required once */
	FLIR								/* FLIR (IR) camera */
} FILEMODE;

static FILETYPE Determine_FileType_from_Name(char *FileName, char *ids) {

	FILETYPE FileType;
	char tmpbuf[PATH_MAX];									/* Large temporary buffer */
	char *aptr;

	FileType = SIMPLEFILE;									/* Assume a simple file */
	if (LexEqual(FileName, "CON:", 3) ||				/* Special console read */
		 (stricmp(FileName, "tty")      == 0) ||		/* Possible names on UNIX */
		 (stricmp(FileName, "/dev/tty") == 0) ) {		/* Possible names on UNIX */
		FileType = CONSOLE;
		strcpy(FileName, CONSOLE_FILE_NAME);
	} else if (strncmp(FileName, "<<", 2) == 0) {	/* Special stream input */
		FileType = STREAM;
	} else if (*FileName == '|') {						/* Special pipe input */
		FileType = PIPE;
	} else if ( (aptr = strstr(FileName, "::")) != NULL) {
		if (*ids == '\0') sprintf(ids, "!%s", FileName);
		*aptr = '\0';											/* Rewrite as pipe */
		sprintf(tmpbuf, "| unzip -p -C %s %s", FileName, aptr+2);
		strcpy(FileName, tmpbuf);
		FileType = PIPE;
	} else if (LexEqual(FileName, "%digitize",4)) {	/* Simple digitizing */
		FileType = DIGITIZE;
	} else if (LexEqual(FileName, "%cursor", 4)) {	/* Simple via cursor */
		FileType = CURSOR_DIGITIZE;
	} else if (SysFindFileGz(FileName, FileName, GptSearchPath, GptSearchExts, R_OK)) {
		if (strlen(FileName) > 3 && stricmp(FileName+strlen(FileName)-3, ".gz") == 0) {
			if (*ids == '\0') sprintf(ids, "!%s", FileName);
			if (strchr(FileName, ' ') != NULL) {
				sprintf(tmpbuf, "| gzip -dc \"%s\"", FileName);
			} else {
				strcat(strcpy(tmpbuf, "| gzip -dc "), FileName);
			}
			strcpy(FileName, tmpbuf);
			FileType = PIPE;
		}
	} else {
		ERRprintf("ERROR: %s does not exist or cannot be accessed\n", FileName);
		FileType = WAS_ERROR;
	}

	return FileType;
}

int GptRead(char *PassedCurve) {

	char	UseCurve[VARNAME_STR_SIZE];		/* Adequate of space				*/
	char	FileName[PATH_MAX];					/* Filename							*/
	int	rcode=-1;								/* Default return code			*/
	int	ercd=0;									/* Internal error code			*/
	
	static char *inbuf=NULL;					/* Read buffer						*/
	static int inbufsize=0;						/* inbufsize						*/

	char	token[DFLT_STR_SIZE],				/* For random parsing			*/
			ids[DFLT_STR_SIZE],					/* For identifier string		*/
			tok80[80];								/* Must BE 80 for RMRead()		*/
	char	*aptr, *bptr;
	char	BeginText[DFLT_STR_SIZE], EndText[DFLT_STR_SIZE];
	enum {GPT_NOERROR, CANTSCAN, BADCHAR, BADEXPR} ertype;
	int	NumErrors;
	
	INT	ncol;										/* Data file num pts/cols		*/
	int	NumRead;									/* Number points read			*/
	int	NumValid;								/* Number of valid points		*/
	int	Iskip;									/* Number of lines to ignore	*/

	BOOL  do_append = FALSE;					/* Initially not append			*/
	int	nlist,									/* Number of columns valid		*/
			maxcol;									/* Number of columns needed	*/
	int   irow,										/* Row counter in ASCII files */
		   FirstRow=1, LastRow=INT_MAX,		/* Min/max rows to read			*/
			FirstCol=1, LastCol=INT_MAX;		/* Min/max cols to read			*/

	int   rd_flags = 0;							/* Read options flags			*/
	int	iframe = 1;								/* Frame or sequence item		*/

/* ... Location where read is to stuff answers ... */
	struct {
		int icol;									/* Column for this data			*/
		enum {IS_REAL, IS_STRING} type;		/* Type of read (string or real) */
		int maxsize;								/* Size of the array				*/
		REAL *x;										/* Where data will be stored	*/
		INT *size;									/* Where to store size			*/
		int next;									/* Which element next to write */
	} entry[NVAR];

	int	i,j,k, minnpt, maxnpt, iuse, vers;

	REAL	xtmp, *buf=NULL;						/* Buffers to read in data		*/
	void **varptr;
	ARRAY *array=NULL;							/* Temporary array pointer		*/
	SURFACE *surface=NULL;						/* Temporary array pointer		*/
	int   type;										/* Type of variable (GVLink)	*/
	REAL *z;											/* Temporary var for SURFACE	*/
	double timevalue;								/* For scanning time values	*/

	BOOL DoScaling = FALSE;						/* Scaling in ASCII reads	*/
	REAL xfact, xoff, yfact, yoff;			/* Scaling factors (maybe)	*/

	BOOL HaveRead;									/* Temporary variable		*/

	BOOL tellinfo=TRUE,							/* Tell info on exit			*/
		  debug=FALSE,								/* Work in debug mode?		*/
		  warn=TRUE,								/* Print warning messages	*/
		  options,									/* Do we allow options		*/
		  EchoFlag;

	enum {XYZ, LIST, SURF, USER} DataType;							/* Type read */
	FILEMODE FileMode;																/* File mode (Binary, ASCII, camera, etc.) */
	FILETYPE FileType;																/* Filetype (Simplefile, pipe, etc.) */
	enum {MAYBE_EXPR, HAS_EXPR, IS_STRICT} AsciiFormat;					/* Does Ascii allow more than just digits? */
	char AsciiDelims[20] = " \t,;\n\r";											/* Column delimiter list for parsing */

/* First, look for a help request */
	if (LexCheckHelp("Read", ReadHelp, NULL)) return(1);

/* ----------------------------------------
-- ... Check on the file now!
---------------------------------------- */
	strscpy(UseCurve, PassedCurve, sizeof(UseCurve));	/* Copy curve name	*/

/* Allocate buffer space if not already done */
	if (inbuf == NULL) {
		inbufsize = LINEBUFSIZE;					/* Initial default */
		inbuf = malloc(inbufsize);
	}

/* Initialize global variables */
	if (! VarsLinked) {
		GVLinkInt("$MaxReadWarn",    GVF_INTERNAL | GVF_NODELETE, &MaxReadWarn);
		GVLinkInt("$MaxReadErrors",  GVF_INTERNAL | GVF_NODELETE, &MaxReadErrors);
		VarsLinked = TRUE;
	}

/* ----------------------------------------------------------------------------
-- Handle the options and the filename specification first.
-- Integrate the request for the filename along with options, so can
-- be given in either order.  The first "non-option" will be used as the
-- filename, and options will be scanned on both sides.
--
-- Logic is a bit backwards.  First check if it is an option and handle.
-- If not an option, then check as the filename.  Continue until we have
-- a filename (as set by FILETYPE != UNSPECIFIED), and no more options.
---------------------------------------------------------------------------- */
	FileType = UNSPECIFIED;						/* No file specified yet	*/
	DataType = XYZ;								/* Read curve x,y,z data	*/
	FileMode = UNKNWN;							/*	Mode is unknown			*/
	AsciiFormat = MAYBE_EXPR;					/* Ascii assumed w/ expressions */

	/* Set up default to use XYZ curve as destination */
	for (i=0; i<3; i++) {
		entry[i].icol    = i+1;					/* -col 1 2 3					*/
		entry[i].type    = IS_REAL;			/* Real array					*/
		entry[i].maxsize = 0;					/* No space for now			*/
		entry[i].x       = NULL;				/* No specific location		*/
		entry[i].size    = NULL;				/* Pointer to # of points	*/
		entry[i].next    = 0;					/* Next entry to be filled	*/
	}
	nlist = (GptCurve->z==NULL) ? 2 : 3 ;	/* default # of columns		*/

	*ids = '\0';									/* So someone can start writing */
	*BeginText = *EndText = '\0';				/* File considered all for ASCII reads */
	options = TRUE;								/* Can be disabled */
	debug = FALSE;									/* Disable debug */
		
	rd_flags = 0;									/* Turn off all flags	*/
	do_append = FALSE;							/* No append for moment */
	while (TRUE) {
		if (FileType == UNSPECIFIED && LexIsEmpty()) {
			LexReadLine("File to read (ABORT): ");
			if (LexEscape(TRUE)) return(1);
		}
		if (options && LexGetOptionEx(token, sizeof(token), "-")) {	/* Check the options */
			if (LexEqual(token, "-DUMMY", 6)) {								/* Dummy read - just initialize */
				return(0);
			} else if (LexEqual(token, "-CURVE", 3)) {
				if (LexGetTokenP(token, sizeof(token), "Curve name (unchanged): ")) {
					strncpy(UseCurve, token, sizeof(UseCurve));
					if (GVGetInfo(UseCurve, &type, NULL)) {				/* Name exists, verify properties */
						if (type == GV_2DCURVE || type == GV_3DCURVE) {
							DataType = XYZ;
						} else if (type == GV_SURFACE) {
							DataType = SURF;
						} else {
							ERRprintf("ERROR: Variable %s exists but not curve type - won't destroy\n", UseCurve);
							return(-1);
						}
					} else {															/* Doesn't exist, create now */
						DataType = XYZ;
						if (Gpt->mode_3d) {
							GVAlloc3DCurve(UseCurve, GVF_USER, 2048);
						} else {
							GVAlloc2DCurve(UseCurve, GVF_USER, 2048);
						}
						if (! GVGetInfo(UseCurve, &type, NULL)) {			/* Verify that it was created successfully */
							ERRprintf("ERROR: Could not allocate %s as a new curve\n", UseCurve);
							return(-1);
						}
					}
				}
				if (DataType == XYZ) {											/* If now XYZ curve, link it as GptCurve */
					if (GptLinkXYZ(UseCurve) != 0) {
						ERRprintf("ERROR: Specified curve %s cannot be accessed\n", UseCurve);
						return(-1);
					}
					nlist = (GptCurve->z == NULL) ? 2 : 3 ;				/* And reset the number of columns valid */
				}
					
			} else if (LexEqual(token, "-DEBUG", 6)) {
				debug = TRUE;											/* Enable debug */

			} else if (LexEqual(token, "-SURFACE", 5) || LexEqual(token, "-MATRIX", 4) || LexEqual(token, "-3D", 3)) {
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: ")) strcpy(UseCurve, "surface");

			} else if (LexEqual(token, "-ASCII", 3) || LexEqual(token, "-CSV", 4)) {
				FileMode = ASCII;

			} else if (LexEqual(token, "-BITMAP", 3) || LexEqual(token, "-BMP", 4)) {
				FileMode = BIT_MAP;
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: "))
					strcpy(UseCurve, "surface");

			} else if (LexEqual(token, "-AVI", 3)) {
				FileMode = AVI_MAP;
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: "))
					strcpy(UseCurve, "surface");

			} else if (LexEqual(token, "-AFM", 4)) {
				FileMode = AFM_IMAGE;
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: "))
					strcpy(UseCurve, "surface");

			} else if (LexEqual(token, "-CAMERA", 3)) {
				FileMode = CAMERA;
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: "))
					strcpy(UseCurve, "surface");
			} else if (LexEqual(token, "-FLIR", 5)) {
				FileMode = FLIR;
				DataType = SURF;
				if (! LexGetTokenP(UseCurve, sizeof(UseCurve), "Curve name: "))
					strcpy(UseCurve, "surface");

			} else if (LexEqual(token, "-BINARY", 2)) {
				FileMode = BINARY;

			} else if (LexEqual(token, "-APPEND", 3)) {
				do_append = TRUE;

			} else if (LexEqual(token, "-SILENT", 4) || LexEqual(token, "-QUIET", 2)) {
				tellinfo = FALSE;

			} else if (LexEqual(token, "-EXPRESSIONS", 5)) {
				AsciiFormat = HAS_EXPR;			/* Forced as just digits */
			} else if (LexEqual(token, "-STRICT", 7)) {
				AsciiFormat = IS_STRICT;		/* Forced as just digits */
			} else if (LexEqual(token, "-NOEXPRESSIONS", 6)) {
				AsciiFormat = IS_STRICT;		/* Forced as just digits */
			} else if (LexEqual(token, "-DELIMITERS", 4) || LexEqual(token, "-DELIMS", 7)) {
				if (LexGetStrExprP(token, sizeof(token), "Delimiter list: ")) {
					aptr = token; bptr = AsciiDelims; 
					for (i=0; *aptr!='\0' && i < sizeof(AsciiDelims)-1; i++) {
						if (*aptr != '\\') {
							*(bptr++) = *(aptr++);
						} else switch (aptr[1]) {
							case 'a': *(bptr++) = '\a'; aptr += 2; break;
							case 'b': *(bptr++) = '\b'; aptr += 2; break;
							case 'f': *(bptr++) = '\f'; aptr += 2; break;
							case 'n': *(bptr++) = '\n'; aptr += 2; break;
							case 'r': *(bptr++) = '\r'; aptr += 2; break;
							case 't': *(bptr++) = '\t'; aptr += 2; break;
							case 'v': *(bptr++) = '\v'; aptr += 2; break;
							case '\\': *(bptr++) = '\\'; aptr += 2; break;
							default: *(bptr++) = *(aptr++); break;
						}
					}
					*bptr = '\0';
				} else {
					strcpy(AsciiDelims, " \t,;");
				}

			} else if (LexEqual(token, "-NOWARNING", 3)) {
				warn = FALSE;

/* Options which apply only to AVI, BITMAP or possibly AFM images */
			} else if (LexEqual(token, "-FRAME", 3)) {
				iframe = LexGetInt(1, "Frame number (1=first): ");
			} else if (LexEqual(token, "-RED", 4)) {
				rd_flags |= RD_RED_ONLY;
			} else if (LexEqual(token, "-GREEN", 6)) {
				rd_flags |= RD_GREEN_ONLY;
			} else if (LexEqual(token, "-BLUE", 5)) {
				rd_flags |= RD_BLUE_ONLY;
			} else if (LexEqual(token, "-CHROMA", 7)) {
				rd_flags |= RD_CHROMA_ONLY;
				
/* Back to general options */
			} else if (LexEqual(token, "-ROWS", 4) || LexEqual(token, "-LINES", 6)) {
				FirstRow = LexGetInt(1,       "First line of file to read (1): ");
				LastRow  = LexGetInt(INT_MAX, "Last line of file to read (MAX): ");

			} else if (LexEqual(token, "-COLUMNS", 4) || LexEqual(token, "-COLS", 5)) {
				if (DataType == SURF) {
					FirstCol = LexGetInt(1, "First column to keep (1=first): ");
					LastCol  = LexGetInt(INT_MAX, "Last column to keep (all): ");
				} else if (DataType == LIST) {
					ERRprintf("ERROR: -COLUMNS option incompatible after -LIST/-SURFACE option\n");
					return(-1);
				} else {
					entry[0].icol = LexGetInt(1, "X column (1): ");
					entry[1].icol = LexGetInt(2, "Y column (2): ");
					if (nlist == 3) entry[2].icol = LexGetInt(3, "Z column (3): ");
				}

/* Text to mark start/end of lines of data in the file */
			} else if (LexEqual(token, "-BEGIN", 4)) {
				if (! LexGetStrExprP(BeginText, sizeof(BeginText), "Text marking beginning of data (none): ")) *BeginText = '\0';
			} else if (LexEqual(token, "-END", 4)) {
				if (! LexGetStrExprP(EndText, sizeof(EndText), "Text marking end of data (none): ")) *EndText = '\0';

/* Read directly into arrays of real numbers or into strings */
			} else if (LexEqual(token, "-LIST", 4)) {
				if (DataType == SURF) {
					ERRprintf("ERROR: -LIST option incompatible after -SURFACE option\n");
					return(-1);
				}
				DataType = LIST;
				for (nlist=0; nlist<NVAR; nlist++) {				/* nlist important -- gets set here */
					if (! LexGetTokenP(token, sizeof(token), "Variable (end): ")) break;
					if (LexEscape(TRUE)) return(0);
					if (*token == '/') break;
					if (! GVGetInfo(token, &type, (void **) &varptr)) {
						ERRprintf("ERROR: Variable %s does not exist\n", token);
						return(-1);
					} else if (type == GV_ARRAY || type == GV_ARRAY_LINK) {
						array = (ARRAY *) *varptr;
						entry[nlist].icol    = LexGetInt(nlist+1, "Column: ");
						entry[nlist].type    = IS_REAL;
						entry[nlist].x       = array->x;
						entry[nlist].maxsize = array->maxsize;
						entry[nlist].size    = array->size;
						entry[nlist].next    = 0;
					} else if (type == GV_STRING_ARRAY) {
						STRING_ARRAY *str_array;
						str_array = (STRING_ARRAY *) *varptr;
						entry[nlist].icol    = LexGetInt(nlist+1, "Column: ");
						entry[nlist].type    = IS_STRING;
						entry[nlist].x       = (REAL *) str_array->sval;
						entry[nlist].maxsize = str_array->maxsize;
						entry[nlist].size    = str_array->size;
						entry[nlist].next    = 0;
					} else {
						ERRprintf("ERROR: Specified variable %s is not an array\n", token);
						return(-1);
					}
					if (LexEscape(TRUE)) return(0);
				}
			} else if (LexEqual(token, "-USER", 5)) {
				if (GptUserRead == NULL) {
					ERRprintf("ERROR: No user read routine loaded\n");
					return(-1);
				}
				DataType = USER;
				options = FALSE;
			} else if (LexEqual(token, "-name", 5) || LexEqual(token, "-filename", 5)) {
				if (! LexGetStrExprP(FileName, sizeof(FileName), "Filename expression: ")) return 1;
				if ( (FileType = Determine_FileType_from_Name(FileName, ids)) == WAS_ERROR) return -1;
				if (FileType == DIGITIZE || FileType == CURSOR_DIGITIZE) options = FALSE;
			} else {
				ERRprintf("ERROR: %s is an unrecognized READ option\n", token);
				return(-1);
			}
		} else if (FileType == UNSPECIFIED) {					/* Don't have file yet */
			if (! LexGetFile(FileName, sizeof(FileName))) return(1);
			if ( (FileType = Determine_FileType_from_Name(FileName, ids)) == WAS_ERROR) return -1;
			if (FileType == DIGITIZE || FileType == CURSOR_DIGITIZE) options = FALSE;
		} else {
			break;
		}
	}

/* First off, handle really trivial cases */
	if (FileType == DIGITIZE)			return(DigitizeCurve(UseCurve));
	if (FileType == CURSOR_DIGITIZE) return(CursorizeCurve(UseCurve));
	if (DataType == USER)				return((*GptUserRead)(FileName, UseCurve));

/* -----------------------------------------------------------------------------
-- ... Modify some flags associated with options to pass to other read routines 
----------------------------------------------------------------------------- */
	if (! tellinfo) rd_flags |= RD_SILENT;
	if (debug)		 rd_flags |= RD_DEBUG;
	if (! warn)     rd_flags |= RD_NOWARN;

/* --------------------------------------------
-- ... Okay, scan structure and check validity
--------------------------------------------- */
	FirstRow = max(1, FirstRow);					/* Become 1 based on FirstRow */
	if (LastRow < FirstRow) {
		ERRprintf("ERROR: Invalid rows specified to read (%d - %d)\n", FirstRow, LastRow);
		return(-1);
	}

	if (*ids == '\0') sprintf(ids, "!%s", FileName);	/* ! Default IDS, not real */
	if (DataType == XYZ) {
		entry[0].x = GptCurve->x;
		entry[1].x = GptCurve->y;
		if (nlist == 3) entry[2].x = GptCurve->z;
		for (j=0; j<nlist; j++) {
			entry[j].type    = IS_REAL;
			entry[j].maxsize = GptCurve->nptmax;
			entry[j].size    = NULL;
			entry[j].next    = do_append ? GptCurve->npt : 0;
		}
	} else if (DataType == LIST) {
		if (do_append) {
			for (j=0; j<nlist; j++) entry[j].next = *entry[j].size;
		}
	} else if (DataType == SURF) {
		if (do_append) {
			ERRprintf("ERROR: -SURFACE is incompatible with -APPEND\n");
			return(-1);
		}
	}
		
	maxcol = 0;											/* Max column we must read */
	maxnpt = 0;											/* Max size of data arrays */
	if (DataType != SURF) {
		minnpt = GVI_MAX_LENGTH;					/* Min size of data arrays */
		for (i=0; i<nlist; i++) {
			maxcol = max(maxcol, entry[i].icol);
			maxnpt = max(maxnpt, entry[i].maxsize-entry[i].next);		/* Free space remaining */
			minnpt = min(minnpt, entry[i].maxsize-entry[i].next);		/* Free space remaining */
		}
		if (maxcol == 0) {
			ERRprintf("ERROR: No data columns specified for READ\n");
			return(-1);
		}
	}

/* -----------------------------------------------
-- ... Set flags based on file type - open files
-------------------------------------------------- */
	switch (FileType) {
		case STREAM:
			EchoFlag   = LexSetLocalNoEcho(TRUE);		/* Shut off echoing		*/
			FileHandle = NULL;								/* Make sure disabled	*/
			FileMode   = ASCII;								/* Must be simple ASCII	*/
			break;
		case CONSOLE:
			FileHandle = NULL;								/* Make sure disabled	*/
			if (FileMode == UNKNWN) FileMode = ASCII;	/* Must be simple ASCII	*/
			if (FileMode != ASCII) {
				ERRprintf("ERROR: Console read only possible for ASCII based data\n");
				goto ReadError;
			}
			TTYprintf(PROMPTFORDATA);
			break;
		case PIPE:
			if (FileMode == UNKNWN) FileMode = ASCII;	/* Assume simple ASCII	*/
			if (FileMode == ASCII || FileMode == CAMERA) {
				FileHandle = popen(FileName+1, "r");
			} else if (FileMode == BINARY || FileMode == BIT_MAP || FileMode == FLIR) {
				FileHandle = popen(FileName+1, "rb");	/* Open as binary */
				RMBinaryMode(NULL);							/* Take default */
			} else {
				ERRprintf("ERROR: Pipe reads only implemented for ASCII, BINARY, BIT_MAP, FLIR and CAMERA data\n");
				goto ReadError;
			}
			if (FileHandle==NULL) goto OpenError;
			break;
		case SIMPLEFILE:
			switch (FileMode) {
				case ASCII:
				case CAMERA:
					if ( (FileHandle = fopen(FileName, "r")) == NULL) goto OpenError;
					break;
				case BIT_MAP:
				case FLIR:
					if ( (FileHandle = fopen(FileName, "rb")) == NULL) goto OpenError;
					break;
				case AFM_IMAGE:
					if ( (FileHandle = fopen(FileName, "r")) == NULL) goto OpenError;
					fclose(FileHandle);
					break;
				case AVI_MAP:
					if ( (FileHandle = fopen(FileName, "rb")) == NULL) goto OpenError;
					fclose(FileHandle);
					break;
				case BINARY:
				case UNKNWN:
					if ( (FileHandle = fopen(FileName, "rb")) == NULL) goto OpenError;
					if (RMBinaryMode(FileHandle)) {			/* Is it binary?			*/
						FileMode = BINARY;
					} else if (FileMode == BINARY) {			/* Supposed to be binary? */
						goto ReadError;
					} else {											/* Assume ASCII now		*/
						FileMode = ASCII;
						fclose(FileHandle);
						if ( (FileHandle = fopen(FileName,"r")) == NULL) goto OpenError;
					}
					break;
			}
			break;
		default:
			panic;
	}
				
/* -----------------------------------
-- ... Read unformatted data files
----------------------------------- */
	if (FileMode == BINARY) {								/* Binary file? */
		if (! RMReadText(tok80)) goto ReadError;
		if (strnicmp(tok80, "VERSION 3.1",11) == 0) {
			vers = 31;
		} else if (strnicmp(tok80, "VERSION GENPLT 4.0",  18)  == 0 ||		/* Normal version for all but surface */
					  strnicmp(tok80, "VERSION GENPLOT 4.0", 19) == 0) {
			vers = 40;
		} else if (strnicmp(tok80, "VERSION GENPLT 4.1",  18)  == 0 ||		/* Same as 4.1, but adds two additional columns */
					  strnicmp(tok80, "VERSION GENPLOT 4.1", 19) == 0) {		/* for the COL and ROW data on surfaces only		*/
			vers = 41;
		} else {
			ercd=1; goto UnformattedError;
		}

		while (TRUE) {											/* Read headers */
			if (! RMReadText(tok80)) {ercd=2; goto UnformattedError;}
			if (*tok80 == CLZ) break;
			if (*ids == '!') strscpy(ids, tok80, sizeof(ids));
			if (strncmp(tok80,"@ ",2) == 0) {			/* Imbedded command */
				LexInsText(tok80+2);
			} else if (tellinfo) {
				TTYputsnl(tok80);
			}
		}

		if (vers == 31) {										/* Read # pts, columns */
			if (! RMReadScale(&xfact, &xoff, &yfact, &yoff)) {ercd=2; goto UnformattedError;}
			if (! RMReadInt(&NumRead)) {ercd=3; goto UnformattedError;}
			ncol = 2;
		} else {
			if (! RMRead2Int(&NumRead, &ncol)) {ercd=4; goto UnformattedError;}
		}

		if (maxcol > ncol) goto TooFewColumns;				/* Enough columns?	*/
		LastRow  = min(LastRow, NumRead);					/* last row				*/
		NumValid = LastRow - FirstRow + 1;					/* # valid points		*/
		buf = (REAL *) malloc(NumRead*sizeof(REAL));		/* Allocate for all	*/

		if (DataType == XYZ) {									/* Checks on data		*/
			if (NumValid > entry[0].maxsize-entry[0].next) {
				if (! GVResize(UseCurve, entry[0].next+NumValid)) goto UnableToResize;
				GptLinkXYZ(UseCurve);
			}
			strscpy(GptCurve->ids, ids, sizeof(GptCurve->ids));
			entry[0].x = GptCurve->x;				/* Relink				*/
			entry[1].x = GptCurve->y;
			if (nlist == 3) entry[2].x = GptCurve->z;
			for (j=0; j<nlist; j++) entry[j].maxsize = GptCurve->nptmax;
			maxnpt = entry[0].maxsize - entry[0].next;
		} else if (DataType == SURF) {
			FirstCol = min(ncol, max(1, FirstCol));
			LastCol  = min(ncol, max(1, LastCol));
			maxcol   = LastCol;
			if (! GVAllocSurface(UseCurve, GVF_USER, NumValid, LastCol-FirstCol+1)) {
				ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
				goto AllExit;
			} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) ||
				(type != GV_SURFACE) ) {
				ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
				goto AllExit;
			}
			surface = (SURFACE *) *varptr;
			z = surface->z;
			strscpy(surface->ids, ids, sizeof(surface->ids));
		}

		for (i=1; i<=maxcol; i++) {				/* Read # of colums needed */
			if (SysChkBreak(FALSE)) goto CTRLC_Abort;
			if (vers == 31) {
				if (i == 2) {xfact=yfact; xoff=yoff;}
			} else {
				if (! RMReadText(tok80)) {ercd=5; goto UnformattedError;}
				if (! RMRead2Scale(&xfact, &xoff)) {ercd=6; goto UnformattedError;}
			}
			HaveRead = FALSE;
			if (DataType == SURF) {
				if (i >= FirstCol) {
					HaveRead = TRUE;
					if (! RMReadArray(buf, NumRead)) {ercd=7; goto UnformattedError;}
					if (xfact != 1 || xoff != 0)
						for (k=FirstRow-1; k<LastRow; k++) buf[k] = buf[k]*xfact + xoff;
					memcpy(z, buf+FirstRow-1, NumValid*sizeof(REAL));
					z += NumValid;
				}
			} else {
				for (j=0; j<nlist; j++) {				/* Only process if in list	*/
					if (entry[j].icol == i) {			/* This one matches!!!		*/
						if (entry[j].type != IS_REAL) {
							ERRprintf("ERROR: No way to read from an unformatted file into string arrays.  Column %d ignored\n", i);
							continue;
						}
						if (DataType == XYZ && j < 3) {		/* Consider setting descriptors */
							strscpy(Gpt->XYZ_descriptor[j], tok80, sizeof(Gpt->XYZ_descriptor[j]));
						}
						if (! HaveRead) {
							HaveRead = TRUE;
							if (! RMReadArray(buf, NumRead)) {ercd=8; goto UnformattedError;}
							if (xfact != 1 || xoff != 0)
								for (k=FirstRow-1; k<LastRow; k++) buf[k] = buf[k]*xfact + xoff;
						}
						iuse = min(NumValid, entry[j].maxsize-entry[j].next);
						memcpy(entry[j].x+entry[j].next, buf+FirstRow-1, iuse*sizeof(REAL));
						if (entry[j].size != NULL) *entry[j].size = entry[j].next+iuse;
					}
				}
			}
			if (! HaveRead) if (! RMDumpReals(NumRead)) {ercd=9; goto UnformattedError;}
		}
		if (DataType == XYZ) GptCurve->npt = entry[0].next + NumValid;

/* If we are doing surfaces and have version 4.1 data, then can recover X,Y coordinates also */
		if (vers == 41 && DataType == SURF) {
			for (i=maxcol+1; i<ncol+1; i++) {									/* Read remaining unused data columns (based on 1 index) */
				if (! RMReadText(tok80)) {ercd=11; goto UnformattedError;}
				if (! RMRead2Scale(&xfact, &xoff)) {ercd=11; goto UnformattedError;}
				if (! RMDumpReals(NumRead)) {ercd=11; goto UnformattedError;}
			}

			/* Need to get ncol of x values, and nrow (NREAD) of y values */
			if (! RMReadText(tok80)) {ercd=12; goto UnformattedError;}				/* Read the row values */
			if (! RMRead2Scale(&xfact, &xoff)) {ercd=12; goto UnformattedError;}
			if (! RMReadArray(buf, NumRead)) {ercd=12; goto UnformattedError;}
			if (xfact != 1 || xoff != 0) for (k=0; k<NumRead; k++) buf[k] = buf[k]*xfact + xoff;
			memcpy(surface->y, buf+FirstRow-1, NumValid*sizeof(REAL));

			if (! RMReadText(tok80)) {ercd=13; goto UnformattedError;}				/* Read the column values */
			if (! RMRead2Scale(&xfact, &xoff)) {ercd=14; goto UnformattedError;}
			if (ncol > NumRead) buf = (REAL *) realloc(buf, ncol*sizeof(REAL));	/* Space for column values */
			if (! RMReadArray(buf, ncol)) {ercd=15; goto UnformattedError;}
			if (xfact != 1 || xoff != 0) for (k=0; k<ncol; k++) buf[k] = buf[k]*xfact + xoff;
			memcpy(surface->x, buf+FirstCol-1, (LastCol-FirstCol+1)*sizeof(REAL));
		}

/* ---------------------------------------------------------------------------
-- .......................... Camera surface reads here .....................
--------------------------------------------------------------------------- */
	} else if (FileMode == CAMERA) {
		surface = GptCameraRead(FileHandle, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags);
		if (surface == NULL) {
			ERRprintf("ERROR: Surface read from a CAMERA file failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

/* ---------------------------------------------------------------------------
-- .......................... Bitmap surface reads here .....................
--------------------------------------------------------------------------- */
	} else if (FileMode == BIT_MAP) {
		surface = GptBitmapRead(FileHandle, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags);
		if (surface == NULL) {
			ERRprintf("ERROR: Surface read from a BITMAP file failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

/* ---------------------------------------------------------------------------
-- .......................... AVI frame surface reads here ..................
--------------------------------------------------------------------------- */
	} else if (FileMode == AVI_MAP) {
		surface = GptAVIRead(FileName, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags, iframe);
		if (surface == NULL) {
			ERRprintf("ERROR: Surface read from an AVI file failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

/* ---------------------------------------------------------------------------
-- .......................... AFM frame surface reads here ..................
--------------------------------------------------------------------------- */
	} else if (FileMode == AFM_IMAGE) {
		surface = GptAFMRead(FileName, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags, iframe);
		if (surface == NULL) {
			ERRprintf("ERROR: Surface read from an AFM file failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

/* ---------------------------------------------------------------------------
-- .......................... FLIR camera surface reads here ................
--------------------------------------------------------------------------- */
	} else if (FileMode == FLIR) {
		surface = GptFLIRRead(FileName, FileHandle, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags, iframe);
		if (surface == NULL) {
			ERRprintf("ERROR: Surface read from a FLIR file failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

/* ---------------------------------------------------------------------------
-- .......................... Formatted reads here ..........................
--------------------------------------------------------------------------- */
	} else if (DataType == SURF) {						/* ASCII FileType		*/
		surface = GptMatrixRead(FileHandle, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags);
		if (surface == NULL) {
			ERRprintf("ERROR: ASCII read of SURFACE files failed\n");
			goto AllExit;
		}
		if (*surface->ids == '\0') strscpy(surface->ids, ids, sizeof(surface->ids));

	} else {
		if (FileType == SIMPLEFILE && FirstRow <= 1) {	/* Check for version	*/
			if (fgets(token,sizeof(token),FileHandle)==NULL) goto ASCIIReadError;
			aptr = token;
			while (isspace(*aptr)) aptr++;						/* Skip white space	*/

			if (strnicmp(aptr, "VERSION 0.0", 11) == 0) {	/* Has header only	*/
				while (TRUE) {
					if (fgets(token, sizeof(token), FileHandle) == NULL) goto ASCIIHeaderError;
					if (*token == CLZ) break;
					if (*ids == '!') strscpy(ids, token, sizeof(ids));
					if (tellinfo) TTYputs(token);
				}
			} else if (	(strnicmp(aptr, "VERSION 1.1", 11) == 0) ||
							(strnicmp(aptr, "VERSION GENPLT 1.1", 18) == 0) ||
							(strnicmp(aptr, "VERSION GENPLOT 1.1", 19) == 0) ) {
				if (DataType != XYZ || nlist != 2) goto FileIncompatible;
				i = 2; while (i) {								/* Scan two headers	*/
					if (fgets(token, sizeof(token), FileHandle) == NULL) goto ASCIIHeaderError;
					if (*token == CLZ) i--;
					if (i == 2) {									/* Primary header only */
						if (*ids == '!') strscpy(ids, token, sizeof(ids));
						if (tellinfo) TTYputs(token);
					}
				}
				DoScaling = TRUE;									/* Will have scaling */
				if (fscanf(FileHandle, "%f %f %f %f", &xfact, &xoff, &yfact, &yoff) != 4) goto ASCIIHeaderError;
				if (fscanf(FileHandle, "%d", &NumRead) != 1) goto ASCIIHeaderError;
			} else {													/* Not my versions	*/
				rewind(FileHandle);								/* Rewind the file	*/
			}
		}

/* ... Common read for ASCII files */
		NumValid  = 0;
		NumErrors = 0;
		ScanTime(NULL, NULL);												/* Reset to handle wrap-around gracefully */
		for (irow=0; irow<LastRow; irow++) {
			if (SysChkBreak(FALSE)) {
				ERRprintf("ERROR: Read aborted by ^C\n");
				LexFlush();
				break;
			}
			Iskip = 0;

ReadAnotherLine:
			if (FileType == STREAM) {										/* Stream input? */
				LexPromptStr(inbuf, inbufsize, ":");					/* Get a line from current input file */
				TrimWhiteSpace(inbuf);										/* Cleanup the input line */
				if (LexEqual(inbuf, FileName+2, -32)) break;			/* <<test terminates on test or on @end */
				if (strnicmp(inbuf,"c ",2)==0 && stricmp(inbuf+2,FileName+2)==0) break;	/* Also terminate in "c test" */
			} else if (FileType == CONSOLE) {
				if (! UserInput(":", inbuf, inbufsize)) break;
				TrimWhiteSpace(inbuf);										/* Cleanup the input line */
				if (stricmp(inbuf, "@end") == 0 || stricmp(inbuf, "@eof") == 0) break;
			} else {												/* File input */
				if (fgets(inbuf, inbufsize, FileHandle) == NULL) break;
CheckForMoreOfThisLine:
				i = (int) strlen(inbuf);
				if (i == inbufsize-1 && inbuf[i-1] != '\n') {	/* Expand and read more */
					inbufsize += LINEBUFSIZE;
					inbuf = realloc(inbuf, inbufsize);
					TTYprintf("MSG: Increasing read buffer size to %d\n", inbufsize);
					if (fgets(inbuf+i, inbufsize-i, FileHandle) == NULL) break;
					goto CheckForMoreOfThisLine;
				}
				TrimWhiteSpace(inbuf);										/* Cleanup the input line */
			}
			if (*BeginText != '\0') {										/* Waiting for anything to start? */
				aptr = inbuf; while (isspace(*aptr)) aptr++;			/* Trim whitespace (duplication) */
				if (strnicmp(aptr, BeginText, strlen(BeginText)) == 0) *BeginText = '\0';
				goto ReadAnotherLine;
			}
			if (Iskip > 0) {Iskip--; goto ReadAnotherLine;}
			if (*inbuf == '\0') continue;									/* Nothing on the line, just skip */
			if (*EndText != '\0') {											/* Watching for end of data marker */
				aptr = inbuf; while (isspace(*aptr)) aptr++;			/* Trim whitespace (duplication) */
				if (strnicmp(aptr, EndText, strlen(EndText)) == 0) break;
			}

/* ... Check if a comment, or if an imbedded command ... */
			if (AsciiFormat != IS_STRICT) {
				if (tolower(*inbuf)=='c' || *inbuf=='#') {	/* Possible comments */
					if (inbuf[1] == '\0') continue;				/* Just line separator	*/
					if (isspace(inbuf[1])) {						/* Real comment		*/
						if (*ids == '!') strscpy(ids, inbuf+2, sizeof(ids));
						continue;
					}														/* If not, maybe value */
				} else if (strncmp(inbuf, "/*", 2) == 0) {	/* Required comment	*/
					if (inbuf[2] != '\0' && *ids == '!') strscpy(ids, inbuf+3, sizeof(ids));
					continue;
				} else if (strnicmp(inbuf, "@end", 4) == 0) {
					break;
				} else if (strnicmp(inbuf, "@skip", 5) == 0) {
					Iskip = atoi(inbuf+5);
					goto ReadAnotherLine;
				} else if (strncmp(inbuf, "@ ",2) == 0) {		/* Imbedded commands	*/
					if (irow >= FirstRow-1) LexInsText(inbuf+2);
					continue;
				}
			}

/* ... Ignore as data if within early rows ... */
			if (irow < FirstRow-1) continue;					/* Ignored line */
 
			if (DataType == XYZ && NumValid+entry[0].next >= entry[0].maxsize) {
				if (GVResize(UseCurve, 2*GptCurve->nptmax)) {	/* Increase size */
					GptLinkXYZ(UseCurve);
					entry[0].x = GptCurve->x;
					entry[1].x = GptCurve->y;
					if (nlist == 3) entry[2].x = GptCurve->z;
					for (j=0; j<nlist; j++) entry[j].maxsize = GptCurve->nptmax;
					maxnpt = GptCurve->nptmax - entry[0].next;
				}
			}

			if (NumValid >= maxnpt) {								/* Check for end */
				if (FileType != CONSOLE && tellinfo) CONputs("\r\n");
				ERRprintf("WARNING: Max # points read -- Extra points will be ignored\n");
				break;
			}

			aptr    = inbuf;								/* Start parsing token		*/
			ertype  = GPT_NOERROR;						/* No errors on this line	*/

			for (i=1; i<=maxcol; i++) {				/* Loop through cols as need */
				if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, AsciiDelims)) {
					ertype = CANTSCAN;
					break;
				}

				HaveRead = FALSE;
				for (j=0; j<nlist; j++) {				/* Only process if in list	*/
					if (entry[j].icol == i) {			/* This one is in the list */
						if (entry[j].next >= entry[j].maxsize) continue;			/* No space */
						if (entry[j].type == IS_STRING) {
							char **sptr;
							sptr = (char **) entry[j].x;
							if (sptr[entry[j].next] != NULL) free(sptr[entry[j].next]);
							sptr[entry[j].next] = strdup(token);
							entry[j].next++;
							if (entry[j].size != NULL) *entry[j].size = entry[j].next;
						} else {
							if (! HaveRead) {
								bptr = token;							/* Check that all chars are nominally	*/
								while (isprint(*bptr)) bptr++;	/* valid before letting GVEvalExpr		*/
								if (*bptr != '\0') {					/* complain about string.  Binary strs	*/
									ertype=BADCHAR; break;			/* may screw up the terminal.				*/
								}
								/* Try to interpret via strtod first, then w/ expressions */
								if (AsciiFormat != HAS_EXPR) xtmp = GVTrimToReal(strtod(token, &bptr));
								/* Also, try to interpret possibly as an hh:mm:ss timestamp */
								if (strchr(token,':') != NULL && ScanTime(token, &timevalue)) {
									xtmp = (REAL) timevalue;
								} else if (AsciiFormat == HAS_EXPR || (*bptr != '\0' && AsciiFormat != IS_STRICT)) {
									xtmp = GVTrimToReal(GVEvalExpr(token, &k));
									if (k != 0) {						/* Skip out on internal errs also */
										ertype=BADEXPR; break;
									}
								} else if (*bptr != '\0') {
									ERRprintf("ERROR: Unrecognized garbage trailing value in -STRICT mode (%s)\n", bptr);
									ertype=BADEXPR; break;
								}
								if (DoScaling) {
									if (i == 1) xtmp = xtmp*xfact + xoff;
									if (i == 2) xtmp = xtmp*yfact + yoff;
								}
								HaveRead = TRUE;
							}
							entry[j].x[entry[j].next] = xtmp;
							entry[j].next++;
							if (entry[j].size != NULL) *entry[j].size = entry[j].next;
						}
					}
				}
				if (ertype != GPT_NOERROR) break;		/* Carry forward inward breaks */
			}

			if (ertype != GPT_NOERROR) {
				if (warn && NumErrors < MaxReadWarn) {
					ERRprintf("WARNING: Too few columns, bad expression or illegal data on line %d\n", irow+1);
				} else if (warn && NumErrors == MaxReadWarn) {
					ERRprintf("WARNING: Warning messages suppressed.  Read may have up to %d errors\n", MaxReadErrors);
					GVMathMode |= (MATH_NO_PARSE_MSG | MATH_NO_CALC_MSG);			/* Turn off messages */
				} else if (NumErrors == MaxReadErrors) {
					ERRprintf("ERROR: Maximum number of read errors reached (%d).  Aborting read\n", NumErrors);
					break;
				}
				NumErrors++;
				continue;
			}

			if ( FileType!=CONSOLE && (NumValid%UPDATE_SCREEN_COUNT)==0 && tellinfo ) {
				sprintf(token,"\rReading: [%5.5i]", NumValid);
				CONputs(token); fflush(stdout);
			}
			NumValid++;
		}
		GVMathMode &= ~(MATH_NO_PARSE_MSG | MATH_NO_CALC_MSG);		/* Turn back on messages */

		if (FileType != CONSOLE && tellinfo) CONputs("\r\n");

		/* If XYZ curve, need to set npt at end of the read */
		if (DataType == XYZ) {
			strscpy(GptCurve->ids, ids, sizeof(GptCurve->ids));
			GptCurve->npt = entry[0].next;
			for (i=1; i<3 && entry[i].x!=NULL; i++) GptCurve->npt = max(GptCurve->npt, entry[i].next);
		}
	}

/* ----------------------------------------
-- Common ending code here
---------------------------------------- */
	if (DataType != SURF) {
		for (j=0; j<nlist; j++) {							/* Handle column 0 requests */
			if (entry[j].icol == 0) {						/* Request for pt # */
				for (k=0; k <= NumValid && entry[j].next < entry[j].maxsize; k++) {
					entry[j].x[entry[j].next] = (REAL) k;
					entry[j].next++;
				}
				if (entry[j].size != NULL) *entry[j].size = entry[j].next;
			}
		}
	}

	if (tellinfo) {
		if (DataType == LIST) {
			TTYprintf("Number of points: %i   Arrays min/max pts: %i %i\n", NumValid, minnpt, maxnpt);
		} else if (DataType == SURF && surface != NULL) {
			TTYprintf("Surface read with %i rows and %i columns\n", surface->nrow, surface->ncol);
		}
	}

	rcode = (DataType==XYZ && tellinfo) ? 0 : 1;	/* Do a "STAT?" */
	goto AllExit;

/* ----------------------
-- ERRORS printout here!
---------------------- */
ASCIIHeaderError:
	ERRprintf("ERROR: Header information never terminated\n");
	goto AllExit;
ASCIIReadError:
	ERRprintf("ERROR: ASCII file read failure or unsupported format\n");
	goto AllExit;
UnableToResize:
	ERRprintf("ERROR: Unable to increase size of %s to allow read\n", UseCurve);
	goto AllExit;
TooFewColumns:
	ERRprintf("ERROR: Too few COLUMNS in data file\n");
	goto AllExit;
UnformattedError:
	ERRprintf("ERROR: Unformatted read failure (VERSION 3.1/4.0) (%d)\n", ercd);
	goto AllExit;
CTRLC_Abort:
	ERRprintf("ERROR: Read aborted by ^C\n");
	LexFlush();
	goto AllExit;
FileIncompatible:
	ERRprintf("ERROR: File format incompatible with -LIST or -COLUMN options\n");
	goto AllExit;
OpenError:
	ERRprintf("ERROR: Unable to open %s for reading\n", FileName);
	goto AllExit;
ReadError:
	ERRprintf("ERROR: Unable to read from file %s\n", FileName);
	goto AllExit;

/* --- Everybody leaves through here --- */
AllExit:
	if (FileType == STREAM) {
		LexSetLocalNoEcho(EchoFlag);
	} else if (FileType == PIPE && FileHandle != NULL) {
		/* Purge the stream so we don't get annoying messages */
		for (j=0; j<100 && ! feof(FileHandle); i++) fread(inbuf, 1, inbufsize, FileHandle);
		pclose(FileHandle);
	} else if (FileHandle != NULL) { 
		fclose(FileHandle);
	}

	if (buf != NULL) free(buf);
	return(rcode);
}


/* ===========================================================================
-- Simple file read utility - returns data in X,Y,Z as given up to maximum
-- number of points.
--
-- Used by routines like NLSFIT to recover input.
=========================================================================== */
int GptSimpleRead(char *filename, int nptmax, REAL *x, REAL *y, REAL *z) {


	REAL	*buf;										/* Buffers to read in data		*/
	REAL  xfact, xoff;							/* Scaling factors (maybe)	*/
	int	i,j, ncol, npt;

	char	inbuf[LINEBUFSIZE],					/* For parsing ASCII reads		*/
			tok80[80];								/* Must BE 80 for RMRead()		*/
	char	*aptr;

/* -----------------------------------
-- ... Open file, try binary
------------------------------------ */
	if ( (FileHandle = fopen(filename, "rb")) == NULL) return(-1);

	if (RMBinaryMode(FileHandle)) {					/* Is it binary? */

		if (! RMReadText(tok80)) {fclose(FileHandle); return(-2);}
		if ( (strnicmp(tok80, "VERSION GENPLT 4.0",18)  != 0) &&
			  (strnicmp(tok80, "VERSION GENPLOT 4.0",19) != 0) ) {
			fclose(FileHandle);
			return(-2);
		}

		do {													/* Read comment headers */
			if (! RMReadText(tok80)) { fclose(FileHandle); return(-2); }
		} while (*tok80 != CLZ);

		if (! RMRead2Int(&npt, &ncol)) {				/* Read # of points/cols */
			fclose(FileHandle);
			return(-2);
		}

		if ( (npt>nptmax) || (ncol<3 && z!=NULL) || (ncol<2 && y!=NULL) ) {
			fclose(FileHandle);
			return(-3);
		}

		for (i=0; i<ncol; i++) {
			if (! RMReadText(tok80) || ! RMRead2Scale(&xfact, &xoff)) {
				fclose(FileHandle);
				return(-3);
			}
			if ( (buf = (i == 0) ? x : ( (i == 1) ? y : z )) == NULL) break;
			if (! RMReadArray(buf, npt)) {
				fclose(FileHandle);
				return(-3);
			}
			if (xfact != 1 || xoff != 0)
				for (j=0; j<npt; j++) buf[j] = buf[j]*xfact + xoff;
		}

/* ---------------------------------------------------------------------------
-- .......................... Formatted reads here ..........................
--------------------------------------------------------------------------- */
	} else {															/* ASCII FileType		*/
		fclose(FileHandle);
		if ( (FileHandle = fopen(filename,"r")) == NULL) return(-1);

		for (npt=0; npt<nptmax; npt++) {
			if (fgets(inbuf, sizeof(inbuf), FileHandle) == NULL) break;

			i = (int) strlen(inbuf);						/* Null terminate and skip */
			while (i && isspace(inbuf[i-1])) i--;		/* Strip spaces and <nl>	*/
			if (i == 0) continue;							/* Nothing on line			*/
			inbuf[i] = '\0';

			if ( (tolower(*inbuf)=='c')        ||		/* Any form of comment */
				  (*inbuf=='#')                 ||
				  (strncmp(inbuf, "/*",2) == 0) || 
			     (strncmp(inbuf, "@ ",2) == 0) ) {
				npt--;											/* Back up over last inc */
				continue;
			}

			aptr = inbuf;
			x[npt] = (REAL) strtod(aptr, &aptr);
			if (y != NULL) y[npt] = GVTrimToReal(strtod(aptr, &aptr));
			if (z != NULL) z[npt] = GVTrimToReal(strtod(aptr, &aptr));
		}
	}

	fclose(FileHandle);
	return(npt);
}

/* ---------------------------------------------------------------------------
-- Usage: void TrimWhiteSpace(char *buf)
--
-- Inputs: buf - pointer to string read for parsing data file
--
-- Output: *buf - same with leading and trailing whitespace removed
--
-- Return: none
--
-- Makes the input more freeform with respect to comments and commands
--------------------------------------------------------------------------- */
static void TrimWhiteSpace(char *buf) {
	char *aptr;

/* Strip trailing whitespace */
	aptr = buf + strlen(buf) - 1;								/* Last character in string	*/
	while (aptr >= buf && isspace(*aptr)) aptr--;
	aptr[1] = '\0';												/* Okay, all spaces removed	*/

/* Strip leading whitespace */
	aptr = buf;														/* First character				*/
	while (isspace(*aptr)) aptr++;							/* Jump to first non-blank		*/
	if (aptr != buf) {											/* Copy remainder to start		*/
		while (*aptr) *(buf++) = *(aptr++);
		*buf = '\0';
	}
	return;
}

/* ---------------------------------------------------------------------------
-- Usage: REAL *read_unknown_block(size_t *num_read, size_t max_num, size_t cols,
--                                 ARBSTR *inbuf, FILE *funit);
--
-- Inputs: num_read - pointer to be returned with number of items read
--         max_num  - maximum number of lines to bother reading
--         cols     - number of items expected per line
--         inbuf - structure containing the read line (may be expanded)
--           inbuf->buf - actual data buffer
--           inbuf->len - size of the data buffer
--         funit - file unit to refill buf with (unless NULL)
--                 if NULL, will only interpret existing line
--
-- Output: *num_read - number of full lines read.  Lines with fewer than
--                     num_per_line are ignored.
--
-- Return: NULL on error
--         Otherwise pointer to properly sized data (malloc'd)
--------------------------------------------------------------------------- */
static REAL *read_unknown_block(int *num_read, size_t max_num, size_t cols, ARBSTR *inbuf, FILE *funit) {

	int ipt;
	unsigned int i, nlines;
	char *aptr, *bptr;

#define	INITIAL_DATA_SIZE	(4096)					/* Initial size - double each time */
	REAL *data;												/* Array to get the data		*/
	int nptmax;												/* Number of elements in data */

	aptr = inbuf->buf;									/* Scan this line first			*/
	data = NULL; nptmax = 0;							/* Nothing allocated yet		*/

	for (ipt=0,nlines=0; nlines < (int) max_num; nlines++) {
		for (i=0; i<cols; i++,ipt++) {
			while (TRUE) {
				if (aptr != NULL) {
					while (isspace(*aptr) || *aptr==',' || *aptr==';' || *aptr==':') aptr++;
				}
				if (aptr != NULL && *aptr == '\0') {
					if (funit == NULL) goto WeAreDone;
					if (SysReadLongLine(inbuf, funit, B_SKIP_BLANK | B_SKIP_COMMENTS | B_ALLOW_CONTINUES) <= 0) goto WeAreDone;
					aptr = inbuf->buf;
				} else {
					if (ipt >= nptmax) {
						REAL *new_data;
						int nnew;
						nnew = (nptmax==0) ? INITIAL_DATA_SIZE : 2*nptmax ;
						new_data = realloc(data, nnew*sizeof(*data));
						if (new_data == NULL) {
							if (data != NULL) {
								TTYprintf("LIMIT: Memory increase beyond %d total points failed.  Stopping here.\n", nptmax);
							} else {
								ERRprintf("ERROR: Unable to allocate space for data\n");
							}
							goto WeAreDone;
						}
						nptmax = nnew; data = new_data;
						if (nptmax*sizeof(*data) > 50000000) TTYprintf("INFO: zdata read buffer now at %d MBytes\n", nptmax*sizeof(*data)/1048576);
					}
					data[ipt] = GVTrimToReal(strtod(aptr, &bptr));
					if (aptr == bptr) {
						ERRprintf("Bad text on matrix data: %s", aptr);
						*aptr = '\0'; 
					} else {
						aptr = bptr;
						break;
					}
				}
			}
		}
	}

WeAreDone:
	if (data != NULL && ipt > 0) data = realloc(data, ipt*sizeof(*data));			/* Just right size it */
	if (num_read != NULL) *num_read = nlines;
	return(data);
}


/* ---------------------------------------------------------------------------
-- Usage: int readblock(REAL *data, size_t num, ARBSTR *inbuf, FILE *funit);
--
-- Inputs: data  - pointer to space to save values scanned
--         num   - number of entities desired
--         inbuf - structure containing the read line (may be expanded)
--           inbuf->buf - actual data buffer
--           inbuf->len - size of the data buffer
--         funit - file unit to refill buf with (unless NULL)
--                 if NULL, will only interpret existing line
--
-- Output: data - filled with interpreted real numbers from the scan
--
-- Returns: Number of items read.  If funit==NULL, this is # of items on
--          the passed line.
--          -1 if we reached the end of the file before all read
--------------------------------------------------------------------------- */
static int readblock(REAL *data, size_t num, ARBSTR *inbuf, FILE *funit) {

	int i;
	char *aptr, *bptr;

	aptr = inbuf->buf;									/* Scan this line first */

	for (i=0; i<(int) num; i++) {
		while (TRUE) {
			if (aptr != NULL) {
				while (isspace(*aptr) || *aptr==',' || *aptr==';' || *aptr==':') aptr++;
			}
			if (aptr != NULL && *aptr == '\0') {
				if (funit == NULL) return(i);
				if (SysReadLongLine(inbuf, funit, B_SKIP_BLANK | B_SKIP_COMMENTS | B_ALLOW_CONTINUES) <= 0) return(i);
				aptr = inbuf->buf;
			} else {
				data[i] = GVTrimToReal(strtod(aptr, &bptr));
				if (aptr == bptr) {
					ERRprintf("Bad text on matrix data: %s", aptr);
					*aptr = '\0'; 
				} else {
					aptr = bptr;
					break;
				}
			}
		}
	}
	return(i);
}



/* ===========================================================================
-- Usage: int GptMatrixWrite(FILE *funit, SURFACE *surface, BOOL silent);
--
-- Inputs: funit - open file containing information
--         surface - pointer to surface structure
--
-- Output: writes the file with necessary headers
--
-- Returns: 0 if successful, -1 otherwise
=========================================================================== */
int GptMatrixWrite(FILE *funit, SURFACE *surface, BOOL silent) {

	BOOL simple;
	int i,j,icnt;
	double delta;

	if (*surface->ids != '\0') fprintf(funit, "COMMENT: %s\n", surface->ids);
	fprintf(funit, "COLS: %d\n", surface->ncol);
	fprintf(funit, "ROWS: %d\n", surface->nrow);

/* Put out the X column data */			
	simple = (surface->ncol > 1);
	if (simple) {
		delta = (surface->x[surface->ncol-1] - surface->x[0]) / (surface->ncol-1);
		simple = (delta != 0);
		if (simple) {
			for (i=0; i<surface->ncol-1; i++) {
				if (fabs(surface->x[i+1]-surface->x[i]-delta) > 0.0001*delta) {
					simple = FALSE;
					break;
				}
			}
		}
	}
#define	MAX_PER_LINE	(60)
	if (simple) {
		fprintf(funit, "XSCALE: %.8g\n", delta);
		fprintf(funit, "XORIGIN: %.8g\n", surface->x[0]);
	} else {
		fprintf(funit, "XDATA: ");
		for (i=0; i<surface->ncol; i++) {
			fprintf(funit, "%.8g ", surface->x[i]);
			if (i%MAX_PER_LINE == MAX_PER_LINE-1) fprintf(funit, "\n");
		}
		fprintf(funit, "\n");
	}

/* Put out the Y column data */			
	simple = (surface->nrow > 1);
	if (simple) {
		delta = (surface->y[surface->nrow-1] - surface->y[0]) / (surface->nrow-1);
		simple = (delta != 0);
		if (simple) {
			for (i=0; i<surface->nrow-1; i++) {
				if (fabs(surface->y[i+1]-surface->y[i]-delta) > 0.0001*delta) {
					simple = FALSE;
					break;
				}
			}
		}
	}
	if (simple) {
		fprintf(funit, "YSCALE: %.8g\n", delta);
		fprintf(funit, "YORIGIN: %.8g\n", surface->y[0]);
	} else {
		fprintf(funit, "YDATA: ");
		for (i=0; i<surface->nrow; i++) {
			fprintf(funit, "%.8g ", surface->y[i]);
			if (i%MAX_PER_LINE == MAX_PER_LINE-1) fprintf(funit, "\n");
		}
		fprintf(funit, "\n");
	}

/* Put out the Z data */			
	fprintf(funit, "ZDATA: \n");
	for (i=0; i<surface->nrow; i++) {
		icnt = 0;
		for (j=0; j<surface->ncol; j++) {
			fprintf(funit, "%.8g ", surface->z[i+j*surface->nrow]);
			if ((icnt++)%MAX_PER_LINE == MAX_PER_LINE-1) fprintf(funit, "\n");
		}
		fprintf(funit, "\n");
	}

	return(0);
}


/* ===========================================================================
-- Usage: SURFACE *GptMatrixRead(FILE *funit, int FirstCol, int LastCol,
--                          int FirstRow, int LastRow, char *UseCurve,
--                          int rd_flags);
--
-- Inputs: funit - open file containing information
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
--
-- Notes: 
--    (1) Recognized header keywords:
--        COLS: <cols>               NCOL: <cols>
--        ROWS: <rows>               NROW: <rows>
--        COMMENT: <comment>
--        XSCALE: <xdelta>           XORIGIN: <xorigin>
--        YSCALE: <ydelta>           YORIGIN: <yorigin>
--        XDATA: <list of cols values>
--        YDATA: <list of rows values>
--        ZDATA: (keyword terminating header and starting data read)
--    (2) Blank lines, or lines beginning with '#' or {slash}* are ignored
--    (3) All XDATA, YDATA and ZDATA numeric data input is freeform,
--        with an arbitrary number of entries per line, up to a line of
--        1024 characters
--    (4) Values for all single parameter keywords must be on the same line
--    (5) ZDATA is implicit before the first line not beginning with an
--        alpha character.
--    (6) Ten unrecognized header keywords will terminate the read
--    (7) Errors in the body of the ZDATA are ignored, and the line containing
--        the offending text is truncated at that point.  No messages given.
--    (8) If COLS: is not given in the header, it is determined by the number
--        of entries on the first data line.
--    (9) If ROWS: is not given, it is set the actual number in the file,
--        up to 1024.  Larger matricies must give the ROW count.
--   (10) A warning is given if the number of read rows does not equal
--        the number specified on the ROWS header line.
--   (11) Use of the XDATA and YDATA headers requires COLS: and ROWS:
--        to be defined before.
--   (12) The -cols and -rows read options are accepted, and should work.
--        However, the entire matrix is first read and then pasted.
--
-- The matrix is read as it appears in a file -- row major format.  However,
-- it is converted to column major before being stored as Z in the surface,
-- as this is the standard in GENPLOT.  For most users, this is irrelevent.
=========================================================================== */
#define	MAX_SEEK_3D_COLS		(33554432)		/* Max # of columns for autosearch of a scanned line */
#define	MAX_SEEK_3D_ROWS		(33554432)		/* Max # of rows for autosearch of a scanned line */

SURFACE *GptMatrixRead(FILE *funit, int FirstCol, int LastCol,
						int FirstRow, int LastRow, char *UseCurve, int rd_flags) {

	int i,j,errcnt=0;
	int Iskip;
	
	int nrows, npt;
	int rows=-1, cols=-1;
	REAL xorigin=0.0, xscale=1.0, yorigin=0.0, yscale=1.0;
	REAL *xdata=NULL, *ydata=NULL, *zdata=NULL;

	ARBSTR inbuf={NULL, 0};						/* For parsing ASCII reads (HUGE) */
	char ids[DFLT_STR_SIZE]="! ";				/* IDS string						*/
	char *aptr;										/* Input buffer					*/
	int   type;										/* Type of variable (GVLink)	*/
	REAL *z;
	void **varptr;
	SURFACE *surface=NULL;						/* Temporary array pointer		*/

/* ------------------------------------------------------------------
-- Scan the headers.  Allow headers either with the : terminator,
-- or without.  If without, next character must be insignificant
------------------------------------------------------------------- */
	Iskip = 0;
	while (SysReadLongLine(&inbuf, funit, B_ALLOW_CONTINUES) > 0) {
		aptr = inbuf.buf;
		if (Iskip > 0) {Iskip--; continue;}				/* Line skipping */
		while (isspace(*aptr)) aptr++;					/* Skip blanks */

		if (tolower(*aptr)=='c' || *aptr=='#') {		/* Possible comments */
			if (aptr[1] == '\0') continue;				/* Just line separator	*/
			if (isspace(aptr[1])) {							/* Real comment		*/
				if (*ids == '!') strscpy(ids, aptr+2, sizeof(ids));
				continue;
			}														/* If not, maybe value */
		} else if (strncmp(aptr, "/*", 2) == 0) {		/* Required comment	*/
			if (aptr[2] != '\0' && *ids == '!') strscpy(ids, aptr+3, sizeof(ids));
			continue;
		} else if (strnicmp(aptr, "@end", 4) == 0) {
			*inbuf.buf = '\0';								/* End of all headers */
			break;
		} else if (strnicmp(aptr, "@skip", 5) == 0) {
			Iskip = atoi(aptr+5);
			continue;
		} else if (strncmp(aptr, "@ ",2) == 0) {		/* Imbedded commands	*/
			LexInsText(aptr+2);
			continue;
		}

		if (! isalpha(*aptr)) break;						/* Number => we are out */
		if (strnicmp(aptr, "ZDATA", 5) == 0) {			/* Done */
			*inbuf.buf = '\0';
			break;
		} else if (strnicmp(aptr, "COLS", 4) == 0 || strnicmp(aptr, "NCOL", 4) == 0) {
			cols = atol(aptr+5);
		} else if (strnicmp(aptr, "ROWS", 4) == 0 || strnicmp(aptr, "NROW", 4) == 0) {
			rows = atol(aptr+5);
		} else if (strnicmp(aptr, "COMMENT", 7) == 0) {	
			aptr = aptr+8; while (isspace(*aptr)) aptr++;
			strscpy(ids, aptr, sizeof(ids));
		} else if (strnicmp(aptr, "XSCALE",  6) == 0) {
			xscale = (REAL) atof(aptr+7);
		} else if (strnicmp(aptr, "YSCALE",  6) == 0) {
			yscale = (REAL) atof(aptr+7);
		} else if (strnicmp(aptr, "XORIGIN", 7) == 0) {
			xorigin = (REAL) atof(aptr+8);
		} else if (strnicmp(aptr, "YORIGIN", 7) == 0) {
			yorigin = (REAL) atof(aptr+8);
		} else if (strnicmp(aptr, "XDATA",   5) == 0) {
			if (cols <= 0) {
				ERRprintf("ERROR: Must specify # of columns before the X values in matrix read\n");
				goto EveryExit;
			}
			xdata = calloc(cols, sizeof(*xdata));
			for (i=0; i<5; i++) *aptr++ = ' ';
			if (*aptr == ':') *aptr = ' ';
			if (readblock(xdata, cols, &inbuf, funit) != cols) {
				ERRprintf("ERROR: Unable to read all the col X values\n");
				goto EveryExit;
			}
		} else if (strnicmp(aptr, "YDATA",   5) == 0) {
			if (rows <= 0) {
				ERRprintf("ERROR: Must specify # of rows before the Y values in matrix read\n");
				goto EveryExit;
			}
			ydata = calloc(rows, sizeof(*ydata));
			for (i=0; i<5; i++) *aptr++ = ' ';
			if (*aptr == ':') *aptr = ' ';
			if (readblock(ydata, rows, &inbuf, funit) != rows) {
				ERRprintf("ERROR: Unable to read all the row Y values\n");
				goto EveryExit;
			}
		} else {
			if (inbuf.size > 80) inbuf.buf[80] = '\0';
			ERRprintf("ERROR: %s is an unrecognized keyword in MATRIX file format\n", aptr);
			if (errcnt++ < 10) continue;
			goto EveryExit;
		}
	}

/* Start by filling the inbuf so there is some data there */
	if (*inbuf.buf == '\0' && SysReadLongLine(&inbuf, funit, B_SKIP_BLANK | B_SKIP_COMMENTS | B_ALLOW_CONTINUES) <= 0) {
		ERRprintf("ERROR: Can't find any data to read\n");
		goto EveryExit;
	}

	if (cols <= 0) {									/* Must determine from data */
		zdata = read_unknown_block(&cols, MAX_SEEK_3D_COLS, 1, &inbuf, NULL);
		if (zdata != NULL) { free(zdata); zdata = NULL; }
		if (cols <= 0) {
			ERRprintf("ERROR: Nothing appears to be there to read for Z\n");
			goto EveryExit;
		}
	}
	if (rows > 0) {
		npt = min(rows, LastRow) * cols;						/* Expected # of points needed to read */
		zdata = calloc(npt, sizeof(*zdata));				/* Create the space							*/
		npt = readblock(zdata, npt, &inbuf, funit);		/* And see how many really read			*/
		nrows = npt/cols;											/* Actual # of rows							*/
	} else {
		zdata = read_unknown_block(&nrows, min(MAX_SEEK_3D_ROWS, LastRow), cols, &inbuf, funit);
	}
	if (rows > 0 && nrows != min(rows,LastRow)) {
		ERRprintf("WARNING: File specified rows=%d, but only found %d\n", rows, nrows);
		rows = min(rows, nrows);
	} else if (nrows <= 0) {
		ERRprintf("ERROR: Can't seem to find enough data for one row\n");
		goto EveryExit;
	} else {
		rows = nrows;
	}
	
	FirstCol = min(cols, max(1, FirstCol)) - 1;	/* Make look as array indices */
	LastCol  = min(cols, max(1, LastCol))  - 1;
	FirstRow = min(rows, max(1, FirstRow)) - 1;
	LastRow  = min(rows, max(1, LastRow))  - 1;
	
	if (! GVAllocSurface(UseCurve, GVF_USER, LastRow-FirstRow+1, LastCol-FirstCol+1)) {
		ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
		goto EveryExit;
	} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
		goto EveryExit;
	}
	surface = (SURFACE *) *varptr;
	strscpy(surface->ids, ids, sizeof(surface->ids) );

/* Surface is stored in COLUMN major format, all of column 1 first, not row */
/* Must be flipped at this step */
	z = surface->z;
	for (i=FirstCol; i<=LastCol; i++) {
		for (j=FirstRow; j<=LastRow; j++) {
			*z++ = zdata[j*cols+i];
		}
	}

	if (xdata != NULL) {
		memcpy(surface->x, xdata+FirstCol, (LastCol-FirstCol+1)*sizeof(*xdata));
	} else {
		for (i=0; i<LastCol-FirstCol+1; i++) surface->x[i] = xorigin+(i+FirstCol)*xscale;
	}

	if (ydata != NULL) {
		memcpy(surface->y, ydata+FirstRow, (LastRow-FirstRow+1)*sizeof(*ydata));
	} else {
		for (i=0; i<LastRow-FirstRow+1; i++) surface->y[i] = yorigin+(i+FirstRow)*yscale;
	}

EveryExit:
	if (xdata != NULL) free(xdata);
	if (ydata != NULL) free(ydata);
	if (zdata != NULL) free(zdata);
	if (inbuf.buf != NULL) free(inbuf.buf);

	return (surface);
}


/* ===========================================================================
-- Usage: SURFACE *GptCameraRead(FILE *funit, int FirstCol, int LastCol,
--                          int FirstRow, int LastRow, char *UseCurve,
--                          int rd_flags);
--
-- Inputs: funit - open file containing information
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
--
-- Notes: 
--    (1) Recognized header keywords:
--          MDF100          ! Expected header name
--          256 256         ! Rows/columns in plane
--          1.5000 8.0000   ! Full X, Full Y (mm)
--          9.7500          ! Z height
--          1.0411 4.0000   ! Center of measurement X,Y (mm)
--          0.0000          ! Detector gain (dB)
--          1               ! Number of averages
--          153.5000        ! Average dark current
--          n1 n2 ... n10   ! Data - 10 values per line
--    (2) Data fills Z array as it is stored.  Okay if square matrix.
--        Don't have data for others so unsure if it makes a difference.
--    (3) Row/Col specifiers are ignored
--
-- The matrix is read as it appears in a file -- row major format.  However,
-- it is converted to column major before being stored as Z in the surface,
-- as this is the standard in GENPLOT.  For most users, this is irrelevent.
=========================================================================== */
char *GetCameraLine(char *buf, int isize, FILE *funit, char *ermsg) {
	char *aptr;
	
	while (TRUE) {
		if (fgets(buf, isize, funit) == NULL) {
			ERRprintf("ERROR: Unexpected end of camera file (%s)\n", ermsg);
			return(NULL);
		}
		if ( (aptr = strchr(buf, '\n')) != NULL) *aptr = '\0';
		aptr = buf;
		while (isspace(*aptr)) aptr++;
		if (*buf != ';' && *buf != '\0') break;
	}
	return(buf);
}

SURFACE *GptCameraRead(FILE *funit, int FirstCol, int LastCol, int FirstRow, int LastRow, char *UseCurve, int rd_flags) {

	int i,j, nrow,ncol, iplane, n_Avg;
	int *z=NULL;
	double x_width, y_width, x_center, y_center, db_Gain;
	char inbuf[256], *aptr;						/* Input buffer and pointers	*/

/* Variables that get linked for other purposes */
	static BOOL First = TRUE;
	static double zPos=0, zBase=0;

	int   type;										/* Type of variable (GVLink)	*/
	void **varptr;
	SURFACE *surface=NULL;						/* Temporary array pointer		*/

/* ------------------------------------------------------------------
-- Scan the headers.  Allow headers either with the : terminator,
-- or without.  If without, next character must be insignificant
------------------------------------------------------------------- */
	iplane = LexGetInt(1, "Which plane (1): ");
	while (iplane > 0) {							/* Skip over unused planes */
		if (GetCameraLine(inbuf, sizeof(inbuf), funit, "finding plane") == NULL) return(NULL);
		if (strnicmp(inbuf, "MDF100", 6) == 0) iplane--;
	}

/* Get the number of rows and columns */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "row/column") == NULL) return(NULL);
	ncol = strtol(inbuf, &aptr, 10);	nrow = strtol(aptr, NULL, 10);

/* X scan and Y scan distances (mm) */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "scan size") == NULL) return(NULL);
	x_width = strtod(inbuf, &aptr); y_width = strtod(aptr, NULL);

/* Z position line */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "Z position") == NULL) return(NULL);
	zPos = strtod(inbuf, &aptr);
	
/* Center of measurement */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "scan center") == NULL) return(NULL);
	x_center = strtod(inbuf, &aptr); y_center = strtod(aptr, NULL);

/* Detector gain (dB) */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "dB gain") == NULL) return(NULL);
	db_Gain = strtod(inbuf, &aptr);

/* Number of averages */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "dB gain") == NULL) return(NULL);
	n_Avg = strtol(inbuf, &aptr, 10);

/* Average dark signal */
	if (GetCameraLine(inbuf, sizeof(inbuf), funit, "dark signal") == NULL) return(NULL);
	zBase = strtod(inbuf, &aptr);

/* Read in all the data first as integers */
	z = calloc(nrow*ncol, sizeof(*z));
	*(aptr=inbuf) = '\0';											/* So I read the first line */
	for (i=0; i<nrow*ncol; i++) {
		while (isspace(*aptr)) aptr++;
		if (*aptr == '\0') {
			if (fgets(inbuf, sizeof(inbuf), funit) == NULL) {
				ERRprintf("ERROR: Ran out of file before I found all the data\n");
				free(z);
				return(NULL);
			}
			if ( (aptr = strchr(inbuf, '\n')) != NULL) *aptr = '\0';
			aptr = inbuf;
		}
		z[i] = strtol(aptr, &aptr, 10);
	}

/* Now, just transfer to the surface itself */
	if (! GVAllocSurface(UseCurve, GVF_USER, nrow, ncol)) {
		ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
		free(z); return(NULL);
	} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
		free(z); return(NULL);
	}
	surface = (SURFACE *) *varptr;
	sprintf(surface->ids, "Beamscan: %.3f", zPos);

/* Copy the data - properly shifted - into the Z array */
/* Unfortunately, stored in row major, need to put in column major */
	for (i=0; i<ncol; i++) {
		for (j=0; j<nrow; j++) {
			surface->z[nrow*i+j] = (FLOAT) (z[ncol*j+i] - zBase);
		}
	}
	free(z);

/* Create the X and Y data sets */
	for (i=0; i<ncol; i++) surface->x[i] = (FLOAT) (x_center + x_width/ncol*(i-ncol/2));
	for (i=0; i<nrow; i++) surface->y[i] = (FLOAT) (y_center + y_width/nrow*(i-nrow/2));
	if (First) {
		GVLinkDouble("zPos", GVF_USER, &zPos);
		GVLinkDouble("zBase", GVF_USER, &zBase);
		First = FALSE;
	}
	
	return (surface);
}


/* ===========================================================================
-- Header information required for bitmap file manipulation.
-- Maintain machine independence as much as possible
=========================================================================== */
#pragma pack(2)

typedef struct _GPT_BITMAPFILEHEADER {
	UINT16 bfType;								/* Must be "BM" == 19778					*/
	UINT32 bfSize;								/* File size in bytes						*/
	UINT16 bfReserved[2];					/* Unused - must be zero					*/
	UINT32 bfOffBits;							/* Offset to where bitmap data starts	*/
} GPT_BITMAPFILEHEADER;

typedef struct _GPT_BITMAPINFOHEADER {
	UINT32 biSize;								/* Size of this header in bytes			*/
	INT32		biWidth;							/* Width of image in pixels				*/
	INT32		biHeight;						/* Height of image in pixels				*/
	UINT16	biPlanes;						/* # planes of target device (must=0)	*/
	UINT16	biBitCount;						/* Number of bits per pixel				*/
	UINT32	biCompression;					/* Compression type (0 => none)			*/
	UINT32	biSizeImage;					/* Bytes in image data (0 ok if uncompressed) */
	UINT32	biXPelsPerMeter;				/* Obvious, but usually zero				*/
	UINT32	biYPelsPerMeter;				/* Obvious, but usually zero				*/
	UINT32	biClrUsed;						/* # colors used. 0 => use biBitCount	*/
	UINT32	biClrImportant;				/* # colors important.  0 => all			*/
} GPT_BITMAPINFOHEADER;

typedef struct _GPT_RGBQUAD {				/* Order of bytes for a color index		*/
	BYTE rgbs[4];
} GPT_RGBQUAD;

#pragma pack()

/* ===========================================================================
-- Usage: int GptBitmapWrite(FILE *funit, SURFACE *surface, BOOL silent);
--
-- Inputs: funit - open file containing information
--         surface - pointer to surface structure
--
-- Output: writes the file with necessary headers
--
-- Returns: 0 if successful, -1 otherwise
=========================================================================== */
static struct {
	REAL zmin, zmax;
	int  nrow, ncol;
	GVP_CONTINUUMPALETTE palette;
} BitMapOptions;

/* Set up values of bitmap options (so can be handled before call) */
static void InitBitMapOptions(SURFACE *surf) {
	
	if (surf != NULL) {
		ArrayMinMax(surf->z, surf->nrow*surf->ncol, &BitMapOptions.zmin, &BitMapOptions.zmax);
		BitMapOptions.nrow = surf->nrow;
		BitMapOptions.ncol = surf->ncol;
	} else {
		BitMapOptions.zmin = 0; BitMapOptions.zmax = 1;
		BitMapOptions.nrow = 480;
		BitMapOptions.ncol = 640;
	}
	BitMapOptions.palette = GV_PAL_DEFAULT;
	return;
}

/* Return TRUE if this is a bitmap option that was handled */
static BOOL WasBitMapOption(char *token) {

	if (LexEqual(token, "-range", 2)) {
		BitMapOptions.zmin = LexGetReal(BitMapOptions.zmin, "Minimum range (data limit): ");
		BitMapOptions.zmax = LexGetReal(BitMapOptions.zmax, "Maximum range (data limit): ");
	} else if (LexEqual(token, "-size", 2)) {
		BitMapOptions.ncol = LexGetInt(BitMapOptions.ncol, "Number of columns (data): ");
		BitMapOptions.nrow = LexGetInt(BitMapOptions.nrow, "Number of rows (data): ");
	} else if (LexEqual(token, "-colormap", 6) || LexEqual(token, "-palette", 4) || LexEqual(token, "-rainbow", 4)) {
		BitMapOptions.palette = GVSelectContinuumPalette(NULL, GV_PAL_DEFAULT);
	} else {
		return(FALSE);
	}
	return(TRUE);
}

/* Actual writing routine now */
/* -- Modification 3/15/05 - change so bitmaps are always written such
 * than increasing X goes left to right and increasing Y from bottom to
 * top
 * */
int GptBitmapWrite(FILE *funit, SURFACE *surf, BOOL silent) {

	int i,j,ii,jj,ineed, nrow,ncol, nrowdim,ncoldim, rgb;
	UCHAR *data;
	REAL *z,zval, zmin, zmax;
	BOOL XRev, YRev;								/* Reverse direction due to data order? */
	GPT_BITMAPFILEHEADER  bmfh;
	GPT_BITMAPINFOHEADER *bmih;
	GVP_CONTINUUMPALETTE palette;

	if (surf == NULL) return(-1);				/* Trivial failure */

/* Recover the options scanned earlier */
	zmin = BitMapOptions.zmin;
	zmax = BitMapOptions.zmax;
	nrow = BitMapOptions.nrow;
	ncol = BitMapOptions.ncol;
	palette = BitMapOptions.palette;

/* Need real dimensioned size also */
	nrowdim = surf->nrow;				/* Dimensioned size */
	ncoldim = surf->ncol;
	XRev = surf->x[1] < surf->x[0];
	YRev = surf->y[1] > surf->y[0];	/* Bitmaps start from top down in painting */

/* Constrain number of columns to a multiple of 4 (required) */
	ncol = (ncol+2) & 0xFFFC;					/* Will force it closest multiple of 4 */
	if (! silent) TTYprintf("Creating bitmap %d x %d\n", ncol, nrow);

	ineed = sizeof(*bmih) + 3*nrow*ncol;	/* Total space required for header and data */
	if ( (bmih = calloc(ineed, 1)) == NULL) {
		ERRprintf("ERROR: Unable to allocate a bitmap info header structure\n");
		return(-1);
	}
	bmih->biSize = sizeof(*bmih);				/* Size of this header in bytes			*/
	bmih->biWidth = ncol;						/* Width of image in pixels				*/
	bmih->biHeight = nrow;						/* Height of image in pixels				*/
	bmih->biPlanes = 1;							/* # planes of target device (must=0)	*/
	bmih->biBitCount = 24;						/* Number of bits per pixel				*/
	bmih->biCompression = 0;					/* Compression type (0 => none)			*/
	bmih->biSizeImage = nrow * ncol * 3;	/* Image data Bytes (0 ok if uncompressed) */
	bmih->biXPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biYPelsPerMeter = 0;					/* Obvious, but usually zero				*/
	bmih->biClrUsed = 0;							/* # colors used. 0 => use biBitCount	*/
	bmih->biClrImportant = 0;					/* # colors important.  0 => all			*/

	z = surf->z;
	data = ((BYTE *) bmih) + sizeof(*bmih);	/* Where does data start */
	for (i=0; i<nrow; i++) {
		ii = YRev ? nrow-1-i : i;
		ii = (ii*(nrowdim-1)+(nrow-1)/2)/(nrow-1);		/* 0 equals zero nrow-1 = nrowdim-1 */
		for (j=0; j<ncol; j++) {
			jj = XRev ? ncol-1-j : j;
			jj = (jj*(ncoldim-1)+(ncol-1)/2)/(ncol-1);	/* 0 equals zero nrow-1 = nrowdim-1 */
			zval = z[jj*surf->nrow+ii];
			rgb = GVSelectContinuumColor(zval, zmin, zmax, palette);	/* zminz,zmaxz defined in gptdef.h */
			*data++ = B_FROM_RGB(rgb);			/* Set into correct color format */
			*data++ = G_FROM_RGB(rgb);
			*data++ = R_FROM_RGB(rgb);
		}
	}

	bmfh.bfType = 19778;
	bmfh.bfSize = sizeof(bmfh)+ineed;
	bmfh.bfReserved[0] = bmfh.bfReserved[1] = 0;
	bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);
	fwrite(&bmfh, 1, sizeof(bmfh), funit);
	fwrite(bmih, 1, ineed, funit);
	free(bmih);
	return(0);
}


/* ===========================================================================
-- Usage: SURFACE *GptBitmapRead(FILE *funit, int FirstCol, int LastCol,
--                          int FirstRow, int LastRow, char *UseCurve,
--                          int rd_flags);
--
-- Inputs: funit - open file handle (binary) containing a Windows BITMAP file
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
=========================================================================== */
static SURFACE *ReadCommonBMPFormat(GPT_BITMAPFILEHEADER *bmfh, GPT_BITMAPINFOHEADER *bmih, UCHAR *bmdata,
									  int FirstCol, int LastCol, int FirstRow, int LastRow,
									  char *UseCurve, int rd_flags) {

	int i,j;
	int bcount, boff, rgb_mode, rows, cols, col_length, extra;
	char ids[DFLT_STR_SIZE]="! ";				/* IDS string						*/
	int   type;										/* Type of variable (GVLink)	*/
	REAL *z;
	void **varptr;
	SURFACE *surface=NULL;						/* Temporary array pointer		*/

/* Start by printing DEBUG and basic information when requested */
	if (rd_flags & RD_DEBUG) {
		if (bmfh != NULL) {
			TTYprintf("Bitmap header structure:\n");
			TTYprintf(" bfType:          0x%4.4x (expect 0x4d42)\n", bmfh->bfType);
			TTYprintf(" bfSize:          %d\n", bmfh->bfSize);
			TTYprintf(" bfReserved[0]:   %d\n", bmfh->bfReserved[0]);
			TTYprintf(" bfReserved[1]:   %d\n", bmfh->bfReserved[1]);
			TTYprintf(" bfOffBits:       %d\n", bmfh->bfOffBits);
		}
		TTYprintf(" biSize:          %d\n", bmih->biSize);
		TTYprintf(" biWidth:         %d\n", bmih->biWidth);
		TTYprintf(" biHeight:        %d\n", bmih->biHeight);
		TTYprintf(" biPlanes:        %d\n", bmih->biPlanes);
		TTYprintf(" biBitCount:      %d\n", bmih->biBitCount);
		TTYprintf(" biCompression:   %d\n", bmih->biCompression);
		TTYprintf(" biSizeImage:     %d\n", bmih->biSizeImage);
		TTYprintf(" biXPelsPerMeter: %d\n", bmih->biXPelsPerMeter);
		TTYprintf(" biYPelsPerMeter: %d\n", bmih->biYPelsPerMeter);
		TTYprintf(" biClrUsed:       %d\n", bmih->biClrUsed);
		TTYprintf(" biClrImportant:  %d\n", bmih->biClrImportant);
	}

/* Decide what we are able to interpret, and what we will have to ignore */
	switch (bmih->biBitCount) {
		case 8:
			bcount = 1; break;
		case 24:
			bcount = 3; break;
		case 32:
			bcount = 4; break;
		default:
			ERRprintf("ERROR: Only 8/24 bit oriented bitmaps handled (biBitCount=%d)\n", bmih->biBitCount);
			return(NULL);
	}
	if (bmih->biWidth < 0 || bmih->biHeight < 0) {
		TTYprintf("WARNING: Reported image height or width was negative - inverting so can proceed\n");
		if (bmih->biWidth  < 0) bmih->biWidth  = -bmih->biWidth;
		if (bmih->biHeight < 0) bmih->biHeight = -bmih->biHeight;
	}

	cols = bmih->biWidth;								/* Copy this now, so can potentially modify */
	rows = bmih->biHeight;
	col_length = bcount*cols;							/* Bytes in each column (to go to next row) */

	if (bmih->biCompression != 0) {
		ERRprintf("ERROR: Only uncompressed bitmap can be read currently (biCompression=%d)\n", bmih->biCompression);
		return(NULL);
	} else if (bmih->biSizeImage != 0 && (INT32) bmih->biSizeImage != bmih->biWidth*bmih->biHeight*bcount) {
		if ((INT32) bmih->biSizeImage < cols*rows*bcount) {
			ERRprintf("ERROR: Reported image size smaller than width/height (%d != %dx%dx%d)\n", bmih->biSizeImage, cols, rows, bcount);
			return(NULL);
		} else {
			extra = bmih->biSizeImage-rows*cols*bcount;						/* Extra bytes in image */
			if (rows*(extra/rows) == extra && extra <= 3*rows) {			/* Okay, so have just more per row */
				col_length = col_length + extra/rows;
				if (! (rd_flags & RD_SILENT)) TTYprintf("INFO: Apparently have %d extra bytes stored per row.  Adjusting.\n", extra/rows);
			} else {
				if (! (rd_flags & RD_SILENT)) ERRprintf("WARNING: Reported image size larger than width/height (%d != %dx%dx%d).  Trailing data ignored.\n", bmih->biSizeImage, bmih->biWidth, bmih->biHeight, bcount);
			}
		}
	}

	if (! (rd_flags & RD_SILENT)) TTYprintf("INFO: Bitmap reported size is %d x %d\n", bmih->biWidth, bmih->biHeight);

/* Decide how much to copy over */
	FirstCol = min(cols, max(1, FirstCol)) - 1;	/* Make look as array indices */
	LastCol  = min(cols, max(1, LastCol))  - 1;
	FirstRow = min(rows, max(1, FirstRow)) - 1;
	LastRow  = min(rows, max(1, LastRow))  - 1;

	if (! GVAllocSurface(UseCurve, GVF_USER, LastRow-FirstRow+1, LastCol-FirstCol+1)) {
		ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
		return(NULL);
	} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
		return(NULL);
	}
	surface = (SURFACE *) *varptr;
	strscpy(surface->ids, ids, sizeof(surface->ids) );

/* Surface is stored in COLUMN major format, all of column 1 first, not row */
/* Must be flipped at this step since bitmap is row by row */
	z = surface->z;
	if (bcount == 1) {
		rgb_mode = 0; boff = 0;
	} else if (rd_flags & RD_BLUE_ONLY) {
		rgb_mode = 0; boff = 0;
	} else if (rd_flags & RD_GREEN_ONLY) {
		rgb_mode = 0; boff = 1;
	} else if (rd_flags & RD_RED_ONLY) {
		rgb_mode = 0; boff = 2;
	} else if (rd_flags & RD_CHROMA_ONLY) {
		rgb_mode = 0; boff = 3;
	} else {
		rgb_mode = 1; boff = 0;
	}

	for (i=FirstCol; i<=LastCol; i++) {
		for (j=FirstRow; j<=LastRow; j++) {
			if (rgb_mode == 0) {
				*z++ = bmdata[col_length*j+bcount*i+boff];
			} else {
				*z++ = (float) (bmdata[col_length*j+bcount*i] + bmdata[col_length*j+bcount*i+1] + bmdata[col_length*j+bcount*i+2]);
			}
		}
	}

/* Dummy fill-in of the X,Y coordinate */
	for (i=0; i<LastCol-FirstCol+1; i++) surface->x[i] = (REAL) (i+FirstCol);
	for (i=0; i<LastRow-FirstRow+1; i++) surface->y[i] = (REAL) (i+FirstRow);

	return(surface);
}


/* ===========================================================================
-- Usage: SURFACE *GptBitmapRead(FILE *funit, int FirstCol, int LastCol,
--                          int FirstRow, int LastRow, char *UseCurve,
--                          int rd_flags);
--
-- Inputs: funit - open file handle (binary) containing a Windows BITMAP file
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
=========================================================================== */
SURFACE *GptBitmapRead(FILE *funit, int FirstCol, int LastCol,
							  int FirstRow, int LastRow, char *UseCurve, int rd_flags) {

	int ineed, igot;
	UCHAR *zdata=NULL;

	SURFACE *surface=NULL;						/* Temporary array pointer		*/

	GPT_BITMAPFILEHEADER bmfh;
	GPT_BITMAPINFOHEADER bmih;

	if (fread(&bmfh, sizeof(bmfh), 1, funit) != 1 || bmfh.bfType != 19778) {
		ERRprintf("ERROR: Bitmap file header read failed, or not marked as a bitmap\n");
		return(NULL);
	} else if (fread(&bmih, sizeof(bmih), 1, funit) != 1) {
		ERRprintf("ERROR: Bitmap information header could not be read\n");
		return(NULL);
	}

/* How many bytes in the image to read? */
	ineed = bmih.biSizeImage;
	if (ineed == 0) ineed = bmih.biWidth * bmih.biHeight * bmih.biBitCount / 8;	/* Calculate # of bytes */

	zdata = malloc(ineed);							/* Read it all in at once	*/
	fseek(funit, bmfh.bfOffBits, SEEK_SET);	/* Move to the data itself */
	if ( (igot = (int) fread(zdata, 1, ineed, funit)) != ineed) {
		ERRprintf("ERROR: Unable to read all the bitmap data (only %d bytes of %d)\n", igot, ineed);
		free(zdata);
		return(NULL);
	}

/* Use common read routine to interpret the bitmap (hopefully) */
	surface = ReadCommonBMPFormat(&bmfh, &bmih, zdata, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags);

/* Clean up and return successful */
	free(zdata);
	return (surface);
}


/* ===========================================================================
-- Usage: SURFACE *GptAVIRead(char *fname, int FirstCol, int LastCol,
--                            int FirstRow, int LastRow, char *UseCurve,
--                            int rd_flags, int iframe);
--
-- Inputs: fname - name of an existing file (has been checked)
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--         iframe            - which frame from the file
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
=========================================================================== */
SURFACE *GptAVIRead(char *fname, int FirstCol, int LastCol,
						  int FirstRow, int LastRow, char *UseCurve, int rd_flags, int iframe) {

#ifndef MSC70
		ERRprintf("ERROR: AVI file read is possible only with the Windows API.  Sorry\n");
		return(NULL);
#else

	int rc;
	SURFACE *surface=NULL;						/* Temporary array pointer		*/
	BOOL First = TRUE;

	PAVISTREAM pavi;
	AVISTREAMINFO psi;
	void *pgf;

	GPT_BITMAPINFOHEADER *bmih;
	BITMAPINFOHEADER;
	void *bmdata;

	static BOOL first=TRUE;
	static int FrameCount=0;
	static double FrameRate=29.97;

	if (First) {
		AVIFileInit();							/* Initialize the AVI interface */
		GVLinkInt   ("$FrameCount", GVF_INTERNAL | GVF_CONSTANT | GVF_NODELETE, &FrameCount);
		GVLinkDouble("$FrameRate",  GVF_INTERNAL | GVF_CONSTANT | GVF_NODELETE, &FrameRate);
		First = FALSE;
	}

/* A simple check first */
	if (sizeof(GPT_BITMAPINFOHEADER) != sizeof (BITMAPINFOHEADER)) {
		ERRprintf("ERROR: No longer agree with Microsoft about bitmap header - have to recode\n");
		return(NULL);
	}
		
/* Open the AVI file and proceed */
	if ( (rc = AVIStreamOpenFromFile(&pavi, fname, streamtypeVIDEO, 0, OF_READ, NULL)) != 0) {
		ERRprintf("ERROR: Unable to open the video stream from %s (%x)\n", fname, rc);
		return(NULL);
	} else if ( (rc = AVIStreamInfo(pavi, &psi, sizeof(psi))) != 0) {
		ERRprintf("ERROR: Error reading the stream information (rc=%d)\n", rc);
		AVIStreamRelease(pavi);
		return(NULL);
	}

/* Set the values now returned */
	FrameCount = psi.dwLength;
	FrameRate  = (1.0*psi.dwRate)/psi.dwScale;

	if (rd_flags & RD_DEBUG) {
		TTYprintf("fccType: %c%c%c%c\n", (psi.fccType>>24)&0xFF, (psi.fccType>>16)&0xFF, (psi.fccType>>8)&0xFF, psi.fccType&0xFF);
		TTYprintf("fccHandler: %c%c%c%c\n", (psi.fccHandler>>24)&0xFF, (psi.fccHandler>>16)&0xFF, (psi.fccHandler>>8)&0xFF, psi.fccHandler&0xFF);
		TTYprintf("dwFlags: %4.4X\n", psi.dwFlags);
		TTYprintf("dwCaps: %4.4X\n", psi.dwCaps);
		TTYprintf("wPriority: %2.2X\n", psi.wPriority);
		TTYprintf("wLanguage: %2.2X\n", psi.wLanguage);
		TTYprintf("dwScale: %d\n", psi.dwScale);
		TTYprintf("dwRate: %d\n", psi.dwRate);
		TTYprintf("Playback rate: %.2f\n", (1.0*psi.dwRate)/psi.dwScale);
		TTYprintf("dwStart: %d\n", psi.dwStart);
		TTYprintf("dwLength: %d\n", psi.dwLength);
		TTYprintf("dwInitialFrames: %d\n", psi.dwInitialFrames);
		TTYprintf("dwSuggestedBufferSize: %d\n", psi.dwSuggestedBufferSize);
		TTYprintf("dwQuality: %d\n", psi.dwQuality);
		TTYprintf("dwSampleSize: %d\n", psi.dwSampleSize);
//		RECT rcFrame;
		TTYprintf("dwEditCount: %d\n", psi.dwEditCount);
		TTYprintf("dwFormatChangeCount: %d\n", psi.dwFormatChangeCount);
		TTYprintf("szName: %s\n", psi.szName);
	}

/* Now, work with "clean-up" decoded information */
	if ( (pgf = AVIStreamGetFrameOpen(pavi, NULL)) == NULL) {
		ERRprintf("ERROR: Unable to get initialize frame extraction\n");
		AVIStreamRelease(pavi);
		return(NULL);
	}

/* Frame # is 0-based, but allow specifying last frame as $framecount for backward compatibility */
/* Prior to 7/2011, frame number was 1-based instead of 0-based.  Need to be consistent. */
	if (iframe < 0) iframe = 0;
	if (iframe == FrameCount) iframe--;

/* And grab it */
	if ( (bmih = (GPT_BITMAPINFOHEADER *) AVIStreamGetFrame(pgf, iframe)) == NULL) {
		ERRprintf("ERROR: OOPS - did not get the frame expected\n");
		AVIStreamGetFrameClose(pgf);
		AVIStreamRelease(pavi);
		return(NULL);
	}
	if (bmih->biSize != sizeof(*bmih)) {
		ERRprintf("WARNING: Reported size of bitmap info header (%d) does not equal expected size (%d)\n", bmih->biSize, sizeof(*bmih));
	}

/* Use common read routine to interpret the bitmap (hopefully) */
	bmdata = ((UCHAR *) bmih) + bmih->biSize + 4*bmih->biClrUsed ;
	surface = ReadCommonBMPFormat(NULL, bmih, bmdata, FirstCol, LastCol, FirstRow, LastRow, UseCurve, rd_flags);

/* And clean-up to return */
	AVIStreamGetFrameClose(pgf);
	AVIStreamRelease(pavi);
	return(surface);

#endif				/* MSC60 */
}


/* ===========================================================================
-- Usage: SURFACE *GptFLIRRead(char *fname, FILE *funit, int FirstCol, int LastCol,
--                          int FirstRow, int LastRow, char *UseCurve,
--                          int rd_flags, int iframe);
--
-- Inputs: fname - extension is scanned to determine if a .flf, .fff, or .seq
--         funit - open file handle (binary) containing a Windows BITMAP file
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--         iframe            - which frame from the file
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
=========================================================================== */
SURFACE *GptFLIRRead(char *fname, FILE *funit, int FirstCol, int LastCol,
							int FirstRow, int LastRow, char *UseCurve, int rd_flags, int iframe) {

	int i,j;

	int ineed, igot, rows, cols;
	float *zdata=NULL;
	unsigned short *izdata=NULL;
	char ext[PATH_MAX];

	char ids[DFLT_STR_SIZE]="! ";				/* IDS string						*/
	int   type;										/* Type of variable (GVLink)	*/
	REAL *z;
	void **varptr;
	SURFACE *surface=NULL;						/* Temporary array pointer		*/

	enum {FPF, SEQUENCE, IR} FLIR_Type;

#pragma pack(2)									/* Data structure must be at most SHORT aligned */
	typedef struct _FPF_HEADER {
		char fpfID[32];							/* FPF_IMAGE_DATA_T */
		unsigned long	version;
		unsigned long	pixelOffset;
		unsigned short	ImageType;
		unsigned short	pixelFormat;
		unsigned short	xSize;
		unsigned short	ySize;
		unsigned long	trig_count;
		unsigned long	frame_count;
		long				spare_fpf_image_data[16];

		#define	FPF_LEN	(31)
		char camera_name[FPF_LEN+1];	/* FPF_CAMDATA_T */
		char camera_partn[FPF_LEN+1];
		char camera_sn[FPF_LEN+1];
		float	camera_range_tmin;
		float	camera_range_tmax;
		char lens_name[FPF_LEN+1];
		char lens_partn[FPF_LEN+1];
		char lens_sn[FPF_LEN+1];
		char filter_name[FPF_LEN+1];
		char filter_partn[FPF_LEN+1];
		char filter_sn[FPF_LEN+1];
		long spare_FPF_CAMDATA[16];
		
		float	emissivity;					/* spare_FPF_OBJECT_PAR */
		float objectDistance;
		float ambTemp;
		float atmTemp;
		float relHum;
		float compuTao;
		float estimTao;
		float refTemp;
		float extOptTemp;
		float extOptTrans;
		long	spare_FPF_OBJECT_PAR[16];
		
		int	Year;							/* FPF_DATETIME_T */
		int	Month;
		int	Day;
		int	Hour;
		int	Minute;
		int	Second;
		int	Millisecond;
		long	spare_FPF_DATETIME[16];

		float tMinCam;						/* FPF_SCALING_T */
		float	tMaxCam;
		float tMinCalc;
		float tMaxCalc;
		float tMinScale;
		float tMaxScale;
		long	spare_FPF_SCALING[16];

		long spare_FPF_IMAGE_DATA[32];				
	} FPF_HEADER;

	FPF_HEADER header;
#pragma pack()

/* Decide what type of file it is */
	SysSplitPath(fname, NULL, NULL, ext);
	if (stricmp(ext, ".seq") == 0) {
		FLIR_Type = SEQUENCE;
	} else if (stricmp(ext, ".fff") == 0) {
		FLIR_Type = IR;
	} else {
		FLIR_Type = FPF;
	}

/* Switch frame from 1-based to 0-based */
	iframe = (iframe <= 1) ? 0 : iframe-1;

	switch (FLIR_Type) {
		case FPF:
			if (sizeof(FPF_HEADER) != 892) {
				ERRprintf("Header size reported is %d - should be 892\n", sizeof(FPF_HEADER));
			}

			if (fread(&header, sizeof(header), 1, funit) != 1) {
				ERRprintf("ERROR: Unable to read header of FLIR file\n");
				return(NULL);
			} else if (strcmp(header.fpfID, "FPF Public Image Format") != 0) {
				ERRprintf("ERROR: FLIR file header not marked properly (%s)\n", header.fpfID);
				return(NULL);
			}

			if (rd_flags & RD_DEBUG) {
				TTYprintf("FLIR Public Image Format structure:\n");
				TTYprintf(" fpfID:           %s\n", header.fpfID);
				TTYprintf(" version:         %d\n", header.version);
				TTYprintf(" pixelOffset      %x\n", header.pixelOffset);
				TTYprintf(" ImageType:       %d\n", header.ImageType);
				TTYprintf(" pixelFormat:     %d\n", header.pixelFormat);
				TTYprintf(" xSize x ySize:   %d x %d\n", header.xSize, header.ySize);
				TTYprintf(" trig_count:      %d\n", header.trig_count);
				TTYprintf(" frame_count:     %d\n", header.frame_count);

				TTYprintf(" Camera info:     \"%s\"  \"%s\"  \"%s\"\n", header.camera_name, header.camera_partn, header.camera_sn);
				TTYprintf(" Camera T range:  %f %f\n", header.camera_range_tmin, header.camera_range_tmax);
				TTYprintf(" Lens info:       \"%s\"  \"%s\"  \"%s\"\n", header.lens_name, header.lens_partn, header.lens_sn);
				TTYprintf(" Filter info:     \"%s\"  \"%s\"  \"%s\"\n", header.filter_name, header.filter_partn, header.filter_sn);

				TTYprintf(" Emissivity:      %f\n", header.emissivity);
				TTYprintf(" objectDistance:  %f\n", header.objectDistance);
				TTYprintf(" ambTemp:         %f\n", header.ambTemp);
				TTYprintf(" relHum:          %f\n", header.relHum);
				TTYprintf(" compuTao:        %f\n", header.compuTao);
				TTYprintf(" estimTao:        %f\n", header.estimTao);
				TTYprintf(" refTemp:         %f\n", header.refTemp);
				TTYprintf(" extOptTemp:      %f\n", header.extOptTemp);
				TTYprintf(" extOptTrans:     %f\n", header.extOptTrans);
				TTYprintf(" Year/Month/Day:  %d/%d/%d\n", header.Year, header.Month, header.Day);
				TTYprintf(" Hour/Min/Sec/ms: %d:%2.2d:%2.2d.%3.3d\n", header.Hour, header.Minute, header.Second, header.Millisecond);
		
				TTYprintf(" tMinCam:         %f\n", header.tMinCam);
				TTYprintf(" tMaxCam:         %f\n", header.tMaxCam);
				TTYprintf(" tMinCalc:        %f\n", header.tMinCalc);
				TTYprintf(" tMaxCalc:        %f\n", header.tMaxCalc);
				TTYprintf(" tMinScale:       %f\n", header.tMinScale);
				TTYprintf(" tMaxScale:       %f\n", header.tMaxScale);
			}

			cols = header.xSize;
			rows = header.ySize;
			ineed = rows*cols;									/* Total number of bytes	*/
			zdata = calloc(ineed, sizeof(*zdata));			/* Read it all in at once	*/
			fseek(funit, header.pixelOffset, SEEK_SET);	/* Move to the data itself */
			if ( (igot = (int) fread(zdata, sizeof(*zdata), ineed, funit)) != ineed) {
				ERRprintf("ERROR: Unable to read all the bitmap data (only %d bytes of %d)\n", igot, ineed);
				free(zdata);
				return(NULL);
			}
			break;

		case IR:
			cols = 320;												/* Basically assume */
			rows = 240;												/* Don't know enough otherwise */
			ineed = rows*cols;									/* Total number of bytes	*/
			izdata = calloc(ineed, sizeof(*izdata));		/* Read it all in at once	*/
			fseek(funit, 544, SEEK_SET);						/* Move to the data itself */
			if ( (igot = (int) fread(izdata, sizeof(*izdata), ineed, funit)) != ineed) {
				ERRprintf("ERROR: Unable to read all the bitmap data (only %d bytes of %d)\n", igot, ineed);
				free(izdata);
				return(NULL);
			}
			break;
			
		case SEQUENCE:
			cols = 320;												/* Basically assume */
			rows = 240;												/* Don't know enough otherwise */
			ineed = rows*cols;									/* Total number of bytes	*/
			izdata = calloc(ineed, sizeof(*izdata));		/* Read it all in at once	*/
			fseek(funit, 155024*iframe+1372, SEEK_SET);		/* Move to the data itself */
			if ( (igot = (int) fread(izdata, sizeof(*izdata), ineed, funit)) != ineed) {
				ERRprintf("ERROR: Unable to read all the bitmap data (only %d bytes of %d)\n", igot, ineed);
				free(izdata);
				return(NULL);
			}
			break;
	}

/* All must now have set: cols, rows and file sitting at data pointers */

/* Decide how much to copy over */
	FirstCol = min(cols, max(1, FirstCol)) - 1;	/* Make look as array indices */
	LastCol  = min(cols, max(1, LastCol))  - 1;
	FirstRow = min(rows, max(1, FirstRow)) - 1;
	LastRow  = min(rows, max(1, LastRow))  - 1;

	if (! GVAllocSurface(UseCurve, GVF_USER, LastRow-FirstRow+1, LastCol-FirstCol+1)) {
		ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
		free(zdata); return(NULL);
	} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
		free(zdata); return(NULL);
	}
	surface = (SURFACE *) *varptr;
	strscpy(surface->ids, ids, sizeof(surface->ids) );

/* Surface is stored in COLUMN major format, all of column 1 first, not row */
/* Must be flipped at this step since bitmap is row by row */
	z = surface->z;
	for (i=FirstCol; i<=LastCol; i++) {
		for (j=FirstRow; j<=LastRow; j++) {
			if (FLIR_Type == FPF) {
				*z++ = zdata[j*cols+i];
			} else {
				*z++ = (REAL) ( izdata[j*cols+i] * 0.1758764 - 732.6202 );		/* Fit from manual comparison */
			}
		}
	}

/* Dummy fill-in of the X,Y coordinate */
	for (i=0; i<LastCol-FirstCol+1; i++) surface->x[i] = (REAL) (i+FirstCol);
	for (i=0; i<LastRow-FirstRow+1; i++) surface->y[i] = (REAL) (i+FirstRow);

/* Clean up and return successful */
	if ( zdata != NULL) free(zdata);
	if (izdata != NULL) free(izdata);
	return (surface);
}


/* ===========================================================================
-- Usage: SURFACE *GptAFMRead(char *fname, int FirstCol, int LastCol,
--                            int FirstRow, int LastRow, char *UseCurve,
--                            int rd_flags, int iframe);
--
-- Inputs: fname - name of an existing file (has been checked)
--         FirstCol, LastCol - columns to read, inclusive
--         FirstRow, LastRow - rows to read, inclusive
--         UseCurve          - name of the defined surface when complete
--         rd_flags          - Read flags - see above for definitions
--         iframe            - which frame from the file
--
-- Output: Creates internal SURFACE structure with name "UseCurve" and fills in
--
-- Returns: Pointer to SURFACE structure if successful, NULL otherwise
=========================================================================== */
SURFACE *GptAFMRead(char *fname, int FirstCol, int LastCol,
						  int FirstRow, int LastRow, char *UseCurve, int rd_flags, int iframe) {

	/* GENPLOT code elements */
	int irow,icol, idx,iz, NumCols, NumRows;
	void **varptr;
	int type;										/* Type of variable (GVLink)	*/
	SURFACE *surface=NULL;						/* Temporary array pointer		*/
	REAL *x,*y,*z;

/* AFM read parameters */
	char aline[4096], *aptr, *bptr;
	int lineno;
	FILE *funit;
	char *cbuf;
	short int *sbuf;
	long  int  *lbuf;

	/* AFM parameters / code elements */
	typedef enum _IMAGE_TYPE {HEIGHT, PHASE, NOT_YET_SET} IMAGE_TYPE;
	IMAGE_TYPE Image_Type = NOT_YET_SET;

	typedef enum _IMAGE_MODE {AT_START, IN_FILE_LIST, IN_EQUIPMENT_LIST, IN_SCANNER_LIST, IN_FAST_SCAN_LIST, IN_SCAN_LIST, IN_IMAGE_LIST, AT_IGNORE, AT_END} IMAGE_MODE;
	IMAGE_MODE mode = AT_START;

	typedef struct _IMAGE_MODE_LIST {
		char *text;
		IMAGE_MODE mode;
	} IMAGE_MODE_LIST;
	IMAGE_MODE_LIST *mode_ptr;
	IMAGE_MODE_LIST  mode_list[] = {
		{"File list",			IN_FILE_LIST}, 
		{"Equipment list",	IN_EQUIPMENT_LIST},
		{"Scanner list",		IN_SCANNER_LIST},
		{"Fast Scan list",	IN_FAST_SCAN_LIST},
		{"Ciao scan list",	IN_SCAN_LIST},
		{"Ciao image list",	IN_IMAGE_LIST},
		{"File list end",		AT_END},
		{NULL,					AT_END}
	};

/* Critical data - should be available on each */
	int data_offset = -1,								/* Where does the data begin	*/
		 data_length = -1,								/* Length of the binary data	*/
		 bytes_per_pixel = -1,							/* Bytes per pixel				*/
		 samps_per_line = -1,							/* Points per line				*/
		 num_lines = -1;									/* Number of lines in image	*/
	int image_count = 0;
	double scan_size[2] = {-1, -1},					/* Scan size (nm corrected)	*/
		 nm_per_V = -1,									/* nm_per_V calibration			*/
		 V_per_LSB = -1;									/* LSB to volts					*/

/* Read the file - open first as an ASCII file for header information */
	if ( (funit = fopen(fname, "r")) == NULL) {
		ERRprintf("ERROR: Failed to open AFM file (%s)\n", fname);
		return(NULL);
	}

/* Correct any potential errors in calling parameters */
	if (iframe <= 0) iframe = 1;

/* Read and interpret the headers (if desired) */
	mode = AT_START; lineno = 0;
	while (mode != AT_END && fgets(aline, sizeof(aline), funit) != NULL) {
		if ( (aptr = strchr(aline, '\n')) != NULL) *aptr = '\0';
		lineno++;
		if (mode == AT_START && strcmp(aline, "\\*File list") != 0) {
			ERRprintf("ERROR: First line of AFM file invalid or unrecognized.\n");
			fclose(funit); return(NULL);
		} else if (*aline != '\\') {
			ERRprintf("ERROR: Badly formed AFM data header - missing backslash on line %d.\n", lineno);
			fclose(funit); return(NULL);
		}
		aptr = aline+1;										/* Skip over the backslash */

		if (*aptr == '*') {									/* Mode change? */
			aptr++;
			mode = AT_IGNORE;
			for (mode_ptr=mode_list; mode_ptr->text != NULL; mode_ptr++) { 
				if (strncmp(aptr, mode_ptr->text, strlen(mode_ptr->text)) == 0) {
					mode = mode_ptr->mode;
					break;
				}
			}
			if (mode == AT_IGNORE) ERRprintf("ERROR: Unrecognized AFM mode ignored - %s", aline);
			if (mode == IN_IMAGE_LIST) image_count++;
		}
		switch (mode) {
			case AT_IGNORE:
			case AT_START:
			case AT_END:
				break;
			case IN_FILE_LIST:
				break;
			case IN_EQUIPMENT_LIST:
				break;
			case IN_FAST_SCAN_LIST:
				break;
			case IN_SCANNER_LIST:
				if (strncmp(aptr, "@Sens. Zscan: V ", 16)      == 0) nm_per_V        = atof(aptr+16);
				break;
			case IN_SCAN_LIST:
				if (strncmp(aptr, "Scan Size: ", 10)           == 0) {
					bptr = aptr+10;
					scan_size[0] = strtol(bptr, &bptr, 10); 
					if (bptr != NULL) scan_size[1] = strtol(bptr, &bptr, 10);
					if (scan_size[1] <= 0) scan_size[1] = scan_size[0];
					if (bptr != NULL) { 
						while (isspace(*bptr)) bptr++;
						if (strncmp(bptr, "nm", 2) == 0) { scan_size[0] *= 1;    scan_size[1] *= 1; }
						else if (strncmp(bptr, "~m", 2) == 0) { scan_size[0] *= 1E3; scan_size[1] *= 1E3; }
						else if (strncmp(bptr, "mm", 2) == 0) { scan_size[0] *= 1E6; scan_size[1] *= 1E6; }
					}
				}
				break;
			case IN_IMAGE_LIST:
				if (image_count > iframe) break;
				if (strncmp(aptr, "Data offset: ", 13)         == 0) data_offset     = atoi(aptr+13);
				if (strncmp(aptr, "Data length: ", 13)         == 0) data_length     = atoi(aptr+13);
				if (strncmp(aptr, "Bytes/pixel: ", 13)         == 0) bytes_per_pixel = atoi(aptr+13);
				if (strncmp(aptr, "Samps/line: ", 12)          == 0) samps_per_line  = atoi(aptr+12);
				if (strncmp(aptr, "Number of lines: ", 17)     == 0) num_lines       = atoi(aptr+17);
				if (strncmp(aptr, "Scan size:", 10)            == 0) {
					bptr = aptr+10;
					scan_size[0] = strtol(bptr, &bptr, 10); 
					if (bptr != NULL) scan_size[1] = strtol(bptr, &bptr, 10);
					if (scan_size[1] <= 0) scan_size[1] = scan_size[0];
					if (bptr != NULL) { 
						while (isspace(*bptr)) bptr++;
						if (strncmp(bptr, "nm", 2) == 0) { scan_size[0] *= 1;    scan_size[1] *= 1; }
						else if (strncmp(bptr, "~m", 2) == 0) { scan_size[0] *= 1E3; scan_size[1] *= 1E3; }
						else if (strncmp(bptr, "mm", 2) == 0) { scan_size[0] *= 1E6; scan_size[1] *= 1E6; }
					}
				}
				if (strncmp(aptr, "@2:Image Data: S [Height]",  25)) Image_Type = HEIGHT;
				if (strncmp(aptr, "@2:Image Data: S [Phase]",   24)) Image_Type = PHASE;
				if (strncmp(aptr, "@2:Z scale: V [Sens. Zscan", 26) == 0) {						/* In height mode */
					bptr = aline+strlen(aline)-1;
					while (bptr != aline && *bptr != '(') bptr--;
					V_per_LSB = atof(bptr+1);
				}
				if (strncmp(aptr, "@2:Z scale: V [Sens. Phase", 26) == 0) {						/* In phase mode */
					bptr = aline+strlen(aline)-1;
					while (bptr != aline && *bptr != '(') bptr--;
					V_per_LSB = atof(bptr+1);
				}
				break;
		}
		if (mode == AT_END) break;		/* Read to get the real data */
	}
	fclose(funit);						/* Done with header reads */

/* Print out for user validation */
	if (rd_flags & RD_DEBUG) {
		TTYprintf("XSCALE: %f\n", scan_size[0]/(samps_per_line-1));
		TTYprintf("YSCALE: %f\n", scan_size[1]/(num_lines-1));
		TTYprintf("/* data_offset 0x%x     data_length: 0x%x    bytes_per_pixel: %d\n", data_offset, data_length, bytes_per_pixel);
		TTYprintf("/* samps_per_line: %d   lines: %d\n", samps_per_line, num_lines);
		TTYprintf("/* scan_size: %f x %f\n", scan_size[0], scan_size[1]);
		TTYprintf("/* nm_per_V: %f   V_per_LSB: %f\n", nm_per_V, V_per_LSB);
	}

/* Check to verify that we have the necessary parameters */
	if (image_count < iframe) {
		ERRprintf("ERROR: Frame %d does not exist in the AFM file (found only %d)\n", iframe, image_count);
		return(NULL);
	} else if (data_offset < 0  || data_length < 0  || bytes_per_pixel < 0 || samps_per_line < 0 || num_lines < 0) {
		ERRprintf("ERROR: AFM Headers missing critical parameters - abort\n");
		return(NULL);
	} else if (bytes_per_pixel != 1 && bytes_per_pixel != 2 && bytes_per_pixel != 4) {
		ERRprintf("ERROR: AFM read limited to 1,2 or 4 bytes_per_pixel formats\n");
		return(NULL);
	} else if (data_length != bytes_per_pixel * samps_per_line * num_lines) {
		ERRprintf("ERROR: AFM data length mismatch (%d versus %d expected)\n", data_length, bytes_per_pixel * samps_per_line * num_lines);
		return(NULL);
	}

/* Verify scaling / info parameters - set defaults if necessary */
	if (scan_size[0] < 0 || scan_size[1] < 0) {
		ERRprintf("ERROR: Scan size failed to parse - set to 1000x1000 nm\n");
		scan_size[0] = scan_size[1] = 1000;
	}
	if (nm_per_V < 0 || V_per_LSB < 0) {
		fprintf(stderr, "ERROR: Unable to convert Z to nm.  Assume 0.001 nm resolution.\n");
		nm_per_V = 0.001;
		V_per_LSB = 1;
	}

/* Reopen as a binary file and read the data */
	if ( (funit = fopen(fname, "rb")) == NULL) {
		ERRprintf("ERROR: Unable to reopen file in binary mode (%s)\n", fname);
		return(NULL);
	} else if ( (cbuf = malloc(data_length)) == NULL) {
		ERRprintf("ERROR: Unable to allocate space to hold the AFM image\n");
		return(NULL);
	} else if ( fseek(funit, data_offset, SEEK_SET) != 0) {
		ERRprintf("ERROR: Unable to seek to data in AFM file\n");
		free(cbuf); fclose(funit); return(NULL);
	} else if ( (int) fread(cbuf, 1, data_length, funit) != data_length) {
		ERRprintf("ERROR: Unable to read the binary data from AFM file\n");
		free(cbuf); fclose(funit); return(NULL);
	}
	fclose(funit);
	sbuf = (void *) cbuf;			/* Multiple names for buffer */
	lbuf = (void *) sbuf;

/* Create the surface */
	FirstCol = min(samps_per_line, max(1, FirstCol));	FirstCol--;		/* End on 0-index based */
	LastCol  = min(samps_per_line, max(1, LastCol));	LastCol--;
	FirstRow = min(num_lines,      max(1, FirstRow));	FirstRow--;
	LastRow  = min(num_lines,      max(1, LastRow));	LastRow--;
	NumRows = LastRow-FirstRow+1;
	NumCols = LastCol-FirstCol+1;
	if (NumRows < 1 || NumCols < 1) {
		ERRprintf("ERROR: Specification of row/col returns no valid data\n");
		free(cbuf); return(NULL);
	} else if (! GVAllocSurface(UseCurve, GVF_USER, NumRows, NumCols)) {
		ERRprintf("ERROR: Unable to allocate a surface with the name %s\n", UseCurve);
		free(cbuf); return(NULL);
	} else if (! GVGetInfo(UseCurve, &type, (void **) &varptr) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", UseCurve);
		free(cbuf); return(NULL);
	}
	surface = (SURFACE *) *varptr;
	x = surface->x;	y = surface->y;	z = surface->z;
	strscpy(surface->ids, "AFM image", sizeof(surface->ids));

	for (irow=0; irow<NumRows; irow++) {
		for (icol=0; icol<NumCols; icol++) {
			idx = (irow+FirstRow)*samps_per_line + (icol+FirstCol);
			iz  = icol*NumRows + irow;
			if      (bytes_per_pixel == 1) { z[iz] = (REAL) cbuf[idx]; }
			else if (bytes_per_pixel == 2) { z[iz] = (REAL) sbuf[idx]; }
			else if (bytes_per_pixel == 4) { z[iz] = (REAL) lbuf[idx]; }
			z[iz] *= (REAL) (nm_per_V * V_per_LSB);
		}
	}
	for (icol=0; icol<NumCols; icol++) x[icol] = GVTrimToReal((icol+FirstRow)*scan_size[0]/(samps_per_line-1));
	for (irow=0; irow<NumRows; irow++) y[irow] = GVTrimToReal((irow+FirstRow)*scan_size[1]/(num_lines-1));

	return(surface);
}

/* ===========================================================================
-- Routine to check for and scan a time-stamp in format hh:mm:ss
--
-- Usage: PRIVATE BOOL ScanTime(char *token, double *value);
--
-- Inputs: token - pointer to string to be interpreted
--         value - pointer to location to save interpreted value
--
-- Output: *value - replaced with integer # of seconds in hh:mm:ss format
--
-- Return: TRUE  -> looks and smells like a timestamp with a :
--         FALSE -> something didn't follow
--
-- Notes: (1) Must be one of the formats:   m:ss     mm:ss  h:mm:ss  hh:mm:ss
--                                          ss may be ss.sss decimal value
--        (2) Must be 24 hour clock, no am/pm
--        (3) Wrap around 24:00 is heurestically handled if in hh:mm:ss 
--            format and samples no more than 1 hour apart
=========================================================================== */
#define	ONE_DAY	 (24*60*60)
PRIVATE BOOL ScanTime(char *token, double *value) {

	int i,elem;
	char *aptr;
	double tval;
	static double last = 0;
	static int iwrap = 0;

/* Reset condition */
	if (token == NULL) { last = 0; return(FALSE); }

/* Interpret the string */
	tval = 0;
	for (i=0; i<3; i++) {										/* Allow only hh:mm:ss */
		if (! isdigit(*token)) return(FALSE);
		elem = *(token++)-'0';									/* Get value */
		if (isdigit(*token)) {
			elem = 10*elem + *(token++)-'0';
		} else if (i != 0) {
			return(FALSE);
		} 
		if (elem > 60) return(FALSE);
		tval = 60.0*tval + elem;
		if (tval > ONE_DAY) return(FALSE);
		if (i != 0) {												/* Only finish on 2nd or 3rd element */
			if (*token == '\0') break;
			if (*token == '.') {						/* Allow decimal values on element */
				tval += strtod(token, &aptr);
				if (*aptr != '\0') return(FALSE);					/* Must end the string */
				break;
			}
		}
		if (*token != ':' || i == 3) return(FALSE);
		token++;
	}

/* Only comes here if successfully interpreted a value */
	tval += ONE_DAY*iwrap;						/* Previous wrapping condition */
	if (tval-last+ONE_DAY < 3600) {			/* Have we wrapped again?		 */
		iwrap++;
		tval += ONE_DAY;
	}
	last = tval;
	if (value != NULL) *value = tval;
	return(TRUE);
}


/* ============================================================================
-- Subroutine to write data out to the disk
--
-- Usage:  bool = gen$wr(x,y,npt,nptmax,ids)
--
-- Inputs: filename - filename to write to
--         x,y, npt - real data to output (NPT)
--
-- Output: gen$wr - .true.  => everything is okay-dokey
--                  .false. => error
--
-- Options: Checks command line for -ASCII option
============================================================================ */
static char WriteHelp[]=
"\n"
" Primary command to write data files - either in ASCII format or binary.\n"
"\n"
"   WRITE <filename> [-options]\n"
"\n"
" Options:\n"
"   -help | -?           Prints this help message\n"
"   -debug               Sometimes debugging info\n"
"\n"
"   -name <str_expr>     Specify name, allowing string expressions (not default)\n"
"   -filename <str_expr> synonymous with -name (can be abbreviated -file)\n"
"   -curve <name>        Writes the specified curve instead of main curve\n"
"   -surface <name>      Writes the specified surface (matrix) curve\n"
"   -list <var1> <var2> <var3> /\n"
"                        Writes the specified list of array variables as columns\n"
"   -bmp    <surface>\n"
"   -bitmap <surface>    Writes the surface as a bitmap file (extra options)\n"
"       -range <zmin> <zmax>    Color band size (default set by surface range)\n"
"       -size <cols> <rows>     Bitmap size (default set by surface dimensions)\n"
"       -colormap [grey|heat|..] Color mapping to use for Z to color.\n"
"       -palette  [grey|heat|..] Color mapping to use for Z to color.\n"
"                               Use `palette rainbow -?' to get list\n"
"\n"
"   -ascii               Write in ASCII text [default user settable]\n"
"   -binary              Write as GENPLOT binary [default user settable]\n"
"   -csv                 Force write as 2D/3D ASCII mode with comma-separated values\n"
"   -append              Append data to existing file (ASCII file mode only)\n"
"\n"
"   -notext              Don't output any text with file - numbers only\n"
"   -text                Prompt for text to be added as comments in file\n"
"       -comments        Synonymous with -text\n"
"   -commands            Prompt for text to be added as commands in datafile\n"
"   -ids <text>          Set identifier text to be saved with file\n"
"       -comment         Synonymous with -ids\n"
"\n"
"   -user                Write using alternate loaded USER module\n"
"\n"
"   -silent | -quiet     Don't print any statistics or prompts during write\n"
"   -force               Don't prompt if overwriting existing file\n"
"\n"
" Filenames may be specified either as simple names, or as piped commands.  Pipe'd\n"
" output is specified with a leading | in the filename.  The extension .gz is\n"
" automatically recognized as compressed output and reformatted as a pipe through\n"
" gzip.  No extenions is automatically appended, but the .dat is recommended.\n"
"     test.file.dat     Writes the file as specified\n"
"     test.dat.gz       Equivalent to ""| gzip -9 > test.dat""\n"
"     | wc              Outputs the file to the command wc (wordcount)\n"
"     >>                Outputs to console (changes default to -silent also)\n"
"\n"
" Examples: write simple.dat\n"
"           write simple.dat -curve c1 -notext -ascii\n"
"           write simple.dat -list a1 a2 a3 /\n";

int GptWrite(char *PassedCurve) {

	char UseCurve[VARNAME_STR_SIZE];			/* Adequate of space				*/
	char token[DFLT_STR_SIZE], *aptr, *mode,
		  comment[DFLT_STR_SIZE]="",			/* Comment buffer					*/
		  FileName[PATH_MAX];					/* Filename							*/
	struct stat statbuf;							/* Buffer for status of name	*/

	BOOL append = FALSE;							/* No file appending				*/
	BOOL silent = FALSE;							/* Any output						*/
	BOOL debug = FALSE;							/* Debug mode?						*/
	BOOL check;										/* Check on file overwrite		*/
	BOOL options;									/* Do we check for more options */

	enum {NONE,CMTS,CMDS} addtext = NONE;	/* Add comments to file?		*/
	BOOL addid = TRUE;							/* Put in the ID in file?		*/

	enum {XYZ, LIST, SURF, USER} DataType;	/* Data Type */
	enum {BINARY, ASCII, CSV, BITMAP} FileMode;	/* File mode */
	enum {SIMPLEFILE, PIPE, CONSOLE} FileType;	/* File type */
	int vers;										/* File format version			*/

	struct {
		REAL *x;										/* Where data will be stored	*/
		int npt;										/* Size of the array				*/
	} entry[NVAR];
	int i,j, ncol, npt;

	REAL  s2[2], *buf=NULL;						/* For writing unformatted		*/
	void **varptr;
	ARRAY *array=NULL;							/* Temporary array pointer		*/
	int   type;										/* Type of variable (GVLink)	*/

	int rc=0;										/* Error code for WriteError	*/

	strscpy(UseCurve, PassedCurve, sizeof(UseCurve));	/* Copy curve to work with	*/

/* First, look for a help request */
	if (LexCheckHelp("Write", WriteHelp, NULL)) return(1);

/* ----------------------------------------------------------------------------
-- Handle the options and the filename specification first.
-- Integrate the request for the filename along with options, so can
-- be given in either order.  The first "non-option" will be used as the
-- filename, and options will be scanned on both sides.
--
-- Logic is a bit backwards.  First check if it is an option and handle.
-- If not an option, then check as the filename.  Continue until we have
-- a filename and no more options.
---------------------------------------------------------------------------- */
	*FileName = '\0';								/* No file specified yet	*/
	DataType  = XYZ;								/* Default to XYZ mode		*/
	FileMode  = (Gpt->OpMode & OpBinaryWrite) ? BINARY : ASCII ;
	check     = (Gpt->OpMode & OpConfirmOverwrite) != 0;

	ncol = 0;										/* No list specified */
	options = TRUE;								/* Can be disabled */
	debug = FALSE;									/* Disable debug */

	while (TRUE) {
		if (*FileName == '\0' && LexIsEmpty()) {
			LexReadLine("File to write (ABORT): ");
			if (LexEscape(TRUE)) return(1);
		}
		if (options && LexGetOptionEx(token, sizeof(token), "-")) {	/* Check the options */

			if (LexEqual(token, "-name", 5) || LexEqual(token, "-filename", 5)) {
				if (! LexGetStrExprP(FileName, sizeof(FileName), "Filename expression: ")) return 1;

			} else if (LexEqual(token, "-CURVE", 3) || LexEqual(token, "-SURFACE", 5)) {
				if (LexGetTokenP(token, sizeof(token), "Curve name (unchanged): ")) {
					strscpy(UseCurve, token, sizeof(UseCurve));
					if (GptLinkXYZ(UseCurve) != 0) {
						ERRprintf("ERROR: Specified curve %s cannot be accessed\n", UseCurve);
						return(-1);
					}
				}

			} else if (LexEqual(token, "-BITMAP", 5) || LexEqual(token, "-BMP", 4)) {
				if (! LexGetTokenP(token, sizeof(token), "Surface (abort): ")) return(-1);
				strscpy(UseCurve, token, sizeof(UseCurve));
				if (GptLinkXYZ(UseCurve) != 0) {
					ERRprintf("ERROR: Specified curve %s cannot be accessed\n", UseCurve);
					return(-1);
				} else if (GptSurface == NULL) {
					ERRprintf("ERROR: Bitmaps can be written only for surfaces\n");
					return(-1);
				}
				FileMode = BITMAP;
				InitBitMapOptions(GptSurface);					/* Initialize the options */
			} else if ( (FileMode == BITMAP) && WasBitMapOption(token)) {
				/* Handled */

			} else if (LexEqual(token, "-DEBUG", 6)) {
				debug = TRUE;											/* Enable debug */
			} else if (LexEqual(token, "-ASCII", 3)) {
				FileMode = ASCII;
			} else if (LexEqual(token, "-CSV", 3)) {
				FileMode = CSV;
			} else if (LexEqual(token, "-BINARY", 2)) {
				FileMode = BINARY;

			} else if (LexEqual(token, "-NOTEXT", 4)) {
				addtext = NONE;
				addid   = FALSE;
			} else if (stricmp(token, "-TEXT") == 0 || stricmp(token, "-COMMENTS") == 0) {
				addtext = CMTS;
			} else if (LexEqual(token, "-COMMANDS", 6)) {
				addtext = CMDS;
			} else if (stricmp(token, "-IDS")==0 || LexEqual(token, "-COMMENT", 4)) {
				LexGetTokenP(comment, sizeof(comment), "Identifier comment: ");
			} else if (LexEqual(token, "-APPEND", 3)) {
				append = TRUE;
			} else if (LexEqual(token, "-SILENT", 4) || LexEqual(token, "-QUIET", 2)) {
				silent = TRUE;
			} else if (LexEqual(token, "-FORCE", 6)) {
				check = FALSE;
			} else if (LexEqual(token, "-LIST", 4)) {
				DataType = LIST;										/* Now in LIS mode */
				for (ncol=0; ncol<NVAR; ncol++) {				/* ncol important */
					if (! LexGetTokenP(token, sizeof(token), "Variable (end): ")) break;
					if (LexEscape(TRUE)) return(0);
					if (*token == '/') break;
					if (! GVGetInfo(token, &type, (void **) &varptr)) {
						ERRprintf("ERROR: Variable %s does not exist\n", token);
						return(-1);
					} else if (type != GV_ARRAY && type != GV_ARRAY_LINK) {
						ERRprintf("ERROR: Specified variable %s is not an array\n",token);
						return(-1);
					}
					array = (ARRAY *) *varptr;
					entry[ncol].x    =  array->x;
					entry[ncol].npt  = *array->size;
				}
			} else if (LexEqual(token, "-USER",   5)) {
				if (GptUserWrite == NULL) {
					ERRprintf("ERROR: No user write routine loaded\n");
					return(-1);
				}
				DataType = USER;
				options = FALSE;
			} else {
				ERRprintf("ERROR: %s is not a valid WRITE option\n", token);
				return(-1);
			}
		} else if (*FileName == '\0') {				/* Don't have file yet */
			if (! LexGetFile(FileName, sizeof(FileName))) return(1);
		} else {
			break;
		}
	}

/*	Set the filetype (PIPE or FILE) now */
	if (stricmp(FileName, ">>") == 0) {
		FileType = CONSOLE;
		silent = TRUE;
	} else if (*FileName == '|') {
		FileType = PIPE;
	} else {
		FileType = SIMPLEFILE;
		if (strlen(FileName) > 3 && stricmp(FileName+strlen(FileName)-3, ".gz") == 0) {
			char inbuf[PATH_MAX];
			FileType = PIPE;
			strcpy(inbuf, FileName); 
			strcat(strcpy(FileName, "gzip -9 > "), inbuf);
		} else if (FileMode == BITMAP) {			/* Default extensions */
			SysAddExt(FileName, ".bmp");
		}
	}

/* First, really trivial */
	if (DataType == USER) return((*GptUserWrite)(FileName, UseCurve));

/* ... Check if the linked curve is SURFACE instead of XYZ */
	if (GptSurface != NULL && DataType == XYZ) {
		DataType = SURF;
	}

/* ... Quick validity checks */
	if ( append && ( (FileMode != ASCII && FileMode != CSV) || FileType != SIMPLEFILE) ) {
		ERRprintf("ERROR: -append is compatible only with ASCII output to a file\n");
		return(-1);
	} else if (FileMode == BITMAP && DataType != SURF) {
		ERRprintf("ERROR: Bitmap writing is only implemented for surfaces\n");
		return(-1);
	} else if (FileMode == BITMAP && append) {
		ERRprintf("ERROR: Drugs are frying your brain.  Append a bitmap?  Come on!\n");
		return(-1);
	}
	
/* ... Check on existance of the file (if necessary) */
	if (FileType == SIMPLEFILE && check && stat(FileName, &statbuf) == 0) {
		TTYprintf("WARNING: %s exists.  Confirm overwrite? ", FileName);
		TTYgets(token, sizeof(token));
		if (tolower(*token) != 'y') return(-1);
	}

/* ... If the -LIST was not specified, default to X,Y,Z */
	if ( (ncol == 0 && DataType == LIST) || DataType == XYZ) {
		DataType = XYZ;
		ncol = (GptCurve->z == NULL) ? 2 : 3 ;
		entry[0].x = GptCurve->x;
		entry[1].x = GptCurve->y;
		if (ncol == 3) entry[2].x = GptCurve->z;
		for (j=0; j<ncol; j++) entry[j].npt = GptCurve->npt;
		if (*GptCurve->ids != '!' && *comment == '\0' && addtext == NONE)
			strscpy(comment, GptCurve->ids, sizeof(comment));
	} else if (DataType == SURF) {
		if (*GptSurface->ids != '!' && *comment == '\0' && addtext == NONE)
			strscpy(comment, GptSurface->ids, sizeof(comment));
	}
		
/* ... Process the command now! */
	mode = (FileMode == BINARY || FileMode == BITMAP) ? "wb" : (append ? "a" : "w") ;
	if (FileType == PIPE) {
		FileHandle = popen(FileName+1,mode);
	} else if (FileType == SIMPLEFILE) {
		FileHandle = fopen(FileName,mode);
	} else {
		FileHandle = stdout;
	}
	if (FileHandle == NULL) goto OpenError;
	
/* Handle comments for binary common first */
	if (FileMode == BITMAP) {
		/* No way to put comments in a bitmap */
	} else if (FileMode != BINARY) {
		if (addid && *comment != '\0') fprintf(FileHandle, "/* %s\n", comment);
		if (addtext  != NONE) {
			if (! silent) TTYputs(PROMPTFORTEXT);
			while (LexPromptLine(token, sizeof(token), ": ", TRUE)) {
				aptr = token+strlen(token)-1;
				while (aptr!=token && isspace(*aptr)) *(aptr--) = '\0';
				for (aptr=token; isspace(*aptr); aptr++) ;
				if (stricmp(aptr, "@END") == 0) {
					break;
				} else if (strnicmp(aptr, "sprintf(",8) == 0) {
					aptr = GVEvalStrExpr(token, NULL);
					strcpy(token, aptr); 
					free(aptr);
				} else if (*aptr == '&') {
					char tmp[9], *cptr=aptr+1;
					for (i=0; i<8 && !isspace(*cptr) && *cptr; i++,cptr++)
						tmp[i] = tolower(*cptr);
					tmp[i] = '\0';
					if (strstr("getarg query yesno encode rootarg", tmp) != NULL) {
						LexInsText(token);
						LexGetToken(token, sizeof(token));
					}
				}
				fprintf(FileHandle, "%s %s\n", (addtext == CMDS) ? "@ " : "/*", token);
			}
		}
	} else {
		if (DataType != SURF) {
			if (! RMWriteText("VERSION GENPLT 4.0")) { rc = 1; goto WriteError; }
			vers = 40;
		} else {
			if (! RMWriteText("VERSION GENPLT 4.1")) { rc = 2; goto WriteError; }		/* Surfaces now get X,Y values also */
			vers = 41;
		}
		if (addid && *comment != '\0' && ! RMWriteText(comment)) { rc = 3; goto WriteError; }
		if (addtext != NONE) {
			if (! silent) TTYputs(PROMPTFORTEXT);
			while (LexPromptLine(token, sizeof(token), ": ", TRUE)) {
				aptr = token+strlen(token)-1;
				while (aptr!=token && isspace(*aptr)) *(aptr--) = '\0';
				for (aptr=token; isspace(*aptr); aptr++) ;
				if (stricmp(aptr, "@END") == 0) {
					break;
				} else if (strnicmp(aptr, "sprintf(",8) == 0) {
					aptr = GVEvalStrExpr(token, NULL);
					strcpy(token, aptr); 
					free(aptr);
				} else if (*aptr == '&') {
					char tmp[9], *cptr=aptr+1;
					for (i=0; i<8 && !isspace(*cptr) && *cptr; i++,cptr++)
						tmp[i] = tolower(*cptr);
					tmp[i] = '\0';
					if (strstr("getarg query yesno encode rootarg", tmp) != NULL) {
						LexInsText(token);
						LexGetToken(token, sizeof(token));
					}
				}
 				if (addtext == CMDS) {								/* Make it a comnand */
					memmove(token+2, token, sizeof(token)-2);
					strncpy(token, "@ ", 2);
					token[sizeof(token)-1] = '\0';
				}
				if (! RMWriteText(token)) { rc = 4; goto WriteError; }
			}
		}
		if (! RMWriteText("\001")) { rc = 5; goto WriteError; }
	}

/* Now, switch based on format */
	if (DataType == SURF) {			/* Handle surface separate */
		if (FileMode == BINARY) {
			npt =  GptSurface->nrow;
			ncol = GptSurface->ncol;
			if (npt >= 16384) {					/* If beyond PC-DOS, use long write */
				if (! RMWriteLongs(npt, ncol)) { rc = 6; goto WriteError; }
			} else {
				if (! RMWriteShorts(npt,ncol)) { rc = 7; goto WriteError; }
			}

			s2[0] = 1.0f;							/* Xfact term */
			s2[1] = 0.0f;							/* Xoff  term */
			for (i=0; i<ncol; i++) {
				sprintf(token, "Matrix column %i", i+1);
				if (! RMWriteText(token))     { rc = 8; goto WriteError; }
				if (! RMWriteArray(s2, 2))    { rc = 9; goto WriteError; }
				if (! RMWriteArray(GptSurface->z+i*npt, npt)) { rc = 10; goto WriteError; }
			}
			if (vers == 41) {			/* Added 1/20/04 - write of Y and X values for column/row coordinates */
				if (! RMWriteText("nrow Y values"))      { rc = 11; goto WriteError; }
				if (! RMWriteArray(s2, 2))					  { rc = 12; goto WriteError; }
				if (! RMWriteArray(GptSurface->y, npt))  { rc = 13; goto WriteError; }
				if (! RMWriteText("ncol X values"))      { rc = 14; goto WriteError; }
				if (! RMWriteArray(s2, 2))					  { rc = 15; goto WriteError; }
				if (! RMWriteArray(GptSurface->x, ncol)) { rc = 16; goto WriteError; }
			}
		} else if (FileMode == BITMAP) {
			GptBitmapWrite(FileHandle, GptSurface, silent);
		} else {
			GptMatrixWrite(FileHandle, GptSurface, silent);
		}

/* Normal XY or XYZ or <list> write mode */
	} else {
		npt = 0;															/* Determine # pts to write */
		for (i=0; i<ncol; i++) npt = max(npt, entry[i].npt);

		if (FileMode == CSV || FileMode == ASCII) {		/* One of the ASCII type writes */

			for (i=0; i<npt; i++) {										/* All data	*/
				aptr = token;
				for (j=0; j<ncol; j++) {								/* Full list */
					if (FileMode == ASCII) {
						sprintf(aptr, "%.8g\t", (i < entry[j].npt) ? entry[j].x[i] : 0.0);
					} else {
						sprintf(aptr, "%.8g , ", (i < entry[j].npt) ? entry[j].x[i] : 0.0);
					}
					aptr += strlen(aptr);
				}
				if (FileMode == ASCII) {
					if (aptr != token && *(aptr-1) == '\t') aptr--;
				} else {
					if (aptr != token && *(aptr-2) == ',') aptr -= 3;
				}
				*aptr++ = '\n'; *aptr = '\0';							/* Terminate */
				fputs(token, FileHandle);
				if ( (i%UPDATE_SCREEN_COUNT) == 0 && ! silent) {
					sprintf(token, "\rWriting: [%5.5i]", i);
					CONputs(token); fflush(stdout);
				}
			}
			if (! silent) CONputs("\r\n");

		} else if (FileMode == BINARY) {
			if (npt >= 16384) {					/* If beyond PC-DOS, use long write */
				if (! RMWriteLongs(npt, ncol)) { rc = 17; goto WriteError; }
			} else {
				if (! RMWriteShorts(npt,ncol)) { rc = 18; goto WriteError; }
			}

			s2[0] = 1.0f;							/* Xfact term */
			s2[1] = 0.0f;							/* Xoff  term */
			buf = malloc(npt * sizeof(REAL));

			for (i=0; i<ncol; i++) {
				if (DataType == XYZ && i < 3) {
					strscpy(token, Gpt->XYZ_descriptor[i], sizeof(token));
				} else {
					sprintf(token, "Data column %i", i+1);
				}
				memset(buf, 0, npt*sizeof(REAL));
				memcpy(buf, entry[i].x, entry[i].npt*sizeof(REAL));
				if (! RMWriteText(token))     { rc = 19; goto WriteError; }
				if (! RMWriteArray(s2, 2))    { rc = 20; goto WriteError; }
				if (! RMWriteArray(buf, npt)) { rc = 21; goto WriteError; }
			}
			free(buf);
		} else {
			ERRprintf("ERROR - Programmer screwup.  Expecting ASCII, CSV or BINARY file type and got otherwise.\n");
		}
	}

	if (FileType == PIPE) pclose(FileHandle);
	if (FileType == SIMPLEFILE) fclose(FileHandle);

#if (defined OS2 && !defined NO_EA_MODE)
	if (FileType == SIMPLEFILE && FileMode == BINARY && DataType != SURF) 
		EA_SetAscii(FileName, ".TYPE", "Genplot Data File");
#endif

	return(0);

/* ----------------------
-- ERRORS printout here!
---------------------- */
OpenError:
	ERRprintf("ERROR: Unable to open file %s for writing\n", FileName);
	return(-1);
WriteError:
	ERRprintf("ERROR: Unknown failure during write (rc=%d)\n", rc);
	(FileType == PIPE) ? pclose(FileHandle) : fclose(FileHandle) ;
	if (buf != NULL) free(buf);
	return(-1);
}


/* ============================================================================
-- Routine to digitize from the cursor.
============================================================================ */
#define dpi 180/3.14159265359
static int RingBellOnDigitizePoint=FALSE;

PRIVATE int DigitizeCurve(char *PassedCurve) {

   static REAL X0=0, Y0=0, XX=1, XY=0, YY=1, YX=0;	/* Transform */
	REAL X1,Y1,XT,YT, tmp, xm[3], ym[3];
	REAL A,B,C,D,det;

	int achr;
	char UseCurve[VARNAME_STR_SIZE];			/* Adequate of space				*/
	char token[OPTION_STR_SIZE];
	char tab_name[VARNAME_STR_SIZE]="";
	INT i,nstart=0;
	BOOL hold=FALSE;

	strscpy(UseCurve, PassedCurve, sizeof(UseCurve));	/* Copy curve to work with	*/
	
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-CURVE", 3)) {
			if (LexGetTokenP(token, sizeof(token), "Curve name (unchanged): ")) {
				strscpy(UseCurve, token, sizeof(UseCurve));
				if (GptLinkXYZ(UseCurve) != 0) {
					ERRprintf("ERROR: Specified curve %s cannot be accessed\n", UseCurve);
					return(-1);
				}
			}
		} else if (LexEqual(token, "-APPEND", 3)) {
			nstart = GptCurve->npt;
		} else if (LexEqual(token, "-HOLD", 3)) {
			hold = TRUE;
		} else if (LexEqual(token, "-TABLET", 2)) {
			if (! LexGetTokenP(tab_name, sizeof(tab_name), "Digitizer tablet: (abort) ")) 
				return(1);
		} else if (LexEqual(token, "-BELL", 2)) {
			RingBellOnDigitizePoint = TRUE;
		} else if (LexEqual(token, "-NOBELL", 4)) {
			RingBellOnDigitizePoint = FALSE;
		} else {
			ERRprintf("ERROR: %s is an unrecognized %%DIGITIZE option\n", token);
			return(-1);
		}
	}

	if (! GetPoint(1, NULL, NULL, (int *) tab_name)) {		/* Initialize */
		ERRprintf("ERROR: %s device does not exist or cannot digitize\n", tab_name);
		return(-1);
	}

	LexEscape(TRUE);				/* Clear <ESC> condition */
	if (! hold) {					/* Determine transform */
		TTYputs("Enter 3 known, non-colinear points ... ");
		TTYflush();
		GetPoint(3, &X0, &Y0, NULL);		/* Origin value */
		TTYputs("[1]");
		TTYflush();
		GetPoint(3, &A, &B,   NULL);		/* Fill out the matrix */
		TTYputs("[2]");
		TTYflush();
		GetPoint(3, &C, &D,   NULL);
		TTYputs("[3]\n");
		TTYflush();

		A = A-X0;
		B = B-Y0;
		C = C-X0;
		D = D-Y0;
		det = A*D-B*C;							/* Determinate! */
		if (fabs(det) <= 0.0001f) {
			ERRprintf("DRUG WARNING: Points colinear(?) - think and then retry later!\n");
			return(-1);
		}
/*
... inverted matrix is  1  | D  -B |
...                    det | -C  A |
*/
		for (i=0; i<3; i++) {						/* Find real values */
			xm[i] = LexGetReal(0.0f, "[X,Y] at the 3 points: ");
			if (LexEscape(TRUE)) return(-1);
			ym[i] = LexGetReal(0.0f, "And [Y] value: ");
			if (LexEscape(TRUE)) return(-1);
		}
		xm[1] = xm[1]-xm[0];
		xm[2] = xm[2]-xm[0];
		ym[1] = ym[1]-ym[0];
		ym[2] = ym[2]-ym[0];
		
		XX = (+D*xm[1]-B*xm[2])/det;		/* X = XX*(x-X0) + XY*(y-Y0) + x[0] */
		XY = (-C*xm[1]+A*xm[2])/det;		/* X = XX*(x-X0) + XY*(y-Y0) + x[0] */
		YX = (+D*ym[1]-B*ym[2])/det;		/* Y = YX*(x-X0) + YY*(y-Y0) + y[0] */
		YY = (-C*ym[1]+A*ym[2])/det;		/* Y = YX*(x-X0) + YY*(y-Y0) + y[0] */

		tmp = xm[0] - XX*X0 - XY*Y0;		/* X = XX*x + XY*y + X1 */
		Y0  = ym[0] - YX*X0 - YY*Y0;		/* Y = YX*x + YY*y + Y1 */
		X0  = tmp;								/* Correct junk */

		TTYprintf(" Theta X: %7.2f  Theta Y: %7.2f\n", dpi*atan2(XY,XX), dpi*atan2(-YX,YY));
	}

/* ... Start entering points! */
	TTYputs( "\n"
				"Digitize:  [1|a] Add, [2|d] Delete, [3|q] Quit, [4|p] Print\n"
				"           [On HP-GL: pen up ==> quit | pen down ==> enter]\n"
				"           [On tablets, use the buttons listed above]\n");

	i = nstart;
	while (TRUE) {
		TTYflush();
		GetPoint(4, &X1, &Y1, &achr);
		XT = X0 + X1*XX + Y1*XY;		/* Set values now */
		YT = Y0 + Y1*YY + X1*YX;
		if ( (strchr(" 01aAeE", achr) != NULL) || (achr == '\n') ) {
			if (i >= GptCurve->nptmax) {
				if (! GVResize(UseCurve, i+200)) {
					ERRprintf("ERROR: No space to append digitized data\n");
					break;
				}
				GptLinkXYZ(UseCurve);
			}
			GptCurve->x[i] = XT;
			GptCurve->y[i] = YT;
			i++;
			TTYprintf("I,X,Y:  %4.4i %14.7g %14.7g\n", i, XT, YT);
		} else if (strchr("2dD", achr) != NULL) {		/* Delete last point */
			if (i > nstart) {
				i--;
				TTYprintf("Point number %i (%f,%f) deleted\n", i, GptCurve->x[i],GptCurve->y[i]);
			}
		} else if ( (strchr("3Qq", achr) != NULL) || (achr == 0x1B) ) {	/* Quit */
			break;
		} else if ( strchr("4pP", achr) != NULL) {
			TTYprintf("Point: %14.7g %14.7g\n", XT, YT);
		} else {
			RingBell();
		}
	}
		
	GetPoint(2, NULL, NULL, NULL);			/* Close tablet */
	GptCurve->npt = i;
	strcpy(GptCurve->ids, "Digitized Curve");
	return(0);
}



/* ---------------------------------------------------------------------------
-- ... Routine to get a point from whatever device is active for TABLET
-- 
-- Usage: bool GetPoint(key,x,y,cpar)
--
-- Inputs: key - 1 ==> initialize the tablet
--               2 ==> close down the tablet
--               3 ==> return a single point
--               4 ==> return next point
--         cpar - Tablet device name to open (key = 1)
--
-- Output: x,y - digitized position in absolute inches (key 3)
-- 
--
-- Calling sequence of TAB_SET and TAB_DSP
--  int PlotSelectTablet(cpar,ipar)
--            Inputs: CPAR - tablet name
--            Output: IPAR 1,2 -> pixels/inch X and Y
--                    IPAR 3,4 -> maximum pixels X and Y
--                    IPAR 5   -> key support (bit mapped)
--
--        PlotTabletFunction(key,ipar)
--            Input: KEY - 901 -> Initialize the device
--                 Returns report as above from tab_set
--                         902 -> Close down and deallocate resources
--                         903 -> Report cursor position ??
--                 ipars(1,2) ==> X,Y position
--                         904 -> Report cursor position ??
--                 ipars(1,2) ==> X,Y position
--------------------------------------------------------------------------- */
PRIVATE BOOL GetPoint(int key, REAL *x, REAL *y, int *cpar) {

	int ipar[5], ix,iy,achr, mykey;
	static REAL xperinch, yperinch;
	static BOOL usrsav=TRUE, UseCursor;

	if (key == 1) UseCursor = (*((char *)cpar) == '\0');

/* ... Digitize via the cursor */
	if (UseCursor) {
		switch (key) {
			case 1:
				if (! PlotSystem(3, NULL, NULL)) return(FALSE);	/* Query cursor */
				usrsav = PlotSetUserMode(FALSE);						/* Work in inches */
				return(TRUE);
			case 2:
				PlotSetUserMode(usrsav);
				return(TRUE);
			case 3:
			case 4:
				PlotCursor(x, y, cpar);					/* Get a single point */
				if (RingBellOnDigitizePoint) RingBell();
				return(TRUE);
		}

/* ... Digitize via a tablet */
	} else {
		switch (key) {
			case 1:
				if (PlotSelectTablet((char *)cpar, ipar) != 0) return(FALSE);
				xperinch = (REAL) ipar[0];
				yperinch = (REAL) ipar[1];
				return(TRUE);
			case 2:
				PlotCloseTablet();
				return(TRUE);
			case 3:
			case 4:
				mykey = (key == 3) ? 0 : 1 ;		/* Get single point or stream */ 
				PlotQueryTablet(mykey, &ix, &iy, &achr);
				*x = ix / xperinch;					/* Absolute distance */
				*y = iy / yperinch;
				if (cpar != NULL) 
					*cpar = (achr<' ') ? achr+'0' : achr;	/* Convert character */
				if (RingBellOnDigitizePoint) RingBell();
				return(TRUE);
		}
	}
	return(FALSE);
}


/* ===========================================================================
-- Routine to enter points from the screen.  Similar to EDIT_DATA but will
-- create totally new curve, similar to any other read command.
=========================================================================== */
PRIVATE int CursorizeCurve(char *PassedCurve) {

	char UseCurve[VARNAME_STR_SIZE];			/* Adequate of space				*/
	char token[OPTION_STR_SIZE];
	REAL xpt, ypt;
	int i,achr,nstart=0;

	if (! PlotSystem(3, NULL, NULL)) {		/* Query cursor */
		ERRprintf("ERROR: Give me a break!  At least use a device with a cursor\n");
		return(-1);
	}
	
	strscpy(UseCurve, PassedCurve, sizeof(UseCurve));	/* Copy curve to work with	*/
	
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-CURVE", 3)) {
			if (LexGetTokenP(token, sizeof(token), "Curve name (unchanged): ")) {
				strscpy(UseCurve, token, sizeof(UseCurve));
				if (GptLinkXYZ(UseCurve) != 0) {
					ERRprintf("ERROR: Specified curve %s cannot be accessed\n", UseCurve);
					return(-1);
				}
			}
		} else if (LexEqual(token, "-APPEND", 3)) {
			nstart = GptCurve->npt;
		} else {
			ERRprintf("ERROR: %s is an unrecognized %%CURSOR option\n", token);
			return(-1);
		}
	}

	TTYputs( "\n"
				"Cursor:  [LMB|a] Add, [d] Delete, [RMB|q|esc] Quit, [p] Print\n"
				"         For rodents, LMB=add, RMB=end\n");

	i = nstart;
	while (TRUE) {
		PlotCursor(&xpt, &ypt, &achr);					/* Get a single point */
		if ( (strchr(" 0aAeE", achr) != NULL) || (achr == '\n') ) {
			if (i >= GptCurve->nptmax) {
				if (! GVResize(UseCurve, i+200)) {
					ERRprintf("ERROR: No space to append digitized data\n");
					break;
				}
				GptLinkXYZ(UseCurve);
			}
			GptCurve->x[i] = xpt;
			GptCurve->y[i] = ypt;
			i++;
			TTYprintf("I,X,Y:  %4.4i %14.7g %14.7g\n", i, xpt, ypt);
		} else if (strchr("2dD", achr) != NULL) {		/* Delete last point */
			if (i > nstart) {
				i--;
				TTYprintf("Point number %i (%f,%f) deleted\n", i, GptCurve->x[i],GptCurve->y[i]);
			}
		} else if ( (strchr("1Qq", achr) != NULL) || (achr == 0x1B) ) {	/* Quit */
			break;
		} else if ( strchr("4pP", achr) != NULL) {
			TTYprintf("Point: %14.7g %14.7g\n", xpt, ypt);
		} else {
			RingBell();
		}
	}
		
	GptCurve->npt = i;
	strcpy(GptCurve->ids, "Digitized Curve from cursor");
	return(0);
}

/* ---------------------------------------------------------------------------
-- Parameters related to the reading/writing of binary files.  The parameter
--
-- RMBUFSIZE - default maximum buffer size held over from DOS GENPLOT.
-- Files will be written such that no record exceeds this length (the data
-- count in bytes).  Must be 4096 as specified in DOS.
--
-- Actual reading routine is insensitive to both byte order and record
-- length.  On read, only requirement is that records must begin and end
-- with a 4 byte record length marker which may be either actual data byte
-- count (UNIXLIKE) or full record count (data + 2 record lengths).  Byte
-- ordering can be either way and will be reversed as necessary.  Records
-- may either by the full size (beyond RMBUFSIZE) or split into multiple
-- records of RMBUFSIZE.  DOS GENPLOT, however, will be able to read only
-- the limited subset that conform to PC byte and have RMBUFSIZE maximum
-- length records.
--------------------------------------------------------------------------- */
#define	RMBUFSIZE		4096

PRIVATE enum {NONE,RMFTN,UNIXLIKE} BinaryType;	/* Type of binary file		*/
PRIVATE BOOL ReverseBytesOnRead=FALSE;				/* Reverse bytes on read	*/

/* ---------------------------------------------------------------------------
-- Painful routine to check the format of a binary data file and properly
-- handle the reads of both types.  For writing, we use the default local.
--
-- Special mode with funit==NULL occurs for PIPE reading.  Can't seek the
-- unit, so must fake it and determine byte order on first read.
--
-- BOOL RMCheckFormat(UINT32 reclen);
--   Given a read record length (first 4 bytes of file), determine whether
--   it was written as a RMFORT record type (length = data + record markers)
--   or as a UNIX record type (length = data only).  Also whether in reading
--   the order is normal or byte reversed.  Actual length must by 80 bytes.
--   RMCheckFormat also called in RMReadRecord if in a pipe reading mode.
--
-- BOOL RMBinaryMode(FILE *funit);
--   Reads first four bytes of file and determines if it appears to be a
--   binary file format based on record length.  Sets type and byte order
--   via a call to RMCheckFormat.
--------------------------------------------------------------------------- */
static BOOL RMCheckFormat80(UINT32 reclen) {

	if (reclen == 0x00000058) {						/* RMFORT, correct order	*/
		BinaryType = RMFTN;
		ReverseBytesOnRead = FALSE;
	} else if (reclen == 0x58000000) {				/* RMFORT, reverse order	*/
		BinaryType = RMFTN;
		ReverseBytesOnRead = TRUE;
	} else if (reclen == 0x00000050) {				/* UNIXLIKE, correct order	*/
		BinaryType = UNIXLIKE;
		ReverseBytesOnRead = FALSE;
	} else if (reclen == 0x50000000) {				/* UNIXLIKE, reverse order	*/
		BinaryType = UNIXLIKE;
		ReverseBytesOnRead = TRUE;
	} else {
		BinaryType = NONE;
		return(FALSE);
	}
	return(TRUE);
}


static BOOL RMBinaryMode(FILE *funit) {
	
	UINT32 reclen;											/* Must be a 4 byte integer */

/* ------
-- If funit NULL, reading via pipe and must defer until first RMReadRecord.
-- Otherwise, read first 4 bytes, rewind file, and check via RMCheckFormat80.
--------- */
	if (funit == NULL) {
		BinaryType = NONE;								/* Assume we don't know		*/
		return(TRUE);
	}

	if (fread(&reclen, sizeof(reclen), 1, funit) != 1) return(FALSE);
	fseek(FileHandle, 0L, SEEK_SET);					/* Rewind */
	return(RMCheckFormat80(reclen));
}
	
/* ------------------------------------------------------------------------- */
static BOOL RMReadText(char *token) {
	int i;
	if (RMReadRecord(token, 1, 80) != 0) return(FALSE);
	for (i=79; i>=0; i--) {if (token[i] != ' ') break;}
	token[++i] = '\0';
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static BOOL RMReadScale(REAL *xfact, REAL *xoff, REAL *yfact, REAL *yoff) {
	REAL32 buf[4];
	if (RMReadRecord(buf, sizeof(*buf), 4) != 0) return(FALSE);
	*xfact = buf[0];
	*xoff  = buf[1];
	*yfact = buf[2];
	*yoff  = buf[3];
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static BOOL RMRead2Scale(REAL *xfact, REAL *xoff) {
	REAL32 buf[2];
	if (RMReadRecord(buf, sizeof(*buf), 2) != 0) return(FALSE);
	*xfact = buf[0];
	*xoff  = buf[1];
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static BOOL RMReadInt(INT *npt) {

	int   rcode;
	INT16 int16;
	INT32 int32;

	rcode = RMReadAltRecords(&int16, sizeof(int16), 1, &int32, sizeof(int32), 1);
	if (rcode == 1) {
		*npt = (INT) int16;
	} else if (rcode == 2) {
		*npt = (INT) int32;
	} else {
		return(FALSE);
	}
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static BOOL RMRead2Int(INT *npt, INT *ncol) {

	int   rcode;
	INT16  sbuf[2];								/* Read short ints */
	INT32  lbuf[2];								/* Read long ints */

	rcode = RMReadAltRecords(sbuf, sizeof(*sbuf), 2, lbuf, sizeof(*lbuf), 2);
	if (rcode == 1) {								/* Short buffer filled */
		*npt  = (INT) sbuf[0];
		*ncol = (INT) sbuf[1];
	} else if (rcode == 2) {
		*npt  = (INT) lbuf[0];
		*ncol = (INT) lbuf[1];
	} else {
		return(FALSE);
	}
	return(TRUE);
}

/* ------------------------------------------------------------------------- */
static BOOL RMReadArray(REAL *x, int len) {

#ifndef REAL_IS_DOUBLE
	return(RMReadRecord(x, sizeof(*x), len) == 0);
#else
	REAL32 *xr;
	int i, rcode;
	xr = malloc(sizeof(*xr)*len);
	if ( (rcode = RMReadRecord(xr, sizeof(*xr), len)) == 0) {
		for (i=0; i<len; i++) x[i] = xr[i];
	}
	free(xr);
	return( (rcode==0) );
#endif

}

/* ------------------------------------------------------------------------- */
static BOOL RMDumpReals(int len) {
	return(RMReadRecord(NULL, sizeof(REAL32), len)==0);
}

/* ------------------------------------------------------------------------- */
static BOOL RMWriteText(char *token) {
	int i=80;
	char buf[80], *aptr=buf;
	memset(buf, ' ', 80);
	while ((*token != '\0') && i--) *(aptr++) = *(token++);
	return(RMWriteRecord(buf, sizeof(*buf), 80));	/* 80 items, each 1 byte */
}

/* ------------------------------------------------------------------------- */
static BOOL RMWriteShorts(int npt, int ncol) {

	INT16 buf[2];
	buf[0] = (INT16) npt;
	buf[1] = (INT16) ncol;
	return(RMWriteRecord(buf, sizeof(*buf), 2));		/* 2 items to write */
}

/* ------------------------------------------------------------------------- */
static BOOL RMWriteLongs(int npt, int ncol) {

	INT32 buf[2];
	buf[0] = (INT32) npt;
	buf[1] = (INT32) ncol;
	return(RMWriteRecord(buf, sizeof(*buf), 2));		/* 2 items to write */
}

/* ------------------------------------------------------------------------- */
static BOOL RMWriteArray(REAL *x, int len) {

#ifndef REAL_IS_DOUBLE
	BOOL result;

	result = RMWriteRecord(x, sizeof(*x), len);
	return(result);
#else
	REAL32 *xr;
	int i, result;

	xr = malloc(sizeof(*xr)*len);
	for (i=0; i<len; i++) xr[i] = GVTrimToFloat(x[i]);
	result = RMWriteRecord(xr, sizeof(*xr), len);
	free(xr);
	return(result);
#endif
}

/* -------------------------------------------------------------------------
-- Routine to read Ryan-McFarland style records from disk to fill len bytes.
-- Will read multiple records from known maximum size RMBUFSIZE.
--
-- Usage: int RMReadRecord(void *buf, size_t size, size_t count);
--
-- Inputs: len - number of bytes we want to read (ignoring blocking bytes)
--
-- Output: Fill buf with the bytes read (guarenteed to read exactly len)
--
-- Return:  0 - everything succeeded
--         -1 - read failure.  Unable to read or mismatch of block bytes
--         -2 - record was not of expected size.  File is rewound to the
--              start of record length to allow multiple attempts
--
-- Modifications:
-- (1) Will now also permit read of full buffer in one record if the length
--     appears correct.  Incompatible with DOS, but okay and useful to simply
--     new programming.
-- (2) All byte reordering is now done in this segment rather than scattered
--     about.  Size and count must be correct now!
--------------------------------------------------------------------------- */
static int RMReadRecord(void *buf, size_t size, size_t count) {

	UINT32 reclen;
	size_t iread, extra, icnt;
	char *obuf, *obufstart;
	int rcode;

	rcode = -1;											/* Default error return			*/
	icnt = size*count;								/* Actual # of bytes to read	*/
	obuf = (char *) buf;								/* Pointer to buffer				*/
	if (obuf == NULL) obufstart = obuf = malloc(icnt);

	while (icnt) {
		if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) break;
		if (BinaryType==NONE && ! RMCheckFormat80(reclen)) break;	/* Check now? */
		if (ReverseBytesOnRead) RMReverse(&reclen, sizeof(reclen), 1);
		extra = (BinaryType==RMFTN) ? 8 : 0;	/* Extra count on record length */

		if (reclen == icnt+extra) {				/* Is record expected size? */
			iread = icnt;
		} else if ( (icnt > RMBUFSIZE) && (reclen == RMBUFSIZE+extra) ) {
			iread = RMBUFSIZE;
		} else {
			fseek(FileHandle, -((long) sizeof(reclen)), SEEK_CUR);
			rcode = -2;
			break;
		}

		if (fread(obuf, iread, 1, FileHandle) != 1) break;
		if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) break;
		if (ReverseBytesOnRead) RMReverse(&reclen, sizeof(reclen), 1);
		if (reclen != iread+extra) break;

		icnt -= iread;
		obuf += iread;
	}

	if (buf == NULL) {								/* Deallocate dummy space? */
		free(obufstart);
	} else if (icnt == 0 && ReverseBytesOnRead) {
		RMReverse(buf, size, count);
	}

	return( (icnt==0) ? 0 : rcode);
}

/* -------------------------------------------------------------------------
-- Routine to read next record which may be one of two sizes.  Only occurs for
-- read of npt/ncol which jumped to 4 bytes to handle > 65536 points.
--
-- Usage: int RMReadAltRecords(void *buf1, size_t size1, size_t count1, void *buf2, size_t size2, size_t count2);
--
-- Inputs: len1 - number of bytes we want to read (ignoring blocking bytes)
--         len2 - number of bytes in alternate structure
--
-- Output: Fill buf1/buf2 with the bytes read (guarenteed to read exactly len)
--
-- Return: +1 - read len1 bytes into buf1
--         +2 - read len2 bytes into buf2
--         -1 - read failure.  Unable to read or mismatch of block bytes
--         -2 - record was not of expected size.  File is rewound to the
--              start of record length to allow multiple attempts
--
-- Notes: len1/len2 must both be less than one record RMBUFSIZE
--------------------------------------------------------------------------- */
static int RMReadAltRecords(void *buf1, size_t size1, size_t count1, void *buf2, size_t size2, size_t count2) {

	UINT32 reclen;
	size_t iread, extra, len1, len2, size, count;
	int rcode;
	void *buf;

	len1 = size1 * count1;							/* Byte counts */
	len2 = size2 * count2;

	if (len1>RMBUFSIZE || len2>RMBUFSIZE || len1==0 || len2==0) return(-1);

	if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) return(-1);
	if (ReverseBytesOnRead) RMReverse(&reclen, sizeof(reclen), 1);
	extra = (BinaryType==RMFTN) ? 8 : 0;		/* Extra count on record length */

	if (reclen == len1+extra) {
		iread = len1;
		size  = size1;
		count = count1;
		buf   = buf1;
		rcode = 1;
	} else if (reclen == len2+extra) {
		iread = len2;
		size  = size2;
		count = count2;
		buf   = buf2;
		rcode = 2;
	} else {
		fseek(FileHandle, -((long) sizeof(reclen)), SEEK_CUR);
		return(-2);
	}

	if (fread(buf, iread, 1, FileHandle) != 1) return(-1);
	if (fread(&reclen, sizeof(reclen), 1, FileHandle) != 1) return(-1);
	if (ReverseBytesOnRead) RMReverse(&reclen, sizeof(reclen), 1);
	if (reclen != iread+extra) return(-1);

	if (ReverseBytesOnRead) RMReverse(buf, size, count);
	return(rcode);
}


/* -------------------------------------------------------------------------
-- RMWriteRecord always writes in the DOS compatibility mode.  Fakes it as
-- if written on a PC (little_endian) and with the RMFORT record format.
--
-- Data will be byte reversed if necessary, and then returned to original
-- ordering at end.
--
-- On non-LITTLE_ENDIAN machines, byte order will be reversed to become
-- DOS compatible.  Records are written in RM-FORTRAN format and are limited
-- to 4096 bytes in compliance with DOS GENPLOT requirements.
--------------------------------------------------------------------------- */
static BOOL RMWriteRecord(void *buf, size_t size, size_t count) {

	size_t icnt, iwrite;
	UINT32 reclen;
	char *obuf;

	if (count == 0) return(TRUE);							/* Well, don't want anything, okay with me */
	if (size < 1 || count < 1) return(FALSE);			/* Avoid problems */

#if (BYTE_ENDIAN_ORDER != LITTLE_ENDIAN)				/* pc is a little_endian */
	RMReverse(buf, size, count);
#endif

	icnt = size*count;										/* # of bytes to write	*/
	obuf = (char *) buf;										/* Pointer to data		*/

	while (icnt) {
		iwrite = min(RMBUFSIZE, icnt);					/* Number to write now */
		reclen = (UINT32) iwrite+8;
#if (BYTE_ENDIAN_ORDER != LITTLE_ENDIAN)				/* pc is a little_endian */
		RMReverse(&reclen, sizeof(reclen), 1);
#endif
		if (fwrite(&reclen, sizeof(reclen), 1, FileHandle) != 1) break;
		if (fwrite(obuf, iwrite, 1, FileHandle) != 1) break;
		if (fwrite(&reclen, sizeof(reclen), 1, FileHandle) != 1) break;
		icnt -= iwrite;
		obuf += iwrite;
	}

#if (BYTE_ENDIAN_ORDER != LITTLE_ENDIAN)			/* pc is a little_endian */
	RMReverse(buf, size, count);
#endif

	return(icnt == 0);										/* Successful if get 0 */
}


/* ------------------------------------------------------------------------- */
static void RMReverse(void *buffer, size_t width, size_t num) {

	char *buf=buffer, achr;
	char tmpbuf[8], *aptr;
	register size_t i;

	if (width == 2) {										/* Do efficiently if possible */
		for (i=0; (int) i<2*num; i+=2) {
			achr = buf[i];
			buf[i] = buf[i+1];
			buf[i+1] = achr;
		}
	} else if (width == 4) {
		for (i=0; (int) i<4*num; i+=4) {
			achr = buf[i];
			buf[i] = buf[i+3];
			buf[i+3] = achr;
			achr = buf[i+1];
			buf[i+1] = buf[i+2];
			buf[i+2] = achr;
		}
	} else if (width > 1) {
		while (num--) {
			memcpy(tmpbuf, buf, width);
			aptr = tmpbuf+width;
			for (i=0; i<width; i++) *(buf++) = *(--aptr);
		}
	}
	return;
}
