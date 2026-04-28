/* lexp2.c */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ---------------------------------------------- */
/* Locally defined global vars (OLD LEXP.INS)     */
/* ---------------------------------------------- */

/* ============================================================================
-- ... Aliasing function!  Designed to be called before any other command
-- ... processors.  Will expand a string to an arbitrary string based on 
-- ... previous commands.
--
-- Usage: BOOL = alias(char *token, int toklen)
--
-- Inputs: TOKEN - character token to be interpreted
--           ALIAS   => store alias for next token
--           EXECUTE => execute next token w/out aliasing
--           other   => Check if exists as an alias and execute
--
-- Added logic to implement replacement tokens for %1-%9 placeholders
============================================================================ */
static char AliasHelp[]
	= "ALIAS creates a line-macro combining multiple commands into a single new user\n"
	  "command.  It is most commonly used to abbreviate common operations into simple\n"
	  "to remember commands.  Alias includes parameter substitution from the remainder\n"
	  "of a command line, but is not as powerful as macros.\n"
	  "\n"
	  "Usage:\n"
	  "  alias <name> <expansion>                   - Create an alias for <name>\n"
	  "  alias [-Add | -DEfine] <name> <expansion>  - Same as no options\n"
	  "  alias F<nn> <expansion>                    - Set alias for a function key\n"
	  "  alias -Delete [<name> | -ALL]              - Delete an alias (or all)\n"
	  "  alias -Show <name>                         - Show expansion of an alias\n"
	  "  alias -List                                - Show expansion of all aliases\n"
	  "  alias -EXPort                              - List commands to create aliases\n"
	  "  alias -Execute <text>                      - Execute with no expansion\n"
	  "\n"
	  "The <name> can be almost any string.  The expansion is the remainder of the\n"
	  "command line, including potential continuation lines.  Each time the string\n"
	  "corresponding the <name> is encountered, it is replaced with the text of the\n"
	  "expansion, possibly with substitutional text described below.  The -exe option\n"
	  "allows commands to be redefined, but still executed manually (see examples).\n"
	  "\n"
	  "Substitutional text:\n"
	  "   In an expansion, %1, %2, ... serve as placeholders for subsitutional text\n"
	  "   taken from the command line when using the alias.  %1 represents the next\n"
	  "   token on the line, and so on.  If there is no token on the command line, the\n"
	  "   placeholder is assigned a single blank character.  If %2 is used but not %1,\n"
	  "   the first is still removed from the command line.\n"
	  "\n"
	  "Notes:\n"
	  "   (1) Aliases are stored as hidden strings in the function evaluator with the\n"
	  "       form $alias$<name>.  They can be defined as strings manually also.\n"
	  "   (2) Defining aliases for function keys is subject to OS acquiesence\n"
	  "\n"
	  "Examples:\n"
	  "   alias go read %1 -row 2 / -column 1 %2 let x = 1000/x let y = log(y)\n"
	  "     go afile.dat 7  -- reads column 1,7 and converts to the data\n"
	  "   alias replot go afile.dat 7 pl go afile.dat 8 ov go afile.dat 9 ov\n"
	  "     replot\n"
	  "   alias replot -delete\n"
	  "   alias F3 plot ov -fit -lt 1 -pen 2\n"
	  "   Use/need for -execute option (very dangerous usage normally)\n"
	  "     alias plot read %1 -silent alias -execute plot read %1 -col 3 4 overlay\n"
	  "     plot f1.dat\n"
	  "     alias -ex plot\n";

