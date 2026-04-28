#ifndef __mytypes
   #define __mytypes

/* ----------------------------------------------------------------------------
-- This file contains definitions of several basic variables types PLUS
-- definitions of basic system extended routines which are useful in a
-- wide variety of situations.  By including these functions (POSIX, SYS
-- routines) here, other programs can make use of these declaratiosn and
-- popular utilities by including this file.
--
-- To use these routines, combine the POSIX routines into a single file,
-- and combine sys_1.c, sys_2.c and sys_3.c into a single file, compile
-- and add to your routines.
--
-- This file should be loaded AFTER all system include files have been loaded
-- but before any of the local include files.  Certain constants and macros
-- are defined if they have not previously been given acceptable values in the
-- system.  Examples: TRUE, FALSE, O_BINARY
--
-- Other variable definitions are in extends.h which has both extended
-- typedef's and function declarations.
---------------------------------------------------------------------------- */

/* --------------------------------------------------------------------- */
/* --- ANSI/POSIX optional and modified constructs handled first     --- */
/* --- Only the libraries where some system has failed to define a   --- */
/* --- needed ANSI/POSIX constant are force loaded here. Inefficient --- */
/* --- but I can't help it if there is no consistent standard (MOT)  --- */
/* --------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

/* ---------------------------------------------------------------------------
-- Deal with two choices (of several) for byte ordering from disk to memory.
-- Choices basically are least significant byte first (i486 format) or the
-- most significant byte first (sun, sparc and 68000 format).  These are
-- designated as LITTLE_ENDIAN and BIG_ENDIAN for reasons unknown.
--
-- This is one of the few places that the architecture setting is examined.
-- One of these must be enabled to survive testing.
--------------------------------------------------------------------------- */
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (pc)       */
#define	BIG_ENDIAN	   4321	/* most-significant  byte first (motorola) */

#if   defined(i386)     || defined(i486)    || defined(i586) || \
      defined(DECalpha) || defined(vax)     || \
      defined(sun386)   || defined(ns32000) || \
      defined(BIT_ZERO_ON_RIGHT)
	#define BYTE_ENDIAN_ORDER LITTLE_ENDIAN
#elif ( defined(sel)      || defined(pyr)     || defined(mc68000) || \
      defined(is68k)    || defined(tahoe)   || defined(sun)     || \
      defined(RISC6000) || defined(aiws)    || defined(_IBMESA) || \
      defined(sparc)    || defined(convex)  || \
      defined(MacInToy) || \
      defined (BIT_ZERO_ON_LEFT) )
	#define BYTE_ENDIAN_ORDER BIG_ENDIAN
#else
	#error *** MACHINE ARCHITECTURE NOT DEFINED - FATAL ***
#endif

/* For basic redefinable operation */
#define	EXTERN	extern 
#define	PRIVATE	static
#define	STATIC	static

#if (defined MSC70 && ! defined STATIC_LINK)
	#define	EXPORT			 __declspec(dllexport)
	#define	IMPORT	EXTERN __declspec(dllimport)
#else
	#define	EXPORT	
	#define	IMPORT	EXTERN
#endif

/* ------- FIRST, SIZE SPECIFIC TYPEDEFS  ---------- */
#if (defined MSC70 || defined LINUX)
	#include <stdint.h>				/* Not standard C, but hopefully exists */
											/* Corresponding UINT64_MAX defined in stdint.h */
	typedef	int8_t	INT8;			/*  8 bit integers */
	typedef	uint8_t	UINT8;		/*  8 bit integers */
	typedef	int16_t	INT16;		/* 16 bit integers */
	typedef	uint16_t	UINT16;		/* 16 bit integers */
	typedef	int32_t	INT32;		/* 32 bit integers */
	typedef	uint32_t	UINT32;		/* 32 bit integers */
	#ifdef INT64_MAX
		typedef	int64_t	INT64;		/* 64 bit integers */
		typedef	uint64_t	UINT64;		/* 64 bit integers */
	#endif

#else
	#error *** Deal with stdint.h in mytypes.h - FATAL ***
#endif

