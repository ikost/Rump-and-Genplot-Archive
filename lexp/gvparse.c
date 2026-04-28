/* gvparse.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#if ! (defined CSET2 || defined MSC60 || defined CONVEX_C || defined MSC70)
	#define BAD_STRTOD_FNC					/* STRTOD bad function -- lots have it */
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "gvdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* WARNING: Do not change CMD_BLOCK_SIZE unless you search for all copying */
/*          of the command string.  This number hard coded in some code.	*/

#define  CMD_BLOCK_SIZE		256					/* Each block is 256 bytes		*/
#define	CMD_BLOCK_LIMIT  4000					/* Maximum of 4000 blocks		*/
#define  CMD_BLOCK_EXPAND	128					/* Expand size is at 256		*/
#define	CMD_STRING_EXPAND	256					/* Maximum string allowed		*/

#define	MAX_FUNCTION_DEPTH	200				/* Function recursion depth	*/

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE int get_number_value(void);							/* Gets nums w/ operators	*/
PRIVATE int get_string_value(void);							/* Gets string w/ operator	*/
PRIVATE int get_name_value(char **name, int *isize);	/* Gets a name argument */

PRIVATE int get_number(void);									/* Gets single item value	*/
PRIVATE int get_string(void);									/* Gets single item string	*/

PRIVATE int get_pointer_value(GVP_PARSEMODE mode);
PRIVATE int get_pointer(GVP_PARSEMODE mode);

static int get_multiple_args(int nargs);					/* Parse for nargs values */
static ARRAY *get_array_entry(void);						/* Parse for an array */

PRIVATE int get_var(int magic, int len, char *aptr, GVP_PARSEMODE mode);
PRIVATE int get_internal_fnc(int magic, int len, char *aptr, GVP_PARSEMODE mode);
PRIVATE int get_name(char **endptr, int *magic, int *len);
PRIVATE GV_ENTRY *get_entry(char **endptr);
PRIVATE int copy_string(char *dest, char *src);
PRIVATE int gv_copy_argument(char *dest, char *src, char **endptr);

/* Common entry point for error messages */
PRIVATE void gv_err_msg(char *msg);
PRIVATE void gv_err_msg2(char *msg, char *start, char *now);

PRIVATE void    gv_array_stats(ARRAY *ar, int *npt, TMPREAL *mean, TMPREAL *stdev);
PRIVATE TMPREAL gv_eval_statistics(ARRAY *ar[], char type);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
PRIVATE char *expstart, *expr;
PRIVATE GVCMDS *cmdstart, *cmd;
PRIVATE int  cmdlen=0;								/* Number of blocks in cmdstart */
PRIVATE GVPARSEINFO cmdinfo;
PRIVATE int RecursionDepth;

PRIVATE int  paren_count;
PRIVATE int   ArgBase,  ArgTop,  ArgNext;		/* Float  argument sub-stack */
PRIVATE int  SArgBase, SArgTop, SArgNext;		/* String argument sub-stack */
PRIVATE BOOL FullParse=TRUE;
PRIVATE BOOL SimpleName;

/* ---------------------------------------------------------------------------
-- Routines to duplicate and free the command parse stack from complex
-- expressions.  These routines used to duplicate the stack in order to avoid
-- high overhead parsing in routine duplication.
--
-- Usage:  GVCMDS *GVDupCmds(GVCMDS *cmds)
--         void    GVFreeCmds(GVCMDS *local)
--
-- Inputs: cmds - Normally the return value from GVParse.  GVParse returns
--                pointer to static block, this routine then duplicates.  Can
--                also be used to duplicate an existing stack.  Internal
--                details of the stack are not visible to the user routines.
--
-- Output: none
--
-- Returns: !NULL -- pointer to copy of the specified command stack
--           NULL -- Failed to allocate memory
--------------------------------------------------------------------------- */
void GVFreeCmds(GVCMDS *local) {
	int id;
	GVCMDS *mycmds=local;

	if ( (local != NULL) && (local != cmdstart)) {
		GVGETITEM(&id, mycmds, int);
		if (id == MY_GV_ID) free(local);
#ifdef DEBUG
		if (id != MY_GV_ID) ERRprintf("Requested free of MYCMDS *block which appeared invalid\n");
#endif
	}
	return;
}

GVCMDS *GVDupCmds(GVCMDS *local) {
	
	GVCMDS *new=local;
	int id,length;

	if (local == NULL) return(NULL);
	GVGETITEM(&id,     new, int);						/* Check ID					*/
	GVGETITEM(&length, new, int);						/* And get block length */
	if (id != MY_GV_ID) return(NULL);

	if ( (new = (GVCMDS *) malloc(length)) == NULL) return(NULL);
	memcpy(new, local, length);
	return(new);
}


/* ---------------------------------------------------------------------------
-- Routines to return information byte about a command parse.
--
-- Usage:  int GVTellCmds(GVCMDS *local, GVPARSEINFO *info);
--
-- Inputs: local - Normally the return value from GVParse.  However, can be
--                 any command stack pointer.
--
-- Output: info - Filled with GVPARSEINFO structure of the command
--
-- Returns:  0 - All okay
--          -1 - Probably not a command stack
--------------------------------------------------------------------------- */
int GVTellCmds(GVCMDS *local, GVPARSEINFO *info) {
	
	GVCMDS *new=local;
	int id,type,length;

	if (local == NULL) return(-1);
	GVGETITEM(&id,     new, int);						/* Check ID				*/
	GVGETITEM(&length, new, int);						/* And block length	*/
	GVGETITEM(&type,   new, int);						/* Type of variables */
	GVGETITEM(&length, new, int);						/* Array lengths		*/
	info->type   = type;
	info->length = length;
	return( (id == MY_GV_ID) ? 0 : -1);
}