BOOL LexAlias(char *token, int toklen) {

	char	name[VARNAME_STR_SIZE],						/* alias var name		*/
			lookup[VARNAME_STR_SIZE+8],				/* Appended name		*/
			definition[HUGE_STR_SIZE];					/* Replacment defn	*/
	char 	tokenbuf[LONG_STR_SIZE], *tokens[10];	/* Replace %1,%2,...	*/
	char	DelimHold[MAX_DELIMS];
	char  *iptr, *optr, *base;							/* Various pointers	*/
	int	i, idx, ntok;

	if (stricmp(token, "alias") == 0) {
		if (LexCheckHelp("ALIAS", AliasHelp, NULL)) return(TRUE);

		if (! LexGetTokenP(name, sizeof(name),
			"Alias [-add|-delete|-show|-list|-execute] <name> {<expansion>}]: "))
			return(TRUE);
		if (LexEqual(name, "-add", 2) || LexEqual(name, "-define", 3)) {
			if (! LexGetTokenP(name, sizeof(name), "Alias name: ")) return(TRUE);
			/* Continue through to code without -add */
		} else if (LexEqual(name, "-delete", 2)) {
			if (! LexGetTokenP(name, sizeof(name), "Alias name: ")) return(TRUE);
			if (stricmp(name, "-all") != 0) {
				strncat(strcpy(lookup,"$alias$"), name, sizeof(lookup)-8);
				GVDeallocate(lookup);
			} else {
				void *entry=NULL;
				char *varname, **vars=NULL;
				int flags, icnt=0, imax=0;
				while (GVGetNextEntry(&entry, &varname, NULL, &flags, NULL)) {
					if (flags & GVF_ALIAS) {					/* Only flagged as GVF_USER */
						if (icnt >= imax) imax+=20, vars=realloc(vars,imax*sizeof(*vars));
						vars[icnt++] = strdup(varname);
					}
				}
				for (i=0; i<icnt; i++) {GVDeallocate(vars[i]); free(vars[i]);}
				free(vars);
			}
			return(TRUE);
		} else if (LexEqual(name, "-show", 2)) {
			if (! LexGetTokenP(name, sizeof(name), "Alias name: ")) return(TRUE);
			strncat(strcpy(lookup,"$alias$"), name, sizeof(lookup)-8);
			if ( (base=GVFindString(lookup)) == NULL) base = "<not defined>";
			TTYprintf(" %s -> %s\n", name, base);
			return(TRUE);
		} else if (LexEqual(name, "-list", 2) || LexEqual(name, "-export", 4)) {
			void *entry=NULL, *adr;
			char *varname, *format;
			int flags, type;
			format = LexEqual(name, "-list", 2) ? "  %-16s -> %s\n" : "alias %s %s\n";
			while (GVGetNextEntry(&entry, &varname, &type, &flags, &adr)) {
				if ( !(flags & GVF_ALIAS) || (type != GV_STRING && type != GV_STRING_LINK) )
					 continue;
				if (strnicmp(varname, "$alias$", 7) == 0)
					TTYprintf(format, varname+7, *((char **) adr));
			}
			return(TRUE);
		} else if (LexEqual(name, "-execute", 2)) {
			return(! LexGetToken(token, toklen));
		}
		LexGetRestNT(definition, sizeof(definition));
		if (*definition == '\0') {
			LexPromptStrNT(definition, sizeof(definition), "Expansion: ");
			if (*definition == '\0') return(TRUE);
		}
		if (tolower(*name) == 'f') {					/* Possible function key */
			i = (int) strtol(name+1, &optr, 10);
			if (*optr=='\0' && CONSetMacro(i,definition) != NULL) return(TRUE);
		}
		strncat(strcpy(lookup,"$alias$"), name, sizeof(lookup)-8);
		if (! GVAllocString(lookup, GVF_HIDDEN | GVF_ALIAS, 0) || !GVSetValue(lookup, definition)) {
			ERRprintf("ERROR: Unable to allocate or set the alias for %s\n", name);
			LexFlush();
		}
		return(TRUE);								/* Successful */
	}

	strncat(strcpy(lookup,"$alias$"), token, sizeof(lookup)-8);
	if ( (base=GVFindString(lookup)) == NULL) return(FALSE);

/*	TTYprintf("Found an alias request %s\n", lookup); */

/* Have a string in base, replace %i with corresponding tokens */
	strscpy(DelimHold, LexGetDelim(), sizeof(DelimHold));	/* Save old delims */
	LexSetDelim(" \t\n\r");						/* Allow only white space	*/

	ntok = 0;										/* No tokens read so far	*/
	tokens[0] = tokenbuf;						/* Next goes in first posn */

/* And, now parse the line replacing %i with corresponding token */
/*	TTYprintf("About to replace the i and f stuff\n"); */

	for (iptr=base, optr=definition; *iptr!='\0'; iptr++) {
		if (*iptr != '%') {
			*optr++ = *iptr;
		} else if (iptr != base && *(iptr-1) == '\\') {
			*(optr-1) = *iptr;
		} else if ( *(iptr+1)=='\0' || strchr("123456789",*(iptr+1))==NULL ) {
			*optr++ = *iptr;
		} else {
			idx = *(++iptr) - '0';				/* Which token?				*/
			while (ntok < idx) {					/* Make sure we have it		*/
				LexGetToken(tokens[ntok], DFLT_STR_SIZE);
				tokens[ntok+1] = tokens[ntok] + strlen(tokens[ntok]) + 1;
				ntok++;
			}
			strcpy(optr, tokens[idx-1]);
			optr += strlen(tokens[idx-1]);
		}
	}
	*optr = '\0';
	LexSetDelim(DelimHold);						/* Restore original */

/*	TTYprintf("About to insert definition\n"); */
	LexInsText(definition);
/*	TTYprintf("Returning from insertion of definition\n"); */
	return(TRUE);
}

