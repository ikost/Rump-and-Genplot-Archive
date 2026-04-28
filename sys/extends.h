#ifndef __extends
   #define __extends

/* ==========================================================================
-- extends.h - declarations for extensions to POSIX/ANSI C for Thompsonware
--
--	Copyright (c) 1991 - Computer Graphics Service, Ltd.
--
-- Purpose:
--	  This file contains extensions to ANSI/POSIX needed for GENPLOT 
--   compilation.  These continue the extensions defined in mytypes.h
--   but are much more specific to GENPLOT and RUMP usage.  Other programs
--   will not normally make use of these routines and adding them to other
--   programs may present tough integration issues.
=========================================================================== */

#include <signal.h>

/* ---------------------------------------------------------------------------
-- Routines and stuff formerly in mytypes.h
--------------------------------------------------------------------------- */
/* Requirement is that VARNAME <= OPTION <= SHORT <= DFLT <= LONG */
/* Also, DFLT_STR should be >= PATH_MAX */
#define	VARNAME_STR_SIZE	32					/* Variable names limited	*/
#define	OPTION_STR_SIZE	32					/* For simple tests only	*/
#define	SHORT_STR_SIZE		80					/* Occasional short string	*/
#define	DFLT_STR_SIZE		256				/* Normal string size		*/
#define	LONG_STR_SIZE		2048				/* Long strings				*/
#define	HUGE_STR_SIZE		16384				/* Mostly max line length	*/

#if defined(AIX_C) || (defined(DECalpha) && defined(OSF1_C)) || (defined(GNU_C) && defined(RISC6000))
	typedef sig_atomic_t					VOLATILE_SIG_ATOMIC_T;
#else
	typedef volatile sig_atomic_t		VOLATILE_SIG_ATOMIC_T;
#endif

typedef struct _COMPLEX {						/* Normal use COMPLEX type */
	REAL x,y;
} COMPLEX;
typedef struct _TMPCOMPLEX {					/* Temporary internal COMPLEX */
	TMPREAL x,y;
} TMPCOMPLEX;

/* For the surface, the array Z holds the values in column major order -
-- all rows of column 0, followed by rows of column 1, ...                 */
typedef struct _SURFACE_STRUCT {
	char	ids[DFLT_STR_SIZE];				/* Name of the surface					*/
	char	options[DFLT_STR_SIZE];			/* Options used in plotting			*/
	INT	nrowmax, ncolmax, nptmax;		/* Cols (x), rows (y), points			*/
	INT	nrow, ncol, npt;					/* Cols (x), rows (y), points			*/
	REAL	*x;									/* x value of each col (ncol)			*/
	REAL	*y;									/* y value of each row (nrow)			*/
	REAL	*z;									/* z values at x,y (nrow*ncol)		*/
} SURFACE;

typedef struct _CURVE_STRUCT {
	char	ids[DFLT_STR_SIZE];				/* Name of the curve						*/
	char	options[DFLT_STR_SIZE];			/* Options used in plotting			*/
	INT	nptmax;								/* Maximum # of points in curve		*/
	INT	npt;									/* Current # of points in curve		*/
	REAL	*x;									/* Pointer to X							*/
	REAL	*y;									/* Pointer to Y							*/
	REAL	*z;									/* Pointer to Z							*/
} CURVE;

typedef struct _ARRAY_STRUCT {
	INT	maxsize;
	INT	*size;
	REAL	*x;
} ARRAY;

typedef struct _INT_ARRAY_STRUCT {
	INT	maxsize;
	INT	*size;
	INT	*ival;
} INT_ARRAY;

typedef struct _STRING_ARRAY_STRUCT {
	INT	maxsize;
	INT	*size;
	char	**sval;
} STRING_ARRAY;

typedef struct _DOUBLE_ARRAY_STRUCT {
	INT	maxsize;
	INT	*size;
	DOUBLE *x;
} DOUBLE_ARRAY;

