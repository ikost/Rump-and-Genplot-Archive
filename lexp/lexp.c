/* lexp.c */

/* ===========================================================================
-- Modification history
--
-- 9/1/96 - MOT
--      Changed handling considerably.  Buffer space is now allocated for each
--      LEXFILE level rather than one for the stream.  Buffer is an allocated
--      entity so can be modified in size as needed.  Will expand to be hold
--      whatever size object is passed.
=========================================================================== */
#define	MAX_LINELEN		HUGE_STR_SIZE			/* Size of largest token		 */
#define	BUF_SIZE			2*MAX_LINELEN			/* Initial # chars in buffer	 */
#define	MAX_FORMATLEN	256						/* Length of a format token	 */

#define	DEFAULT_DELIM_LIST	" \t,;\n\r"		/* < > <tab> <,> <;> <NL> <CR> */
#define	OPEN_CHARS				"{(["				/* Open parenthesis list		 */
#define	CLOSE_CHARS				"})]"				/* Close parenthesis list		 */

#if (defined OS2 || defined NT)
	#define	LOGICAL_EOF			0x1A				/* ^Z Logical close on file */
#else
	#define	LOGICAL_EOF			0x03				/* ^D for UNIX */
#endif

#define	NOLDS					10
#define	QUOTE					'\"'					/* Double quote char				*/
#define	SQUOTE				'\''					/* Single quote char				*/
#define  TOKSAVEMARKER		((char) 0xFE)		/* Marker in ptr for saved token */

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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <stddef.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	LEXP_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#define LEXP_EXTENSIONS
#include "lexp.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct dirent DIRENT;

typedef enum _FMMODE {FM_NONE, FM_COUNT, FM_WILD, FM_LIST} FMMODE;

typedef enum _MEM_MODE {MEM_NONE, MEM_COUNT, MEM_CONDITION, MEM_LIST, MEM_LOOP} MEM_MODE;

typedef enum _FILETYPE {CONSOLE,SIMPLEFILE,PIPE,MEMORY} FILETYPE;


typedef struct _LEXFILE {
	int		 id;										/* Structure ID					*/
	struct	_LEXFILE *backlink;					/* Pointer to last LEXFILE		*/

	FILETYPE	 filetype;								/* Type of unit					*/
	FILE		*funit;									/* File/Pipe for this one		*/
	char		*memory, *memptr;						/* For memory units				*/

	MEM_MODE	 mem_mode;								/* Mode to handle memory		*/
	int		 mem_maxcount;							/* Number of iterations			*/
	int		 mem_count;								/* Current loop/for counter	*/
	char		*mem_condition;						/* Test condition					*/
	char		*mem_listptr;
	int		 mem_ival, mem_inc;					/* FORTRAN loop values			*/

	BOOL		 echo;									/* Is this unit echo'ed			*/
		#define	DO_ECHO	0x80						/* Echo local commands			*/
		#define	DO_DEBUG	0x01						/* Echo via debug mode			*/
	BOOL		 can_give_args;						/* Is in "args" pass status	*/
															/* &getarg can get one now		*/
															/* from this unit					*/

	int		 bufsize;								/* Size of linebuf buffer		*/
	char		*linebuf;								/* input line buffer				*/
	char		*ptr;										/* Pointer into linebuf			*/

	FMMODE	 fmmode;									/* If we are in fm mode			*/
	int		 fmrepeat;								/* Repeat mode if requested	*/
	char		 fmchar;									/* Substitute char (%f)			*/
	char		*fmcommand;								/* String of last command		*/
	char		*fmvarlist;								/* List of variables				*/
	char		*fmvarptr;								/* Pointer into variable list	*/
	char		*fmnamestart;
	DIR		*fmdirp;									/* Pointer to directory			*/
	int		 fmcnt;									/* Number we've seen here		*/

	char		**sub_vars[26];						/* %a-%z substitution strings	*/

/* Note: loop_value is place before fname/pname in case P. Smith wants to use
--       a very large "ForFormat" expression.  Will overwrite save memory */
	char		 loop_value[20];						/* String value from loop %x	*/
	char		 fname[PATH_MAX];						/* Macro filename (as given)	*/
	char		 pname[PATH_MAX];						/* Full pathname of file		*/

} LEXFILE;


typedef struct _LEXSTREAM {						/* LEXP typedefs as LEXSTREAM */
	int   cookie;										/* Magic cookie					*/
	LEXFILE *lf;										/* Currently active file		*/
	char  tok_sav[MAX_LINELEN];					/* Saved token for BACKUP		*/
	int   ptrndx;										/* My backup index				*/
	ptrdiff_t ptrold[NOLDS];						/* Backup points (ptr-linebuf) */
} LEXSTREAM;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
#if ! (defined MSC60 && defined _DLL)
	void		 sv_undo(void);
#endif

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
typedef enum {NORMAL, DUMP} POPMODE; 

PRIVATE	void		trnslt(char *tokout, char *tokin, INT toklen, LEXFILE *mylf);
PRIVATE	void		PopLexFile(void);
PRIVATE	int		PushLexFile(char *name);
PRIVATE	LEXFILE *CreateLexFile(LEXFILE *last);
PRIVATE	void		DestroyLexFile(LEXFILE *ltmp);
PRIVATE  void		SubSpecial(char *token, int len, char *str, LEXFILE *mylf);
PRIVATE	void		SubSpecialOld(char *token, int len, char *str, char subst, int cnt, char *file);
PRIVATE	BOOL		ObsoletePackString(char *str, int toklen);
PRIVATE	BOOL		ModifyFormat(char *format, size_t length);
PRIVATE	BOOL		YesNo_U(char *prompt, BOOL dflt);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
#if (defined OS2 || defined NT)
	EXPORT char LexMacroSearchPath[LONG_STR_SIZE] = "\0";
	EXPORT char LexMacroExtList[PATH_MAX] = ".mac;.xeq;.cmd";
#else
	EXPORT char LexMacroSearchPath[LONG_STR_SIZE] = "\0";
	EXPORT char LexMacroExtList[PATH_MAX] = ".mac:.MAC:.xeq:.XEQ:.cmd:.CMD";
#endif

/* Make it available so others can access if they understand what's happening */
/* Only visible if LEXP_EXTENSIONS is defined */
/* Mode to parse within */
LEXCONVERTMODE LexConvertMode=FIXED;
LEXFLUSHMODE	LexFlushMode = FLUSH_QUERY;
int				LexFlushCount = 0;			/* Number of flushes occuring */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
PRIVATE	LEXSTREAM *lstream=NULL;
PRIVATE	LEXFILE	 *lf=NULL;

/* Parameters which need not be held on input stream switch */
PRIVATE char MacroName[PATH_MAX]="<stdin>",
				 MacroPath[PATH_MAX]="<stdin>";
PRIVATE char ForFormat[SHORT_STR_SIZE];

PRIVATE char	 DelimList[MAX_DELIMS] = DEFAULT_DELIM_LIST;
PRIVATE BOOL	 SawEscape     = FALSE;				/* Did user type <ESC> to abort?	*/
PRIVATE LEXCASEMODE UserCaseMode = CASE_SAME;	/* Default parse conversion		*/
PRIVATE LEXCASEMODE CaseMode     = CASE_SAME;	/* Specific parse conversion		*/

PRIVATE	BOOL	In_GOTO_Request = FALSE;			/* Searching goto mode */
PRIVATE	BOOL	LexStreamClosed = FALSE;			/* Was stream closed last moment */

/* ===========================================================================
-- Routine to create the lexical input stream for all subsequent routines
--
-- Usage:  void *LexCreateStream();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: pointer to a structure describing a input stream for Lexical
--          processing.  Although (void *), really is LEXSTREAM * and can
--          be passed LexSwitchStream() or LexDestroyStream().
--          NULL if unable to create the stream.
--
-- Errors: Only if can't allocate memory
=========================================================================== */
void *LexCreateStream(void) {

	LEXSTREAM *ls;

	if ( (ls = malloc(sizeof(LEXSTREAM))) == NULL)  /* Stream pointer		*/
		return(NULL);

	ls->cookie   = 0x1210;							/* My birthday					*/
	*ls->tok_sav = '\0';								/* And the save tokens		*/
	ls->ptrndx   = -1;								/* And the backup tokens	*/

/* And create the base file string */
	if ( (ls->lf = CreateLexFile(NULL)) == NULL) {
		ERRprintf("FATAL: Major screwup in LexCreateStream()\n");
		free(ls);
		ls = NULL;
	}

	return((void *) ls);
}


/* ===========================================================================
-- Routine to destroy a lexical input stream
--
-- Usage: LexDestroyStream(void *stream);
--
-- Inputs: stream - valid Lexical stream opened by LexCreateStream
--
-- Output: none
--
-- Returns: 0 if successful, -1 if stream was not valid
--
-- Output: closes all currently open files and then deallocates the stream
=========================================================================== */
int LexDestroyStream(void *vls) {

	LEXSTREAM *ls;
	LEXFILE   *last;

	ls = (LEXSTREAM *) vls;
	if (ls == NULL || ls->cookie != 0x1210) return(-1);

	while (ls->lf != NULL) {
		last = ls->lf;
		ls->lf = last->backlink;
		DestroyLexFile(last);
	}

	ls->cookie = 0;						/* So can't be restarted with call */
	free(ls);								/* And finally the stream	*/
	return(0);
}


/* ===========================================================================
-- Routine to switch a lexical input stream to new one (returning previous)
--
-- Usage:  void *LexSwitch(void *newstream);
--
-- Inputs: newstream - new LEXSTREAM to use, returned from LexCreateStream
--
-- Output: none
--
-- Returns: pointer to stream currently active (of internal type LEXSTREAM *)
--          but structure completely unknown to user.  If unable to switch
--          streams, returns NULL.
=========================================================================== */
void *LexSwitchStream(void *vnew) {

	LEXSTREAM *tmp, *new;
	LEXFILE *ltmp;
#if (defined OS2 || defined NT)
	char *aptr;
#endif

	new = (LEXSTREAM *) vnew;
	if (new == NULL || new->cookie != 0x1210) return(NULL);

	tmp = lstream;
	lstream = new;
	lf = lstream->lf;

	ltmp = lf;
	while (ltmp->filetype != MEMORY && ltmp->backlink != NULL) ltmp = ltmp->backlink;
	strcpy(MacroName, ltmp->fname);
	strcpy(MacroPath, ltmp->pname);
#if (defined OS2 || defined NT)
	while ( (aptr=strchr(MacroName, '\\')) != NULL) *aptr = '/';
	while ( (aptr=strchr(MacroPath, '\\')) != NULL) *aptr = '/';
#endif

	return((void *) tmp);
}


/* ============================================================================
-- Usage: LexInitialize()
--
-- Inputs: none
--
-- Output: Initializes Console I/O and resets my variables
============================================================================ */
void LexInitialize(void) {
	int i;

	CONInitialize();								/* Initialize CONsole if not done */

	if (lstream == NULL) {						/* Do I have a stream? */
		lstream = LexCreateStream();
		lf = lstream->lf;							/* And initial LEXFILE active */
	}
	GVLinkInt("$SysDebugFlag", GVF_INTERNAL | GVF_HIDDEN, (INT *) &SysDebugFlag);

	srand((unsigned int) time(NULL));		/* Seed the random number generator */
	for (i=0; i<100; i++) rand();				/* And randomize */

	LexReset(TRUE);								/* And reset myself */
	return;
}

/* ============================================================================
-- Usage: CALL RST$LEXP
--
-- Inputs: none
--
-- Output: Resets all common block variables
============================================================================ */
void LexReset(BOOL FullReset) {

	LEXFILE *mylf;

	LexSystem(U_QUIT,NULL);						/* Handle LOGFILE and LEARN */

	GVLinkString("$MacroName", GVF_INTERNAL | GVF_HIDDEN | GVF_CONSTANT | GVF_NODELETE,
                 MacroName, sizeof(MacroName));
	GVLinkString("$MacroPath", GVF_INTERNAL | GVF_HIDDEN | GVF_CONSTANT | GVF_NODELETE,
                 MacroPath, sizeof(MacroPath));
	GVLinkString("$FORFormat", GVF_INTERNAL | GVF_HIDDEN, ForFormat, sizeof(ForFormat));

	strcpy(ForFormat, "%.3i");					/* Reset FOR/REPEAT loop counters */

	strcpy(DelimList, DEFAULT_DELIM_LIST);	/* Reset the delimiters	*/
	UserCaseMode   = CASE_SAME;				/* No case translation	*/
	LexConvertMode = FIXED;						/* Parse mode is fixed	*/

	LexFlushMode = FLUSH_QUERY;				/* Reset to query mode */
	GVLinkInt("$LexFlushMode",  GVF_INTERNAL | GVF_HIDDEN, (INT *) &LexFlushMode);
	GVLinkInt("$LexFlushCount", GVF_INTERNAL | GVF_HIDDEN, &LexFlushCount);

	for (mylf=lf; mylf!=NULL; mylf=mylf->backlink) mylf->echo |= DO_ECHO;
	return;
}

/* ============================================================================
-- Routine to check the status of the <ESC> flag.
--
--     Usage: BOOL LexEscape(flag)
--
--     Inputs: FLAG - .TRUE.  => Clear the flag after returning the status
--	             .FALSE. => Leave the status as is
--
--     Output: E_CHECK - Did an <ESC> occur on the last read
--
-- Note: If <ESC> occured, the target value will be unchanged.
============================================================================ */
BOOL LexEscape(BOOL flag) {

	BOOL temp_flag;
	
	temp_flag = SawEscape;
	if (flag) SawEscape = FALSE;
	return(temp_flag);
}