/* ============================================================================
--     Routine to check (preparse) a token from the command line.
--
--     Usage:  BOOL = LexChkToken(TOKEN)
--
--     Inputs: none
--
--     Output: TOKEN  - character token
============================================================================ */
BOOL LexChkToken(char *token, INT toklen) {

	BOOL rc;
	rc = LexGetToken(token, toklen);
	if (rc) LexBackup();
	return(rc);
}

/* ============================================================================
-- Routine to get a token from the command line but return only if it is
-- an option as identified by a '-' or '/' first character.
-- If the option is identified by a '/', it is converted to a '-' char.
--
-- Usage:  BOOL LexGetOption(char *token, INT toklen);
--         BOOL LexGetOptionEx(char *token, INT toklen, char *list);
--
-- Inputs: token - pointer to string to receive option token (if exists)
--         toklen - length of token to return
--         list  - list of characters which mark an option
--
-- Output: *token  - filled with option (including the - character)
--
-- Return: TRUE if the next token is an option, false otherwise.
--
-- Note: LexGetOption(token, toklen) == LexGetOptionEx(token, toklen, "-/");
--       Default delimiters are the - and /.  In UNIX, there is great value
--       in using only the -, but this is incompatible so do on limited basis.
--       The returned token will always have the first character from the list
--       as the leading character.
============================================================================ */
BOOL LexGetOptionEx(char *token, INT toklen, char *list) {

	char *aptr;

	if (list == NULL) list = "-/";

	if (LexGetToken(token, toklen)) {
		for (aptr=list; *aptr; aptr++) {
			if (*token != *aptr) continue;
			if (*token == '/' && (token[1] == '\0' || token[1] == '*')) continue;		/* Special handling */
			*token = *list;
			return(TRUE);
		}
		LexBackup();
	}
	return(FALSE);
}

BOOL LexGetOption(char *token, INT toklen) {
	return LexGetOptionEx(token, toklen, NULL);
}


/* ============================================================================
--     Retrieve the next argument from the command line and convert it as
--     an integer argument.  If no token is present, attempt to read another
--     command line.  If an illegal argument is passed, reset the command
--     line and read a new value
--
--     Usage: INT = NRDARG(IDFLT,PRMPT)
--
--     Inputs: IDFLT - Default value (INT)
--             PRMPT - Text for query if no token present (CHARACTER)
--
--     Output: NRDARG - Value returned.  A <CR> or a / is intepreted as
--                      the default.
--
--     Note: Includes the Kludge for DEC systems which can't do list-directed
--           internal reads
--
-- July 1987 - MOT
--     Switched NRDARG to go through RDARG and does a NINT operation.  Allows
--     full use of RDEVAL expressions.
============================================================================ */
INT LexGetInt(INT dflt, const char *prompt) {
	
	char		token[LONG_STR_SIZE];				/* Expression string */
	TMPREAL	tmp;
	INT		itmp;
	int		ierr;

	while (TRUE) {
		if (! LexGetMathP(token, sizeof(token), prompt)) return(dflt);
		tmp = GVEvalExpr(token, &ierr);
		if (ierr != 0) {
			ERRprintf("ERROR: Illegal numerical expression (%s)\n", token);
		} else if (tmp < INT_MIN || tmp > INT_MAX) {
			ERRprintf("ERROR: Value out of range as an integer (%g)\n", tmp);
		} else {
			break;
		}
		LexFlush();
	}

	itmp = (int) ((tmp > 0) ? tmp+0.5 : tmp-0.5);
	if (fabs(itmp-tmp) > 0.1) ERRprintf("OUCH! Failed to properly convert %g\n", tmp);

	return(itmp);
}