/* ---------------------------------------------------------------------------
-- Routine to modify flags in a GVCMDS structure to include 
-- GV_INFO_COMPLEX_REFERENCE bit setting.
--
-- WARNING: There are internal usages of the "magic" expression in GVPARSE
--------------------------------------------------------------------------- */
EXTERN void gv_force_complex(GVCMDS *local) {

	int type;

	local += 2*sizeof(int);							/* Skip magic & block length */
	GVGETITEM(&type, local, int);					/* Get the type				  */
	type |= GV_INFO_COMPLEX_REFERENCE;			/* Force complex mode		  */
	local -= sizeof(int);							/* Backup and put in place	  */
	GVPUTITEM(local, type, int);
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to parse a character math expression into command structure ready
-- to be evaluated by expression evaluator.
--
-- Usage:  GVCMDS *GVParse(char *myexp, GVPARSEINFO *info)
--         GVCMDS *GVChkParse(char *myexp, GVPARSEINFO *info)
--
-- Inputs: myexp -- character string expression to be evaluated.
--
-- Output: info    -- if info is not NULL, *info returns bit info on type of 
--                    expression.
--                       01 ==> contains raw array references (unindexed)
--
-- Returns:  NULL -- unsuccessful parsing of expression
--          !NULL -- Otherwise pointer to a static command structure with
--                   instructions for evaluator.  Use GVDupCmds to duplicate 
--                   structure and save the instruction stack for directly 
--							calling GVEvalCmds
--
-- Only difference is in the evaluation of @xxx(nsl) commands.  GVParse
-- actually evaluates them and inserts the value while GVChkParse only checks
-- on the validity.
--
-- 7-Apr-92 - MOT
--   Changed code in @min, @max section to allow recursive search through an
--   expression as well as simple arrays.  eval @min(ln(y)) or @min(abs(y))
--   to find the value closest to zero.  Handled in gvcalc.c which which looks
--   at the command stack and evaluates at parse time as appropriate.
--------------------------------------------------------------------------- */
GVCMDS *GVChkParse(char *myexp, GVPARSEINFO *info) {

	GVCMDS *rcode;
	FullParse = FALSE;							/* Disable full parsing */
	rcode = GVParse(myexp, info);
	FullParse = TRUE;								/* Reenable full parsing */
	return rcode;
}

/* Parse for numerical arguments only */
GVCMDS *GVParse(char *myexp, GVPARSEINFO *info) {
	return GVParseEx(myexp, info, GVP_NUMERIC);
}

/* Parse for either numeric or string arguments */
GVCMDS *GVParseEx(char *myexp, GVPARSEINFO *info, GVP_PARSEMODE mode) {
	
	int  rcode=0;

	if (cmdlen == 0) {
		if ( (cmdstart = (GVCMDS *) malloc(2*CMD_BLOCK_SIZE)) == NULL) {
			ERRputs("(GVParse) Unable to allocate temporary memory\n");
			return(NULL);
		}
		cmdlen = 2;											/* cmdstart now has 2 block */
	}

/* Any sort of parse resets the math error count */
	gv_math_error_msg(NULL);
							 
/* Because of complex handling now, it is necessary to distinguish between a
-- very simple string expression consisting of simple characters and a complex
-- expression involving string operations.  Do by looking for operators and
-- function evaluations - note that the string override gives complex mode */
	SimpleName = (mode == GVP_STRING) && (strpbrk(myexp, "([{+/") == NULL);

	if (*myexp == '#') {
		if (mode == GVP_STRING) {
			SimpleName = FALSE;							/* Not even if otherwise */
			myexp++;
		} else {
			gv_err_msg("Evaluating a numeric eval but expression has string override");
			return(NULL);
		}
	}

	if ( (expstart = (char *) malloc(strlen(myexp)+1)) == NULL) {
		ERRputs("(GVParse) Unable to allocate temporary memory\n");
		return(NULL);
	}

	cmdinfo.type   = 0;									/* No options found			*/
	cmdinfo.length = 1;									/* Assume size is 1			*/
	paren_count = 0;										/* Check paren levels		*/
	 ArgBase =  ArgTop =  ArgNext = 0;				/* No args on arg stack		*/
	SArgBase = SArgTop = SArgNext = 0;				/* No args on arg stack		*/
	expr = expstart;										/* Start at beginning		*/
	cmd  = cmdstart + 4*sizeof(int);					/* Parse starts 4 ints in	*/

	rcode = copy_string(expstart, myexp);			/* Make a clean copy			*/

	if (rcode == 0) {										/* Successful, parse it		*/
		RecursionDepth = 0;								/* Limit recursion to 100	*/
		if (mode == GVP_NUMERIC) {
			rcode = get_number_value();
		} else {
			rcode = get_string_value();
		}
		if (rcode == 0) {									/* And get a value			*/
			if (*expr != '\0') {							/* Not at end of expression */
				gv_err_msg("Trailing garbage on expression");
				rcode = 1;
			} else {											/* Write ID, length (bytes), */
				int itmp, ilen;							/* type and array length	  */
				ilen = (int) (cmd-cmdstart+1);		/* # of commands				  */
				cmd = cmdstart;
				switch (mode) {
					case GVP_NUMERIC:
						cmdinfo.type |= GV_INFO_IS_NUMERIC_EXPR;
						break;
					case GVP_STRING:
						cmdinfo.type |= GV_INFO_IS_STRING_EXPR;
						break;
					case GVP_FILEPTR:
						cmdinfo.type |= GV_INFO_IS_FUNIT_EXPR;
						break;
					default:
						ERRprintf("(GVParse): Saw a mode (%d) that should not occur\n", mode);
						return(NULL);
				}
				itmp = MY_GV_ID; 				GVPUTITEM(cmd, itmp, int);
				itmp = ilen*sizeof(*cmd);	GVPUTITEM(cmd, itmp, int);
				itmp = cmdinfo.type;			GVPUTITEM(cmd, itmp, int);
				itmp = cmdinfo.length;		GVPUTITEM(cmd, itmp, int);
				if (info != NULL) *info = cmdinfo;		/* Return if interested   */
				if (GVMathMode & MATHDEBUG) TTYprintf("GVParse: info = %4.4x  length = %d\n", cmdinfo.type, cmdinfo.length);
				if ( (GVMathMode & MATHINFO) &&
					(cmdinfo.type & GV_INFO_INEQUALITY_COMPARISONS) &&
					( (GVMathMode & MATHCOMPLEX) || (cmdinfo.type & GV_INFO_COMPLEX_REFERENCE))
					) ERRputs("(GVParse) INFO: Comparisons in complex mode are by magnitude only\n");
			}
			if (paren_count != 0) 
				ERRputs("(GVParse) HUH? Despite best effort, parentheses don't balance\n");
			if (ArgBase != 0 || ArgTop != 0 || ArgNext != 0) 
				ERRprintf("(GVParse) HUH? Argument stack top (%d), base (%d) or next (%d) is not 0\n", ArgTop, ArgBase, ArgNext);
			if (SArgBase != 0 || SArgTop != 0 || SArgNext != 0) 
				ERRprintf("(GVParse) HUH? String argument stack top (%d), base (%d) or next (%d) is not 0\n", SArgTop, SArgBase, SArgNext);
		}
	}

	if (expstart != NULL) free(expstart);			/* Dump the memory used			*/
	return( (rcode == 0) ? cmdstart : NULL);		/* And return as appropriate	*/

}

/* ---------------------------------------------------------------------------
-- Routine makes a copy of a string, eliminating any spaces and making it
-- lower case.  But checks for string components and copies them exactly.
--
-- Usage:  int copy_string(char *dest, char *src)
--
-- Inputs: orig -- original string
--
-- Output: copy -- pointer to space for copy of orig
--
-- Returns: 0 ==> successful
--          1 ==> parentheses mismatch
--------------------------------------------------------------------------- */
PRIVATE int copy_string(char *dest, char *src) {

	char *iptr = src, *optr = dest;
	char achr;
	int  parens=0;
	char quotechar=0;

	do {
		achr = *(iptr++);				/* Get the character		*/
		if (quotechar) {				/* Are we quoted?			*/
			if (achr == quotechar) {
				if (*iptr != quotechar) {
					quotechar = 0;
				} else {
					*(optr++) = *(iptr++);
				}
			}
			*(optr++) = achr;

		} else if (achr == '\'' || achr == '\"') {
			*(optr++) = quotechar = achr;

		} else { 
			switch (achr) {		/* And interpret			*/
				case ' ':					/* 0x20 space				*/
				case '\t':					/* 0x09 horizontal tab	*/
				case '\n':					/* 0x0A linefeed			*/
				case '\v':					/* 0x0B vertical tab		*/
				case '\f':					/* 0x0C formfeed			*/
				case '\r':					/* 0x0D carriage return	*/
					break;
				case '(':					/* Any variety of open paren */
				case '{':
				case '[':
					parens++;
					*(optr++) = '(';
					break;
				case ')':					/* Any variety of close paren */
				case '}':
				case ']':
					if (--parens < 0) {
						gv_err_msg2("Unmatched close parenthesis", src, iptr-1);
						return(1);
					}
					*(optr++) = ')';
					break;
				default:						/* All others, just copy lower version */
					*(optr++) = (char) tolower(achr);
			}
		}
	} while (achr != '\0');

	if (parens != 0) {
		gv_err_msg2("Missing close parenthesis in expression", src, iptr-1);
		return(1);
	}
	if (quotechar != 0) {
		gv_err_msg2("Missing closing quote on string in expression", src, iptr-1);
		return(1);
	}

	return(0);
}

/* ---------------------------------------------------------------------------
-- Routine copies the next argument from a string, dealing properly with all
-- parenthesis and quoted strings.  Returns up to the next delimiter ',' or
-- close parenthesis.
--
-- Usage:  int gv_copy_argument(char *dest, char *src, char **endptr)
--
-- Inputs: orig -- original string
--
-- Output: copy -- pointer to space for copy of orig
--
-- Returns: 0 ==> successful
--          1 ==> parentheses mismatch
--------------------------------------------------------------------------- */
PRIVATE int gv_copy_argument(char *dest, char *src, char **endptr) {

	char *iptr=src, *optr=dest;
	char achr;
	int  parens=0;
	int  quotechar=0;

	while ( (achr = *iptr) != '\0') {				/* Get next character */
		if (quotechar != 0) {							/* Are we in a '"' or " string */
			if (achr == quotechar) quotechar = 0;
		} else if (achr == '\'' || achr == '\"') {
			quotechar = achr;
		} else if (achr == '(') {
			parens++;
		} else if (achr == ')' && parens > 0) {
			parens--;
		} else if (parens == 0 && (achr == ',' || achr == ')') ) {
			break;
		}
		*(optr++) = *(iptr++);
	}
	*optr = '\0';
	if (endptr != NULL) *endptr = iptr;				/* Points to next char after string */

	if (parens != 0) {
		gv_err_msg2("Missing close parenthesis in expression", src, iptr-1);
		return(1);
	}
	if (quotechar != 0) {
		gv_err_msg2("Missing closing quote on string in expression", src, iptr-1);
		return(1);
	}

	return(0);
}

		
/* ---------------------------------------------------------------------------
-- Simple routine to guarentee that there is at least a requested amount of
-- space left on the cmd parse stack.
--------------------------------------------------------------------------- */
int CheckCmdSpace(int min_size) {

	ptrdiff_t left, ilen;
	int newsize;
	GVCMDS *tmp;
	
	ilen = cmd-cmdstart;
	left = cmdlen*CMD_BLOCK_SIZE - ilen;
	if (left < min_size) {
		if (cmdlen >= CMD_BLOCK_LIMIT) {
			ERRputs("(GVParse) Parse size exceeded -- circular expression?\n");
			free(cmdstart); cmd=cmdstart=NULL; cmdlen=0;
			return(1);										/* Return failure */
		}
		TTYputs("(GVPARSE) INFO: Increasing parse stack size\n");
		newsize = (int) (cmdlen + 1 + (min_size-left)/CMD_BLOCK_SIZE);
		if ( (tmp=(GVCMDS *) realloc(cmdstart, newsize*CMD_BLOCK_SIZE)) == NULL) {
			ERRputs("(GVParse) Unable to increase parse size\n");
			return(1);
		}
		cmdlen = newsize;
		cmdstart = tmp; cmd = cmdstart + ilen;		/* Reset values		*/
	}
	return(0);
}


/* ---------------------------------------------------------------------------
-- Simple routine to interpret an escape character constant (characters
-- following the backslash).
--------------------------------------------------------------------------- */
PRIVATE int hex_to_int(char achr) {
	if (achr >= '0' && achr <= '9') return achr-'0';
	if (achr >= 'a' && achr <= 'f') return achr-'a'+10;
	if (achr >= 'A' && achr <= 'F') return achr-'A'+10;
	return 0;
}
	
PRIVATE int interp_escape(char *str, char **endptr) {

	int achr;

	achr = *(str++);										/* Get the next character */
	switch (achr) {
		case 'a':	achr = '\a'; break;
		case 'b':	achr = '\b'; break;
		case 't':	achr = '\t'; break;
		case 'n':	achr = '\n'; break;
		case 'f':	achr = '\f'; break;
		case 'r':	achr = '\r'; break;
		case 'v':	achr = '\v'; break;
#if 0
		case 34:		achr = 34;   break;			/* Double quote */
		case 39:		achr = 39;   break;			/* Single quote */
#endif
		case '\\':	achr = '\\'; break;
		case 'x':										/* Hexadecimal code */
			achr = 0;
			if (isxdigit(*str)) achr = 16*achr + hex_to_int(*(str++));
			if (isxdigit(*str)) achr = 16*achr + hex_to_int(*(str++));
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			achr -= '0';
			if (strchr("01234567", *str) != NULL) achr = achr*8 + *(str++) - '0';
			if (strchr("01234567", *str) != NULL) achr = achr*8 + *(str++) - '0';
			break;
	}
	if (endptr != NULL) *endptr = str;
	return(achr);
}

/* ---------------------------------------------------------------------------
-- Routine to get a variable name only as the next argument.  A name is
-- defined as a sequence of allowed characters to the next invalid
-- point.  The trailing , or ) separating arguments is eaten as well.
--------------------------------------------------------------------------- */
PRIVATE int get_name_value(char **name, int *isize) {
	int rc;
	char *endptr;
	
	*name = expr;
	if ( (rc = get_name(&endptr, NULL, isize)) != 0) return rc;
	expr = endptr;
	if (*expr == ',') {
		expr++;
	} else if (*expr == ')') {
		paren_count--;	expr++;
	}
	return 0;
}
	
/* ---------------------------------------------------------------------------
-- Routine to get a value.  A value is defined as numbers separated by
-- operators.  Calls get_number as necessary between operators.
--
-- Usage:  int get_number_value();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: 0 ==> successful parsing of a generalized value expression
--         >0 ==> any of several failures to parse.  Will print message on
--                any error it detects, simply returns if another routine
--                detects an error and returns non-zero (eg. get_number)
--
-- Modifies: cmdstart, cmd, expr, cmd, paren_count
--------------------------------------------------------------------------- */
#define	P_POWER			0x0F00	/* Power operations					** ^			*/
#define	P_UNARY			0x0E00	/* Unary operations					+ -			*/
#define	P_MUL				0x0D00	/* Multiplication precedence		* / %			*/
#define	P_ADD				0x0C00	/* Addition precedence				+ -			*/
#define	P_BIT				0x0B00	/* Bitwise Shift						<< >>			*/
#define	P_RELATE			0x0A00	/* Relational							> < <= >=	*/
#define	P_EQUAL			0x0900	/* equal, not equal					== !=			*/
#define	P_B_NOT			0x0800	/* bitwise .NOT.						~				*/
#define	P_B_AND			0x0700	/* bitwise .AND.						&				*/
#define	P_B_EOR			0x0600	/* bitwise .EOR. (exclusive or)	^				*/
#define	P_B_OR			0x0500	/* bitwise .OR.						|				*/
#define	P_L_NOT			0x0400	/* logical .NOT.						(no C form)	*/
#define	P_L_AND			0x0300	/* logical .AND.						&&				*/
#define	P_L_OR			0x0200	/* logical .OR.						||				*/
#define	P_L_EQV			0x0100	/* logical .EQV. and .NEQV.		(no C form)	*/
#define	P_CONDITIONAL	0x00FF

PRIVATE int get_number_value(void) {
	
	int pending_list[20];								/* Pending operations */
	int *pending=pending_list;
	int opcode, rcode=0;
	
/* Check the recursion depth */
	if (RecursionDepth++ > MAX_FUNCTION_DEPTH) {
		ERRprintf("(GVPARSE) Maximum function depth of %d exceeded\n", MAX_FUNCTION_DEPTH);
		return(1);
	}
	*pending = 0;											/* Push a "NOP" on pending */

	do {
		if ( (rcode = CheckCmdSpace(CMD_BLOCK_EXPAND)) != 0) return(rcode);

		switch (*expr) {								/* First, check qualifiers */
			case '\0':
				gv_err_msg("Reached end of expression while looking for a number");
				return(1);
			case '+':									/* Unary plus? */
				expr++; break;
			case '-':									/* Unary minus? */
				expr++; *(++pending) = P_UNARY | chs_me; break;
			case '!':
				expr++; *(++pending) = P_L_NOT | not_me; break;
			case '~':
				cmdinfo.type |= GV_INFO_BITWISE_OPERATIONS;
				expr++; *(++pending) = P_B_NOT | bit_not_me; break;

			case '.':									/* Potential .NOT. symbol	*/
				if (strncmp(expr, ".not.", 5) != 0) break;
				*(++pending) = P_L_NOT | not_me;	/* Push .NOT. command			*/
				expr += 5;								/* Jump over .not.				*/
				break;
		}

		if ( (rcode = get_number()) != 0) return(rcode);

/* Now, check special case of factorial (which may instead be a != symbol) */
		if (*expr == '!') {							/* Check on factorial!			*/
			if (expr[1] != '=') {					/* But also allow !=				*/
				GVPUTCMD(cmd, fact_me);				/* Immediate factorial op		*/
				expr++;
			}
		}

/* There is a painful problem of 2.ne.2 which gets read as (2.)ne.2 and
   fails.  Special code backs up over a . if at alphanumeric now */
		if (isalpha(*expr) && expr != expstart) {
			if (*(--expr) != '.') expr++;
		}

/* Now, get an operator, or the end of operations */
		opcode = 0;											/* At moment, no opcode		*/
		switch (*expr) {
			case '\0':										/* End of whole che-bang!	*/
				break;
			case ')':										/* End of sub-expression	*/
				expr++; paren_count--; break;
			case ':':										/* Middle of conditional	*/
			case '#':										/* Middle of conditional	*/
				expr++; break;
			case ',':										/* End of element express	*/
				expr++; break;
			case '?':										/* Conditional operation	*/
				expr++;
				opcode = P_CONDITIONAL; break;		/* Will exit!					*/
			case '+':
				expr++; opcode = P_ADD   | add_me; break;	/* Add operation			*/
			case '-':
				expr++; opcode = P_ADD   | sub_me; break;	/* Subtract operation	*/
			case '*':
				expr++; opcode = P_MUL   | mul_me;
				if (*expr == '*') {opcode = P_POWER | pow_me; expr++;}
				break;
			case '/':
				expr++; opcode = P_MUL   | div_me; break;	/* Divide operation		*/
			case '&':
				expr++;								
				if (*expr == '&') {								/* Logical AND				*/
					expr++;
					opcode = P_L_AND | and_me;
					cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				} else {												/* Bitwise AND				*/
					opcode = P_B_AND | bit_and_me;
					cmdinfo.type |= GV_INFO_BITWISE_OPERATIONS;
				}
				break;	
			case '|':
				expr++;								
				if (*expr == '|') {								/* Logical OR				*/
					expr++;
					opcode = P_L_OR  | or_me;					/* Or operation			*/
					cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				} else {												/* Bitwise OR				*/
					opcode = P_B_OR | bit_or_me;
					cmdinfo.type |= GV_INFO_BITWISE_OPERATIONS;
				}
				break;
			case '^':
				expr++; opcode = P_POWER | pow_me;
				if (*expr == '=') {opcode = P_EQUAL | ne_me; expr++;}
				break;
			case '<':
				expr++; opcode = P_RELATE | lt_me;
				if (*expr == '=') {
					opcode = P_RELATE | le_me; expr++;
				} else if (*expr == '>') {
					opcode = P_RELATE | ne_me; expr++;
				}
				if ((opcode & 0xFF) != ne_me) cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				break;
			case '>':
				expr++; opcode = P_RELATE | gt_me;
				if (*expr == '=') {
					opcode = P_RELATE | ge_me; expr++;
				} else if (*expr == '<') {
					opcode = P_RELATE | ne_me; expr++;
				}
				if ((opcode & 0xFF) != ne_me) cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				break;
			case '=':
				expr++;
				if      (*expr == '=') opcode = P_EQUAL  | eq_me;
				else if (*expr == '>') opcode = P_RELATE | ge_me;
				else if (*expr == '<') opcode = P_RELATE | le_me;
				else     goto ParseError;
				if ((opcode & 0xFF) != eq_me) cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				expr++;
				break;
			case '!':											/* Only allow != */
				if (expr[1] != '=')  goto ParseError;
				expr += 2; opcode = P_EQUAL | ne_me; break;
			case '.':									/* Potential logicals */
				     if (strncmp(expr, ".lt." ,  4) == 0)  opcode = P_RELATE | lt_me;
				else if (strncmp(expr, ".le." ,  4) == 0)  opcode = P_RELATE | le_me;
				else if (strncmp(expr, ".ne." ,  4) == 0)  opcode = P_EQUAL  | ne_me;
				else if (strncmp(expr, ".gt." ,  4) == 0)  opcode = P_RELATE | gt_me;
				else if (strncmp(expr, ".ge." ,  4) == 0)  opcode = P_RELATE | ge_me;
				else if (strncmp(expr, ".eq." ,  4) == 0)  opcode = P_EQUAL  | eq_me;
				else if (strncmp(expr, ".or." ,  4) == 0)  opcode = P_L_OR   | or_me;
				else if (strncmp(expr, ".and.",  5) == 0) {opcode = P_L_AND  | and_me;  expr++;}
				else if (strncmp(expr, ".eqv.",  5) == 0) {opcode = P_L_EQV  | eqv_me;  expr++;}
				else if (strncmp(expr, ".neqv.", 6) == 0) {opcode = P_L_EQV  | neqv_me; expr += 2;}
				else    goto ParseError;
				expr += 4;
				if ((opcode & 0xFF) != eq_me && (opcode & 0xFF) != ne_me) 
					cmdinfo.type |= GV_INFO_INEQUALITY_COMPARISONS;
				break;
			default:
				goto ParseError;
		}

/* ----------------------------------------------------------------------
-- Okay, now we have an operation (possibly) and a pending stack.
-- Walk down the pending stack as long as what is to be done is higher priority
-- than what we are about to do (ie. all *'s get done if we have a +
---------------------------------------------------------------------------- */
		while (*pending != 0 && (*pending & 0x7F00) >= (opcode & 0x7F00) ) {
			GVPUTCMD(cmd, *pending & 0x00FF);				/* Pop codes as ok */
			pending--;												/* WARNING: Macro side effect above */
		}
		*(++pending) = opcode;									/* Push new opcode */

	} while ( (opcode & 0x7F00) != 0);

	if (opcode == P_CONDITIONAL) {							/* Handle conditional */
		ptrdiff_t l1, l2, ltmp;
		GVCMDS *c1;
		GVPUTCMD(cmd, conditional_me);
		l1   = cmd-cmdstart;										/* Jump position */
		cmd += sizeof(ptrdiff_t);		
		if ( (rcode=get_number_value()) != 0) return(rcode);
		if (*(expr-1) != ':' && *(expr-1) != '#') {
			gv_err_msg("Conditional missing  : or #.  Use (a) ? (b) # (c)");
			return(1);
		}
		GVPUTCMD(cmd, jump_me);									/* Jump over next */
		l2   = cmd-cmdstart;
		cmd += sizeof(ptrdiff_t);		
		if ( (rcode=get_number_value()) != 0) return(rcode);
		c1 = cmdstart + l1;										/* Point to 1st jump */
		ltmp = l2-l1; 							GVPUTITEM(c1, ltmp, ptrdiff_t);
		c1 = cmdstart + l2;										/* Point to 2nd jump */
		ltmp = cmd-c1-sizeof(ptrdiff_t);	GVPUTITEM(c1, ltmp, ptrdiff_t);
	}

	*cmd = 0xFF;													/* Mark as end now		*/
	RecursionDepth--;												/* Pop recursion level	*/
	return(0);														/* We have finished		*/

ParseError:
	gv_err_msg("Could not see a valid operator at this point\n"); 
	return(2);
}


/* ---------------------------------------------------------------------------
-- Routine to get a number.  A number is defined as a single value which may
-- be either an actual number, an expression enclosed in parentheses, or a
-- function call.  Calls get_value, get_function, etc. as necessary.
--
-- Usage:  int get_number();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: 0 ==> successful loading of a generalized number
--          1 ==> any of several failures to parse.  Will print message on
--                any error it detects, simply returns if another routine
--                detects an error and returns non-zero (eg. get_value)
--
-- Modifies: expr, cmd, paren_count
--
-- Expects either: (     ==> expression
--                 &     ==> stack value
--                 [0-9] ==> actual numeric value
--                 [a-z] ==> function of some type
--------------------------------------------------------------------------- */
PRIVATE int get_number(void) {

	char *endptr;
	int i,quotechar,rcode,magic,len;
	TMPREAL xtmp;

/* On parenthesis, go get a full expression value */
	if (*expr == '(') {
		expr++;
		paren_count++;								/* Recursive call for value */
		if ( (rcode=get_number_value()) != 0) return(rcode);
		if (*(expr-1) != ')') {
			gv_err_msg("Expression beginning with an ( did not terminate with a )");
			return(1);
		}

/* Allow single character constant like 'a' or '\x7A' or '\n' */
	} else if (*expr == '\'' || *expr == '\"') {	/* Single character constants */
		quotechar = *(expr++);
		if (*expr == '\\') {
			expr++;
			xtmp = (TMPREAL) interp_escape(expr, &expr);
		} else {
			xtmp = (TMPREAL) *(expr++);
		}
		if (*expr != quotechar) {
			gv_err_msg("Invalid single character constant");
			return(1);
		}
		expr++;											/* And we are done with this */
		GVPUTCMD(cmd, load_immed_real);
		GVPUTITEM(cmd, xtmp, TMPREAL);			/* Put the value xtmp on stack */
		
/* Internal stack reference (internal functions) - &1 */
	} else if (*expr == STACK_REF_CHAR) {		/* Stack reference */
		expr++;
		if (*expr == 's') {
			expr++;
			i = (int) strtol(expr, &expr, 10) + SArgBase;
			if (i >= SArgTop) {
				gv_err_msg("Stack variable reference outside current range");
				TTYprintf("i: %d, SArgBase: %d  SArgTop: %d\n", i, SArgBase, SArgTop);
				return(1);
			}
			GVPUTCMD(cmd, interp_string_stack_val);
			*(cmd++) = (unsigned char) i;
		} else {
			i = (int) strtol(expr, &expr, 10) + ArgBase;
			if (i >= ArgTop) {
				gv_err_msg("Stack variable reference outside current range");
				TTYprintf("i: %d, ArgBase: %d  ArgTop: %d\n", i, ArgBase, ArgTop);
				return(1);
			}
			GVPUTCMD(cmd, load_stack_val);
			*(cmd++) = (unsigned char) i;
		}

/* Simple hexadecimal constant */
	} else if (*expr == '0' && expr[1] == 'x') {	
		xtmp = strtol(expr+2, &expr, 16);	/* Will be interpreted hexadecimal */
		if (tolower(*expr)=='i' || tolower(*expr)=='j') {	/* Imaginary load? */
			expr++;
			GVPUTCMD(cmd, load_immed_imag);
			cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
		} else {
			GVPUTCMD(cmd, load_immed_real);
		}
		GVPUTITEM(cmd, xtmp, TMPREAL);			/* Put the value xtmp on stack */

/* Simple numbers 0.18472 */
	} else if (isdigit(*expr) || *expr == '.') {
#ifdef BAD_STRTOD_FNC							/* Truly is a bug in many libraries */
	   int ii;
	   #ifdef TMPREAL_IS_LONG
	     sscanf(expr, "%Lg%n", &xtmp, &ii);
	   #else
	     sscanf(expr, "%lg%n", &xtmp, &ii);
	   #endif
		ii = min(ii, strlen(expr));
		expr += ii;
#else
		xtmp = STRTOD(expr, &expr);				/* Try to get a value */
#endif
		if (tolower(*expr)=='i' || tolower(*expr)=='j') {	/* Imaginary load? */
			expr++;
			GVPUTCMD(cmd, load_immed_imag);
			cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
		} else if (*expr == '%' && (expr[1] == '\0' || strchr("+-*/^|&#,:<>=!.)",expr[1]) != NULL)) {
			xtmp /= 100.0;
			expr++;
			GVPUTCMD(cmd, load_immed_real);
		} else {
			GVPUTCMD(cmd, load_immed_real);
		}
		GVPUTITEM(cmd, xtmp, TMPREAL);			/* Put the value xtmp on stack */

/* Try as a name in the variable or function space */
	} else if (get_name(&endptr, &magic, &len) == 0) {
		if ( (rcode=get_var(magic, len, endptr, GVP_NUMERIC)) >= 0)		/* Try user fncs */
			return(rcode);	
		if ( (rcode=get_internal_fnc(magic, len, endptr, GVP_NUMERIC)) >= 0)		/* Try internal fncs */
			return(rcode);	
		gv_err_msg("Could not interpret string as a function or variable (get_number)");
		return(1);

/* Otherwise, give up as an error */
	} else {
		gv_err_msg("Illegal character found when expecting #, fnc or var (get_number)");
		TTYprintf("0x%2.2x\n", *expr);
		return(2);
	}

	return(0);
}

/* ---------------------------------------------------------------------------
-- Routine to get a string value.  A value is defined as numbers separated by
-- operators.  Calls get_number as necessary between operators.
--
-- Usage:  int get_string_value();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: 0 ==> successful parsing of a generalized value expression
--         >0 ==> any of several failures to parse.  Will print message on
--                any error it detects, simply returns if another routine
--                detects an error and returns non-zero (eg. get_number)
--
-- Modifies: cmdstart, cmd, expr, cmd, paren_count
--------------------------------------------------------------------------- */
#define	P_CONCAT			0x0E00	/* concatenate operator				//				*/

PRIVATE int get_string_value(void) {
	
	int pending_list[20];								/* Pending operations */
	int *pending=pending_list;
	int opcode, rcode=0;
	
/* Check the recursion depth */
	if (RecursionDepth++ > MAX_FUNCTION_DEPTH) {
		ERRprintf("(GVPARSE) Maximum function depth of %d exceeded\n", MAX_FUNCTION_DEPTH);
		return(1);
	}

/* The stack now includes string references */
	cmdinfo.type |= GV_INFO_STRING_REFERENCE;		/* We are using strings! */
	*pending = 0;											/* Push a "NOP" on pending */

	do {
		if ( (rcode = CheckCmdSpace(CMD_STRING_EXPAND)) != 0) return(rcode);

		if (*expr == '\0') {
			gv_err_msg("Reached end of expression while looking for a string");
			return(1);
		}

		if ( (rcode = get_string()) != 0) return(rcode);

/* Now, get an operator, or the end of operations */
		opcode = 0;											/* At moment, no opcode		*/
		switch (*expr) {
			case '\0':										/* End of whole che-bang!	*/
				break;
			case ')':										/* End of sub-expression	*/
				expr++; paren_count--; break;
			case ',':										/* End of element express	*/
				expr++; break;
			case '/':
				expr++; opcode = P_CONCAT | rexx_concat ;
				if (*expr != '/') {
					gv_err_msg("Expecting concatenation operator // but second slash not seen");
					return(1);
				}
				expr++;
				break;
			case '+':
				expr++; opcode = P_CONCAT | rexx_concat ;
				break;

			default:
				goto ParseError;
		}

/* ----------------------------------------------------------------------
-- Okay, now we have an operation (possibly) and a pending stack.
-- Walk down the pending stack as long as what is to be done is higher priority
-- than what we are about to do (ie. all *'s get done if we have a +
---------------------------------------------------------------------------- */
		while (*pending != 0 && (*pending & 0x7F00) >= (opcode & 0x7F00) ) {
			GVPUTCMD(cmd, *pending & 0x00FF);				/* Pop codes as ok */
			pending--;												/* WARNING: Macro side effect above */
		}
		*(++pending) = opcode;									/* Push new opcode */

	} while ( (opcode & 0x7F00) != 0);

	*cmd = 0xFF;													/* Mark as end now		*/
	RecursionDepth--;												/* Pop recursion level	*/
	return(0);														/* We have finished		*/

ParseError:
	gv_err_msg("Could not see a valid operator at this point (get_string_value)\n"); 
	return(2);
}


/* ---------------------------------------------------------------------------
-- Routine to get a string.  A string is defined as a single 'value' which may
-- be either an actual embedded string, an expression enclosed in parenthesis
-- which must resolve to a string, a string variable, or a function call
-- resolving to a string. Calls other functions as needed.
--
-- Usage:  int get_string();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: 0 ==> successful loading of a generalized number
--          1 ==> any of several failures to parse.  Will print message on
--                any error it detects, simply returns if another routine
--                detects an error and returns non-zero (eg. get_string_value)
--
-- Modifies: expr, cmd, paren_count, itype
--
-- Expects either: [a-z] ==> string name (must resolve as simple var)
--                 "xxx" ==> local string
--                 'yyy' ==> local string
--
-- Notes: (1) Does either a "load_string" with the string copied to the cmd array
--            for local use, or a "load_string_ptr" if the reference is to a var.
--        (2) Sets "STRING_REFERENCE" in the type flag.
--        (3) No check is done for ludicrously long fixed strings.
--------------------------------------------------------------------------- */
#define	C_STRING_IDENTIFIER	'`'

PRIVATE int get_string(void) {

	char quotechar, *endptr;
	int i,achr,rcode,magic,len;
	BOOL C_String;									/* Should handle as C string? */

/* On parenthesis, go get a full expression value */
	if (*expr == '(') {
		expr++;
		paren_count++;								/* Recursive call for value */
		if ( (rcode=get_string_value()) != 0) return(rcode);
		if (*(expr-1) != ')') {
			gv_err_msg("Expression beginning with an ( did not terminate with a )");
			return(1);
		}

/* Accept string constants "xxxx" */
	} else if (*expr == '\"' || *expr == '\'') {	/* Have a quoted string */
		quotechar = *(expr++);
		GVPUTCMD(cmd, load_string_tmp);					/* Load local string */

		/* Possible C string if starting with a " */
		C_String = FALSE;
		if (quotechar == '\"' && *expr == C_STRING_IDENTIFIER) {		/* Might be a C-string	*/
			if (expr[1] == C_STRING_IDENTIFIER) {							/* Logical escape			*/
				expr++;
			} else if (expr[1] != '\0') {										/* And not trivial str	*/
				expr++;
				C_String = TRUE;
			}
		}

		while ( (achr = *expr) ) {
			expr++;											/* Go to the next one		*/
			if (achr == quotechar) {					/* Are we done?				*/
				if (*expr != quotechar) break;		/* Okay, end of string		*/
				expr++;										/* Otherwise, just put in	*/
			} else if (C_String && achr=='\\' && *expr) {	/* Escaped char	*/
				achr = interp_escape(expr, &expr);
			}
			*(cmd++) = achr;								/* And store the value */
		}
		*(cmd++) = '\0';									/* Terminate the string */

/* Internal stack reference (internal functions) - &1 */
	} else if (expr[0] == STACK_REF_CHAR && expr[1] == 's') {	/* Stack reference */
		expr += 2;
		i = (int) strtol(expr, &expr, 10) + ArgBase;
		if (i >= SArgTop) {
			gv_err_msg("String stack variable reference outside current range");
			TTYprintf("i: %d, SArgBase: %d  SArgTop: %d\n", i, SArgBase, SArgTop);
			return(1);
		}
		GVPUTCMD(cmd, load_string_stack_val);
		*(cmd++) = (unsigned char) i;

/* Try as a name in the variable or function space */
	} else if (get_name(&endptr, &magic, &len) == 0) {
		if ( (rcode=get_var(magic, len, endptr, GVP_STRING)) >= 0) {						/* Try user fncs */
			return rcode;
		} else if ( (rcode=get_internal_fnc(magic, len, endptr, GVP_STRING)) >= 0) {	/* Try internal fncs */
			return rcode;
		} else {
			gv_err_msg("Could not interpret string as a function or variable (get_string)");
			return(1);
		}

/* Otherwise, give up as an error */
	} else {
		gv_err_msg("Illegal character found when expecting string expression (get_string)");
		return(2);
	}

	return(0);
}


/* ---------------------------------------------------------------------------
-- Routine to get a generic pointer of specified type.  A pointer can only be
-- a variable - all other uses are invalid.
--
-- Usage:  int get_pointer_value(GVP_PARSEMODE type);
--
-- Inputs: mode - mode to accept variables
--                      GVP_FILEPTR   - get a FILE * pointer
--                      GVP_STRINGPTR - get a STRING pointer
--
-- Output: none
--
-- Returns: 0 ==> successful loading of a pointer
--          1 ==> any type of failure
--
-- Modifies: expr, cmd, paren_count, itype
--
-- Expects either: [a-z] ==> string name (must resolve as simple var)
--
-- Notes: (1) Does an appropriate "load_pointer" to actual pointer
--------------------------------------------------------------------------- */
PRIVATE int get_pointer_value(GVP_PARSEMODE mode) {
	
	int rcode=0;
	
/* The stack now includes string references - file and string pointers on string stack */
	cmdinfo.type |= GV_INFO_STRING_REFERENCE;		/* We are using strings! */

	if ( (rcode = CheckCmdSpace(CMD_STRING_EXPAND)) != 0) return(rcode);

	if (*expr == '\0') {
		gv_err_msg("Reached end of expression while looking for a pointer");
		return(1);
	}

	if ( (rcode = get_pointer(mode)) != 0) return(rcode);

/* Now, get an operator, or the end of operations */
	switch (*expr) {
		case '\0':										/* End of whole che-bang!	*/
			break;
		case ')':										/* End of sub-expression	*/
			expr++; paren_count--; break;
		case ',':										/* End of element express	*/
			expr++; break;
		default:
			goto ParseError;
	}

	return(0);														/* We have finished		*/

ParseError:
	gv_err_msg("Could not see a valid pointer operator at this point\n"); 
	return(2);
}


/* ---------------------------------------------------------------------------
-- Routine to get a pointer.  A pointer can only be a variable reference.
--
-- Usage:  int get_pointer();
--
-- Inputs: none
--
-- Output: none
--
-- Returns: 0 ==> successful loading of a generalized number
--          1 ==> any of several failures to parse.  Will print message on
--                any error it detects, simply returns if another routine
--                detects an error and returns non-zero (eg. get_string_value)
--
-- Modifies: expr, cmd, paren_count, itype
--
-- Expects either: [a-z] ==> pointer variable  name (must resolve as simple var)
--
-- Notes: (1) Does a "load_pointer" as appropriate
--------------------------------------------------------------------------- */
PRIVATE int get_pointer(GVP_PARSEMODE mode) {

	char *endptr;
	int rcode,magic,len;
	FILE **pfunit;
	static FILE *is_stdout, *is_stdin, *is_stderr;

/* Try as a name in the variable or function space.  Abandon if can't get name */
	if (get_name(&endptr, &magic, &len) != 0) {
		gv_err_msg("Illegal character found when expecting pointer expression");
		return(2);
	}

	if ( (rcode=get_var(magic, len, endptr, mode)) >= 0) {	/* Try user fncs */
		return(rcode);
	} else if (mode != GVP_FILEPTR) {			/* If not FILEPTR, can't do anything */
		gv_err_msg("Could not interpret string as a function or variable");
		return(1);
	}

/* Special case handling for GVP_FILEPTR to allow stdin, stdout and stderr */
	if (len == 6 && strnicmp(expr, "stdout", len) == 0) {
		is_stdout = stdout; pfunit = &is_stdout;
	} else if (len == 6 && strnicmp(expr, "stderr", len) == 0) {
		is_stderr = stderr; pfunit = &is_stderr;
	} else if (len == 5 && strnicmp(expr, "stdin", len) == 0) {
		is_stdin  = stdin;  pfunit = &is_stdin;
	} else {
		gv_err_msg("Could not identify string as a file pointer or equivalent variable");
		return 1;
	}
	GVPUTCMD(cmd, load_pfile_ptr);			/* Load a file pointer */
	GVPUTITEM2(cmd, pfunit, void *, void **);
	expr = endptr;
	return 0;
}

/* ---------------------------------------------------------------------------
-- Routine to look up an entry in the allocation table.  Returns the address
-- of the entry structure if successful.
--
-- Usage:  GV_ENTRY *get_entry(char **endptr);
--
-- Inputs: none
--
-- Output: *endptr - set the the pointer which would be after the string
--                   This is set whether or not the name is found.
--          If endptr is NULL, it is not set.
--
-- Modifies: expr
--
-- Returns:  NULL ==> no entry of that name exists
--          !NULL ==> entry found and structure address returned
--------------------------------------------------------------------------- */
PRIVATE GV_ENTRY *get_entry(char **endptr) {

	char *ptr;
	int magic, len, rc;
	GV_ENTRY *entry=GVFirstEntry;

	rc = get_name(&ptr, &magic, &len);						/* Is there validity? */
	if (endptr != NULL) *endptr = ptr;
	if (rc != 0) return(NULL);									/* No valid name */

	do {
		if (entry->magic == magic)	{							/* Reasonable?			*/
			if (strnicmp(expr,entry->varname,len)==0) {	/* Really valid?		*/
				expr = ptr;											/* YES -- update ptr	*/
				return(entry);										/* And return			*/
			}
		}
	} while ( (entry=entry->next) != NULL );				/* Walk through list */
	return(NULL);
}

/* ---------------------------------------------------------------------------
-- Routine to check expr for a name of function or variable.  Identifies the
-- extent of the name and returns pointers to the end of the name, the length,
-- and the magic number of the name.
--
-- Usage:  int get_name(char **endptr, int *magic, int *len);
--
-- Inputs: appropriate pointers
--
-- Output: *endptr - pointer to next character after name
--         *magic  - magic value of the name located
--         *len    - length of the name
--
-- Returns: -1 ==> current expr does not point to a valid name
--           0 ==> successful location of a name
--
-- Modifies: nothing
--
-- Notes: names must start with alpha or one of [$,@].  All characters must be
--        either alpha or in set [_,$,:]
--------------------------------------------------------------------------- */
PRIVATE int get_name(char **endptr, int *magic, int *len) {
	
	char *ptr=expr;

	if ( ! ( isalpha(*ptr) || *ptr=='$' || *ptr=='@' || *ptr == '_') ) {
		*endptr = ptr;
		return(-1);
	}

	if (magic != NULL) *magic = *ptr;
	ptr++;
	while ( isalnum(*ptr) || *ptr=='_' || *ptr=='$' || *ptr==':' ) {
		if (magic != NULL) *magic = MAGIC(*magic,*ptr);
		ptr++;
	}

	*len = (int) (ptr-expr);
	*endptr = ptr;
	return(0);
}

#define	RETURN_STRING	0x1000	/* Pattern saying returns string				*/
#define	RETURN_FILEPTR	0x2000	/* Pattern saying returns pointer			*/

#define	IS_CONST				0x01	/* Constant load									*/
#define	IS_CMPLX				0x02	/* Imaginary constant load						*/
#define	IS_INT_VALUE		0x03	/* Load integer constant (from nargs)		*/

#define	IS_MULTI				0x04	/* Function (min/max) of arbitrary # of args */
#define	IS_ARBITRARY		0x05	/* Functions of arbitrary args not min/max */
#define	IS_FNC				0x06	/* Function of fixed # of arguments			*/
#define	IS_CFNC				0x07	/* Complex function of fixed # of args		*/

#define	IS_R_ARRAY			0x08	/* Function of array/curve + n args			*/
#define	IS_R2_ARRAY			0x09	/* Function of two arrays + n args			*/
#define	IS_CURVE_EVAL		0x0A	/* Curve function evaluated locally			*/
#define	IS_QUERY				0x0B	/* Information on entry only					*/
	#define	SIZEOF_OP			0x01
	#define	TYPEOF_OP			0x02
	#define	EXISTS_OP			0x03

#define	IS_SURF_EVAL		0x0C	/* Curve function evaluated locally			*/
#define	IS_FUNCTION_EVAL	0x0D	/* @solve function - painful coding			*/

#define	IS_SPC				0x0E	/* Special function								*/
	#define	IS_T_TEST			0x01	/* @t_test(a1,a2)		Normal Student t-test */
	#define	IS_TD_TEST			0x02	/* @td_test(a1,a2)	Dependent sample t-test */
	#define	IS_F_TEST			0x03	/* @f_test(a1,a2) */
	#define	IS_U_TEST			0x04	/* @u_test(a1,a2)	*/
	#define	IS_Z_TEST			0x05	/* @z_test(a1,x0,s) */
#define	IS_FPRINTF			0x0F	/* fread, fwrite, fclose, etc. */

typedef enum {F_CONST, F_BASIC, F_TRIG, F_SPEC, F_FUNC, F_STAT, F_POLY, F_VAR, F_STR, F_REXX, F_FILE, F_CONV, F_OS} FNC_CLASS;

typedef struct _FNC_DEF {
	char	*name;						/* Character name of function */
	int	magic;						/* Magic code for hashing		*/
	int	fnctype;						/* Function type (operation)	*/
	int	nargs;						/* Number of arguments			*/
	const char	*argtypes;			/* List of argtypes by character */
	int	opcode;						/* Opcode (load to GVCMDS)		*/
	FNC_CLASS type;					/* Class of function				*/
	const char	*usage;
	const char	*desc;				/* Description of the function */
} FNC_DEF;

PRIVATE FNC_DEF fncs[] = {
	{"e",				0, IS_CONST,								0,			NULL,		load_e,				F_CONST,	"e",									"2.71828..."},
	{"pi",			0, IS_CONST,								0,			NULL,		load_pi,				F_CONST,	"pi",									"3.14159..."},
	{"i",				0, IS_CONST,								0,			NULL,		load_i,				F_CONST,	"i",									"array index"},
	{"j",				0, IS_CMPLX,								0,			NULL,		load_j,				F_CONST, "j",									"sqrt(-1)"},
	{"REAL_MIN",	0, IS_CONST,								0,			NULL,		load_realmin,		F_CONST,	"REAL_MIN",							"Smallest representable floating point value"},
	{"REAL_MAX",	0, IS_CONST,								0,			NULL,		load_realmax,		F_CONST, "REAL_MAX",							"Largest representable floating point value"},

	{"abs",			0, IS_FNC,									0,			"z",		abs_me,				F_BASIC,	"abs(x)",							"absolute value"},
	{"fabs",			0, IS_FNC,									0,			"z",		abs_me,				F_BASIC,	"fabs(x)",							"absolute value"},
	{"magn",			0, IS_FNC,									0,			"z",		abs_me,				F_BASIC,	"magn(x)",							"absolute value (especially complex numbers)"},
	{"sign",			0, IS_FNC,									0,			"z",		sign_me,				F_BASIC,	"sign(x)",							"-1,0,1 for x<0, x=0, x>0"},
	{"min",			0, IS_MULTI,								0,			NULL,		min_me,				F_BASIC,	"min(a,b,...)",					"minimum of list"},
	{"max",			0, IS_MULTI,								0,			NULL,		max_me,				F_BASIC,	"max(a,b,...)",					"maximum of list"},
	{"ave",			0, IS_ARBITRARY,							0,			NULL,		ave_me,				F_BASIC,	"ave(a,b,...)",					"average of list"},
	{"avg",			0, IS_ARBITRARY,							0,			NULL,		ave_me,				F_BASIC,	"avg(a,b,...)",					"average of list"},
	{"average", 	0, IS_ARBITRARY,							0,			NULL,		ave_me,				F_BASIC,	"average(a,b,..)",				"average of list"},
	{"mean",			0, IS_ARBITRARY,							0,			NULL,		ave_me,				F_BASIC,	"mean(a,b,...)",					"average of list"},
	{"std",		 	0, IS_ARBITRARY,							0,			NULL,		std_me,				F_BASIC,	"std(a,b,...)",					"standard deviation of list"},
	{"sdev",			0, IS_ARBITRARY,							0,			NULL,		std_me,				F_BASIC,	"sdev(a,b,...)",					"standard deviation of list"},
	{"stdev",		0, IS_ARBITRARY,							0,			NULL,		std_me,				F_BASIC,	"stdev(a,b,...)",					"standard deviation of list"},
	{"stddev",		0, IS_ARBITRARY,							0,			NULL,		std_me,				F_BASIC,	"stddev(a,b,...)",				"standard deviation of list"},
	{"sdom",			0, IS_ARBITRARY,							0,			NULL,		sdom_me,				F_BASIC,	"sdom(a,b,...)",					"standard deviation the mean of list"},
	{"median",		0, IS_ARBITRARY,							0,			NULL,		median_me,			F_BASIC,	"median(a,b,...)",				"median of list"},
	{"mad",			0, IS_ARBITRARY,							0,			NULL,		mad_me,				F_BASIC,	"mad(a,b,...)",					"MAD of list"},
	{"count",		0, IS_ARBITRARY,							0,			NULL,		count_me,			F_BASIC,	"count(a,b,...)",					"number of items in list"},
	{"limit",		0, IS_FNC,									0,			"rrr",	limit_me,			F_BASIC,	"limit(x,low,high)",				"x if low<x<high, otherwise low or high"},
	{"int",			0, IS_FNC,									0,			"r",		int_me,				F_BASIC,	"int(x)",							"integer part of x"},
	{"nint",			0, IS_FNC,									0,			"r",		nint_me,				F_BASIC,	"nint(x)",							"nearest integer to x"},
	{"frac",			0, IS_FNC,									0,			"r",		frac_me,				F_BASIC,	"frac(x)",							"fractional part of x, with sign preserved"},
	{"ceil",			0, IS_FNC,									0,			"r",		ceil_me,				F_BASIC,	"ceil(x)",							"smallest integer greater than x"},
	{"floor",		0, IS_FNC,									0,			"r",		floor_me,			F_BASIC,	"floor(x)",							"largest integer less than x"},
	{"mod",			0, IS_FNC,									0,			"rr",		mod_me,				F_BASIC,	"mod(x,y)",							"X mod Y returns remainder of X/Y"},
	{"fmod",			0, IS_FNC,									0,			"rr",		mod_me,				F_BASIC,	"fmod(x,y)",						"X mod Y returns remainder of X/Y"},
	{"mantissa",	0, IS_FNC,									0,			"r",		mantissa_me,		F_BASIC,	"mantissa(x)",						" Returns the mantissa from scientific notation"},
	{"exponent",	0, IS_FNC,									0,			"r",		exponent_me,		F_BASIC,	"exponent(x)",						" Returns the exponent from scientific notation"},
	{"m1n",			0, IS_FNC,									0,			"i",		m1n_me,				F_BASIC,	"m1n(n)",							"(-1)^n done efficiently"},

	{"magn",			0, IS_CFNC,									0,			"z",		abs_me,				F_BASIC,	"magn(x)",							"absolute value (especially complex numbers)"},
	{"real",			0, IS_CFNC,									0,			"z",		real_me,				F_BASIC,	"real(z)",							"Real part (of a complex number)"},
	{"imag",			0, IS_CFNC,									0,			"z",		imag_me,				F_BASIC,	"imag(z)",							"Imaginary part (of a complex number)"},
   {"conj",			0, IS_CFNC,									0,			"z",		conj_me,				F_BASIC,	"conj(z)",							"Complex conjugate of z"},
	{"arg",			0, IS_CFNC,									0,			"z",		arg_me,				F_BASIC,	"arg(z)",							"Angle from complex z = r e^j theta"},

	{"sin",			0, IS_FNC,									0,			"z",		sin_me,				F_TRIG,	"sin(rad)",							"SIN function of radian argument"},
	{"cos",			0, IS_FNC,									0,			"z",		cos_me,				F_TRIG,	"cos(rad)",							"COS function of radian argument"},
	{"tan",			0, IS_FNC,									0,			"z",		tan_me,				F_TRIG,	"tan(rad)",							"TAN function of radian argument"},
	{"cot",			0, IS_FNC,									0,			"z",		cot_me,				F_TRIG,	"cot(rad)",							"COT function of radian argument"},
	{"sind",			0, IS_FNC,									0,			"z",		sind_me,				F_TRIG,	"sind(deg)",						"SIN function of degree argument"},
	{"cosd",			0, IS_FNC,									0,			"z",		cosd_me,				F_TRIG,	"cosd(deg)",						"COS function of degree argument"},
	{"tand",			0, IS_FNC,									0,			"z",		tand_me,				F_TRIG,	"tand(deg)",						"TAN function of degree argument"},
	{"cotd",			0, IS_FNC,									0,			"z",		cotd_me,				F_TRIG,	"cotd(deg)",						"COT function of degree argument"},
	{"asin",			0, IS_FNC,									0,			"z",		asin_me,				F_TRIG,	"asin(x)",							"arcsin function returning radians"},
	{"acos",			0, IS_FNC,									0,			"z",		acos_me,				F_TRIG,	"acos(x)",							"arccos function returning radians"},
	{"atan",			0, IS_FNC,									0,			"z",		atan_me,				F_TRIG,	"atan(x)",							"arctan function returning radians"},
	{"acot",			0, IS_FNC,									0,			"z",		acot_me,				F_TRIG,	"acot(x)",							"arccot function returning radians"},
	{"atan2",		0, IS_FNC,									0,			"zz",		atan2_me,			F_TRIG,	"atan2(y,x)",						"Proper quadrant arctan of y/x returning radians"},
	{"asind",		0, IS_FNC,									0,			"z",		asind_me,			F_TRIG,	"asind(x)",							"arcsin function returning degrees"},
	{"acosd",		0, IS_FNC,									0,			"z",		acosd_me,			F_TRIG,	"acosd(x)",							"arccos function returning degrees"},
	{"atand",		0, IS_FNC,									0,			"z",		atand_me,			F_TRIG,	"atand(x)",							"arctan function returning degrees"},
	{"acotd",		0, IS_FNC,									0,			"z",		acotd_me,			F_TRIG,	"acotd(x)",							"arccot function returning degrees"},
	{"atan2d",		0, IS_FNC,									0,			"zz",		atan2d_me,			F_TRIG,	"atan2d(y,x)",						"Proper quadrant arctan of y/x returning degrees"},
	{"arcsin",		0, IS_FNC,									0,			"z",		asin_me,				F_TRIG,	"arcsin(x)",						"arcsin function returning radians"},
	{"arccos",		0, IS_FNC,									0,			"z",		acos_me,				F_TRIG,	"arccos(x)",						"arccos function returning radians"},
	{"arctan",		0, IS_FNC,									0,			"z",		atan_me,				F_TRIG,	"arctan(x)",						"arctan function returning radians"},
	{"arccot",		0, IS_FNC,									0,			"z",		acot_me,				F_TRIG,	"arccot(x)",						"arccot function returning radians"},
	{"arctan2",		0, IS_FNC,									0,			"zz",		atan2_me,			F_TRIG,	"arctan2(y,x)",					"Proper quadrant arctan of y/x returning radians"},
	{"arcsind",		0, IS_FNC,									0,			"z",		asind_me,			F_TRIG,	"arcsind(x)",						"arcsin function returning degrees"},
	{"arccosd",		0, IS_FNC,									0,			"z",		acosd_me,			F_TRIG,	"arccosd(x)",						"arccos function returning degrees"},
	{"arctand",		0, IS_FNC,									0,			"z",		atand_me,			F_TRIG,	"arctand(x)",						"arctan function returning degrees"},
	{"arccotd",		0, IS_FNC,									0,			"z",		acotd_me,			F_TRIG,	"arccotd(x)",						"arccot function returning degrees"},
	{"arctan2d",	0, IS_FNC,									0,			"zz",		atan2d_me,			F_TRIG,	"arctan2d(y,x)",					"Proper quadrant arctan of y/x returning degrees"},

	{"sinh",			0, IS_FNC,									0,			"z",		sinh_me,				F_TRIG,	"sinh(x)",							"hyperbolic sin function"},
	{"cosh",			0, IS_FNC,									0,			"z",		cosh_me,				F_TRIG,	"cosh(x)",							"hyperbolic cos function"},
	{"tanh",			0, IS_FNC,									0,			"z",		tanh_me,				F_TRIG,	"tanh(x)",							"hyperbolic tan function"},
	{"sech",			0, IS_FNC,									0,			"z",		sech_me,				F_TRIG,	"sech(x)",							"hyperbolic secant function"},
	{"csch",			0, IS_FNC,									0,			"z",		csch_me,				F_TRIG,	"csch(x)",							"hyperbolic cosecant function"},
	{"coth",			0, IS_FNC,									0,			"z",		coth_me,				F_TRIG,	"coth(x)",							"hyperbolic cotangent function"},

	{"asinh",		0, IS_FNC,									0,			"z",		asinh_me,			F_TRIG,	"asinh(x)",							"inverse hyperbolic sin function"},
	{"acosh",		0, IS_FNC,									0,			"z",		acosh_me,			F_TRIG,	"acosh(x)",							"inverse hyperbolic cos function"},
	{"atanh",		0, IS_FNC,									0,			"z",		atanh_me,			F_TRIG,	"atanh(x)",							"inverse hyperbolic tan function"},
	{"asech",		0, IS_FNC,									0,			"z",		asech_me,			F_TRIG,	"asech(x)",							"inverse hyperbolic secant function"},
	{"acsch",		0, IS_FNC,									0,			"z",		acsch_me,			F_TRIG,	"acsch(x)",							"inverse hyperbolic cosecant function"},
	{"acoth",		0, IS_FNC,									0,			"z",		acoth_me,			F_TRIG,	"acoth(x)",							"inverse hyperbolic cotangent function"},
	{"arsinh",		0, IS_FNC,									0,			"z",		asinh_me,			F_TRIG,	"arsinh(x)",						"inverse hyperbolic sin function"},
	{"arcosh",		0, IS_FNC,									0,			"z",		acosh_me,			F_TRIG,	"arcosh(x)",						"inverse hyperbolic cos function"},
	{"artanh",		0, IS_FNC,									0,			"z",		atanh_me,			F_TRIG,	"artanh(x)",						"inverse hyperbolic tan function"},
	{"arsech",		0, IS_FNC,									0,			"z",		asech_me,			F_TRIG,	"arsech(x)",						"inverse hyperbolic secant function"},
	{"arcsch",		0, IS_FNC,									0,			"z",		acsch_me,			F_TRIG,	"arcsch(x)",						"inverse hyperbolic cosecant function"},
	{"arcoth",		0, IS_FNC,									0,			"z",		acoth_me,			F_TRIG,	"arcoth(x)",						"inverse hyperbolic cotangent function"},

	{"ln",			0, IS_FNC,									0,			"z",		ln_me,				F_BASIC,	"ln(x)",								"natural (base e) logarithm"},
	{"log",			0, IS_FNC,									0,			"z",		log_me,				F_BASIC,	"log(x)",							"common (base 10) logarithm"},
	{"exp",			0, IS_FNC,									0,			"z",		exp_me,				F_BASIC,	"exp(x)",							"e raised to argument"},
	{"sqrt",			0, IS_FNC,									0,			"z",		sqrt_me,				F_BASIC,	"sqrt(x)",							"square root of x"},
	{"pow",			0, IS_FNC,									0,			"zz",		pow_me,				F_BASIC,	"pow(x,y)",							"x raised to y power (safe)"},
	{"fact",			0, IS_FNC,									0,			"z",		fact_me,				F_BASIC,	"fact(n)",							"factorial of n (real args okay)"},
	{"gamma",		0, IS_FNC,									0,			"z",		gamma_me,			F_BASIC,	"gamma(x)",							"gamma function of x - n! = gamma(n+1)"},
	{"lngamma",		0, IS_FNC,									0,			"z",		lngamma_me,			F_BASIC,	"lngamma(x)",						"natural logarithm of the gamma function"},
	{"digamma",		0, IS_FNC,									0,			"z",		digamma_me,			F_BASIC,	"digamma(x)",						"digamma (derivative of lngamma) function"},
	{"ldexp",		0, IS_FNC,									0,			"ri",		ldexp_me,			F_BASIC,	"ldexp(x,n)",						"sets 2^n exponent of x to nint(n)"},
   {"round",      0, IS_FNC,									0,			"ri",		round_me,			F_BASIC,	"round(x,n)",						"round x to nint(n) digits.  Negative n permitted"},

#ifdef HAS_BESSEL
	{"j0",			0, IS_FNC,									0,			"z",		j0_me,				F_SPEC,	"j0(x)",								"Bessel function (first kind) of zero order"},
	{"j1",			0, IS_FNC,									0,			"z",		j1_me,				F_SPEC,	"j1(x)",								"Bessel function (first kind) of first order"},
	{"jn",			0, IS_FNC,									0,			"zr",		jn_me,				F_SPEC,	"jn(n,x)",							"Bessel function (first kind) of order n"},
	{"y0",			0, IS_FNC,									0,			"z",		y0_me,				F_SPEC,	"y0(x)",								"Bessel function (second kind) of zero order"},
	{"y1",			0, IS_FNC,									0,			"z",		y1_me,				F_SPEC,	"y1(x)",								"Bessel function (second kind) of first order"},
	{"yn",			0, IS_FNC,									0,			"zr",		yn_me,				F_SPEC,	"yn(n,x)",							"Bessel function (second kind) of order n"},
#endif

	{"rand",			0, IS_FNC,									0,			NULL,		rnd_me,				F_SPEC,	"rand()",							"Random number on [0,1] (system supplied)"},
	{"srand",		0, IS_FNC,									0,			"r",		srand_me,			F_SPEC,	"srand(seed)",						"Sets seed for random number generator (system supplied)"},
	{"gnoise",		0, IS_FNC,									0,			NULL,		gnoise_me,			F_SPEC,	"gnoise()",							"Gaussian noise generator (system supplied)"},
/*	{"pnoise",		0, IS_FNC,									0,			NULL,		pnoise_me,			F_SPEC,	"pnoise()",							"Poisson noise generator (system supplied)"},		*/

/* 48-bit versions of random number routines */
	{"drand48",		0, IS_FNC,									0,			NULL,		drand48_me,			F_SPEC,	"drand48()",						"better random number generator on [0,1]"},
	{"lrand48",		0, IS_FNC,									0,			NULL,		lrand48_me,			F_SPEC,	"lrand48()",						"better random number generator on [0,2^31-1]"},
	{"mrand48",		0, IS_FNC,									0,			NULL,		mrand48_me,			F_SPEC,	"mrand48()",						"better random number generator on [-2^31-1, 2^31-1]"},
	{"srand48",		0, IS_FNC,									0,			"z",		srand48_me,			F_SPEC,	"srand48()",						"seed value for the \"48\" random number functions"},
	{"drand",		0, IS_FNC,									0,			NULL,		drand48_me,			F_SPEC,	"drand()",							"equivalent to drand48()"},
	{"lrand",		0, IS_FNC,									0,			NULL,		lrand48_me,			F_SPEC,	"lrand()",							"equivalent to lrand48()"},
	{"mrand",		0, IS_FNC,									0,			NULL,		mrand48_me,			F_SPEC,	"mrand()",							"equivalent to mrand48()"},

/* Mersenne twister random number routines */
	{"random",			0, IS_FNC,								0,			NULL,			rnd_drand_me,		F_STAT,	"random()",							"Mersenne Twister random on [0,1].  Same as rnd_drand()"},
	{"rnd",			   0, IS_FNC,								0,			NULL,		   rnd_drand_me,		F_SPEC,	"rnd()",								"Random number on [0,1] (Mersenne Twister)"},
	{"rnd_seed",		0,	IS_FNC,								0,			"i=-1",		rnd_seed_me,		F_STAT,	"rnd_seed([iseed])",				"Initialize Mersenne Twister with iseed (default=time())"},
	{"rnd_lrand",		0, IS_FNC,								0,			NULL,			rnd_lrand_me,		F_STAT,	"rnd_lrand()",						"Random integer value (32 bits)"},
	{"rnd_drand",		0, IS_FNC,								0,			NULL,			rnd_drand_me,		F_STAT,	"rnd_drand()",						"Random double value on [0,1])"},
	{"rnd_iuniform",	0, IS_FNC,								0,			"i=0|i=9",	rnd_iuniform_me,	F_STAT,	"rnd_iuniform(min, max)",		"Uniform integers on interval [min,max)"},
	{"rnd_uniform",	0, IS_FNC,								0,			"r=0|r=1",	rnd_uniform_me,	F_STAT,	"rnd_uniform(min, max)",		"Uniform doubles on interval [min,max]"},
	{"rnd_exponential",0,IS_FNC,								0,			"r",			rnd_exponential_me,F_STAT,	"rnd_exponential(mean)",		"Exponentially distributed random numbers with specified mean"},
	{"rnd_erlang",		0, IS_FNC,								0,			"ir",			rnd_erlang_me,		F_STAT,	"rnd_erlang(k, mean)",			"k-Erlang distributed random numbers with int k, double mean"},
	{"rnd_norm",		0, IS_FNC,								0,			NULL,			rnd_norm_me,		F_STAT,	"rnd_norm()",						"Normally distribution random numbers u=0,s=1"},
	{"rnd_normal",		0, IS_FNC,								0,			"rr",			rnd_normal_me,		F_STAT,	"rnd_normal(mean, sigma)",		"Normally distribution random numbers with mean/sigma"},
	{"rnd_lognormal",	0,	IS_FNC,								0, 		"rr",			rnd_lognormal_me,	F_STAT,	"rnd_lognormal(mu, sigma)",	"Lognormal distributed random where ln x is normal with mu/sigma"},
	{"rnd_triangle",	0,	IS_FNC,								0, 		"rrr",		rnd_triangle_me,	F_STAT,	"rnd_triangle(min,max,mode)",	"Triangular distributed random numbers on [min,max] with mode"},
	{"rnd_weibull",	0, IS_FNC,								0,			"rr",			rnd_weibull_me,	F_STAT,	"rnd_weibull(beta, eta)",	   "Weibull distributed random numbers with shape/scale parameters"},

	{"erf",			0, IS_FNC,									0,			"z",		erf_me,				F_SPEC,	"erf(x)",							"error function"},
	{"erfc",			0, IS_FNC,									0,			"z",		erfc_me,				F_SPEC,	"erfc(x)",							"complementary error function 1-erf(x)"},
	{"erfi",			0, IS_FNC,									0,			"z",		erfi_me,				F_SPEC,	"erfi(x)",							"inverse error function"},
	{"erfci",		0, IS_FNC,									0,			"z",		erfci_me,			F_SPEC,	"erfci(x)",							"inverse complementary error function"},
	{"lnerfc",		0, IS_FNC,									0,			"z",		lnerfc_me,			F_SPEC,	"lnerfc(x)",						"natural logarithm of erfc(x)"},
	{"ndtr",			0, IS_FNC,									0,			"r",		ndtr_me,				F_STAT,	"ndtr(x)",							"normal distribution - probability of z<x"},
	{"ndtri",		0, IS_FNC,									0,			"r",		ndtri_me,			F_STAT,	"ndtri(x)",							"inverse normal distribution"},
	{"fdm0p5",		0, IS_FNC,									0, 		"r",		fdm0p5_me,			F_STAT,	"fdm0p5(x)",						"F_{-0.5}(x) Fermi-Dirac Integral of -0.5 order"},
	{"fdp0p5",		0, IS_FNC,									0, 		"r",		fdp0p5_me,			F_STAT,	"fdp0p5(x)",						"F_{+0.5}(x) Fermi-Dirac Integral of  0.5 order"},
	{"fdp1p5",		0, IS_FNC,									0, 		"r",		fdp1p5_me,			F_STAT,	"fdp1p5(x)",						"F_{+1.5}(x) Fermi-Dirac Integral of  1.5 order"},
	{"fdp2p5",		0, IS_FNC,									0, 		"r",		fdp2p5_me,			F_STAT,	"fdp2p5(x)",						"F_{+2.5}(x) Fermi-Dirac Integral of  2.5 order"},
	{"binomial",   0, IS_FNC,									0,			"rrr",	binomial_me,		F_STAT,	"binomial(x,n,p)",				"Binomial - observe x of n events with probability p"},
	{"edgeworth",	0, IS_FNC,									0,			"rrrrr",	edgeworth_me,		F_STAT,	"edgeworth(x,x0,s,sk,ku)",		"Edgeworth distribution with mean x0, sigma s, skew sk and kurtosis ku"},
	{"normal",		0, IS_FNC,									0,			"rrr",	gauss_me,			F_STAT,	"normal(x,x0,s)",					"Normal (Gaussian) with mean x0 and sigma s."},
	{"gauss",		0, IS_FNC,									0,			"rrr",	gauss_me,			F_STAT,	"gauss(x,x0,s)",					"Gaussian with mean x0 and sigma s.  FWHM = 2 sqrt(2ln 2) s"},
	{"gaussian",	0, IS_FNC,									0,			"rrr",	gauss_me,			F_STAT,	"gaussian(x,x0,s)",				"Gaussian with mean x0 and sigma s.  FWHM = 2 sqrt(2ln 2) s"},
	{"gaussn",		0, IS_FNC,									0,			"rrr",	gaussn_me,			F_STAT,	"gaussn(x,x0,s)",					"Normalized Gaussian with mean x0 and sigma s"},
	{"lorentz",    0, IS_FNC,									0,			"rrr",	lorentz_me,			F_STAT,	"lorentz(x,x0,w)",				"Lorentz distribution for mean x0 and width w"},
	{"lorentzian", 0, IS_FNC,									0,			"rrr",	lorentz_me,			F_STAT,	"lorentz(x,x0,w)",				"Lorentz distribution for mean x0 and width w"},
	{"poisson",    0, IS_FNC,									0,			"rr",		poisson_me,			F_STAT,	"poisson(x,x0)",					"Poisson statistics for x given mean x0"},
	{"weibull",		0, IS_FNC,									0,			"rrrr",	weibull_me,			F_STAT,	"weibull(T,gamma,eta,beta)",	"Weibull distribrution with location gamma, scale eta and shape beta"},
/* {"pearson_IV",	0, IS_FNC,									0,			"rrrrr",	pearson_IV_me,		F_STAT,	"pearson_IV()",					"Pearson IV distribution"},	*/
/* {"pearson_VI",	0, IS_FNC,									0,			"rrrrr",	pearson_VI_me,		F_STAT,	"pearson_IV()",					"Pearson VI distribution"},	*/

	{"beta",			0, IS_FNC,									0,			"zz",		beta_me,				F_STAT,	"beta(a,b)",						"Beta distribution function"},
	{"lnbeta",		0, IS_FNC,									0,			"zz",		lnbeta_me,			F_STAT,	"lnbeta(a,b)",						"natural logarithm of beta distribution function"},
	{"betai",		0, IS_FNC,									0,			"rrr",	betai_me,			F_STAT,	"betai(x,a,b)",					"B(x;a,b) incomplete beta function"},
	{"betai_Ix",	0, IS_FNC,									0, 		"rrr",	betai_Ix_me,		F_STAT,	"betai_Ix(x,a,b)",				"I_x(a,b) regularized incomplete beta function"},

	{"t_test",		0, IS_FNC,									0,			"rr",		t_test_me,			F_STAT,	"t_test(t,v)",						"A(t|v) Student t-test on t with v DOF"},
	{"f_test",		0, IS_FNC,									0,			"rrr",	f_test_me,			F_STAT,	"f_test(F,v1,v2)",				"Q(F|v1,v2) F-Distribution test"},
	{"chisqr",		0, IS_FNC,									0,			"rr",		chi2_me,				F_STAT,	"chisqr(x,nu)",					"chi^2 distribution with nu degrees of freedom"},
	{"chi2",			0, IS_FNC,									0,			"rr",		chi2_me,				F_STAT,	"chiw(x,nu)",						"chi^2 distribution with nu degrees of freedom"},
	{"p_chi",		0, IS_FNC,									0,			"rr",		p_chi_me,			F_STAT,	"p_chi(chisqr,v)",				"P(chisqr|v) chi-square probability"},
	{"q_chi",		0, IS_FNC,									0,			"rr",		q_chi_me,			F_STAT,	"q_chi(chisqr,v)",				"Q(chisqr|v) chi-square probability"},
	{"@z_test",		0, IS_SPC,									2,			NULL,		IS_Z_TEST,			F_STAT,	"@z_test(ar,mu,s)",				"Z-test on array ar given mean mu and sigma s"},
	{"@t_test",		0, IS_SPC,									2,			NULL,		IS_T_TEST,			F_STAT,	"@t_test(ar1,ar2 | mu)",		"Student t-test on two arrays or one array and mean mu"},
	{"@td_test",	0, IS_SPC,									2,			NULL,		IS_TD_TEST,			F_STAT,	"@td_test(ar1,ar2)",				"Student t-test on two dependent arrays ar1, ar2"},
	{"@u_test",		0, IS_SPC,									2,			NULL,		IS_U_TEST,			F_STAT,	"@u_test(ar1,ar2)",				"Student t-test on two arrays with uneven variance"},
	{"@f_test",		0, IS_SPC,									2,			NULL,		IS_F_TEST,			F_STAT,	"@f_test(ar1,ar2)",				"f-test on two arrays"},

	{"spline",		0, IS_FNC,									0,"zA=spl$data",	spline_me,			F_POLY,	"spline(x [,ar])",				"spline at x (from internal spline fit)"},
	{"ispln",		0, IS_FNC,									0,"zzA=spl$data",	ispln_me,			F_POLY,	"ispln(xlow,xhigh [,ar])",		"integral of spline from xlow to xhigh"},
	{"dspln",		0, IS_FNC,									0,"zA=spl$data",	dspln_me,			F_POLY,	"dspln(x [,ar])",					"derivative of spline at x"},
	{"ddspln",		0, IS_FNC,									0,"zA=spl$data",	ddspln_me,			F_POLY,	"ddspln(x [,ar])",				"second derivative of spline at x"},
	{"spline2",		0, IS_FNC,									0,			"zA",		spline_me,			F_POLY,	"spline2(x,ar)",					"spline at x using spline structure in ar"},
	{"dspln2",		0, IS_FNC,									0,			"zA",		dspln_me,			F_POLY,	"dspln2(x,ar)",					"derivative of spline at x using structure in ar"},
	{"ddspln2",		0, IS_FNC,									0,			"zA",		ddspln_me,			F_POLY,	"ddspln2(x,ar)",					"second derivative of spline at x using structure in ar"},

	{"hv$",			0, IS_FNC,									0,			"r",		hv_me,				F_SPEC,	"hv$(x)",							"Heavyside operator 0,0.5,1 for x<0,x=0,x>0"},
	{"tn",			0, IS_FNC,									0,			"iz",		tn_me,				F_SPEC,	"Tn(n,x)",							"Chebyshev polynomial T_n(x)"},

	{"poly",			0, IS_FNC,									0,			"zA",		poly_me,				F_POLY,	"poly(x,ar)",						"polynomial evaluation = sum ar[i]*x^i"},
	{"dpoly",		0, IS_FNC,									0,			"zAi=1",	dpoly_me,			F_POLY,	"dpoly(x,ar,[order])",			"derivative of polynomial evaluation at x"},
	{"cheby",		0, IS_FNC,									0,			"zA",		cheby_me,			F_POLY,	"cheby(x,ar)",						"Chebyshev polynomial = sum ar[i]*T_i(x)"},

	{"@min",			0, IS_R_ARRAY ,							2,			NULL,		array_min,			F_STAT,	"@min(ar [,il,ih])",				"minimum of array, optionally limited to subset il<=i<=ih"},
	{"@max",			0, IS_R_ARRAY ,							2,			NULL,		array_max,			F_STAT,	"@max(ar [,il,ih])",				"maximum of array, optionally limited to subset il<=i<=ih"},
	{"@sum",			0, IS_R_ARRAY ,							2,			NULL,		array_sum,			F_STAT,	"@sum(ar [,il,ih])",				"sum of array, optionally limited to subset il<=i<=ih"},
	{"@ave",			0, IS_R_ARRAY ,							2,			NULL,		array_avg,			F_STAT,	"@ave(ar [,il,ih])",				"average of array, optionally limited to subset il<=i<=ih"},
	{"@avg",			0, IS_R_ARRAY ,							2,			NULL,		array_avg,			F_STAT,	"@avg(ar [,il,ih])",				"average of array, optionally limited to subset il<=i<=ih"},
	{"@average",	0, IS_R_ARRAY ,							2,			NULL,		array_avg,			F_STAT,	"@average(ar [,il,ih])",		"average of array, optionally limited to subset il<=i<=ih"},
	{"@mean",		0, IS_R_ARRAY ,							2,			NULL,		array_avg,			F_STAT,	"@mean(ar [,il,ih])",			"average of array, optionally limited to subset il<=i<=ih"},
	{"@E",			0, IS_R_ARRAY ,							2,			NULL,		array_avg,			F_STAT,	"@E(ar [,il,ih])",				"expectation value (average) of array, opt subset il<=i<=ih"},
	{"@median",		0, IS_R_ARRAY ,							2,			NULL,		array_median,		F_STAT,	"@median(ar [,il,ih])",			"median of array, optionally limited to subset"},
	{"@mad",			0, IS_R_ARRAY ,							2,			NULL,		array_mad,			F_STAT,	"@mad(ar [,il,ih])",				"MAD of array, optionally limited to subset"},
	{"@count",		0, IS_R_ARRAY ,							2,			NULL,		array_count,		F_STAT,	"@count(ar [,il,ih])",			"count of array, optionally limited to subset"},
	{"@variance",	0, IS_R_ARRAY ,							2,			NULL,		array_var,			F_STAT,	"@variance(ar [,il,ih])",		"variance of array, optionally limited to subset"},
	{"@var",			0, IS_R_ARRAY ,							2,			NULL,		array_var,			F_STAT,	"@var(ar [,il,ih])",				"variance of array, optionally limited to subset"},
	{"@covariance",0,	IS_R2_ARRAY,							2,			NULL,		array_covar,		F_STAT,	"@covariance(ar, ar2, [,il,ih])",				"Covariance of two arrays, optionally limited to subset"},
	{"@covar",		0,	IS_R2_ARRAY,							2,			NULL,		array_covar,		F_STAT,	"@covar(ar, ar2, [,il,ih])",	"Covariance of two arrays, optionally limited to subset"},
	{"@cov",			0,	IS_R2_ARRAY,							2,			NULL,		array_covar,		F_STAT,	"@cov(ar, ar2, [,il,ih])",	   "Covariance of two arrays, optionally limited to subset"},
	{"@std",			0, IS_R_ARRAY ,							2,			NULL,		array_std,			F_STAT,	"@std(ar [,il,ih])",				"standard deviation of array, optionally limited"},
	{"@sdev",		0, IS_R_ARRAY ,							2,			NULL,		array_std,			F_STAT,	"@sdev(ar [,il,ih])",			"standard deviation of array, optionally limited"},
	{"@stdev",		0, IS_R_ARRAY ,							2,			NULL,		array_std,			F_STAT,	"@stdev(ar [,il,ih])",			"standard deviation of array, optionally limited"},
	{"@stddev",		0, IS_R_ARRAY ,							2,			NULL,		array_std,			F_STAT,	"@stddev(ar [,il,ih])",			"standard deviation of array, optionally limited"},
	{"@sdom",		0, IS_R_ARRAY ,							2,			NULL,		array_sdom,			F_STAT,	"@sdom(ar [,il,ih])",			"standard deviation of the mean, optionally limited"},
	{"@rms",			0, IS_R_ARRAY ,							2,			NULL,		array_rms,			F_STAT,	"@rms(ar [,il,ih])",				"rms deviation of array, optionally limited"},
	{"@skew",		0, IS_R_ARRAY ,							2,			NULL,		array_skew,			F_STAT,	"@skew(ar [,il,ih])",			"skew of array, optionally limited"},
	{"@skewness",	0, IS_R_ARRAY ,							2,			NULL,		array_skew,			F_STAT,	"@skewness(ar [,il,ih])",		"skew of array, optionally limited"},
	{"@kurt",		0, IS_R_ARRAY ,							2,			NULL,		array_kurt,			F_STAT,	"@kurt(ar [,il,ih])",			"excess kurtosis of array, optionally limited"},
	{"@kurtosis",	0, IS_R_ARRAY ,							2,			NULL,		array_kurt,			F_STAT,	"@kurtosis(ar [,il,ih])",		"excess kurtosis of array, optionally limited"},
	{"@range",		0, IS_R_ARRAY ,							2,			NULL,		array_span,			F_STAT,	"@range(ar [,il,ih])",			"span of array (max-min), optionally limited"},
	{"@span",		0, IS_R_ARRAY ,							2,			NULL,		array_span,			F_STAT,	"@span(ar [,il,ih])",			"span of array (max-min) of array, optionally limited"},
	{"@absmin",		0, IS_R_ARRAY ,							2,			NULL,		array_absmin,		F_STAT,	"@absmin(ar [,il,ih])",			"minimum of absolute value of array, optionally limited"},
	{"@absmax",		0, IS_R_ARRAY ,							2,			NULL,		array_absmax,		F_STAT,	"@absmax(ar [,il,ih])",			"maximum of absolute value of array, optionally limited"},
	{"@abssum",		0, IS_R_ARRAY ,							2,			NULL,		array_abssum,		F_STAT,	"@abssum(ar [,il,ih])",			"sum of absolute value of array, optionally limited"},
	{"@absavg",		0, IS_R_ARRAY ,							2,			NULL,		array_absavg,		F_STAT,	"@absavg(ar [,il,ih])",			"average of absolute value of array, optionally limited"},

	{"@wmean",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_avg,	F_STAT,	"@wmean(ar, sigma [,il,ih])",		"weighted average of array and sigma, subset optional il<=i<=ih"},
	{"@wave",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_avg,	F_STAT,	"@wave(ar, sigma [,il,ih])",		"weighted average of array and sigma, subset optional il<=i<=ih"},
	{"@wavg",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_avg,	F_STAT,	"@wavg(ar, sigma [,il,ih])",		"weighted average of array and sigma, subset optional il<=i<=ih"},
	{"@waverage",	0, IS_R2_ARRAY ,							2,			NULL,		array_weight_avg,	F_STAT,	"@wmaverage(ar, sigma [,il,ih])","weighted average of array and sigma, subset optional il<=i<=ih"},
	{"@wsigma",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_sigma,F_STAT,	"@wsigma(ar, sigma [,il,ih])",	"weighted uncertainty sigma, subset optional il<=i<=ih"},
	{"@wvariance",	0, IS_R2_ARRAY ,							2,			NULL,		array_weight_var,	F_STAT,	"@wvariance(ar, sigma [,il,ih])","weighted variance of array, optionally limited"},
	{"@wvar",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_var,	F_STAT,	"@wvar(ar, sigma [,il,ih])",		"weighted variance of array, optionally limited"},
	{"@wstd",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_std,	F_STAT,	"@wstd(ar, sigma [,il,ih])",		"weighted standard deviation of array, optionally limited"},
	{"@wstdev",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_std,	F_STAT,	"@wstdev(ar, sigma [,il,ih])",	"weighted standard deviation of array, optionally limited"},
	{"@wstddev",	0, IS_R2_ARRAY ,							2,			NULL,		array_weight_std,	F_STAT,	"@wstddev(ar, sigma [,il,ih])",	"weighted standard deviation of array, optionally limited"},
	{"@wsdom",		0, IS_R2_ARRAY ,							2,			NULL,		array_weight_sdom,F_STAT,	"@wsdom(ar, sigma [,il,ih])",		"weighted standard deviation of the mean, optionally limited"},
	{"@wabsavg",	0, IS_R2_ARRAY ,							2,			NULL,		array_weight_absavg,F_STAT,"@wabsavg(ar, sigma [,il,ih])",	"weighted average of abs(x) of array, optionally limited"},

	{"@index",		0, IS_FNC ,									0,			"Ar",		array_index,		F_SPEC,	"@index(ar,x)",					"index of first array entry >= x, -1 if > ar[npt-1]"},

	{"solve",		0, IS_FUNCTION_EVAL,						0x0302,	NULL,		solve_me,			F_FUNC,	"solve(fnc,xl,xh[,...])",		"solve for zero of a function"},
	{"dydx",			0, IS_FUNCTION_EVAL,						0x0101,	NULL,		dydx_me,				F_FUNC,	"dydx(fnc,x[,eps])",				"numeric derivative of a function"},
	{"integral",	0, IS_FUNCTION_EVAL,						0x0302,	NULL,		integrate_me,		F_FUNC,	"integral(fnc,x1,x2[,...])",	"numeric integral of a function"},
	{"integrate",	0, IS_FUNCTION_EVAL,						0x0302,	NULL,		integrate_me,		F_FUNC,	"integrate(fnc,x1,x2[,...])",	"numeric integral of a function"},
	{"sum",			0, IS_FUNCTION_EVAL,						0x0102,	NULL,		sum_me,				F_FUNC,	"sum(fnc,ilow,ihigh,[istep])","summation over integer index"},
	{"prod",			0, IS_FUNCTION_EVAL,						0x0102,	NULL,		prod_me,				F_FUNC,	"prod(fnc,ilow,ihigh,[istep])","continued product over integer index"},

	{"@integral",		0, IS_CURVE_EVAL,						2,			NULL,		curve_integral,	F_STAT,	"@integral(cv [,xl,xh])",		"integral of curve over defined range"},
	{"@integrate",		0, IS_CURVE_EVAL,						2,			NULL,		curve_integral,	F_STAT,	"@integrate(cv [,xl,xh])",		"integral of curve over defined range"},
	{"@correlate",		0, IS_CURVE_EVAL,						2,			NULL,		curve_correlate,	F_STAT,	"@correlate [,xl,xh](cv)",		"correlation coefficient between x and y"},
	{"@pdf_ave",		0, IS_CURVE_EVAL,						2,			NULL,		curve_avg,			F_STAT,	"@pdf_ave(cv [,xl,xh])",		"mean/average of PDF curve, optionally limited"},
	{"@pdf_avg",		0, IS_CURVE_EVAL,						2,			NULL,		curve_avg,			F_STAT,	"@pdf_avg(cv [,xl,xh])",		"mean/average of PDF curve, optionally limited"},
	{"@pdf_average",	0, IS_CURVE_EVAL,						2,			NULL,		curve_avg,			F_STAT,	"@pdf_average(cv [,xl,xh])",	"mean/average of PDF curve, optionally limited"},
	{"@pdf_mean",		0, IS_CURVE_EVAL,						2,			NULL,		curve_avg,			F_STAT,	"@pdf_mean(cv [,xl,xh])",		"mean/average of PDF curve, optionally limited"},
	{"@pdf_median",	0, IS_CURVE_EVAL,						2,			NULL,		curve_median,		F_STAT,	"@pdf_median(cv [,xl,xh])",	"median of PDF curve, optionally limited"},
	{"@pdf_rms",		0, IS_CURVE_EVAL,						2,			NULL,		curve_std,			F_STAT,	"@pdf_rms(cv [,xl,xh])",		"standard deviation of PDF curve, optionally limited"},
	{"@pdf_std",		0, IS_CURVE_EVAL,						2,			NULL,		curve_std,			F_STAT,	"@pdf_std(cv [,xl,xh])",		"standard deviation of PDF curve, optionally limited"},
	{"@pdf_sdev",		0, IS_CURVE_EVAL,						2,			NULL,		curve_std,			F_STAT,	"@pdf_sdev(cv [,xl,xh])",		"standard deviation of PDF curve, optionally limited"},
	{"@pdf_stdev",		0, IS_CURVE_EVAL,						2,			NULL,		curve_std,			F_STAT,	"@pdf_stdev(cv [,xl,xh])",		"standard deviation of PDF curve, optionally limited"},
	{"@pdf_stddev",	0, IS_CURVE_EVAL,						2,			NULL,		curve_std,			F_STAT,	"@pdf_stddev(cv [,xl,xh])",	"standard deviation of PDF curve, optionally limited"},
	{"@pdf_variance",	0, IS_CURVE_EVAL,						2,			NULL,		curve_var,			F_STAT,	"@pdf_variance(cv [,xl,xh])",	"variance of PDF curve, optionally limited"},
	{"@pdf_var",		0, IS_CURVE_EVAL,						2,			NULL,		curve_var,			F_STAT,	"@pdf_var(cv [,xl,xh])",		"variance of PDF curve, optionally limited"},
	{"@pdf_skew",		0, IS_CURVE_EVAL,						2,			NULL,		curve_skew,			F_STAT,	"@pdf_skew(cv [,xl,xh])",		"skewness of PDF curve, optionally limited"},
	{"@pdf_skewness",	0, IS_CURVE_EVAL,						2,			NULL,		curve_skew,			F_STAT,	"@pdf_skewness(cv [,xl,xh])",	"skewness of PDF curve, optionally limited"},
	{"@pdf_kurt",		0, IS_CURVE_EVAL,						2,			NULL,		curve_kurt,			F_STAT,	"@pdf_kurt(cv [,xl,xh])",		"excess kurtosis of PDF curve, optionally limited"},
	{"@pdf_kurtosis",	0, IS_CURVE_EVAL,						2,			NULL,		curve_kurt,			F_STAT,	"@pdf_kurtosis(cv [,xl,xh])",	"excess kurtosis of PDF curve, optionally limited"},

	{"@pintegral",	0, IS_FNC,									0,			"Crr",	curve_pintegral,	F_STAT,	"@pintegral(cv,xl,xh)",			"integral of curve bewteen xl and xh"},
	{"@pintegrate",0, IS_FNC,									0,			"Crr",	curve_pintegral,	F_STAT,	"@pintegrate(cv,xl,xh)",		"integral of curve bewteen xl and xh"},
	{"@nearest",   0, IS_FNC,									0,			"Crr",	curve_near,			F_STAT,	"@nearest(cv,x,y)",				"index of point on curve nearest x,y"},
	{"@3d_nearest",0, IS_FNC,									0,			"Crrr",	curve_3d_near,		F_STAT,	"@3d_nearest(cv,x,y,z)",		"index of point on curve nearest x,y,x"},

	{"@zinterp",   0, IS_FNC,									0,			"Srr",	surf_interp,		F_STAT,	"@zinterp(s1,x,y)",				"interpolate z at x,y [Obsolete - use s1(x,y)]"},
	{"@zintegral", 0, IS_SURF_EVAL,							0,			NULL,		surf_integral,		F_STAT,	"@zintegral(s1)",					"surface integral over full range of s1"},

	{"sizeof",		0, IS_QUERY,								0,			NULL,		SIZEOF_OP,			F_VAR,	"sizeof(var)",						"size of internal variable - normally # of array elements"},
	{"typeof",		0, IS_QUERY,								0,			NULL,		TYPEOF_OP,			F_VAR,	"typeof(var)",						"bit-flag value with characteristics of var"},
	{"exists",		0, IS_QUERY,								0,			NULL,		EXISTS_OP,			F_VAR,	"exists(var)",						"returns 1 if variable exists, 0 otherwise"},
	{"isvar",		0, IS_QUERY,								0,			NULL,		EXISTS_OP,			F_VAR,	"isvar(var)",						"returns 1 if variable exists, 0 otherwise"},

	{"time",			0, IS_FNC,									0,			NULL,		time_me,				F_SPEC,	"time()",							"time in seconds since epoch"},
	{"clock",		0, IS_FNC,									0,			NULL,		clock_me,			F_SPEC,	"clock()",							"time in seconds for program"},
	{"timer",		0, IS_FNC,									0,			"r=0",	timer_me,			F_SPEC,	"timer([BOOL reset])",			"high precision time in seconds from last reset"},
	{"ctime",		0, IS_FNC | RETURN_STRING,				0,			"r",		ctime_me,			F_STR,	"ctime(t)",							"ASCII representation of time - ctime(time())"},
	{"asctime",		0, IS_FNC | RETURN_STRING,				0,			"r",		ctime_me,			F_STR,	"asctime(t)",						"ASCII representation of time - asctime(time())"},
	{"strftime",	0, IS_FNC | RETURN_STRING,				0,			"sr=-1.0",strftime_me,		F_STR,	"strftime(format[,time])",		"Format time string"},
	{"RGB",			0, IS_FNC,									0,			"rrr",	rgb_me,				F_SPEC,	"RGB(r,g,b)",						"encode RGB color value as an integer"},
	{"rainbow",		0, IS_FNC,									0,			"rr=0.0|r=1.0",	rainbow_me,			F_SPEC,	"rainbow(x,low,high)",			"RGB rainbow color based on x in range [low,high]"},

	{"RGB_color",	0, IS_FNC,									0,			"s",		rgb_color_me,		F_SPEC,	"RGB_color(color)",				"RGB integer corresponding to color such (e.g. pink)"},
	{"strcmp",		0, IS_FNC,									0,			"ss",		strcmp_me,			F_STR,	"strcmp(s1,s2)",					"string compare returning -1,0,1 in collating order"},
	{"stricmp",		0, IS_FNC,									0,			"ss",		stricmp_me,			F_STR,	"stricmp(s1,s2)",					"case independent string compare returning -1,0,1"},
	{"strncmp",		0, IS_FNC,									0,			"ssi",	strncmp_me,			F_STR,	"strncmp(s1,s2,n)",				"string compare of first n characters"},
	{"strnicmp",	0, IS_FNC,									0,			"ssi",	strnicmp_me,		F_STR,	"strnicmp(s1,s2,n)",				"case independent string compare of first n characters"},
	{"strlen",		0, IS_FNC,									0,			"s",		strlen_me,			F_STR,	"strlen(s1)",						"length of string (# of chars to terminating null)"},
	{"strnlen",		0, IS_FNC,									0,			"s",		strnlen_me,			F_STR,	"strnlen(s1)",						"length of string ignoring trailing whitespace"},
	{"strnblen",	0, IS_FNC,									0,			"s",		strnlen_me,			F_STR,	"strnblen(s1)",					"length of string ignoring trailing whitespace"},
	{"strspn",		0, IS_FNC,									0,			"ss",		strspn_me,			F_STR,	"strspn(s1,s2)",					"length of s1 containing only characters in s2"},
	{"strcspn",		0, IS_FNC,									0,			"ss",		strcspn_me,			F_STR,	"strcspn(s1,s2)",					"length of s1 containing no characters in s2"},
	{"LexEqual",   0, IS_FNC,									0,			"ssi",	lexequal_me,		F_STR,	"LexEqual(s1,s2,n)",				"string compare - return 1 if agree with >n characters in both"},

	{"isfile",		0, IS_FNC,									0,			"s",		file_isfile_me,	F_FILE,	"isfile(str)",						"return 1 if str is an existing file"},
	{"isdir",		0, IS_FNC,									0,			"s",		file_isdir_me,		F_FILE,	"isdir(str)",						"return 1 if str is an existing directory"},
	{"filedate",	0, IS_FNC,									0,			"s",		file_dateof_me,	F_FILE,	"filedate(str)",					"time in seconds since epoch for file modification"},
	{"filetime",	0, IS_FNC,									0,			"s",		file_dateof_me,	F_FILE,	"filetime(str)",					"time in seconds since epoch for file modification"},
	{"filesize",	0, IS_FNC,									0,			"s",		file_sizeof_me,	F_FILE,	"filesize(str)",					"size in bytes of file"},

	{"ichar",		0, IS_FNC,									0,			"s",		rexx_ichar,			F_STR,	"ichar(str | c)",					"integer value of c or first character in str"},
	{"atof",			0, IS_FNC,									0,			"s",		atof_me,				F_CONV,	"atof(str)",						"converts str to a floating value"},
	{"isatof",		0, IS_FNC,									0,			"s",		isatof_me,			F_CONV,	"isatof(str)",						"validates str for conversion via atof - 1 if okay"},
	{"atoi",			0, IS_FNC,									0,			"s",		atoi_me,				F_CONV,	"atoi(str)",						"converts str to an integer value"},
	{"atol",			0, IS_FNC,									0,			"s",		atoi_me,				F_CONV,	"atol(str)",						"converts str to a long (integer) value"},
	{"isatoi",		0, IS_FNC,									0,			"s",		isatoi_me,			F_CONV,	"isatoi(str)",						"validates str for conversion via atoi - 1 if okay"},
	{"strtol",		0, IS_FNC,									0,			"si",		strtol_me,			F_CONV,	"strtol(str,base)",				"convert str to integer in specified base"},

	{"x2d",			0, IS_FNC,									0,			"s",		hex2int_me,			F_CONV,	"x2d(str)",							"convert hexadecimal str to an integer"},
	{"d2x",			0, IS_FNC | RETURN_STRING,				0,			"i",		int2hex_me,			F_CONV,	"d2x(i)",							"convert i to a hexadecimal string"},

	{"int2hex",		0, IS_FNC | RETURN_STRING,				0,			"i",		int2hex_me,			F_CONV,	"int2hex(i)",						"convert i to a hexadecimal string - same as d2x()"},
	{"hex2int",		0, IS_FNC,									0,			"s",		hex2int_me,			F_CONV,	"hex2int(str)",					"convert hexadecimal str to an integer - same as x2d()"},
	{"int2bin",		0, IS_FNC | RETURN_STRING,				0,			"i",		int2bin_me,			F_CONV,	"int2bin(i)",						"convert i to a binary string"},
	{"bin2int",		0, IS_FNC,									0,			"s",		bin2int_me,			F_CONV,	"bin2int(str)",					"convert binary str to an integer"},
	{"int2oct",		0, IS_FNC | RETURN_STRING,				0,			"i",		int2oct_me,			F_CONV,	"int2oct(i)",						"convert i to a octal string"},
	{"oct2int",		0, IS_FNC,									0,			"s",		oct2int_me,			F_CONV,	"oct2int(str)",					"convert octal str to an integer"},
	{"int2base",	0, IS_FNC | RETURN_STRING,				0,			"ii",		int2base_me,		F_CONV,	"int2base(i,base)",				"convert i to string in an arbitrary base"},
	{"base2int",	0, IS_FNC,									0,			"si",		base2int_me,		F_CONV,	"base2int(str,base)",			"convert arbitrary base string to an integer"},

	{"hex2bin",		0, IS_FNC | RETURN_STRING,				0,			"s",		hex2bin_me,			F_CONV,	"hex2bin(str)",					"convert hexadecimal str to a binary string"},
	{"bin2hex",		0, IS_FNC | RETURN_STRING,				0,			"s",		bin2hex_me,			F_CONV,	"bin2hex(str)",					"convert binary str to a hexadecimal string"},

	{"float2hex",	0, IS_FNC | RETURN_STRING,				0,			"r",		float2hex_me,		F_CONV,	"float2hex(x)",					"convert x to a internal hexadecimal float representation"},
	{"hex2float",	0, IS_FNC,									0,			"s",		hex2float_me,		F_CONV,	"hex2float(str)",					"convert internal hexadecimal representation of float to value"},
	{"real2hex",	0, IS_FNC | RETURN_STRING,				0,			"r",		float2hex_me,		F_CONV, 	"real2hex(x)",						"convert x to a internal hexadecimal real representation"},
	{"hex2real",	0, IS_FNC,									0,			"s",		hex2float_me,		F_CONV,	"hex2real(str)",					"convert internal hexadecimal representation of float to value"},
	{"double2hex",	0, IS_FNC | RETURN_STRING,				0,			"r",		double2hex_me,		F_CONV, 	"double2hex(x)",					"convert x to a internal hexadecimal double representation"},
	{"hex2double",	0, IS_FNC,									0,			"s",		hex2double_me,		F_CONV,	"hex2double(str)",				"convert internal hexadecimal representation of double to value"},

	{"time2float",	0, IS_FNC,									0,			"s",		time2double_me,	F_CONV,	"time2float(\"hh:mm:ss\")",	"convert time string to numerical value"},
	{"time2double",0, IS_FNC,									0,			"s",		time2double_me,	F_CONV,	"time2double(\"hh:mm:ss\")",	"convert time string to numerical value"},

	{"fullpath",	0, IS_FNC | RETURN_STRING,				0,			"s",		fullpath_me,		F_FILE,	"fullpath(fname)",				"returns full path of partial filename"},
	{"pwd",			0, IS_FNC | RETURN_STRING,				0,			NULL,		pwd_me,				F_FILE,	"pwd()",								"returns current working directory"},
	{"cwd",			0, IS_FNC | RETURN_STRING,				0,			NULL,		pwd_me,				F_FILE,	"cwd()",								"returns current working directory"},
	{"getenv",		0, IS_FNC | RETURN_STRING,				0,			"s",		getenv_me,			F_STR,	"getenv(str)",						"returns envirnoment value of str variable"},

	{"fopen",		0, IS_FNC | RETURN_FILEPTR,			0,	"ss=NULL",		fopen_me,			F_FILE,	"fopen(fname [,mode])",			"open file fname in mode (r, rw, w, etc.)"},
	{"popen",		0, IS_FNC | RETURN_FILEPTR,			0, "ss=NULL",		popen_me,			F_FILE,	"popen(cmd [,mode])",			"open a pipe to cmd in mode"},
	{"fclose",		0, IS_FNC,									0,			"F",		fclose_me,			F_FILE,	"fclose(funit)",					"close an opened file handle"},	
	{"pclose",		0, IS_FNC,									0,			"F",		pclose_me,			F_FILE,	"pclose(punit)",					"close an opened pipe handle"},	
	{"feof",			0, IS_FNC,									0,			"F",		feof_me,				F_FILE,	"feof(funit)",						"test for EOF on file handle"},
	{"ferror",		0, IS_FNC,									0,			"F",		ferror_me,			F_FILE,	"ferror(funit)",					"test for ERROR on file handle"},
	{"fflush",		0, IS_FNC,									0,			"F",		fflush_me,			F_FILE,	"fflush(funit)",					"commit file writes to disk"},
	{"ftell",		0, IS_FNC,									0,			"F",		ftell_me,			F_FILE,	"ftell(funit)",					"tell current position of file handle"},
	{"fseek",		0, IS_FNC,									0,			"Fii",	fseek_me,			F_FILE,	"fseek(funit,posn,seek)",		"move file position to specified in mode seek"},
	{"fgetc",		0, IS_FNC,									0,			"F",		fgetc_me,			F_FILE,	"fgetc(funit)",					"get single character from file handle"},
	{"fgets",		0, IS_FNC | RETURN_STRING,				0,			"F",		fgets_me,			F_FILE,	"fgets(funit)",					"get one line (to NL) from file handle"},
	{"fputc",		0, IS_FNC,									0,			"iF",		fputc_me,			F_FILE,	"fputc(c,funit)",					"write one character to the file handle"},
	{"fputs",		0, IS_FNC,									0,			"sF",		fputs_me,			F_FILE,	"fputs(s,funit)",					"write string to the file handle"},
	{"fprintf",		0, IS_FPRINTF,								0,			NULL,		fprintf_me,			F_FILE,	"fprintf(funit, x,...)",		"output formatted string to file handle"},
	{"sprintf",		0, IS_FPRINTF | RETURN_STRING,		0,			NULL,		sprintf_me,			F_STR,	"sprintf(format, x,...)",		"return formatted string"},		/* KLUDGE */
	{"printf",		0, IS_FPRINTF,								0,			NULL,		printf_me,			F_FILE,	"printf(format, x,...)",		"output formatted string to console"},
	{"SEEK_SET",	0, IS_INT_VALUE,							SEEK_SET,	NULL,	0,						F_CONST,	"SEEK_SET",							"_lseek whence option for absolute position"},
	{"SEEK_CUR",	0, IS_INT_VALUE,							SEEK_CUR,	NULL,	0,						F_CONST,	"SEEK_CUR",							"_lseek whence option for relative position"},
	{"SEEK_END",	0, IS_INT_VALUE,							SEEK_END,	NULL,	0,						F_CONST,	"SEEK_END",							"_lseek whence option from end of file"},
#ifdef NT
	{"_open",		0, IS_FNC,									0, "si=-1|i=-1",	open_me,				F_FILE,	"_open(fname[,flags[,mode]])","opens standard file descriptor with mode"},
	{"_creat",		0, IS_FNC,									0, "si=-1",			creat_me,			F_FILE,	"_creat(fname)",					"creates new file with standard file descriptor"},
	{"_close",		0, IS_FNC,									0, "i",				close_me,			F_FILE,	"_close(fd)",						"closes the file descriptor (from _open)"},
	{"_write",		0, IS_FNC,									0, "is",				write_me,			F_FILE,	"_write(fd, string)",			"writes string to the open file descriptor"},
	{"_read",		0, IS_FNC | RETURN_STRING,				0,	"ii=256",		read_me,				F_FILE,	"_read(fd[,count])",				"read maximum of count (256) bytes returning string"},
	{"_query",		0,	IS_FNC | RETURN_STRING,				0,	"isi=256",		query_me,			F_FILE,	"_query(fd,str[,count])",		"combined write/read to file descriptor"},
	{"_lseek",		0, IS_FNC,									0, "iii",			lseek_me,			F_FILE,	"_lseek(fd, offset, whence)",	"set file position pointer"},
	{"_eof",			0, IS_FNC,									0,	"i",				eof_me,				F_FILE,	"_eof(fd)",							"test of end of file indicator"},
	{"_tell",		0, IS_FNC,									0,	"i",				tell_me,				F_FILE,	"_tell(fd)",						"return current file position"},
	{"_open_comx",	0,	IS_FNC,									0,	"is=NULL|i=-1", open_comx_me,		F_FILE,	"_open_comx(port[,baud[,ms]])","Serial port.  Optional baud rate (\"9600,n,8,1\") and read timeout (ms)"},
	{"_set_baud",	0,	IS_FNC,									0,	"is",				set_baud_me,		F_FILE,	"_set_baud(fd,str)",				"Set baud rate and flow parms of last opened comx (9600,n,8,1\")"},
	{"_get_baud",	0,	IS_FNC | RETURN_STRING,				0,	"i",				get_baud_me,		F_FILE,	"_get_baud(fd)",					"Return string w/ baud rate and flow parms of comx"},
	{"_set_timeout",0,IS_FNC,									0,	"ii=-1|i=-1|i=-1|i=-1|i=-1",set_timeout_me,F_FILE,	"_set_timeout(fd[,p1..p5])",	"ms RdInterval, RdPerByte, RdOverhead, WrByte and WrOverhead)"},
	{"_get_timeout",0,IS_FNC,									0, "ii=-1",			get_timeout_me,	F_FILE,	"_get_timeout(fd[,index])",	"Get timeout parameter for comx port"},
	{"beep",			0,	IS_FNC,									0, "r=880|r=150",	beep_me,				F_OS,		"beep([freq[,ms]])",				"Generate a beep tone"},
	{"O_APPEND",	0, IS_INT_VALUE,							O_APPEND,	NULL,	0,						F_CONST,	"O_APPEND",							"_open flag to append to file"},
	{"O_BINARY",	0, IS_INT_VALUE,							O_BINARY,	NULL,	0,						F_CONST,	"O_BINARY",							"_open flag to open as binary file"},
	{"O_CREAT",		0, IS_INT_VALUE,							O_CREAT,		NULL,	0,						F_CONST,	"O_CREAT",							"_open flag to create file"},
	{"O_RDONLY",	0, IS_INT_VALUE,							O_RDONLY,	NULL,	0,						F_CONST,	"O_RDONLY",							"_open flag to open read only"},
	{"O_RDWR",		0, IS_INT_VALUE,							O_RDWR,		NULL,	0,						F_CONST,	"O_RDWR",							"_open flag to open read/write"},
	{"O_TEXT",		0, IS_INT_VALUE,							O_TEXT,		NULL,	0,						F_CONST,	"O_TEXT",							"_open flag to open as text file"},
	{"O_TRUNC",		0, IS_INT_VALUE,							O_TRUNC,		NULL,	0,						F_CONST,	"O_TRUNC",							"_open flag to truncate on open"},
	{"O_WRONLY",	0, IS_INT_VALUE,							O_WRONLY,	NULL,	0,						F_CONST,	"O_WRONLY",							"_open flag to open write only"},
	{"S_IWRITE",	0, IS_INT_VALUE,							S_IWRITE,	NULL, 0,						F_CONST,	"S_IWRITE",							"_open mode to create with write permission"},
	{"S_IREAD",		0, IS_INT_VALUE,							S_IREAD,		NULL, 0,						F_CONST,	"S_IREAD",							"_open mode to create with read permission"},
#endif

	{"char",			0, IS_FNC | RETURN_STRING,				0,	"i",				rexx_char,			F_STR,	"char(i)",							"string with single ASCII character i"},
	{"strcat",		0, IS_FNC | RETURN_STRING,				0,	"ss",				rexx_concat,		F_STR,	"strcat(s1,s2)",					"concatenates strings s1 and s2"},
	{"concat",		0, IS_FNC | RETURN_STRING,				0,	"ss",				rexx_concat,		F_STR,	"concat(s1,s2)",					"concatenates strings s1 and s2"},
	{"upcase",		0, IS_FNC | RETURN_STRING,				0,	"s",				rexx_upcase,		F_STR,	"upcase(str)",						"uppercases all characters in str"},
	{"uppercase",	0, IS_FNC | RETURN_STRING,				0,	"s",				rexx_upcase,		F_STR,	"uppercase(str)",					"uppercases all characters in str"},
	{"lowcase",		0, IS_FNC | RETURN_STRING,				0,	"s",				rexx_lowercase,	F_STR,	"lowcase(str)",					"lowercases all characters in str"},
	{"lowercase",	0, IS_FNC | RETURN_STRING,				0,	"s",				rexx_lowercase,	F_STR,	"lowercase(str)",					"lowercases all characters in str"},

	{"isalnum",		0,	IS_FNC,									0, "i",				ctype_isalnum,		F_STR,	"isalnum(chr)",					"returns non-zero if alphanumeric character"},
	{"isalpha",		0,	IS_FNC,									0, "i",				ctype_isalpha,		F_STR,	"isalpha(chr)",					"returns non-zero if a-z or A-Z"},
	{"iscntrl",		0,	IS_FNC,									0, "i",				ctype_iscntrl,		F_STR,	"iscntrl(chr)",					"returns non-zero if control character"},
	{"isdigit",		0,	IS_FNC,									0, "i",				ctype_isdigit,		F_STR,	"isdigit(chr)",					"returns non-zero if 0-9"},
	{"isgraph",		0,	IS_FNC,									0, "i",				ctype_isgraph,		F_STR,	"isgraph(chr)",					"returns non-zero if printable non-space"},
	{"islower",		0,	IS_FNC,									0, "i",				ctype_islower,		F_STR,	"islower(chr)",					"returns non-zero if lowercase a-z"},
	{"isprint",		0,	IS_FNC,									0, "i",				ctype_isprint,		F_STR,	"isprint(chr)",					"returns non-zero if printable (including space)"},
	{"ispunct",		0,	IS_FNC,									0, "i",				ctype_ispunct,		F_STR,	"ispunct(chr)",					"returns non-zero if printable but not space or isalnum()"},
	{"isspace",		0,	IS_FNC,									0, "i",				ctype_isspace,		F_STR,	"isspace(chr)",					"returns non-zero if blank (space, tab, etc.)"},
	{"isupper",		0,	IS_FNC,									0, "i",				ctype_isupper,		F_STR,	"isupper(chr)",					"returns non-zero if lowercase A-Z"},
	{"isxdigit",	0,	IS_FNC,									0, "i",				ctype_isxdigit,	F_STR,	"isxdigit(chr)",					"returns non-zero if hexadecimal digit"},
	{"tolower",		0,	IS_FNC,									0, "i",				ctype_tolower,		F_STR,	"tolower(chr)",					"returns lowercase chr if isalpha()"},
	{"toupper",		0,	IS_FNC,									0, "i",				ctype_toupper,		F_STR,	"toupper(chr)",					"returns uppercase chr if isalpha()"},

	{"abbrev",		0, IS_FNC,									0,	"ssi=-1",		rexx_abbrev,		F_REXX,	"abbrev(s1,s2,n)",				"is s2 is an abbreviation for s1 with n chars"},
	{"center",		0, IS_FNC | RETURN_STRING,				0, "sii=32",		rexx_center,		F_REXX,	"center(str,n [,c1]])",			"center justify str in n-length string, padding with c1 (space)"},
	{"centre",		0, IS_FNC | RETURN_STRING,				0, "sii=32",		rexx_center,		F_REXX,	"centre(str,n [,c1]])",			"center justify str in n-length string, padding with c1 (space)"},
	{"compare",		0,	IS_FNC,									0,	"ssi=-1",		rexx_compare,		F_REXX,	"compare(s1,s2 [,pad])",		"compare s1 and s2 returning index of first mismatch or -1 if all match"},
	{"copies", 		0, IS_FNC | RETURN_STRING,				0,	"si",				rexx_copies,		F_REXX,	"copies(str,n)",					"duplicates str n times"},
	{"delstr",		0, IS_FNC | RETURN_STRING,				0,	"sii=-1",		rexx_delstr,		F_REXX,	"delstr(str,n[,len])",			"delete substring from str"},
	{"delword",		0, IS_FNC | RETURN_STRING,				0,	"sii=-1",		rexx_delword,		F_REXX,	"delword(str,n[,len])",			"delete substring words from str"},
	{"index",		0,	IS_FNC,									0, "ssi=0",			rexx_pos,			F_REXX,	"index(s1,s2 [,n])",				"index of s1 in s2[n...] - returns -1 if not present"},
	{"insert",		0, IS_FNC | RETURN_STRING,				0,	"ssi=0|i=-1|i=32", rexx_insert,	F_REXX,	"insert(str,new[,posn[,len[,pad]]])",	"insert new string into current str"},
	{"justify",		0, IS_FNC | RETURN_STRING,				0,	"sii=32",		rexx_justify,		F_REXX,	"justify(str,len[,pad])",		"left/right justify words to specified length"},
	{"lastpos",		0, IS_FNC,									0, "ssi=-1",		rexx_lastpos,		F_REXX,	"lastpos(s1,s2 [,n]",			"index of last occurance of s1 in s2, ignoring last n chrs - returns -1 if absent"},
	{"left",			0, IS_FNC | RETURN_STRING,				0,	"sii=32",		rexx_left,			F_REXX,	"left(str,n [,c1]])",			"left justify str in n-length string, padding with c1 (space)"},
	{"length",		0, IS_FNC,									0,		"s",			strlen_me,			F_REXX,	"length(str)",						"length of string (# of chars to terminating null)"},
	{"overlay",		0, IS_FNC | RETURN_STRING,				0,	"ssi=0|i=-1|i=32", rexx_overlay,	F_REXX,	"overlay(str,new[,posn[,len[,pad]]])",	"overlay new string into current str"},
	{"pos",			0,	IS_FNC,									0,	"ssi=0",			rexx_pos,			F_REXX,	"pos(s1,s2 [,n])",				"index of s1 in s2[n...] - returns -1 if not present"},
	{"reverse",		0, IS_FNC | RETURN_STRING,				0,	"s",				rexx_reverse,		F_REXX,	"reverse(str)",					"reverses order of characters in str"},
	{"right",		0, IS_FNC | RETURN_STRING,				0,	"sii=32",		rexx_right,			F_REXX,	"right(str,n [,c1]])",			"right justify str in n-length string, padding with c1 (space)"},
	{"space",		0, IS_FNC | RETURN_STRING,				0,	"si=1|i=32",	rexx_space,			F_REXX,	"space(str[,n[,pad]])",			"evenly spaces words in string"},
	{"strip",		0, IS_FNC | RETURN_STRING,				0, "ss=NULL|i=0",	rexx_strip,			F_REXX,	"strip(str [,mode [,chr]])",	"strips leading/trailing/both/all blanks (or chr) from str"},
	{"substr",		0, IS_FNC | RETURN_STRING,				0,	"sii=-1|i=32",	rexx_substr,		F_REXX,	"substr(str,n[,len[,pad]])",	"returns string starting at str[n] of length len"},
	{"subword",		0, IS_FNC | RETURN_STRING,				0,	"sii=-1",		rexx_subword,		F_REXX,	"subword(str,n [,len])",		"returns string starting at word n for len words"},
	{"translate",	0, IS_FNC | RETURN_STRING,				0,	"ss=NULL|s=NULL|i=0",rexx_translate,F_REXX,	"translate(str[,new,old[,pad]])",	"translates str replacing old chars with new (default uppercase)"},
	{"verify",		0, IS_FNC,									0, "sss=NULL|i=0",rexx_verify,		F_REXX,	"verify(s1,ref [,mode [,start]])",	"index of first char in s1 (not in | in) ref, skipping start. mode 'M' => not in, 'N' => in"},
	{"word",			0, IS_FNC | RETURN_STRING,				0,	"si",				rexx_word,			F_REXX,	"word(str,n)",						"returns nth word in str"},
	{"wordindex",	0, IS_FNC,									0,		"si",			rexx_wordindex,	F_REXX,	"wordindex(str, n)",				"character position of <n> word in the string"},
	{"wordlength",	0, IS_FNC,									0,		"si",			rexx_wordlength,	F_REXX,	"wordlength(str, n)",			"length of <n> word in the string"},
	{"wordpos",		0, IS_FNC,									0, "ssi=0",			rexx_wordpos,		F_REXX,	"wordpos(s1,s2 [,n])",			"word index of s1 in s2[n..] (n = words).  -1 if not present"},
	{"words",		0, IS_FNC,									0,		"s",			rexx_words,			F_REXX,	"words(str)",						"number of words in the string"},
	{"xrange",		0, IS_FNC | RETURN_STRING,				0,	"ii",				rexx_xrange,		F_REXX,	"xrange(istart, iend)",			"returns string of characters [istart,iend]"},

/** -- Potential list of additional OS functions 
   chdrive     cp
-- **/
	{"chdir",		0, IS_FNC,									0,			"s",		chdir_me,			F_OS,		"chdir(dir)",						"change directory"},
	{"cd",			0, IS_FNC,									0,			"s",		chdir_me,			F_OS,		"cd(dir)",							"change directory"},
	{"mkdir",		0, IS_FNC,									0,			"s",		mkdir_me,			F_OS,		"mkdir(dir)",						"makes the specified directory"},
	{"rmdir",		0, IS_FNC,									0,			"s",		rmdir_me,			F_OS,		"rmdir(dir)",						"removes the specified directory"},
	{"rm",			0, IS_FNC,									0,			"s",		rm_me,				F_OS,		"rm(str)",							"removes the specified file"},
	{"unlink",		0, IS_FNC,									0,			"s",		unlink_me,			F_OS,		"unlink(str)",						"unlink the specified file"},
	{"mv",			0, IS_FNC,									0,			"ss",		mv_me,				F_OS,		"mv(old,new)",						"moves file old to new"},
	{"rename",		0, IS_FNC,									0,			"ss",		mv_me,				F_OS,		"rename(old,new)",				"moves file old to new"},

/* Some binary and hex bit-wise logical operations on strings */
	{"bitor",		0, IS_FNC | RETURN_STRING,				0,			"ss",		bin_or_me,			F_CONV,	"bitor(bitstr1, bitstr2)",		"Bitwise OR  of two binary strings"},
	{"bitand",		0, IS_FNC | RETURN_STRING,				0,			"ss",		bin_and_me,			F_CONV,	"bitand(bitstr1, bitstr2)",	"Bitwise AND of two binary strings"},
	{"bitxor",		0, IS_FNC | RETURN_STRING,				0,			"ss",		bin_xor_me,			F_CONV,	"bitxor(bitstr1, bitstr2)",	"Bitwise XOR of two binary strings"},
	{"hexor",		0, IS_FNC | RETURN_STRING,				0,			"ss",		hex_or_me,			F_CONV,	"hexor(hexstr1, hexstr2)",		"Bitwise OR  of two hex strings"},
	{"hexand",		0, IS_FNC | RETURN_STRING,				0,			"ss",		hex_and_me,			F_CONV,	"hexand(hexstr1, hexstr2)",	"Bitwise AND of two hex strings"},
	{"hexxor",		0, IS_FNC | RETURN_STRING,				0,			"ss",		hex_xor_me,			F_CONV,	"hexxor(hexstr1, hexstr2)",	"Bitwise XOR of two hex strings"},

/** -- Potential list of Lex functions 
	BOOL		 LexChkToken (char *token, INT toklen);
	BOOL		 LexGetOption(char *token, INT toklen);
	BOOL		 LexGetOptionEx(char *token, INT toklen, char *list);
	INT		 LexGetInt   (INT dflt, const char *prompt);
	REAL		 LexGetReal  (REAL    dflt, const char *prompt);
	void		 LexReadLine (const char *prompt);
	void		 LexPromptStr(char *str, INT toklen, const char *prmpt);
	void		 LexPromptStrNT(char *str, INT toklen, const char *prmpt);
	BOOL		 LexPromptLine(char *str, INT toklen, const char *prmpt, BOOL translate);
	void		 LexInsText  (const char *text);
	void		 LexClrPtr	 (void);
	void		 LexFlush    (void);
	void		 LexBackup	 (void);
	void		 LexGetRest  (char *str, INT toklen);
	void		 LexGetRest  (char *str, INT toklen);
	BOOL		 LexSingle(BOOL dflt, const char *cmmd, const char *prmpt);
#define	 LexOnOff(dflt,prmpt) LexChoice(dflt, "ON", "OFF", prmpt)
#define	 LexYesNo(dflt,prmpt) LexChoice(dflt, "YES", "NO", prmpt)
-- **/
	{"LexGetToken",	0,	IS_FNC,								0,			"p",		lex_get_token,		F_STR,	"LexGetToken(str)",				"fills str with gets next token from command line"},		/* int = LexGetToken(string_ptr) */
	{"LexGetTokenP",	0, IS_FNC,								0,			"ps",		lex_get_token_p,	F_STR,	"LexGetTokenP(str,prompt)",	"gets next token from command line with prompt str"},		/* int = LexGetTokenP(string_ptr, prompt) */
	{"LexChkToken",	0,	IS_FNC,								0,			"p",		lex_chk_token,		F_STR,	"LexChkToken(str)",				"checks on next token from command line"},					/* int = LexChkToken(string_ptr) */

	{NULL,				0, 0,										0,			NULL,		0,						F_STR,	NULL,									NULL}
};

/* ===========================================================================
-- Routine to print out the detail information about the internal functions
-- in the function evaluator.
--
-- Usage: void GVPrintFncList(int detail);
--
-- Input: detail - 0 ==> only the names
--                 1 ==> names and short description
--                 2 ==> names and short description
--        name   - detail = 1 & name != NULL
--                   print only data on that function or class of functions.
--                 detail = 2
--                   print data on function matching description to name
--
-- Output: Prints information to the screen
--
-- Return: none
=========================================================================== */
void GVPrintFncList(int detail, char *name) {

	size_t i,j, ncnt,usage_lenmax, name_lenmax;
	char format[20];
	FNC_DEF *ptr;
	struct {
		FNC_CLASS type;
		int  minmatch;
		char *desc;
	} list[] = { {F_CONST,	5, "Constants"},
					 {F_BASIC,	5, "Basic functions"},
					 {F_TRIG,	4, "Trigometric functions"},
					 {F_SPEC,	4, "Special functions"},
					 {F_POLY,	4, "Polynomial functions"},
					 {F_CONV,	4, "Conversion functions"},
					 {F_FUNC,	4, "Function of functions"},
					 {F_STAT,	4, "Statistical functions"},
					 {F_VAR,		3, "Variable monitoring"},
					 {F_STR,		3, "String functions"},
					 {F_REXX,	4, "REXX string functions"},
					 {F_FILE,	4, "File manipulation functions"},
					 {F_OS,		2, "OS (operating system) functions"}
				};
#define	NUM_LIST	(sizeof(list)/sizeof(*list))

/* Determine the longest of the "name", "usage" strings */
	usage_lenmax = name_lenmax = 0;
	for (ptr=fncs; ptr->name!=NULL; ptr++) {
		if (strlen(ptr->name)  > name_lenmax)  name_lenmax  = strlen(ptr->name);
		if (strlen(ptr->usage) > usage_lenmax) usage_lenmax = strlen(ptr->usage);
	}

/* Print a brief list of all commands in usage format */
	if (detail < 0) {
		ncnt = 80/(name_lenmax+1);
		sprintf(format, "%%-%ds", name_lenmax+1);
		if (ncnt < 3) {
			ncnt = 3;
			sprintf(format, "%%-26s");
		}
		for (i=0; i<NUM_LIST; i++) {
			ScrSetAttrib(D_BOLD); TTYprintf("%s\n", list[i].desc); ScrSetAttrib(D_NORMAL);
			j = 0;									/* Number currently on line */
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (ptr->type != list[i].type) continue;
				TTYprintf((++j==ncnt)?" %s\n":format, ptr->name);
				if (j == ncnt) j = 0;
			}
			if (j != 0) TTYputc('\n');
		}
		
	} else if (detail == 0 && name == NULL) {
		ncnt = 80/(usage_lenmax+1);
		if (ncnt < 3) ncnt = 3;
		sprintf(format, " %%-%ds", usage_lenmax+1);
		for (i=0; i<NUM_LIST; i++) {
			ScrSetAttrib(D_BOLD); TTYprintf("%s\n", list[i].desc); ScrSetAttrib(D_NORMAL);
			j = 0;									/* Number currently on line */
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (ptr->type != list[i].type) continue;
				TTYprintf((++j==ncnt)?" %s\n":format, ptr->usage);
				if (j == ncnt) j = 0;
			}
			if (j != 0) TTYputc('\n');
		}

	} else if (detail == 0) {
		ncnt = 80/(usage_lenmax+1);
		if (ncnt < 3) ncnt = 3;
		sprintf(format, " %%-%ds", usage_lenmax+1);
		for (i=0; i<NUM_LIST; i++) {
			if (! LexEqual(name, list[i].desc, list[i].minmatch)) continue;
			ScrSetAttrib(D_BOLD); TTYprintf("%s\n", list[i].desc); ScrSetAttrib(D_NORMAL);
			j = 0;									/* Number currently on line */
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (ptr->type != list[i].type) continue;
				TTYprintf((++j==ncnt)?" %s\n":format, ptr->usage);
				if (j == ncnt) j = 0;
			}
			if (j != 0) TTYputc('\n');
		}
		
	} else if (detail == 1 && name == NULL) {
		sprintf(format, " %%-%ds%%s\n", usage_lenmax+2);
		for (i=0; i<NUM_LIST; i++) {
			ScrSetAttrib(D_BOLD); TTYprintf("%s\n", list[i].desc); ScrSetAttrib(D_NORMAL);
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (ptr->type != list[i].type) continue;
				TTYprintf(format, ptr->usage, ptr->desc);
			}
		}

	} else if (detail == 1) {
		sprintf(format, " %%-%ds%%s\n", usage_lenmax+2);
		for (i=0; i<NUM_LIST; i++) {
			if (LexEqual(name, list[i].desc, list[i].minmatch)) break;
		}
		if (i < NUM_LIST) {
			ScrSetAttrib(D_BOLD); TTYprintf("%s\n", list[i].desc); ScrSetAttrib(D_NORMAL);
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (ptr->type != list[i].type) continue;
				TTYprintf(format, ptr->usage, ptr->desc);
			}
		} else {
			for (ptr=fncs; ptr->name!=NULL; ptr++) {
				if (SysCheckMatch(ptr->name, name)) TTYprintf(format, ptr->usage, ptr->desc);
			}
		}
	} else {
		char token[LONG_STR_SIZE];
		if (name == NULL) {
			strcpy(token, "*");
		} else if (strchr(name, '*') == NULL && strchr(name, '?') == NULL) {
			sprintf(token, "*%s*", name);
		} else {
			strcpy(token, name);
		}
		sprintf(format, " %%-%ds%%s\n", usage_lenmax+2);
		ScrSetAttrib(D_BOLD); TTYprintf("Function apropos: %s\n", token); ScrSetAttrib(D_NORMAL);
		for (ptr=fncs; ptr->name!=NULL; ptr++) {
			if (SysCheckMatch(ptr->name, token) || SysCheckMatch((char *) ptr->desc, token)) TTYprintf(format, ptr->usage, ptr->desc);
		}
	}
	
	return;
}