/* ===========================================================================
-- Routine to parse a given line into token.  Handles the possibility of "" in
-- the name.
--
-- Usage:  BOOL LexParseLine(char *tok, INT len, char *line, char **newline);
-- Usage:  BOOL LexParseLineEx(char *tok, INT len, char *line, char **newline, int ConvertMode, char *delims);
--
-- Input:  line    - pointer to input string containing line to parse
--         len     - maximum # of characters to save in token (including NULL)
--         newline - If not NULL, address to receive updated line position
--         mode    - Bit settings determining how the parse is to behave.
--                   Same as LexConvertMode from LEXP_EXTENSIONS.  It is not
--                   not possible to query the QUOTED bit with LexParseLineEx.
--                   0 is a typical value, 1 for math mode.
--         delims  - string giving the delimiter characters.  If NULL, uses
--                   the LEXP defaults.
--
-- Output: token     - next token from the command line.  
--                     NULL if there were no tokens remaining
--         LexParseLine - TRUE gives success
--        *newline   - If newline not NULL, will receive pointer into line at
--                     start of next token.
--
-- Notes: LexParseLine is identical to LexParseLineEx using the internal
--        LexConvertMode and delimiter list -- except that LexParseLine will
--        also potentially upper or lower case the string via CaseMode
--        setting.  LexParseLineEx always returns a case retentive token.
--        LexParseLine also accesses the internal options of Ex for use by
--        Lexp routines.
=========================================================================== */
static int ParseConvertMode;							/* For private communication */

BOOL LexParseLine(char *token, int maxlen, char *line, char **newline) {

	BOOL rcode;

	rcode = LexParseLineEx(token, maxlen, line, newline, LexConvertMode, DelimList);

/* Handle case conversion as per user setting */
	if (token != NULL) {
		if (CaseMode == CASE_UPPER) strupr(token);
		if (CaseMode == CASE_UPPER) strlwr(token);
	}

	LexConvertMode = ParseConvertMode;			/* Reset to new value	*/
	CaseMode       = UserCaseMode;				/* Reset to default		*/
	return(rcode);
}


BOOL LexParseLineEx(char *token, int maxlen, char *line, char **newline, int mode, char *delims) {

	int		 quotechar;
	int		 NumParen;							/* Num of () parens			*/
	char		 lastchr;							/* Terminating char			*/
	char		*iptr, *optr;						/* Input/output pointers	*/
	char		 result[HUGE_STR_SIZE];			/* Very long tmp result		*/
	BOOL		 Done, QuoteMode;					/* Various flags				*/

/* Use internal defaults if called via LexParseLine */
	if (delims == NULL) delims = DelimList;

/* Strip values in the conversion mode flag which are set versus checked */
	ParseConvertMode = mode & PARSE_CONVERT_MASK;

/* Check on trivial results */
	if (line == NULL) {							/* Avoid working non-strings */
		if (token   != NULL) *token = '\0';
		if (newline != NULL) *newline = NULL;
		goto ParseExit;
	}
	if (token != NULL) *token = '\0';		/* In case we exit early		*/
	optr = result;									/* For storing results			*/

/* ---------------------------------------- 
-- Separate out -- handle mathmode separate 
-- from general version
---------------------------------------- */
	Done = FALSE;									/* Starting assumption			*/
	if (ParseConvertMode & MATH) {

/* Skip white space */
		while (isspace(*line) || *line == '=') line++;

/* Handle quoted strings (which are actually painful! */
		if (*line == SQUOTE) {									/* If a quote char	*/
			quotechar = *line;									/* Keep for test		*/
			optr = result;											/* Where to put text	*/
			if (ParseConvertMode & RETAIN_QUOTE) *optr++ = SQUOTE;

			for (iptr=line+1; ;iptr++) {						/* Scan forward		*/
				if (*iptr == '\0') {
					line = iptr;									/* End of the token	*/
					Done = TRUE;
					break;
				} else if (*iptr == quotechar) {				/* Possible end		*/
					if (iptr[1] == quotechar) {				/* Escaped!				*/
						iptr++;
					} else if (iptr[1] == '\0' || strchr(delims, iptr[1]) != NULL) {
						line = iptr+1;								/* End of the token	*/
						Done = TRUE;
						break;
					} else if (iptr[2] == quotechar) {		/* Form of 'd' */
						*optr++ = *iptr++;
						*optr++ = *iptr++;
					} else {
						Done = FALSE;
						break;
					}
				}
				*optr++ = *iptr;
			}
			if (Done) {												/* If was quoted, handle */
				if (ParseConvertMode & RETAIN_QUOTE) *optr++ = SQUOTE;
				ParseConvertMode |= QUOTED;					/* Tell outside world */
			}
		}

/* If quotes did not finish, then just continue on -- being careful yet again */
		if (! Done) {												/* Something already?	*/
			Done = (*line != '\0');								/* Is anything there		*/
			optr = result;											/* Where to put text		*/
			lastchr  = 0x7F;
			NumParen  = 0;											/* And no parenthesis	*/
			QuoteMode = FALSE;									/* Not in quote mode		*/

			for (iptr=line; ;iptr++) {
				if (*iptr == '\0') {								/* This finishes all		*/
					line = iptr; break;
				} else if (QuoteMode) {							/* In quoted mode			*/
					if (*iptr == quotechar) {
						if (iptr[1] == quotechar) {
							*optr++ = *iptr++;
						} else {
							QuoteMode = FALSE;
						}
					}
				} else if (*iptr == QUOTE || *iptr == SQUOTE) {
					QuoteMode = TRUE;
					quotechar = *iptr;
				} else if (NumParen == 0) {					/* Bother looking?	*/
					if (strchr(delims, *iptr) != NULL) {
						line = iptr;
						break;
					} else if ( (*iptr == '=')	&&				/* Isolated = sign? */
									(strchr("!^=<>", lastchr) == NULL) &&
									(strchr("=<>", iptr[1]) == NULL)) {
						line = iptr;
						break;
					} else if (strchr(OPEN_CHARS, *iptr) != NULL) { /* Open parenthesis?	*/
						NumParen++;
					} else if (strchr(CLOSE_CHARS,*iptr) != NULL) {	/* Close parenthesis	*/
						if (NumParen) NumParen--;
					}
				} else if (strchr(OPEN_CHARS, *iptr) != NULL) { /* Open parenthesis?	*/
					NumParen++;
				} else if (strchr(CLOSE_CHARS,*iptr) != NULL) {	/* Close parenthesis	*/
					if (NumParen) NumParen--;
				}
				lastchr = *iptr;									/* Keep track */
				*optr++ = *iptr;
			}
		}

/* --------------------------------------------
-- Separate out -- handle non-mathmode separate 
-- from the mathmode version.
-------------------------------------------- */
	} else {

/* Skip white space */
		while (isspace(*line)) line++;						/* Skip white space	*/

/* Quoted strings (can be single or double quotes */
		if (*line == QUOTE || *line==SQUOTE) {				/* If a quote char	*/
			quotechar = *line;									/* Keep for test		*/
			optr = result;											/* Where to put text	*/
			if (ParseConvertMode & RETAIN_QUOTE) *optr++ = QUOTE;

			for (iptr=line+1; ;iptr++) {						/* Scan forward		*/
				if (*iptr == '\0') {
					line = iptr;									/* End of the token	*/
					Done = TRUE;
					break;
				} else if (*iptr == quotechar) {				/* Possible end		*/
					if (iptr[1] == quotechar) {				/* Escaped!				*/
						iptr++;
					} else if (iptr[1] == '\0' || strchr(delims, iptr[1]) != NULL) {
						line = iptr+1;								/* End of the token	*/
						Done = TRUE;
						break;
					} else {
						Done = FALSE;
						break;
					}
				}
				*optr++ = *iptr;
			}
			if (Done) {												/* If was quoted, handle */
				if (ParseConvertMode & RETAIN_QUOTE) *optr++ = QUOTE;
				ParseConvertMode |= QUOTED;					/* Tell outside world */
			}
		}

/* If quotes did not finish, then just continue on -- being careful yet again */
		if (! Done) {												/* Are we done yet?	*/
			Done = (*line != '\0');								/* Is anything there		*/
			optr = result;											/* Where to put text		*/
			for (iptr=line; ;iptr++) {
				if (*iptr == '\0') {
					line = iptr; break;
				} else if (strchr(delims, *iptr) != NULL) {
					line = iptr; break;
				}
				*optr++ = *iptr;
			}
		}
	}

/* Now, copy over as long as we have somewhere to go */
	*optr = '\0';
	if (token != NULL) { strncpy(token, result, maxlen); token[maxlen-1] = '\0'; }
	
/* Now, delete white space between tokens.  If we termed on a <white> char, we
   also swallow the next char if it is a delimiter
   1. Move pointer forward as long as whitespace
	2. If not at EOS, string termed on a whitespace, and at a delimiter, move
		pointer over the delimiter and any more whitespace.
.. */
	if (newline != NULL) {
		if (isspace(*line)) {						/* <sp> terminator - check for other */
			while (isspace(*line)) line++;
			if (*line && (strchr(delims,*line) != NULL)) line++;
		} else if (*line) {							/* <delim> terminator - skip */
			line++;
		}
		while (isspace(*line)) line++;			/* Eliminate white space after */
		*newline = line;
	}
		
ParseExit:
/* Reset ConvertMode to normal */
	ParseConvertMode = (ParseConvertMode & ~PARSE_CONVERT_MASK) | FIXED;
	return(Done);
}


/* ============================================================================
--     Routine to set another set of characters as token delimiters.  A token
--     delimiter marks the END of a token, beginning is identified as the first
--     non-whitespace character.
--
--     Usage:  void delim(char *chrs)
--
--     Inputs: chrs - string (or array) of delimiters
--
--     Output: none
--
--     Note: Delimiters need not include any of the whitespace characters.
--           However, whitespace characters remains special.  Whitespace
--           between a delimiter and the start of the next token is ignored.
--           Whitespace within a token the next delimiter may be included in
--           the token.  For instance, if "?/" are set as delimiters
--           "  THIS IS A TEST    /   WHERE?       FOR  THIS  /AT parses into
--           "THIS IS A TEST   " "WHERE" "FOR THIS  " "AT".
============================================================================ */
void LexSetDelim(char *delims) {
	strscpy(DelimList, delims, sizeof(DelimList));		/* Copy as possible		 */
	return;
}


/* ============================================================================
--     This routine returns the address of the string containing the
--     delimiters chars.  String can be selectively saved and restored.
--
--     Usage:   char *LexGetDelim();
--
--     Inputs:  none
--
--     Outputs: getdel - address of the internal list of delimiters
--                       (NULL terminated)
--
--     Used in MATHPARSE to allow , in exp's.
============================================================================ */
char *LexGetDelim(void) {
	return(DelimList);
}


/* ============================================================================
--     Routine to change the case conversion flag of parse
--
--     Usage: INT setcas(INT flag)
--
--     Inputs: flag: -1 ==> Convert string to lowercase 
--                    0 ==> Don't change case of strings
--                    1 ==> Convert string to uppercase
--
--     Output: setcas: previous state of the flag
============================================================================ */
LEXCASEMODE LexSetCase(LEXCASEMODE flag) {
	
	INT ltmp;

	ltmp = UserCaseMode;
	UserCaseMode = CaseMode = flag;					/* Set both of these now */
	return(ltmp);
}


/* ============================================================================
-- Usage: BOOL LexGetMath (char *token, INT toklen)
--        BOOL LexGetMathP(char *token, INT toklen, char *prompt)
--
--     Returns next lexical entity equation or expression from command line.
--     MATH mode matches open/close parenthesis and breaks on isolated = signs.
--     Leading = signs are striped as preceedors of functions.
--     "=f(x)" becomes "f(x)"  and "= f(x)" becomes "f(x)".
--
-- Usage: BOOL LexGetFile (char *token, INT toklen)
--        BOOL LexGetFileP(char *token, INT toklen, char *prompt)
--
--     Routine to get the next lexical entity regarding it as a filename.
--     Retains case and reduces delimiters to <cr><lf><sp><tab> only.
--
-- Usage: BOOL LexGetStrExpr (char *token, INT toklen)
--        BOOL LexGetStrExprP(char *token, INT toklen, char *prompt)
--
--     Routine to get the string from the command line.  Strings will
--     retain the " if they were enclosed in quotes, and will otherwise
--     parse similar to math mode in following () pairs.  Used in routines
--     which may later process strings -- strcat("this"," is ","a string")
--
-- Usage: BOOL LexGetList (char *token, INT toklen)
--        BOOL LexGetListP(char *token, INT toklen, char *prompt)
--
--     Routine to return a comma delimited list from command line.  Separaters
--     are set to =,<sp>,<tab>,<null> only.
--
-- Usage: BOOL LexGetSearchPath (char *token, INT toklen)
--        BOOL LexGetSearchPathP(char *token, INT toklen, char *prompt)
--
--     Routine to return a semicolon or colon delimited list of paths for
--     search path setting.  Separaters are set to <sp>,<tab>,<null> only.
--
-- Usage: BOOL LexGetToken(char *token, INT toklen)
--        BOOL LexGetTokenP(char *token, INT toklen, char *prmpt)
--        BOOL tokchk(char *token, INT toklen)
--
-- Returns next lexical entity on a command line.  The LexGetTokenP reads a new
-- command line if no tokens are left on the existing lines.  
--
-- Inputs: prompt  - Text to query with if no token (CHARACTER)
--         toklen  - Maximum # of chars available in token
--
-- Output: token  - Next available token (CHARACTER)
--         LexGetMath,LexGetMathP - Was any token returned?
============================================================================ */
BOOL	LexGetMath(char *tken, INT toklen) {
	return LexGetMathP(tken, toklen, NULL);
}
/* ------------------------------------------------------------------------- */
BOOL	LexGetMathP(char *tken, INT toklen, const char *prmpt) {
	LexConvertMode = MATH;
	return(LexGetTokenP(tken, toklen, prmpt));
}