/* ============================================================================
--     Retrieve the next argument from the command line and convert it as
--     a REAL number.  If no token is present, attempt to read another
--     command line.  If an illegal argument is passed, reset the command
--     line and read a new value
--
--     Usage: REAL = RDARG(DFLT,PRMPT)
--
--     Inputs: DFLT  - Default value (REAL)
--             PRMPT - Text for query if no token present (CHARACTER)
--
--     Output: RDARG - Value returned.  A <CR> or a / is intepreted as
--                      the default.
--
--     Note: Includes the Kludge for DEC systems which can't do list-directed
--           internal reads
--
-- ... Jan, 1986 - PC now uses external routine to allow interpretation of
--                 complex expressions.
============================================================================ */
REAL LexGetReal(REAL dflt, const char *prompt) {

	char		token[LONG_STR_SIZE];				/* Expression string */
	REAL		tmp;
	int		ierr;

	while (TRUE) {
		if (! LexGetMathP(token, sizeof(token), prompt)) return(dflt);
		tmp = GVTrimToReal(GVEvalExpr(token, &ierr));
		if (ierr == 0) return(tmp);
			ERRprintf("ERROR: Illegal numerical expression (%s)\n", token);
			LexFlush();
			continue;
	}
	panic; return(dflt);								/* BETTER NOT HAPPEN! */
}


/* ============================================================================
-- Routine to check for a single command word.
--
--     Usage: LOG = LexSingle(DEFAULT,COMMAND,PRMPT)
--
--     Inputs: DEFAULT - Default return value (BOOL)
--             COMAND  - Command to check for (CHARACTER)
--             PRMPT   - Text string for query if no token already present
--
--     Output: SINGLE  - Result. If typed command matches the COMAND, .TRUE.
--                       is returned.  Any other response returns .FALSE.
--                       A <CR> typed to the PRMPT query results in DEFAULT.
============================================================================ */
BOOL LexSingle(BOOL dflt, const char *cmmd, const char *prmpt) {

	char token[DFLT_STR_SIZE];						/* Token to read */

	if (LexGetTokenP(token, sizeof(token), prmpt)) 
		return(LexEqual(token, cmmd, 1));
	else
		return(dflt);
}

/* ============================================================================
--     Routine to test the next argument for some form of ON or OFF.
--
--     Usage: LOG = ONOFF(DEFAULT,PRMPT)
--
--     Inputs: DEFAULT - Default return (BOOL)
--             PRMPT   - Text to query response if no token present (CHARACTER)
--
--     Output: ONOFF - .TRUE. if next argument some form of ON
--                     .FALSE. if next argument some form of OFF
--                     DEFAULT if <CR> response to query
--                     Requery if neither
--
-- #define LexOnOff(dflt,prmpt) LexChoice(dflt, "ON", "OFF", prmpt)
============================================================================ */

/* ============================================================================
--     Routine to test the next argument for some form of YES or NO.
--
--     Usage: LOG = YESNO(DEFAULT,PRMPT)
--
--     Inputs: DEFAULT - Default return (BOOL)
--             PRMPT   - Text to query response if no token present (CHARACTER)
--
--     Output: YESNO - .TRUE. if next argument some form of YES
--                     .FALSE. if next argument some form of NO
--                     DEFAULT if <CR> response to query
--                     Requery if neither
--
-- #define LexYesNo(dflt,prmpt) LexChoice(dflt, "YES", "NO", prmpt)
============================================================================ */