PRIVATE FNC_DEF *find_fnc(char *name, int magic, int len) {
	
	FNC_DEF *entry;
	static BOOL first=TRUE;

	if (first) {										/* If first time here, create */
		first = FALSE;									/* the magic # corresponding	*/
		entry = fncs;									/* to each function name		*/
		while (entry->name != NULL) {
			entry->magic = gv_make_magic(entry->name);
			entry++;
		}
	}

	if (*expr == '$') return(NULL);				/* Internal fncs don't use $	*/
	entry = fncs;										/* Start searching the list	*/
	while (entry->name != NULL) {					/* If magic & name match, OK!	*/
		if (entry->magic == magic) {
			if (strnicmp(entry->name, name, len) == 0) break;
		}
		entry++;
	}
	
	return( (entry->name == NULL) ? NULL : entry);
}


/* ---------------------------------------------------------------------------
-- This routine handles the lookup and implementation of internal functions
-- such as the sin/cos etc.  Also has constants PI and E and some array
-- functions @min, @max etc.
--
-- Usage:  int get_internal_fnc(int magic, int len, char *endptr, GVP_PARSEMODE mode);
--
-- Inputs: magic - magic value of the name currently sitting at expr
--         len   - length of the name sitting at expr
--         *endptr - pointer to end of the name (ie next valid character in expr)
--         mode  - type of parse to use (GVP_NUMERIC, GVP_STRING, GVP_FILEPTR)
--
-- Output: none
--
-- Returns: -1 ==> current expr does not point to an internal function name
--           0 ==> successful execution of the name
--
-- Modifies: cmd, expr
--
-- Notes: function cannot start with $.
--------------------------------------------------------------------------- */
#define	MAXPTRS	(10)
PRIVATE int get_internal_fnc(int magic, int len, char *endptr, GVP_PARSEMODE mode) {

	FNC_DEF *entry;
	GV_ENTRY *varentry, *varentry2;
	int i, opcode, icnt, nargs, rcode, FncType;
	char *args, *tmpexpr, tmpstr[32], errmsg[80], *aptr,*bptr;
	BOOL use_default, is_curve;
	TMPREAL xtmp;

	void *ptrs[MAXPTRS];										/* Pointers obtained during argument parse */
	int nptrs;													/* but put on stack after the command */

	if ( (entry = find_fnc(expr, magic, len)) == NULL) return(-1);

/* Check if function returns string, and then allow only in STRING_MODE */
	if ( ((entry->fnctype & (RETURN_STRING | RETURN_FILEPTR)) != 0) != (mode != GVP_NUMERIC))
		return(-1);
	
/* -- We have several potential function types, each handled a bit different */
	FncType = entry->fnctype & 0x0FFF;				/* Get the function type	*/
	nargs   = entry->nargs;								/* Number of arguments		*/
	args    = (char *) entry->argtypes;				/* Argument types (string where defined) */
	opcode  = entry->opcode;							/* Opcode (actual operation) */
	switch (FncType) {

		case IS_CMPLX:										/* Simple complex constant	*/
		case IS_CONST:										/* Simple constant load		*/
			if (FncType == IS_CMPLX) cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
			GVPUTCMD(cmd, entry->opcode);
			expr = endptr;									/* Skip over the constant	*/
			break;
		case IS_INT_VALUE:								/* Just load an integer value */
			xtmp = nargs;									/* Default value					*/
			GVPUTCMD(cmd, load_immed_real);			/* Simple load						*/
			GVPUTITEM(cmd, xtmp, TMPREAL);			/* And value						*/
			expr = endptr;									/* Skip over the constant	*/
			break;
		case IS_MULTI:										/* Fncs like min(a,b,c,d)	*/
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if ( (rcode=get_number_value()) != 0) return(rcode);		/* First value */
			while (*(expr-1) != ')') {											/* And forever */
				if ( (rcode=get_number_value()) != 0) return(rcode);	/* Get second */
				GVPUTCMD(cmd, entry->opcode);			/* And perform min */
			}
			break;
		case IS_ARBITRARY:								/* Fncs like ave(a,b,c,d)	*/
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/
			icnt = 0;
			while (*(expr-1) != ')') {											/* And forever */
				if ( (rcode=get_number_value()) != 0) return(rcode);	/* Get args */
				icnt++;
			}
			GVPUTCMD(cmd, entry->opcode);				/* Ask to do the operation	*/
			GVPUTITEM(cmd, icnt, int);					/* w/ number of arguments */
			break;

/* Args types specified as the list string */
		case IS_CFNC:										/* Is complex function		*/
		case IS_FNC:
			if (FncType == IS_CFNC) cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if (args == NULL || *args == '\0') {	/* If no arguments, tolerate possibly one */
				if (*expr == ')') {
					expr++; paren_count--;
				} else {										/* Dump arguments now */							
					ptrdiff_t l1;
					l1 = cmd-cmdstart;
					if ( (rcode=get_number_value()) != 0) return(rcode);
					cmd = cmdstart+l1;
					if (*(expr-1) != ')') {						/* Better be end of args */
						gv_err_msg("Too many arguments for function call");
						return(1);
					}
				}
				GVPUTCMD(cmd, opcode);					/* Push argument now */
				break;
			}
			nptrs = 0;										/* No pointers stored yet					*/
			while (*args) {
				use_default = FALSE;						/* Not using default values yet			*/
				rcode = 0;									/* And assume okay on conversions		*/
				if ((*(expr-1) == '(' || *(expr-1) == ',') && *expr == ')') { expr++; paren_count--; }
				if (*(expr-1) == ')') {					/* Are we at end of given arguments?	*/
					use_default = (args[1] == '=');	/* Is there a default value for arg?	*/
					if (! use_default) {					/* If not, have to abort w/ error		*/
						gv_err_msg("Too few arguments for function call"); 
						return(1);
					}
				} else if (*expr == ',') {				/* Request to use default (no argument) */
					use_default = (args[1] == '=');	/* Is there a default value for arg?	*/
					if (! use_default) {					/* If not, have to abort w/ error		*/
						gv_err_msg("No default value assigned for this argument"); 
						return 1;
					}
					expr++;
				}
				if (! use_default) switch (*args) {
					case 'i':										/* Integer */
					case 'r':										/* Real */
					case 'z':										/* Complex */
						rcode = get_number_value();	break;
					case 's':										/* String expression */
						rcode = get_string_value();	break;
					case 'p':										/* String pointer expression */
						rcode = get_pointer_value(GVP_STRINGPTR);	break;
					case 'F':										/* File handle */
						rcode = get_pointer_value(GVP_FILEPTR); break;
					case 'A':										/* Array pointer */
						if ( (varentry = get_entry(NULL)) == NULL) {
							gv_err_msg("Expecting an array descriptor at this point");
							return 1;
						} else if ( (opcode == poly_me) && (varentry->type == GV_COMPLEX_ARRAY || varentry->type == GV_COMPLEX_ARRAY_LINK) ) {
							cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
							opcode = complex_poly_me;					/* Modify the opcode */
							ptrs[nptrs++] = (void *) varentry->var.c_array;
						} else if (varentry->type == GV_ARRAY || varentry->type == GV_ARRAY_LINK) {
							ptrs[nptrs++] = (void *) varentry->var.array;
						} else {
							gv_err_msg("Element seen at this point was not an array as expected");
							return 1;
						}
						if (*(expr++) == ')') paren_count--;
						break;
					case 'C':										/* Curve pointer */
						if ( (varentry = get_entry(NULL)) == NULL ||
							  (varentry->type != GV_2DCURVE && varentry->type != GV_3DCURVE) ) {
							gv_err_msg("Expecting a 2D or 3D curve descriptor at this point");
							return 1;
						} else if (entry->opcode == curve_3d_near && varentry->type != GV_3DCURVE) {
							gv_err_msg("Must be a 3D curve descriptor at this point");
							return 1;
						}
						ptrs[nptrs++] = (void *) varentry->var.curve;
						if (*(expr++) == ')') paren_count--;
						break;
					case 'S':										/* Surface pointer */
						if ( (varentry = get_entry(NULL)) == NULL || varentry->type != GV_SURFACE) {
							gv_err_msg("Expecting a surface descriptor at this point");
							return 1;
						}
						ptrs[nptrs++] = (void *) varentry->var.surface;
						if (*(expr++) == ')') paren_count--;
						break;
					default:
						sprintf(errmsg, "No method for argument of type %c (Programmer screwup GVPARSE)\n", *args);
						gv_err_msg(errmsg);
						return 1;
				} else {
					aptr = args+2; bptr = tmpstr;				/* Copy default text into single string */
					while (*aptr && *aptr != '|') *(bptr++) = *(aptr++);
					*bptr = '\0';
					switch (*args) {
						case 'i':									/* Integer */
						case 'r':									/* Real */
						case 'z':									/* Complex */
							xtmp = strtod(tmpstr, NULL);		/* Default value	*/
							GVPUTCMD(cmd, load_immed_real);	/* Simple load		*/
							GVPUTITEM(cmd, xtmp, TMPREAL);	/* And value		*/
							break;
						case 's':									/* String expression */
							cmdinfo.type |= GV_INFO_STRING_REFERENCE;		/* Normally done by get_string_value() */
							if (strcmp(tmpstr, "NULL") == 0) {
								aptr = NULL;
								GVPUTCMD(cmd, load_string_ptr);	/* Load a string pointer */
								GVPUTITEM(cmd, aptr, CHAR *);
							} else {
								GVPUTCMD(cmd, load_string_tmp);
								strcpy((char *) cmd, tmpstr);
								cmd += strlen(tmpstr)+1;
							}
							break;
						case 'A':									/* Array pointer */
							tmpexpr = expr;
							expr = tmpstr;							/* Find the array (lc) itself	*/
							varentry = get_entry(NULL);
							expr = tmpexpr;						/* Restore this puppy		*/
							if (varentry == NULL) {
								sprintf(errmsg, "Default array (%s) not found", tmpstr);
								gv_err_msg(errmsg);
								return 1;
							} else if (varentry->type == GV_ARRAY || varentry->type == GV_ARRAY_LINK) {
								ptrs[nptrs++] = (void *) varentry->var.array;
							} else {
								sprintf(errmsg, "Default array (%s) not an array as expected", tmpstr);
								gv_err_msg(errmsg);
								return(1);
							}
							break;
						default:
							sprintf(errmsg, "No method for default arg of type %c (GVPARSE)\n", *args);
							gv_err_msg(errmsg);
							return 1;
					}
				}
				args++;
				if (*args == '=') {											/* Skip over default value */
					args++;
					while (*args && *args != '|') args++;
					if (*args == '|') args++;
				}
				if (rcode != 0) return(rcode);
			}
			if (*(expr-1) != ')') {						/* Better be end of args */
				gv_err_msg("Too many arguments for function call");
				return(1);
			}
			GVPUTCMD(cmd, opcode);						/* Push argument now */
			for (i=0; i<nptrs; i++) {					/* Push the post command pointers now */
				GVPUTITEM(cmd, ptrs[i], void *);				
			}
			break;

		case IS_R_ARRAY:									/* Reversed order array		*/
		case IS_R2_ARRAY:									/* Looks very similar		*/
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if ( (varentry = get_entry(NULL)) == NULL) {	/* Array must be first */
				gv_err_msg("Expecting an array descriptor at this point");
				return(1);
			} else if (varentry->type == GV_2DCURVE && FncType == IS_R_ARRAY) {
				is_curve = TRUE;
			} else if (varentry->type == GV_ARRAY || varentry->type == GV_ARRAY_LINK) {
				is_curve = FALSE;
			} else {
				gv_err_msg("Element seen at this point was not an array/curve as expected");
				return(1);
			}
			if (FncType == IS_R2_ARRAY) {				/* Need another array */
				if (*expr != ',') {
					gv_err_msg("Expecting a second array pointer here");
					return(1);
				}
				expr++;
				if ( (varentry2 = get_entry(NULL)) == NULL) {	/* Array must be first */
					gv_err_msg("Expecting an array descriptor at this point");
					return(1);
				} else if (varentry2->type != GV_ARRAY && varentry2->type != GV_ARRAY_LINK) {
					gv_err_msg("Element seen at this point was not an array as expected");
					return(1);
				}
			}

/* Now, either zero arguments -> evaluate immediately, or narg arguments */
			if (*expr == ')') {							/* No arguments				*/
				paren_count--; expr++;
				xtmp = 0.0;
				if (FullParse) {
					if (FncType == IS_R_ARRAY) {				/* Have options here */
						if (is_curve) {
							xtmp = gv_eval_curve_fnc(entry->opcode, varentry->var.curve, -REAL_MAX, REAL_MAX);
						} else {
							xtmp = gv_eval_array_fnc(entry->opcode, varentry->var.array, 0, INT_MAX);
						}
					} else if (FncType == IS_R2_ARRAY) {
						xtmp = gv_eval_array_fnc2(entry->opcode, varentry->var.array, varentry2->var.array, 0, INT_MAX);
					}
				}
				GVPUTCMD(cmd, load_immed_real);				/* Simple load		*/
				GVPUTITEM(cmd, xtmp, TMPREAL);				/* And value		*/
			} else if (*expr != ',') {
				gv_err_msg("Expecting either a ) or a , at this point - invalid character");
				return(1);
			} else {
				expr++;
				while (nargs--) {							/* Get # of arguments		*/
					if (*(expr-1) == ')') {				/* But watch for too few	*/
						gv_err_msg("Too few arguments for function call"); 
						return(1);
					}
					if ( (rcode=get_number_value()) != 0) return(rcode);
				}
				if (*(expr-1) != ')') {					/* Better be end of args */
					gv_err_msg("Too many arguments for function call");
					return(1);
				}
				GVPUTCMD(cmd, entry->opcode);			/* Push argument now */
				*(cmd++) = (unsigned char) (is_curve ? 1 : 0);
				GVPUTITEM(cmd, varentry->var.array, ARRAY *);
				if (FncType == IS_R2_ARRAY) GVPUTITEM(cmd, varentry2->var.array, ARRAY *);
			}
			break;

		case IS_CURVE_EVAL:								/* Curve @fncs evaluated here	*/
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if ( ((varentry = get_entry(NULL)) == NULL) ||
				(varentry->type != GV_2DCURVE && varentry->type != GV_3DCURVE) ) {
				gv_err_msg("Expecting a 2D or 3D curve descriptor at this point");
				return(1);
			}

/* Now, either zero arguments -> evaluate immediately, or narg arguments */
			if (*expr == ')') {							/* No arguments				*/
				paren_count--; expr++;
				xtmp = 0;
				if (FullParse) {								/* Must we interpret?		*/
					xtmp = gv_eval_curve(entry->opcode, varentry->var.curve, -REAL_MAX, REAL_MAX, &rcode);
					if (rcode != 0) return(rcode);
				}
				GVPUTCMD(cmd, load_immed_real);			/* Simple load		*/
				GVPUTITEM(cmd, xtmp, TMPREAL);			/* And value		*/
			} else if (*expr != ',') {
				gv_err_msg("Expecting either a ) or a , at this point - invalid character");
				return(1);
			} else {
				expr++;
				while (nargs--) {							/* Get # of arguments		*/
					if (*(expr-1) == ')') {				/* But watch for too few	*/
						gv_err_msg("Too few arguments for function call"); 
						return(1);
					}
					if ( (rcode=get_number_value()) != 0) return(rcode);
				}
				if (*(expr-1) != ')') {					/* Better be end of args */
					gv_err_msg("Too many arguments for function call");
					return(1);
				}
				GVPUTCMD(cmd, entry->opcode);			/* Push argument now */
				GVPUTITEM(cmd, varentry->var.curve, CURVE *);
			}
			break;

		case IS_SURF_EVAL:								/* Surface @fncs evaluated here	*/
		{
			TMPREAL xtmp=0.0;

			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if ( ((varentry = get_entry(NULL)) == NULL) ||
				  (varentry->type != GV_SURFACE) ) {
				gv_err_msg("Expecting a surface descriptor at this point");
				return(1);
			}
			if (*expr != ')') {
				gv_err_msg("Too many arguments for function call");
				return(1);
			}
			paren_count--; expr++;

			if (FullParse) {								/* Must we interpret?		*/
				rcode = gv_eval_surface(entry->opcode, &xtmp, varentry->var.surface);
				if (rcode != 0) return(rcode);
			}

			GVPUTCMD(cmd, load_immed_real);			/* Simple load		*/
			GVPUTITEM(cmd, xtmp, TMPREAL);			/* And value		*/
			break;
		}

		case IS_QUERY:
		{
			char *ptr;
			int ilen;
			TMPREAL xtmp;

			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/
			if ( (varentry = get_entry(&ptr)) == NULL) {	/* Does it exist		*/
				if (entry->opcode == SIZEOF_OP) {
					gv_err_msg("Expecting a variable descriptor at this point");
					return(1);
				} else {										/* If not, dump anyway		*/
					expr = ptr;								/* Jump over it anyway		*/
				}
			}
			if (*expr != ')') {
				gv_err_msg("Too many arguments for sizeof/typeof/exists functions");
				return(1);
			}
			paren_count--; expr++;

			ilen = 0;										/* Resolves compiler complaints */
			if (entry->opcode == TYPEOF_OP) {
				ilen = (varentry == NULL) ? -1 : varentry->type;
			} else if (entry->opcode == EXISTS_OP) {
				ilen = (varentry == NULL) ? 0 : 1 ;
			} else if (entry->opcode == SIZEOF_OP) {
				switch (varentry->type) {
					case GV_REAL:
					case GV_REAL_LINK:
					case GV_DOUBLE:
					case GV_DOUBLE_LINK:
					case GV_INT:
					case GV_INT_LINK:
					case GV_COMPLEX:
					case GV_COMPLEX_LINK:
					case GV_FUNCTION:
					case GV_FUNCTION_LINK:
					case GV_FUNCTION_A_LINK:
					case GV_STR_FUNCTION_LINK:
					case GV_POINTER:
					case GV_FILEPTR:
						ilen = 1;
						break;
					case 	GV_ARRAY:
					case 	GV_ARRAY_LINK:
						ilen = varentry->var.array->maxsize;
						break;
					case 	GV_INT_ARRAY:
					case 	GV_INT_ARRAY_LINK:
						ilen = varentry->var.i_array->maxsize;
						break;
					case 	GV_DOUBLE_ARRAY:
					case 	GV_DOUBLE_ARRAY_LINK:
						ilen = varentry->var.d_array->maxsize;
						break;
					case 	GV_COMPLEX_ARRAY:
					case 	GV_COMPLEX_ARRAY_LINK:
						ilen = varentry->var.c_array->maxsize;
						break;
					case 	GV_2DCURVE:
					case 	GV_3DCURVE:
						ilen = varentry->var.curve->nptmax;
						break;
					case 	GV_SURFACE:
						ilen = varentry->var.surface->ncol*varentry->var.surface->nrow;
						break;
					case 	GV_STRING:
						ilen = 256;
						break;
					case 	GV_STRING_LINK:
						ilen = varentry->extra;
						break;
					case GV_STRING_ARRAY:
						ilen = varentry->var.s_array->maxsize;
						break;
					default:
						ERRprintf("ERROR: entry type was not recognized (programming screwup!) - %d)\n", varentry->type);
						return(1);
				}
			}
				
			GVPUTCMD(cmd, load_immed_real);			/* Simple load		*/
			xtmp = ilen;									/* Turn into real	*/
			GVPUTITEM(cmd, xtmp, TMPREAL);			/* And value		*/
			break;
		}

		case IS_SPC: {
			ARRAY *ar[2];								/* Need 2 arrays potentially */
			char *hold_expr, achr;					/* For backing up the expression tree */
			TMPREAL xtmp, mean, stdev;

			switch (entry->opcode) {
				case IS_Z_TEST:								/* Needs <ar,mean,stdev> */

					if (*endptr != '(') return(-1);		/* Must have a ( sign							*/
					expr = endptr+1; paren_count++;		/* Point past the ( sign and track parens	*/

					if ( (ar[0] = get_array_entry()) == NULL) return 1;
					if (nargs != 0) {
						if (*expr++ != ',') {
							gv_err_msg("Expecting more arguments here, but none found");
							return(1);
						}
						if ( (rcode = get_multiple_args(nargs)) != 0) return rcode;		/* True mean / stdev */
					}
					if (*(expr-1) != ')') {									/* Better be end of args */
						gv_err_msg("Too many arguments for function call");
						return(1);
					}

					gv_array_stats(ar[0], &icnt, &mean, NULL);		/* Get values of the array */
					GVPUTCMD(cmd, z_test_me);								/* Push command now */
					GVPUTITEM(cmd, icnt, int);								/* Number of elements in the array */
					GVPUTITEM(cmd, mean, TMPREAL);						/* Observed mean of the array */
					break;

				case IS_T_TEST:
					if (*endptr != '(') return(-1);		/* Must have a ( sign		*/
					expr = endptr+1; paren_count++;		/* Point past the ( sign and track parens	*/

					if ( (ar[0] = get_array_entry()) == NULL) return 1;
					if (*expr++ != ',') {
						gv_err_msg("Expecting a second argument, but none found");
						return(1);
					}

					/* Second argument can be an array or an expression */
					hold_expr = expr;							/* Hold so can reset if needed */
					if ( (varentry = get_entry(NULL)) != NULL &&											/* Is it a name? */
						  (varentry->type == GV_ARRAY || varentry->type == GV_ARRAY_LINK) &&		/* Is it an array? */
						  *expr == ')') {																			/* And is that it? */
						expr++; paren_count--;																	/* Then format is (ar1,ar2) */
						ar[1] = varentry->var.array;

						xtmp = gv_eval_statistics(ar, 'T');
						GVPUTCMD(cmd, load_immed_real);				/* Load the result */
						GVPUTITEM(cmd, xtmp, TMPREAL);

					} else {													/* Retry for a single number with just mean test */
						expr = hold_expr;
						if ( (rcode = get_multiple_args(1)) != 0) return rcode;		/* True mean */
						if (*(expr-1) != ')') {									/* Better be end of args */
							gv_err_msg("Too many arguments for function call");
							return(1);
						}
						gv_array_stats(ar[0], &icnt, &mean, &stdev);	/* Get values of the array */
						GVPUTCMD(cmd, t1_test_me);								/* Push command now */
						GVPUTITEM(cmd, icnt, int);								/* Number of elements in the array */
						GVPUTITEM(cmd, mean, TMPREAL);						/* Observed mean of the array */
						GVPUTITEM(cmd, stdev, TMPREAL);						/* Observed stdev of the array */
					}
					break;

				case IS_TD_TEST:
				case IS_U_TEST:
				case IS_F_TEST:
					/* Set the type of test being done (at calc only) */
					if (entry->opcode == IS_TD_TEST)		  achr = 'D';	/* Dependent test */
					else if (entry->opcode == IS_U_TEST)  achr = 'U';	/* Different variance T-test */
					else											  achr = 'F';	/* F-test */

					if (*endptr != '(') return(-1);		/* Must have a ( sign		*/
					expr = endptr+1; paren_count++;		/* Point past the ( sign and track parens	*/

					if ( (ar[0] = get_array_entry()) == NULL) return 1;
					if (*expr++ != ',') { gv_err_msg("Too few arguments"); return(1); }
					if ( (ar[1] = get_array_entry()) == NULL) return 1;
					if (*expr != ')') { gv_err_msg("Too many arguments"); return(1); }
					expr++; paren_count--;

					xtmp = gv_eval_statistics(ar, achr);
					GVPUTCMD(cmd, load_immed_real);				/* Simple load		*/
					GVPUTITEM(cmd, xtmp, TMPREAL);				/* And value		*/
					break;

				default:
					ERRprintf("ERROR: Huh? Unrecognized special op-code\n");
					return(1);
			}

			}	/* End of scope for all the IS_SPC cases */
			break;

		case IS_FPRINTF:									/* Take a pointer argument */
			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			/* First argument must be a FILE * variable for all but sprintf */
			if (entry->opcode != sprintf_me && entry->opcode != printf_me) {
				if ( (rcode = get_pointer_value(GVP_FILEPTR)) != 0) return(rcode);
			}
			if ( (rcode = get_string_value()) != 0) return rcode;
			/* Oh boy - now the hard work begins! */
			{			
				char type[256], *tptr;						/* Better not be that many */
				tptr = type;
				while (*(expr-1) != ')' && *expr) {
					if (GVGuessExprType(expr) == GVP_STRING) {
						*(tptr++) = 's';	rcode = get_string_value();
					} else {
						*(tptr++) = 'r';	rcode = get_number_value();
					}
					if (rcode != 0) return(rcode);
				}
				*tptr = '\0';									/* End of the order list	*/
				GVPUTCMD(cmd, load_string_tmp);
				strcpy((char *) cmd, type);
				cmd += strlen(type)+1;
			}

			if (*(expr-1) != ')') {							/* Better be end of args */
				gv_err_msg("Too many arguments for function call");
				return(1);
			} else {
				GVPUTCMD(cmd, entry->opcode);				/* Push argument now */
			}
			break;

/* Complex - essentially way to load a subfunction for multiple evaluations */
/* within a single evaluation.  Parts taken from GVAllocFnc and function    */
/* handling routines.  Essentially a "sub-parse" to be evalated with eval_real() */
		case IS_FUNCTION_EVAL:
		{
			char fnc_call[LONG_STR_SIZE], var[32], *aptr;
			int cmd_hold;									/* Always track offsets, not value */
			GVCMDS *cmdptr;
			int i, icnt, iargs, iopts;
			char *holdexpr, *holdexpstart;

			if (*endptr != '(') return(-1);			/* Must have a ( sign		*/
			expr = endptr+1;								/* Point past the ( sign	*/
			paren_count++;									/* And track parens			*/

			if (gv_copy_argument(fnc_call, expr, &expr) != 0) return 1;
			if (*expr != ',') return -1;				/* Must have arguments		*/
			expr++;

/* Get the required and possibly optional arguments */
			iargs = nargs & 0xFF;						/* Required arguments		*/
			iopts = nargs >> 8;							/* Optional arguments		*/
			for (i=0; i<iargs; i++) {					/* Get the required ones	*/
				if ( (rcode=get_number_value()) != 0) return(rcode);
			}
			for (i=0; i<iopts; i++) {					/* And check for optional	*/
				if (*(expr-1) != ',') break;			/* Not present ==> done		*/
				if (*expr == ',') {						/* Pass defaults if ','		*/
					expr++;
					GVPUTCMD(cmd, load_dummy_value);
				} else if ( (rcode = get_number_value()) != 0) {
					return(rcode);
				}
				iargs++;										/* And track actual number	*/
			}
			if (*(expr-1) != ')') {
				gv_err_msg("Too many arguments for function call");
				return(1);
			}

/* Parse the function - look for format "cos(y)-y|y" - the |y is "given y" */
/* and causes replacement variable to be y.  Makes life nicer for user     */
/* The assumed dummy variable is x for all but sum where it is i*/
			aptr = fnc_call + strlen(fnc_call) - 1;					/* Start at the end and look for last | structure */
			while (aptr != fnc_call && isalnum(*aptr) && *aptr != '|') aptr--;
			if (*aptr == '|') {
				*(aptr++) = '\0'; strncpy(var, aptr, sizeof(var));
				if (! isalpha(*aptr)) { gv_err_msg("Invalid dummy variable"); return 1; }
				while (*(++aptr)) {
					if ( (! isalnum(*aptr)) && (strchr(":$_", *aptr) == NULL)) {
						gv_err_msg("Invalid dummy variable"); return 1;
					}
				}
			} else {
				strcpy(var, "x");
				if (entry->opcode == sum_me || entry->opcode == prod_me) strcpy(var, "i");
			}
/*			TTYprintf("fnc_call: \"%s\"   var: \"%s\"   ArgTop: %d  ArgBase: %d ArgNext: %d  iargs: %d\n", fnc_call, var, ArgTop, ArgBase, ArgNext, ArgTop-ArgBase); */
			gv_replace_args(fnc_call, var, ARG_REAL, ArgNext-ArgBase, 0,0);	/* Replace X with &00 */
/*			TTYprintf("fnc_call: \"%s\"\n", fnc_call); */

/* Effectively, just need to add more element on existing variable stack for my argument */
			GVPUTCMD(cmd, load_pi);							/* Just a dummy value	*/
			GVPUTCMD(cmd, save_stack_val);				/* And put on stack		*/
			ArgNext++; ArgTop++;								/* One more there now	*/

/* Now set up for subroutine call */
			GVPUTCMD(cmd, entry->opcode);					/* Push argument now */
			*(cmd++) = iargs;									/* Push number of arguments */
			cmd_hold = (int) (cmd-cmdstart);
			cmd += sizeof(int);								/* Leave space for length of "subroutine" */

			holdexpr     = expr;								/* Save current ptrs		*/
			holdexpstart = expstart;

			expr = expstart = fnc_call;					/* Parse the function	*/
			if ( (rcode = get_number_value()) != 0) return rcode;

			expr         = holdexpr;						/* Restore last one		*/
			expstart     = holdexpstart;
			*(cmd++) = 0xFF;									/* End of the stack		*/

			icnt = (int) ((cmd-cmdstart)-cmd_hold)-sizeof(int);	/* Bytes in the expansion */
			cmdptr = cmdstart+cmd_hold;
			GVPUTITEM(cmdptr, icnt, int);					/* Store back after the @solve call */

			GVPUTCMD(cmd, pop_stack);						/* Dump my dummy argument from stack */
			*(cmd++) = (unsigned char) 1;					/* Only the one */
			ArgNext--; ArgTop--;								/* Update pointers here of no dummy */

			break;
		}

		default:
			ERRprintf("VERY BAD ERROR: Found unrecognized type of function call - abuse the developers\n");
			return(1);
	}
	return(0);
}