/* ------------------------------------------------------------------------- */
BOOL	LexGetFile(char *tken, INT toklen) {
	return LexGetFileP(tken, toklen, NULL);
}
/* ------------------------------------------------------------------------- */
BOOL	LexGetFileP(char *tken, INT toklen, const char *prmpt) {
	BOOL rcode;
	char DelimHold[MAX_DELIMS];

	CaseMode = CASE_SAME;
	strcpy(DelimHold, DelimList);						/* Dup old list */
	strcpy(DelimList, " \t\n\r");						/* Permit , and others */
	rcode = LexGetTokenP(tken, toklen, prmpt);	/* Now, do it */
	strcpy(DelimList, DelimHold);						/* And restore list */
	return(rcode);
}

/* ------------------------------------------------------------------------- */
BOOL LexGetStrExpr (char *tken, INT toklen) {
	return LexGetStrExprP(tken, toklen, NULL);
}
/* ------------------------------------------------------------------------- */
BOOL LexGetStrExprP(char *tken, INT toklen, const char *prmpt) {
	BOOL rcode;
	char *aptr = NULL;
	int err;

	CaseMode = CASE_SAME;
	LexConvertMode = MATH | RETAIN_QUOTE ;
	rcode = LexGetTokenP(tken, toklen, prmpt);

	if (rcode) {
		if (GVGuessExprType(tken) == GVP_STRING) {
			aptr = GVEvalStrExpr(tken, &err);
		} else if (*tken == SQUOTE && tken[strlen(tken)-1] == SQUOTE) {
			tken[strlen(tken)-1] = '\0';
			memmove(tken, tken+1, strlen(tken));
		}
		if (aptr != NULL && err == 0) strscpy(tken, aptr, toklen);
		if (aptr != NULL) free(aptr);
	}

	return(rcode);
}

/* ------------------------------------------------------------------------- */
BOOL	LexGetList(char *tken, INT toklen) {
	return LexGetListP(tken, toklen, NULL);
}
/* ------------------------------------------------------------------------- */
BOOL	LexGetListP(char *tken, INT toklen, const char *prmpt) {
	BOOL rcode;
	char DelimHold[MAX_DELIMS];

	strcpy(DelimHold, DelimList);						/* Dup old list */
	strcpy(DelimList, (LexConvertMode & MATH) ? " \t\n\r" : " \t\n\r=");
	rcode = LexGetTokenP(tken, toklen, prmpt);	/* Now, do it */
	strcpy(DelimList, DelimHold);						/* And restore list */
	return(rcode);
}

/* ------------------------------------------------------------------------- */
BOOL	LexGetSearchPath(char *tken, INT toklen) {
	return LexGetSearchPathP(tken, toklen, NULL);
}
/* ------------------------------------------------------------------------- */
BOOL	LexGetSearchPathP(char *tken, INT toklen, const char *prmpt) {
	BOOL rcode;
	char DelimHold[MAX_DELIMS];

	CaseMode = CASE_SAME;
	strcpy(DelimHold, DelimList);						/* Dup old list */
	strcpy(DelimList, " \t\n\r");						/* Make very simple */
	rcode = LexGetTokenP(tken, toklen, prmpt);	/* Now, do it */
	strcpy(DelimList, DelimHold);						/* And restore list */
	return(rcode);
}

/* ------------------------------------------------------------------------- */
BOOL	LexGetTokenP(char *tken, INT toklen, const char *prmpt) {

	LEXCONVERTMODE cvtmp;
	BOOL rcode;

	cvtmp = LexConvertMode;							/* Save in case reread */

	rcode = LexGetToken(tken, toklen);
	if (prmpt == NULL) return(rcode);
	
	if (! rcode) {										/* Is a token already available	*/
		LexReadLine(prmpt);							/* No, read a new line from user	*/
		LexConvertMode = cvtmp;
		LexGetToken(tken,toklen);
	}
	if (tken[0] == '/' && tken[1] == '\0') return(FALSE);		/* Inline dflt */
	return(*tken != '\0');
}


#define	NUM_AMP_LIST	12

typedef struct _AMP_LIST {
	char *name;
	enum {GETARG, QUERY, YESNO, ENCODE, ROOTARG, REMOTE, ENDITEM} rcode;
	void (*fnc)(char *str, int len);
} AMP_LIST;

static AMP_LIST ampersand_list[NUM_AMP_LIST] = {
	{"&getarg",	GETARG,	NULL}, {"&query",	QUERY,	NULL},
	{"&yesno",	YESNO,	NULL}, {"&encode",ENCODE,	NULL},
	{"&rootarg",ROOTARG, NULL}, {NULL,		REMOTE,	NULL},
	{NULL,		REMOTE,	NULL}, {NULL,		REMOTE,	NULL},
	{NULL,		REMOTE,	NULL}, {NULL,		REMOTE,	NULL},
	{NULL,		REMOTE,	NULL}, {NULL,		ENDITEM,	NULL}
};

/* ===========================================================================
-- Routine to add an &xxxx type entry to the processing list
--
-- Usage: LexAddAmpEntry(char *name, void (*fnc)(char *str, int len));
--
-- Inputs: name - the name (including the &) that should be recognized
--         fnc  - routine which, when called, fills in the string up to
--                specified length with appropriate string
--
-- Output: TRUE - successfully added, FALSE - no more room.
--
-- Once inserted, should behave just like all other &getarg commands.
=========================================================================== */
BOOL LexAddAmpEntry(char *name, void (*fnc)(char *str, int len)) {

	AMP_LIST *entry;
	for (entry=ampersand_list; entry->rcode != ENDITEM; entry++) {
		if (entry->name == NULL) {
			entry->name = strdup(name);
			entry->rcode = REMOTE;
			entry->fnc = fnc;
			return(TRUE);
		} else if (stricmp(entry->name, name) == 0) {
			entry->rcode = REMOTE;
			entry->fnc = fnc;
			return(TRUE);
		}
	}
	return(FALSE);
}


/* ----------------------------------------------------------------------------
-- Actual low level routine for all of the above 
---------------------------------------------------------------------------- */
BOOL LexIsEmpty(void) {

	if (SysChkBreak(FALSE)) {
		ERRputs("WARNING: Command line aborted by user ^C (LexIsEmpty)\n");
		LexFlush();
		return(FALSE);
	}

	return ( *lf->ptr!=TOKSAVEMARKER && ! LexParseLine(NULL,0,lf->ptr,NULL) );
}

/* ----------------------------------------------------------------------------
---------------------------------------------------------------------------- */
BOOL LexGetToken(char *tken, INT toklen) {

	char		*prmpt, *dflt, *oldptr;
	char		tok_short[10];
	char		tokline[MAX_LINELEN];
	char		format[MAX_FORMATLEN];
	char		arg_prompt[DFLT_STR_SIZE];
	char		*entry_ptr;
	LEXCONVERTMODE ConvertMode;						/* Local copy of conversion mode */

	AMP_LIST *entry;
	LEXFILE *mylf, *TranslateLF;
	int		i;

	TranslateLF = lf;										/* How to translate %i etc. */
	ConvertMode = LexConvertMode;						/* Local copy before parse */
	ConvertMode &= GETTOK_CONVERT_MASK;				/* Limit to my values		*/
	tken[0] = tken[toklen-1] = '\0';					/* Make sure we have EOS	*/
	entry_ptr = lf->ptr;									/* For backup save later	*/

	if (SysChkBreak(FALSE)) {
		ERRputs("WARNING: Command line aborted by user ^C\n");
		LexFlush();
		return(FALSE);
	}

/* If the pointer marks a token save (via lexcheck or otherwise),
   we return that value subject to translation flags.                */
	if (*lf->ptr == TOKSAVEMARKER) {
		BOOL no_translate;										/* Translate option	*/
		lf->ptr++;													/* Skip over it		*/
		while (isspace(*lf->ptr)) lf->ptr++;				/* And any blanks		*/
		no_translate = (ConvertMode & NO_TRANSLATE) ||	/* Specific request	*/
						 ! (lstream->tok_sav[0] & 0x80);		/* Or has been done	*/
		lstream->tok_sav[0] &= 0x7F;
		if (no_translate) {
			strscpy(tken, lstream->tok_sav, toklen);		/* Copy result			*/
		} else {
			trnslt(tken, lstream->tok_sav, toklen, NULL);/* Translate result	*/
		}
		goto LexGetTokenReturn;									/* And return now		*/
	}

/* Not already there, now try to parse from the command line */
	if (! LexParseLine(tokline, sizeof(tokline), lf->ptr, &lf->ptr)) {
		*tken = '\0';
		return(FALSE);
	}

/* Check if we have one of the &encode type parse objects */
	entry = NULL;
	if (*tokline == '&' && tokline[1] != '\0' && ! (ConvertMode & NO_AMP_EXPAND) ) {
		entry = ampersand_list;
		while (entry->name != NULL && stricmp(tokline,entry->name)!=0) entry++;
	}

/* 99% of the time, no &encode so just return value subject to translation */
	if (entry == NULL || entry->name == NULL) {	/* 99% of the time */
		if (ConvertMode & NO_TRANSLATE) {
			strscpy(tken, tokline, toklen);			/* Just copy over */
		} else {
			trnslt(tken, tokline, toklen, NULL);	/* Translate %xx% strings */
		}
		goto LexGetTokenReturn;
	}

/* 1% of time, interpret the &encode stuff */
	switch (entry->rcode) {
		case ENCODE:
			LexParseLine(format, sizeof(format), lf->ptr, &lf->ptr);	/* Get format */
			if (! ModifyFormat(format, sizeof(format))) {		/* Move F77 -> C */
				ERRputs("WARNING: Can't translate the FORTRAN format -- use C formats instead\n");
				strcpy(format, "%f");
			}
			LexEncodeString(tokline, sizeof(tokline), format);
			ConvertMode |= NO_TRANSLATE;						/* No more translation */
			break;

		case REMOTE:												/* External fncs */
			*tokline = '\0';
			(*entry->fnc)(tokline, sizeof(tokline));		/* Let function fill in */
			break;
			
		case YESNO:
		case GETARG:
		case ROOTARG:
		case QUERY:
			dflt   = NULL;
			prmpt  = NULL;
			while (TRUE) {
				oldptr = lf->ptr;					/* Keep track of posn here */
				LexParseLine(tok_short, sizeof(tok_short), lf->ptr, &lf->ptr);
				if (LexEqual(tok_short, "-default", 2)) {
					dflt  = lf->ptr;
					LexParseLine(tok_short, sizeof(tok_short), lf->ptr, &lf->ptr);
				} else if (LexEqual(tok_short, "-prompt", 2)) {
					prmpt = lf->ptr;
					LexParseLine(tok_short, sizeof(tok_short), lf->ptr, &lf->ptr);
				} else {
					lf->ptr = oldptr;
					break;
				}
			}

/* ... Check back on command line for all but QUERY */
			*tokline = '\0';								/* Nothing read yet */
			if (entry->rcode != QUERY) {				/* Rootarg backs all the way */
				for (mylf=lf->backlink; mylf!=NULL && mylf->ptr!=NULL; mylf=mylf->backlink) {
					if (! mylf->can_give_args) continue;
					if (LexParseLine(tokline, sizeof(tokline), mylf->ptr, &mylf->ptr)) {
						if (*tokline != '\0') TranslateLF = mylf;
						break;
					}
					if (entry->rcode != ROOTARG) break;
				}
			}

/* ... If don't have anything yet, create prompt and input */
			if (*tokline == '\0' && (prmpt != NULL || dflt == NULL)) {	/* Prompt for input	*/
				if (prmpt != NULL && 
					 LexParseLine(tokline, sizeof(tokline), prmpt, NULL) &&
					 *tokline != '\0' ) {
					trnslt(arg_prompt, tokline, sizeof(arg_prompt), NULL);
				} else {
					strcpy(arg_prompt, "Argument: ");		/* Default prompt		*/
				}
				UserInput(arg_prompt, tokline, sizeof(tokline));
			}

/* ... If still nothing, try the default response */
			if ( (*tokline=='\0' || strcmp(tokline,"/")==0) && dflt != NULL)
				LexParseLine(tokline, sizeof(tokline), dflt, NULL);	/* Use default */
			break;
			
		default:
			strcpy(tokline, "STUPID DEVELOPERS - TELL THEM SO");
	}
			
/* ... From the resulting stream, now interpret as appropriate */
	if (ConvertMode & NO_TRANSLATE) {
		strscpy(lstream->tok_sav, tokline, sizeof(lstream->tok_sav));
	} else {
		trnslt(lstream->tok_sav, tokline, sizeof(lstream->tok_sav), TranslateLF);	/* Translate %xx% */
	}

	if (entry->rcode == YESNO)								/* YES/NO argument? */
		strcpy(lstream->tok_sav, LexEqual(lstream->tok_sav,"YES",1) ? "1" : "0");

	strscpy(tken, lstream->tok_sav, toklen);
	if (ConvertMode & NO_TRANSLATE) lstream->tok_sav[0] |= 0x80;	/* Flag no translation done */

/* Mark in LexConvertMode that we've returned a "quoted string" unless YESNO */
	if (entry->rcode != YESNO) LexConvertMode |= QUOTED;

/* Mark in string that we have stored the value as tok_sav */
	oldptr = entry_ptr;										/* Recover starting point */
	*(oldptr++) = TOKSAVEMARKER;							/* Mark as "tok_sav" */
	*(oldptr++) = ' ';
	memmove(oldptr, lf->ptr, strlen(lf->ptr)+1);
	lf->ptr = oldptr;
	goto LexGetTokenReturn;

/* All returns go through here to set backup pointers (if successful) */
LexGetTokenReturn:
	if (++lstream->ptrndx == NOLDS) {				/* Next backup marker */
		for (i=--lstream->ptrndx; i; i--)
			lstream->ptrold[i-1] = lstream->ptrold[i];
	}
	lstream->ptrold[lstream->ptrndx] = (entry_ptr-lf->linebuf);	/* Push current value */

	return(TRUE);
}