/* ============================================================================
--     Routine to select between two command choices.
--
--     Usage: LOG = CHOICE(DEFAULT,TRUE,FALSE,PRMPT)
--
--     Inputs: DEFAULT - Default return value (BOOL)
--             TRUE    - Command for true return (CHARACTER)
--             FALSE   - Command for false return (CHARACTER)
--             PRMPT   - Text string for query if no token already present
--
--     Output: CHOICE  - Result. Returns .TRUE. if response matches TRUE and
--                       .FALSE. if response matches FALSE.  If no token is
--                       available, another command line is read with PRMPT.
--                       A CR response to this query returns DEFAULT.
--
-- Note: The TRUE and FALSE options can be multiple collections of
-- valid responses.  Each possible is separated by a semicolon.
--    LexChoice(TRUE, "ON;YES", "OFF;NO", "Continue? ")
-- returns true for either ON or YES response, false for OFF or NO.
-- Much more flexible.
============================================================================ */
BOOL LexChoice(BOOL dflt, const char *istrue, const char *isfalse, const char *prmpt) {

	char token[SHORT_STR_SIZE], *aptr, mytrue[SHORT_STR_SIZE], myfalse[SHORT_STR_SIZE];
	BOOL utrue, ufalse;

	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), prmpt)) return(dflt);
		utrue = ufalse = FALSE;
		aptr = (char *) istrue;
		while (aptr && ! utrue) {
			if (LexEqual(token, aptr, 1)) utrue = TRUE;
			if ( (aptr = strchr(aptr, ';')) != NULL) aptr++;
		}
		aptr = (char *) isfalse;
		while (aptr && ! ufalse) {
			if (LexEqual(token, aptr, 1)) ufalse = TRUE;
			if ( (aptr = strchr(aptr, ';')) != NULL) aptr++;
		}
		if (! utrue && ! ufalse) {
			strscpy(mytrue,  istrue,  sizeof(mytrue));
			strscpy(myfalse, isfalse, sizeof(myfalse));
			if ( (aptr = strchr(mytrue, ';')) != NULL) *aptr = '\0';
			if ( (aptr = strchr(myfalse,';')) != NULL) *aptr = '\0';
			ERRprintf("ERROR: Invalid or non-unique response.  Expecting %s or %s\n", mytrue, myfalse);
			LexFlush();
		} else {
			return(utrue);
		}
	}
	panic; return(utrue);							/* BETTER NOT HAPPEN! */
}

/* ============================================================================
--     Routine to interpret commands.  A command table is passed and
--     the routine searches for the input token.  The search is made
--     for unique match.  All commands may be abbreviated to the minimum
--     unique characters.
--
--     Usage: INT = COMAND(TOKEN, TABLE, TABSIZ)
--
--     Inputs: TOKEN -  Command to be searched for (CHARACTER)
--             TABLE -  Table of commands. (CHARACTER array)
--             TABSIZ - Number of commands in the TABLE (INT)
--
--     Output: COMAND - Return code indicating which command specified
--                  code          meaning
--                   -1   illegal command (null, ambiguous command, . . .)
--                    0   command not found in table
--                   >0   index of command in table
--
-- Example:
--     CHARACTER*4 TABLE(3),TOKEN
--     DATA TABLE /'OFF', 'ON', 'THIS'/
--     I = SELECT(TOKEN,TABLE,3)
--
-- April 23, 1986 - MOT
--     Modified slightly.  The program will "find" a successful command even
--     if it is not unique if it EXACTLY matches the command in the list.
--     For example, TABLE = "FOR   FORM", will return true with passed token
--     of "FOR" where it previously return ambiguous.  Requires exact match.
============================================================================ */
INT LexComand(const char *token, const char *table[]) {

	int i,nmatch=0,icnt=0,imatch=0;
	
	while (*table != NULL) {						/* While more entries */
		icnt++;
		if ((i=LexEqual(token, *table, 1)) != 0) {	/* Compare the strings */
			if (i == 2) return(icnt);				/* Exact, return now */
			imatch=icnt;
			nmatch++;
		}
		table++;
	}
	if (nmatch > 1) return(-1);
	return(imatch);									/* Return proper value */
}