typedef struct _COMPLEX_ARRAY_STRUCT {
	INT	maxsize;
	INT	*size;
	COMPLEX	*z;
} COMPLEX_ARRAY;

/* We carry around and use colors.  These may be index lookups into the
-- current pallette, an RGB color, or a special color object.  All are
-- packed into a single 32 bit integer.  The high order byte is used to
-- flag special features.  If it is 00, then the lower pattern is a simple
-- pen color. */
#define	CLR_RGB					0x01000000				/* RGB pattern				*/
#define	CLR_TRANSPARENT		0x02000000				/* Transparent object	*/
#define	CLR_NOMARK				0x04000000				/* No pen marks			*/
#define	MY_RGB(r,g,b)			( (INT32) (CLR_RGB | (r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16)) )
#define	IS_PEN(rgb)				( (rgb & 0xFF000000) == 0)
#define	IS_TRANSPARENT(rgb)	( (rgb & CLR_TRANSPARENT) != 0)
#define	IS_NOMARK(rgb)			( (rgb & CLR_NOMARK) != 0)
#define	IS_RGB(rgb)				( (rgb & CLR_RGB) != 0)
#define	R_FROM_RGB(rgb)		((int) (rgb      ) & 0xFF)
#define	G_FROM_RGB(rgb)		((int) (rgb >>  8) & 0xFF)
#define	B_FROM_RGB(rgb)		((int) (rgb >> 16) & 0xFF)

#if defined MSC60 && (defined _MT || defined _DLL)
	#define	SIGHANDLERTYPE	_cdecl _far _loadds
#else
	#define	SIGHANDLERTYPE
#endif

/* ---------------------------------------------------------------------------
-- Bitwise function allowing routines to modify the behavior of the console
-- code.  Required for use with special features of the XGENPLOT NT code.
--------------------------------------------------------------------------- */
#ifdef LOW_IO_C_SOURCE
	EXPORT int SysConsoleBehavior;						/* Bit-wise feature word */
#else
	IMPORT int SysConsoleBehavior;						/* Bit-wise feature word */
#endif
	#define	SYSCONSOLE_PASS_CTRL_Z	0x01


/* ---------------------------------------------------------------------------
-- Local console information.
--------------------------------------------------------------------------- */
typedef struct _SCRINFO {
	int rows;									/* # of rows on display			*/
	int cols;									/* # of cols on display			*/
	int buf_rows;								/* If ! -1, # rows in buffer	*/
	int buf_cols;								/* If ! -1, # cols in buffer	*/
	int LowerScrollRow;
	int UpperScrollRow;
} SCRINFO;

#ifdef SCREEN_C_SOURCE
	EXPORT SCRINFO *ScrInfo;					/* Global accessible information		*/
#else
	IMPORT SCRINFO *ScrInfo;					/* Global accessible information		*/
#endif

/* ======================================================================== */

#define	D_NORMAL		0x20			/* Normal color for text output	*/
#define	D_INPUT		0x21			/* Normal color for text input	*/
#define	D_BOLD		0x22			/* Normal color, but bold			*/
#define	D_PROMPT		0x23			/* Normal color for text prompts	*/
#define	D_ERROR		0x24			/* Color for ERROR messages		*/
#define	D_FILEIN		0x25			/* Color for echo of file input	*/

/* -------------------------------------------------------------------
-- To replace existing list of attributes, set SysAttribList to a
-- structure relating the mode (above) with and escape sequence.  
-- Last entry in the list must be 0xFF as mode with default value.
------------------------------------------------------------------- */
typedef struct _SYSATTRIBLIST {
	int mode;
	char *string;
} SYSATTRIBLIST;

#ifdef LOW_IO_C_SOURCE
	EXPORT SYSATTRIBLIST *SysAttribList;
#else
	IMPORT SYSATTRIBLIST *SysAttribList;
#endif

