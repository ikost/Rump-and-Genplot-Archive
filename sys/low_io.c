/* Relatively low level I/O handling routines.  Lowest level reading from */
/* console routines defined in con_os2.c or con_unix.c files.             */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* #define READ_INPUT_FROM_PIPE */		/* If defined, use pipe input		*/

#if (defined HP_C)
	#define	SET_INS_RPL(x)	
#elif (defined NT)
	#define	SET_INS_RPL(x)	if (SysGUIMode) CONputs(x ? "\033[8h" : "\033[8l")
#else
	#define	SET_INS_RPL(x)	CONputs(x ? "\033[8h" : "\033[8l")
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
	extern HANDLE hwndStdout;						/* From con_nt.c */
	extern int ConIsPipe;							/* From con_nt.c */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	LOW_IO_C_SOURCE
#include "mytypes.h"
#include "extends.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	SAVEBUFSIZE			4096		/* Size of the save buffer					*/
#define	MINCHARSAVE			2			/* # of char in a command to be saved	*/
#define	NUMSHOWHISTORY		24			/* # of history lines to show (max)		*/

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
PRIVATE char *GetAttribStr(int attrib);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
EXPORT	TABCOMPLETIONMODE TabCompletionMode = FULL_COMPLETE;
EXPORT	SYSATTRIBLIST *SysAttribList = NULL;		/* Replacement attribute list				*/
EXPORT	int				 SysConsoleBehavior = 0;	/* Interaction with console routines	*/
			BOOL				 SysInsertMode = TRUE;		/* Entry insert mode on cmd				*/

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
#define	LOG_ECHO			0x01							/* Log all echoed text */
#define	LOG_COMMANDS	0x02							/* Log all typed commands */

PRIVATE	FILE		*LogHandle=NULL;					/* File unit */
PRIVATE	INTEGER	 LogStatus;
PRIVATE	INTEGER	 LogPause;
PRIVATE	BOOL		 IsRedirected=FALSE;

/* ============================================================================
-- Low level I/O routines.  All I/O should be directed through the routines
-- listed below.  For each machine, it may be necessary to optimize the code to
-- handle the particular hardware.
--
-- Distinguish the standard output unit (referred to as the TTY) from the 
-- console or controlling terminal (referred to as the CON) which may or may
-- not be the TTY.  CONxxx routines go directly to the console while TTYxxx
-- routines output through standard units, possibly to the CON.  These are
-- modelled after the ANSI routines fgetc, fgets, fputc, fputs, fprintf, with
-- additional control for terminal direct such as checking for keystroke.
--
-- LEVEL 0:  Low level output directly to the CON (if connected).  This is
--           not necessarily the standard output device.  External use of these
--           routines should be limited to cases where logging or redirection
--           is inappropriate.  Output by these routines is untrapped.
--           Redirection of stdin poses a big problem for these routines.
--				 Input routines getc, gets and chkchr do not echo.
--					void  CONInitialize(void)			--> initialize internal
--					void  CONSuspend(void)				--> suspend internal as in ^Z
--					void  CONRestart(void)				--> restart as after a ^Z
--					int	CONgetc(void)					--> getc()         (no echo)
--					int	CONungetc(int achr)			--> ungetc()
--					int	CONwaitchr(int msecs)		--> CONgetc() with timeout
--					int   CONchkchr(void)				--> non-blocking CONgetc()
--					char *CONgets(char *s, int n)		--> get string     (no echo)
--					int	CONungetc(int achr)			--> ungetc()
--					int	CONputc(int c)					--> Echo char     (\r\n unique)
--					int	CONputs(char *str)			--> Echo string   (\r\n unique)
--					int	CONflush(void)	            --> fflush(stdout)
--					int	CONBreakNotify(VOLATILE_SIG_ATOMIC_T *flag, int action);
--					void	CONBreakClear(void)			--> clear internal break
--          These lowest level routines are found in con_os2.c or con_unix.c
--          since there are substantial operating system differences.  Note
--          that some of these may be implemented as MACROS as well for
--          purposes of efficiency.  Generally, these are to be avoided by
--          all but the lowest level routines -- use TTY versions below.
--				^C handling in these also.  First ^C sets all signals requested by
--				CONBreakNotify.  If CONBreakClear() not called first, next ^C will
--				abort the process.
--
-- LEVEL 0A: Other low level CON routines which are common to all versions.
--           These are the higher level read, and setting of F<n> function
--           key macros currently.
--
--					char *CONread(char *s, int n)		--> get string w/ edit & echo
--					char *CONSetMacro(int macro, char *str)
--
-- LEVEL 0B: Screen position and clearing.  These must provide the rudimentary
--           commands equivalent to a VT100 type terminal.  As written, will
--           output for a VT100.  Call LEVEL 0A routines.
--					cls				cls_attr
--					dcolor			dcol_s
--					setpos_			getpos_
--					scrl_				scrl_att
--					esclin_			escpag_
--					putstr_		
--					getchr_(ival)	scr_inf
--
-- LEVEL I:  TTY input/output to user interface.  Low level for normal trapped
--           I/O with script capture handled in this code segment.  Concept of
--           TTY is generic, may be redirected through file.  If input/output
--           is from the CON, use low level routines.  All input should be 
--           echoed and, if possible, routed through CONread to allow editing.
--					int	TTYgetc(void)					--> fgetc char   from stdin
--					char *TTYgets(char *s, int n)		--> fgets string from stdin
--					int	TTYputc(int c)					--> fputc char   to stdout
--					int	TTYputs(char *str)			--> fputs string to stdout
--					int	TTYputsnl(char *str)			--> fputs string w/ NL
--					int	TTYprintf(char *form, ...)	--> fprintf      to stdout
--					int	TTYflush(void)	            --> fflush(stdout)
--          These should be the primary interface to user functions.  All
--          internal capture is handled properly by these routines.
--
-- LEVEL II: Secondary I/O commands through LEVEL I/0B and control functions.  
--					void	RingBell(void)					--> ring a bell
--					BOOL	UserInput(CHAR *prompt, CHAR *str, int len);
--					void	type2(CHAR *text1, CHAR *text2);
--					void	ERRputs(char *str)			--> fputs error message
--					void	ERRputsnl(char *str)			--> fputs error message w/ NL
--					void	ERRprintf(char *form, ...)	--> fprintf error message
--					void	gen_err(CHAR *text);
--					void	gen_err2(CHAR *text, CHAR *tok);
--					void	gen_warn(CHAR *text);
--
--					void	ScrDrawText(CHAR *str, INTEGER row, INTEGER col, INTEGER attrib);
--					BOOL	SysLogFile(INTEGER key, CHAR *string, FILE **funit)
============================================================================ */


/* ============================================================================
--           *******************************************************
--           *******************    LEVEL 0A   *********************
--           *******************************************************
============================================================================ */

/* ============================================================================
-- CONgetc   - routine to retrieve next character from console.  Waits if none.
-- CONchkchr - routine to check for a character waiting for input
-- CONgets   - routine to return a string of specified length or to first <nl>
--
-- Syntax: int   CONgetc()
--         int   CONchkchr();
--         char *CONgets(char *string, int n);
--
-- Output: CONgetc   returns next character code.
--         CONchkchr returns next available character or -1 if none waiting
--         CONgets   returns next up to n-1 characters or up to first NL read.
--                   <nl> is stripped from returned string
--
-- Note: (1) No characters should ever be echoed to the screen
--       (2) CONgetc should wait until a character is available (see CONchkchr)
--       (3) Strange machines (PRIME!) must strip parity bit (if stupid)
--       (4) Newline pair CR/LF, if read, should be returned as '\n' single
--       (5) CONgetc will return extended codes if appropriate
--
-- Each machine must implement CONgetc and CONchkchr.  Default CONgets will
-- work properly if these are implemented.  See machine specific files
-- for these routines (con_os2.c, con_unix.c)
============================================================================ */



/* ============================================================================
-- Routines to output character string to terminal.  No translation.
--
-- Syntax: int CONputs(CHAR *string);
--
-- Inputs: string - character string to output			CHAR*(*)
--
-- Output: String is output to current device.
--
-- These routines normally defined as macros in the #include files
--
--  #define CONputs(str) fputs(str, stdout)
--  #define CONputc(c)   fputc(c,   stdout)
============================================================================ */