#if 0													/* These are defined in stdint.h */
	#define	INT8_MIN		(SCHAR_MIN)
	#define	INT8_MAX		(SCHAR_MAX)
	#define	UINT8_MAX	(UCHAR_MAX)
	#define	INT16_MIN	(SHRT_MIN)
	#define	INT16_MAX	(SHRT_MAX)
	#define	UINT16_MAX	(USHRT_MAX)
	#define	INT32_MIN	(INT_MIN)
	#define	INT32_MAX	(INT_MAX)
	#define	UINT32_MAX	(UINT_MAX)
	#define	INT64_MIN	(LLONG_MIN)
	#define	INT64_MAX	(LLONG_MAX)
	#define	UINT64_MAX	(ULLONG_MAX)
#endif
	
typedef	float						REAL32;		/* 4 byte real number		*/
typedef	double					REAL64;		/* 8 byte real number		*/

/* Corresponding min/max ranges */

/* ------ Need an INT_PTR to be same size as a pointer ----- */
/* ------ In windows, windows.h defines INT_PTR and UINT_PTR */

#ifndef PIPE_BUF							/* Should be in <limits.h> (quasi-POSIX) */
	#define PIPE_BUF 4096
#endif

#ifndef PATH_MAX							/* Should be in <limits.h> (quasi-POSIX) */
	#ifdef _POSIX_PATH_MAX
		#define PATH_MAX	_POSIX_PATH_MAX
	#else
		#define PATH_MAX 255
	#endif
#endif

#ifndef TRUE							/* Any non-zero better be considered TRUE */
	#define	TRUE			(1)
#endif
#ifndef FALSE							/* If FALSE is not 0, we don't have C	*/
	#define	FALSE			(0)
#endif
#ifndef O_BINARY						/* UNIX may not have binary file concept */
	#define	O_BINARY		0x00
#endif
#ifndef max								/* These are not really in STDLIB.H */
	#define	max(a,b)		(((a) > (b)) ? (a) : (b))
#endif
#ifndef min
	#define	min(a,b)		(((a) < (b)) ? (a) : (b))
#endif

/* -- Define REAL_IS_DOUBLE here or in makexxx.h to get internal vars double */
/* #define REAL_IS_DOUBLE */				/* Default is float */ 

/* -- If there is a long double, you can try TMPREAL_IS_LONG */
/*	#define TMPREAL_IS_LONG */				/* Default is double */

/* ---------------------------------------------------------------------------
-----  No user servicable parts below   ------
--
-- Usage of INTEGER or LOGICAL is now highly discouraged.  Use INT or BOOL
-- instead.
--------------------------------------------------------------------------- */
typedef 	int		INTEGER;						/* Integer (use INT instead)		*/
typedef 	int		LOGICAL;						/* Logical (use BOOL instead)		*/

typedef	float		FLOAT;						/* In correspondence to INT, etc	*/
typedef	double	DOUBLE;						/* -- ditto --							*/

#if ! (defined OS2_INCLUDED || defined _OS2EMX_H)	/* Taken from basedef.h	*/
	typedef int INT;								/* i   Signed integer		*/
	typedef unsigned int UINT;					/* u   Unsigned integer		*/
	typedef INT BOOL;								/* f   Logical variable		*/

	typedef unsigned char BYTE;				/* b   Byte type				*/
	typedef unsigned short WORD;				/* w   Word type				*/
	typedef unsigned long DWORD;				/* dw  Double word type		*/

	typedef char CHAR;							/* ch  Signed char			*/
	typedef unsigned char UCHAR;				/* uch Unsigned char			*/
	typedef short SHORT;							/* s   Signed short int		*/
	typedef unsigned short USHORT;			/* us  Unsigned short int	*/
	typedef long LONG;							/* l   Signed long int		*/
	typedef unsigned long ULONG;				/* ul  Unsigned long int	*/
#endif

#ifdef REAL_IS_DOUBLE								/* Either float or double */
	typedef double			REAL;
	#define REAL_MAX		DBL_MAX
	#define REAL_MIN		DBL_MIN
	#define REAL_EPSILON	DBL_EPSILON
#else
	typedef float			REAL;
	#define REAL_MAX		FLT_MAX
	#define REAL_MIN		FLT_MIN
	#define REAL_EPSILON	FLT_EPSILON
#endif