/* ===========================================================================
-- Routine to parse for an array descriptor and return the array pointer.
-- Prints error messages if not an array as appropriate
--
-- Usage: int get_multiple_args(int nargs)
--
-- Inputs: nargs - number of real number arguments to parse
--
-- Output: Prints error message if something wrong in the expression
--
-- Returns: 0 if successful, 1 if any error.  Calling program should return
--          1 on non-zero return codes.
=========================================================================== */
static int get_multiple_args(int nargs) {

	int rcode;

	while (nargs--) {											/* Get # of arguments		*/
		if (*(expr-1) == ')') {								/* But watch for too few	*/
			gv_err_msg("Too few arguments for function call"); 
			return(1);
		}
		if ( (rcode=get_number_value()) != 0) return(rcode);
	}
	return 0;
}


/* ===========================================================================
-- Routine to parse for an array descriptor and return the array pointer.
-- Prints error messages if not an array as appropriate
--
-- Usage: ARRAY *get_array_entry()
--
-- Inputs: none
--
-- Output: Prints error message if something wrong in the expression
--
-- Returns: Pointer to an array structure.  Calling routine should
--          return (1) if this function returns a NULL.
=========================================================================== */
static ARRAY *get_array_entry(void) {
	GV_ENTRY *varentry;

	if ( (varentry = get_entry(NULL)) == NULL) {
		gv_err_msg("Expecting an array descriptor at this point");
		return NULL;
	}
	if (varentry->type != GV_ARRAY && varentry->type != GV_ARRAY_LINK) {
		gv_err_msg("Element seen at this point was not an array as expected");
		return NULL;
	}
	return varentry->var.array;
}