/* ============================================================================
--     Slightly modified COMAND function which allows inline calls of commands.
--     Routine to interpret commands.  A command table is passed and
--     the routine searches for the input token.  The search is made
--     for unique match.  All commands may be abbreviated to the minimum
--     unique characters.  However, an exact match will always return.
--
--     Usage: INT LexSelect(char *token, char *table);
--
--     Inputs: TOKEN -  Command to be searched for (CHARACTER)
--             STRING -  Table of commands. (CHAR space deliminated)
--
--     Output: SELECT - Return code indicating which command specified
--                  code          meaning
--                   -1   illegal command (null, ambiguous command, . . .)
--                    0   command not found in table
--                   >0   index of command in table
--
-- Example:
--     i = LexSelect(token, "THIS WHAT IS WHERE");
============================================================================ */
INT LexSelect(const char *token, const char *table) {

	char	*iptr, tokmatch[SHORT_STR_SIZE];		/* Token match */
	int	i,icnt=0,match=0,nmatch=0;

	if (*token == '\0') return(-1);				/* Null command */

	while (TRUE) {
		while (isspace(*table) && (*table != '\0')) ++table;
		if (*table == '\0') break;
		iptr = tokmatch; i = sizeof(tokmatch);
		while ( (!isspace(*table)) && (*table != '\0')) {
			if (--i > 0) *(iptr++) = *table;
			++table;
		}
		icnt++;											/* Have another command		*/
		*iptr = '\0';									/* Null terminate tokmatch */
		if ( (i=LexEqual(token, tokmatch, 1)) != 0) {
			if (i == 2) return(icnt);				/* Exact match, return now */
			match = icnt;
			nmatch++;
		}
	}
	if (nmatch > 2 || icnt == 0) {				/* Ambiguous command */
		return(-1);
	} else {
		return(match);									/* Return code */
	}
}


/* ============================================================================
--     New routine to interpret commands.  More flexible than the
--     old COMAND command.  Allows commands to have required minimum characters
--     as well as arbitrary return codes.  Allows for easy synonymous commands.
--
-- Usage:  void *LexCmdl(char *token, void *cmlist, size_t cmlen);
--
-- Inputs: token  - Command to be searched for (CHARACTER)
--         cmlist - Array of command structures.  Structure must have as
--                  its first two elements
--                     CHAR *command  -- Actual text to match.  If NULL,
--                                       marks last element of array.
--							  INT minlen     -- Minimum chrs to match
--								                 >0 ==> listed in PRTCMx commands
--									              <0 ==> synonym (not listed in help)
--         cmlen  - sizeof(*cmlist);
--
-- Output: LexCmdl - Pointer to the first structure which matched.
--                   NULL if no array structure matches given token.
--
-- Example:
-- 
--	typedef struct _CMDLTYPE {
--		char		*command;
--		INT		 minlen;
--		char		*string;
--	} CMDLTYPE;
--
--	CMDLTYPE cmlist[] = {  {"SIZE",			2,	"Requested size command"},
--								{"ANGLE",		3,	"Command request for angle"},
--								{"DIRECTION",	3,	"Command requesting direction change"},
--								{"COLOR",		5,	"Color command"},
--								{"HELP",			4,	"User wants help"},
--								{NULL,         0, NULL} };
--	CMDLTYPE	*gotlist;
--	INT		 err;
--	char		 tok8[9];
--
--	while (TRUE) {
--		while (! LexGetTokenP(tok8, sizeof(tok8), "ANNOTATE")) {};
--		if ( (gotlist=LexCmdl(tok8, cmlist, &err)) != NULL) {
--			printf("String returned \"%s\"\n", gotlist->string);
--			continue;
--		} else if (err > 0) 
--			printf("Ambiguous command: %s %i %p\n",tok8, err, gotlist);
--		else
--			printf("Unrecognized command" %s %i %p\n",tok8, err, gotlist);
--		LexFlush();
--	}
=========================================================================== */
typedef struct _CMDLTYPE {
	char		*command;
	int		minlen;
} CMDLTYPE;