#ifdef TMPREAL_IS_LONG							/* Either long or double */
	typedef long double	TMPREAL;
	#define	TMPREAL_MAX			LDBL_MAX
	#define	TMPREAL_MIN			LDBL_MIN
	#define	TMPREAL_EPSILON	LDBL_EPSILON
#else
	typedef double			TMPREAL;
	#define	TMPREAL_MAX			DBL_MAX
	#define	TMPREAL_MIN			DBL_MIN
	#define	TMPREAL_EPSILON	DBL_EPSILON
#endif

/* ---------------------------------------------------------------------------
-- Routines which I now consider a base part of the OS
--------------------------------------------------------------------------- */
void		SysPanic(char *file, int line);
long int	SysQueryMem(void);
BOOL		SysSetDisk(char disk);

void		SysSplitPath(const char *path, char *dir, char *file, char *ext);
void		SysMakePath (char *path, const char *dir, const char *file, const char *ext);
void     SysMarkPath (const char *path, char **fname, char **dirnull);

char	  *SysAddExt(char *pathname, char *newext);
char	  *SysReplaceExt(char *pathname, char *newext);
char	  *SysDeleteExt(char *pathname);

int		SysFileCheck(char *name, char *fulldir, char *filename);
BOOL		SysCheckMatch(char *string, char *pattern);

BOOL		SysFindFile(char *fullpath, char *base, char *path, char *exts, int amode);
BOOL		SysFindFileGz(char *fullpath, char *base, char *path, char *exts, int amode);
char    *SysQualifyPath(char *buffer, char *path, size_t buffer_len);
int		SysCompleteFilename(char *pattern, char *name, size_t maxlen);
char	  *SysTmpFilename(char *dfltdir, char *suffix);
FILE	  *SysTmpFile(char *pathname, char *dfltdir, char *ext, char *mode);

int		SysMoveFile(char *old, char *new);

/* Arbitrary line reading function */
typedef struct ARBSTR {
	char *buf;
	int size;
} ARBSTR;
#define	B_STRIP_LEADING			0x01
#define	B_STRIP_TRAILING		0x02
#define	B_SKIP_BLANK				0x04
#define	B_SKIP_COMMENTS			0x08
#define	B_ALLOW_CONTINUES		0x10
int SysReadLongLine(ARBSTR *inbuf, FILE *funit, int bFlags);

/* ===========================================================================
-- Other routines
=========================================================================== */

/* --- String and memory functions --- */
size_t	 strnblen(const char *string);
char		*strscpy(char *s1, const char *s2, size_t len);
#ifndef NT
	#if !(defined AIX_C && defined _ALL_SOURCE)
		char	*strdup(const char *string);
	#endif
char		*strlwr(char *str);
char		*strupr(char *str);
int		 stricmp(const char *str1, const char *str2);
int		 strnicmp(const char *str1, const char *str2, size_t count);
int		 memicmp(const void *buf1, const void *buf2, size_t count);
#endif
int		 LexEqual(const char *tok1, const char *tok2, INTEGER minlen);

/* --- Replacement function --- */
#ifndef NT
	#define	System(request)	system(request);
#else
	int System(const char *request);		/* From POSIX\NT\pipes.c */
	IMPORT BOOL SysGUIMode;					/* From POSIX\NT\pipes.c */
#endif

/* --- Random functions --- */
int		nint(double val);
unsigned int NanoSleep(unsigned long timeval);
unsigned int MilliSleep(unsigned long timeval);

/* --- File and pathname control operations --- */
int		 ftrunc(FILE *stream, long posn);
int		 fprivate(FILE *stream);
#if (defined OS2)
	#include <io.h>
	#include <conio.h>
	#ifdef GNU_C
		#ifdef _UNISTD_H					/* If user loaded unistd.h, add process.h */
			#include <sys/process.h>
		#endif
	#else
		#define	ftruncate(stream,posn)  chsize(stream, posn)
	#endif
#elif (defined NT)
	#include <io.h>
	#define	ftruncate(stream,posn)  chsize(stream, posn)
#endif

#define	DRIVE_MAX	3			/* Maximum length for DRIVE component of name */
#ifndef NT
char	  *_fullpath(char *buffer, const char *path, size_t maxlen);
#endif