/* ============================================================================
-- Subroutine to translate (expand) a string containing references of form
-- %xxx% which convert to strings.
--
-- Usage: trnslt(char *tokout, char *tokin, INT toklen, LEXFILE *mylf)
--
-- Inputs: tokin  - input token character*(*)
--         toklen - maximum size of tokout
--         mylf   - pointer to use to stream info - if NULL, uses global lf
--
-- Output: tokout - translated token
--
-- Options: \% is translated directly as % even if variable exists
--          \$ is translated directly as $ even if variable exists
--          $<NAME> is translated to environment variable if NAME is
--          is uppercased.
============================================================================ */
PRIVATE void trnslt(char *tokout, char *tokin, INT toklen, LEXFILE *mylf) {

	char		*inptr, *outptr, *aptr, *cptr;
	char		 varname[2*VARNAME_STR_SIZE];
	char		 tmpbuf[LONG_STR_SIZE];							/* Lots of space */
	char		 lastchr = 0x7F;
	int       length;
	BOOL		 missed_string = FALSE;

/* First, replace %i and %f, etc. with current values if in appropriate mode */
	if (mylf == NULL) mylf = lf;
	while (mylf->filetype == MEMORY && mylf->mem_mode == MEM_NONE) mylf = mylf->backlink;
	if (mylf->filetype == MEMORY) {
		SubSpecial(tmpbuf, sizeof(tmpbuf), tokin, mylf);
		tokin = tmpbuf;
	}

/* Now, check for converting variable and environment references */
	outptr = tokout; inptr = tokin;
	length = toklen;
	while ((*inptr != '\0') && (length > 1)) {
		if (*inptr == TOKSAVEMARKER) {					/* Saved token */
			inptr++; aptr = lstream->tok_sav;
			while ((*aptr != '\0') && (length > 1)) {
				*(outptr++) = *(aptr++); length--;
			}
			continue;
		} else if (*inptr == '%') {						/* Escaped character?			*/
			if (lastchr == '\\') {							/* Was it a \% character?		*/
				outptr--; length++;
			} else if (missed_string) {					/* Can we tolerate unmatched? */
				missed_string = FALSE;
			} else if ((cptr=strchr(inptr+1,'%')) != NULL) {	/* Matching %%?		*/
				if (cptr != inptr+1) strncpy(varname, inptr+1, cptr-inptr-1);
				varname[cptr-inptr-1] = '\0';
				if ( (aptr = GVFindString(varname)) == NULL) {
					missed_string = (strchr(varname,' ') == NULL);	/* Did we fail for real? */
				} else {
					while (*aptr != '\0' && length > 1) {
						*outptr++ = *aptr++;					/* Copy over */
						length--;
					}
					inptr = cptr+1;							/* Done with characters */
					continue;
				}
			}
		} else if (*inptr == '$') {						/* Environment character? */
			if (lastchr == '\\') {
				outptr--; length++;
			} else {
				cptr = inptr+1;
				while (isupper(*cptr)) cptr++;			/* cptr gets last non-upcase */
				if (cptr != inptr+1) {
					strncpy(varname, inptr+1, cptr-inptr-1);
					varname[cptr-inptr-1] = '\0';
					if ( (aptr = getenv(varname)) != NULL) {
						while (*aptr != '\0' && length > 1) {
							*outptr++ = *aptr++;
							length--;
						}
						inptr = cptr;
						continue;
					}
				}
			}
		}
		lastchr = *(outptr++) = *(inptr++);		/* Copy character over */
		length--;
	}
	*outptr = '\0';
	return;
}


/* ============================================================================
--     Routine to skip commands until find label TOKEN
--
--     Usage: LexGotoLabel(char *label);
--
--     Inputs: label - label to be searched for.  Will stop on ':' // token
--                     or end of current file.  Will also search TERMI input
--                     string, but no prompts so be warned.
============================================================================ */
BOOL	LexGotoLabel(const char *label) {

	int	id, blank_lines;
	FILETYPE type;
	char	token[DFLT_STR_SIZE], btmp[MAX_LINELEN];
	BOOL	match;

LexGotoBegin:

/* Rewind those input types which are possible */
	type = lf->filetype;								/* Type of stream			*/
	id   = lf->id;										/* To know when changes */
	switch (type) {
		case SIMPLEFILE:
			fseek(lf->funit, 0L, SEEK_SET);
			break;
		case MEMORY:
			lf->memptr = lf->memory;
			break;
		case CONSOLE:									/* Can't do it, but add for lint */
		case PIPE:
			break;
	}

/* ---------------------------------------------------------------------------
-- Scan for the GOTO label.  We set In_GOTO_Request so that LexPromptStrNT
-- will not read the string off preceeding stream if end of current stream
-- occurs.  Thus we always end up with a relatively clean process.
--
-- If match, or blank line when in CONSOLE mode, we are successful.
-- Otherwise, if memory type was active, we continue to scan pop-ed feature.
-- On failure, just dump rest of string back into the buffer.
--------------------------------------------------------------------------- */
	blank_lines = 0;
	while (TRUE) {
		In_GOTO_Request = TRUE;
		LexPromptStrNT(btmp, sizeof(btmp), "GOTO$: ");	/* Read last file */
		In_GOTO_Request = FALSE;

/* If anything on line, compare to given string; if not, check if console */
		if (LexParseLine(token, sizeof(token), btmp, NULL)) {
			match = (*token == ':') && (stricmp(&token[1],label) == 0);
		} else {
			match = (lf->filetype==CONSOLE) && (blank_lines++>5);
		}
		if (match) {									/* We have found request	*/
			*lf->ptr = '\0';							/* Erase rest of this line	*/
			lstream->ptrndx = -1;					/* Erase backups				*/
			return(TRUE);
		}

/* If we've changed streams, quit unless in memory, in which case start again */
		if (lf->id != id) {							/* Possibly recycle			*/
			if (type == MEMORY) goto LexGotoBegin;
			break;
		}
	}

/* Here if we fail -- so erase everything and return FALSE */
	lf->ptr = lf->linebuf;								/* Reset the pointer	*/
	strscpy(lf->ptr, btmp, lf->bufsize);			/* Copy string over	*/
	lstream->ptrndx = -1;								/* Erase backups		*/
	return(FALSE);
}


/* ============================================================================
--     Routine to prompt & read a command line from the terminal.
--
--     Usage: void LexReadLine(const char *prompt)
--
--     Inputs: PRMPT - Character string to used for prompting the user.
============================================================================ */
void LexReadLine(const char *prmpt) {

	char mybuf[MAX_LINELEN], *aptr;

	do {
		LexPromptStrNT(mybuf, sizeof(mybuf), prmpt);	/* Read last file */
		if (lf->filetype == CONSOLE) break;				/* Pass on TTY input */
		aptr = mybuf;
		while (isspace(*aptr)) aptr++;
	} while (*aptr == ':');									/* Skip over label lines */

	strscpy(lf->linebuf, mybuf, lf->bufsize);			/* Copy appropriately */
	lf->ptr = lf->linebuf;									/* Reset the pointer	 */
	lstream->ptrndx = -1;									/* No backups yet		 */
	return;
}

/* ===========================================================================
-- Routine to print a prompt string and read a response string
--
-- Usage: void LexPromptStr(token, toklen, prmpt)      -- Includes translation
--        void LexPromptStrNT(token, toklen, prmpt)    -- No translation
--        BOOL LexPromptLine(token, toklen, prmpt, translate);
--
-- Inputs: (char *) token  - buffer to hold read
--         (int)    toklen - size of token
--         (char *) prmpt  - prompt to use at command line (NULL -> no prompt/echo)
--         (BOOL)   translate - is translation to be done?
--
-- Output: *token - filled with incoming string
--
-- Returns: FALSE if read failed, TRUE otherwise
--
-- All are very similar:
--    (1) LexPromptStr will read next line and translate %var% as necessary
--    (2) LexPromptStrNT reads next line without translating %var%
--    (3) LexPromptLine does either, but does not
--        (a) follow any of the for %f or repeat processing rules
--        (b) force CONSOLE to succeed.  ^Z on console will return FALSE
--            allowing indicating of an end of read.  See GptRead() usage.
--
-- Future plans: The section of 3 if's which actually read the stream should
--               be stripped out into a new routine.  Then slight modification
--               would allow this to read an arbitrary long string by 
--               modifying the size as we go.
============================================================================ */
#define	DO_PACK			0x01					/* Try PackString on fly			*/
#define	DO_NOFAIL		0x02					/* Don't allow console to fail	*/
#define	DO_TRANSLATE	0x04					/* Translate before return			*/

PRIVATE BOOL ReadStream(char *token, INT toklen, const char *prmpt, int flags);

void LexPromptStr(char *token, INT toklen, const char *prmpt) {
	ReadStream(token, toklen, prmpt, DO_PACK | DO_NOFAIL | DO_TRANSLATE);
	return;
}

/* ========================================================================= */
void LexPromptStrNT(char *token, INT toklen, const char *prmpt) {
	ReadStream(token, toklen, prmpt, DO_PACK | DO_NOFAIL);
	return;
}

/* ========================================================================= */
BOOL LexPromptLine(char *token, INT toklen, const char *prmpt, BOOL translate) {
	return ReadStream(token, toklen, prmpt, (translate) ? DO_TRANSLATE : 0);
}

/* ========================================================================= */
PRIVATE BOOL ReadStream(char *token, INT toklen, const char *prmpt, int flags) {

	int	ErrCount=20;								/* Error count on terminal		*/
	int	length, icnt,jcnt, ierr;
	char	*aptr, *str;
	char  mybuf[MAX_LINELEN];

	BOOL	rcode, do_echo, do_rewind;

	rcode  = TRUE;										/* Assume we will succeed		*/
	str    = token;									/* Where to store input text	*/
	*str   = '\0';										/* Minimum, mark as no input	*/
	length = toklen;									/* And length available			*/

/* On NOFAIL, clear SawEscape flag; on DO_PACK, check for that mode */
	if (flags & DO_NOFAIL) SawEscape=FALSE;
	if ((flags & DO_PACK) && ObsoletePackString(str, length)) goto CheckTranslate;

/* Read a line from active level.  Return as the string to length toklen */
StartLoop:
	do_echo = (lf->echo != 0) &&					/* Echo requested					*/
				 lf->filetype!=CONSOLE ;			/* Not coming from console		*/

	while (length > 0) {								/* As long as space to store	*/

		if (lf->filetype == CONSOLE) {
			if (flags & DO_NOFAIL) {				/* Escape processing?			*/
				sv_undo();								/* HCOPY patch in (to mark)	*/
				while (! (rcode=UserInput(prmpt, str, length)) ) {
					ERRputs("-- Console input error --\n");
					if (! ErrCount--) exit(EXIT_FAILURE);
				}
				if (*str == 0x1B) {					/* Is it an <ESC> character	*/
					SawEscape = TRUE;
					*str = ' ';
					break;
				}
			} else {
				rcode = UserInput(prmpt, str, length);
			}
			if (prmpt != NULL) prmpt = "-> ";	/* For subsequent lines			*/

		} else if (lf->filetype == MEMORY) {
			aptr = strchr(lf->memptr, '\n');
			if (aptr == NULL && *lf->memptr == '\0') {
				do_rewind = FALSE;
				lf->mem_count++;
				if (lf->mem_mode == MEM_LOOP) {
					lf->mem_ival += lf->mem_inc;
					sprintf(lf->loop_value, ForFormat, lf->mem_ival);	/* Goes 000 002 004 ... */
				}
				if (! In_GOTO_Request) switch (lf->mem_mode) {
					case MEM_LOOP:
					case MEM_COUNT:
						do_rewind = (lf->mem_count < lf->mem_maxcount);
						break;
					case MEM_CONDITION:
						SubSpecial(mybuf, sizeof(mybuf), lf->mem_condition, lf);
						do_rewind = (GVEvalExpr(mybuf, &ierr) > 0.0f && ierr == 0);
						break;
					case MEM_LIST:
						lf->mem_listptr += strlen(lf->mem_listptr)+1;
						do_rewind = *lf->mem_listptr != '\0';
						break;
					case MEM_NONE:
						break;
				}
				if (do_rewind) {
					lf->memptr = lf->memory;
				} else {
					PopLexFile();									/* Close/pop file		*/
					if (! In_GOTO_Request && str==token && *str=='\0')
						LexGetRest(str, length);				/* Get remainder		*/
				}
				if (str!=token || *str!='\0') break;		/* Done if anything	*/
				if (In_GOTO_Request) return(FALSE);			/* Done if a GOTO		*/
				goto StartLoop;
			} 
			if (aptr == NULL) {
				strscpy(str, lf->memptr, length);
				lf->memptr += strlen(lf->memptr);
			} else {
				icnt = (int) (aptr-lf->memptr);	/* Number of characters */
				jcnt = min(icnt, length-1);		/* Number we can move	*/
				memcpy(str, lf->memptr, jcnt);
				str[jcnt] = '\0';
				lf->memptr += icnt+1;				/* Skip over the '\n' */
			}
		} else {														/* From a macro file */
			if (fgets(str, length, lf->funit) == NULL) {
				if (do_echo) {
					ScrSetAttrib(D_FILEIN);
					TTYputsnl("INFO: Macro completed normally");
					ScrSetAttrib(D_NORMAL);
				}
				PopLexFile();										/* Close/pop file		*/
				if ((str!=token) || (*str!='\0')) break;	/* Done if anything	*/
				if (In_GOTO_Request) return(FALSE);			/* Done if GOTO		*/
				LexGetRest(str, length);						/* Get previous		*/
				if (*str == '\0') goto StartLoop;			/* Keep trying			*/
			} 
			if ( (aptr = strchr(str, LOGICAL_EOF)) != NULL) {	/* Logical end? */
				*aptr = '\0';
				fseek(lf->funit, 0L, SEEK_END);
			}
		}

		if (*str == '\0') break;					/* No text at this point ok	*/
		aptr = str+strlen(str)-1;					/* Otherwise check last chars	*/
		if (*aptr == '\n') *aptr--='\0';			/* Strip <nl> from end of str	*/
		while (aptr != str && isspace(*aptr)) aptr--;
		if (aptr == str) break;						/* Check for <sp>\ as continue */
		if (! isspace(*(aptr-1)) || *aptr!='\\') break;
		*aptr = '\0';									/* It is a logical continue	*/
		length -= (int) strlen(str);				/* Set as end of line and		*/
		str = aptr;										/* decrement space left			*/
	}
		
	if (do_echo && prmpt != NULL) {				/* Be aware that lf may have	*/
		ScrSetAttrib(D_PROMPT);						/* been changed by PopLexFile	*/
		TTYputs(prmpt);								/* so do_echo is set before	*/
		ScrSetAttrib(D_FILEIN);						/* we start reading				*/
		TTYputsnl(token);
		ScrSetAttrib(D_NORMAL);
	}

CheckTranslate:
	if (rcode && (flags & DO_TRANSLATE)) {	/* One more routine */
		trnslt(mybuf, token, sizeof(mybuf), NULL);
		strscpy(token, mybuf, toklen);
	}
	
	return(rcode);
}