/*** Virtual key values */
#ifndef NT											/* Of course NT must be differnt */
	#define VIRTUAL_BUTTON1         0x101			/* Virtual key values */
	#define VIRTUAL_BUTTON2         0x102
	#define VIRTUAL_BUTTON3         0x103
	#define VIRTUAL_BREAK           0x104
	#define VIRTUAL_BACKSPACE       0x105
	#define VIRTUAL_TAB             0x106
	#define VIRTUAL_BACKTAB         0x107
	#define VIRTUAL_NEWLINE         0x108
	#define VIRTUAL_SHIFT           0x109
	#define VIRTUAL_CTRL            0x10A
	#define VIRTUAL_ALT             0x10B
	#define VIRTUAL_ALTGRAF         0x10C
	#define VIRTUAL_PAUSE           0x10D
	#define VIRTUAL_CAPSLOCK        0x10E
	#define VIRTUAL_ESC             0x10F
	#define VIRTUAL_SPACE           0x110
	#define VIRTUAL_PAGEUP          0x111
	#define VIRTUAL_PAGEDOWN        0x112
	#define VIRTUAL_END             0x113
	#define VIRTUAL_HOME            0x114
	#define VIRTUAL_LEFT            0x115
	#define VIRTUAL_UP              0x116
	#define VIRTUAL_RIGHT           0x117
	#define VIRTUAL_DOWN            0x118
	#define VIRTUAL_PRINTSCRN	     0x119
	#define VIRTUAL_INSERT          0x11A
	#define VIRTUAL_DELETE          0x11B
	#define VIRTUAL_SCRLLOCK        0x11C
	#define VIRTUAL_NUMLOCK         0x11D
	#define VIRTUAL_ENTER           0x11E
	#define VIRTUAL_SYSRQ           0x11F

	#define VIRTUAL_SHIFT_TAB		  0x120		/* Made up (see xdisplay.c) */
	#define VIRTUAL_CTRL_PAGEUP     0x121
	#define VIRTUAL_CTRL_PAGEDOWN   0x122
	#define VIRTUAL_CTRL_END        0x123
	#define VIRTUAL_CTRL_HOME       0x124
	#define VIRTUAL_CTRL_LEFT       0x125
	#define VIRTUAL_CTRL_UP         0x126
	#define VIRTUAL_CTRL_RIGHT      0x127
	#define VIRTUAL_CTRL_DOWN       0x128
	#define VIRTUAL_CTRL_INSERT     0x12A
	#define VIRTUAL_CTRL_DELETE     0x12B

	#define VIRTUAL_F1              0x140
	#define VIRTUAL_F2              0x141
	#define VIRTUAL_F3              0x142
	#define VIRTUAL_F4              0x143
	#define VIRTUAL_F5              0x144
	#define VIRTUAL_F6              0x145
	#define VIRTUAL_F7              0x146
	#define VIRTUAL_F8              0x147
	#define VIRTUAL_F9              0x148
	#define VIRTUAL_F10             0x149
	#define VIRTUAL_F11             0x14A
	#define VIRTUAL_F12             0x14B
	#define VIRTUAL_F13             0x14C
	#define VIRTUAL_F14             0x14D
	#define VIRTUAL_F15             0x14E
	#define VIRTUAL_F16             0x14F
	#define VIRTUAL_F17             0x150
	#define VIRTUAL_F18             0x151
	#define VIRTUAL_F19             0x152
	#define VIRTUAL_F20             0x153
	#define VIRTUAL_F21             0x154
	#define VIRTUAL_F22             0x155
	#define VIRTUAL_F23             0x156
	#define VIRTUAL_F24             0x157