/* ---------------------------------------------------------------------------
-- Routine to set and query function key macros in the command line editor
--
-- Usage:   char * = CONSetMacro(unsigned int macro, char *str);
--
-- Inputs:  macro - macro number to set or query (1==>F1, 2==>F2, etc)
--          str   - if not NULL, string to copy into macro
--
-- Returns: pointer to the macro string, or NULL if macro invalid or undefined
--------------------------------------------------------------------------- */
#define	NUM_MACROS	12
PRIVATE char *FkeyMacro[NUM_MACROS] = {
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

char *CONSetMacro(int macro, char *str) {
	if (macro < 1 || macro > NUM_MACROS) return(NULL);
	if (str != NULL) {
		free(FkeyMacro[macro-1]);
		FkeyMacro[macro-1] = strdup(str);
	}
	return(FkeyMacro[macro-1]);
}


/* ===========================================================================
-- Routine to create a list of all files satisfying a pattern.
--
-- Usage: TABFILELIST *TabFileCreate(char *pattern, int *num)
--
-- Inputs: dir     - directory to search.  If NULL, search CWD
--         pattern - pattern to match.  If no wild cards, * is appended.
--
-- Output: *num - if not NULL, number of matching files
--
-- Return: Pointer to a structure which can be sequenced to return list.
--         If NULL, indicates an error searching directory
--
-- Notes: (1) A blank pattern is converted to * effectively.
--        (2) If there is no wildcard in the pattern, * is appended
--        (3) If the length is greater than PATH_MAX, error (NULL) returned
--        (4) No matching files will also return NULL
=========================================================================== */
typedef struct _TABFILELIST {
	char dir[PATH_MAX];								/* Directory (needed to make)			*/
	BOOL ForceQuote;									/* Did file originally start with "	*/
	char *list;											/* List of names							*/
	int  *posn;											/* index into list for each entry	*/
	int  nfiles, nlast;								/* Number in last and last returned */
} TABFILELIST;


TABFILELIST *TabFileCreate(char *pat, int *num) {

	TABFILELIST *tabfilelist;
	DIR	*dirp;										/* Directory unit */
	struct dirent *entry;							/* And an entry	*/

	BOOL ForceQuote = FALSE;

	char dir[PATH_MAX], wild[PATH_MAX], pattern[PATH_MAX], *fname, *dirnull, *aptr;
	int ilen;

	char *list;											/* List of valid files */
	int list_alloc, list_ptr;
	int *posn;											/* Where they are located */
	int posn_alloc, posn_ptr;

/* Create default return values */
	if (num != NULL) *num = 0;						/* Default values */

/* Initial error testing */
	if ( (ilen = (int) strlen(pat)) >= PATH_MAX) return(NULL);

/* Copy the actual search pattern.  Strip whitespace and  " from lead/trail */
	aptr = pat;
	while (isspace(*aptr)) aptr++;
	if (*aptr == '"') { ForceQuote = TRUE; aptr++; }

	strcpy(pattern, aptr);							/* Make a local copy */
	for (aptr = pattern+strlen(pattern)-1; aptr>=pattern; aptr--) {
		if (! isspace(*aptr)) break;
		*aptr = '\0';
	}
	if (aptr >= pattern && *aptr == '"') *aptr = '\0';

/* Split for wildcards - append * if required */
	SysMarkPath(pattern, &fname, &dirnull);	/* Split off the directory	*/
	strcpy(wild, fname);								/* Copy  the wildcard pattern */
	*dirnull = '\0';
	strcpy(dir, pattern);							/* Copy the directory */
	if (strpbrk(wild, "*?") == NULL) strcat(wild, "*");
	
/* Create a list of all files matching the given pattern */
	list = NULL;										/* List of names				*/
	list_alloc = 0;									/* Bytes allocated			*/
	list_ptr   = 0;									/* Current position (used) */
	posn = NULL;										/* Positions in list			*/
	posn_alloc = 0;									/* Number of files alloc	*/
	posn_ptr   = 0;									/* Number of files in list	*/
	if ( (dirp = opendir(*dir ? dir : ".")) != NULL) {
		while ( (entry = readdir(dirp)) != NULL) {
			if (! SysCheckMatch(entry->d_name, wild)) continue;
			ilen = (int) strlen(entry->d_name);
			if (list_ptr+ilen+1 > list_alloc) {
				list_alloc += 4096;					/* Page at a time */
				list = realloc(list, list_alloc*sizeof(*list));
			}
			if (posn_ptr >= posn_alloc) {
				posn_alloc += 128;					/* 128 entries at a time */
				posn = realloc(posn, posn_alloc*sizeof(*posn));
			}
			strcpy(list+list_ptr, entry->d_name);
			posn[posn_ptr++] = list_ptr;
			list_ptr += ilen+1;
		}
		closedir(dirp);
	}
	if (posn_ptr == 0) return(NULL);				/* No matching */

/* Create and transfer this info to the TABFILELIST structure */
	tabfilelist = calloc(1, sizeof(TABFILELIST));
	strcpy(tabfilelist->dir, dir);
	tabfilelist->ForceQuote = ForceQuote;
	tabfilelist->list   = list;
	tabfilelist->posn   = posn;
	tabfilelist->nfiles = posn_ptr;
	tabfilelist->nlast  = -1;					/* Will recycle to 0	*/

	if (num != NULL) *num = posn_ptr;
	return(tabfilelist);
}


/* ===========================================================================
-- Return an entry from the list
--
-- Usage: char *TabFileGetNext(TABFILELIST *list, int idx);
--
-- Inputs: TABFILELIST - structure returned from a TabFileCreate call
--         idx - +1 => next item in list
--               -1 => previous item in list
--                0 => first item in list
--
-- Output: none
--
-- Return: Pointer to a static structure containing the fully formed path
--         of the next entry in the tab matching list.  If necessary, quote
--         marks will be placed on either side of the name.  If there are
--         no entries, or otherwise an error, NULL is returned.
=========================================================================== */
char *TabFileGetNext(TABFILELIST *tabfilelist, int idx) {

	static char path[PATH_MAX];
	BOOL is_dir;
	struct stat statbuf;
	int ilen;
	
	path[0] = '\0';

	if (tabfilelist == NULL) return(NULL);

	if (tabfilelist->nfiles == 0) return(NULL);	/* No chars */
	if (idx != 0) idx = tabfilelist->nlast + idx;
	while (idx < 0) idx += tabfilelist->nfiles;

	idx = idx % tabfilelist->nfiles;					/* Keep within range */
	tabfilelist->nlast = idx;							/* Start from here next time */

	SysMakePath(path, tabfilelist->dir, tabfilelist->list+tabfilelist->posn[idx], NULL);
	is_dir = stat(path, &statbuf)==0 && S_ISDIR(statbuf.st_mode);
#ifdef NT
	if (is_dir) strcat(path, "\\");
#else
	if (is_dir) strcat(path, "/");
#endif

/* If any blanks in the name, then append quotes */
	if (tabfilelist->ForceQuote || (strchr(path, ' ') != NULL)) {
		ilen = (int) strlen(path);
		memmove(path+1, path, ilen);
		path[0] = path[ilen+1] = '"';
		path[ilen+2] = '\0';
		if (is_dir) path[ilen+1] = '\0';			/* No trailing " */
	}

	return(path);
}

/* ===========================================================================
-- Routine to release a tab list structure
--
-- Usage: void TabFileFree(TABFILELIST *list);
--
-- Inputs: list - pointer returned from a TabFileCreate call
--
-- Output: none
--
-- Return: none
=========================================================================== */
void TabFileFree(TABFILELIST *tabfilelist) {
	if (tabfilelist != NULL) {
		if (tabfilelist->list != NULL) free(tabfilelist->list);
		if (tabfilelist->posn != NULL) free(tabfilelist->posn);
		free(tabfilelist);
	}
	return;
}


/* ============================================================================
-- CONread - Input line of text from CON.  Fully echo and permit editing of
--           command line as appropriate.  Input terminated on <CR>, <LF>,
--           <ESC> or <^Z>.
--
-- Syntax: char *CONread(char *str, int len);
--
-- Inputs: str - pointer to string for storing input
--         len - sizeof(str)
--
-- Output:  Return NULL on EOF, otherwise returns str
--
-- Note: 1) An EOF condition is specified by return of NULL.  EOF should be
--          returned for either ^Z or ^D.  If ^Z or ^D occur, return
--          immediately with empty string and NULL.
--       2) <ESC> should be detected if possible with an immediate return.
--          String should contain only <ESC> w/ normal return.
--       3) This routine should provide full buffering and editing of the 
--          command line.  Routines which do not want echo/edit should call
--          CONgets instead.
--       4) In accordance with gets, the terminating NL is not returned but is
--          rather translated to a '\0'.
--       5) Isolated \ at end of line is taken as continuation character.
--          String terminated at the \ and input continues
--
-- Under OS/2 PM or OS/2 Window ^C is not recognized until a CR is received. 
-- Under OS/2 PM, ^L is trapped at PM level as a screen redraw.
============================================================================ */
#define CTRL_NULL					0x00
#define CTRL_A						0x01
#define CTRL_B						0x02
#define CTRL_C						0x03
#define CTRL_D						0x04
#define CTRL_E						0x05
#define CTRL_F						0x06
#define CTRL_G						0x07
#define CTRL_H						0x08
#define CTRL_I						0x09
#define CTRL_J						0x0A
#define CTRL_K						0x0B
#define CTRL_L						0x0C
#define CTRL_M						0x0D
#define CTRL_N						0x0E
#define CTRL_O						0x0F
#define CTRL_P						0x10
#define CTRL_Q						0x11
#define CTRL_R						0x12
#define CTRL_S						0x13
#define CTRL_T						0x14
#define CTRL_U						0x15
#define CTRL_V						0x16
#define CTRL_W						0x17
#define CTRL_X						0x18
#define CTRL_Y						0x19
#define CTRL_Z						0x1A
#define ESC							0x1B
#define CTRL_RIGHT_BRACKET		0x1D
#define FAKE_CTRL_C				0x1F
#define DEL							0x7F
#define DO_EOF_CHAR				0xFE

#if (defined OS2 || defined NT)	/* OS/2 system expects ^Z as EOF	*/
	#define	EOF_CHAR	CTRL_Z
#else										/* UNIX inevitably is a ^D EOF	*/
	#define	EOF_CHAR	CTRL_D
#endif

/* History command structures */
PRIVATE	char *SaveBuf=NULL;				/* Command line save buffer		*/
PRIVATE	char *svptr;						/* Current pointer into SaveBuf	*/
PRIVATE	int  HistoryCount;				/* Number of lines of history		*/

PRIVATE  char kill_buffer[LONG_STR_SIZE] = "";	/* ^K/^Y kill buffer (ala EMACS)	*/

char *CONread(char *token, int len) {

	char *str, *ptr, *endptr, *lastptr, *point, *fname;
	char *tmp, atmp;
	char extra[LONG_STR_SIZE];						/* Temporary hold buffer */
	unsigned int achr;
	BOOL InsertMode;
	enum {M_NORMAL, M_CTRLX, M_QUOTE} mode=M_NORMAL;
	int i, dir, recall=0;

	TABFILELIST *TabFileList=NULL;				/* For tab filename completion */
	char *tabfileposn = NULL;						/* Position where filename starts */

	char *svtmp;
	VOLATILE_SIG_ATOMIC_T SysBreakSeen;			/* Break flag (give to con_x.c) */
	
	if (SaveBuf == NULL) {							/* Initialize buffers?	*/
		if ( (SaveBuf = malloc(SAVEBUFSIZE)) == NULL) panic;
		memset(SaveBuf, '\0', SAVEBUFSIZE);
		HistoryCount = 0;
		svptr = SaveBuf;
	}

/* -- token = actual string.  str = current segment (possibly continuation) */
	str     = token;					/* Use str for where we are stroring from */
	point   = str;						/* Current "marked" point in text			*/
	ptr     = str;						/* Current position in text string			*/
	endptr  = str;						/* Current end of text string (next free)	*/
	lastptr = str + (len-1);		/* Last possible point for character		*/
	svtmp   = svptr;

	InsertMode = SysInsertMode;	/* Start with system wide default			*/
	SET_INS_RPL(InsertMode);

	SysBreakSeen = 0;											/* Clear my break flag	*/
	if (CONBreakNotify(&SysBreakSeen, TRUE) != 0) 	/* Add to break list		*/
		ERRprintf("TELL DEVELOPERS THEY HAVE A SIGINT SET PROBLEM\n");

	while (SysBreakSeen == 0) {
#if (defined MSC60 && defined _DLL)				/* Give up time-slice, fake CTRL_C */ 
		while (kbhit() == 0) {if (SysBreakSeen) ungetch(FAKE_CTRL_C); MilliSleep(30);}
#endif
		achr = CONgetc();								/* Get a character */
		if (achr != CTRL_I && achr != VIRTUAL_SHIFT_TAB && TabFileList != NULL) {
			TabFileFree(TabFileList);
			TabFileList = NULL;
		}
		if (SysBreakSeen || achr == CTRL_C || achr == FAKE_CTRL_C) {
			if (achr == CTRL_C || achr == FAKE_CTRL_C) SysBreakCount--;
			break;
		}
		if (achr == 0x103) achr = CTRL_NULL;	/* Make it an NULL character */

recycle:
		if (mode == M_CTRLX) {
			switch (achr) {
				case 'Q':								/* Quote the next char */
				case 'q':
					mode = M_QUOTE;
					break;
				case ' ':								/* Setmark equivalents */
				case '2':
				case '@':
					achr = CTRL_NULL;
					mode = M_NORMAL;
					goto recycle;
				case CTRL_Z:							/* push shell */
					SysPushShell(); 
					mode = M_NORMAL;
					break;
				case CTRL_X:							/* exchange point */
					tmp = ptr; ptr = point;	point = tmp;
					while (tmp   < ptr) CONputc(*(tmp++));
					while (tmp-- > ptr) CONputc('\b');
					mode = M_NORMAL;
					break;
				default:
					mode = M_NORMAL;
					RingBell();
			}

		} else if (mode != M_QUOTE && (iscntrl(achr) || (achr & 0x100))) {

			mode = M_NORMAL;
			if (achr==EOF_CHAR && endptr==str) 		/* EOF if only thing on line */
			   achr = DO_EOF_CHAR;						/* Physically change char */

			switch (achr) {
				case VIRTUAL_F1:
				case VIRTUAL_F2:
				case VIRTUAL_F3:
				case VIRTUAL_F4:
				case VIRTUAL_F5:
				case VIRTUAL_F6:
				case VIRTUAL_F7:
				case VIRTUAL_F8:
				case VIRTUAL_F9:
				case VIRTUAL_F10:
				case VIRTUAL_F11:
				case VIRTUAL_F12:
					if ( (tmp=FkeyMacro[achr-VIRTUAL_F1]) == NULL) break;
					while (ptr>str) {CONputc('\b'); ptr--;}	/* To beginning of line */
					while (*tmp) CONputc(*(ptr++)=*(tmp++));
					tmp = ptr;											/* Erase end of line */
					while (tmp<endptr) {CONputc(' '); tmp++;}	/* Putting blanks		*/
					while (tmp!=ptr)   {CONputc('\b');tmp--;}	/* And back up			*/
					endptr = ptr;
					break;

				case CTRL_RIGHT_BRACKET:
					SysInsertMode = ! SysInsertMode;
					InsertMode = SysInsertMode;
					SET_INS_RPL(InsertMode);
					break;
				case VIRTUAL_INSERT:
					InsertMode = ! InsertMode;
					SET_INS_RPL(InsertMode);
					break;

				case CTRL_NULL:								/* set mark  */
					point = ptr; break;

				case CTRL_A:									/* ^A ==> go to BOL */
				case VIRTUAL_HOME:
					while (ptr > str) {CONputc('\b'); ptr--;} 
					break;
				case VIRTUAL_LEFT:
				case CTRL_B:									/* ^B ==> Back one char */
					if (ptr > str) {CONputc('\b'); ptr--;}
					break;
				case CTRL_E:									/* ^E ==> End of line */
				case VIRTUAL_END:
					while (ptr < endptr) CONputc(*(ptr++));
					break;
				case VIRTUAL_RIGHT:
				case CTRL_F:									/* ^F ==> Forward one char */
					if (ptr < endptr) CONputc(*(ptr++));
					break;

				case VIRTUAL_CTRL_LEFT:						/* <ctrl><left> ==> Back one word */
					if (ptr == str) break;					/* Nothing to do			  */
					CONputc('\b'); ptr--;					/* Always back at least 1 */
					while (ptr > str && (isspace(*ptr)||ispunct(*ptr)) ) {
						CONputc('\b'); ptr--;
					}
					while (ptr > str && ! (isspace(*(ptr-1))||ispunct(*(ptr-1))) ) {
						CONputc('\b'); ptr--;
					}
					break;
				case VIRTUAL_CTRL_RIGHT:					/* <ctrl><left> ==> Forward one word */
					if (ptr == endptr) break;				/* Nothing to do			*/
					CONputc(*ptr); ptr++;					/* Always go at least 1	*/
					while (ptr < endptr && (isspace(*ptr)||ispunct(*ptr)) ) {
						CONputc(*ptr); ptr++;
					}
					while (ptr < endptr && ! (isspace(*ptr)||ispunct(*ptr)) ) {
						CONputc(*ptr); ptr++;
					}
					break;

				case CTRL_G:									/* ^G ==> RingBell */
					RingBell();
					break;
				case CTRL_J:									/* ^L ==> line feed		  */
				case CTRL_M:									/* ^M ==> carriage return */
					CONputs("\r\n");							/* Terminate screen line  */
					*endptr = '\0';							/* Mark end					  */
#ifdef ALLOW_LOGICAL_CONTINUATION_AT_THIS_LEVEL
					if (endptr>str && *(endptr-1)=='\\') {	/* Continuation?		*/
						*(--endptr) = '\0';					/* New end of line		*/
						point = ptr = str = endptr;		/* Continuation goes on	*/
						CONputs("->");							/* Mark continuation		*/
						break;
					}
#endif
					/* Handle the History retrieval here now, instead of in system.c */
					if ( (endptr-str > 1) && (str[0] == '!') && (! isspace(str[1])) ) {
						i = strtol(str+1, &tmp, 10);
						if (*tmp != '\0') i = 0;
						if ( (tmp = TTYRetrieveHistory(i, str+1)) != NULL) strcpy(str, tmp);
						TTYprintf("<auto> %s\n", str);

					/* And possible save new text in history buffer */
					} else if (endptr-token >= MINCHARSAVE) {	/* Conditions for save */
						tmp = token;							/* See if str is same */
						if (recall == 1) {					/* last cmd, check for mods */
							while ((tmp <= endptr) && (*tmp == *svtmp)) {
								tmp++;
								svtmp++; if (svtmp-SaveBuf >= SAVEBUFSIZE) svtmp = SaveBuf;
							}
						}
						if (tmp <= endptr) {					/* Not duplicate of last */
							tmp = token;
							while (tmp <= endptr) {
								*(svptr++) = *(tmp++);
								if (svptr-SaveBuf >= SAVEBUFSIZE) svptr = SaveBuf;
							}
							*svptr = '\0';						/* Mark as double end	*/
							HistoryCount++;					/* Number of commands	*/
						}
					}

					str = token;								/* str is always returned */
					goto AllExit;

				case CTRL_L:									/* ^L ==> Rewrite line */
					while (ptr > str) {CONputc('\b'); ptr--;}
					*endptr = '\0'; CONputs(str); ptr = endptr;
					break;

				case CTRL_R:									/* ^R ==> rewrite line */
					CONputc('\n');								/* Return to redraw */
					*endptr = '\0'; CONputs(str);
					for (tmp=endptr; tmp!=ptr; tmp--) CONputc('\b');
					break;

				case CTRL_T:									/* ^T ==> twiddle chars */
					if (ptr-str > 1) {
						char atmp,btmp;
						atmp = *(--ptr); btmp = *(--ptr);
						*(ptr++) = atmp; *(ptr++) = btmp;
						CONputc('\b');CONputc('\b');CONputc(atmp); CONputc(btmp);
					}
					break;
				case CTRL_X:									/* Switch modes */
					mode = M_CTRLX;
					break;
				case CTRL_Q:									/* Quote the next char */
					mode = M_QUOTE;
					break;
				case DO_EOF_CHAR:								/* EOF character */
					*str = '\0';
					CONputs("^Z\r\n");
					str = NULL;									/* str is always returned */
					goto AllExit;
				case ESC:										/* <ESC> */
					CONputs("<ESC>\r\n");
					*str++ = ESC; *str = '\0';
					goto AllExit;

				case CTRL_H:									/* Backspace */
					if (ptr == str) break;
					CONputc('\b'); ptr--;					/* Backspace then delete */
				case CTRL_D:									/* delete a char */
				case DEL:										/* delete a char */
				case VIRTUAL_DELETE:							/* Delete key	  */
					if (ptr != endptr) {
						char *a1, *a2;
						a1 = a2 = ptr; 
						while (++a2 < endptr) *(a1++) = (CHAR) CONputc(*a2);
						CONputc(' ');							/* Erase last spot */
						while (a2-- > ptr) CONputc('\b');
						endptr--;
					}
					break;

				case CTRL_W:									/* ^W - cut region */
					if (ptr != point) {
						char *src, *dest, *tmp2, *mark;	/* Current point */

						*endptr = '\0';						/* Mark the string end	*/
						mark = ptr;								/* Other end of block	*/

						/* Ensure point < mark */
						if (point > mark) {
							tmp = point; point = mark; mark = tmp;
						}
						/* Copy region to kill buffer */
						for (src=point,dest=kill_buffer; src<mark;) *(dest++)=*(src++);
						*dest = '\0';
																
						/* Walk back to point */
						while (ptr > point) {CONputc('\b'); ptr--;}

						/* Move the rest of the line into this location */
						tmp2 = ptr;								/* Will become new endptr */
						while (*mark) CONputc(*(tmp2++) = *(mark++));

						/* Clear out the rest of the screen buffer */
						for (tmp=tmp2; tmp<endptr; tmp++) CONputc(' ');
						while (tmp>ptr) {CONputc('\b'); tmp--;}

						/* And reset all the pointers */
						endptr = tmp2;
						point = ptr;							/* And this now set */
					}
					break;
						
				case CTRL_U:								/* Erase entire line */
					while (ptr > str) {CONputc('\b'); ptr--;}
				case CTRL_K:								/* Erase to end of line */
					if (ptr != endptr) {
						char *a1=ptr;
						*endptr = '\0';					/* Terminate so can copy */
						strscpy(kill_buffer, ptr, sizeof(kill_buffer));
						while (a1++ < endptr) CONputc(' '); 
						while (--a1 > ptr) CONputc('\b');
						endptr = ptr;
					}
					break;

				case CTRL_Y:									/* ^Y */
					if (*(tmp=kill_buffer) != '\0') {
						char *tmp2;

						/* Save the text beyond pointer */
						*endptr = '\0'; strscpy(extra, ptr, sizeof(extra));

						/* Insert the new text */
						while (ptr != lastptr && *tmp) CONputc(*(ptr++) = *(tmp++));

						/* Restore old stuff and backup to ptr */
						tmp  = extra;
						tmp2 = ptr;
						while (tmp2 != lastptr && *tmp) CONputc(*(tmp2++) = *(tmp++));
						endptr = tmp2;
						while (tmp2 != ptr) {CONputc('\b'); tmp2--;}
					}
					break;

				case VIRTUAL_DOWN:
				case CTRL_N:									/* ^N => next line */
				case VIRTUAL_UP:
				case CTRL_P:									/* ^P => prev line */
					if (achr == VIRTUAL_DOWN || achr == CTRL_N) {
						tmp = svtmp;	dir = +1;
					} else {
						tmp = svtmp-2; if (tmp < SaveBuf) tmp += SAVEBUFSIZE;
						dir = -1;
					}
					if (*tmp == '\0') break;				/* Nothing further on */

					recall -= dir ;							/* recall=1 ==> last	*/
					while (*tmp != '\0') {
						tmp += dir;
						if (tmp-SaveBuf >= SAVEBUFSIZE) tmp = SaveBuf;
						if (tmp < SaveBuf)              tmp += SAVEBUFSIZE;
					}
					tmp++; if (tmp-SaveBuf >= SAVEBUFSIZE) tmp = SaveBuf;
					svtmp = tmp;

					while (ptr != str) {CONputc('\b'); ptr--;}
					while (*tmp != '\0') {
						if (ptr == lastptr) break;
						*(ptr++) = (CHAR) CONputc(*(tmp++));
						if (tmp-SaveBuf >= SAVEBUFSIZE) tmp = SaveBuf;
					}
					tmp = ptr;
					while (tmp++ < endptr) CONputc(' ');
					while (--tmp >  ptr)   CONputc('\b');
					endptr = ptr;
					break;
					
/* Filename completion - always in insert mode */
#ifdef NT
	#define	BING()	MessageBeep(MB_OK)
#else
	#define	BING()	RingBell()
#endif
				case CTRL_I:									/* Tab character */
				case VIRTUAL_SHIFT_TAB:
					/* Is this command line completion? */
					/* Make sure this always agrees with code in lexp/system.c */
					if ( (achr == CTRL_I) && (ptr-str > 1) && (str[0] == '!') && (! isspace(str[1])) ) {
						i = strtol(str+1, &tmp, 10);
						if (*tmp != '\0') i = 0;
						if ( (tmp = TTYRetrieveHistory(i, str+1)) != NULL) {
							/* Erase all existing text and rewrite */
							while (ptr > str) { CONputc('\b'); ptr--;}
							while (*tmp) CONputc(*(ptr++) = *(tmp++));
							tmp = ptr;									/* New endptr */
							while (tmp < endptr) { CONputc(' ');  tmp++; }
							while (tmp > ptr)    { CONputc('\b'); tmp--; }
							endptr = ptr;
						} else {
							BING();
						}
						break;
					}

					/* Is this the first attempt at a tab filename completion? */
					if (TabCompletionMode == FULL_COMPLETE) {
						if ( TabFileList == NULL &&			/* Try making new one?		*/
							  ptr != str                 &&	/* Can't be at line begin	*/
							  (ptr==endptr||isspace(*ptr)) &&	/* Must be <sp> or eol		*/
							  ! isspace(*(ptr-1))) {			/* And must be non-blank	*/
						
							/* Identify the starting point of the filename */
							*endptr = '\0';
							fname = ptr-1;
							while (fname != str && ! isspace(*(fname-1))) fname--;
							/* Look further to see if a <sp>"<chr> structure */
							for (tmp=fname-2; tmp>=str; tmp--) {
								if (*tmp == '\"' && isalnum(tmp[1]) && (tmp==str || isspace(*(tmp-1))) ) {
									fname = tmp;
									break;
								}
							}
							/* Null terminate at ptr, and try to create the list */
							tabfileposn = fname;
							atmp = *ptr; *ptr = '\0';
							TabFileList = TabFileCreate(tabfileposn, NULL);
							*ptr = atmp;
						}

						/* For addition, effectively cut/paste */
						if ( (tmp = TabFileGetNext(TabFileList, (achr==CTRL_I)?+1:-1)) != NULL) {
							char *ptr_1, *ptr_2, *ptr_old;

							/* Save the text beyond pointer */
							*endptr = '\0'; strscpy(extra, ptr, sizeof(extra));

							/* Don't redraw text that is identical - so check quickly */
							ptr_1 = tabfileposn;							/* Starting position */
							while (*ptr_1 == *tmp && *tmp != '\0') { ptr_1++; tmp++; }

							/* Walk backwards and overwrite the new text */
							ptr_old = ptr;
							while (ptr > ptr_1) { CONputc('\b'); ptr--;}
							while (*tmp != '\0') CONputc(*(ptr++) = *(tmp++));

							/* If not at the same pointer position, have to write extra */
							if (ptr != ptr_old) {						/* Do I have to change other stuff? */
								ptr_1 = ptr;								/* Where I'll want to be */
								tmp = extra;
								while (ptr != lastptr && *tmp) CONputc(*(ptr++) = *(tmp++));
								ptr_2 = ptr;								/* New endptr */
								while (ptr < endptr) { CONputc(' ');  ptr++;}
								while (ptr != ptr_2) { CONputc('\b'); ptr--;}
								endptr = ptr_2;
								while (ptr != ptr_1) { CONputc('\b'); ptr--;}
							}
							if (ptr == endptr && strchr(":/\\", *(ptr-1)) == NULL) {
								CONputc(*(ptr++) = ' ');
								endptr = ptr;
							}	
						} else {
							BING();
						}
					} else if (TabCompletionMode == PARTIAL_COMPLETE) {
						if ( ptr != str &&						/* Can't be at line begin	*/
							  (ptr==endptr||isspace(*ptr)) &&	/* Must be <sp> or eol		*/
							  ! isspace(*(ptr-1))) {				/* And must be non-blank	*/
							char tmpname[PATH_MAX], *fname, *tmp2, *extra=NULL;
							int rcode;
						
							fname = ptr-1;
							while (fname != str && ! isspace(*(fname-1))) fname--;
							/* Look further back to see if there is a <sp>"<chr> structure */
							for (tmp=fname-2; tmp>str; tmp--) {
								if (*tmp != '\"') continue;	/* Will terminate this now! */
								if (isalnum(tmp[1]) && isspace(*(tmp-1)))
									fname = tmp;
								break;
							}
							tmp = fname;	tmp2=tmpname;
							if (*tmp == '\'' || *tmp == '\"') tmp++;
							while (tmp != ptr) *tmp2++ = *tmp++;
							*tmp2 = '\0';
							rcode = SysCompleteFilename(tmpname, tmpname, sizeof(tmpname));
							if (rcode > 0) {						/* Partially successful */
								if (strchr("/\\", tmpname[strlen(tmpname)-1]) != NULL)
									rcode++;							/* If dir, treat as multiple match */
								if (ptr != endptr) {				/* Save text after point */
									*endptr = '\0';
									extra = strdup(ptr);
								}
								if (strpbrk(tmpname, " ,;'\"^%") != NULL) {
									memmove(tmpname+1, tmpname, sizeof(tmpname)-1);
									*tmpname = '\"';
									if (rcode == 1) strcat(tmpname, "\"");
								}
								tmp = tmpname;
								while (*fname == *tmp && fname != ptr) {fname++; tmp++;}
								tmp2 = ptr;
								while (tmp2 != fname) {CONputc('\b'); tmp2--;}
								while (tmp2 != ptr && *tmp) CONputc(*tmp2++ = *tmp++);
								while (ptr != lastptr && *tmp) CONputc(*(ptr++) = *(tmp++));
								if (extra == NULL) {
									if (rcode == 1 && ptr != lastptr) CONputc(*(ptr++)=' ');
									endptr = ptr;
								} else {
									tmp = extra;
									if (rcode == 1) CONputc(*(ptr++) = *(tmp++));
									tmp2 = ptr;
									while (tmp2 != lastptr && *tmp) CONputc(*(tmp2++) = *(tmp++));
									endptr = tmp2;
									while (tmp2 != ptr) {CONputc('\b'); tmp2--;}
									free(extra);
								}
							} else {
								BING();
							}
						} else {
							BING();
						}
					}
					break;

/* All NOP's (almost) */
				case CTRL_O:									/* ^O */
				case CTRL_S:									/* ^S */
				case CTRL_V:									/* ^V */
					point = ptr;
					break;
				default:
					RingBell();
			}

		} else {												/* Normal character */
			mode = M_NORMAL;								/* Eliminate quotes now */
			if (ptr == lastptr) {						/* Are we out of space? */
				RingBell();
			} else {
				CONputc(achr);								/* Put the char out */
				if (! InsertMode || ptr == endptr) {
					*(ptr++) = (CHAR) achr;
				} else {
					int a1;
					tmp = ptr++;
					if (endptr == lastptr) endptr--;
					while (tmp < endptr) {
						a1 = CONputc(*tmp); 
						*(tmp++) = (CHAR) achr;
						achr = a1;
					}
					*(endptr++) = (CHAR) achr;
					tmp++;
					while (tmp > ptr) {CONputc('\b'); tmp--;}
				}
			}
		}
		if (ptr   > endptr) endptr = ptr;
		if (point > endptr) point  = endptr;
	}

/* ... Here if user has pressed <CTRL><C> since starting line read */
	CONputs("^C\r\n");							/* Break seen! */
	*endptr = '\0';
	*token = '\0';									/* Truncate everything read to this point */
	goto AllExit;

AllExit:
	if (TabFileList != NULL) TabFileFree(TabFileList);
	if (CONBreakNotify(&SysBreakSeen, FALSE) != 0)		/* Remove signal now */
		ERRprintf("TELL DEVELOPERS THEY HAVE A SIGINT REMOVE PROBLEM\n");
	return token;
}


/* ===========================================================================
-- Routine to flush the input "keyboard" buffer until get to the ^C pressed 
--
-- Usage: void CONFlushCtrlC(void);
--
-- Inputs: none
--
-- Output: reads from console input until all known ^C's are flushed
--
-- Return: none
=========================================================================== */
void CONFlushCtrlC(void) {
	unsigned int achr;

	while (SysBreakCount > 0) {
		achr = CONgetc();								/* Get a character */
		if (achr == CTRL_C || achr == FAKE_CTRL_C) SysBreakCount--;
	}
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to list out the history stored in the command stack
--
-- Usage:   void TTYShowHistory(FILE *handle);
--
-- Inputs:  handle - if not NULL, history will be written to the open file
--							if NULL, output is via TTYprintf()
--
-- Returns: void
--
-- Command storage stack looks like:
--    l s \0 plot \0 this \0 \0 \0 \0 \0 ...
--                           **
--------------------------------------------------------------------------- */
void TTYShowHistory(FILE *handle) {

	int  i, icnt;
	char achr, *aptr, *tmp;

	aptr = svptr;											/* Start of non-command	*/

	for (icnt=HistoryCount,i=0; i<NUMSHOWHISTORY; i++,icnt--) {
		tmp = aptr-2;										/* End of last cmd		*/
		if (tmp < SaveBuf) tmp += SAVEBUFSIZE;		/* Keep within buffer	*/
		if (*tmp == '\0') break;						/* No more commands		*/
		while (*tmp != '\0') {							/* Walk to end of last	*/
			tmp--;
			if (tmp < SaveBuf) tmp += SAVEBUFSIZE;
		}
		aptr = tmp+1;										/* Has start of command	*/
		if (aptr-SaveBuf >= SAVEBUFSIZE) aptr = SaveBuf;
	}
	if (i == 0) return;									/* There was NOTHING!!!	*/

	while (*aptr) {										/* Until out of commands */
		icnt++;
		(handle != NULL) ? fprintf(handle, "[%2.2i] %s\n", icnt, aptr) : TTYprintf("[%2.2i] %s\n", icnt, aptr) ;
		do {
			achr = *aptr++;
			if (aptr-SaveBuf >= SAVEBUFSIZE) aptr = SaveBuf;
		} while (achr);
	}

	return;
}

/* ---------------------------------------------------------------------------
-- Routine to retrieve string from history stored in the command stack
--
-- Usage:   char *TTYRetrieveHistory(int count, char *str);
--
-- Inputs:  count - if non-negative, count of the history line to be returned.
--                  count is 0 for first typed line and increments on each.
--          str   - if count is negative, beginning string of command line.
--                  Must match exactly (case insensitive though)
--
-- Returns: !NULL - pointer to static (and volatile) string in history buffer.  
--                  Must be copied or used quickly before it becomes stale!
--           NULL - if command # no longer exists or failed to find string.
--------------------------------------------------------------------------- */
char *TTYRetrieveHistory(int count, char *str) {

	int  i;
	char *aptr, *tmp;
	BOOL PartialMatch;

/* ------------------------------------------------
-- Quick validations of the matching string
-- !! ==> !-1
-- !? ==> partial match
-- If (str == NULL) better have finite count
------------------------------------------------ */
	if (count == 0) {										/* Uses string spec		*/
		if (str == NULL) return(NULL);				/* No way we can match	*/
		while (isspace(*str)) str++;					/* Skip whitespace		*/
		if (*str == '!') count = -1;					/* Previous command		*/
		if ( (PartialMatch=(*str=='?')) ) str++;	/* Partial match?			*/
	} 
	if (count < 0) count += HistoryCount+1;		/* -1 becomes last one	*/

	aptr = svptr;											/* Start of non-command	*/
	for (i=HistoryCount; i>0; i--) {
		tmp = aptr-2;										/* End of last cmd		*/
		if (tmp < SaveBuf) tmp += SAVEBUFSIZE;		/* Keep within buffer	*/
		if (*tmp == '\0') break;						/* No more commands		*/
		while (*tmp != '\0') {							/* Walk to end of last	*/
			tmp--;
			if (tmp < SaveBuf) tmp += SAVEBUFSIZE;
		}
		aptr = tmp+1;										/* Has start of command	*/
		if (aptr-SaveBuf >= SAVEBUFSIZE) aptr = SaveBuf;

		if (count > 0) {									/* Are we at the place?	*/
			if (i == count) return(aptr);
		} else if (PartialMatch) {						/* Test string if not	*/
				if (strstr(aptr, str) != NULL) return(aptr);
		} else {
			tmp = aptr;										/* Ignore whitespace		*/
			while (isspace(*tmp)) tmp++;				/* in comparisons			*/
			if (strnicmp(tmp, str, strlen(str)) == 0) return(aptr);
		}
	}

	return(NULL);
}


/* ============================================================================
--           *******************************************************
--           *******************    LEVEL I   **********************
--           *******************************************************
============================================================================ */

/* ============================================================================
-- Routine to retrieve next character directly from terminal.  Waits for input.
--
-- Syntax: int TTYputc(const int c);
--         int TTYputs(const char *str);
--			  int TTYputsnl(const char *str);
--         int TTYprintf(const char *format, ...);
--         char *TTYgets(char *str, const int n);
--
-- Inputs: c      - character to be output to standard output
--         str    - string to be output (NULL NOT translated to NL)
--         format - format equivalent to fprintf for output
--
-- Output: TTYputc   - c
--         TTYputs   - EOF on error, non-negative otherwise 
--         TTYputsnl - EOF on error, non-negative otherwise 
--         TTYprintf - Number of characters written or negative if an error
============================================================================ */
int TTYputc(const int c) {
	fputc(c, stdout);
	if (LogStatus & 0x01  &&  LogHandle != NULL) fputc(c, LogHandle);
	return(c);
}

int TTYputs(const char *str) {
	int rcode;
	rcode = fputs(str, stdout);
	if (LogStatus & 0x01  &&  LogHandle != NULL) fputs(str, LogHandle);
	return(rcode);
}

int TTYputsnl(const char *str) {
	int rcode;
	rcode = fputs(str, stdout);
	fputc('\n', stdout);
	if (LogStatus & 0x01  &&  LogHandle != NULL) {
		fputs(str, LogHandle);
		fputc('\n', LogHandle);
	}
	return(rcode);
}

int TTYprintf(const char *format, ...) {
	int rcode;
	va_list var1;

	va_start(var1, format);
	rcode = vfprintf(stdout, format, var1);
	va_end(var1);
	
	if (LogStatus & 0x01  &&  LogHandle != NULL) {
		va_start(var1, format);
		vfprintf(LogHandle, format, var1);
		va_end(var1);
	}
	return(rcode);
}

int TTYflush(void) {
	if (LogHandle != NULL) fflush(LogHandle);
	return(fflush(stdout));
}

int TTYgetc(void) {
	int rcode;

	rcode = (! IsRedirected) ? CONgetc() : fgetc(stdin) ;

	if (rcode != EOF && LogStatus & 0x01  &&  LogHandle != NULL)
		fputc(rcode, LogHandle);

	return(rcode);
}


char *TTYgets(char *str, int n) {
	char *rcode;
	int i;

	rcode = (! IsRedirected) ? CONread(str, n) : fgets(str, n, stdin) ;

	if (rcode != NULL) {
		if ( (i = (int) strlen(str)) != 0)						/* Strip nl to null */
			if (str[i-1] == '\n') str[i-1] = '\0';
		if (LogStatus & 0x01  &&  LogHandle != NULL) {
			fputs(str, LogHandle);
			fputc('\n', LogHandle);
		}
	}
	return(rcode);
}


/* ============================================================================
--
--           *******************************************************
--           *******************    LEVEL II  **********************
--           *******************************************************
--
--
============================================================================ */

/* ============================================================================
-- ScrDrawText - Low level routine for outputing characters to console.
--
-- Syntax: CALL ScrDrawText(string,length,irow,icol,iattrib)
--
-- Inputs: string    - character string to output
--         length    - number of characters:  0 => use defined length
--         irow/icol - row/column of output:  0 => use current location
--	  iattrib   - chr attribute to use:  0 => use current attribute
--
-- Output: Puts characters on screen as necessary and outputs to files as
--	  necessary.  CR/LF must be imbedded in the string if it is desired.
--
-- Note:   Cursor is returned left at the final position.
============================================================================ */
void ScrDrawText(CHAR *str, INTEGER irow, INTEGER icol, INTEGER attrib) {

	if (irow != 0) {
		ScrSetPosn(irow, icol, attrib);
	} else if (attrib != 0) {
		ScrSetAttrib(attrib);
	}
	TTYputs(str);
	return;
}

/* ============================================================================
-- Usage BOOL SysLogFile(INTEGER key, CHAR *name, void *funit)
--
-- Inputs: key   - what to do
-- SYSLOG_CHECK - query current funit value
--      TRUE/FALSE  => is unit active?
--  SYSLOG_OPEN   => open requested name as a LOG file
--  SYSLOG_APPEND => open requested name as a LOG file in append mode
--       TRUE/FALSE  => success of opening
--  SYSLOG_SWITCH => connect *funit as active log file
--       TRUE/FALSE  => switched to an active session (funit != NULL)
--  SYSLOG_CLOSE => close current LOG file
--       TRUE/FALSE  => was a file really closed
--  SYSLOG_UNPAUSE    => Set pause count to 0  (enabled)
--  SYSLOG_PAUSE      => Set pause count to 1  (disabled)
--  SYSLOG_TMPPAUSE   => Increment pause count (effectively pauses or undoes)
--  SYSLOG_UNTMPPAUSE => Increment pause count (effectively unpauses or undoes)
--       TRUE/FALSE   => was there a file to really closed or disable
--  SYSLOG_SETMODE => set [LOG_STATUS] to the INTEGER value in *FUNIT
--	    **funit is recast as an INTEGER * and returns [LOG_STATUS] byte
--	        bit 0 => Collect anything useful echoed to the terminal
--			  bit 1 => Collect anything typed by the user
--       TRUE     => always returns true
--  SYSLOG_WRITE => Output string to current session if "is_on"
--  SYSLOG_FORCE => Force output string to the current log file
--       TRUE/FALSE => Able to output to current session
--
--	  name  - ASCII filename or string to use as the LOG file
--	  funit - file handle of the log file
--	  
-- Output: funit - If not NULL, returns current file handle of log file
--                 NULL value represents no active file
============================================================================ */
BOOL SysLogFile(INTEGER key, CHAR *string, FILE **funit) {

	BOOL rcode=FALSE;						/* Default return code */
	char OpenFlags[2]="w";					/* Open flags */
	int  i;

	switch (key) {

/* ----------------------------------
 SYSLOG_CHECK - query current funit value
      TRUE/FALSE  => is unit active?
------------------------------------- */
		case SYSLOG_CHECK:								/* Check on status */
			rcode = (LogHandle != NULL);				/* Return status */
			break;

/* -------------------------------------------------------
  SYSLOG_OPEN   => open requested name as a LOG file
  SYSLOG_APPEND => open requested name as a LOG file in append mode
       TRUE/FALSE  => success of opening
---------------------------------------------------------- */
		case SYSLOG_APPEND:									/* Open in append mode */
			OpenFlags[0] = 'a';
		case SYSLOG_OPEN:										/* Open specified file */
			if (LogHandle != NULL) fclose(LogHandle);
			if ( (LogHandle=fopen(string,OpenFlags)) == NULL) {
				gen_err("Failed to open log file (LOG_FILE_MSG)");
				break;
			}
			fprivate(LogHandle);				/* Set FD_NOEXEC flag */
			LogPause  = 0;						/* No pauses in effect */
			LogStatus = LOG_ECHO | LOG_COMMANDS;
			rcode = TRUE;
			break;

/* -------------------------------------------------------
  SYSLOG_SWITCH => connect *funit as active log file
       TRUE/FALSE  => switched to an active session (funit != NULL)
---------------------------------------------------------- */
		case SYSLOG_SWITCH:
			LogHandle = *funit;				/* Simply install (do not close) */
			LogPause  = 0;						/* No pauses in effect */
			LogStatus = LOG_ECHO | LOG_COMMANDS;
			rcode = (LogHandle != NULL);
			break;

/* -------------------------------------------------------
  SYSLOG_CLOSE => close current LOG file
       TRUE/FALSE  => was a file really closed
---------------------------------------------------------- */
		case SYSLOG_CLOSE:									/* Close */
			if (LogHandle != NULL) {
				fclose(LogHandle);			/* Effectively turn off */
				LogHandle = NULL;
				rcode = TRUE;
			}
			break;

/* ------------------------------------------------------------------
  SYSLOG_UNPAUSE    => Set pause count to 0  (enabled)
  SYSLOG_PAUSE      => Set pause count to 1  (disabled)
  SYSLOG_TMPPAUSE   => Increment pause count (effectively pauses or undoes)
  SYSLOG_UNTMPPAUSE => Increment pause count (effectively unpauses or undoes)
       TRUE/FALSE   => was there a file to really pause
------------------------------------------------------------------- */
		case SYSLOG_UNPAUSE:							/* Pause */
			LogPause = 0;
			rcode = (LogHandle != NULL);
			break;
		case SYSLOG_PAUSE:
			LogPause = 1;
			rcode = (LogHandle != NULL);
			break;
		case SYSLOG_TMPUNPAUSE:
			LogPause--;
			rcode = (LogHandle != NULL);
			break;
		case SYSLOG_TMPPAUSE:
			LogPause++;
			rcode = (LogHandle != NULL);
			break;
			
/* -------------------------------------------------------
  SYSLOG_SETMODE => set [LOG_STATUS] to the value in FUNIT (0 or 1)
	    *FUNIT is recast as an INTEGER
		 FUNIT returns previous status of the [LOG_STATUS] byte
	      bit 0 => Collect anything useful echoed to the terminal
			bit 1 => Collect anything typed by the user
       TRUE     => always returns true
---------------------------------------------------------- */
		case SYSLOG_SETMODE:
			i = LogStatus;
			LogStatus = *((INTEGER *) funit);
			*((INTEGER *) funit) = i;
			return(TRUE);

/* -------------------------------------------------------
  SYSLOG_WRITE => Output string to current session if "is_on"
  SYSLOG_FORCE => Force output string to the current log file
       TRUE/FALSE => Able to output to current session
---------------------------------------------------------- */
		case SYSLOG_WRITE:								/* Write string if "on" */
			if (LogPause > 0 || LogStatus == 0)		/* Are we "on" */
				break;		
		case SYSLOG_FORCE:								/* Write string */
			if (LogHandle != NULL) {
				fputs(string, LogHandle);
				fputc('\n', LogHandle);
				rcode = TRUE;
			}
			break;

		default:
			break;
	}

	if (funit != NULL) *funit = LogHandle;
	return(rcode);
}


/* ============================================================================
--     Routine to prompt terminal user for input and return text string.
--     Lowest level call for terminal input.
--
--     Syntax: CALL UINP$U(PRMPT,STR)
--
--     Inputs: PRMPT - String to be used for input prompt
--
--     Output: STR   - User typed string
--
-- Prompt if necessary using D_PROMPT attribute
-- Read from console   using D_INPUT  attribute
============================================================================ */
BOOL UserInput(const char *prmpt, char *str, int len) {

	BOOL rcode;

	*str = '\0';									/* Initially make NULL */
	if (prmpt != NULL) {
		ScrSetAttrib(D_NORMAL);
		TTYputs(prmpt);
	}
	ScrSetAttrib(D_INPUT);
	rcode = (TTYgets(str, len) != NULL);
	ScrSetAttrib(D_NORMAL);
	return(rcode);
}


/* ============================================================================
-- Routine to ring a bell to alert user
--
-- Syntax: RingBell()
--
-- Inputs: none
--
-- Output: ^G to terminal (or other bell)
============================================================================ */
void RingBell(void) {
	CONputc(0x07);									/* Send bell to console */
	return;
}

/* ============================================================================
-- Subroutines to output character strings to the terminal.  
--
-- Syntax: type2(CHAR *str1, CHAR *str2);
--
-- Inputs: string - Character string for output.			CHAR*(*)
--
-- Output: Outputs string to terminal.  TYPER follows string with a <newline>.
--
-- Notes: The number of characters output depends on the type of string.
============================================================================ */
void type2(const char *text1, const char *text2) {
	TTYputs(text1);
	TTYputsnl(text2);
	return;
}

/* ============================================================================
-- Subroutines to output error message strings to the terminal.  
--
--	Syntax:	void	ERRputs(char *str)			--> fputs error message
--				void	ERRputsnl(char *str)			--> fputs error message w/ NL
--				void	ERRprintf(char *form, ...)	--> fprintf error message
--
-- Inputs: str - Character string for output
--
-- Output: Outputs error message to terminal.  Bell is rung and message is
--         highlighted if possible.
============================================================================ */
void ERRputsnl(const char *string) {

	ScrSetAttrib(D_ERROR);
	RingBell();
	TTYputsnl(string);
	ScrSetAttrib(D_NORMAL);
	return;
}

void ERRputs(const char *string) {

	ScrSetAttrib(D_ERROR);
	RingBell();
	TTYputs(string);
	ScrSetAttrib(D_NORMAL);
	return;
}

void ERRprintf(const char *format, ...) {

	va_list var1;
	char ermsg[LONG_STR_SIZE];

	ScrSetAttrib(D_ERROR);
	RingBell();

	va_start(var1, format);
	vsprintf(ermsg, format, var1);
	va_end(var1);
	TTYputs(ermsg);
	ScrSetAttrib(D_NORMAL);
	return;
}

/* ============================================================================
-- ERROR Messages
--
-- gen_err (CHAR *text);
-- gen_err2(CHAR *text, CHAR *tok);
-- gen_warn(CHAR *text);
============================================================================ */
void gen_err(const char *text) {
	ERRprintf("ERROR: %s\n", text);
	return;
}

void gen_err2(const char *text, const char *tok) {
	ERRprintf("ERROR: %s (%s)\n", text, tok);
	return;
}

void gen_warn(const char *text) {
	ERRprintf("WARNING: %s\n", text);
	return;
}

/* ============================================================================
--           *******************************************************
--           *******************   LEVEL 0A  ***********************
--           *******************************************************
============================================================================ */

/* ============================================================================
-- Routine to clear the screen and return cursor to upper left corner
--
-- Syntax: call cls
--         call cls_attr(attr)
--
-- Inputs: attr - Attribute to use filling lines
--
-- Output: cursor control
============================================================================ */
void ScrClear(void) {
#ifdef NT
	if (! ConIsPipe) {
		COORD coord={0,0};
		int dummy;
		FillConsoleOutputCharacter(hwndStdout, ' ', 65536, coord, &dummy);
		SetConsoleCursorPosition(hwndStdout, coord);
		return;
	}
#endif
	CONputs("\033[H\033[2J");
	return;
}

int ScrClearAttrib(INTEGER attrib) {
	int rcode;
#ifdef NT
	if (! ConIsPipe) {
		COORD coord={0,0};
		int dummy;
		char *string;

		string = GetAttribStr(attrib);
		SetConsoleTextAttribute(hwndStdout, (WORD) string);
		FillConsoleOutputCharacter(hwndStdout, ' ', 65536, coord, &dummy);
		FillConsoleOutputAttribute(hwndStdout, (WORD) string, 65536, coord, &dummy);
		SetConsoleCursorPosition(hwndStdout, coord);

		rcode = ScrSetAttrib(attrib);
		return(rcode);
	}
#endif
	CONputs("\033[H");
	rcode = ScrSetAttrib(attrib);
	CONputs("\033[2J");
	return(rcode);
}

/* ============================================================================
-- Usage Guide:
--
--     Routine which changes the active color or text rendition on terminals so
--     equiped.  Useful for highlighting error messages and differentiating
--     output data.
--
--     Syntax: CALL ScrSetAttrib(CODE)
--
--     Inputs: CODE - Code of the text rendition desired. (INTEGER)
--                    See documentation on DCOL$S for meaning of this code.
--
--     Output: Sends control sequence to terminal.
============================================================================ */
int ScrSetAttrib(INTEGER attrib) {

	char *string;
	static int last_attrib=0;
	int rcode;

	rcode = last_attrib;
	if (attrib != -1) {
		string = GetAttribStr(attrib);
#ifndef NT
		if (string != NULL && *string != '\0') CONputs(string);
#else
		if (! ConIsPipe) {
			SetConsoleTextAttribute(hwndStdout, (WORD) string);
		} else {
			if (string != NULL && *string != '\0') CONputs(string);
		}
#endif
		last_attrib = attrib;
	}
	return(rcode);

}

/* ============================================================================
-- Routine which returns a string to switch the active color or text
-- rendition.  Useful for highlighting error messages and differentiating
-- output data.
--
-- Syntax: CALL DCOL$S(CODE,STRING)
--
-- Inputs: CODE - Code of the text rendition desired. (INTEGER)
--               -1 => Turn off color changing attributes
--                0 => Turn on  color changing attributes
--                1 => Normal computer output
--                2 => Normal computer input
--                3 => ERROR messages
--                4 => Alternate input
--              - For the TEK4105, selects CODE color - all # acceptable
--              - For the VT100,   selects as follows currently
--                1 - All attributes off
--                2 - All attributes off
--                3 - Bold characters
--                4 - Underscore
--                5 - Blink
--                6 - Reverse image
--
-- OUTPUTS:  STRING   ASCII sequence to do the function
--
-------------------------------------------------------------------
-- bit 1 => REVERSE
-- bit 2 => BOLD
-- bit 3 => UNDERLINE	NORM REVR BOLD      UNDL            
-- bit 4 => BLINK	0000 0001 0010 0011 0100 0101 0110 0111
-- bit 5 => Specific field uses
--
--  10h => title field  14h => Current input field   18h => Help on input field
--  11h => text fields  15h => Current input char    19h-20h unused
--  12h => separators   16h => secondary input field
--  13h => Input fields 17h => Help field
--
--  (01) 20h => computer output normal
--  (02) 21h => computer input normal
--  (03) 22h => bold output messages
--  (04) 23h => alternate input modes
--  (05) 24h => reserved
--  (06) 25h => reverse video w/ information/query information
--  (07) 26h => computer prompts for input
--  (08) 27h => computer error messages
--
-- User may have redefined list by setting global variable
--
--	typedef _SYSATTRIBLIST {
--		int mode;
--		char *string;
--	} SYSATTRIBLIST;
--
--	extern SYSATTRIBLIST *SysAttribList = NULL;
============================================================================ */
#if defined OS2
	#define	DEFAULT_TERMINAL	TEK4105
#elif defined NT
	#define	DEFAULT_TERMINAL	NT_TERM
#else
	#define	DEFAULT_TERMINAL	NONE
#endif

PRIVATE char *GetAttribStr(int attrib) {

	static enum {UNKNWN, NONE, VT100, TEK4105, NT_TERM} termtype=DEFAULT_TERMINAL;

#ifdef NT
	#define FORE_BLACK   (0)
	#define FORE_INTENSE (FOREGROUND_INTENSITY)
	#define FORE_WHITE   (FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE)
	#define FORE_RED     (FOREGROUND_RED)
	#define FORE_GREEN   (FOREGROUND_GREEN)
	#define FORE_BLUE    (FOREGROUND_BLUE)
	#define FORE_CYAN    (FOREGROUND_GREEN|FOREGROUND_BLUE)
	#define FORE_MAGENTA (FOREGROUND_RED|FOREGROUND_BLUE)
	#define FORE_YELLOW  (FOREGROUND_RED|FOREGROUND_GREEN)

	#define BACK_BLACK   (0)
	#define BACK_INTENSE (BACKGROUND_INTENSITY)
	#define BACK_WHITE   (BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE)
	#define BACK_RED     (BACKGROUND_RED)
	#define BACK_GREEN   (BACKGROUND_GREEN)
	#define BACK_BLUE    (BACKGROUND_BLUE)
	#define BACK_CYAN    (BACKGROUND_GREEN|BACKGROUND_BLUE)
	#define BACK_MAGENTA (BACKGROUND_RED|BACKGROUND_BLUE)
	#define BACK_YELLOW  (BACKGROUND_RED|BACKGROUND_GREEN)

	static SYSATTRIBLIST ntlist[] = {
		{0x10, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_MAGENTA)},/* title field						*/
		{0x11, (char *)(FORE_BLACK                 | BACK_CYAN)},	/* text fields						*/
		{0x12, (char *)(FORE_WHITE                 | BACK_YELLOW)},	/* separators						*/
		{0x13, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_GREEN)},	/* Input fields					*/
		{0x14, (char *)(FORE_YELLOW | FORE_INTENSE | BACK_RED)},		/* Current input field			*/
		{0x15, (char *)(FORE_YELLOW | FORE_INTENSE | BACK_GREEN)},	/* Current input char (huh?)	*/
		{0x16, (char *)(FORE_WHITE                 | BACK_RED)},		/* secondary input field		*/
		{0x17, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_YELLOW)},	/* Help field						*/
		{0x18, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_CYAN)},	/* Help on input field			*/

		{0x20, (char *)(FORE_WHITE                 | BACK_BLUE)},	/* D_NORMAL - normal output	*/
		{0x21, (char *)(FORE_YELLOW | FORE_INTENSE | BACK_BLUE)},	/* D_INPUT  - normal input		*/
		{0x22, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_BLUE)},	/* D_BOLD   - bold output		*/
		{0x23, (char *)(FORE_WHITE                 | BACK_BLUE)},	/* D_PROMPT - prompts input	*/
		{0x24, (char *)(FORE_RED    | FORE_INTENSE | BACK_BLUE)},	/* D_ERROR  - error messages	*/
		{0x25, (char *)(FORE_CYAN   | FORE_INTENSE | BACK_BLUE)},	/* D_FILEIN - file input		*/
		{0x26, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* alternate input modes		*/
		{0x27, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* reserved							*/

		{0x01, (char *)(FORE_WHITE                 | BACK_BLUE)},	/* computer output (D_NORMAL)	*/
		{0x02, (char *)(FORE_YELLOW | FORE_INTENSE | BACK_BLUE)},	/* computer input  (D_INPUT)	*/
		{0x03, (char *)(FORE_WHITE  | FORE_INTENSE | BACK_BLUE)},	/* bold output     (D_FILEIN)	*/
		{0x04, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* alternate input modes		*/
		{0x05, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* reserved 						*/
		{0x06, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* reverse video					*/
		{0x07, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* computer prompts for input */
		{0x08, (char *)(FORE_WHITE                 | BACK_BLACK)},	/* computer error messages		*/

		{0xFF, (char *)(FORE_WHITE                 | BACK_BLACK)}	/* default value					*/
	};
#endif

	static SYSATTRIBLIST teklist[] = {
		{0x10, "\033[0;1;37;45m"},		/* title field   - white on magenta    */
		{0x11, "\033[0;30;46m"},		/* text fields   - black on cyan       */
		{0x12, "\033[0;37;43m"},		/* separators    - gray on brown       */
		{0x13, "\033[0;1;37;42m"},		/* Input fields  - white on dark green */
		{0x14, "\033[0;1;33;41m"},		/* Current input field  - yellow on red */
		{0x15, "\033[0;1;33;42m"},		/* Current input char (huh?) - yellow on dark green */
		{0x16, "\033[0;37;41m"},		/* secondary input field - gray on red */
		{0x17, "\033[0;1;37;43m"},		/* Help field    - white on brown */
		{0x18, "\033[0;1;37;46m"},		/* Help on input field - white on cyan */

		{0x20, "\033[0;44;37m"},		/* D_NORMAL - computer output normal		*/
		{0x21, "\033[0;44;33;1m"},		/* D_INPUT  - computer input normal			*/
		{0x22, "\033[0;44;37;1m"},		/* D_BOLD   - computer output bold			*/
		{0x23, "\033[0;44;37m"},		/* D_PROMPT - computer prompts for input	*/
		{0x24, "\033[0;44;31;1m"},		/* D_ERROR  - computer error messages		*/
		{0x25, "\033[0;44;36;1m"},		/* D_FILEIN - text input from files			*/
		{0x26, "\033[0m"},				/* alternate input modes						*/
		{0x27, "\033[0m"},				/* reserved											*/

		{0x01, "\033[0;44;37m"},		/* computer output normal (D_NORMAL)		*/
		{0x02, "\033[0;44;33;1m"},		/* computer input normal  (D_INPUT)			*/
		{0x03, "\033[0;44;37;1m"},		/* bold output messages   (D_FILEIN)		*/
		{0x04, "\033[0m"},				/* alternate input modes						*/
		{0x05, "\033[0m"},				/* reserved */
		{0x06, "\033[0m"},				/* reverse video w/ information/query information */
		{0x07, "\033[0m"},				/* computer prompts for input */
		{0x08, "\033[0m"},				/* computer error messages */

		{0xFF, "\033[0m"}					/* default value				*/
	};

	static SYSATTRIBLIST vt100list[] = {
		{0x10, "\033[0;7m"},				/* title field */
		{0x11, "\033[0m"},				/* text fields */
		{0x12, "\033[0m"},				/* separators */
		{0x13, "\033[0m"},				/* Input fields */
		{0x14, "\033[0;7m"},				/* Current input field */
		{0x15, "\033[0m"},				/* Current input char (huh?) */
		{0x16, "\033[0;7m"},				/* secondary input field */
		{0x17, "\033[0;7m"},				/* Help field */
		{0x18, "\033[0;1m"},				/* Help on input field */

		{0x20, "\033[0m"},				/* D_NORMAL - computer output normal		*/
		{0x21, "\033[0m"},				/* D_INPUT  - computer input normal			*/
		{0x22, "\033[0;1m"},				/* D_BOLD   - computer output bold			*/
		{0x23, "\033[0;4m"},				/* D_PROMPT - computer prompts for input	*/
		{0x24, "\033[0;5m"},				/* D_ERROR  - computer error messages		*/
		{0x25, "\033[0;7m"},				/* D_FILEIN - text input from files			*/
		{0x26, "\033[0m"},				/* alternate input modes						*/
		{0x27, "\033[0;1m"},				/* reserved											*/

		{0x01, "\033[0m"},				/* computer output normal (D_NORMAL)		*/
		{0x02, "\033[0m"},				/* computer input normal  (D_INPUT)			*/
		{0x03, "\033[0;1m"},				/* bold output messages   (D_FILEIN)		*/
		{0x04, "\033[0;4m"},				/* alternate input modes						*/
		{0x05, "\033[0;5m"},				/* reserved */
		{0x06, "\033[0;7m"},				/* reverse video w/ information/query information */
		{0x07, "\033[0m"},				/* computer prompts for input */
		{0x08, "\033[0;5m"},				/* computer error messages */

		{0xFF, "\033[0m"}					/* default value				*/
	};

	SYSATTRIBLIST *mylist=NULL;
	char *aptr;

/* Use the user string list if he has set one */
	if (SysAttribList == NULL && termtype == NONE) {
		if ( (aptr=getenv("term")) == NULL) aptr=getenv("TERM");
		if (aptr != NULL) {
			if (strnicmp(aptr, "TEK", 3) == 0 || strnicmp(aptr, "TK", 2) == 0) {
				termtype = TEK4105;
			} else if (strnicmp(aptr, "VT", 2) == 0) {
				termtype = VT100;
			} else if (strnicmp(aptr, "NT", 2) == 0) {
				termtype = NT_TERM;
			} else {
				termtype = UNKNWN;
			}
		} else {
			termtype = TEK4105;
		}
	}
		
	if (SysAttribList != NULL) {
		mylist = SysAttribList;
	} else if (termtype == VT100) {
		mylist = vt100list;
	} else if (termtype == TEK4105) {
		mylist = teklist;
#ifdef NT
	} else if (termtype == NT_TERM) {
	  	mylist = (ConIsPipe) ? teklist : ntlist;
#endif
	} else {
		return(NULL);
	}
		
	while (mylist->mode != 0xFF) {
		if (mylist->mode == attrib) return(mylist->string);
		mylist++;
	}
	return(NULL);
}

/* ============================================================================
-- Subroutine to print text at a specified row and column position
--
-- Syntax: call putstr$(row,col,string,attrib)
--
-- Inputs: row, col - Position to put at
--         string   - Character string to print
--         attrib   - attrib to use
--                    7 => Normal   31 => Intens  159 => Attn   112 => Bold
--
-- Note: Cursor position is not saved by this command.  Call getpos$ if wanted.
============================================================================ */
void ScrPutString(INTEGER row, INTEGER col, CHAR *string, INTEGER attrib) {

	ScrSetPosn(row, col, attrib);
	TTYputs(string);
	return;
}


/* ============================================================================
-- Subroutine to set the current cursor position
--
-- Syntax: call setpos$(row,column,attrib)
--
-- Output: row    - current row
--         column - current column
--         attrib - Attribute to use
============================================================================ */
void ScrSetPosn(INTEGER row,INTEGER col,INTEGER attrib) {

	char string[20];
	if (row == 0) row = 1;
	if (col == 0) col = 1;

#ifdef NT
	if (! ConIsPipe) {
		COORD coord={row-1,col-1};
		SetConsoleCursorPosition(hwndStdout, coord);
		ScrSetAttrib(attrib);
		return;
	}
#endif

	sprintf(string,"\033[%i;%iH", row, col);
	if (*string != '\0') CONputs(string);
	ScrSetAttrib(attrib);
	return;
}