/* ============================================================================
--     Routine to return the active unit presently returning input.
--     This file may be used, with some care, by other routines.
--
--     Usage: FILE *LexQueryActiveInput(char *namebuf)
--
--     Inputs: none
--
--     Output: If namebuf not NULL, name of currently active file.  Must
--             be large enough to take full name (PATH_MAX)
--
--     Returns: FILE * pointer to current file being read for input via
--              Lex routines.  Will be stdin at terminal level.
-- ============================================================================ */
FILE *LexQueryActiveInput(char *namebuf) {

	if (namebuf != NULL) strcpy(namebuf, lf->pname);
	return( (lf->filetype != MEMORY) ? lf->funit : NULL);
}


/* ============================================================================
--     Routine to return the active ID presently returning input.
--     This ID is 0 if at lowest console level, otherwise linear w/ macro level
--
--     Usage: FILE *LexQueryActiveInputID(char *namebuf)
--
--     Inputs: namebuf - pointer to save name of stream (if not NULL)
--
--     Output: If namebuf not NULL, name of currently active file.  Must
--             be large enough to take full name (PATH_MAX)
--
--     Returns: ID - id of the stream.  0 is console level and increases with
--                   each level of macro.
-- ============================================================================ */
INT LexQueryActiveInputID(char *namebuf) {

	if (namebuf != NULL) strcpy(namebuf, lf->pname);
	return(lf->id);
}


/* ============================================================================
--     Routine to open a file or pipe for input as a macro file.
--     All subsequent commands will be taken from this stream until
--     EOF is reached or command is explicitly terminated by another call.
--
--     Usage: BOOL LexExecFile(FILENAME)
--
--     Inputs: FILENAME - file to be opened for reading from
--
--     Note:
--       (1) NULL or BLANK filename will turn off active file
--       (2) Leading <> or [] on filename indicates use of same directory
--           as the current macro operating (last file based block)
-- ============================================================================ */
BOOL LexExecFile(char *name) {

	BOOL UseSameDir = FALSE;								/* Execute from last directory? */

/* ... Strip leading spaces from the name */
/* ... Determine if this is to be executed from same directory */
	while (isspace(*name)) name++;
	if (strncmp(name, "<>/",  3) == 0 || strncmp(name, "[]/",  3) == 0 ||
		 strncmp(name, "<>\\", 3) == 0 || strncmp(name, "[]\\", 3) == 0) {
		UseSameDir = TRUE;
		name += 3;
		while (isspace(*name)) name++;
	}
	
/* ... Consider this a complete shutdown and abort of commands */
	if (*name=='\0' || LexEqual(name, "-tty", 4)) {	/* Special turn off */
		while (lf->id != 0) PopLexFile();
		lf->ptr = lf->linebuf;
		*lf->ptr = '\0';

/* Pop back out of a macro file level -- drop all memory blocks plus calling file */
	} else if (LexEqual(name, "-return", 4)) {		/* Return from this file */
		while (lf->filetype == MEMORY) PopLexFile();
		PopLexFile();											/* Pop one real file level */

/* Break out of a loop control level (drop memory blocks to previous non-NONE) */
	} else if (stricmp(name, "-break")==0 || stricmp(name, "-continue")==0) {
		LEXFILE *ltmp;
		int id;
		ltmp = lf;
		/* First, drop back out of all memory blocks which are not loop control */
		while (ltmp->filetype == MEMORY && ltmp->mem_mode == MEM_NONE) {
			ltmp = ltmp->backlink;
		}
		/* Verify that we are in a loop control block - MEMORY but not MEM_NONE */
		if (ltmp->filetype != MEMORY || ltmp->filetype == MEM_NONE) return(FALSE);
		id = ltmp->id;
		while (lf->id != id) PopLexFile();
		lf->ptr = lf->linebuf;								/* Flush everything	 */
		*lf->ptr = '\0';
		/* On break, just pop this level.  Otherwise, skip all pending lines */
		if (stricmp(name,"-break") == 0) {				/* Either close file	 */
			PopLexFile();
		} else {													/* Or flush remainder */
			while (*lf->memptr != '\0') lf->memptr += strlen(lf->memptr)+1;
		}

/* ... Normal file request */
	} else {
		int ierr;
		char path[PATH_MAX];

		if (! UseSameDir || stricmp(MacroPath, "<stdin>") == 0) {
			strcpy(path, name);
		} else {
			SysSplitPath(MacroPath, path, NULL, NULL);
			SysMakePath(path, path, name, NULL);			/* Create full pathname */
		}

		if ( (ierr=PushLexFile(path)) != 0 ) {
			if (ierr == 1) {
				ERRprintf("ERROR: Macro file %s cannot be accessed\n", path);
			} else if (ierr == 2) {
				ERRprintf("ERROR: Macro file %s failed to open\n", path);
			} else if (ierr == 3) {
				ERRputs("ERROR: Too many nested macro files (really shouldn't happen).\n");
			}
			return(FALSE);
		}
		lf->backlink->can_give_args = TRUE;				/* Prev level can give args */
	}
	return(TRUE);
}

/* ----------------------------------------------------------------------------
---------------------------------------------------------------------------- */
PRIVATE void MakeRoom(LEXFILE *ltmp, int count) {

	ptrdiff_t used;
	int left;

	used = ltmp->ptr - ltmp->linebuf;
	left = (int) strlen(ltmp->ptr);
	if (used+left+count+1 > ltmp->bufsize) {
		while (used+left+count+1 > ltmp->bufsize) ltmp->bufsize += MAX_LINELEN;
		ltmp->linebuf = realloc(ltmp->linebuf, ltmp->bufsize);
		ltmp->ptr = ltmp->linebuf+used;
	}
	return;
}

/* ---------------------------------------------------------------------------
-- This routine creates a LEXFILE structure and initializes all parameters
-- to normal default values.  
--
-- If called with a NULL "last", it will be correct as first in a stream.
--------------------------------------------------------------------------- */
PRIVATE LEXFILE *CreateLexFile(LEXFILE *last) {
	LEXFILE *ltmp;
	int i;

/* Allocate space */
	if ( (ltmp = (LEXFILE *) malloc(sizeof(LEXFILE))) == NULL) return(NULL);

/* Set all parameters as if this is the top level file */
	ltmp->id       = 0;									/* Assume first in chain	*/
	ltmp->backlink = last;								/* Backward link				*/
	ltmp->filetype = CONSOLE;							/* And make type console	*/
	ltmp->funit    = stdin;								/* Default is console		*/
	ltmp->memory   = NULL;								/* Definitely not memory	*/
	ltmp->memptr   = NULL;								/* So this does not matter	*/
	strcpy(ltmp->fname, "<stdin>");					/* Set name as memory		*/
	strcpy(ltmp->pname, "<stdin>");

	ltmp->echo				= DO_ECHO;					/* Is it live, or memorex?	*/
	ltmp->can_give_args	= FALSE;						/* Not in mode to give args */

	ltmp->bufsize		= BUF_SIZE;						/* Choose a buffer size		*/
	ltmp->linebuf		= malloc(ltmp->bufsize);	/* And create it				*/
	ltmp->ptr			= ltmp->linebuf;				/* Start at beginning		*/
	*ltmp->ptr			= '\0';							/* With nothing				*/

	ltmp->fmmode		= FM_NONE;
	ltmp->fmrepeat		= 0;
	ltmp->fmchar      = 'f';							/* Default is %f				*/
	ltmp->fmcommand	= NULL;
	ltmp->fmvarlist	= NULL;
	ltmp->fmvarptr		= NULL;
	ltmp->fmnamestart	= NULL;
	ltmp->fmdirp		= NULL;
	ltmp->fmcnt			= 0;

/* By default, clear all of the substituted vars */	
	for (i=0; i<26; i++) ltmp->sub_vars[i] = NULL;

/* If last was passed, then link in necessary inheritances */
/* Also, if not, clear all of the temporary vars section */
	if (last != NULL) {									/* Correct if non-NULL		*/
		ltmp->id       = last->id+1;					/* Give unique ID now		*/
		ltmp->echo     = last->echo;					/* Default echo as before	*/
	}

	return(ltmp);
}

/* ---------------------------------------------------------------------------
-- Routine to close file, and destroy allocated memory in a LEXFILE
--------------------------------------------------------------------------- */
PRIVATE void DestroyLexFile(LEXFILE *ltmp) {	
	
	LexStreamClosed = TRUE;							/* If anyone interested */

	if (ltmp == NULL) return;						/* Don't bother further */

	switch (ltmp->filetype) {						/* Close input stream */
		case CONSOLE:									/* Nothing to do */
			break;
		case SIMPLEFILE:
			fclose(ltmp->funit);
			break;
		case PIPE:
			pclose(ltmp->funit);
			break;
		case MEMORY:
			free(ltmp->memory);
			free(ltmp->mem_condition);
			break;
	}

	free(ltmp->linebuf);								/* Free buffer				*/
	free(ltmp->fmcommand);							/* Free memory usage		*/
	free(ltmp->fmvarlist);							/* Free memory usage		*/
	if (ltmp->fmdirp != NULL) closedir(ltmp->fmdirp);

	free(ltmp);											/* Free itself				*/
	return;
}


/* ----------------------------------------------------------------------------
-- Pops one file level off the processing stack.  Will not terminate the base
-- process (id = 0), but otherwise just undoes one level.
--
-- Usage: static void PopLexFile(void)
---------------------------------------------------------------------------- */
PRIVATE void PopLexFile(void) {

	LEXSTREAM *ls;
	LEXFILE *ltmp, *back;
	char *aptr;
	int  new_cnt, old_cnt;

	ls = lstream;													/* Default to current */
	ltmp = ls->lf;													/* And appropriate lf */

	if (ltmp != NULL && ltmp->id != 0) {
		back = ltmp->backlink;									/* Get previous struct */
		back->can_give_args = FALSE;							/* No longer can give args */
		
/* ... If not dump, insert remainder of line at start of previous buffer */
		if (*ltmp->ptr != '\0') {								/* Preserve line?		*/
			while (TRUE) {											/* Skip comments		*/
				while (isspace(*ltmp->ptr)) ltmp->ptr++;
				if (strncmp(ltmp->ptr, "//", 2) == 0) {
					*ltmp->ptr = '\0'; break;
				} else if (strncmp(ltmp->ptr, "/*", 2) != 0) {
					break;
				} else if ( (aptr = strstr(ltmp->ptr+2, "*/")) != NULL) {
					ltmp->ptr = aptr+2;
				} else {
					*ltmp->ptr = '\0'; break;					/* End the string */
				}
			}
			if (back != NULL && *ltmp->ptr != '\0') {		/* Only if place, valid */
				new_cnt = (int) strlen(ltmp->ptr);
				old_cnt = (int) strlen(back->ptr);
				MakeRoom(back, new_cnt);
				memmove(back->ptr+new_cnt+1, back->ptr, old_cnt+1);
				memcpy (back->ptr,           ltmp->ptr, new_cnt);
				back->ptr[new_cnt] = ' ';
			}
		}

		DestroyLexFile(ltmp);							/* Free memory and dump */

		ls->lf = back;										/* Mark in stream			*/
		ls->ptrndx = -1;									/* Can't backup anymore */
		if (ls == lstream) {								/* and maybe in MAIN		*/
			lf = back;						
			if (lf->filetype != MEMORY) {
				strcpy(MacroName, lf->fname);
				strcpy(MacroPath, lf->pname);
#if (defined OS2 || defined NT)
				while ( (aptr=strchr(MacroName, '\\')) != NULL) *aptr = '/';
				while ( (aptr=strchr(MacroPath, '\\')) != NULL) *aptr = '/';
#endif
			}
		}
	}
	return;
}