#else				/* NT version */
	#define VIRTUAL_BUTTON1         0x101		/* VK_LBUTTON	*/
	#define VIRTUAL_BUTTON2         0x102		/* VK_RBUTTON	*/
	#define VIRTUAL_BUTTON3         0x104		/* VK_MBUTTON	*/
	#define VIRTUAL_BREAK           0x103		/* VK_CANCEL	*/
	#define VIRTUAL_BACKSPACE       0x108		/* VK_BACK		*/
	#define VIRTUAL_TAB             0x109		/* VK_TAB		*/
	#define VIRTUAL_BACKTAB         	0x107
	#define VIRTUAL_NEWLINE         0x10D		/* VK_RETURN	*/
	#define VIRTUAL_SHIFT           0x110		/* VK_SHIFT		*/
	#define VIRTUAL_CTRL            0x111		/* VK_CONTROL	*/
	#define VIRTUAL_ALT             0x112		/* VK_MENU		*/
	#define VIRTUAL_ALTGRAF          0x10C
	#define VIRTUAL_PAUSE           0x113		/* VK_PAUSE		*/
	#define VIRTUAL_CAPSLOCK        0x114		/* VK_CAPITAL	*/
	#define VIRTUAL_ESC             0x11B		/* VK_ESCAPE	*/
	#define VIRTUAL_SPACE           0x120		/* VK_SPACE		*/
	#define VIRTUAL_PAGEUP          0x121		/* VK_PRIOR		*/
	#define VIRTUAL_PAGEDOWN        0x122		/* VK_NEXT		*/
	#define VIRTUAL_END             0x123		/* VK_END		*/
	#define VIRTUAL_HOME            0x124		/* VK_HOME		*/
	#define VIRTUAL_LEFT            0x125		/* VK_LEFT		*/
	#define VIRTUAL_UP              0x126		/* VK_UP			*/
	#define VIRTUAL_RIGHT           0x127		/* VK_RIGHT		*/
	#define VIRTUAL_DOWN            0x128		/* VK_DOWN		*/
	#define VIRTUAL_PRINTSCRN	     0x12A		/* VK_PRINT		*/
	#define VIRTUAL_INSERT          0x12D		/* VK_INSERT	*/
	#define VIRTUAL_DELETE          0x12E		/* VK_DELETE	*/
	#define VIRTUAL_SCRLLOCK        0x191		/* VK_SCROLL	*/
	#define VIRTUAL_NUMLOCK         0x190		/* VK_NUMLOCK	*/
	#define VIRTUAL_ENTER           0x12B		/* VK_EXECUTE	*/
	#define VIRTUAL_SYSRQ           0x1F6		/* VK_ATTN		*/

	#define VIRTUAL_SHIFT_TAB		  0x130		/* Made up (see xdisplay.c) */
	#define VIRTUAL_CTRL_PAGEUP     0x131
	#define VIRTUAL_CTRL_PAGEDOWN   0x132
	#define VIRTUAL_CTRL_END        0x133
	#define VIRTUAL_CTRL_HOME       0x134
	#define VIRTUAL_CTRL_LEFT       0x135
	#define VIRTUAL_CTRL_UP         0x136
	#define VIRTUAL_CTRL_RIGHT      0x137
	#define VIRTUAL_CTRL_DOWN       0x138
	#define VIRTUAL_CTRL_INSERT     0x13A
	#define VIRTUAL_CTRL_DELETE     0x13B

	#define VIRTUAL_F1              0x170		/* VK_F1			*/
	#define VIRTUAL_F2              0x171		/* VK_F2			*/
	#define VIRTUAL_F3              0x172		/* VK_F3			*/
	#define VIRTUAL_F4              0x173		/* VK_F4			*/
	#define VIRTUAL_F5              0x174		/* VK_F5			*/
	#define VIRTUAL_F6              0x175		/* VK_F6			*/
	#define VIRTUAL_F7              0x176		/* VK_F7			*/
	#define VIRTUAL_F8              0x177		/* VK_F8			*/
	#define VIRTUAL_F9              0x178		/* VK_F9			*/
	#define VIRTUAL_F10             0x179		/* VK_F10		*/
	#define VIRTUAL_F11             0x17A		/* VK_F11		*/
	#define VIRTUAL_F12             0x17B		/* VK_F12		*/
	#define VIRTUAL_F13             0x17C		/* VK_F13		*/
	#define VIRTUAL_F14             0x17D		/* VK_F14		*/
	#define VIRTUAL_F15             0x17E		/* VK_F15		*/
	#define VIRTUAL_F16             0x17F		/* VK_F16		*/
	#define VIRTUAL_F17             0x180		/* VK_F17		*/
	#define VIRTUAL_F18             0x181		/* VK_F18		*/
	#define VIRTUAL_F19             0x182		/* VK_F19		*/
	#define VIRTUAL_F20             0x183		/* VK_F20		*/
	#define VIRTUAL_F21             0x184		/* VK_F21		*/
	#define VIRTUAL_F22             0x185		/* VK_F22		*/
	#define VIRTUAL_F23             0x186		/* VK_F23		*/
	#define VIRTUAL_F24             0x187		/* VK_F24		*/