void *LexCmdl(const char *token, const void *cmlist, const size_t cmlen) {

	CMDLTYPE	 *list = (CMDLTYPE *) cmlist;		/* Local copy */
	int		 toklen;									/* Length of token   */

/* Search the command table for one or more matches */

	while (isspace(*token)) token++;					/* Eliminate white space */

	if ( (toklen = (int) strlen(token)) > 0) {	/* Anything there? */
		while (list->command != NULL) {
			if (list->minlen == 0) {
				if (stricmp(token, list->command) == 0) return((void *) list);
			} else if (toklen >= abs(list->minlen)) {
				if (memicmp(token,list->command,toklen)==0) return((void *) list);
			}
			list = (CMDLTYPE *) ( ((char *) list) + cmlen);
		}
	}

	return(NULL);
}

/* ============================================================================
--     Subroutine prtcmd - Simple routine to print out the commands from
--     the table used for cmdl in a clean manner.  Converts the required
--     characters to upper case and outputs as many as possible on a 80
--     column line.  Uses a maximum of TOKLEN characters.
--
--     Usage: CALL PRTCMD(CMLIST,CMSPEC)
--            CALL PRTCM1(CMLIST,CMSPEC,TITLE)
--
--     Inputs: See CMDL documentation above
--             Command listing has no blank lines before or after.
--
-- Example:
-- CALL TONL								! Put a blank line around list
--     CALL PRTCMD(CMLIST,CMSPEC)   ! Output the help
--       or
--     CALL PRTCM1(CMLIST,CMSPEC,'General useful commands:')
--     CALL TONL                    ! And end with blank line
============================================================================ */
#define LINESIZE 80

void LexCmdlPrint(const void *cmlist, size_t cmlen, const char *header) {
	LexCmdlPrintEx(cmlist, cmlen, 8, header);
	return;
}

void LexCmdlPrintEx(const void *cmlist, size_t cmlen, int maxlen, const char *header) {

	CMDLTYPE	 *list;								/* Local copy */
	int		 ocnt,								/* Count of output characters */
				 upcnt,								/* Number of chars to upcase	*/
				 usecnt;								/* Number of chars left to copy */
	char		 outbuf[LINESIZE+1];
	char		 *iptr, *optr;

	if (header != NULL) {						/* Header line? */
		ScrSetAttrib(D_BOLD);					/* Make bold */
		TTYputsnl(header);
		ScrSetAttrib(D_NORMAL);
	}
	
	if (maxlen <= 0) {							/* Determine from maximum size */
		list = (CMDLTYPE *) cmlist;			/* Reset the counter */
		while (list->command != NULL) {
			ocnt = (int) strlen(list->command);
			maxlen = max(maxlen, ocnt);
			list = (CMDLTYPE *) ( ((char *) list) + cmlen);
		}
	}

	list = (CMDLTYPE *) cmlist;					/* Reset the counter		*/
	optr = outbuf;										/* Start in buffer		*/
	ocnt = 0;											/* No characters output */

	while ( (iptr=list->command) != NULL) {	/* Go through second time */
		if ( (upcnt=min(maxlen,list->minlen)) > 0) {
			if (optr != outbuf) {*optr++ = ' '; *optr++ = ' ';}
			ocnt  += maxlen+2;
			usecnt = maxlen;
			while (upcnt--)               {*optr++ = (char) toupper(*iptr++); usecnt--;}
			while (*iptr!='\0' && usecnt) {*optr++ = (char) tolower(*iptr++); usecnt--;}
			while (usecnt--)		          *optr++ = ' ';
		}
		if (ocnt+maxlen > LINESIZE) {			/* Is there space for another? */
			*optr = '\0';
			TTYputsnl(outbuf);
			optr = outbuf;
			ocnt = 0;
		}
		list = (CMDLTYPE *) ( ((char *) list) + cmlen);
	}
	if (optr != outbuf) {*optr = '\0'; TTYputsnl(outbuf);}
	return;
}