/* ----------------------------------------------------------------------------
-- Routine to open and begin reading from a new file
--
-- Usage: int PushLexFile(char *name);
--
-- Inputs: name - filename to be opened
--
-- Output: Open the requested stream and begins taking commands from it
--
-- Return: 0 --> all okay
--         1 --> File could not be found
--         2 --> File found but could not be opened
--         3 --> Too many levels of macros (some maximum)
--
---------------------------------------------------------------------------- */
PRIVATE int PushLexFile(char *name) {

	LEXSTREAM *ls;
	LEXFILE *ltmp;
	char path[PATH_MAX], namebuf[PATH_MAX];	/* namebuf for modified name */
	int  rc;
	char *aptr;

	ls = lstream;										/* Default value */

/* Create a new file structure */
	if ( (ltmp = CreateLexFile(ls->lf)) == NULL) return(3);

/* Create the necessary "path" by looking up the name in various ways */
/* Accept gzip'd files and relabel them as using pipe mode */
	if (*name == '|') {								/* Is a pipe request */
		rc = 0;
		strcpy(path, name);
	} else if (SysFindFileGz(path, name, LexMacroSearchPath, LexMacroExtList, R_OK)) {
		if (strlen(path) > 3 && stricmp(path+strlen(path)-3, ".gz") == 0) {
			strcat(strcpy(namebuf, "| gzip -dc "), path);
			name = namebuf;
			strcpy(path, namebuf);
		}
		rc = 0;
	} else if (strstr(name, "::") != NULL) {
		strscpy(path, name, sizeof(path));		/* Work with local copy */
		if ( (aptr = strstr(path, "::")) == NULL) {
			rc = 1;
		} else {
			*aptr = '\0';
			sprintf(namebuf, "| unzip -p %s %s", path, aptr+2);
			name = namebuf;
			strcpy(path, namebuf);
			rc = 0;
		}
	} else {
		rc = 1;
	}
	if (rc != 0) goto errors;

/* Either handle or regular file -- both name and path are filled in now */
	if (*name != '|') {								/* Not requesting a pipe? */
		if (access(path, R_OK) != 0) {
			rc = 1;
		} else if ( (ltmp->funit=fopen(path,"r")) == NULL) {
			rc = 2;
		} else {
			fprivate(ltmp->funit);					/* Make it private file		*/
			ltmp->filetype = SIMPLEFILE;
			strscpy(ltmp->fname, name, sizeof(ltmp->fname));
			SysQualifyPath(ltmp->pname, path, sizeof(ltmp->pname));
		}

/* ... or as a pipe */
	} else {												/* Requesting a PIPE			*/
		name++;											/* Skip over the | symbol	*/
		while (isspace(*name)) name++;			/* Skip leading spaces		*/
		if (*name == '\0') {							/* Must be something left	*/
			rc = 1;
		} else if ( (ltmp->funit=popen(name, "r")) == NULL) {
			rc = 2;
		} else {
			fprivate(ltmp->funit);					/* Make it private file		*/
			ltmp->filetype = PIPE;
			strscpy(ltmp->fname, name, sizeof(ltmp->fname));
			strscpy(ltmp->pname, path, sizeof(ltmp->pname));
		}
	}
	if (rc != 0) goto errors;						/* Free space and exit */

/* Mark the necessary positions in the LEXSTREAM */
	ls->ptrndx = -1;									/* Can't backup anymore		*/
	ls->lf = ltmp;										/* Exchange the control		*/

	if (ls == lstream) {								/* Do we have the primary? */
		lf = ltmp;
		strcpy(MacroName, lf->fname);
		strcpy(MacroPath, lf->pname);
#if (defined OS2 || defined NT)					/* Change backslash to forward */
		while ( (aptr=strchr(MacroName, '\\')) != NULL) *aptr = '/';
		while ( (aptr=strchr(MacroPath, '\\')) != NULL) *aptr = '/';
#endif
	}
	return(0);

errors:
	DestroyLexFile(ltmp);
	return(rc);
}

/* ============================================================================
--     Usage:  int LexSetDebug(int mode)
--
--     Inputs: mode - 0 => Disable all debug modes
--                    1 => Turn on debug mode
--
--     Output: Previous state
============================================================================ */
INT LexSetDebug(INT flag) {
	
	int temp_flag;

	temp_flag = (lf->echo & DO_DEBUG) ? 1 : 0 ;
	if (flag == 0) {
		lf->echo &= ~DO_DEBUG;						/* Remove debug bits		*/
	} else {
		lf->echo |=  DO_DEBUG;
	}
	return(temp_flag);								/* Return old state		*/
}

/* ============================================================================
--     Usage:  BOOL LexSetNoEcho(BOOL flag)
--
--     Inputs: flag - .TRUE.  -> No echo of command file input
--                    .FALSE. -> Echo all text (normal mode)
--
--     Output: Previous state
============================================================================ */
BOOL LexSetNoEcho(BOOL flag) {
	
	LEXFILE *mylf;
	int temp_flag;

	temp_flag = ! (lf->echo & DO_ECHO);				/* Save current flag		*/
	if (flag) {												/* This says don't echo */
		for (mylf=lf; mylf!=NULL; mylf=mylf->backlink) mylf->echo &= ~DO_ECHO;
	} else {													/* This says echo			*/
		for (mylf=lf; mylf!=NULL; mylf=mylf->backlink) mylf->echo |=  DO_ECHO;
	}
	return(temp_flag);									/* Return old state		*/
}

/* ============================================================================
--     Usage:  BOOL LexSetLocalNoEcho(BOOL flag)
--
--     Inputs: flag - .TRUE.  -> No echo of command file input
--                    .FALSE. -> Echo all text (normal mode)
--
--     Output: Previous state
============================================================================ */
BOOL LexSetLocalNoEcho(BOOL flag) {
	
	BOOL temp_flag;
	temp_flag = ! (lf->echo & DO_ECHO);				/* Save current flag		*/
	if (flag) {												/* This says don't echo */
		lf->echo &= ~DO_ECHO;
	} else {													/* This says echo			*/
		lf->echo |=  DO_ECHO;
	}
	return(temp_flag);									/* Return old state		*/
}


/* ============================================================================
--     Routine to place a user text line into the input buffer
--
--     Usage: void LexInsText(char *text)
--
--     Inputs: text - Text to be placed in buffer (CHARACTER)
--                    Lines greater than buffer will be truncated.
--
============================================================================ */
void LexInsText(const char *text) {

	int lentext;

	if ( (lentext = (int) strlen(text)) == 0) return;			/* Length of text	 */
	MakeRoom(lf, lentext);

	memmove(lf->ptr+lentext+1, lf->ptr, strlen(lf->ptr)+1);	/* Shift text up   */
	memcpy(lf->ptr, text, lentext);
	lf->ptr[lentext] = ' ';
	return;
}

/* ============================================================================
--     Routine to reset the pointer to the end of line, NO more data.
--     Same as erasing the input line - BACKUP has no effect after this call.
--
--     Usage: LexClrPtr();
============================================================================ */
void LexClrPtr(void) {

	lf->ptr = lf->linebuf;
	*lf->ptr = '\0';
	lstream->ptrndx = -1;
	return;
}

/* ============================================================================
--     Routine to clear out the remainder of a command line, typically after
--     an error processing one token.  Any remaining text on a command line is
--     deleted and the user is warned of such deletions.  If any command files
--     are open, the user is requested at each level if the file commands are
--     to be continued, or the command file terminated.
--     BACKUP has no effect after this call.
--
--     Usage: LexFlush()
--            LexFlushEx(mode)
--
--     Inputs: mode - FLUSH_QUERY (0) - default behavior.  Ask what to do.
--                    FLUSH_YES   (1) - respond yes to all flush queries
--                    FLUSH_NO    (2) - respond no  to all flush queries
--
--     Notes: LexFlush is equivalent to LexFlushEx(LexFlushMode)
--               LexFlushMode may be changed in several ways.
============================================================================ */
void LexFlush(void) {
	LexFlushEx(LexFlushMode);
	return;
}

void LexFlushEx(LEXFLUSHMODE mode) {
	
	char token[PATH_MAX+30];

	if (*lf->ptr != '\0') ERRprintf("ERROR: Command line flushed (%s)\n", lf->ptr);

	*lf->ptr = '\0';												/* Clear the puppy */
	lstream->ptrndx = -1;

/* If terminated by a ^C, read until we clear the ^C in the input stream */
	if (SysChkBreak(TRUE)) CONFlushCtrlC();

/* Now, see what should be dumped */
	while (lf->id != 0) {
		strcpy(token, lf->fname);								/* Copy the name */
		strcat(token, " active. Terminate (YES|no)? ");
		if (mode == FLUSH_NO) break;
		if (mode == FLUSH_QUERY && ! YesNo_U(token, TRUE)) break;
		PopLexFile();
	}
	lf->ptr = lf->linebuf;
	*lf->ptr = '\0';
	if (lf->fmmode != FM_NONE) {
		if (mode == FLUSH_YES || (mode == FLUSH_QUERY && YesNo_U("Abort repeat process (YES|No)? ", TRUE))) {
			free(lf->fmvarlist); lf->fmvarlist = NULL;
			free(lf->fmcommand); lf->fmcommand = NULL;
			if (lf->fmdirp != NULL) closedir(lf->fmdirp); 
			lf->fmdirp = NULL;
			lf->fmmode = FM_NONE;
		}
	}

	LexFlushCount++;									/* Increment number of flushes */
	return;
}


/* ============================================================================
--     Routine to back up one token.
--     No input or output is generated.  The internal pointer is moved back
--     to just before the last token read.  If not possible to back up, the
--     pointer is left as was.  Can be called a maximum of NOLDS times, or as
--     many as tokens exist on the line.
--
--     Usage: LexBackup()
============================================================================ */
void LexBackup(void) {

	if (lstream->ptrndx >= 0) {							/* Do nothing if <= 0 */
		lf->ptr = lf->linebuf + lstream->ptrold[lstream->ptrndx--];
	}
	return;
}

/* ============================================================================
-- Routine to return remainder of the command line as raw text
--
--     Usage: CALL LexGetRest(STR)      - Return rest of command line
--            CALL LexGetRestNT(STR)    - Return rest of command line no translate
--
--     Output: STR    - String where response is returned (CHARACTER)
--                      Will be left justified
============================================================================ */
void LexGetRest(char *str, INT toklen) {

	if (++lstream->ptrndx == NOLDS) {					/* Next backup marker */
		int i;
		for (i=--lstream->ptrndx; i; i--)
			lstream->ptrold[i-1] = lstream->ptrold[i];
	}

	while (isspace(*lf->ptr)) lf->ptr++;				/* Find next non-white	*/
	trnslt(str, lf->ptr, toklen, NULL);					/* Translate into space */
	lstream->ptrold[lstream->ptrndx] = (lf->ptr-lf->linebuf); /* Push current value	*/
	lf->ptr += strlen(lf->ptr);							/* Put ptr at EOF			*/
	return;
}

/* ------------------------------------------------------------------------- */
void LexGetRestNT(char *str, INT toklen) {

	if (++lstream->ptrndx == NOLDS) {					/* Next backup marker */
		int i;
		for (i=--lstream->ptrndx; i; i--)
			lstream->ptrold[i-1] = lstream->ptrold[i];
	}

	while (isspace(*lf->ptr)) lf->ptr++;				/* Find next non-white	*/
	strscpy(str, lf->ptr, toklen);						/* No translation			*/
	lstream->ptrold[lstream->ptrndx] = (lf->ptr-lf->linebuf); /* Push current value	*/
	lf->ptr += strlen(lf->ptr);							/* Put ptr at EOF			*/
	return;
}