#endif
 
#define	SYSLOG_CHECK			0
#define	SYSLOG_OPEN				1
#define	SYSLOG_APPEND			2
#define	SYSLOG_SWITCH			3
#define	SYSLOG_CLOSE			4
#define	SYSLOG_WRITE			5
#define	SYSLOG_FORCE			6
#define	SYSLOG_TMPUNPAUSE		7
#define	SYSLOG_TMPPAUSE		8
#define	SYSLOG_PAUSE			9
#define	SYSLOG_UNPAUSE			10
#define	SYSLOG_SETMODE			11

/* Interprocess communication for exception/error handlers */
#ifdef SYS_OS_C_SOURCE
	EXPORT	INT32 SysDebugFlag;
	EXPORT	VOLATILE_SIG_ATOMIC_T	SysBreakFlag;
	EXPORT   VOLATILE_SIG_ATOMIC_T	SysBreakCount;
	EXPORT   VOLATILE_SIG_ATOMIC_T	SysXtermCharCount;		/* Way to determine chars waiting */
#else
	IMPORT	INT32 SysDebugFlag;
	IMPORT	VOLATILE_SIG_ATOMIC_T	SysBreakFlag;
	IMPORT   VOLATILE_SIG_ATOMIC_T	SysBreakCount;
	IMPORT   VOLATILE_SIG_ATOMIC_T	SysXtermCharCount;		/* Way to determine chars waiting */
#endif

BOOL		SysChkBreak(BOOL flag);
char	  *SysGetSysSearchPath(void);
char	  *SysSetSysSearchPath(char *newpath);
BOOL		SysResolveDyntName(char *path, const char *basename, int path_len);
BOOL     SysCheckLicense(char *product);
BOOL     SysModifyLicense(char *serial);
#ifdef SYS_OS_C_SOURCE
	EXPORT   char SysSerialNumber[];
	EXPORT   char SysSerialNumberEx[];
#else
	IMPORT   char SysSerialNumber[];
	IMPORT   char SysSerialNumberEx[];
#endif

BOOL		SysFileMore(char *filename);
BOOL		SysListDir(char *spec, char *option);
BOOL		SysPushShell(void);
int		SysSystem(char *command);

BOOL		SysHelpRequest(const char *module, const char *query);
void		SysSetHelpModule(const char *name);
BOOL		SysLogFile(INTEGER key, char *string, FILE **funit);

void		ScrClear			(void);
int		ScrClearAttrib	(INTEGER attrib);
int		ScrSetAttrib	(INTEGER attrib);
void		ScrSetPosn		(INTEGER row,INTEGER col,INTEGER attrib);
void		ScrPutString	(INTEGER row, INTEGER col, char *string, INTEGER attrib);
void		ScrDrawText		(char *str, INTEGER row, INTEGER col, INTEGER attrib);
void		ScrEraseLine(int key);
void		ScrErasePage(int key);
void		ScrScroll(int key, int firstrow, int lastrow);