/* ============================================================================
--     Subroutine CHKCMD - Simple routine to check out the command list for
--     the CMDL command.  Will increase the minimum number of characters
--     in any command to eliminate any ambigous commands.  The first command
--     will remain unchanged (unless 0) and the second increased to first
--     different character.
--
--     Usage: CALL CHKCMD(CMLIST,CMSPEC)
--
--     Inputs: See CMDL documentation above
--             Command listing has no blank lines before or after.
--
-- Example:
--     CALL CHKCMD(CMLIST,CMSPEC)   Check the command list CMLIST
--
-- NOTES: VAX Fortran versions 4.0-4.3 inclusive will crash terminally on this
--        routine.  The routine is meant mainly for development and can safely
--        be nulled out (ie. RETURN / END) for all applications.
============================================================================ */
void LexCmdlCheck(const void *cmlist, const size_t cmlen) {

	CMDLTYPE	*list, *list2;							/* Local copies */
	size_t   max1,min1,max2,min2,k;

	list = (CMDLTYPE *) cmlist;
	while (list->command != NULL) {				/* Check the list */
		max1 = strnblen(list->command);			/* Full length */
		min1 = abs(list->minlen);					/*	Minimum required length */
		if (max1 == 0) {								/* NULL command */
			ERRprintf("WARNING: Null or blank command in a CMDL list (CHKCMD)\n");
		} else if (min1 > max1) {					/* Can't ever match? */
			ERRprintf("WARNING: Command (%s) unreachable since min length greater than real (CHKCMD)\n", list->command);
		} else if (min1 == 0) {
			ERRprintf("WARNING: Command (%s) has zero specified as minimum length\n", list->command);
		} else {
			list2 = (CMDLTYPE *) ( ((char *) list) + cmlen);
			while (list2->command != NULL) {
				max2 = strnblen(list2->command);
				min2 = abs(list2->minlen);
				if (min2 != 0 && max2 != 0 && min2 <= max2) {
					k = max(min1, min2);						/* How far necessary? */
					if (k <= max1 && k <= max2) {
						if (memicmp(list->command, list2->command, k) == 0) {
							ERRprintf("WARNING: Potentially ambiguous commands: \"%s\" and \"%s\" (CHKCMD)\n", list->command, list2->command);
						}
					}
				}
				list2 = (CMDLTYPE *) ( ((char *) list2) + cmlen);
			}
		}
		list = (CMDLTYPE *) ( ((char *) list) + cmlen);
	}
}


/* ============================================================================
--     Function FMTENG - Converts a real number to engineering format,
--                       e.g. 5.6E6 -> "5.600 M";  Four digit only.
--
--     Usage: STRING = FMTENG(REAL)
--
--     Inputs: REAL - Real number to be converted
--
--     Output: FMTENG - (Function value) 8 Character string containing result
--
--     Warning: FMTENG must be declared CHARACTER*8 in the calling program
--
============================================================================ */
char *ToEngFormat(REAL x) {

	REAL		xme;
	int		iexp=0, idig=0;
	char		unit;
	static	char	 output[SHORT_STR_SIZE];
	static	char	 bigu[]   = " kMGTPE";
	static	char	 smallu[] = " munpfa";
	static	char	*fmts[]   = {"%6.3f %c", "%6.2f %c", "%6.1f %c"};

	if (x == 0.0f) {
		strcpy(output,"   0.0 ");
		return(output);
	}

	xme = (REAL) fabs(x);							/* Value I encode */
	while (xme >= 10.0f) {xme /= 10.0f; iexp++;}
	while (xme < 1.00f)  {xme *= 10.0f; iexp--; idig++;}
	while (iexp%3 != 0)  {xme *= 10.0f; iexp--; idig++;}
	if    (x < 0.0f)      xme = -xme;			/* xme is value I encode! */
	iexp /= 3;											/* No lookup value */

	if (abs(iexp) >= 6)								/* Too big (more than 10**18) */
		sprintf(output,"%f",x);
	else {
		if (iexp >= 0) 
			unit = bigu[iexp];
		else
			unit = smallu[-iexp];
		sprintf(output, fmts[idig], xme, unit);
	}
	return(output);
}