/* ===========================================================================
-- Routine to copy the next block of statements { ... } into a string and
-- return a pointer to that block statement.  The \n are retained between
-- lines of the parse.  No translation is done on lines.
--
-- A block of commands is identified as (1) the remainder of the command
-- line if the next character (this line or next) is not a {, or all lines
-- from subsequent { to matching }
--
-- Usage: char *LexGetBlockStringNT(char *prompt);
--
-- Inputs: prompt - prompt on subsequent lines (if necessary)
--
-- Output: none
--
-- Returns: NULL - no command found (file closed before any actual text)
=========================================================================== */
CHAR *LexGetBlockStringNT(const char *prmpt) {

	char mybuf[MAX_LINELEN];
	char *block=NULL;
	int blocksize, blockcnt;
	int numbraces, lastchr, achr;

	if (prmpt==NULL && lf->id==0) prmpt = ": ";
	
/* First, scan and get next character. Nothing on line reads and tries again. */
/* However, if the current macro closes, we assume the statement is blank		*/
restart:
	lstream->ptrndx = -1;						/* Backup is out of the question */
	while (isspace(*lf->ptr)) lf->ptr++;
	if (*lf->ptr == '\0') {						/* Nothing there */
		LexStreamClosed = FALSE;
		ReadStream(mybuf, sizeof(mybuf), prmpt, DO_NOFAIL);
		strscpy(lf->linebuf, mybuf, lf->bufsize);
		lf->ptr = lf->linebuf;
		if (LexStreamClosed) return(NULL);	/* Nothing if macro closes */
		goto restart;
	}

/* Okay, we have valid characters.  See if it is an isolated line { */
	if (*lf->ptr!='{' || (lf->ptr[1]!='\0' && !isspace(lf->ptr[1])) ) {
		block = strdup(lf->ptr);
		*lf->ptr = '\0';
		return(block);
	}

/* Okay, we are a block if.  Skip over it and scan to non-blank */
	lf->ptr++;
	while (isspace(*lf->ptr)) lf->ptr++;
	numbraces = 0;									/* Number of isolated {} pairs */
	blocksize = 0;
	blockcnt  = 0;
	lastchr   = '\n';

	while (TRUE) {									/* Now, just keep going */
		achr = *lf->ptr++;
		if (achr == '\0') {						/* End of string, need more */
			LexStreamClosed = FALSE;
			ReadStream(mybuf, sizeof(mybuf), prmpt, DO_NOFAIL);
			strscpy(lf->linebuf, mybuf, lf->bufsize);
			lf->ptr = lf->linebuf;
			if (LexStreamClosed) break;
			achr = '\n';							/* Insert EOS */
		} else if (achr == '{') {				/* a new open brace */
			if (isspace(lastchr) && (*lf->ptr=='\0' || isspace(*lf->ptr)) )
				numbraces++;
		} else if (achr == '}') {
			if (isspace(lastchr) && (*lf->ptr=='\0' || isspace(*lf->ptr)) )
				numbraces--;
		}
		if (numbraces < 0) break;
		if (blockcnt >= blocksize) {
			blocksize += 1024;
			block = realloc(block, blocksize);
		}
		block[blockcnt++] = lastchr = achr;
	}
	block[blockcnt] = '\0';
	return(block);
}



/* ===========================================================================
-- Routine to loop execute a block of commands stored as a memory string.
--
-- Usage: BOOL LexExecLoop(char *block, char subst, int start, int end, int increment);
--
-- Inputs: block - malloc'd block of memory with command statements to execute
--                 block will be free'd upon completion
--         subst - character which is used as the %f for substitution in strings.
--                 Default is %f, but can be any other character also
--         start - beginning value of %<subst>
--         end   - ending value of %<subst>
--         increment - increment value of %<subst>
--
-- Output: none
--
-- Returns: TRUE - command accepted.
--
-- This is almost identical to other loop command, but the %<subst> is
-- replaced with "ddd" where ddd is the decimal value of the string.  Always
-- at least 3 characters so can be used in filename extensions.
--
-- %c and %i continue to be given actual counter values.
=========================================================================== */
BOOL LexExecLoop(char *block, char subst, int start, int end, int inc) {

	LEXSTREAM *ls;
	LEXFILE *ltmp;
	int i,count;

/* Determine number of loops to do and see if anything at all */
	if (block == NULL) return(TRUE);
	if (! ((inc > 0 && end >= start) || (inc < 0 && end <= start)) ) {
		return(TRUE);
	}
	count = 1+(end-start)/inc;
	
/* Create a new file structure */
	ls = lstream;												/* Default value */
	if ( (ltmp = CreateLexFile(ls->lf)) == NULL) return(FALSE);

/* Copy over the string substitions */
	for (i=0; i<26; i++) ltmp->sub_vars[i] = ls->lf->sub_vars[i];

	ltmp->filetype = MEMORY;
	ltmp->funit    = NULL;
	ltmp->memory   = ltmp->memptr = block;

	strcpy(ltmp->fname, "Block statement");
	strcpy(ltmp->pname, "Block statement");

	ltmp->mem_mode      = MEM_LOOP;				/* Memory count mode */
	ltmp->mem_condition = NULL;
	ltmp->mem_listptr   = NULL;
	ltmp->mem_count     = 0;
	ltmp->mem_maxcount  = count;
	ltmp->mem_ival      = start;
	ltmp->mem_inc       = inc;

/* Handle the character substitution problem */
	subst = isalpha(subst) ? tolower(subst) : 'f' ;
	ltmp->fmchar = subst;
	ltmp->mem_listptr = ltmp->loop_value;
	ltmp->sub_vars[subst-'a'] = &ltmp->mem_listptr;
	sprintf(ltmp->loop_value, ForFormat, ltmp->mem_ival);	/* Goes 000 002 004 ... */
	
/* Make default off under most situations */
/*	if (ltmp->mem_mode != MEM_NONE) ltmp->echo = FALSE; */
	ltmp->echo = 0;									/* Default is silent */
	if (ls->lf != NULL) ltmp->echo = ls->lf->echo & ~DO_ECHO;

	ls->ptrndx = -1;									/* Can't backup anymore		*/
	ls->lf = ltmp;										/* Exchange the control		*/

	if (ls == lstream) lf = ltmp;					/* Do we have the primary? */
	return(TRUE);

}


/* ===========================================================================
-- Routine to execute a block of commands stored as a memory string.  The string
-- may well have multiple lines with separating \n -- from LexGetBlockStringNT
--
-- Usage: BOOL LexExecBlock(char *block, char subst, int count, char *condition, char *list);
--
-- Inputs: block - malloc'd block of memory with command statements to execute
--                 block will be free'd upon completion
--         subst - character which is used as the %f for substitution in strings.
--                 Default is %f, but can be any other character also
--         count - <0 if  disabled, otherwise number of times to execute.
--                 0 is valid indicating never execute
--                 -1 --> don't run with a count
--         condition - expression of while () format.  Will be free'd 
--                 when routine is complete.
--                 NULL --> don't run with a condition
--         list  - a list of \0 terminated strings with an extra \0 marking
--                 the final end.  This should be a malloc'd block which
--                 will be free'd by this routine.
--                 NULL --> don't run with a list
--
-- Output: none
--
-- Returns: TRUE - command accepted.
--
-- Note: Order is count, condition, list.  First one which is not disabled
--       will be used.  The memory strings, if used, will be deallocated.
=========================================================================== */
BOOL LexExecBlock(char *block, char subst, int count, char *condition, char *list) {

	LEXSTREAM *ls;
	LEXFILE *ltmp;
	char mybuf[DFLT_STR_SIZE];
	int i,ierr;

/* Make first checks to see if there is anything to do (at all) */
/* For conditions -- have to wait until we have var substition set up */
	if (block == NULL || count == 0 || (list!=NULL && *list=='\0') )
		return(TRUE);											

/* Create a new file structure */
	ls = lstream;												/* Default value */
	if ( (ltmp = CreateLexFile(ls->lf)) == NULL) return(FALSE);

/* Copy over the string substitions */
	for (i=0; i<26; i++) ltmp->sub_vars[i] = ls->lf->sub_vars[i];

	ltmp->filetype = MEMORY;
	ltmp->funit    = NULL;
	ltmp->memory   = ltmp->memptr = block;

	strcpy(ltmp->fname, "Block statement");
	strcpy(ltmp->pname, "Block statement");

	ltmp->mem_mode = MEM_NONE;					/* Memory mode not real		*/
	ltmp->mem_condition = NULL;
	ltmp->mem_listptr   = NULL;
	ltmp->mem_count     = 0;

	if (condition != NULL) {
		ltmp->mem_mode = MEM_CONDITION;
		ltmp->mem_condition = condition;
	} else if (count > 0) {
		ltmp->mem_mode = MEM_COUNT;
		ltmp->mem_maxcount = count;
	} else if (list != NULL) {
		ltmp->mem_mode = MEM_LIST;
		ltmp->mem_listptr = ltmp->mem_condition = list;
/*		for (aptr=list; *aptr!='\0'; aptr+=strlen(aptr)+1) printf("string: %s\n", aptr); */
	}

/* Handle the character substitution problem -- but only for MEM_LIST mode */
	if (ltmp->mem_mode == MEM_LIST) {
		subst = isalpha(subst) ? tolower(subst) : 'f' ;
		ltmp->fmchar = subst;
		ltmp->sub_vars[subst-'a'] = &ltmp->mem_listptr;
	}

/* Make default off under most situations */
/*	if (ltmp->mem_mode != MEM_NONE) ltmp->echo = FALSE; */
	ltmp->echo = 0;									/* Default is silent */
	if (ls->lf != NULL) ltmp->echo = ls->lf->echo & ~DO_ECHO;

/* Okay - now can test condition to see if it ever will execute.  If not,
-- have to clean up all the structures we created and return no change */
	if (ltmp->mem_mode == MEM_CONDITION) {
		SubSpecial(mybuf, sizeof(mybuf), ltmp->mem_condition, ltmp);
		if (GVEvalExpr(mybuf, &ierr) <= 0.0f || ierr != 0) {
			DestroyLexFile(ltmp);
			return(TRUE);
		}
	}

/* Okay, everything is okay to finish up */
	ls->ptrndx = -1;									/* Can't backup anymore		*/
	ls->lf = ltmp;										/* Exchange the control		*/

	if (ls == lstream) lf = ltmp;					/* Do we have the primary? */
	return(TRUE);

}