/* ---------------------------------------------------------------------------
-- UNIX routines don't recognize the shell alias ~name/dir.dir.  Rather than
-- fight to remember to handle manually, replace the routines which use
-- pathnames with alternate routines that handle the translation.
--
-- For others, still have the problem of filenames potentially surrounded by
-- a single layer of quotes.  Handle!
--------------------------------------------------------------------------- */
#ifndef NO_PATH_EXTENSIONS				/* Turned off in sys_3.c */
	#include <stdio.h>					/* Need these definitions initially */
	#include <dirent.h>
	#include <sys/types.h>
	#include <sys/stat.h>

	#define	access(path,amode)	E_ACCESS(path,amode)
	#define	chdir(path)				E_CHDIR(path)
	#define	chmod(path,mode)		E_CHMOD(path,mode)
	#define	fopen(path,mode)		E_FOPEN(path,mode)
	#define	freopen(path,mode,s)	E_FREOPEN(path,mode,s)
	#define	opendir(dirname)		E_OPENDIR(dirname)
	#define	remove(path)			E_REMOVE(path)
	#define	rename(old,new)		E_RENAME(old,new)
	#define	rmdir(path)				E_RMDIR(path)
	#define	stat(path,buf)			E_STAT(path,buf)
	#define	unlink(path)			E_UNLINK(path)

	int	E_ACCESS(const char *path, int amode);
	int	E_CHDIR(const char *path);
	int	E_CHMOD(const char *path, mode_t mode);
	FILE *E_FOPEN(const char *path, const char *mode);
	FILE *E_FREOPEN(const char *path, const char *mode, FILE *stream);
	DIR  *E_OPENDIR(const char *dirname);
	int	E_REMOVE(const char *path);
	int	E_RENAME(const char *old, const char *new);
	int	E_RMDIR(const char *path);
	int	E_STAT(const char *path, struct stat *buf);
	int	E_UNLINK(const char *path);

	#if (defined UNIX || defined LINUX)
		#define	pathconf(path,name)	E_PATHCONF(path,name)
		long	E_PATHCONF(const char *path, int name);
	#endif

#endif /* ! NO_PATH_EXTENSIONS */

/* =========================================================== */
/* LINUX -- clean up the compile as much as possible           */
/* The routines popen and pclose exist but are not valid POSIX */
/* routines.  Rather than fight in each file with modified     */
/* compile options, just add the prototype for them here.      */		
/* =========================================================== */
#ifdef LINUX
   FILE *popen(const char *command, const char *type);
	int pclose(FILE *stream);
#endif


/* ------- SECOND, SYSTEM DECLARATIONS IF NECESSARY ---------- */
/* ------- At some point, I'm going to dump these ------------ */
#ifdef GNU_EXTENSIONS
	#include <stdlib.h>
	#include <time.h>
	typedef int sig_atomic_t;
	void *memcpy (void *s1, const void *s2, size_t n);
	void *memmove(void *s1, const void *s2, size_t n);
	char *strerror(int errcode);
	void *REALLOC(void *memblock, size_t size);

	typedef unsigned long fpos_t;
	#define realloc(ptr,size) REALLOC(ptr,size)		/* Fix old GCC fuck-up */
	#define fgetpos(f,p) (*(p)=ftell(f))				/* Just don't exist??  */
	#define fsetpos(f,p) (fseek(f,*(p),SEEK_SET))		

	#ifdef NULL									/* Redefine this puppy so malloc okay */
		#undef NULL
	#endif
	#define NULL (void *) 0
	#ifndef EXIT_SUCCESS						/* Should be in <stdlib.h> */
		#define EXIT_SUCCESS 0
	#endif
	#ifndef EXIT_FAILURE						/* Should be in <stdlib.h> */
		#define EXIT_FAILURE -1
	#endif
	#ifndef CLOCKS_PER_SEC					/* Should be in <time.h>	*/
		#define CLOCKS_PER_SEC 1
	#endif
	#ifndef RAND_MAX							/* Should bin in <stdlib.h> */
		#define RAND_MAX 2147483647
	#endif

#endif	/* GNU_EXTENSIONS */
/* ------- END OF SYSTEM MODIFICATIONS ---------- */

#endif	/* __mytypes */