/* ---------------------------------------------------------------------------
-- This routine handles the lookup and implementation of allocated and linked
-- variables and arrays.  
--
-- Usage:  int get_var(int magic, int len, char *endptr, int *type);
--
-- Inputs: magic - magic value of the name currently sitting at expr
--         len   - length of the name sitting at expr
--         *endptr - pointer to end of the name (ie next valid character in expr)
--
-- Output: none
--
-- Returns:  0 ==> successful execution of the name
--          !0 ==> current expr does not point to a proper linked variable
--
-- Modifies: cmd, expr
--
-- Modification History:
--    MOT 12/28/94 - Added new code to reduce a:b expressions to only a if
--                   the compound object does not exist.  Fixes expressions
--                   parsing of the form (a<b)?a:b.
--------------------------------------------------------------------------- */
PRIVATE int get_var(int magic, int len, char *endptr, GVP_PARSEMODE mode) {

	GV_ENTRY *entry;
	int i, rcode;
	long il;
	TMPREAL xtmp;
	void *avoidptr;
	FILE **pfunit;
	char *aptr, *expr_original, *holdexpr, *holdexpstart;
	BOOL bStripColon = FALSE;
	char tmpdef[4096];										/* Allow a very large function definition */

/* Entries for function handling */
	int iargs, sargs, aargs, nargs;						/* Old stack status		*/
	ARGTYPE *argtypes;
	int MyArgBase,  ArgBaseHold,  ArgTopHold;			/* Previous arg stack	*/
	int SMyArgBase, SArgBaseHold, SArgTopHold;		/* Previous arg stack	*/

/* Code begins */
	expr_original = expr;									/* Save for errors */

/* Search list for match, using magic key first followed by exact match */
	while (TRUE) {
		for (entry=GVFirstEntry; entry!=NULL; entry=entry->next) {
			if (entry->magic == magic && strnicmp(expr,entry->varname,len) == 0)
				break;
		}
		if (entry != NULL) break;							/* Okay, really have it */
		if (mode  != GVP_NUMERIC) return(-1);			/* No other options non-NUMERIC */

/* ------
-- In numeric, might be problem of "(7<4) ? var : var" conditional expression
-- Try again by stripping back to previous : character
------ */
		endptr--;
		while (endptr>expr && *endptr!=':') endptr--;
		if (endptr <= expr) return(-1);					/* Nothing left	*/
		len = (int) (endptr-expr);							/* New length		*/
		magic = *expr;											/* New magic code	*/
		for (aptr=expr+1; aptr<endptr; aptr++) magic = MAGIC(magic, *aptr);
		bStripColon = TRUE;									/* Remember later */
	}
/* Code continues */
	expr = endptr;												/* These nominally handled */
	rcode = 0;													/* Default is all okay		*/

	switch (mode) {
		case GVP_FILEPTR:
			if (entry->type == GV_FILEPTR) {
				GVPUTCMD(cmd, load_pfile_ptr);			/* Load a file pointer */
/*				GVPUTITEM(cmd, (void *) entry->var.fileptr, void *); */
				pfunit = &entry->var.fileptr;
				GVPUTITEM2(cmd, pfunit, void *, void **);
			} else {
				gv_err_msg("Variable is not a pointer type variable");
				rcode = 1;
			}
			break;

		case GVP_STRINGPTR:									/* For functions like gettoken(string_var) */
			switch (entry->type) {
				case GV_STRING:
				case GV_STRING_LINK:
					if (entry->flags & GVF_CONSTANT) {
						gv_err_msg("Cannot pass a CONSTANT string by reference -- unmodifiable");
						return 1;
					}
					GVPUTCMD(cmd, load_varentry_ptr);	/* Load an entry pointer */
					avoidptr = entry;
					GVPUTITEM2(cmd, avoidptr, void *, void *);
					break;
				default:
					gv_err_msg("String filling by reference currently limited to simple string variables");
					return(1);
			}
			break;												/* Skips to the return(rcode) */

		case GVP_STRING:
			switch (entry->type) {
				case GV_STRING:
				case GV_STRING_LINK:
					GVPUTCMD(cmd, load_string_ptr);	/* Load a string pointer */
					GVPUTITEM(cmd, entry->var.stradr, CHAR *);
					break;
				case GV_STRING_ARRAY:
				case GV_STRING_ARRAY_LINK:
					if (*expr == '(') {								/* Element of reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
					} else {
						*cmd++ = load_i;
						if ( (cmdinfo.type & GV_INFO_UNINDEXED_ARRAY_REFERENCE) == 0) {
							cmdinfo.type   |= GV_INFO_UNINDEXED_ARRAY_REFERENCE;
							cmdinfo.length  = *entry->var.s_array->size;
						} else {
							cmdinfo.length  = min(cmdinfo.length, *entry->var.s_array->size);
						}
					}
					GVPUTCMD(cmd, load_strarray_idx);
					GVPUTITEM(cmd, entry->var.s_array, STRING_ARRAY *);
					break;
					
				case GV_FUNCTION:
					/* Strange mode - allow string functions but use code below */
					if (entry->var.function->fnctype == FNC_STRING && *expr == '(') {	/* String fnc? */
						goto HandleFunctionRef;
						break;
					}
					if (SimpleName) {
						GVPUTCMD(cmd, load_string_tmp);
						strcpy((CHAR *) cmd, expr_original);
						cmd += strlen(expr_original)+1;
					} else {
						aptr = entry->var.function->givendef;
						GVPUTCMD(cmd, load_string_ptr);	/* Load a string pointer */
						GVPUTITEM(cmd, aptr, CHAR *);
					}
					break;

				case GV_STR_FUNCTION_LINK:
					goto HandleFunctionRef;

				case GV_2DCURVE:						/* Not handled */
				case GV_3DCURVE:
				case GV_SURFACE:
					if (SimpleName) {
						GVPUTCMD(cmd, load_string_tmp);
						strcpy((CHAR *) cmd, expr_original);
						cmd += strlen(expr_original)+1;
					} else {
						aptr = (entry->type == GV_SURFACE) ? entry->var.surface->ids : entry->var.curve->ids ;
						GVPUTCMD(cmd, load_string_ptr);	/* Load a string pointer */
						GVPUTITEM(cmd, aptr, CHAR *);
					}
					break;
				default:
					gv_err_msg("Variable is not a string or simple function definition");
					return(1);
			}
			break;												/* Skips to the return(rcode) */

/* Numeric mode */
/* If we stripped a colon, only accept numerical formats, no characters */
		case GVP_NUMERIC:
			switch (entry->type) {
				case GV_FILEPTR:
					memcpy(&il, &entry->var.fileptr, min(sizeof(il),sizeof(FILE *)));
					xtmp = (TMPREAL) il;
					GVPUTCMD(cmd, load_immed_real);			/* Fake load a real */
					GVPUTITEM(cmd, xtmp, TMPREAL);
					break;
				case GV_REAL:
					GVPUTCMD(cmd, load_real);
					avoidptr = (void *) &(entry->var.floatvalue);
					GVPUTITEM(cmd, avoidptr, REAL *);
					break;
				case GV_REAL_LINK:
					GVPUTCMD(cmd, load_real);
					GVPUTITEM(cmd, entry->var.floatadr, REAL *);
					break;
				case GV_DOUBLE:
					GVPUTCMD(cmd, load_double);
					avoidptr = (void *) &(entry->var.doublevalue);
					GVPUTITEM(cmd, avoidptr, DOUBLE *);
					break;
				case GV_DOUBLE_LINK:
					GVPUTCMD(cmd, load_double);
					GVPUTITEM(cmd, entry->var.doubleadr, DOUBLE *);
					break;
				case GV_COMPLEX:
					GVPUTCMD(cmd, load_complex);
					avoidptr = (void *) &(entry->var.complexvalue);
					GVPUTITEM(cmd, avoidptr, COMPLEX *);
					cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
					break;
				case GV_COMPLEX_LINK:
					GVPUTCMD(cmd, load_complex);
					GVPUTITEM(cmd, entry->var.complexadr, COMPLEX *);
					cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
					break;
				case GV_INT:
					GVPUTCMD(cmd, load_int);
					avoidptr = (void *) &(entry->var.intvalue);
					GVPUTITEM(cmd, avoidptr, INT *);
					break;
				case GV_INT_LINK:
					GVPUTCMD(cmd, load_int);
					GVPUTITEM(cmd, entry->var.intadr, INT *);
					break;

				case GV_ARRAY:
				case GV_ARRAY_LINK:
					if (*expr == '(') {								/* Element of reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
						GVPUTCMD(cmd, load_real_idx);
					} else {
						GVPUTCMD(cmd, load_real_array);
						if ( (cmdinfo.type & GV_INFO_UNINDEXED_ARRAY_REFERENCE) == 0) {
							cmdinfo.type   |= GV_INFO_UNINDEXED_ARRAY_REFERENCE;
							cmdinfo.length  = *entry->var.array->size;
						} else {
							cmdinfo.length  = min(cmdinfo.length, *entry->var.array->size);
						}
					}
					GVPUTITEM(cmd, entry->var.array, ARRAY *);
					break;
					
				case GV_INT_ARRAY:
				case GV_INT_ARRAY_LINK:
					if (*expr == '(') {								/* Element of reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
						GVPUTCMD(cmd, load_int_idx);
					} else {
						GVPUTCMD(cmd, load_int_array);
						if ( (cmdinfo.type & GV_INFO_UNINDEXED_ARRAY_REFERENCE) == 0) {
							cmdinfo.type   |= GV_INFO_UNINDEXED_ARRAY_REFERENCE;
							cmdinfo.length  = *entry->var.i_array->size;
						} else {
							cmdinfo.length  = min(cmdinfo.length, *entry->var.i_array->size);
						}
					}
					GVPUTITEM(cmd, entry->var.i_array, INT_ARRAY *);
					break;
					
				case GV_DOUBLE_ARRAY:
				case GV_DOUBLE_ARRAY_LINK:
					if (*expr == '(') {								/* Element of reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
						GVPUTCMD(cmd, load_double_idx);
					} else {
						GVPUTCMD(cmd, load_double_array);
						if ( (cmdinfo.type & GV_INFO_UNINDEXED_ARRAY_REFERENCE) == 0) {
							cmdinfo.type   |= GV_INFO_UNINDEXED_ARRAY_REFERENCE;
							cmdinfo.length  = *entry->var.d_array->size;
						} else {
							cmdinfo.length  = min(cmdinfo.length, *entry->var.d_array->size);
						}
					}
					GVPUTITEM(cmd, entry->var.d_array, DOUBLE_ARRAY *);
					break;
					
				case GV_COMPLEX_ARRAY:
				case GV_COMPLEX_ARRAY_LINK:
					cmdinfo.type |= GV_INFO_COMPLEX_REFERENCE;
					if (*expr == '(') {								/* Element of reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
						GVPUTCMD(cmd, load_complex_idx);
					} else {
						GVPUTCMD(cmd, load_complex_array);
						if ( (cmdinfo.type & GV_INFO_UNINDEXED_ARRAY_REFERENCE) == 0) {
							cmdinfo.type   |= GV_INFO_UNINDEXED_ARRAY_REFERENCE;
							cmdinfo.length  = *entry->var.c_array->size;
						} else {
							cmdinfo.length  = min(cmdinfo.length, *entry->var.c_array->size);
						}
					}
					GVPUTITEM(cmd, entry->var.c_array, COMPLEX_ARRAY *);
					break;

				case GV_STRING_ARRAY:
				case GV_STRING_ARRAY_LINK:
					if (*expr != '(') {								/* Element of reference */
						gv_err_msg("String arrays must be indexed to be evaluated");
						return(1);
					}
					expr++;
					i = strtol(expr, &endptr, 10);
					if (*endptr != ')') {
						gv_err_msg("String array references limited to one constant argument");
						return(1);
					}
					expr = endptr+1;
					aptr = entry->var.s_array->sval[i];			/* String to be exectued */
					
					if (aptr != NULL) {
						holdexpr = expr; holdexpstart = expstart;			/* Save old values */
						
						if ( (expr=expstart=(char *) malloc(strlen(aptr)+1)) == NULL) {
							ERRprintf("(GVParse) Unable to allocate temporary memory\n");
							return(1);
						}
						if ( (rcode = copy_string(expstart, aptr)) == 0) 
							rcode = get_number_value();
						
						free(expstart);											/* Free memory		  */
						expr = holdexpr; expstart = holdexpstart;			/* Restore last one */
					}
					break;
					
				case GV_STRING:										/* May have single char */
				case GV_STRING_LINK:
					if (bStripColon) {								/* Didn't really work! */
						expr = expr_original;						/* Reset for error msg */
						return(-1);
					}
					
					if (*expr == '(') {								/* Single character reference */
						expr++; paren_count++; 
						if ( (rcode=get_number_value()) != 0) return(rcode);
						if (*(expr-1) != ')') {
							gv_err_msg("Array references limited to one argument");
							return(1);
						}
						GVPUTCMD(cmd, load_string_idx);
						GVPUTITEM(cmd, entry->var.stradr, CHAR *);
						break;
					}
					aptr = entry->var.stradr;
					
					if (aptr != NULL) {
						holdexpr = expr; holdexpstart = expstart;			/* Save old values */
						
						if ( (expr=expstart=(char *) malloc(strlen(aptr)+1)) == NULL) {
							ERRprintf("(GVParse) Unable to allocate temporary memory\n");
							return(1);
						}
						if ( (rcode = copy_string(expstart, aptr)) == 0) 
							rcode = get_number_value();
						
						free(expstart);											/* Free memory		  */
						expr = holdexpr; expstart = holdexpstart;			/* Restore last one */
					}
					break;
					
				case GV_SURFACE:
					if (*expr != '(') {								/* Element of reference */
						gv_err_msg("String arrays must be indexed to be evaluated");
						return(1);
					}
					expr++; paren_count++;
					if ( (rcode=get_number_value()) != 0) return(rcode);		/* Get the row # */
					if (*(expr-1) != ',') {
						gv_err_msg("Surface references must be [xrow,ycol]");
						return 1;
					}
					if ( (rcode=get_number_value()) != 0) return(rcode);		/* Get the column # */
					if (*(expr-1) != ')') {
						gv_err_msg("Surface references must be [xrow,ycol]");
						return 1;
					}
					GVPUTCMD(cmd, load_surface_pt);
					GVPUTITEM(cmd, entry->var.surface, SURFACE *);
					break;

				case GV_FUNCTION:
				case GV_FUNCTION_LINK:
				case GV_FUNCTION_A_LINK:
				case GV_STR_FUNCTION_LINK:
					goto HandleFunctionRef;

				case GV_2DCURVE:										/* Not handled */
				case GV_3DCURVE:
				default:													/* Default unknown action */
					expr = expr_original;							/* Reset for error msg */
					rcode = -1;
			}
			break;

		default:
			ERRprintf("ERROR: Mode specified (%d) in get_var() was invalid - programming error\n", mode);
			rcode = 1;
			break;
	}
	
	return(rcode);

/* ---------------------------------------------------------------------------
-- Handling of function references - much more complex so all here
--------------------------------------------------------------------------- */
HandleFunctionRef:
	if (entry->type == GV_FUNCTION) {
		nargs = entry->var.function->nargs;		/* Number of arguments	*/
		argtypes = entry->var.function->argtypes;
	} else if (entry->type == GV_FUNCTION_LINK) {
		nargs = entry->var.ext_fnc->nargs;		/* Number of arguments	*/
		argtypes = entry->var.ext_fnc->argtypes;
	} else if (entry->type == GV_FUNCTION_A_LINK) {
		nargs = entry->var.ext_fnca->nargs;		/* Number of arguments	*/
		argtypes = entry->var.ext_fnca->argtypes;
	} else if (entry->type == GV_STR_FUNCTION_LINK) {
		nargs = entry->var.str_ext_fnc->nargs;	/* Number of arguments	*/
		argtypes = entry->var.str_ext_fnc->argtypes;
	} else {
		nargs = entry->extra;						/* Store # with load		*/
		argtypes = NULL;								/* All must be float		*/
	}

/* For internally define function, copy over the definition so can be
 * modified for array arguments */
	if (entry->type == GV_FUNCTION) strcpy(tmpdef, entry->var.function->def);
	
/* Values for floating point stack */
	MyArgBase   = ArgNext;							/* I start at next free	*/
	ArgBaseHold = ArgBase;							/* Save to restore		*/
	ArgTopHold  = ArgTop;							/* For safety, both		*/

/* And same for string stack */
	SMyArgBase   = SArgNext;						/* I start at next free	*/
	SArgBaseHold = SArgBase;						/* Save to restore		*/
	SArgTopHold  = SArgTop;							/* For safety, both		*/

/* ... Parse the arguments f(x,y,z) - load x,y,z on argument stack ... */
	rcode = 0;											/* Presently okay			*/
	iargs = sargs = aargs = 0;
	if (*expr != '(') {								/* Permit no args to be	*/
		if (nargs != 0) rcode = -1;				/* name, no parenthesis	*/

	} else { 
		expr++;											/* Skip over the ( sign	*/
		paren_count++;									/* And track parens		*/

		for (i=0; i<nargs; i++) {					/* Get # of arguments	*/
			if (*(expr-1) == ')') {					/* Watch for end			*/
				gv_err_msg("Too few arguments for function call"); 
				rcode = 1; break;
			}
			if (argtypes == NULL || argtypes[i] == ARG_REAL) {		/* Floating argument		*/
				if ( (rcode=get_number_value()) != 0) break;
				GVPUTCMD(cmd, save_stack_val);	/* Push argument now		*/
				iargs++;
				ArgNext++; ArgTop++;					/* One more there now	*/

			} else if (argtypes[i] == ARG_STRING) {
				if ( (rcode = get_string_value()) != 0) break;
				GVPUTCMD(cmd, save_string_stack_val);
				sargs++;
				SArgNext++; SArgTop++;				/* One more there now	*/

			} else if (argtypes[i] == ARG_ARRAY) {			/* Text replace of %a02 with actual array name */
				char arg[5], *nameptr;
				int isize,ilen;

				if (entry->type != GV_FUNCTION) {
					ERRprintf("ERROR: Only internally defined functions may access arrays currently.\n");
					rcode = 1; break;
				} else if (get_name_value(&nameptr, &isize) != 0) {
					ERRprintf("ERROR: Something bad with the name expected for an array here\n");
					rcode = 1; break;
				}
				sprintf(arg, "%ca%2.2d", STACK_REF_CHAR, aargs);	/* &a02 format variable */
				ilen  = (int) (strlen(tmpdef)+1);						/* Number of characters in def'n */
				while ( (aptr = strstr(tmpdef, arg)) != NULL) {
					memmove(aptr+isize, aptr+4, ilen-(aptr-tmpdef));
					memcpy(aptr, nameptr, isize);
					ilen += isize-4;
				}
				aargs++;

			} else {
				ERRprintf("ERROR: Unexpected argument type in code.  Tell programmers.\n");
				rcode = 1; break;
			}
		}

		if (rcode == 0) {								/* Only check if succeed */
			if (nargs == 0 && *expr == ')') {	/* Allow f() w/ no args	*/
				paren_count--; expr++;
			} else if (*(expr-1) != ')') {		/* Better be end of args */
				gv_err_msg("Too many arguments for function call");
				rcode = 1;
			}
		}
	}

	if (rcode == 0) {									/* Abort on errors now */
		ArgBase      = MyArgBase;					/* Base reference			*/
		ArgTop       = ArgNext;						/* And top value			*/
		SArgBase     = SMyArgBase;					/* Base reference			*/
		SArgTop      = SArgNext;					/* And top value			*/
		if (entry->type == GV_FUNCTION) {
			holdexpr     = expr;						/* Save current ptrs		*/
			holdexpstart = expstart;
			expr = expstart = tmpdef;
			if (mode == GVP_STRING) {
				if (entry->var.function->fnctype != FNC_STRING) {
					gv_err_msg("Expecting string function but found numeric function");
					rcode = 1;
				} else {
					rcode = get_string_value();	/* And evaluate it		*/
				}
			} else {
				if (entry->var.function->fnctype != FNC_REAL) {
					gv_err_msg("Expecting numeric function but found string function");
					rcode = 1;
				} else {
					rcode = get_number_value();	/* And evaluate it		*/
				}
			}
			expr         = holdexpr;				/* Restore last one		*/
			expstart     = holdexpstart;
		} else if (entry->type == GV_FUNCTION_LINK) {
			GVPUTCMD(cmd, xeq_function);
			GVPUTITEM(cmd, entry->var.ext_fnc->fnc, EXT_FNC_LINK *);
			*(cmd++) = (unsigned char) MyArgBase;
		} else if (entry->type == GV_FUNCTION_A_LINK) {
			GVPUTCMD(cmd, xeq_function_a);
			GVPUTITEM(cmd, entry->var.ext_fnca->fnc, EXT_FNCA_LINK *);
			*(cmd++) = (unsigned char) sargs;
			*(cmd++) = (unsigned char) MyArgBase;
			*(cmd++) = (unsigned char) SMyArgBase;
		} else if (entry->type == GV_STR_FUNCTION_LINK) {
			GVPUTCMD(cmd, xeq_str_function);
			GVPUTITEM(cmd, entry->var.str_ext_fnc->fnc, EXT_STR_FNC_LINK *);
			*(cmd++) = (unsigned char) sargs;
			*(cmd++) = (unsigned char) MyArgBase;
			*(cmd++) = (unsigned char) SMyArgBase;
		}
		ArgBase      = ArgBaseHold;				/* Unwind arg stack		*/
		ArgTop       = ArgTopHold;
		SArgBase     = SArgBaseHold;				/* Unwind arg stack		*/
		SArgTop      = SArgTopHold;
		if (rcode == 0) {								/* If okay, pop stack	*/
			if (iargs != 0) {
				GVPUTCMD(cmd, pop_stack);
				*(cmd++) = (unsigned char) iargs;
			}
			if (sargs != 0) {
				GVPUTCMD(cmd, pop_string_stack);
				*(cmd++) = (unsigned char) sargs;
			}
		}
	}
	ArgNext =  MyArgBase;							/* Dump my arguments	*/
	SArgNext = SMyArgBase;							/* Dump my arguments	*/
	return(rcode);
}


/* ===========================================================================
-- Routine to guess whether the given expression is a string or a numeric
-- type expression.  Basically so can know how to interpret later.
--
-- Usage: GVPARSEMODE GVGuessExprType(char *expr)
--
-- Inputs: expr - the mathematical expression (unprocessed)
--
-- Output: none
--
-- Returns:  GVP_STRING  => is string
--           GVP_NUMERIC => is numeric
--           GVP_FILEPTR => if FILE * pointer
--           GVP_UNKNOWN => probably invalid, but suggest trying as numeric
=========================================================================== */
GVP_PARSEMODE GVGuessExprType(char *expr) {

	GV_ENTRY *var_entry;
	FNC_DEF *fnc_entry;
	char szTmp[256];
	int len, magic;
	char *aptr;
	BOOL MightBeFnc, MightBeVar;

/* Skip over any opening white space or parenthesis to first real object */
/* If a # sign, this is cast prototype to string expression */
	while (isspace(*expr)) expr++;
	if (*expr == '#') return(GVP_STRING);
	while (strchr("({[", *expr) != NULL) expr++;

/* If it starts with a string, then we are a string expression */
	if (*expr == '"') return(GVP_STRING);

/* If a stack reference, maybe string or number */
	if (*expr == STACK_REF_CHAR && isdigit(expr[1])) return GVP_NUMERIC;
	if (*expr == STACK_REF_CHAR && expr[1] == 's' && isdigit(expr[2])) return GVP_STRING;

/* If it starts with an operator or a digit, then a number */
	if (strchr("+-!.&0123456789", *expr) != NULL) return(GVP_NUMERIC);

/* If it is of form 'd', then a number */
	if (*expr == '\'') return(GVP_NUMERIC);
	
/* Return if definitely no chance */
	if (! (isalpha(*expr) || strchr("$@_", *expr) != NULL)) return(GVP_UNKNOWN);
		
/* Is either an internal function, or an intrinsic function -- check which */
/* Copy over the name and create the magic number for other functions */
	aptr = expr;
	while (isalnum(*expr) || *expr=='_' || *expr=='$' || *expr==':' ) expr++;
	len = min((int) (expr-aptr), sizeof(szTmp)-1);
	strncpy(szTmp, aptr, len); szTmp[len] = '\0';
	while (isspace(*expr)) expr++;
	magic = gv_make_magic(szTmp);

/* Depending on following character, either a variable or a function may be allowed */
/* Added ,) on list for "MightBeVar" so can tolerate mid-level expressions */
	MightBeFnc = strchr("({[", *expr) != NULL;
	MightBeVar = (*expr == '\0' || strchr("?+-*/&|^<>=!.,)", *expr) != NULL);
	if (! (MightBeFnc || MightBeVar)) return(GVP_UNKNOWN);

/* Search list for match, using magic key first followed by exact match */
	for (var_entry=GVFirstEntry; var_entry!=NULL; var_entry=var_entry->next) {
		if (var_entry->magic == magic && strnicmp(szTmp,var_entry->varname,len) == 0)
			break;
	}
	if (var_entry != NULL) {
		switch (var_entry->type) {

			case GV_STRING_ARRAY:				/* Both var and var[i] are strings */
			case GV_STRING_ARRAY_LINK:
				return(GVP_STRING);

			case GV_2DCURVE:						/* Returns the variable IDS */
			case GV_3DCURVE:
			case GV_SURFACE:
				return(MightBeVar ? GVP_STRING : GVP_UNKNOWN);

			case GV_STRING:						/* If var, is string.  If function, element of string */
			case GV_STRING_LINK:
				return(MightBeVar ? GVP_STRING : GVP_NUMERIC);

			case GV_FUNCTION:						/* Must be function!  State depends on type in entry */
				if (! MightBeFnc) return(GVP_UNKNOWN);
				return ( (var_entry->var.function->fnctype == FNC_STRING) ? GVP_STRING : GVP_NUMERIC);

			case GV_STR_FUNCTION_LINK:
				if (! MightBeFnc) return(GVP_UNKNOWN);
				return GVP_STRING;
				
			case GV_REAL:
			case GV_REAL_LINK:
			case GV_DOUBLE:
			case GV_DOUBLE_LINK:
			case GV_INT:
			case GV_INT_LINK:
			case GV_COMPLEX:
			case GV_COMPLEX_LINK:
				return(MightBeVar ? GVP_NUMERIC : GVP_UNKNOWN);

			case GV_ARRAY:
			case GV_ARRAY_LINK:
			case GV_DOUBLE_ARRAY:
			case GV_DOUBLE_ARRAY_LINK:
			case GV_COMPLEX_ARRAY:
			case GV_COMPLEX_ARRAY_LINK:
			case GV_INT_ARRAY:
			case GV_INT_ARRAY_LINK:
				return(GVP_NUMERIC);

			case GV_FILEPTR:
				return(GVP_FILEPTR);
		}
	}

/* Okay, not there.  Time to search the intrinsic function list */
	if ( (fnc_entry = find_fnc(szTmp, magic, len)) == NULL) return(GVP_UNKNOWN);

	if ((fnc_entry->fnctype & RETURN_STRING)  != 0) return(GVP_STRING);
	if ((fnc_entry->fnctype & RETURN_FILEPTR) != 0) return(GVP_FILEPTR);
	return(GVP_NUMERIC);

}

/* ---------------------------------------------------------------------------
-- Routine to print out an error message and indicate the position where the
-- error occurred.  In this version, uses the default expression start and
-- current point.  See gv_err_msg2 for lower level form
--
-- Usage:  void gv_err_msg(char *msg);
--
-- Inputs: msg - message to indicate the problem encountered
--
-- Output: Outputs error message to the users.
--------------------------------------------------------------------------- */
PRIVATE void gv_err_msg(char *msg) {

	gv_err_msg2(msg, expstart, expr);
	return;
}

/* ---------------------------------------------------------------------------
-- Routine to print out an error message and indicate the position where the
-- error occurred.  In this version, uses the default expression start and
-- current point.  See gv_err_msg2 for lower level form
--
-- Usage:  void gv_err_msg2(char *msg, char *start, char *now);
--
-- Inputs: msg   - message to indicate the problem encountered
--         start - pointer to start of expression being evaluated
--         now   - pointer to current point in the expression
--
-- Output: Outputs error message to the users.
--------------------------------------------------------------------------- */
PRIVATE void gv_err_msg2(char *msg, char *start, char *now) {

	char achr;

	/* Don't print anything if turned off by other programs */
	if (GVMathMode & MATH_NO_PARSE_MSG) return;
	
	/* Otherwise, do depending on whether we are at the end of the line */
	if ( (achr=*now) == '\0')
		ERRprintf("(GVParse) %s\n  %s <--\n", msg, start);
	else {
		*now = '\0';
		ERRprintf("(GVParse) %s\n  %s <--> %c%s\n", msg, start, achr, now+1);
		*now = achr;
	}
	return;
}

/* ===========================================================================
-- Routine to determine mean/stdev of an array (for pre-calcs above)
--
-- Usage:  gv_array_stats(ARRAY *ar, int *npt, TMPREAL *mean, TMPREAL *stdev);
--
-- Inputs: ar    - pointer to an array structure
--         npt, mean, stdev - pointers to variables or NULL if not needed
--
-- Output: npt, mean, stdev if not NULL
=========================================================================== */
PRIVATE void gv_array_stats(ARRAY *ar, int *icnt, TMPREAL *mean, TMPREAL *stdev) {
	int i,npt;
	double sx, sxx;

	npt = *(ar->size);									/* Number of points */
	if (icnt   != NULL) *icnt = npt;
	if (mean == NULL && stdev == NULL) icnt = 0;	/* Don't waste time if info never needed */

	if (npt <= 0) {										/* No data, just set everything to zero */
		if (mean  != NULL) *mean  = 0;
		if (stdev != NULL) *stdev = 0;
	} else if (npt == 1) {								/* One point - trivial but no stdev */
		if (mean  != NULL) *mean  = ar->x[0];
		if (stdev != NULL) *stdev = 0;
	} else {													/* Real case */
		for (sx=sxx=0,i=0; i<npt; i++) { sx += ar->x[i]; sxx += ar->x[i]*ar->x[i]; }
		if (mean  != NULL) *mean  = (TMPREAL) (sx/npt);
		if (stdev != NULL) *stdev = SQRT( (sxx-sx*sx/npt)/(npt-1) );
	}
	return;
}


/* ===========================================================================
-- Function to do statistical tests on two arrays
--
-- Inputs: ARRAY *ar[2] - array of pointers to the arrays of interest
--         type - one of 'U', 'T' or 'F' corresponding to
--                  T --> Student t-test on data
--                  U --> Student t-test assuming non-equal variance
--                  F --> two-tailed F test on variances
--
-- Returns: Appropriate values
=========================================================================== */
PRIVATE TMPREAL gv_eval_statistics(ARRAY *ar[], char type) {

	TMPREAL ave[2], var[2], tmp, t,v,v1,v2,F;
	int i,j,len[2];
	REAL *x,*y;

		/* Single array efforts */
	if (type == 'M') {
		len[0] = *ar[0]->size;
		x = ar[0]->x;

		if (len[0] < 2) {
			ERRprintf("ERROR: Arrays in statistics tests must have at least 2 elements\n");
			return(0.0);				/* Must have 2 elements per array */
		}

		for (tmp=0,j=0; j<len[0]; j++) tmp  += x[j];
		ave[0] = tmp/len[0];

		for (tmp=0,j=0; j<len[0]; j++) tmp += pow(x[j]-ave[0],2);
		var[0] = tmp/(len[0]-1);

		/* Repeated measurement comparison */
	} else if (type == 'D') {
		len[0] = *ar[0]->size;
		len[1] = *ar[1]->size;
		x = ar[0]->x;
		y = ar[1]->x;
		if (len[0] < 2) {
			ERRprintf("ERROR: Arrays in statistics tests must have at least 2 elements\n");
			return(0.0);				/* Must have 2 elements per array */
		} else if (len[0] != len[1]) {
			ERRprintf("ERROR: Dependent Student t-test only possible with same sized arrays\n");
			return(0.0);
		}

		for (tmp=0,j=0; j<len[0]; j++) tmp  += (x[j]-y[j]);
		ave[0] = tmp/len[0];

		for (tmp=0,j=0; j<len[0]; j++) tmp += pow((x[j]-y[j])-ave[0],2);
		var[0] = tmp/(len[0]-1);

		/* All other 2-array efforts */
	} else {
		for (i=0; i<2; i++) {
			len[i] = *ar[i]->size;
			x = ar[i]->x;

			if (len[i] < 2) {
				ERRprintf("ERROR: Arrays in statistics tests must have at least 2 elements\n");
				return(0.0);				/* Must have 2 elements per array */
			}

			for (tmp=0,j=0; j<len[i]; j++) tmp  += x[j];
			ave[i] = tmp/len[i];

			for (tmp=0,j=0; j<len[i]; j++) tmp += pow(x[j]-ave[i],2);
			var[i] = tmp/(len[i]-1);
		}
	}

/* Now start calculating */
	if (type == 'T') {								/* T-test */
		double sp;
		v =  len[0]+len[1]-2;
		sp = sqrt( ((len[0]-1)*var[0]+(len[1]-1)*var[1]) / v);
		t = (ave[0]-ave[1]) / (sp * sqrt(1.0/len[0] + 1.0/len[1]));
		tmp = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);

	} else if (type == 'D') {
		t = ave[0]/sqrt(var[0]/len[0]);
		v = len[0]-1;
		tmp = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);

	} else if (type == 'U') {						/* uneven variance T-test */
		t = (ave[0]-ave[1]) / SQRT(var[0]/len[0]+var[1]/len[1]);
		v =  pow(var[0]/len[0]+var[1]/len[1],2) /
			  (pow(var[0]/len[0],2)/(len[0]-1) + pow(var[1]/len[1],2)/(len[1]-1));
		tmp = 1.0 - BETAI_Ix(v/(v+t*t), v/2, 0.5);

	} else /* if (type == 'F') */ {			/* F-test */
		F = var[0] / var[1];						/* F value */
		v1 = len[0]-1;								/* Degrees of freedom */
		v2 = len[1]-1;
		tmp = BETAI_Ix(v2/(v2+v1*F), v2/2, v1/2);
	}
	return(tmp);
}