/* ============================================================================
-- This routine takes and input line and packs it into an output line
-- converting the characters %f %c %i as necessary to strings passed.
--
-- Usage: void SubSpecial(char *token, int len, char str, LEXFILE *mylf);
--
-- Inputs: token - pointer to string where result will be stored
--         len   - number of character available in token
--         str   - input pattern string
--         mylf  - contains info for replacing characters
--
-- Output: *token - filled with converted string
============================================================================ */
PRIVATE void SubSpecial(char *token, int len, char *str, LEXFILE *mylf) {
	
	char count[10],index[10], *tptr;
	int i;

	sprintf(index, ForFormat, mylf->mem_count);	 /* Goes 000 001 002 ... */
	sprintf(count, ForFormat, mylf->mem_count+1); /* Goes 001 002 003 ... */

	len--;													/* Space for the EOS	*/
	while (*str && len) {
		if (*str != '%') {								/* If not % sign, copy */
			*(token++) = *(str++); --len;
		} else { 
			str++;											/* Skip over the % sign */
			i = tolower(*str)-'a';						/* Check as an index */
			if (*str == '%') {							/* %% is replaced by % */
				*(token++) = *(str++); --len;
			} else if (i>=0 && i<26 && mylf->sub_vars[i] != NULL) { /* %f replaced by file	*/
				tptr = *mylf->sub_vars[i];
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else if (tolower(*str) == 'c') {		/* %c replaced by count */
				tptr = count;
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else if (tolower(*str) == 'i') {		/* %i replaced by index */
				tptr = index;
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else {
				*(token++) = '%'; --len;
			}
		}
	}
	*token = '\0';
	return;
}


/* ============================================================================
-- This routine takes and input line and packs it into an output line
-- converting the characters %f %c %i as necessary to strings passed.
--
-- Usage: void SubSpecialOld(char *token, int len, char str, char subst, int cnt, char *file);
--
-- Inputs: token - pointer to string where result will be stored
--         len   - number of character available in token
--         str   - input pattern string
--         cnt   - current count (0, 1, 2, ...)
--         file  - if not NULL, replacement for %f
--
-- Output: *token - filled with converted string
============================================================================ */
PRIVATE void SubSpecialOld(char *token, int len, char *str, char subst, int cnt, char *file) {
	
	char count[10],index[10], *tptr;

	sprintf(index, ForFormat, cnt);					/* Goes 000 001 002 ... */
	sprintf(count, ForFormat, cnt+1);				/* Goes 001 002 003 ... */

	len--;													/* Space for the EOS	*/
	while (*str && len) {
		if (*str != '%') {								/* If not % sign, copy */
			*(token++) = *(str++); --len;
		} else { 
			str++;											/* Skip over the % sign */
			if (*str == '%') {							/* %% is replaced by % */
				*(token++) = *(str++); --len;
			} else if (tolower(*str) == subst && file != NULL) {		/* %f replaced by file	*/
				tptr = file;
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else if (tolower(*str) == 'c') {		/* %c replaced by count */
				tptr = count;
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else if (tolower(*str) == 'i') {		/* %i replaced by index */
				tptr = index;
				while (*tptr && --len) *(token++) = *(tptr++);
				str++;
			} else {
				*(token++) = '%'; --len;
			}
		}
	}
	*token = '\0';
	return;
}


/* ============================================================================
--     Subroutine to pack a converted string into a user buffer in the file
--     mode (FMMODE).  The %f and %c characters are converted to the appropriat
--     filename and file count.
--
--     SUBROUTINE PCKSTR(STR)
--
--     Outputs: STR - Converted string
============================================================================ */
PRIVATE BOOL ObsoletePackString(char *token, int toklen) {

	char fname[PATH_MAX];
	char *aptr,*optr;
	struct dirent *entry;
	size_t pcnt;

	if (lf->fmmode == FM_NONE) return(FALSE);			/* Nothing to encode */

	while (TRUE) {
		if (lf->fmmode == FM_WILD) {						/* Working on wild list */
			if ( (entry = readdir(lf->fmdirp)) == NULL) {	/* Next entry */
				closedir(lf->fmdirp);
				lf->fmdirp = NULL;
				lf->fmmode = FM_LIST;						/* Return to list mode */
				continue;
			} else if (strcmp(entry->d_name, ".")  == 0 || strcmp(entry->d_name, "..") == 0
						  || (! SysCheckMatch(entry->d_name, lf->fmnamestart))) {
				continue;
			} else {
				pcnt = lf->fmnamestart-lf->fmvarlist;	/* Number of chars in root */
				strncpy(fname, lf->fmvarlist, pcnt);	/* Copy the chars */
				strcpy (fname+pcnt, entry->d_name);		/* And append the name */
				break;
			}

		} else if (lf->fmmode == FM_COUNT) {
			if (lf->fmcnt >= lf->fmrepeat) {				/* End of counting? */
				lf->fmmode = FM_NONE;
				free(lf->fmcommand);	lf->fmcommand = NULL;
				return(FALSE);
			}
			strcpy(fname, "repeat_mode");
			break;

		} else if (lf->fmmode == FM_LIST) {				/* Parsing mode */
			if (! LexParseLine(fname, sizeof(fname), lf->fmvarptr, &lf->fmvarptr)) {
				lf->fmmode = FM_NONE;
				free(lf->fmvarlist); lf->fmvarlist = NULL;
				free(lf->fmcommand); lf->fmcommand = NULL;
				return(FALSE);
			}
			SysMarkPath(fname, &aptr, &optr);			/* Find file and end of dir */
			if (strpbrk(aptr, "?*") == NULL) break;	/* Not wild, just use */
			strcpy(lf->fmvarlist, fname);					/* Copy it back for later */
			lf->fmnamestart = lf->fmvarlist + (aptr-fname);
			*optr = '\0';
			if (*fname == '\0') strcpy(fname, ".");
			if ( (lf->fmdirp = opendir(fname)) == NULL) {
				ERRprintf("ERROR: * or ?? specified with invalid directory (%s)\n", fname);
			} else {
				lf->fmmode = FM_WILD;
			}
			continue;
		} 
	}

/* --- Okay - put the string back into the command buffer --- */
	SubSpecialOld(token, toklen,  lf->fmcommand, lf->fmchar, lf->fmcnt++, fname);
	return(TRUE);
}


/* ============================================================================
--     Subroutine to start a %FOR type command reprocessing level
--
--     Usage: CALL LexFMMake(char *wildlist, char subst, char *command)
--            CALL LexFMCount(int count, cahr *command)
--
--     Inputs: wildlist - Wild card string to use as file input (may be multiple)
--             subst    - character to substitute for each %f
--             count    - Number of simple repeat times
--             command  - String to replace each time
--
--     Output: Sets up the common block
============================================================================ */
void LexFMMake(char *wild, char subst, char *command) {
	char myvar[MAX_LINELEN];

	free(lf->fmcommand); lf->fmcommand = NULL;
	free(lf->fmvarlist); lf->fmvarlist = NULL;
	if (lf->fmdirp != NULL) closedir(lf->fmdirp);
	lf->fmdirp = NULL;

	lf->fmcommand = strdup(command);
	trnslt(myvar, wild, sizeof(myvar), NULL);
	lf->fmchar    = (subst != '\0') ? subst : 'f' ;
	lf->fmvarptr  = lf->fmvarlist = strdup(myvar);
	lf->fmmode    = FM_LIST;
	lf->fmcnt     = 0;									/* Counter at 0 */
	return;
}

void LexFMCount(int count, char *command) {
	
	free(lf->fmcommand); lf->fmcommand = NULL;
	free(lf->fmvarlist); lf->fmvarlist = NULL;
	if (lf->fmdirp != NULL) closedir(lf->fmdirp); 
	lf->fmdirp = NULL;

	lf->fmcommand = strdup(command);
	lf->fmrepeat  = count;
	lf->fmmode    = FM_COUNT;
	lf->fmcnt     = 0;									/* Counter at 0 */
	return;
}


/* ---------------------------------------------------------------------------
-- Routine to modify a given format string from FORTRAN type to C type
-- It is not infinitely wise, only trivial cases will be properly handled
--
-- Usage:   ModifyFormat(char *format);
--
-- Inputs:  format - C type format string (to printf)
--
-- Output:  outstring filled with the converted string
--
-- Returns: nothing
--------------------------------------------------------------------------- */
PRIVATE BOOL ModifyFormat(char *format, size_t length) {

	char *aptr;
	char mycopy[MAX_FORMATLEN];
	int achr, repeatcount=1;

/* First, check to make sure we are looking at a fortran format */
	if (*format != '(') return(TRUE);					/* No leading ( ==> F77 */
	if ((strchr(format, '%')!=NULL) && (strchr(format,'\'')==NULL)) return(TRUE);

/* Okay, we now have a F77 string.  Delete parenthesis and start operation */
	strcpy(mycopy, format+1);								/* Have my copy now */
	if ( (aptr = strrchr(mycopy,')')) != NULL) *aptr = '\0';

	aptr = mycopy;												/* Where we copy from	*/
	
	while ( (achr=*(aptr++)) != '\0') {					/* Start moving chars */
		achr = tolower(achr);
		if (achr == 'f' || achr == 'g' || achr == 'e') {
			*(format++) = '%';								/* Start with % code */
			while (isdigit(*aptr) || *aptr == '.') *(format++) = *(aptr++);
			*(format++) = (char) achr;
		} else if (achr == 'x') {							/* Skip spaces */
			while (repeatcount--) *(format++) = ' ';
		} else if (achr == '\'') {
			while (*aptr != '\'') *(format++) = *(aptr++);
			aptr++;
		} else if (isdigit(achr)) {						/* Assume a count like 5 */
			repeatcount = (int) strtol(aptr, &aptr, 10);
			continue;											/* Skip reset of repeatcount */
		} else if (achr != ',' && !isspace(achr)) {
			return(FALSE);
		}
		repeatcount = 1;
	}
	*format = '\0';											/* Terminate the string */
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Routine to encode a string via a given format and series of tokens on the
-- command line.  Each %x element in the command string will grab the indicated
-- type token from the command line and encode it as required.  Integers
-- handled as the "nint" .
--
-- Usage:   char *LexEncodeString(char *out, size_t length, char *format);
--
-- Inputs:  format - C type format string (to printf)
--          out    - if NULL, string will be allocated
--          length - length of out (if out != NULL)
--
-- Output:  outstring filled with the converted string
--
-- Returns: Pointer to final string.  If out was NULL, this is allocated 
--          space and calling routine is responsible to free when done
--
-- Notes: With a NULL out, the maximum per-item size is 4096 characters.  
--        Will guarentee space for string variables as long as there
--        isn't a rediculous field length specified.  %s always works,
--        while %10000s may fail.
--------------------------------------------------------------------------- */
char *LexEncodeString(char *out, size_t length, const char *format) {

	char			*optr, *myout;							/* Output string pointer */
	int			mysize;
	char			szBuf[MAX_LINELEN],szTmp[MAX_LINELEN];
	char			myformat[MAX_FORMATLEN];			/* Local string for single var format */
	char			*strptr, *aptr;						/* Local pointers */
	int			VarType, VarModifier;				/* Type of encode, and potential modifier */
	BOOL			logtmp;
	int			i, ierr;
	ptrdiff_t	iused;
	TMPREAL		xl;										/* Real result		*/
	TMPCOMPLEX	cl;										/* Complex result */
	long			il;

/* If out is NULL, we have to allocate space for it instead */
	mysize = 8192;											/* Only needed if out == NULL, but set for lint */
	if ((myout=out) == NULL) myout = malloc(mysize);	/* Make it big - expect dump soon */
	*(optr = myout) = '\0';								/* Start as a NULL string */

	while (*format) {										/* Scan entire structure */

/* If I'm allocating space, make sure always 4096 chars available */
		if (out == NULL && (optr-myout) < 4096) {
			iused = optr-myout;
			mysize += 4096;
			myout = realloc(myout, mysize*sizeof(*myout));
			optr = myout+iused;
		}

/* Format should not deal with escaped chars - let string manipulation do that */
		if (*format == '%') {
			format++;										/* Dealt with that char		*/
			if (*format == '%') {						/* %% makes %					*/
				*(optr++) = *(format++);
			} else {											/* Real format specifier	*/
				aptr = myformat;							/* Make the format string	*/
				VarType = VarModifier = '\0';			/* Assume no modifiers		*/
				*(aptr++) = '%';							/* Need the % back now		*/
				while (*format && strchr("+- #.0123456789", *format) != NULL) 
					*(aptr++) = *(format++);
				if (*format && strchr("lLh", *format) != NULL) 
					VarModifier = *(aptr++) = *(format++);
				if (*format) VarType = *(aptr++) = *(format++);
				*aptr = '\0';								/* Terminate my format		*/
				if (strchr("cdioxXuzZeEfgGs", VarType) != NULL) LexConvertMode = MATH;
				logtmp = LexParseLine(szTmp, sizeof(szTmp), lf->ptr, &lf->ptr);
				trnslt(szBuf, szTmp, sizeof(szBuf), NULL);
				switch (VarType) {
					case 's':
						if (logtmp) strptr = GVEvalStrExpr(szBuf, &ierr);
						if (out == NULL && strlen(strptr) > (unsigned int) (mysize-(optr-myout))) {
							iused = optr-myout;
							mysize += (int) strlen(strptr);
							myout = realloc(myout, mysize*sizeof(*myout));
							optr = myout+iused;
						}
						if (logtmp && strptr!=NULL && ierr==0) {
							sprintf(optr, myformat, strptr);
							free(strptr);
						} else {
							ERRputs("ERROR: Invalid string expression (&encode)\n"); 
							sprintf(optr, myformat, "invalid");
						}
						optr += strlen(optr);
						break;
					case 'p':
						if (! GVGetAdrInfo(szBuf, NULL, (void **) &strptr, NULL)) {
							ERRputs("ERROR: Invalid pointer expression (&encode)\n");
							strptr = NULL;
						}
						sprintf(optr, myformat, strptr);
						optr += strlen(optr);
						break;
					case 'c':								/* character			*/
					case 'd':								/* decimal				*/
					case 'i':								/* integer				*/
					case 'o':								/* Octal constant		*/
					case 'x':								/* Hex constant		*/
					case 'X':
					case 'u':								/* Unsigned decimal	*/
						if (logtmp) xl = GVEvalExpr(szBuf,&ierr);
						if (! logtmp || ierr != 0) {
							ERRputs("ERROR: Invalid math expression\n"); 
							xl = 0;
						}
						il = nint(GVTrimToDouble(xl));	/* Convert to long integer */
						if (VarModifier == 'l')
							sprintf(optr, myformat, il);
						else										/* For short and int */
							sprintf(optr, myformat, (int) il);
						optr += strlen(optr);
						break;

					case 'z':								/* complex notation */
					case 'Z':
						aptr--;								/* Get the 'z' char */
						*aptr++ = 'g'-('z'-VarType);	/* Change from z to g format */
						strptr = aptr+1;
						if (myformat[1] != '+') *strptr++ = '+';
						strcpy(strptr, myformat+1);	/* Copy all but % symbol	*/
						*aptr = '%';						/* Replace first \0 with %	*/
						strcat(myformat, "j");			/* And add the char 'j'		*/

						cl = GVEvalComplexExpr(szBuf, &ierr);
						if (! logtmp || ierr != 0) {
							ERRputs("ERROR: Invalid math expression\n");
							cl.x = cl.y = 0;
						}
						if (VarModifier == 'L')
							sprintf(optr, myformat, cl.x, cl.y);
						else
							sprintf(optr, myformat, GVTrimToDouble(cl.x), GVTrimToDouble(cl.y));
						optr += strlen(optr);
						break;
					case 'e':								/* scientific notation */
					case 'E':
					case 'f':								/* Floating point notation */
					case 'g':								/* general notation */
					case 'G':
						if (logtmp) xl = GVEvalExpr(szBuf,&ierr);
						if (! logtmp || ierr != 0) {
							ERRputs("ERROR: Invalid math expression\n"); 
							xl = 0.0;
						}
						if (VarModifier == 'L') 
							sprintf(optr, myformat, xl);
						else
							sprintf(optr, myformat, GVTrimToDouble(xl));
						optr += strlen(optr);
						break;
					case 'n':								/* Screwball twits! */
						GVGetAdrInfo(szBuf, &i, (void **) &strptr, NULL);
						if (i == GV_INT) 
							*((int *) strptr) = (int) (optr-out);
						else if (i == GV_REAL) 
							*((REAL *) strptr) = (REAL) (optr-out);
						break;
					default:
						aptr = myformat; while (*aptr) *(optr++) = *(aptr++);
				}
			}
		} else {
			*(optr++) = *(format++);
		}
	}
	*optr = '\0';

/* Last step - either return passed string, or duplicate temporary string  */
	if (out == NULL) { out = strdup(myout); free(myout); }
	return out;
}

/* ---------------------------------------------------------------------------
-- Internal routine requiring a yes/no answer from the console!  Used to
-- query whether levels of a macro are to be aborted.
--
-- Usage: BOOL YesNo_U(CHAR *prompt, BOOL dflt);
--
-- Inputs: prompt - text to prompt user
--         dflt   - default response on <CR>
--
-- Output: Text to console and request from user typing.  The command line
--         is flushed before requesting input.
--
-- Return: TRUE for YES response, FALSE for NO response.  dflt on no response
--------------------------------------------------------------------------- */
PRIVATE BOOL YesNo_U(CHAR *prompt, BOOL dflt) {

	char *aptr,inline[8];
	
	TTYputs(prompt);
	while (TRUE) {
		TTYgets(inline,sizeof(inline));
		aptr = inline;
		while (*aptr == ' ') aptr++;
		if (*aptr=='\0' || strcmp(aptr,"/")==0) return(dflt);
		if (LexEqual(aptr, "yes", 1)) return(TRUE);
		if (LexEqual(aptr, "no", 1))  return(FALSE);
		ERRputs("ERROR: Answer the stupid question!  YES or NO? ");
	}
	panic; return(dflt);							/* BETTER NOT HAPPEN! */
}