void		CONInitialize(void);			/* initialize internal				*/
void		CONSuspend(void);				/* suspend internal as in ^Z		*/
void		CONRestart(void);				/* restart as after a ^Z			*/
int		CONgetc(void);					/* getc() (one key)  (no echo)	*/
int		CONgetcRaw(void);				/* getc() (one byte) (no echo)	*/
int		CONungetc(int achr);			/* ungetc()								*/
int		CONwaitchr(int msecs);		/* CONgetc() with timeout			*/
int		CONchkchr(void);				/* non-blocking CONgetc()			*/
char	  *CONgets(char *s, int n);	/* get string      (no echo)		*/
int		CONungetc(int achr);			/* ungetc()								*/
int		CONBreakNotify(VOLATILE_SIG_ATOMIC_T *flag, int action);
void		CONBreakClear(void);			/* Clear internal breakflag		*/
typedef enum  {FULL_COMPLETE, PARTIAL_COMPLETE} TABCOMPLETIONMODE;
#ifdef LOW_IO_C_SOURCE
	EXPORT	TABCOMPLETIONMODE TabCompletionMode;
#else
	IMPORT	TABCOMPLETIONMODE TabCompletionMode;
#endif

#ifdef CON_PUTS_ARE_FUNCTIONS				/* This is never true				*/
	int	CONputc(int c);				/* Echo char       (\r\n unique)	*/
	int	CONputs(char *str);			/* Echo string     (\r\n unique)	*/
	int	CONflush(void);            /* fflush(stdout)						*/
#elif (defined MSC60)
	#define	CONputc(c)		putch(c)
	#define	CONputs(str)	cputs(str)
	#define	CONflush()		fflush(stdout)
#else
	#define	CONputc(c)		fputc(c,   stdout)
	#define	CONputs(str)	fputs(str, stdout)
	#define	CONflush()		fflush(stdout)
#endif

char	  *CONread(char *s, int n);
void		CONFlushCtrlC(void);
char	  *CONSetMacro(int macro, char *str);

int		TTYgetc(void);
char	  *TTYgets(char *s, int n);
int		TTYputc(const int c);
int		TTYputs(const char *str);
int		TTYputsnl(const char *str);
int		TTYprintf(const char *form, ...);
int		TTYflush(void);
void		TTYShowHistory(FILE *handle);
char	  *TTYRetrieveHistory(int count, char *str);

void		ERRputs(const char *str);
void		ERRputsnl(const char *str);
void		ERRprintf(const char *form, ...);

void		RingBell(void);
BOOL		UserInput(const char *prompt, char *str, int len);
void		type2(const char *text1, const char *text2);
void		gen_err(const char *text);
void		gen_err2(const char *text, const char *tok);
void		gen_warn(const char *text);

#if defined MSC60 && (defined _MT || defined _DLL)
	int raise(int sig);
#endif
	
/* Centralize dependence on SYSV vs. BSD resignalling needs */
#if (defined CSET2 || defined AIX_C || defined GNU_C || defined SOLARIS_C)	/* Must reset signal after each use	*/
	#define REsignal(code,handler) signal(code,handler)
#elif defined (MSC70)
	#define REsignal(code,handler) signal(code,handler)
#else
	#define REsignal(code,handler)
#endif

#if (defined CSET2 || defined WATCOM || defined MSC60 || defined MSC70)	/* matherr.c externals */
	IMPORT	volatile sig_atomic_t _Sys_Math_Exception;
	IMPORT	char                  _Sys_Math_Message[];
#endif

#ifdef NT
	#ifdef CON_NT_C_SOURCE
		EXPORT void *SysMainWindowHwnd;
		EXPORT void *SysGraphWindowHwnd;
	#else
		IMPORT void *SysMainWindowHwnd;
		IMPORT void *SysGraphWindowHwnd;
	#endif
#endif

#endif /*  __extends */
