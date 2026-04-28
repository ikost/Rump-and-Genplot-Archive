/* SYSTEM.F77 */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <float.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef NT
	#undef _POSIX_
	#include <stdlib.h>
	#define _POSIX_
	#include <windows.h>
#else
	#if (defined LINUX && ! defined __USE_BSD)	/* Need prototype for set/unsetenv()	*/
		#define	__USE_BSD								/* Must be placed just before stdlib.h */
	#endif													/* And because of ENDIAN, must be last	*/
	#include <stdlib.h>
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#define LEXP_EXTENSIONS
#include "lexp.h"
#include "defaults.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
BOOL	LexSystem(int key, const char *token);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE BOOL do_alloc(void);
PRIVATE BOOL do_basemacro(void);
PRIVATE BOOL do_chdir(void);
PRIVATE BOOL do_dchdir(void);
PRIVATE BOOL do_pushd(void);
PRIVATE BOOL do_popd(void);
PRIVATE BOOL do_cls(void);
PRIVATE BOOL do_cmdlin(void);
PRIVATE BOOL do_comment(void);
PRIVATE BOOL do_line_comment(void);
PRIVATE BOOL do_block_comment(void);
PRIVATE BOOL do_dealloc(void);
PRIVATE BOOL do_yesno(void);
PRIVATE BOOL do_why(void);
PRIVATE BOOL do_declare(void);
PRIVATE BOOL do_define(void);
PRIVATE BOOL do_lldir(void);
PRIVATE BOOL do_lfdir(void);
PRIVATE BOOL do_dir(void);
PRIVATE BOOL do_echo(void);
PRIVATE BOOL do_localecho(void);
PRIVATE BOOL do_emacs(void);
PRIVATE BOOL do_mathmode(void);
PRIVATE BOOL do_eval(void);
PRIVATE BOOL do_eval_quiet(void);
PRIVATE BOOL do_eval_all(int mode);
PRIVATE BOOL do_for(void);
PRIVATE BOOL do_fprintf(void);
PRIVATE BOOL do_goto(void);
PRIVATE BOOL do_history(void);
PRIVATE BOOL do_if(void);
PRIVATE BOOL do_elseif(void);
PRIVATE BOOL do_else(void);
PRIVATE BOOL do_while(void);
PRIVATE BOOL do_loop(void);
PRIVATE BOOL do_foreach(void);
PRIVATE BOOL do_forlists(void);
PRIVATE BOOL do_break(void);
PRIVATE BOOL do_continue(void);
PRIVATE BOOL do_let(void);
PRIVATE BOOL do_listvar(void);
PRIVATE BOOL do_logit(void);
PRIVATE BOOL do_macro(void);
PRIVATE BOOL do_mem(void);
PRIVATE BOOL do_more(void);
PRIVATE BOOL do_nop(void);
PRIVATE BOOL do_pause(void);
PRIVATE BOOL do_printf(void);
PRIVATE BOOL do_errprintf(void);
PRIVATE BOOL do_messagebox(void);
PRIVATE BOOL do_sprintf(void);
PRIVATE BOOL do_pwd(void);
PRIVATE BOOL do_query(void);
PRIVATE BOOL do_repeat(void);
PRIVATE BOOL do_savemacro(void);
PRIVATE BOOL do_savevar(void);
PRIVATE BOOL do_script(void);
PRIVATE BOOL do_setvar(void);
PRIVATE BOOL do_solve(void);
PRIVATE BOOL do_sleep(void);
PRIVATE BOOL do_system(void);
PRIVATE BOOL do_timer(void);
PRIVATE BOOL do_wait(void);
PRIVATE BOOL do_xeq(void);
PRIVATE BOOL do_xxeq(void);				/* XEQ with possible check for = assign */
PRIVATE BOOL do_setenv(void);
PRIVATE BOOL do_printenv(void);
PRIVATE BOOL do_insertmode(void);
PRIVATE BOOL do_replacemode(void);
PRIVATE BOOL do_setlocal(void);
PRIVATE BOOL do_setglobal(void);
PRIVATE BOOL do_endlocal(void);
PRIVATE BOOL do_setpriority(void);
#ifdef NT
PRIVATE BOOL do_setregistry(void);
#endif

PRIVATE BOOL ImplicitSetvar(char *var);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Collection of all the Operating System commands
--
--     Usage: LOG = SYSTEM(KEY, TOKEN)
--
--     Inputs: KEY    Chooses the operation:  0 = Process command
--                                           U$INIT = Initialize
--                                           U$RST  = Reset
--                                           U$HELP = List Commands
--                                           U$PARM = Display Parameters
--                                           U$QUIT = Turn Off
--                    See definitions in UKEYS.INS
--             TOKEN - Command to be processed for KEY = 0 (CHARACTER)
--
--     Output: SYSTEM -  (KEY .eq. 0) .and. (Command Processed)
--                       True if this process could recognize the command
============================================================================ */
#define VarDefChanged "WARNING: Previous definition of this variable has been deleted\n"

static BOOL	MacroOn=FALSE;
static CHAR	MacroName[PATH_MAX]="\0";
static CHAR	ScriptFile[PATH_MAX]="\0";

typedef struct _CMTYPE {
	CHAR *command;
	INT  minlen;
	BOOL (*fnc)(void);
} CMTYPE;

static const CMTYPE cmlist[] =
	{	{"directory",	 3, do_dir},		{"ls",			 2, do_dir},
		{"sl",			-2, do_dir},		{"lf",			-2, do_lfdir},
		{"ll",			-2, do_lldir},
		{"cd",			 2, do_chdir},		{"dcd",			 3, do_dchdir},
		{"chdir",		-5, do_chdir},
		{"pushdir",		 5, do_pushd},		{"popdir",		 4, do_popd},
		{"where",		 3, do_pwd},		{"pwd",			-3, do_pwd},
		{"type",			 2, do_more},		{"cat",			-3, do_more},
		{"more",       -4, do_more},
		{"cls",			 3, do_cls},
		{"!",           1, do_system},
		{"dos",			-3, do_system},	{"ok;",			-2, do_system},
		{"csh",			-3, do_system},	{"shell",		-5, do_system},
		{"memory",		 3, do_mem},
		{"emacs",		 2, do_emacs}, 	{"eps",			-3, do_emacs},
		{"editor",		 6, do_emacs},
/* ... Dumb system operations */
		{"setenv",		-6, do_setenv},	{"putenv",		-6, do_setenv},
		{"unsetenv",	-8, do_setenv},
		{"printenv",	-8, do_printenv},
		{"listenv",		-7, do_printenv},
		{"showenv",		-7, do_printenv},
		{"getenv",		-6, do_printenv},
/* ... Macro processing */
		{"x",				-1, do_xxeq},
		{"xeq",			 2, do_xeq},		{"call",			-4, do_xeq},
		{"execute",		-3, do_xeq},
		{"echo",			 4, do_echo},		{"quiet",		-5, do_echo},
		{"@echo",		 5, do_localecho},
		{"printf",		 6, do_printf},	{"errprintf",   5, do_errprintf},
		{"sprintf",		 7, do_sprintf},	{"MessageBox",	 6, do_messagebox},
		{"macro",		 3, do_macro},
		{"save_macro",	 6, do_savemacro},{"savemacro",	-5, do_savemacro},
		{"macropath",	 6, do_basemacro},{"macro_path", -7, do_basemacro},
		{"base_macro",	-6, do_basemacro},{"basemacro",	-5, do_basemacro},
/* ... System interface, repeat processing */
		{"script",		 6, do_script},	{"logfile",		-4, do_script},
		{"logit",		 4, do_logit},		{"fprintf",		 7, do_fprintf},
		{"repeat",		 6, do_loop},		{"for",			 3, do_foreach},
		{"oldrepeat",	-9, do_repeat},	{"oldfor",		-6, do_for},
		{"if",			 2, do_if},			{"elseif",		 6, do_elseif},
		{"else",			 4, do_else},		{"goto",			 4, do_goto},
		{"while",		 5, do_while},		{"loop",			 4, do_loop},
		{"foreach",		 4, do_foreach},
		{"forlists",	 4, do_forlists},
		{"break",		 5, do_break},		{"continue",	 8, do_continue},
		{"pause",		 5, do_pause},		{"wait",			 4, do_wait},
		{"sleep",		 5, do_sleep},		{"timer",		 5, do_timer},
		{"cmdlin",		 6, do_cmdlin},	{"history",		 4, do_history},
/* ... Function evaluator functions */
		{"mathmode",	 4, do_mathmode}, {"mm",			-2, do_mathmode},
		{"evaluate",	 2, do_eval},		{"let",			 3, do_let},
		{"qevaluate",	-3, do_eval_quiet},
		{"allocate",	 5, do_alloc},		{"setvars",		 4, do_setvar},
		{"solve",		 5, do_solve},
		{"define",		 3, do_define},	{"declare", 	 3, do_declare},
		{"query",		 5, do_query},
		{"listvars",	 5, do_listvar},	{"lv",			-2, do_listvar},
		{"savevars",	 5, do_savevar},	{"writevars",	-6, do_savevar},
		{"deallocate",	 5, do_dealloc},	{"free",       -4, do_dealloc},
		{"unsetvar",   -6, do_dealloc},
		{"setlocal",	 8, do_setlocal},	{"endlocal",	 8, do_endlocal},
		{"unsetlocal", -6, do_endlocal},
		{"globalize",	 6, do_setglobal},
		{"setpriority", 5, do_setpriority},
		{"set_priority", -6, do_setpriority},
#ifdef NT
		{"set_registry",5, do_setregistry},
#endif
/* ... Non-function functions */
		{"yes",			 3, do_yesno},		{"no",			 2, do_yesno},
		{"why?",			-3, do_why}, 
		{"/*",			 2, do_comment},	{"/**",			 3, do_block_comment},
		{"//",			-2, do_line_comment},
		{"nop",			 3, do_nop},		{"c",				-1, do_nop},

		{"replace_mode", -9, do_replacemode},
		{"insert_mode",  -8, do_insertmode},

		{NULL,         0,	 NULL}
	};

/* ---------------------------------------------------------------------- */
BOOL LexSystem(INT key, const char *token) {

	CMTYPE *cmitem;
	char tok2[4];

	switch (key) {

		case 0:													/* Execute the command	*/
			if (token == NULL || *token == '\0') {		/* Null commands	*/
				return(TRUE);

			} else if (token[1] == ':' && token[2] == '\0') {
				if (! SysSetDisk(*token)) {
					ERRprintf("ERROR: %s seems not to be a valid drive\n", token);
					LexFlush();
				}

/* ! history now handled as atomic in low_io.c */
			} else if (token[0] == '!' && token[1] != '\0') {
				ERRprintf("ERROR: %s was not found on history stack\n", token);
				LexFlush();

			} else if ( (cmitem=LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL) {
				if (! (*cmitem->fnc)()) LexFlush();

			} else {											/* Handle the implicit x = expr */
																/* See also do_xxeq() explicit */
				LexConvertMode = NO_AMP_EXPAND | NO_TRANSLATE;
				if (LexChkToken(tok2,sizeof(tok2)) && strcmp(tok2,"=") == 0) {
					LexGetToken(tok2, sizeof(tok2));		/* Dump the = sign */
					if (! ImplicitSetvar((char *) token)) LexFlush();
				} else if (strpbrk(token, "+-*/^({[!:?&|<>=") != NULL ||
							  GVGetAdrInfo((char *) token, NULL, NULL, NULL)) {
					LexBackup();
/*					LexInsText(token); */
					if (! do_eval_all(2)) LexFlush();
				} else {
					return(FALSE);
				}
			}
			return(TRUE);

		case U_INIT:										/* Initialize	*/
		case U_RESET:										/* Or reset		*/
			*LexMacroSearchPath = '\0';				/* Reset the macro search */
			GVLinkString("$BaseMacro", GVF_INTERNAL | GVF_HIDDEN | GVF_NODELETE, LexMacroSearchPath, sizeof(LexMacroSearchPath));
			return(FALSE);

		case U_HELP:
			LexCmdlPrint(cmlist, sizeof(CMTYPE), "General System Commands:");
			return(FALSE);

		case U_PARM:												/* Parameter listing */
			do_pwd();
			if (SysLogFile(SYSLOG_CHECK,NULL,NULL))		/* Is it on? */
				TTYprintf(" Session script file: %s\n", ScriptFile);
			if (MacroOn)				     
				TTYprintf(" Local macro: %s\n", MacroName);
			if (*LexMacroSearchPath != '\0')
				TTYprintf(" Macro search path: %s\n", LexMacroSearchPath);
			return(FALSE);

		case U_QUIT:										/* Quit and close down */
			if (MacroOn) {
				if (LexYesNo(TRUE, "Delete macro? (YES) "))
					unlink(MacroName);
				else
					type2("Temporary macro saved as: ", MacroName);
			}
			if (SysLogFile(SYSLOG_CLOSE,NULL,NULL)) TTYputs("SCRIPT session closed\n");
			return(FALSE);

	}
	return(FALSE);									/* Unrecognized personal keys */
}


/* -------------------------------------------------------------------
-- BASE_MACRO - Sets a default second directory to search for macros.
-------------------------------------------------------------------- */
PRIVATE BOOL do_basemacro(void) {
	char pathlist[LONG_STR_SIZE];
	
	if (LexGetSearchPathP(pathlist, sizeof(pathlist), "Macro directory: "))
		strscpy(LexMacroSearchPath, pathlist, sizeof(LexMacroSearchPath));

	return(TRUE);
}

/* -------------------------------------------------------------------
-- Underlying routine to change a directory - called several times below
--
-- Usage:  BOOL my_chdir(char *dir, BOOL setdisk);
--
-- Inputs: dir - directory to change to.  May be UNC or include a drive
--         setdisk - if true and a drive letter is specified, will also
--                   switch the active drive to that letter
--
-- Output: Changes the current working directory of specified drive
--
-- Return: TRUE if successful if any sort of error.  Error printed.
-------------------------------------------------------------------- */
static BOOL my_chdir(char *dir, BOOL setdisk) {
	char *aptr=NULL;

/* Is this a simple directory */
	if (chdir(dir) == 0) {
#if (defined OS2 || defined NT)
		if (setdisk && dir[1] == ':') SysSetDisk(*dir);
#endif
		return(TRUE);
	}

/* More complicated, try evaluating as a string expression */
	if ( (aptr = GVEvalStrExpr(dir, NULL)) == NULL) {
		ERRprintf("ERROR: %s is not a valid directory nor a directory expression\n", dir);
		return FALSE;
	}

/* See if this works as a directory name */
	if (chdir(aptr) == 0) {
#if (defined OS2 || defined NT)
		if (setdisk && aptr[1] == ':') SysSetDisk(*aptr);
#endif
		free(aptr);
		return TRUE;
	}

/* Nope - abandon all hope now and just print error message */
	ERRprintf("ERROR: Expression (%s=%s) not a valid directory\n", dir, aptr);
	free(aptr);
	return FALSE;
}

static char CD_Help[] 
	= "The following commands manage the current working directory.\n"
	  "\n"
	  "   CD [-d]  <directory>   - change the active directory\n"
	  "   CHDIR [-d] <directory> - synonymous with CD\n"
	  "   DCD <directory>        - change the active directory and set the disk\n"
	  "   x:                     - change the active drive to x\n"
	  "\n"
	  "   PUSHD <directory>      - save current working directory and do a DCD\n"
	  "   POPD                   - return to previously saved directory\n"
	  "\n"
	  "   PWD                    - print the current working directory\n"
	  "   WHERE                  - synonymous with PWD\n"
	  "\n"
	  "Windows notes:\n"
	  "  (1) DCD changes the directory of the specified drive and makes that drive active.\n"
	  "      CD only changes the path of the specified drive without forcing it active.\n"
	  "  (2) To set D as the active drives, type D:\n"
	  "  (3) The -d option in CD causes it to behave like DCD\n"
	  "  (4) Both drive based and UNC paths are allowed.  However, system shell commands\n"
	  "      (such as ! and csh) may not work when connected to a UNC\n"
	  "\n"
	  "General Notes:\n"
	  "  (1) Forward and backslashes are synonymous.  Converted internally as necessary.\n"
	  "  (2) If the directory is not found as typed, it will be treated as a possible\n" 
	  "       string expression.  This creates some strange error messages.\n"
	  "  (3) PUSHD may be called multiple times.  Each POPD returns one level.\n"
	  "  (4) Concepts that don't apply in Linux are normally ignored (i.e. DCD == CD)\n"
	  "\n"
	  "Example:\n"
	  "   dcd f:/expt1/data/20060928\n"
	  "   pushd //ian-pc/s$/run7\n"
	  "   popd\n"
	  ;
/* ---------------------------------------------------------------------------
-- Routine to handle both CHDIR and DCHDIR (CD and DCD) for simplicity
-- These differ only in whether the active disk is set in NT/Windows
--------------------------------------------------------------------------- */
PRIVATE BOOL do_both_cd(BOOL setdisk) {

	char *aptr, dir[PATH_MAX];

	if (LexCheckHelp("Directory commands", CD_Help, NULL)) return(TRUE);

/* Look first for options.  Only allowed is -d, just backup if not found */
	if (LexGetOption(dir, sizeof(dir))) {
		if (stricmp(dir, "-d") == 0) {
			setdisk = TRUE;
		} else {
			LexBackup();
		}
	}

/* Look for the directory name.  If not given, try to use HOME from the env */
	if (! LexGetFile(dir, sizeof(dir))) {
		if ( (aptr = getenv("HOME")) != NULL) {
			strcpy(dir, aptr);
		} else if (! LexGetFileP(dir, sizeof(dir), "Directory (abort): ")) {
			return(FALSE);
		}
	}
	if (*dir == '\0') return(TRUE);

#if (defined OS2 || defined NT)
	while ( (aptr = strchr(dir,'/')) != NULL) *aptr = '\\';		/* Convert / to \ for MS vs UNIX	*/
	aptr = dir + strlen(dir) - 1;											/* Drop trailing \ from CD			*/
	if (aptr-dir > 2 && *aptr == '\\') *aptr = '\0';
#endif

	return my_chdir(dir, setdisk);
}

/* ----------------------------------------
--     DCD - Emulates CD command of DOS
---------------------------------------- */
PRIVATE BOOL do_dchdir(void) {
	return do_both_cd(TRUE);
}

/* ----------------------------------------
--     CD - Emulates CD command of DOS
---------------------------------------- */
PRIVATE BOOL do_chdir(void) {
	return do_both_cd(FALSE);
}

/* -------------------------------------------------------------------
-- PUSHD - Saves current directory and moves to the new one specified
-------------------------------------------------------------------- */
typedef struct _PUSHD_INFO {
	char saved_directory[PATH_MAX];
	struct _PUSHD_INFO *previous;
} PUSHD_INFO;
PUSHD_INFO *pushd_info = NULL;

PRIVATE BOOL do_pushd(void) {

	PUSHD_INFO *next;
	char current[PATH_MAX];

	if (LexCheckHelp("Directory commands", CD_Help, NULL)) return(TRUE);

	if ( getcwd(current, sizeof(current)) == NULL) {
		ERRprintf("Unable to determine current directory.  Use CD or DCD instead\n");
		return(FALSE);
	} else if (! do_dchdir()) {					/* Try now to go to a new directory */
		return(FALSE);
	}

	/* Okay, we have current directory and successfully changed directories */
	next = malloc(sizeof(*next));					/* Make a new entry in the linked list */
	next->previous = pushd_info;
	strcpy(next->saved_directory, current);
	pushd_info = next;

	return TRUE;
}

/* -------------------------------------------------------------------
-- POPD - Undoes the PUSHD command, restores previous directory
-------------------------------------------------------------------- */
PRIVATE BOOL do_popd(void) {
	PUSHD_INFO *last;
	BOOL rc;

	if (LexCheckHelp("Directory commands", CD_Help, NULL)) return(TRUE);

	if ( (last = pushd_info) == NULL) {
		ERRprintf("To where? Neverland?  Think about doing pushd first, huh?\n");
		return(FALSE);
	}
	rc = my_chdir(last->saved_directory, TRUE);	/* Try to return to the last location	*/
	pushd_info = last->previous;						/* Point back to last entry in list		*/
	free(last);												/* Delete the space used by this one	*/
	return rc;
}

/* ----------------------------------------
-- CLS - Clears the terminal screen
---------------------------------------- */
PRIVATE BOOL do_cls(void) {
	ScrClearAttrib(D_NORMAL);
	return(TRUE);
}

/* -----------------------------------------------------------------
-- CMDLIN process.  Allow putting next token back in as commands
----------------------------------------------------------------- */
PRIVATE BOOL do_cmdlin(void) {
	char token[LONG_STR_SIZE];
	if (LexGetToken(token, sizeof(token))) LexInsText(token);
	return(TRUE);
}

/* ----------------------------------------
-- Block comment lines
---------------------------------------- */
PRIVATE BOOL do_block_comment(void) {
	char token[DFLT_STR_SIZE];
	int id;											/* ID of current stream */
	
	id = LexQueryActiveInputID(NULL);		/* Which unit is being read */
	LexClrPtr();									/* Dump this line */

	while (LexQueryActiveInputID(NULL) == id) {
		if (! LexPromptLine(token, sizeof(token), ":", FALSE)) break;
		if (strstr(token, "**/") != NULL) break;
	}

	return(TRUE);
}

/* ----------------------------------------
-- Comment line
---------------------------------------- */
PRIVATE BOOL do_comment(void) {
	char token[DFLT_STR_SIZE], *aptr;

	LexGetRestNT(token, sizeof(token));
	if ( (aptr = strstr(token, "*/")) != NULL) {
		aptr += 2;
		if (*aptr != '\0') LexInsText(aptr);
	}
	return(TRUE);
}

PRIVATE BOOL do_line_comment(void) {
	char token[DFLT_STR_SIZE];
	LexGetRestNT(token, sizeof(token));
	return(TRUE);
}

/* ----------------------------------------
-- YES/NO tokens which just get eaten
---------------------------------------- */
PRIVATE BOOL do_yesno(void) {
	const char *msgs[] = {
	  "Yes",
	  "No",
	  "Well yes AND no to you!",
	  "Well I never ...!",
	  "And just exactly why",
	  "But are you sure",
	  "Okay",
	  "Well perhaps, but maybe you better check.",
	  "Well --- okay",
	  "But would Monty Python agree?",
	  "Your mother never told you that.",
	  "You didn't say please first.",
	  "We have the answer, now what's the question?",
	  "That does not compute.",
	  "Non-sequitur",
	  "It's true -- sum of the square of the sides equals the square of the hypotenuse",
	  "An African or European swallow?",
	  "A short response favored by youth.",
	  "Yo ho ho and a bottle of rum for that!",
	  "But only yesterday you were saying the opposite!"
	};
	int index;
	index = rand()%(sizeof(msgs)/sizeof(msgs[0]));
	TTYprintf("%s\n", msgs[index]);
	return(TRUE);
}

PRIVATE BOOL do_why(void) {
	const char *msgs[] = {
		"Because your mother told you so!",
		"If I told you, I'd have to kill you",
		"Because that's the way the cookie crumbles",
		"Because the sum of the square of the sides is equal to the square of the hypotenuse",
		"Don't ask, don't tell",
		"Cause Ian's the boss!",
		"Cause if you don't, you have to stay in for recess",
		"Cause you'll get no dessert if you don't",
		"Don't ask why, just do it!",
		"I don't know - it just is",
		"I don't know - and don't ask me again!",
		"I don't know - ask your mother!",
		"I don't know - ask your father!",
		"We don't know - ask your brother!",
		"We don't know - ask your sister!",
		"No one knows - ask your sys admin.",
		"Why not?"
	};
	int index;
	index = rand()%(sizeof(msgs)/sizeof(msgs[0]));
	TTYprintf("%s\n", msgs[index]);
	return(TRUE);
}

/* ----------------------------------------------
-- LS - Lists out current files in the directory
---------------------------------------------- */
PRIVATE BOOL do_lldir(void) {
	LexInsText("-l");
	return(do_dir());
}

PRIVATE BOOL do_lfdir(void) {
	LexInsText("-lf");
	return(do_dir());
}

/* -- modified LexGetOption() to tolerate only - options */
PRIVATE BOOL GetOption(char *token, size_t toklen) {

	if (! LexGetToken(token, (int) toklen)) return(FALSE);
	if (*token == '-' && token[1] != '\0') return(TRUE);
	LexBackup();
	return(FALSE);
}

static char DirHelp[]
	= "The DIR, LS, LF, and LL commands list the contents of the current directory\n"
	  "or any specified directory.  LS and DIR are identical, while LL and LF have\n"
	  "specific options by default.  By default, the case-insensitive, case-retention\n"
	  "philosophy is followed - wild card matches are case insensitive but the full\n"
	  "upper/lower case names are returned.\n"
	  "\n"
	  "Usage: DIRectory [options] [<wildcard>] [options]\n"
	  "       LS        [options] [<wildcard>] [options]\n"
	  "       LF        [options] [<wildcard>] [options]\n"
	  "       LL        [options] [<wildcard>] [options]\n"
	  "\n"
	  "The options, to match UNIX behavior, may be given before or after the wildcard\n"
     "or directory specification.  Options exist both as single letter (aka UNIX) or\n"
	  "as keywords.\n"
	  "\n"
	  " Named Options:\n"
	  "   -NAME           - sort files alphabetically (default)\n"
	  "   -DATE           - sort files by modification date/time\n"
	  "   -TIME           - sort files by modification date/time\n"
	  "   -SIZE           - sort files by size\n"
	  "   -EXTension      - sort files by extension\n"
	  "   -REVerse        - reverse the sense of any sort\n"
	  "   -NODir          - exclude directories from listing\n"
	  "   -PAUSE          - pipe output through more pausing every 25 lines\n"
	  "   -LL             - switch to long listing mode\n"
	  "   -LF             - switch to listing by filename extension mode\n"
	  "   -LS             - switch to normal listing mode\n"
"\n"
	  " Letter Options:\n"
	  "   -x              - preserve case on comparisons\n"
	  "   -l              - long format listing mode\n"
	  "   -F              - appends / to directories\n"
	  "   -d              - don't dive into directory            (unimplemented)\n"
	  "   -a              - list all files (including hidden)\n"
	  "   -A              - list all files except . and ..\n"
	  "   -u              - Use access date for time sorting\n"
	  "   -1              - single column\n"
	  "   -C              - multiple column output (default)\n"
	  "   -r              - reverse the sense of all sorts\n"
	  "   -R              - recursive listing                    (unimplemented)\n"
	  "   -ciqfghsHL      - ignored options\n"
	  "\n"
	  "Notes:\n"
	  "  (1) The -lf mode is a holdover from DOS 8.3 days.  It lists by extension.\n"
	  "  (2) There may be issues if filenames start getting rediculously long\n"
	  "\n"
	  "Examples:\n"
	  "   ls\n"
	  "   ls -A c:/\n"
	  "   ls data_dir -size\n"
	  ;

PRIVATE BOOL do_dir(void) {
	char pathname[PATH_MAX], option[PATH_MAX];
	char *aptr=option;

	if (LexCheckHelp("Dir", DirHelp, NULL)) return(TRUE);

	*aptr = '\0';

	while (GetOption(aptr, sizeof(option)-(aptr-option))) {
		aptr+=strlen(aptr);
		*(aptr++) = ' '; *aptr = '\0';
	}
	if (! LexGetFile(pathname, sizeof(pathname))) *pathname = '\0';
	while (GetOption(aptr, sizeof(option)-(aptr-option))) {
		aptr+=strlen(aptr);
		*(aptr++) = ' '; *aptr = '\0';
	}

	SysListDir(pathname,option);
	return(TRUE);
}

/* ----------------------------------------------------
-- history print of command stack
---------------------------------------------------- */
PRIVATE BOOL do_setenv(void) {

	int rcode=TRUE;
	char token[DFLT_STR_SIZE], *aptr;

	if (! LexGetTokenP(token, sizeof(token), "Environment SET command [ var=expr | var= | var ] (abort): "))
		return(TRUE);

	aptr = strchr(token, '=');
	
#if defined CONVEX_C || defined __linux__
	if (aptr == NULL) {
		unsetenv(token);
	} else {
		*aptr++ = '\0';
		rcode = (setenv(token, aptr, 1) == 0);		/* Overwrite the value */
	}
#else
	if (aptr == NULL) strcat(token, "=");			/* Erase by using var= format */
	rcode = (putenv(token) == 0);
#endif

	return(rcode);
}


/* ----------------------------------------------------
-- print one, or full, environment list
---------------------------------------------------- */
#if defined CONVEX_C || defined LINUX || defined UNIX
	extern char **environ;
#endif

PRIVATE BOOL do_printenv(void) {

	char token[SHORT_STR_SIZE], *aptr;

	if (LexGetTokenP(token, sizeof(token), "Environment variable (ALL): ")
		&& strcmp(token, "/") != 0) {
		aptr = getenv(token);
		if (aptr == NULL) aptr = getenv(strlwr(token));
		if (aptr == NULL) aptr = getenv(strupr(token));
		TTYprintf("%s=%s\n", token, (aptr == NULL) ? "<does not exist>" : aptr);
	} else {
		char **evptr;
		for (evptr=environ; *evptr!=NULL; evptr++) TTYprintf("%s\n", *evptr);
	}
	return(TRUE);
}

/* ----------------------------------------------------
-- history print of command stack
---------------------------------------------------- */
PRIVATE BOOL do_history(void) {
	TTYShowHistory(NULL);
	return(TRUE);
}

/* ----------------------------------------------------
-- printf() command inside genplot
---------------------------------------------------- */
static char PrintfHelp[]
	= "PRINTF, ERRPRINTF and SPRINTF implement the equivalent C function formatting\n"
	  "output to the screen or to a string variable.  There are equivalent command\n"
	  "evaluator functions fprintf() and sprintf() for writing to arbitrary files and\n"
	  "with more control over line terminators (no default cr/lf on printf).  The\n"
	  "command PRINTF outputs a text line to the screen, ERRPRINTF outputs the text\n"
	  "as if there were an error (beep and alternate color), while SPRINTF write the\n"
	  "text to a string.  The MESSAGEBOX command is an alternative for Windows users.\n"
	  "\n"
	  "Usage: printf \"format\" arg1 arg2 arg3\n"
	  "       errprintf \"format\" arg1 arg2 arg3\n"
	  "       sprintf <svar> \"format\" arg1 arg2 arg3\n"
	  "\n"
     "The format string specifies how the arguments are to be interpreted.  Each\n"
	  "%x element in the format causes one of the arguments to be interpreted as\n"
	  "either a number or string and formatted accordingly.  The format and list\n"
	  "of elements is essentially equivalent to the C definitions:\n"
	  "   %[flags][width].[precision]type\n"
	  "\n"
	  " Types:\n"
	  "   c - character            s - string\n"
	  "   d - signed int           i - signed int           u - unsigned int\n"
	  "   o - unsigned octal       x/X - unsigned hexadecimal\n"
     "   g - general float (e/f)  f/F - floating point     e/E - exponent form\n"
	  "   p - pointer\n"
	  "\n"
	  " Flags:\n"
	  "   -   left align result in given field width\n"
	  "   +   prefix output with a sign (for signed values)\n"
     "   0   Zero-pad string to the specified width\n"
	  "   #   For octal, prefix numbers with 0,0x or 0X\n"
	  "       For floating formats, forces decimal point\n"
	  "   <blank> Prefix output with blank for signed positive values\n"
	  "\n"
	  " Width     - optional number specifying minimum width of output characters\n"
	  " Precision - optional number of significant digits to print\n"
	  "\n"
	  "Notes:\n"
	  "  (1) To get a % character in the output, use %% in the format\n"
	  "  (2) Be careful of string substitution with %i and %f.  Recommend using\n"
	  "      %d and either %g or %.3f.\n"
	  "  (3) An implicit new-line (CR/LF) is always appended with printf\n"
	  "  (4) The string variable <svar> will be created if it doesn't exist\n"
	  "  (5) Qualifiers h,l,I64 and such may not be handled properly\n"
	  "  (6) GENPLOT attempts to properly interpret each argument\n"
	  "  (7) To embed formatting control characters into the format (tabs, etc.),\n"
	  "      the string must begin with a backhash character `\n"
	  "\n"
	  "Examples:\n"
	  "   printf \"This is a test\"\n"
	  "   printf \"Results: a=%.2f, b=%g c=%g icnt=%d\" cf$[0] cf$[1] sigma$ npt\n"
	  "   printf \"Current directory: %s\" upcase(pwd())\n"
	  "   printf \"`%.2f\t%.2f\t%.2f\t%.2f\" x[0], x[1], x[2], x[3]\n"
	  "   sprintf result_str \"Velocity was %7.3f m/s\" distance/time\n"
	  ;

PRIVATE BOOL do_printf(void) {
	char format[LONG_STR_SIZE], *string;
	
	if (LexCheckHelp("printf", PrintfHelp, NULL)) return(TRUE);

	if (! LexGetStrExprP(format, sizeof(format), "Format: (abort) ")) {
		return(FALSE);
	} else {
		string = LexEncodeString(NULL, 0, format);
		TTYputsnl(string);
		free(string);
	}
	return(TRUE);
}
		
PRIVATE BOOL do_errprintf(void) {
	char format[LONG_STR_SIZE], *string;

	if (LexCheckHelp("ERRprintf", PrintfHelp, NULL)) return(TRUE);

	if (! LexGetStrExprP(format, sizeof(format), "Format: (abort) ")) {
		return(FALSE);
	} else {
		string = LexEncodeString(NULL,0, format);
		ERRputsnl(string);
		free(string);
	}
	return(TRUE);
}

static char MessageBoxHelp[]
	= "The MessageBox command (available only under the Windows OS) generates a\n"
	  "pop-up window (a message box) containing a title, some text, an icon and\n"
	  "some number of buttons.  The message box will wait for the user to respond\n"
     "before continuing execution.\n"
	  "\n"
	  "Usage: MessageBox [-options] \"format\" arg1 arg2 arg3\n"
	  "\n"
	  "The format string specifies how the arguments are to be interpreted.  Each\n"
	  "%x element in the format causes one of the arguments to be interpreted as\n"
	  "either a number or string and formatted accordingly.  The format and list\n"
	  "of elements is essentially equivalent to the C definitions.  See printf for\n"
     "a complete description.\n"
	  "\n"
	  "Options\n"
	  "   -TITLE <text string> - specifies alternate title on message box\n"
	  "   -ICON [ERROR | WARNing | INFOrmation | QUEStion}\n"
	  "   -BUTTONS [OK | OKCANcel | YESNO | YESNOCANcel | RETRYCANcel |\n"
     "             ABORTretryignore | CANCELtrycontinue]\n"
	  "   -DEFAULT [1 | 2 | 3] - specifies which button is default\n"
	  "   -FOREGROUND          - causes window to pop to front\n"
	  "   -SYSMODAL            - only for really serious errors - system errors\n"
	  "\n"
	  "The return value from the function is stored in the variable $MB.  Since this was\n"
	  "clearly desired, the variable is not hidden.  Values below may change if Microsoft\n"
	  "changes the underlying include files\n"
	  "   OK:    1       CANCEL: 2      ABORT: 3       RETRY: 4        IGNORE: 5\n"
	  "   YES:   6       NO: 7          TRYAGAAIN: 10  CONTINUE: 11\n"
	  "\n"
	  "Notes: The [ Cancel | TryAgain | Continue ] options only works on V14 or above.\n"
	  "\n"
	  "Examples:\n"
	  "   MessageBox -ICON ERROR \"Can't find the specified file\"\n"
	  "   MessageBox -BUTTONS ABORT -title \"Major Screwup\" \"%s does not exist\" fname\n"
	  "      if ($mb == 5) goto retry\n"
	  ;

PRIVATE BOOL do_messagebox(void) {

#ifndef NT
	if (LexCheckHelp("MessageBox", MessageBoxHelp, NULL)) return(TRUE);

	ERRprintf("ERROR: The MESSAGEBOX command is only available under Windows\n");
	return FALSE;
#else
	char format[LONG_STR_SIZE], *string,
		  title[LONG_STR_SIZE] = "Genplot User Message";
	char token[SHORT_STR_SIZE];
	int i;

	static BOOL mb_linked = FALSE;						/* Result back to GENPLOT processor */
	static int mb = 0;

	UINT icon    = MB_ICONINFORMATION;					/* Default messages */
	UINT buttons = MB_OK;
	UINT dflt    = MB_DEFBUTTON1;
	UINT opts    = 0;

	if (LexCheckHelp("MessageBox", MessageBoxHelp, NULL)) return(TRUE);

	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-title", 6)) {
			LexGetStrExprP(title, sizeof(title), "Title for message box");
		} else if (LexEqual(token, "-icon", 5)) {
			if (! LexGetTokenP(token, sizeof(token), "Type of icon (ERROR): ")) {
				icon = MB_ICONERROR;
			} else if (LexEqual(token, "ERROR", 3)) {
				icon = MB_ICONERROR;
			} else if (LexEqual(token, "WARNING", 4)) {
				icon = MB_ICONWARNING;
			} else if (LexEqual(token, "INFORMATION", 4)) {
				icon = MB_ICONINFORMATION;
			} else if (LexEqual(token, "QUESTION", 4)) {
				icon = MB_ICONQUESTION;
			} else {
				ERRprintf("ERROR: %s in not a valid icon type.  Try MessageBox -? for help\n", token);
				return FALSE;
			}
		} else if (LexEqual(token, "-buttons", 4)) {
			if (! LexGetTokenP(token, sizeof(token), "Type of button (OK): ")) {
				buttons = MB_OK;
			} else if (LexEqual(token, "OK", 2)) {
				buttons = MB_OK;
			} else if (LexEqual(token, "OKCANCEL", 5)) {
				buttons = MB_OKCANCEL;
			} else if (LexEqual(token, "YESNO", 5)) {
				buttons = MB_YESNO;
			} else if (LexEqual(token, "YESNOCANCEL", 8)) {
				buttons = MB_YESNO;
			} else if (LexEqual(token, "RETRYCANCEL", 8)) {
				buttons = MB_RETRYCANCEL;
			} else if (LexEqual(token, "ABORTRETRYIGNORE", 5)) {
				buttons = MB_ABORTRETRYIGNORE;
		#if (_MSC_VER >= 1400)
			} else if (LexEqual(token, "CANCELTRYCONTINUE", 6)) {
				buttons = MB_CANCELTRYCONTINUE;
		#endif
			} else {
				ERRprintf("ERROR: %s in not a valid button type.  Try MessageBox -? for help\n", token);
				return FALSE;
			}
		} else if (LexEqual(token, "-default", 4)) {
			i = LexGetInt(1, "Which button will be default [1]: ");
			dflt = (i==1) ? MB_DEFBUTTON1 : (i==2) ? MB_DEFBUTTON2 : MB_DEFBUTTON3;
		} else if (LexEqual(token, "-foreground", 5)) {
			opts |= MB_SETFOREGROUND;
		} else if (LexEqual(token, "-systemmodal", 7)) {
			opts |= MB_SYSTEMMODAL;
		} else {
			ERRprintf("ERROR: %s is not a valid option for MessageBox.  See -? help\n", token);
			return FALSE;
		}
	}

	if (LexGetStrExprP(format, sizeof(format), "Message to display: ")) {
		string = LexEncodeString(NULL, 0, format);
		mb = MessageBox(NULL, string, title, icon | buttons | dflt | opts);
		free(string);
		if (! mb_linked) {
			GVLinkInt("$mb", GVF_INTERNAL, &mb);
			mb_linked = TRUE;
		}
	}

	return(TRUE);
#endif
}


/* ----------------------------------------------------
-- sprintf() command inside genplot
---------------------------------------------------- */
PRIVATE BOOL do_sprintf(void) {

	char varname[SHORT_STR_SIZE], format[LONG_STR_SIZE], *string=NULL;
	int  oldtype;
	BOOL rcode;

	if (LexCheckHelp("sprintf", PrintfHelp, NULL)) return(TRUE);

	if (! LexGetToken(varname, sizeof(varname))) {
		TTYprintf("  Usage: sprintf <var> \"format\" [<expr1> [<expr2> ...]] \n");
		return(FALSE);
	} else if (! LexGetStrExprP(format, sizeof(format), "Format: (abort) ")) {
		return(FALSE);
	} else {
		string = LexEncodeString(NULL, 0, format);
	}

/* No do equivalent of a "declare" function */
	if (! GVGetInfo(varname, &oldtype, NULL)) {
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else if (oldtype != GV_STRING && oldtype != GV_STRING_LINK) {
		ERRprintf(VarDefChanged);
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else {
		rcode = TRUE;
	}

	if (rcode) rcode = GVSetValue(varname, string);
	if (! rcode) ERRprintf("ERROR: Bad name or insufficient memory (%s)\n", varname);
	free(string);
	return(rcode);
}


/* ----------------------------------------------------
-- QUIET and ECHO - Set echo mode for MACRO execution
---------------------------------------------------- */
PRIVATE BOOL do_echo(void) {
	char token[LONG_STR_SIZE];
	
	if (LexGetToken(token, sizeof(token))) {
		if ((stricmp(token, "-?") == 0) || (stricmp(token, "-help") == 0)) {
			TTYprintf(
"  Usage: echo [-? | ON | OFF | DEBUG | NORMAL | <message>\n"
"\n"
"     ON and OFF set global echoing of command file lines.  DEBUG overrides\n"
"     echo and turns it on for all subsequent macro levels, independent of\n"
"     any other echo commands, until a ECHO NORMAL is executed.  Echo can\n"
"     also be used to print a single line of text.\n"
				);
		} else if (stricmp(token, "ON") == 0) {
			LexSetNoEcho(FALSE);
		} else if (stricmp(token,"OFF") == 0) {
			LexSetNoEcho(TRUE);
		} else if (LexEqual(token, "debug", 3)) {
			LexSetDebug(1);
		} else if (LexEqual(token, "nodebug", 3) || LexEqual(token, "normal", 4)) {
			LexSetDebug(0);
		} else {
			LexBackup();
			LexGetRest(token, sizeof(token));
			if (stricmp(token, "\\n") == 0) *token = '\0';
			TTYputsnl(token);
		}
	} else {
		LexSetNoEcho(! LexOnOff(TRUE, "Echo XEQ commands? [ON|off] "));
	}
	return(TRUE);
}

/* ----------------------------------------------------
-- @ECHO ON/OFF - Set echo mode for MACRO execution - non global
---------------------------------------------------- */
PRIVATE BOOL do_localecho(void) {
	char token[DFLT_STR_SIZE];
	
	if (LexGetToken(token, sizeof(token))) {
		if (stricmp(token, "ON") == 0)
			LexSetLocalNoEcho(FALSE);
		else if (stricmp(token,"OFF") == 0)
			LexSetLocalNoEcho(TRUE);
		else {
			LexBackup();
			LexGetRest(token, sizeof(token));
			if (stricmp(token, "\\n") == 0) *token = '\0';
			TTYputsnl(token);
		}
	} else {
		LexSetLocalNoEcho(! LexOnOff(TRUE, "Echo XEQ commands? [ON|off] "));
	}
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- EDIT processor.  Starts up some micro-emacs type editor with the specified
-- filename.  If no token is on the current line, starts up editor with
-- default or blank editor
--------------------------------------------------------------------------- */
/* ============================================================================
--     Usage: BOOL SysFileMore(CHAR *filename);
--
--     Inputs: filename - file name to be output
============================================================================ */
PRIVATE BOOL TrivialEmacs(char *pathname) {

	ERRprintf("ERROR: The internal EMACS editor no longer exists to view %s.\n"
				 "       Instead, set the environment variable EDITOR to your own favorite editor's\n"
				 "       name (or command path).  Under OS/2, the editor may need to be PM based.\n"
				 , pathname);
	return(FALSE);
}

PRIVATE BOOL do_emacs(void) {

	int  rcode=TRUE, background=FALSE;
	char *editor=NULL, **aptr;
	int type;
	char cmdline[PATH_MAX], pathname[PATH_MAX];

	if (! LexGetToken(pathname,sizeof(pathname))) *pathname = '\0';
	if (LexChkToken(cmdline, sizeof(cmdline)) && strcmp(cmdline, "&") == 0) {
		background = TRUE;
		LexGetToken(cmdline, sizeof(cmdline));
	}
#if (defined NT || defined OS2)
	background = TRUE;
#endif

/* Select the editor -- priority is internal name, environment, or default */
	if (GVGetInfo("$editor", &type, (void **) &aptr) && type == GV_STRING) {
		editor = *aptr;
	} else if ( (editor = getenv("EDITOR")) == NULL) {
		editor = DefaultEditor;
	}

/* Either use my own home-written EMACS editor, or use the specified editor */
	if (editor == NULL || stricmp(editor, "internal") == 0) {
		rcode = TrivialEmacs(pathname);

	} else {
		if (background) {
#ifdef OS2
			sprintf(cmdline, "start/f %s %s", editor, pathname);
#else
			if (strchr(editor, ' ') != NULL) {
				sprintf(cmdline, "\"%s\" \"%s\" &", editor, pathname);
			} else {
				sprintf(cmdline, "%s \"%s\" &", editor, pathname);
			}
#endif
		} else {
			sprintf(cmdline, "\"%s\" \"%s\"", editor, pathname);
		}
		if (SysSystem(cmdline) != 0) {
			ERRprintf("ERROR: Spawned edit session failed (%s)\n", cmdline);
			rcode = FALSE;
		}
	}
	return(rcode);
}

/* ----------------------------------------------------------------------
-- GOTO processing  GOTO label, searches for :label before continuing
-- IF '(x .lt. y)' GOTO is_small
---------------------------------------------------------------------- */
PRIVATE BOOL do_goto(void) {
	char token[SHORT_STR_SIZE];
	if (LexGetToken(token, sizeof(token))) LexGotoLabel(token);
	return(TRUE);
}

/* ------------------
-- IF processing
------------------ */
#define	MAXIMUM_IF_NEXT	64		/* Maximum levels of if/else nesting */
#define	IF_PROMPT			": "
PRIVATE	char	IfStatus[MAXIMUM_IF_NEXT];


PRIVATE BOOL do_if(void) {
	char		token[LONG_STR_SIZE];
	char		*block;
	int		ilevel,ierr;
	TMPREAL	xtmp;

	if (LexGetMath(token,sizeof(token))) {
		xtmp = GVEvalExpr(token, &ierr);
		if (ierr != 0) return(FALSE);
		ilevel = LexQueryActiveInputID(NULL);
		ilevel = min(ilevel, MAXIMUM_IF_NEXT-1);
		block = LexGetBlockStringNT(IF_PROMPT);
		if (xtmp <= 0.0f) {
			IfStatus[ilevel] = FALSE;
			free(block);
		} else {
			IfStatus[ilevel] = TRUE;
			LexExecBlock(block, 0, -1, NULL, NULL);
		}
	}
	return(TRUE);
}
PRIVATE BOOL do_elseif(void) {
	char		token[LONG_STR_SIZE];
	char		*block;
	int		ilevel,ierr;
	TMPREAL	xtmp;

	if (LexGetMath(token,sizeof(token))) {
		xtmp = GVEvalExpr(token, &ierr);
		if (ierr != 0) return(FALSE);
		ilevel = LexQueryActiveInputID(NULL);
		ilevel = min(ilevel, MAXIMUM_IF_NEXT-1);
		block = LexGetBlockStringNT(IF_PROMPT);
		if (xtmp <= 0.0f || IfStatus[ilevel]) {
			free(block);
		} else {
			IfStatus[ilevel] = TRUE;
			LexExecBlock(block, 0, -1, NULL, NULL);
		}
	}
	return(TRUE);
}
PRIVATE BOOL do_else(void) {
	char		*block;
	int		ilevel;

	ilevel = LexQueryActiveInputID(NULL);
	ilevel = min(ilevel, MAXIMUM_IF_NEXT-1);
	block = LexGetBlockStringNT(IF_PROMPT);
	if (IfStatus[ilevel]) {
		free(block);
	} else {
		LexExecBlock(block, 0, -1, NULL, NULL);
	}
	IfStatus[ilevel] = TRUE;
	return(TRUE);
}

static char WhileHelp[] 
	= "WHILE provides a simple control method to repeat a block of commands\n"
	  "as long as a specified condition is true.\n"
	  "\n"
	  "Usage: while <condition> <block statements>\n"
	  "\n"
	  "  The condition is any mathematical expression resolving to a numeric\n"
	  "  value.  Values greater than zero are true, zero or negative are FALSE.\n"
	  "  The block statements will be repeated until the condition is false or\n"
	  "  until a BREAK command is encountered.\n"
	  "\n"
	  "  %c and %i are substituted with the count and index of the loops starting\n"
	  "  from 001 and 000 respectively.\n"
	  "\n"
	  "Notes:\n"
	  "  (1) Block statements may be a single line, or may be multiple lines\n"
	  "      enclosed between { } braces.\n"
	  "  (2) The command CONTINUE will cause the current loop to be completed\n"
	  "      and processing to continue with the next index value\n"
	  "  (3) The command BREAK will exit the loop immediately\n"
	  "\n"
	  "Examples:\n"
	  "   let j = 7 while (j>7) eval j let j = j-1\n"
	  "   while (1) {\n"
	  "      if !isfile(\"basename.%c\") break \n"
	  "      read basename.%c -silent eval @ave(y)\n"
	  "   }\n"
	  ;
PRIVATE BOOL do_while(void) {
	char	*block;
	char	token[DFLT_STR_SIZE];

	if (LexCheckHelp("While", WhileHelp, NULL)) return(TRUE);

	if (! LexGetMathP(token, sizeof(token), "while (condition): ")) return(TRUE);
	block = LexGetBlockStringNT(IF_PROMPT);
	LexExecBlock(block, 0, -1, strdup(token), NULL);
	return(TRUE);
}

static char LoopHelp[] 
 	= "LOOP provides a simple control method to repeat a block of commands a\n"
	  "specific number of times.\n"
	  "\n"
	  "Usage: loop <count> [do | times]      <block statements>\n"
	  "       loop %%x=<start>,<end>[,<inc>] <block statements>\n"
	  "\n"
	  "  In the second form, the start, end and increment values must all be\n"
	  "  together in a single extended token -- ie. no spaces are allowed.\n"
	  "  Expresssions are allowed, but will be converted to nearest integer\n"
	  "  values.  %x (or any other letter) will be substituted with a file\n"
	  "  extension format (ie. normally 007); x may be any character.\n"
	  "\n"
	  "  %c and %i are always substituted with the count and index starting\n"
	  "  from 001 and 000 respectively.\n"
	  "\n"
	  "Notes:\n"
	  "  (1) Block statements may be a single line, or may be multiple lines\n"
	  "      enclosed between { } braces.\n"
	  "  (2) The command CONTINUE will cause the current loop to be completed\n"
	  "      and processing to continue with the next index value\n"
	  "  (3) The command BREAK will exit the loop immediately\n"
	  "\n"
	  "Examples:\n"
	  "   loop 20 echo %c\n"
	  "   loop %a=91,113,1 { read basefile.%a plot hcopy dev pogo }\n"
	  ;
PRIVATE BOOL do_loop(void) {
	int count;
	char *block;
	char *aptr, list[DFLT_STR_SIZE], token[DFLT_STR_SIZE];
	int  istart, iend, inc, ierr;
	char subst;

	if (LexCheckHelp("Loop", LoopHelp, NULL)) return(TRUE);

	if (! LexGetMathP(token, sizeof(token), "Number of times or %x=1,10,4 (abort): ")) {
		return(TRUE);
	} else if (*token != '%') {
		count = nint(GVEvalExpr(token, &ierr));
		if (ierr != 0) {
			ERRprintf("ERROR: Invalid <count> expression (%s)\n", token);
			return(FALSE);
		}
		if (LexChkToken(token, sizeof(token))) {
			if (stricmp(token,"times") == 0 || stricmp(token,"do") == 0) 
				LexGetToken(token, sizeof(token));
		}
		block = LexGetBlockStringNT(IF_PROMPT);
		LexExecBlock(block, 0, count, NULL, NULL);
	} else {
		if ( (subst = token[1]) == '\0' || token[2] != '\0') {
			strcpy(list, token); 
			goto ErrorUsage;
		}
		if (! LexGetListP(list, sizeof(list), "start,end[,inc]")) return(TRUE);

		LexConvertMode = MATH;
		aptr = list;
		if (! LexParseLine(token, sizeof(token), aptr, &aptr)) goto ErrorUsage;
		istart = nint(GVEvalExpr(token, &ierr));
		if (ierr != 0) goto ErrorUsage;
		
		if (! LexParseLine(token, sizeof(token), aptr, &aptr)) goto ErrorUsage;
		iend = nint(GVEvalExpr(token, &ierr));
		if (ierr != 0) goto ErrorUsage;

		if (! LexParseLine(token, sizeof(token), aptr, &aptr)) {
			inc = 1;
		} else {
			inc = nint(GVEvalExpr(token, &ierr));
			if (ierr != 0) goto ErrorUsage;
		}
		if (*aptr != '\0') goto ErrorUsage;

		block = LexGetBlockStringNT(IF_PROMPT);
		LexExecLoop(block, subst, istart, iend, inc);
	}
	return(TRUE);

ErrorUsage:
	ERRprintf("ERROR: Format (%s) invalid.  Use LOOP -? for short help.\n", list);
	return(FALSE);
}

/* ============================================================================
-- Subroutine to translate (expand) a string containing references of form
-- %xxx% which convert to strings.
--
-- Usage: ExpandStrings(char *tokout, char *tokin, INT toklen)
--
-- Inputs: tokin  - input token character*(*)
--         toklen - maximum size of tokout
--
-- Output: tokout - translated token
--
-- Options: \% is translated directly as % even if variable exists
--          \$ is translated directly as $ even if variable exists
--          $<NAME> is translated to environment variable if NAME is
--          is uppercased.
============================================================================ */
PRIVATE void ExpandStrings(char *tokout, char *tokin, INT toklen) {

	char		*inptr, *outptr, *aptr, *cptr;
	char		 varname[2*VARNAME_STR_SIZE];
	char		 lastchr = 0x7F;
	BOOL		 missed_string = FALSE;

	outptr = tokout; inptr = tokin;

	while ((*inptr != '\0') && (toklen > 1)) {
		if (*inptr == '%') {							/* Escaped character? */
			if (lastchr == '\\') {					/* Was it a \% character? */
				outptr--; toklen++;
			} else if (missed_string) {			/* Can we tolerate unmatched? */
				missed_string = FALSE;
			} else if ((cptr=strchr(inptr+1,'%')) != NULL) {	/* Matching %%? */
				if (cptr != inptr+1) strncpy(varname, inptr+1, cptr-inptr-1);
				varname[cptr-inptr-1] = '\0';
				missed_string = TRUE;									/* Assume we fail */
				if ( (aptr = GVFindString(varname)) != NULL) {
					missed_string = FALSE;
					while (*aptr != '\0' && toklen > 1) {
						*outptr++ = *aptr++;					/* Copy over */
						toklen--;
					}
					inptr = cptr+1;							/* Done with characters */
					continue;
				}
			}
		} else if (*inptr == '$') {				/* Environment character? */
			if (lastchr == '\\') {
				outptr--; toklen++;
			} else {
				cptr = inptr+1;
				while (isupper(*cptr)) cptr++;	/* cptr gets last non-upcase */
				if (cptr != inptr+1) {
					strncpy(varname, inptr+1, cptr-inptr-1);
					varname[cptr-inptr-1] = '\0';
					if ( (aptr = getenv(varname)) != NULL) {
						while (*aptr != '\0' && toklen > 1) {
							*outptr++ = *aptr++;
							toklen--;
						}
						inptr = cptr;
						continue;
					}
				}
			}
		}
		lastchr = *(outptr++) = *(inptr++);		/* Copy character over */
		toklen--;
	}
	*outptr = '\0';
	return;
}


/* =========================================================================== */
PRIVATE BOOL do_foreach_list(BOOL islist);

PRIVATE BOOL do_forlists(void) {
	return do_foreach_list(TRUE);
}

PRIVATE BOOL do_foreach(void) {
	return do_foreach_list(FALSE);
}

#define	LIST_BLOCK_SIZE	4096								/* Block size for string list */

static char ForeachHelp[] 
	= "FOREACH and FORLISTS provides a looping control over a list of objects given\n"
	  "on the command or in a string variable.\n"
	  "\n"
	  "Usage: foreach  [%x] [in] (item1, item2, ...)    [do] <block statements>\n"
	  "       forlists [%x] [in] (stringvar, var2, ...) [do] <block statements>\n"
	  "\n"
	  "  %x (or any other letter) will be substituted with each entry in the list,\n"
	  "  potentially including all matching wildcard filenames.  %c and %i are\n"
	  "  substituted with the count (starting at 1) and index (starting at 0) for\n"
	  "  each name.  foreach loops may be nested - though you must use a different\n"
	  "  letter (%x) each time or only the last one will be visible.  If no %x is\n"
	  "  specified, %f will be assumed.\n"
	  "\n"
	  "  foreach and forlists differ only in how the list is presented.  In foreach,\n"
	  "  the list is a collection of items within parenthesis.  The forlists command\n"
	  "  accepts a list of variables/fncs - each of which is evaluated to a list.\n"
	  "\n"
	  "Notes:\n"
	  "  (1) Block statements may be a single line, or may be multiple lines\n"
	  "      enclosed between { } braces.\n"
	  "  (2) The command CONTINUE will cause the current loop to be completed\n"
	  "      and processing to continue with the next index value\n"
	  "  (3) The command BREAK will exit the loop immediately\n"
	  "\n"
	  "Example:\n"
	  "   foreach (1 2 5 8) eval fit(%f)\n"
	  "   foreach (basefile.*) { read %f let summary[%i] = @ave(y) }\n"
	  "   foreach %a (1 2 3) foreach %b (4 5 6) echo %a%b\n"
	  "   define alist = \"a b c\"\n"
	  "   forlists (alist) echo %f\n"				 
	  ;
PRIVATE BOOL do_foreach_list(BOOL islist) {

	char token[LONG_STR_SIZE], *namelist, *listptr, *varlists_ptr;
	char *aptr, *bptr, *fname;
	char wild[PATH_MAX], *namestart;
	char *block;
	char subst='f';								/* Substitution char %f */
	size_t pcnt, len, lenmax, icnt;
	int err;

	DIR *dirp;
	struct dirent *entry;

	if (LexCheckHelp("Foreach", ForeachHelp, NULL)) return(TRUE);

	if (! LexGetToken(token, sizeof(token))) goto BadFormat;
	if (LexEqual(token, "repeat", 3)) {		/* Repeat processing only */
		return(do_loop());
	} else if (*token == '%' && strlen(token) == 2) {
		subst = tolower(token[1]);						/* Normally %f */
		if (LexChkToken(token, sizeof(token))) {
			if (stricmp(token, "in") == 0) LexGetToken(token, sizeof(token));
		}
	} else {
		LexBackup();
	}

	LexGetRestNT(token, sizeof(token));				/* Get all of it */

	aptr = token;
	while (isspace(*aptr)) aptr++;

	if (*(aptr++) != '(') goto BadFormat;
	listptr = aptr;
	while (*aptr && *aptr != ')') aptr++;
	if (*aptr != ')') goto BadFormat;
	*(aptr++) = '\0';										/* End of the list */

	while (isspace(*aptr)) aptr++;
	if (strnicmp(aptr, "do", 2) == 0) {				/* Make the do optional */
		aptr += 2;
		if (! isspace(*(aptr++))) goto BadFormat;
	}
	LexInsText(aptr);										/* Return everything else */

/* For lists, first expand strings replacing the listptr string */
	len = lenmax = 0;
	fname = varlists_ptr = NULL;
	if (islist) {
		while (LexParseLine(wild, sizeof(wild), listptr, &listptr)) {
			aptr = GVEvalStrExpr(wild, &err);
			if (aptr == NULL || err != 0) {
				ERRprintf("ERROR: Invalid string variable in forlists command (%s)\n", wild);
				return(FALSE);
			}
			if (len < strlen(aptr)+1) {				/* Have enough space? */
				icnt = LIST_BLOCK_SIZE;
				while (len+icnt < strlen(aptr)+1) icnt += LIST_BLOCK_SIZE;
				lenmax += icnt;
				len    += icnt;
				icnt = fname-varlists_ptr;
				varlists_ptr = realloc(varlists_ptr, lenmax);
				fname = varlists_ptr + icnt;
			}
			if (fname != varlists_ptr) *(fname++) = ' ';
			strcpy(fname, aptr); fname += strlen(fname);
			free(aptr);
		}
		listptr = varlists_ptr;
	}

/* Expand the given list (qualified with strings, wildcards) into a complete unqualified list */
/* The list will end up in the string namelist which expands without limit 4096 chars at a time */
/* fname is next open location in the namelist string */
	len = lenmax = LIST_BLOCK_SIZE;
	fname = namelist = malloc(len);
	dirp = NULL;

	while (TRUE) {
		if (len < PATH_MAX) {							/* Have at least PATH_MAX free */
			lenmax += LIST_BLOCK_SIZE;
			len    += LIST_BLOCK_SIZE;
			icnt = fname-namelist;
			namelist = realloc(namelist, lenmax);
			fname = namelist + icnt;
		}
		/* If actively working with a wildcard list, go ahead and process next one now */
		if (dirp != NULL) {								/* If wildcard, use it now */
			if ( (entry = readdir(dirp)) == NULL) {
				closedir(dirp);
				dirp = NULL;
				continue;
			}
			if (strcmp(entry->d_name, ".")  == 0 || strcmp(entry->d_name, "..") == 0) continue;
			if (! SysCheckMatch(entry->d_name, namestart) ) continue;
			strncpy(fname, wild, pcnt);					/* Copy the chars			*/
			strcpy (fname+pcnt, entry->d_name);			/* And append the name	*/
			len   -= strlen(fname)+1;
			fname += strlen(fname)+1;
		/* No using current wildcard, so go ahead and parse next entry from given list */
		} else {
			if (! LexParseLine(wild, sizeof(wild), listptr, &listptr)) break;
			ExpandStrings(fname, wild, (int) len);
			SysMarkPath(fname, &aptr, &bptr);			/* Check for wildcards	*/
			if (strpbrk(aptr, "?*") == NULL) {
				len   -= strlen(fname)+1;
				fname += strlen(fname)+1;
				continue;
			}
			strcpy(wild, fname);
			pcnt = aptr-fname;								/* # of chars in root name */
			namestart = wild + pcnt;
			*bptr = '\0';										/* Truncate at directory */
			if (*fname == '\0') strcpy(fname, ".");
			if ( (dirp = opendir(fname)) == NULL) {
				ERRprintf("ERROR: * or ?? specified with invalid directory (%s)\n", fname);
				continue;
			}
		}
	}
	*fname = '\0';											/* Double NULL terminator */

	if (islist) free(varlists_ptr);

	block = LexGetBlockStringNT(IF_PROMPT);
	LexExecBlock(block, subst, -1, NULL, namelist);

	return(TRUE);

BadFormat:
	ERRprintf("ERROR: Invalid format - get with the game, man!  Or try FOREACH -HELP!\n");
	return(FALSE);
}

/* -------------------------------------------------------------------------
-- break()    - break out of current "foreach" or "loop" command if active
-- continue() - continue next term of "foreach" or "loop" command if active
---------------------------------------------------- */
PRIVATE BOOL do_break(void) {
	if (LexExecFile("-break")) return(TRUE);
		ERRprintf("ERROR: Cannot BREAK.  No foreach/loop/while block is active.\n");
		return(FALSE);
}

PRIVATE BOOL do_continue(void) {
	if (LexExecFile("-continue")) return(TRUE);
		ERRprintf("ERROR: Cannot CONTINUE.  No foreach/loop/while block is active.\n");
		return(FALSE);
}

/* ----------------------------------------------------
-- printf() command inside genplot
---------------------------------------------------- */
PRIVATE BOOL do_fprintf(void) {
	char format[DFLT_STR_SIZE], *string;
	
	if (! LexGetStrExpr(format, sizeof(format))) {
		TTYprintf("  Usage: fprintf \"format\" [<expr1> [<expr2> ...]] \n");
		return(TRUE);
	}

	string = LexEncodeString(NULL, 0, format);

	if (! SysLogFile(SYSLOG_CHECK,NULL,NULL)) {
		ERRputs("WARNING: No active SCRIPT file - comments lost\n");
		TTYputsnl(string);
	} else {
		SysLogFile(SYSLOG_WRITE, "*** BEGIN COMMENTS ***", NULL);	/* Output */
		SysLogFile(SYSLOG_FORCE, string, NULL);
	}
	free(string);
	return(TRUE);
}

/* -------------------------------------------------------
-- LOGIT - Add a logging comment to the current log file
------------------------------------------------------- */
PRIVATE BOOL do_logit(void) {

	char token[LONG_STR_SIZE];
	int ConsoleSave;
	
	if (! SysLogFile(SYSLOG_CHECK,NULL,NULL)) 
		ERRputs("ERROR: No active SCRIPT file - comments lost\n");

	SysLogFile(SYSLOG_WRITE, "*** BEGIN COMMENTS ***", NULL);	/* Output */
	LexGetRest(token, sizeof(token));

	if (strnblen(token) != 0) {
		SysLogFile(SYSLOG_FORCE,token,NULL);
	} else {
#if (defined OS2 || defined NT)
		TTYputs("Enter LOG file comments. End with ^Z\n");
#else
		TTYputs("Enter LOG file comments. End with ^D\n");
#endif
		SysLogFile(SYSLOG_TMPUNPAUSE,NULL,NULL);			/* Temporarily enable	*/
		ConsoleSave = SysConsoleBehavior;					/* Save for restore		*/
		SysConsoleBehavior |= SYSCONSOLE_PASS_CTRL_Z;	/* Need ^Z passed			*/
		while (TRUE) {												/* Just put in log		*/
			if (! UserInput("\0",token,sizeof(token))) break;
		}
		SysConsoleBehavior = ConsoleSave;					/* Reset flag				*/
		SysLogFile(SYSLOG_TMPPAUSE,NULL,NULL);				/* Reset pause mode		*/
	}
	SysLogFile(SYSLOG_WRITE,"*** END   COMMENTS ***",NULL);
	return(TRUE);
}

/* ----------------------------------------------------------------------
-- MACRO - Create and temporarily save a macro  (almost obsolete!!)
---------------------------------------------------------------------- */
PRIVATE BOOL do_macro(void) {

	FILE *funit=NULL;
	char token[DFLT_STR_SIZE];
	int ConsoleSave;

	if (MacroOn && (funit=fopen(MacroName, "w")) == NULL)
		ERRprintf("Warning: Unable to reopen old macro file - %s\n", MacroName);

	if (funit == NULL) {
		if ( (funit = SysTmpFile(MacroName, NULL, ".mac", "w")) == NULL) {
			ERRprintf("ERROR: Unable to create temporary macro filename\n");
			return(FALSE);
		}
	}

	MacroOn = TRUE;
	TTYputc('\n');
	SysQualifyPath(MacroName, MacroName, sizeof(MacroName));
	type2("MACRO opened on: ", MacroName);
#if (defined OS2 || defined NT)
	TTYputs("Enter macro command lines.  End w/ EOF character (^Z)\n");
#else
	TTYputs("Enter macro command lines.  End w/ EOF character (^D)\n");
#endif

	ConsoleSave = SysConsoleBehavior;					/* Save for restore	*/
	SysConsoleBehavior |= SYSCONSOLE_PASS_CTRL_Z;	/* Need ^Z passed		*/
	while (UserInput(": ",token,sizeof(token))) { 
		fputs(token,funit); 
		fputc('\n',funit);
	}
	SysConsoleBehavior = ConsoleSave;					/* Reset flag			*/
	fclose(funit);
	return(TRUE);
}

/* --------------------------------------------
-- System MEMORY - report on available memory
-------------------------------------------- */
PRIVATE BOOL do_mem(void) {
	REAL maxblock;
	maxblock = SysQueryMem()/1024.0f;
	TTYprintf("Virtually unlimited.  Largest free block now: %.1f kB\n",maxblock);
	return(TRUE);
}

/* ----------------------------------------
-- TYPE - Prints out a file to the terminal
---------------------------------------- */
PRIVATE BOOL do_more(void) {
	char pathname[PATH_MAX];
	if (LexGetFileP(pathname, sizeof(pathname), "Filename: ")) SysFileMore(pathname);
	return(TRUE);
}

/* ----------------------------------------
-- "C" line (C becomes ignored!)
---------------------------------------- */
PRIVATE BOOL do_nop(void) {
	return(TRUE);
}

/* ---------------------------------------------------
-- PAUSE command - Push a secondary command processor
------------------------------------------------------ */
PRIVATE BOOL do_pause(void) {
	return(SysPushShell());
}

/* ----------------------------------------
-- WHERE - Returns current attach point
---------------------------------------- */
PRIVATE BOOL do_pwd(void) {
	char pathname[PATH_MAX];

	if (LexCheckHelp("Directory commands", CD_Help, NULL)) return(TRUE);

	TTYprintf(" Current directory: %s\n", 
		(getcwd(pathname, sizeof(pathname)) != NULL) ? pathname : "<unable to determine>");
	return(TRUE);
}

/* ------------------------------------------------
-- SAVE_MACRO Save the macro to specified file
------------------------------------------------ */
PRIVATE BOOL do_savemacro(void) {

	char pathname[PATH_MAX];
	
	if (! MacroOn) {
		ERRputs("ERROR: No temporary macro file to save\n");
		return(FALSE);
	}
	MacroOn = FALSE;										/* Rid of this puppy */

	if (LexGetFileP(pathname, sizeof(pathname), "Filename: (default) ") &&
		  strcmp(pathname,"/") != 0) {
		SysAddExt(pathname, ".mac");
		switch (SysMoveFile(MacroName, pathname)) {
			case 0:
				break;
			case 1:
				ERRprintf("ERROR: Cannot overwrite existing file %s\n"
							 "       File saved as: %s\n", pathname, MacroName);
				break;
			default:
				ERRprintf("ERROR: Both rename() and copy() failed on %s\n"
							 "       File saved as: %s\n", pathname, MacroName);
		}
	} else {
		TTYprintf("INFO: Temporary macro saved as %s\n", MacroName);
	}

	return(TRUE);
}

/* ----------------------------------------
-- SCRIPT processor.
---------------------------------------- */
PRIVATE BOOL do_script(void) {

	char	token[SHORT_STR_SIZE];
	int	OpenType, OpenMode;
	
	if (! LexChkToken(token, sizeof(token))) 
		LexReadLine("{ <filename> [-APPEND][-LOGONLY|-COMMAND] |-END|-PAUSE|-CONT}: ");

	if (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token,"-END",2) || LexEqual(token,"-CLOSE",3)) {
			if (SysLogFile(SYSLOG_CLOSE,NULL,NULL)) TTYputs("SCRIPT session closed\n");
		} else if (LexEqual(token,"-PAUSE",2)) {
			if (! SysLogFile(SYSLOG_PAUSE,NULL,NULL))						/* Pause unit */
				ERRputs("WARNING: No SCRIPT session active\n");
		} else if (LexEqual(token,"-CONTINUE",2)) {
			if (! SysLogFile(SYSLOG_UNPAUSE,NULL,NULL)) {
				ERRputs("ERROR: No SCRIPT session previously paused or unable to continue\n");
				return(FALSE);
			}
		} else {
			ERRprintf("ERROR: Illegal option (%s)\n", token);
			return(FALSE);
		}
	} else if (LexGetFile(ScriptFile, sizeof(ScriptFile))) {	/* Specify a name */
		OpenType = SYSLOG_OPEN;									/* Open, no append */
		OpenMode = 1;												/* w/ on always */
		while (LexGetOption(token, sizeof(token))) {
			if (LexEqual(token,"-APPEND",3))
				OpenType = SYSLOG_APPEND;
			else if (LexEqual(token,"-LOGONLY",4))
				OpenMode = 0;
			else if (LexEqual(token,"-COMMANDS",3))
				OpenMode = 3;
			else {
				ERRprintf("ERROR: Invalid SCRIPT open option (%s)\n",token);
				return(FALSE);
			}
		}
		if (! SysLogFile(OpenType,ScriptFile,NULL)) {
			ERRputs("ERROR: Unable to open SCRIPT session file\n");
			return(FALSE);
		}
		SysLogFile(SYSLOG_SETMODE,NULL,(FILE **) &OpenMode);	/* Set LOG mode */
	}
	return(TRUE);
}

/* ----------------------------------------
-- SLEEP command 
---------------------------------------- */
PRIVATE BOOL do_sleep(void) {
	REAL time;
	int nblocks;
	time = LexGetReal(1.0f, "How many seconds to sleep (1 sec): ");
	/* Do in increments of 250 ms so can catch ^C.  This seems exact on PC */
	nblocks = nint(time*4);
	while (nblocks-- > 0 && ! SysChkBreak(FALSE)) MilliSleep(250);
	return(TRUE);
}

/* ----------------------------------------------------------------------
--  OK; command  - On PC generates a second copy of command processor
----------------------------------------------------------------------- */
PRIVATE BOOL do_system(void) {
	char token[LONG_STR_SIZE];
	LexGetRest(token, sizeof(token));
	if (strnblen(token) == 0)
		LexPromptStr(token, sizeof(token), "Internal command: ");

	if (strnblen(token) == 0) {
		if (SysPushShell() == 0) return(TRUE);
	} else {
		if (SysSystem(token) == 0) return(TRUE);
	}
	return(FALSE);
	
}

/* ---------------------------------------
-- WAIT  command - FORTRAN pause
---------------------------------------- */
static char WaitHelp[]
	= "WAIT is a method of pausing for user delay before proceeding with\n"
     "processing.  Commonly used between graphs to provide an indeterminate\n"
	  "time for human interpretation (compare to SLEEP).  Options provide for\n"
     "specific text and for an automatic timeout.\n"
	  "\n"
	  "Usage:\n"
	  "  wait [-prompt \"text prompt\"]  [-timeout <seconds>]\n"
	  "\n"
	  "Notes:\n"
	  "   (1) Default prompt is \"Press any key to continue ... \"\n"
     "   (2) Default timeout is forever\n"
	  "   (3) Timeout now works properly on Windows.  Other OS's not tested.\n"
     "   (4) Value of key pressed is returned in the $key variable.  If\n"
     "       a timeout occured, value is 0\n"
	  "\n"
	  "Examples:\n"
	  "   wait\n"
	  "   wait -prompt \"Press Y to continue or any other key to keep going\"\n"
	  "   wait -prompt \"You got 5 seconds to hit a key\" -timeout 5\n";

PRIVATE BOOL do_wait(void) {
	char format[DFLT_STR_SIZE], string[DFLT_STR_SIZE];
	char *prompt = "\nPress any key to continue ... ";
	int timeout;
	static int key=0;
	static BOOL key_linked = FALSE;

	if (LexCheckHelp("Wait", WaitHelp, NULL)) return(TRUE);

	while (LexGetOption(format, sizeof(format))) {
		if (LexEqual(format,"-PROMPT",2) || LexEqual(format,"-TEXT",2)) {
			LexGetToken(format, sizeof(format));
			LexEncodeString(string, sizeof(string), format);
			prompt = string;
		} else if (LexEqual(format, "-timeout", 5)) {
			timeout = LexGetInt(1, "Timeout (1 second): ");
		} else {
			ERRprintf("ERROR: Illegal option (%s)\n", format);
			return(FALSE);
		}
	}

/* Link the variable key so user can determine key pressed to exit wait */
	key = 0;
	if (! key_linked) {
		GVLinkInt("$key", GVF_INTERNAL | GVF_HIDDEN, &key);
		key_linked = TRUE;
	}

	CONputs(prompt);
	if (timeout <= 0) {
		key = CONgetc();
	} else {
		CONwaitchr(nint(1000*timeout));
	}
	CONputs("\r\n");
	return(TRUE);
}

/* -------------------------------------
-- XEQ - execute a specified macro
------------------------------------- */
PRIVATE BOOL do_xxeq(void) {
	char tok2[4];

	LexConvertMode = NO_AMP_EXPAND | NO_TRANSLATE;
	if (LexChkToken(tok2,sizeof(tok2)) && strcmp(tok2,"=") == 0) {
		LexGetToken(tok2, sizeof(tok2));
		if (! ImplicitSetvar("x")) LexFlush();
		return(TRUE);
	}
	return(do_xeq());
}

PRIVATE BOOL do_xeq(void) {
	char pathname[PATH_MAX];

	if (! LexGetFileP(pathname, sizeof(pathname), "XEQ file: (NOW) ")) 
		strcpy(pathname, "/");														/* Default */

	if ( stricmp(pathname, "now") == 0 	|| stricmp(pathname, "n") == 0
		  || stricmp(pathname, "/") == 0 ) {
		if (! MacroOn) {
			ERRputs("ERROR: No temporary macro defined\n");
			return(FALSE);
		}
		strcpy(pathname, MacroName);
	}
	return(LexExecFile(pathname));
}

/* -------------------------------------
-- ALLOCATE - allocate variables
------------------------------------- */
static char AllocHelp[]
	= "ALLOCATE is the most general command for allocating internal variables\n"
	  "for general mathematical operations.  Other commands such as SETVAR\n"
	  "and DECLARE exist for creating and setting one type of variable.  ALLOC\n"
	  "can create these (real and string) as well as curves, arrays, and complex\n"
     "variables.\n"
	  "\n"
	  "Usage:\n"
	  "  alloc <varname>[,<var2>,...] <type> [<qualifier>] [<size>]\n"
     "\n"
     "     <varname>[,<var2>,...]   - name(s) to be created\n"
     "     <type>                   - type of variable to create (below)\n"
     "     [<qualifier>]            - allowed qualifier on some types (below)\n"
     "     [<size>]                 - size for arrays, curves, or surfaces\n"
	  "\n"
	  "Allowed types (and qualifiers/sizes) are:\n"
	  "   real | float         - simple real number\n"
	  "   double               - double precision real value\n"
     "   integer              - 32 bit integer value\n"
	  "   complex              - single precision complex number\n"
     "   string <size>        - string with <size> characters\n"
     "   array <size>         - real number array with <size> elements\n"
	  "   complex_array <size> - complex number array with <size> elements\n"
     "   c_array       <size> - synonymous with complex_array\n"
	  "   string_array  <size> - string array with <size> elements\n"
     "   string array         - (alternate for string_array)\n"
	  "   s_array       <size> - synonymous with string_array\n"
	  "   curve         <size> - 2D curve with <size> points\n"
	  "   2D_curve      <size> - 2D curve with <size> points\n"
	  "   3D_curve      <size> - 3D curve with <size> points\n"
	  "   surface  <row> <col> - Surface of specified size\n"
	  "   fileptr              - File pointer (for fopen)\n"
     "Only the first two or 3 letters are required\n"
	  "\n"
	  "For compatibility and readibility, the following do the expected\n"
     "   real array    ==> array\n"
	  "   string array  ==> string_array\n"
     "   complex array ==> complex_array\n"
	  "   2D curve      ==> 2D_curve\n"
	  "   3D curve      ==> 3D_curve\n"
	  "\n"
	  "Examples:\n"
	  "   alloc xl,xh array 100\n"
     "   alloc cstr string_array 5\n"
	  "   alloc c1 curve npt\n";

PRIVATE BOOL do_alloc(void) {

	char varname[VARNAME_STR_SIZE], list[DFLT_STR_SIZE], *tokptr;
	char token[DFLT_STR_SIZE];
	int  oldtype, type, length, nrow, ncol;
	BOOL rcode, allokay;
	COMPLEX z;

	typedef struct CMTYPE2 {
		char *cmd;
		int minlen;
		int rcode;
	} CMTYPE2;

	static CMTYPE2 cmlist[] = {
		{"?",		  -1, 0},	{"list",	-4, 0},	{"-?", -2, 0},
		{"real",		2,	1},	{"float", -2, 1},
		{"double",	2, 2},
		{"integer", 3, 3},	
		{"complex",	2,	4},
		{"string",	2,	5},
		{"array",	2,	6},
		{"c_array",	3,	7},	{"complex_array", -8, 7},
		{"s_array",	3,	8},	{"string_array", -7, 8},
		{"curve",	2,	9},
		{"2d_curve",2,	10},
		{"3d_curve",2,	11},
		{"surface",	2,	12},
		{"fileptr",	2,	13},
		{NULL,		0, 0}
	};
	CMTYPE2 *cmitem;

/* Check for help request */
	if (LexCheckHelp("Allocate", AllocHelp, NULL)) return(TRUE);

	if (! LexGetListP(list,   sizeof(list),  "Variable name(s) to allocate: ")) return(TRUE);
	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), "Type (? gives list): ")) return(TRUE);
		if ( (cmitem = LexCmdl(token, cmlist, sizeof(CMTYPE2))) == NULL) {
			ERRprintf("ERROR: Sorry, invalid variable type specified! (%s)\n", token);
			return(FALSE);
		}
		if ( (type = cmitem->rcode) != 0) break;		/* Keep in while() for help only */
		TTYprintf("   REAL                - simple floating point number\n"
					 "   DOUBLE              - double precision floating point value\n"
					 "   INTEGER             - 32 bit integer value\n"
					 "   COMPLEX             - two REAL component complex value\n"
					 "   STRING              - simple string variable\n"
					 "   CURVE               - 2D curve\n"
					 "   2D                  - 2D curve\n"
					 "   3D                  - 3D curve\n"
					 "   SURFACE             - Surface (matrix) representation of 3D\n"
					 "   ARRAY               - Array of REAL values\n"
					 "   C_ARRAY             - Array of COMPLEX values\n"
					 "   S_ARRAY             - Array of STRING values\n"
					 "   REAL ARRAY          - Array of REAL values\n"
					 "   COMPLEX ARRAY       - Array of COMPLEX values\n"
					 "   STRING ARRAY        - Array of STRING values\n"
					 "   FILEPTR             - File handle (pointer)\n");
	}

/* Allow formats "ALLOC REAL ARRAY" "ALLOC COMPLEX ARRAY" "ALLOC 2D CURVE" */
	if ( (type==1 || type==4 || type == 5 || type==10 || type==11) && LexChkToken(token, sizeof(token)) ) {
		if ( (type == 1 || type == 4 || type == 5) && LexEqual(token, "ARRAY", 3) ) {
			LexGetToken(token, sizeof(token));		/* Dump the token */
			if (type == 1) type = 6;
			if (type == 4) type = 7;
			if (type == 5) type = 8;
		} else if ( (type == 10 || type == 11) && LexEqual(token, "CURVE", 3) ) {
			LexGetToken(token, sizeof(token));		/* Dump the token */
		}
	}

/* Request size if needed */
	switch (type) {
		case 5:
			length = LexGetInt(1, "Initial string length (1): ");
			break;
		case 6:
		case 7:
		case 8:
			length = LexGetInt(8, "Number of elements in array (8): ");
			break;
		case 9:
		case 10:
		case 11:
			length = LexGetInt(200, "Number of points in the curve (200): ");
			break;
		case 12:
			nrow = LexGetInt(10, "Number of rows in the surface matrix (10): ");
			ncol = LexGetInt(10, "Number of columns in the surface matrix (10): ");
			break;
		default:
			break;
	}

	allokay = TRUE;									/* Assume we are successful */
	tokptr = list;
	while (LexParseLine(varname, sizeof(varname), tokptr, &tokptr)) {
		if (! GVIsNameValid(varname)) {
			ERRprintf("ERROR: %s is not a valid variable name.\n", varname);
			allokay = FALSE;
			continue;
		}
		if (! GVGetInfo(varname, &oldtype, NULL)) oldtype = -1;
		switch (type) {
			case 1:
				if (oldtype != -1 && oldtype != GV_REAL) ERRprintf(VarDefChanged);
				rcode = GVAllocReal(varname, GVF_USER, 0.0f);
				break;
			case 2:
				if (oldtype != -1 && oldtype != GV_DOUBLE) ERRprintf(VarDefChanged);
				rcode = GVAllocDouble(varname, GVF_USER, 0.0);
				break;
			case 3:
				if (oldtype != -1 && oldtype != GV_INT) ERRprintf(VarDefChanged);
				rcode = GVAllocInt(varname, GVF_USER, 0);
				break;
			case 4:
				if (oldtype != -1 && oldtype != GV_COMPLEX) ERRprintf(VarDefChanged);
				z.x = z.y = 0.0f;
				rcode = GVAllocComplex(varname, GVF_USER, z);
				break;
			case 5:
				if (oldtype != -1 && oldtype != GV_STRING) ERRprintf(VarDefChanged);
				rcode = GVAllocString(varname, GVF_USER, length);
				break;
			case 6:
				if (oldtype != -1 && oldtype != GV_ARRAY) ERRprintf(VarDefChanged);
				rcode = GVAllocArray(varname, GVF_USER, length);
				break;
			case 7:													/* Complex array curve */
				if (oldtype != -1 && oldtype != GV_COMPLEX_ARRAY) ERRprintf(VarDefChanged);
				rcode = GVAllocComplexArray(varname, GVF_USER, length);
				break;
			case 8:
				if (oldtype != -1 && oldtype != GV_STRING_ARRAY) ERRprintf(VarDefChanged);
				rcode = GVAllocStrArray(varname, GVF_USER, length);
				break;
			case 9:													/* 2D curve */
			case 10:
				if (oldtype != -1 && oldtype != GV_2DCURVE) ERRprintf(VarDefChanged);
				rcode = GVAlloc2DCurve(varname, GVF_USER, length);
				break;
			case 11:
				if (oldtype != -1 && oldtype != GV_3DCURVE) ERRprintf(VarDefChanged);
				rcode = GVAlloc3DCurve(varname, GVF_USER, length);
				break;
			case 12:
				if (oldtype != -1 && oldtype != GV_SURFACE) ERRprintf(VarDefChanged);
				rcode = GVAllocSurface(varname, GVF_USER, nrow, ncol);
				break;
			case 13:
				if (oldtype != -1 && oldtype != GV_FILEPTR) ERRprintf(VarDefChanged);
				rcode = GVAllocFilePtr(varname, GVF_USER);
				break;
			default:
				ERRputs("ERROR: Sorry, type not implemented (real screwup!)\n");
				return(FALSE);
		}
		if (! rcode) {
			ERRprintf("ERROR: Bad name or insufficient memory (%s)\n", varname);
			allokay = FALSE;
		}
	}
	return(allokay);
}

/* ------------------------------------------------------------
-- DEALLOCATE - deallocate variables from function evaluator
--
-- -all will delete all variables of class GVF_USER
-----------------------=------------------------------------- */
PRIVATE BOOL do_dealloc(void) {

	char list[DFLT_STR_SIZE];
	
	if (! LexGetListP(list, sizeof(list), "Variable(s): ")) return(TRUE);

	if (stricmp(list, "-all") != 0) {
		char name[VARNAME_STR_SIZE], *tokptr=list;
		while (LexParseLine(name, sizeof(name), tokptr, &tokptr))
			if (! GVDeallocate(name)) ERRprintf("WARNING: Could not deallocate %s\n", name);
	} else {
		void *entry=NULL;
		char *varname, **vars=NULL;
		int i, flags, icnt=0, imax=0;
		while (GVGetNextEntry(&entry, &varname, NULL, &flags, NULL)) {
			if (flags & GVF_USER) {					/* Only flagged as GVF_USER */
				if (icnt >= imax) imax+=20, vars=realloc(vars,imax*sizeof(*vars));
				vars[icnt++] = strdup(varname);
			}
		}
		for (i=0; i<icnt; i++) {					
			if (! GVDeallocate(vars[i])) ERRprintf("WARNING: Could not deallocate %s\n", vars[i]);
			free(vars[i]);								/* And free the "dup'd" space */
		}
		free(vars);
	}
	return(TRUE);
}

/* ------------------------------------------------------------
-- SETLOCAL - set local for all new variables defined from here on
-- ENDLOCAL - end local mode for variable definitions
--
-- -all will delete all variables of class GVF_USER
-----------------------=------------------------------------- */
PRIVATE BOOL do_setlocal(void) {
	GVSetLocal();
	return(TRUE);
}

PRIVATE BOOL do_endlocal(void) {
	GVEndLocal();
	return(TRUE);
}

PRIVATE BOOL do_setglobal(void) {

	char name[VARNAME_STR_SIZE], list[DFLT_STR_SIZE], *tokptr=list;
	
	if (! LexGetListP(list, sizeof(list), "Variable(s): ")) return(TRUE);
	while (LexParseLine(name, sizeof(name), tokptr, &tokptr))
		if (! GVMakeGlobal(name)) ERRprintf("WARNING: Could not convert %s to global status\n", name);
	return(TRUE);
}


/* ----------------------------------
-- DECLARE - set a string variable --
------------------------------------- */
PRIVATE BOOL do_declare(void) {

	char varname[VARNAME_STR_SIZE];
	char value[LONG_STR_SIZE] = "";
	int  oldtype;
	BOOL rcode;
	
	if (! LexGetTokenP(varname, sizeof(varname), "String name: ")) return(TRUE);

	if (! GVIsNameValid(varname)) {
		ERRprintf("ERROR: %s is not a valid string variable name.\n", varname);
		return(FALSE);
	}

	if (! LexGetMath (value,   sizeof(value)))
		LexPromptStr(value, sizeof(value), "Definition:  ");
	else if (strcmp(value,"=") == 0) {
		if (! LexGetMath(value, sizeof(value))) 
			LexPromptStr(value, sizeof(value), "Definition: ");
	}

	if (! GVGetInfo(varname, &oldtype, NULL)) {
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else if (oldtype != GV_STRING) {
		ERRprintf(VarDefChanged);
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else {
		rcode = TRUE;
	}
	
	if (rcode) rcode = GVSetValue(varname, value);
	if (! rcode) ERRprintf("ERROR: Bad name or insufficient memory (%s)\n", varname);
	return(rcode);
}

/*----------------------------------------------------------------------------
-- DEFINE - define functions 
--
-- Compatibility issues.  define str "string"  must work properly to set a 
-- string instead of a function.  Must also properly replace string with
-- function if there is a define str(x) "string"
---------------------------------------------------------------------------- */
/**
Needs: To specify the type of function in a define
  define [real complex string] f(x, char *var, double *r, REAL *x,
  double *x)
 * **/

static char Define_Help[] 
	= "The define function allocates a structure for a function definition with\n"
	  "appropriate dummy variables.\n"
	  "\n"
	  "   define <fnc>(arg1, arg2, ...)\n"
	  "   define [#]<fnc> ( [#|*]<arg1> [, ... ] )\n"
	  "\n"
	  "Arguments and the function return are, by default, numeric values (possibly\n"
     "real or complex) and are evaluated at full precision of the system (double or\n"
     "or higher precision).\n"
     "\n"
	  "If a dummy variable is prefixed with a # character, it is taken as a string\n"
	  "argument.\n"
	  "\n"
	  "If a dummy variable is prefixed with a * character, it is taken as a\n"
	  """replacement"" string in the definition -- at evaluation time the argument\n"
     "appears as if it had been typed in place of the dummy argument.  Thus compound\n"
     "variable structures such as curve:y or array[7] can be used.  See examples.\n"
	  "\n"
	  "Finally, if the function name is prefixed with a # character, then the\n"
     "function returns a string value rather than a numeric value.\n"
	  "\n"
	  "Examples:\n"
	  "   define f(x) = a*sin(x)+b*cos(x)         - a and b are values\n"
     "   define f(x,a,b) = a*sin(x)+b*cos(x)     - a and b are dummy parameters\n"
	  "   define f(x,#name) = x*strlen(name)      - mixed string and numeric\n"
     "   define #fname(#name,nmax) = substr(name, pos("","", name)+2, nmax)\n"
     "     eval fname(""Thompson, Mike"", 10) = ""Mike""\n"
     "\n"
     "   define f(x,*ar) = ar[0]+ar[1]*x+ar[2]*x^2\n"
	  "     alloc cp array 3 let cp(..) = 1 2 3 eval f(1,cp)\n"
	  "   define f(*c) = @max(c:y)-@min(c:y)\n"
	  "     archive a_curve eval f(a_curve)\n"
	  "   define f(x,*c,*a,j) = sin(x+c:npt*a[j])\n"
	  ;

PRIVATE BOOL do_define(void) {

	char varname[SHORT_STR_SIZE], value[LONG_STR_SIZE], *aptr;
	int  oldtype;
	BOOL exists;
	
	if (LexCheckHelp("Define", Define_Help, NULL)) return(TRUE);

	if (! LexGetMathP(varname, sizeof(varname), "Function name [ex. U(V,S)]: ")) return(TRUE);
	if (! LexGetMath(value, sizeof(value)))
		LexPromptStr(value, sizeof(value), "Definition: ");
	if (*value == '\0') return(TRUE);

	aptr = strchr(varname,'(');
	if (aptr != NULL) *aptr = '\0';				/* Simple name */

	if (! GVIsNameValid((*varname == '#')?varname+1:varname)) {		/* Allow string function definition */
		ERRprintf("ERROR: %s is not a valid function name.\n", varname);
		return(FALSE);
	}

	exists = GVGetInfo(varname, &oldtype, NULL);
	if (aptr != NULL) *aptr = '(';				/* And reset */

	if (exists && oldtype != GV_FUNCTION) ERRprintf(VarDefChanged);

	if (! GVAllocFnc(varname, GVF_USER, value)) {
		ERRprintf("ERROR: Function name or specification invalid (%s)\n", varname);
		return(FALSE);
	}
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Piddly routine to check a token to see if it really is a list of
-- numbers separated by ,'s.  If yes, only first is kept and remainder
-- is pushed back on command line.  Used to allow
--      let a,b,c,d = &boxcursor
-- type commands.
--
-- Takes character string and scans following parenthesis.  If we get a
-- comma outside a {} bracket, assume it is a second evaluation and put back
-- on the command line.  Will be retrieved next time.
--------------------------------------------------------------------------- */
static void CheckAndPush(char *value) {
	int paren=0;
	char *aptr;

	for (aptr=value; *aptr!='\0'; aptr++) {
		if (strchr("([{", *aptr) != NULL) {
			paren++;
		} else if (strchr(")]}", *aptr) != NULL) {
			paren--;
		} else if (paren == 0 && *aptr == ',') {
			LexInsText(aptr+1);
			*aptr = '\0';
			break;
		}
	}
	return;
}
	
/*------------------------------------------------------------
-- ImplicitSetvar - implicit let/setv command line when no other command
--                  is recognized.  If variable exists, then set as if the
--                  command were a "let".  If it doesn't exist, then set as
--                  if a SETVAR with an informational message that it is being
--                  allocated.
--
-- Usage:  BOOL ImplicitSetvar(char *token);
--
-- Inputs: token - variable name first extracted from the command line
--
-- Output: Passes control to either do_let or do_setvar
-------------------------------------------------------------- */
PRIVATE BOOL ImplicitSetvar(char *varname) {

	char nameonly[DFLT_STR_SIZE], value[LONG_STR_SIZE];
	int  type;
	BOOL rcode, simplevar;
	COMPLEX czero={0.0f,0.0f};
	char *AllocType, *aptr;

	strscpy(nameonly, varname, sizeof(nameonly));
	if ( (aptr = strpbrk(nameonly, "([{")) != NULL) {
		*aptr = '\0';
		simplevar = FALSE;
	} else {
		simplevar = TRUE;
	}

/* If variable does not exist, create as a real or as function (if any () ) */
	if (! GVGetAdrInfo(nameonly, &type, NULL, NULL)) {		/* Does it not exist? */
		if (simplevar) {												/* Make it a SETVAR */
			if (! GVIsNameValid(varname)) {
				ERRprintf("ERROR: %s is not a valid variable name for implicit setvar.\n", varname);
				return(FALSE);
			}
			if (! LexGetMathP(value, sizeof(value), "Value: ")) return(TRUE);
			if (LexConvertMode & QUOTED) {
				AllocType = "STRING";
				rcode = GVAllocString(varname, GVF_USER, 0);
			} else switch (GVGuessExprType(value)) {
				case GVP_STRING:										/* String type */
					AllocType = "STRING";
					rcode = GVAllocString(varname, GVF_USER, 0);
					break;
				case GVP_FILEPTR:
					AllocType = "FILE HANDLE";
					rcode = GVAllocFilePtr(varname, GVF_USER);
					break;
				default:
					if (GVMathMode & MATHCOMPLEX) {
						AllocType = "COMPLEX";
						rcode = GVAllocComplex(varname, GVF_USER, czero);
					} else {
						AllocType = "REAL";
						rcode = GVAllocReal(varname, GVF_USER, 0.0f);
					}
			}
			if (rcode) rcode = GVSetValue(varname, value);
		} else {
			AllocType = "FUNCTION";
			if (! LexGetMath(value, sizeof(value))) LexPromptStr(value, sizeof(value), "Definition: ");
			if (*value == '\0') return(TRUE);
			rcode = GVAllocFnc(varname, GVF_USER, value);
		}
		if (! rcode) {
			ERRprintf("ERROR: Bad variable name or insufficient memory (%s)\n", varname);
		} else {
			TTYprintf("MSG: Variable %s has been implicitly allocated as type %s\n", varname, AllocType);
		}
		return(rcode);
	}

/* Okay, variable already exists, go based on the type of variable */
	switch (type) {
		case GV_REAL:
		case GV_REAL_LINK:
		case GV_DOUBLE:
		case GV_DOUBLE_LINK:
		case GV_INT:
		case GV_INT_LINK:
		case GV_INT_ARRAY:
		case GV_INT_ARRAY_LINK:
		case GV_COMPLEX:
		case GV_COMPLEX_LINK:
		case GV_ARRAY:
		case GV_ARRAY_LINK:
		case GV_COMPLEX_ARRAY:
		case GV_COMPLEX_ARRAY_LINK:
		case GV_FILEPTR:
			LexInsText(varname);
			rcode = do_let();
			break;

		case GV_SURFACE:								/* Could be setting a value, or the IDS */
			if (! simplevar) {						/* Form is s1[7,3] and becomes a let value */
				LexInsText(varname);
				rcode = do_let();
			} else {
				if (! LexGetMathP(value, sizeof(value), "Surface identification string: ")) return(TRUE);
				if (*value == '\0') return(TRUE);
				rcode = GVSetValue(varname, value);
			}
			break;
			
		case GV_STRING:
		case GV_STRING_LINK:
		case GV_2DCURVE:
		case GV_3DCURVE:
			if (! LexGetMathP(value, sizeof(value), "Expression: ")) return(TRUE);
			if (*value == '\0') return(TRUE);
			rcode = GVSetValue(varname, value);
			break;

		case GV_FUNCTION:
			if (! LexGetMath(value, sizeof(value))) LexPromptStr(value, sizeof(value), "Definition: ");
			if (*value == '\0') return(TRUE);
			rcode = GVAllocFnc(varname, GVF_USER, value);
			break;

		case GV_FUNCTION_LINK:
			ERRprintf("ERROR: Function %s exists as linked function - cannot simply set\n", nameonly);
			return(FALSE);

		default:
			ERRprintf("ERROR: Variable %s exists, but don't recognize type to do set (%d)\n", nameonly, type);
			return(FALSE);

	}
	return(rcode);
}


/*------------------------------------------------------------
-- SETVAR - Allocate (if not present) and set a real variable
--  Routine to set specific variables to values
--
-- Usage:  BOOL FUNCTION LET$(key)
--
--     Inputs: key - 0 => Generate error if variable does not exist
--                   1 => Allocate variable if it does not exist
--
-- Output: Executes the assignment statement on command line
--
-- Example: LET PLOT:X = C1:X+C2:Y

-- LET    - Assign a variable already present to values 
-------------------------------------------------------------- */
static char SetvHelp[]
	= "SETVar allocates and sets the value of a variable.  It is the most\n"
	  "common (and simplest) way to create variables of several types.  In the\n"
	  "simplest terms, it is a combination of an ALLOCATE command (if needed)\n"
	  "and a LET command.\n"
	  "\n"
	  "Usage:\n"
	  "  setv [type_option] <varname>[,<var2>,...] <value1>[,<val2>,...]\n"
	  "\n"
	  "     <varname>[,<var2>,...]   - name(s) to be created\n"
     "     <value1>,[<val2>,...]    - list of values to assign to the variables\n"
	  "\n"
	  "The variable type may optionally be set with\n"
	  "      -float   | -real        - force a single precision real constant\n"
	  "      -double                 - force a double precision real constant\n"
     "      -complex | -imaginary   - force a complex constant\n"
	  "      -integer                - force an integer constant\n"
	  "      -fileptr                - force a FILE * pointer (only for fopen)\n"
     "For backward compatibility, the keywords real | double | complex | fileptr\n"
     "without the leading minus sign are also accepted.\n"
	  "\n"
	  "If the variable type is not explicitly forced, it will be created based on\n"
     "the apparent type of the expression.  The option is generally not used.\n"
	  "\n"
	  "Notes:\n"
	  "  (1) Without a type qualifier, SETV will force at least REAL type on the\n"
     "      variable.  If previously allocated as INT, the existing definition\n"
     "      will be deleted and replaced with a REAL.\n"
	  "\n"
	  "Examples:\n"
	  "   setv hbar = h/(2*pi)\n"
     "   setv cval = cos(7*pi+3j)\n"
     "   setv -complex cval = 7.0\n"
     "   setv funit = fopen(\"test.dat\", \"w\")\n";

PRIVATE BOOL do_setvar(void) {

	char token[DFLT_STR_SIZE], varname[DFLT_STR_SIZE], list[DFLT_STR_SIZE], *tokptr;
	char value[LONG_STR_SIZE];
	int  type;
	BOOL rcode, reuse, rc;
	COMPLEX czero={0.0f,0.0f};
	enum {DONTCARE=0, COMPLEXTYPE=1, REALTYPE=2, DOUBLETYPE=3, INTTYPE=4, FILEPTRTYPE=5} AllocType;
	GVP_PARSEMODE guess;

/* Check for help request */
	if (LexCheckHelp("SETVar", SetvHelp, NULL)) return(TRUE);

/* Check for request to force allocate a REAL or COMPLEX over the default */
	AllocType = DONTCARE;
	if (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-complex", 8) || LexEqual(token, "-imaginary", 5)) {
			AllocType = COMPLEXTYPE;
		} else if (LexEqual(token, "-real", 5) || LexEqual(token, "-float", 6)) {
			AllocType = REALTYPE;
		} else if (LexEqual(token, "-double", 7)) {
			AllocType = DOUBLETYPE;
		} else if (LexEqual(token, "-fileptr", 5)) {
			AllocType = FILEPTRTYPE;
		} else if (LexEqual(token, "-integer", 4)) {
			AllocType = INTTYPE;
		} else {
			ERRprintf("ERROR: Unrecognized setvar option (%s)\n", token);
			return (FALSE);
		}
	} else if (LexGetToken(token, sizeof(token))) {
		if (stricmp(token, "complex") == 0) {
			AllocType = COMPLEXTYPE;
		} else if (stricmp(token, "real") == 0) {
			AllocType = REALTYPE;
		} else if (stricmp(token, "double") == 0) {
			AllocType = DOUBLETYPE;
		} else if (stricmp(token, "fileptr") == 0) {
			AllocType = FILEPTRTYPE;
		} else {
			LexBackup();
		}
	}

/* Get a list of variables to set */
	if (! LexGetListP(list, sizeof(list), "Variable(s): ")) return(TRUE);

	rcode  = TRUE;
	tokptr = list;
	while (LexParseLine(varname, sizeof(varname), tokptr, &tokptr)) {

		if (! LexGetMathP(value, sizeof(value), "Value: ")) return(rcode);
		CheckAndPush(value);											/* Strip one off */

		if (! GVIsNameValid(varname)) {
			ERRprintf("ERROR: %s is not a valid variable name.\n", varname);
			rcode = FALSE;
			continue;
		}

		guess  = GVGuessExprType(value);							/* Guess expression format */

		if (guess == GVP_FILEPTR && AllocType != DONTCARE && AllocType != FILEPTRTYPE) {
			ERRprintf("ERROR: Expression returns file pointer but override is numeric (%s)\n", varname);
			rcode = FALSE;
			continue;
		} else if (AllocType == FILEPTRTYPE && guess != GVP_FILEPTR) {
			ERRprintf("ERROR: Override is for file handle but expression does not return such (%s)\n", varname);
			rcode = FALSE;
			continue;
		}

/* Check if variable exists and must be overwritten */
		reuse = FALSE;													/* Can't reuse variable */
		if (GVGetAdrInfo(varname, &type, NULL, NULL)) {
			switch (type) {
				case GV_FILEPTR:
					reuse = (AllocType == DONTCARE) || (AllocType == FILEPTRTYPE);
					break;
				case GV_INT:
					reuse = (AllocType == INTTYPE);
					break;
				case GV_COMPLEX:
					reuse = (AllocType == DONTCARE) || (AllocType == COMPLEXTYPE);
					break;
				case GV_DOUBLE:
					reuse = (AllocType == DONTCARE) || (AllocType == DOUBLETYPE);
					break;
				case GV_REAL:
					reuse = (AllocType == DONTCARE) || (AllocType == REALTYPE);
					break;
				default:
					reuse = FALSE;
			}
			reuse = reuse && ((guess == GVP_FILEPTR) == (type == GV_FILEPTR));
			if (! reuse) ERRprintf("WARNING: Variable %s will be redefined as %s type\n", varname, (guess==GVP_FILEPTR) ? "file pointer" : "numeric");
		}

		if (! reuse) {
			if (AllocType == FILEPTRTYPE || guess == GVP_FILEPTR) {
				rc = GVAllocFilePtr(varname, GVF_USER);
			} else if (AllocType == INTTYPE) {
				rc = GVAllocInt(varname, GVF_USER, 0);
			} else if (AllocType == DOUBLETYPE) {
				rc = GVAllocDouble(varname, GVF_USER, 0.0);
			} else if (AllocType == COMPLEXTYPE || (AllocType == DONTCARE && (GVMathMode & MATHCOMPLEX)) ) {
				rc = GVAllocComplex(varname, GVF_USER, czero);
			} else {										/* DONTCARE or REALTYPE */
				rc = GVAllocReal(varname, GVF_USER, 0.0f);
			}
			if (! rc) {
				ERRprintf("ERROR: Bad variable name or insufficient memory (%s)\n" ,varname);
				rcode = FALSE;
				continue;
			}
		}

		if (! GVSetValue(varname, value)) rcode = FALSE;
	}
	return(rcode);
}


/*------------------------------------------------------------
-- LET - Assign a variable already present to values 
--
-- Usage:  BOOL FUNCTION do_let(void)
--
-- Output: Executes the assignment statement on command line
--
-- Example: LET PLOT:X = C1:X+C2:Y
-------------------------------------------------------------- */
static char LetHelp[]
	= "LET is used to change the value of a variable already defined.\n"
     "It handles almost all types of variables, but unlike SETV never\n"
     "creates new ones.  It is the safest form for a macro to do auto\n"
	  "variable checking.\n"
     "\n"
	  "Usage:\n"
	  "  let <varname>[,<var2>,...] [=] <value>[,<val2>,...]\n"
     "\n"
     "Multiple variables may be set with a single let.  If there are not\n"
     "enough values to match the variables, the user is prompted."
     "\n"
	  "There are special forms for dealing with arrays.\n"
     "    alloc p array 6\n"
     "    let p = 7                       /* Sets all values to 7         */\n"
	  "    let p[2] = 43                   /* Sets a single element        */\n"
	  "    let p() = 7 2 43 8 24 92        /* Sets all values individually */\n"
	  "    let p(1..3) = 2 43 8            /* Sets only elements 1,2,3     */\n"
     "\n"
	  "Examples:\n"
	  "   let a,b,c = 0 7 24\n"
     "   let a,b,c_str,d = 0,7,""a test"",24\n";

PRIVATE BOOL do_let(void) {
	char varname[DFLT_STR_SIZE], list[DFLT_STR_SIZE];
	char value_token[LONG_STR_SIZE], prompt[SHORT_STR_SIZE];
	char *tokptr=list, *aptr, *bptr, *cptr;
	int  i, type, length, ifirst, ilast, err;
	enum {SIMPLE, ARRAY, ARRAY_SUBSET} set_type;

/* Check for help request */
	if (LexCheckHelp("Let", LetHelp, NULL)) return(TRUE);

/* Get a list of variables to set */
	if (! LexGetListP(list, sizeof(list), "Variable(s): ")) return(TRUE);

	LexConvertMode = MATH;
	while (LexParseLine(varname, sizeof(varname), tokptr, &tokptr)) {

/* Are we trying to set an array? */
		ifirst = 0; ilast = INT_MAX-1;								/* ILAST+1 MUST BE VALID!!! */
		if ( (aptr = strstr(varname,"()")) != NULL) {			/* Form p() = */
			set_type = ARRAY;
			*aptr = '\0';
		} else if ( (bptr = strstr(varname, "..")) != NULL) {	/* Form p(7..9) = */
			set_type = ARRAY_SUBSET;
			if ( ((aptr = strchr(varname, '(')) == NULL) || (aptr > bptr)) {
				ERRprintf("ERROR: Let could not interpret %s\n", varname);
				return(FALSE);
			}
			*aptr = *bptr = '\0';										/* Truncate parts */
			err = 0;															/* No errors */
			cptr=aptr+1;													/* And after ( */
			if (*cptr != '\0') ifirst = (int) GVEvalExpr(cptr, &err);
			if (err != 0) {
				ERRprintf("ERROR: Unable to interpret %s as starting posn\n", cptr);
				return(FALSE);
			}
			bptr++; while (*bptr == '.') bptr++;					/* Find last one */
			if (*bptr != ')') {
				if ( (cptr = strrchr(bptr, ')')) == NULL) {
					ERRprintf("ERROR: Could not find ) in array spec of %s\n", varname);
					return(FALSE);
				}
				*cptr = '\0'; ilast = (int) GVEvalExpr(bptr, &err);
				if (err != 0) {
					ERRprintf("ERROR: Unable to interpret %s as ending posn\n", bptr);
					return(FALSE);
				}
			}
		} else {
			set_type = SIMPLE;
		}

/* Look up the name */
		if (! GVGetAdrInfo(varname, &type, NULL, &length)) {
			ERRprintf("ERROR: Variable %s does not exist - let aborts\n", varname);
			return(FALSE);
		}

/* Make sure we understand - if not array form, have only one to do */
		if (set_type != ARRAY && set_type != ARRAY_SUBSET) {
			length = 1;
		} else if (type != GV_ARRAY && type != GV_ARRAY_LINK && type != GV_INT_ARRAY && type != GV_INT_ARRAY_LINK) {
			ERRprintf("ERROR: Could not interpret %s() as an array - let aborts\n", varname);
			return(FALSE);
		}

/* Go through the array list (okay for simple vars since length set to 1 */
		for (i=ifirst; i<min(length,ilast+1); i++) {
			if (set_type == ARRAY || set_type == ARRAY_SUBSET) sprintf(aptr, "[%d]", i);
			if (! LexGetMath(value_token, sizeof(value_token))) {
				sprintf(prompt, "Value of %s: ", varname);
				if (! LexGetMathP(value_token, sizeof(value_token), prompt)) return(TRUE);
			}
			if (type!=GV_STRING && type!=GV_STRING_LINK) CheckAndPush(value_token);
			if (! GVSetValue(varname, value_token)) {
				if (SysChkBreak(FALSE)) {
					ERRprintf("ERROR: Set %s with expression %s aborted by %c\n", varname, value_token);
				} else {
					ERRprintf("ERROR: Failed to set %s with expression %s - let aborts\n", varname, value_token);
				}
				return(FALSE);
			}
		}
	}
	return(TRUE);
}

/* ---------------------------------------
--  do_mathmode - sets internal math evaluation configurations
--------------------------------------- */
static char MathModeHelp[]
	= "MathMode sets an internal set of flags that modify how some math operations are\n"
	  "handled internally.  These include:\n"
	  "     SAFETIES      - blocks some attempts to write beyond array limits\n"
	  "     WARNING LEVEL - level of annoyance with error/warning message\n"
     "     COMPLEX MODE  - whether calculations are forced to complex or real mode.\n"
     "     BRANCH CUT    - where the complex function branch cut is taken\n"
	  "     IN PLACE      - whether calculations can be done in place, or copy first\n"
	  "Most of these functions are for the advanced users, though old macros may require\n"
	  "safeties to be turned off to run properly.\n"
	  "\n"
	  "Usage:\n"
	  "  mathmode STatus                        - print current settings\n"
	  "  mathmode Warninglevel <n>              - sets warning noise level\n"
	  "           0 -> none    1 -> fatal    2 -> error    3 -> warning    4 -> info\n"
	  "  mathmode SAFEty [on | off]             - turns on/off array bound write safeties\n"
	  "  mathmode [Complex | Real | FORCEReal]  - default expression handling mode\n"
	  "  mathmode BRanch [+ | -]                - sets branch cut along +/- real axis\n"
	  "  mathmode [InPlace | NotInPlace]        - whether calculations are done in place\n"
	  "  mathmode [debug | nodebug]             - internal debugging usage\n"
	  "\n"
	  "Comments: (1) In REAL mode, expressions are assumed to be real unless there is an\n"
	  "              explicit complex argument or function.  In COMPLEX mode, all expressions\n"
	  "              are handled as complex allowing, for example, SQRT(-1).  ForceReal\n"
	  "              prohibits going to complex mode.\n"
	  "          (2) The safeties only block only some accesses, such as LET X[-1] = 7\n"
	  "          (3) Array calculations are normally evaluated in temporary memory with\n"
	  "              results copied to the final array.  Thus LET Y = Y[I]+Y[7] works.\n"
	  "              For huge arrays, INPLACE may save time and avoid swap thrashing.\n"
	  "\n"
	  "Default values are a REAL, BRANCH -, WARNINGLEVEL 4, SAFETY ON, and NOTINPLACE.\n";

PRIVATE BOOL do_mathmode(void) {
	char token[SHORT_STR_SIZE];
	int i;

/* Check for help request */
	if (LexCheckHelp("MathMode", MathModeHelp, NULL)) return(TRUE);

	if (! LexGetTokenP(token, sizeof(token),
		"Mathmode to set [?|safety|complex|real|branch|warning|[Not]InPlace|status]: ")) return(TRUE);

	if (LexEqual(token, "help", 4) || strcmp(token, "?") == 0 || strcmp(token, "-?") == 0) {
		TTYputs("  mathmode STatus\n"
				  "  mathmode [SAFEty on | off]\n"
			     "  mathmode [Complex | Real | FORCEReal]\n"
				  "  mathmode [Warninglevel <n>]\n"
				  "      0 - none   1 - fatal   2 - error   3 - warnings   4 - info\n"
				  "  mathmode [BRanch [+ | -] ]\n"
				  "  mathmode [InPlace | NotInPlace]\n"
			);
	} else if (LexEqual(token, "safety", 4) || LexEqual(token, "safeties", 4)) {
		GVMathMode = (GVMathMode & ~MATH_SAFETY_ON);
		if (LexOnOff(TRUE, "Turn math write safeties ON or off? ")) GVMathMode |= MATH_SAFETY_ON;
	} else if (LexEqual(token, "complex", 1)) {
		GVMathMode = (GVMathMode & ~MATHFORCEREAL) | MATHCOMPLEX;
	} else if (LexEqual(token, "forcereal", 6) || LexEqual(token, "realonly", 5)) {
		GVMathMode = (GVMathMode & ~MATHCOMPLEX) | MATHFORCEREAL ;
	} else if (LexEqual(token, "real", 1) || LexEqual(token, "default", 1)) {
		GVMathMode &= ~(MATHCOMPLEX | MATHFORCEREAL);
	} else if (LexEqual(token, "inplace", 3) || LexEqual(token, "In_Place", 4)) {
		GVMathMode |= MATH_INPLACE;
	} else if (LexEqual(token, "notinplace", 6) || LexEqual(token, "Not_In_Place", 8)) {
		GVMathMode &= ~MATH_INPLACE;
	} else if (LexEqual(token, "status", 2)) {
		TTYprintf(" Math write safeties are %s\n", (GVMathMode & MATH_SAFETY_ON) ? "on" : "off");
		if (GVMathMode & MATHCOMPLEX) {
			TTYprintf(" Default math operations are complex\n");
		} else if (GVMathMode & MATHFORCEREAL) {
			TTYprintf(" Default math operations are real only\n");
		} else {
			TTYprintf(" Default math operations are real, but complex if necessary\n");
		}
		TTYprintf(" Complex branch cut is along the %s real axis\n", (GVMathMode & MATH_BRANCH_PLUS) ? "positive" : "negative");
		TTYprintf(" Array assignments are done by %s\n",(GVMathMode & MATH_INPLACE) ? "immediate overwrite" : "copying after evaluation");
		TTYprintf(" Math fatal errors are %s\n", (GVMathMode & MATHFATAL)   ? "printed" : "not printed");
		TTYprintf(" Math errors are %s\n",       (GVMathMode & MATHERROR)   ? "printed" : "not printed");
		TTYprintf(" Math warnings are %s\n",     (GVMathMode & MATHWARN)    ? "printed" : "not printed");
		TTYprintf(" Math info warnings are %s\n",(GVMathMode & MATHINFO)    ? "printed" : "not printed");
	} else if (LexEqual(token, "debug", 5)) {
		GVMathMode |= MATHDEBUG;
	} else if (LexEqual(token, "nodebug", 5)) {
		GVMathMode &= ~MATHDEBUG;
	} else if (LexEqual(token, "warninglevel", 1) || LexEqual(token, "errorlevel", 3)) {
		i = LexGetInt(3, "Warning levels to be printed [0-4]: ");
		GVMathMode &= ~(MATHFATAL | MATHERROR | MATHWARN | MATHINFO);
		if (i > 0) GVMathMode |= MATHFATAL;
		if (i > 1) GVMathMode |= MATHERROR;
		if (i > 2) GVMathMode |= MATHWARN;
		if (i > 3) GVMathMode |= MATHINFO;
	} else if (LexEqual(token, "branch", 2)) {
		if (LexGetTokenP(token, sizeof(token),
			"Complex branch cut along [+|-|pos|neg] real axis (no change): ")) {
			if ( (i = LexSelect(token, "POSITIVE NEGATIVE + -")) <= 0) {
				ERRprintf("ERROR: Illegal branch cut (%s) specification ignored", token);
				return(FALSE);
			} else if ( i == 1 || i == 3) {
				GVMathMode |= MATH_BRANCH_PLUS;
			} else {
				GVMathMode &= ~MATH_BRANCH_PLUS;
			}
		}
	} else {
		ERRprintf("Unrecognized mathmode option specified: %s\n", token);
		return(FALSE);
	}
	return(TRUE);
}
	
/* ---------------------------------------
--  EVAL - Evaluates an expression
--------------------------------------- */
static char EvaluateHelp[]
	= "Evaluate returns the value of either a string or math expression.  The\n"
	  "expression is always parsed and evaluated, though the result may either\n"
	  "be printed or simply ignored (often useful with file manipulation).  The\n"
	  "two key functions are:\n"
	  "\n"
	  "Usage:\n"
	  "   EValuate  <name | expr> [,expr ...] - evaluate and print the result\n"
	  "   QEValuate <name | expr> [,expr ...] - evaluate but don't print the result\n"
	  "\n"
	  "The argument can be the name of a SURFACE, 2D or 3D CURVE, STRING ARRAY, or a\n"
	  "FILEPTR.  If one of these, EVAL prints out basic information.  However, almost\n"
	  "always the expression is evaluated as a real, complex or string value.\n"
	  "\n"
	  "This function also allows access to the programmed list of functions and a\n"
	  "minimal amount of information on each.  As this is from the actual code, the\n"
	  "list will be absolutely current and appropriate for the implementation.\n"
	  "  EVAL -names            ==> print only list of the function names\n"
	  "  EVAL -list             ==> prints a full list of internal functions\n"
/*	  "  EVAL -list [regexp]    ==> prints list of internal functions in group regexp\n" */
	  "  EVAL -detail           ==> prints list with usage information\n"
	  "  EVAL -detail [regexp]  ==> prints info on functions matching regexp\n"
	  "  EVAL -apropos [regexp] ==> any function with regexp in description or name\n"
	  "The regular expression can be a single name, a wild card function name, or the\n"
	  "name of a class of functions (STAT, STRING, etc.).\n"
	  "\n"
	  "Examples:\n"
	  "  eval cos(3*pi/4)\n"
	  "  eval @sum(x),@sum(y),@span(x),@span(y)\n"
	  "  eval -detail *tan*\n"
	  ;

PRIVATE BOOL do_eval_quiet(void) {
	return do_eval_all(0);
}

PRIVATE BOOL do_eval(void) {
	return do_eval_all(1);
}

/* mode = 0 ==> no print */
/* mode = 1 ==> print full */
/* mode = 2 ==> print minimal */
PRIVATE BOOL do_eval_all(int mode) {

	char tmpbuf[DFLT_STR_SIZE];
	char expr[LONG_STR_SIZE], list[LONG_STR_SIZE], *tokptr;
	TMPREAL rval;
	TMPCOMPLEX cval;
	int  err, rcode;
	GVCMDS *cmds;
	GVPARSEINFO info;
	BOOL UseComplex;

	int   type;										/* Type of variable (GVLink)	*/
	char  *aptr;
	FILE  *funit;
	void **varptr;
	CURVE *curve;
	SURFACE *surface;
	STRING_ARRAY *s_var;
	int i,j;

	#define	FMT2D	"%5i:  %14.7G  %14.7G\n"
	#define	FMT3D	"%5i:  %14.7G  %14.7G  %14.7G\n"
	#define	FMTSF	"  %14.7G"
	#define	FMTSS	"Value: %s = \"%s\"\n"

#if (defined TMPREAL_IS_LONG) && (DBL_DIG >= 13)
	#define	FMTC	"%.13LG%+.13LGj"
	#define	FMTS	"Value: %s = %s\n"
	#define	FMTS_B	" := %s\n"
	#define	FMTP	"Value: %p = %s\n"
	#define	FMT0	"Value: %40s = %s\n"
	#define	FMT0_B	" := %s\n"
	#define	FMT1	"Value: %20.13LG = %s\n"
	#define	FMT1_B	" := %-20.13LG\n"
	#define	FMT2	"%5i:  %14.7LG"
	#define	FMT3	"%14.7LG"
	#define	FMTA	"%.7LG%+.7LGj"
	#define	FMT4	"%5i:  %31s"
	#define	FMT5	" %31s"
#elif (defined TMPREAL_IS_LONG)
	#define	FMTC	"%.8LG%+.8LGj"
	#define	FMTS	"Value: %s = %s\n"
	#define	FMTS_B	" := %s\n"
	#define	FMTP	"Value: %p = %s\n"
	#define	FMT0	"Value: %30s = %s\n"
	#define	FMT0_B	" := %s\n"
	#define	FMT1	"Value: %15.8LG = %s\n"
	#define	FMT1_B	" := %-15.8LG\n"
	#define	FMT2	"%5i:  %14.7LG"
	#define	FMT3	"%14.7LG"
	#define	FMTA	"%.7LG%+.7LGj"
	#define	FMT4	"%5i:  %31s"
	#define	FMT5	" %31s"
#else
	#define	FMTC	"%.8G%+.8Gj"
	#define	FMTS	"Value: %s = %s\n"
	#define	FMTS_B	" := %s\n"
	#define	FMTP	"Value: %p = %s\n"
	#define	FMT0	"Value: %30s = %s\n"
	#define	FMT0_B	" := %s\n"
	#define	FMT1	"Value: %15.8G = %s\n"
	#define	FMT1_B	" := %-15.8G\n"
	#define	FMT2	"%5i:  %14.7G"
	#define	FMT3	"%14.7G"
	#define	FMTA	"%.7G%+.7Gj"
	#define	FMT4	"%5i:  %31s"
	#define	FMT5	" %31s"
#endif

/* Check for help request */
	if (LexCheckHelp("Evaluate", EvaluateHelp, NULL)) return(TRUE);

	rcode = TRUE;											/* If no errors, return TRUE */

	LexConvertMode = MATH;								/* Get list in MATH mode */
	if (! LexGetListP(list, sizeof(list), "Expression(s): ")) return(TRUE);

	tokptr = list;
	while (TRUE) {
		LexConvertMode = MATH;							/* Switch to MATH mode */
		if (! LexParseLine(expr, sizeof(expr), tokptr, &tokptr)) break;
		if (*expr == '\0') continue;
		if (stricmp(expr, "-names") == 0) {
			GVPrintFncList(-1, NULL);
			continue;
		} else if (stricmp(expr, "-list") == 0) {				/* Print list of known functions */
			GVPrintFncList(0, LexGetToken(expr, sizeof(expr)) ? expr : NULL);
			continue;
		} else if (stricmp(expr, "-detail") == 0) {	/* Print list and detail of known functions */
			GVPrintFncList(1, LexGetToken(expr, sizeof(expr)) ? expr : NULL);
			continue;
		} else if (stricmp(expr, "-apropos") == 0) {	/* Print list and detail of known functions */
			if (! LexGetTokenP(expr, sizeof(expr), "Text: ")) strcpy(expr, "*");
			GVPrintFncList(2, expr);
			continue;
		}

/* If given the name of a curve or surface, just print it out */
		if (GVGetInfo(expr, &type, (void **) &varptr)) {
			switch (type) {
				case GV_SURFACE:
					if (mode == 0) continue;
					surface = (SURFACE *) (*varptr);
					TTYprintf("               ");
					for (j=0; j<surface->ncol; j++) {
						if (j!=0 && j%4==0) TTYprintf("\n               ");
						TTYprintf(FMTSF, surface->x[j]);
					}
					TTYprintf("\n");

					for (i=0; i<surface->nrow; i++) {
						TTYprintf("%14.7g:", surface->y[i]);
						for (j=0; j<surface->ncol; j++) {
							if (j!=0 && j%4==0) TTYprintf("\n               ");
							TTYprintf(FMTSF, surface->z[i+j*surface->nrow]);
						}
						TTYprintf("\n");
						if (SysChkBreak(TRUE)) break;
					}
					continue;

				case GV_2DCURVE:
				case GV_3DCURVE:
					if (mode == 0) continue;
					curve = (CURVE *) (*varptr);
					for (i=0; i<curve->npt; i++) {
						if (type == GV_2DCURVE) {
							TTYprintf(FMT2D, i, curve->x[i], curve->y[i]);
						} else {
							TTYprintf(FMT3D, i, curve->x[i], curve->y[i], curve->z[i]);
						}
						if (SysChkBreak(TRUE)) break;
					}
					continue;

				case GV_STRING_ARRAY:
				case GV_STRING_ARRAY_LINK:
					if (mode == 0) continue;
					s_var = (STRING_ARRAY *) (*varptr);
/*					TTYprintf("String array: %s   Size = %d   Maxsize = %d\n", expr, *s_var->size, s_var->maxsize); */
					for (i=0; i<*s_var->size; i++) TTYprintf("  [%2.2d] %s\n", i, s_var->sval[i]);
					continue;
					
				case GV_FILEPTR:
					if (mode == 0) continue;
					funit = *((FILE **) varptr);
					TTYprintf("%s is a file handle: %p (eof=%d) (error=%d)\n", expr, funit, (funit == NULL) ? EOF : feof(funit), (funit == NULL) ? EOF : ferror(funit));
					continue;

				default:
					break;
			}
		}

/* Otherwise, try as a general expression */
		switch (GVGuessExprType(expr)) {
			case GVP_STRING:
				if ( (cmds=GVParseEx(expr,&info,GVP_STRING)) == NULL) {
					rcode = FALSE;
					TTYprintf("Unable to parse expression: %s\n", expr);
				} else {
					aptr = GVEvalStrCmds(cmds, &err);
					if (mode == 1) {
						TTYprintf(FMTSS, expr, aptr);
					} else if (mode == 2) {
						TTYprintf(FMTS_B, aptr);
					}
					free(aptr);
				}
				break;

			case GVP_FILEPTR:
				funit = GVEvalPtrExpr(expr, &err, GVP_FILEPTR);
				if (err != 0) {
					rcode = FALSE;
					TTYprintf("Unable to parse expression: %s\n", expr);
				} else if (mode != 0) {
					TTYprintf((mode == 1) ? FMTP : "%p\n", funit, expr);
					if (funit != NULL) ERRprintf(
						"MSG: Okay - so now you have a file pointer.  What do you plan to\n"
						"     do next.  You can't write to it.  You can't read from it.\n"
						"     Heck, you can't even close it.  A brilliant move - NOT!\n");
				}
				break;

			case GVP_NUMERIC:
			case GVP_UNKNOWN:
				if ( (cmds=GVParse(expr,&info)) == NULL) {
					rcode = FALSE;
					TTYprintf("Unable to parse expression: %s\n", expr);
					break;
				}
				UseComplex = (info.type & GV_INFO_COMPLEX_REFERENCE) || (GVMathMode & MATHCOMPLEX);
				if (info.length <= 1) {
					if (UseComplex) {								/* Complex reference */
						cval = GVEvalComplexCmds(cmds, &err);
						if (err == 0 && mode != 0) {
							if (cval.y != 0) {
								sprintf(tmpbuf, FMTC, cval.x, cval.y);
								TTYprintf((mode == 1) ? FMT0 : FMT0_B, tmpbuf, expr);
							} else {
								TTYprintf((mode == 1) ? FMT1 : FMT1_B, cval.x, expr);
							}
						}
					} else {
						rval = GVEvalCmds(cmds, &err);
						if (err == 0 && mode != 0) TTYprintf((mode == 1) ? FMT1 : FMT1_B, rval, expr);
					}
					if (err != 0) {
						rcode = FALSE;
						TTYprintf("Unable to evaluate expression: %s\n", expr);
					}
				} else {
					INT i=0,j;
					char tmptok[40], outtoken[256];
					while (i < info.length) {
						if (SysChkBreak(TRUE)) break;
						if (UseComplex) {
							cval = GVEvalComplexCmdsI(cmds, i, &err);
							sprintf(tmpbuf, FMTA, cval.x, cval.y);
							sprintf(outtoken, FMT4, i, tmpbuf);
							for (j=1; j; j--) {
								if (++i >= info.length) break;
								cval = GVEvalComplexCmdsI(cmds, i, &err);
								sprintf(tmpbuf, FMTA, cval.x, cval.y);
								sprintf(tmptok, FMT5, tmpbuf);
								strcat(outtoken, tmptok);
							}
						} else {
							rval = GVEvalCmdsI(cmds, i, &err);
							sprintf(outtoken, FMT2, i, rval);
							for (j=4; j; j--) {
								if (++i >= info.length) break;
								rval = GVEvalCmdsI(cmds, i, &err);
								sprintf(tmptok, FMT3, rval);
								strcat(outtoken, tmptok);
							}
						}
						if (mode != 0) TTYputsnl(outtoken);
						++i;
					}
				}
				break;

			default:
				ERRprintf("Expression appears to be a format I don't understand (%s)\n", expr);
		}
	}

	return(rcode);
}

/* -------------------------------------
-- LISTV - list a subset of variables
------------------------------------- */
static char LISTVARS_Help[] 
	= "The LISTVars command lists the variables currently defined in the function evaluator\n"
	  "\n"
	  "   LISTVars [-options]\n"
	  "\n"
	  "Type options - include only variables of the designated type(s)\n"
	  "   -STrings | -TExt   ==> string variables\n"
	  "   -FUnctions         ==> function definitions\n"
	  "   -ARRays            ==> array defitions (real, complex, integer)\n"
     "   -VARs              ==> real, integer or complex single variables\n"
	  "   -INteger           ==> integer single variables\n"
	  "   -REals             ==> real single variables\n"
	  "   -DOubles           ==> double precision single variables\n"
	  "   -COmplex           ==> complex single variables\n"
	  "   -CUrves            ==> 2D and 3D curves\n"
	  "   -SUrfaces          ==> Surfaces\n"
	  "   -MOdules           ==> linked function modules\n"
	  "   -FIles             ==> File handle variables\n"
	  "Modified options\n"
	  "   -HIDden            ==> Include also hidden variables\n"
     "   -DEBUG | -DETAIL   ==> Include debug information on variables\n"
     "                          (only partially implemented as needed)\n"
     "\n"
	  "Single letter designators\n"
	  "   -[acdfhimprstvz]   ==> [ a => arrays     c => curves   d => doubles\n"
     "                            f => functions  h => hidden   i => integers\n"
     "                            m => modules    p => fileptr  r => real\n"
     "                            s => surface and string       t => string\n"
     "                            v => vars       z => complex]\n"
     "\n"
	  "Default is to list all but hidden variables.\n"
	  "\n"
	  "Example:\n"
	  "   listv -f   ==> list functions\n"
	  ;

PRIVATE BOOL do_listvar(void) {
	int	mytypes=0,									/* Show nothing */
			myflags=GVF_HIDDEN,						/* And don't show hidden */
			myopts= 0;
	char token[SHORT_STR_SIZE], *aptr;

	if (LexCheckHelp("LISTVARS commands", LISTVARS_Help, NULL)) return(TRUE);

	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-hidden", 4)) {
			myflags &= ~GVF_HIDDEN;
		} else if (LexEqual(token, "-debug",      6) ||
					  LexEqual(token, "-detail",     7)) {
			myopts |= 0x01;
		} else if (LexEqual(token, "-strings",		3) ||
					  LexEqual(token, "-text",       3)) {
			mytypes |= GV_STRING;
		} else if (LexEqual(token, "-functions",	3)) {
			mytypes |= GV_FUNCTION;
		} else if (LexEqual(token, "-arrays",		4)) {
			mytypes |= GV_ARRAY | GV_COMPLEX_ARRAY | GV_INT_ARRAY | GV_DOUBLE_ARRAY;
		} else if (LexEqual(token, "-vars",			4)) {
			mytypes |= (GV_REAL | GV_INT | GV_COMPLEX);
		} else if (LexEqual(token, "-integer",		3)) {
			mytypes |= GV_INT;
		} else if (LexEqual(token, "-reals",		3)) {
			mytypes |= GV_REAL;
		} else if (LexEqual(token, "-doubles",		3)) {
			mytypes |= GV_REAL;
		} else if (LexEqual(token, "-complex",		3)) {
			mytypes |= GV_COMPLEX;
		} else if (LexEqual(token, "-curves",		3)) {
			mytypes |= (GV_2DCURVE | GV_3DCURVE);
		} else if (LexEqual(token, "-surfaces",   3)) {
			mytypes |= GV_SURFACE;
		} else if (LexEqual(token, "-modules",		3)) {
			mytypes |= GV_FUNCTION_LINK;
		} else if (LexEqual(token, "-files",      3)) {
			mytypes |= GV_FILEPTR;
		} else if (strspn(token, "-acdfhimprstvz") == strlen(token)) {
			aptr = token+1;
			while (*aptr != '\0') switch (*aptr++) {
				case 'a':	mytypes |= GV_ARRAY | GV_COMPLEX_ARRAY | GV_INT_ARRAY | GV_DOUBLE_ARRAY; break;
				case 'c':	mytypes |= (GV_2DCURVE | GV_3DCURVE);			break;
				case 'd':	mytypes |= GV_REAL;									break;
				case 'f':	mytypes |= GV_FUNCTION;								break;
				case 'h':	myflags &= ~GVF_HIDDEN;								break;
				case 'i':	mytypes |= GV_INT;									break;
				case 'm':	mytypes |= GV_FUNCTION_LINK;						break;
				case 'p':	mytypes |= GV_FILEPTR;								break;
				case 'r':	mytypes |= GV_REAL;									break;
				case 's':	mytypes |= (GV_SURFACE | GV_STRING);			break;
				case 't':	mytypes |= GV_STRING;								break;
				case 'v':	mytypes |= (GV_REAL | GV_INT | GV_COMPLEX);	break;
				case 'z':	mytypes |= GV_COMPLEX;								break;
			}
		} else {
			ERRprintf("ERROR: Invalid option to listvar (%s)\n", token);
		}
	}

	if (mytypes == 0) mytypes = 0xFFFFFFFF;			/* If none specified, do all */
	GVListVars(mytypes, myflags, myopts);
	return(TRUE);
}


/* ----------------------------------------
-- SAVEVAR - Save all variables to file
---------------------------------------- */
PRIVATE BOOL do_savevar(void) {

	char filename[PATH_MAX]="";
	FILE *handle;
	int mytypes=0,								/* Show anything			 */
		 myflags=GVF_HIDDEN;					/* And don't show hidden */
	char token[SHORT_STR_SIZE], *aptr;
	int i;

	for (i=0; i<2; i++) {					/* Make two passes */
		while (LexGetOption(token, sizeof(token))) {
			if (LexEqual(token, "-hidden", 4)) {
				myflags &= ~GVF_HIDDEN;
			} else if (LexEqual(token, "-strings",		3) ||
						  LexEqual(token, "-text",       3)) {
				mytypes |= GV_STRING;
			} else if (LexEqual(token, "-functions",	3)) {
				mytypes |= GV_FUNCTION;
			} else if (LexEqual(token, "-arrays",		4)) {
				mytypes |= GV_ARRAY | GV_COMPLEX_ARRAY | GV_INT_ARRAY | GV_DOUBLE_ARRAY;
			} else if (LexEqual(token, "-vars",			4)) {
				mytypes |= (GV_REAL | GV_INT | GV_COMPLEX);
			} else if (LexEqual(token, "-integer",		3)) {
				mytypes |= GV_INT;
			} else if (LexEqual(token, "-reals",		3)) {
				mytypes |= GV_REAL;
			} else if (LexEqual(token, "-doubles",		3)) {
				mytypes |= GV_DOUBLE;
			} else if (LexEqual(token, "-complex",		3)) {
				mytypes |= GV_COMPLEX;
			} else if (LexEqual(token, "-curves",		3)) {
				mytypes |= (GV_2DCURVE | GV_3DCURVE);
			} else if (LexEqual(token, "-modules",		3)) {
				mytypes |= GV_FUNCTION_LINK;
			} else if (LexEqual(token, "-surfaces",   3)) {
				mytypes |= GV_SURFACE;
			} else if (strspn(token, "-acfhimrstvz") == strlen(token)) {
				aptr = token+1;
				while (*aptr != '\0') switch (*aptr++) {
					case 'a':	mytypes |= GV_ARRAY | GV_COMPLEX_ARRAY | GV_INT_ARRAY | GV_DOUBLE_ARRAY; break;
					case 'c':	mytypes |= (GV_2DCURVE | GV_3DCURVE);			break;
					case 'd':	mytypes |= GV_REAL;									break;
					case 'f':	mytypes |= GV_FUNCTION;								break;
					case 'h':	myflags &= ~GVF_HIDDEN;								break;
					case 'i':	mytypes |= GV_INT;									break;
					case 'm':	mytypes |= GV_FUNCTION_LINK;						break;
					case 'r':	mytypes |= GV_REAL;									break;
					case 's':	mytypes |= GV_SURFACE;								break;
					case 't':	mytypes |= GV_STRING;								break;
					case 'v':	mytypes |= (GV_REAL | GV_INT | GV_COMPLEX);	break;
					case 'z':	mytypes |= GV_COMPLEX;								break;
				}
			} else {
				ERRprintf("ERROR: Illegal option (%s)\n",token);
			}
		}
		if (*filename == '\0')
			if (! LexGetFileP(filename, sizeof(filename), "Filename: "))
				return(TRUE);
	}
	if (mytypes == 0) mytypes = 0xFFFFFFFF;			/* If none specified, do all */

	if ( (handle = fopen(filename, "w")) == NULL) {
		ERRprintf("ERROR: %s failed to open\n", filename);
		return(FALSE);
	}

	GVWriteVars(handle, mytypes, myflags);
	fclose(handle);
	return(TRUE);
}


/* ----------------------------------------                                     
-- TIMER process.  Allow timing of operations.
---------------------------------------- */
PRIVATE BOOL do_timer(void) {

	static clock_t split_time=0;
	clock_t current_time;
	time_t tod;
	char token[OPTION_STR_SIZE];

	current_time = clock();								/* Get current time */
	time(&tod);												/* Get current time */

	if (LexGetOption(token, sizeof(token))) {
		if (! (LexEqual(token, "-START", 3) || LexEqual(token, "-RESET", 4))) {
			ERRprintf("ERROR: Unrecognized option (%s)\n", token);
			return(FALSE);
		}
		split_time = current_time;
	} else {
		TTYprintf("Timer: %10.3f  %s\n", 
			(current_time-split_time)/((float) CLOCKS_PER_SEC),
			asctime(localtime(&tod)) );
	}
	return(TRUE);
}

/* -----------------------------------------------------
-- QUERY - Prompt for and set a string variable
----------------------------------------------------- */
PRIVATE BOOL do_query(void) {

	char prompt[DFLT_STR_SIZE], varname[VARNAME_STR_SIZE],
		  value[LONG_STR_SIZE], junk[2];
	int  oldtype;
	BOOL rcode;

	if (! LexGetToken(prompt, sizeof(prompt))) return(TRUE);
	UserInput(prompt, value, sizeof(value));
	if (*value == '\0')									/* Nothing given, use dflt */
		LexGetToken(value, sizeof(value));
	else														/* Strip from cmd line */
		LexGetToken(junk, sizeof(junk));	
	if (! LexGetToken(varname, sizeof(varname))) {
		ERRputs("ERROR: No variable specified in QUERY\n");
		return(FALSE);
	}
	if (! GVGetInfo(varname, &oldtype, NULL)) {
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else if (oldtype != GV_STRING) {
		ERRprintf(VarDefChanged);
		rcode = GVAllocString(varname, GVF_USER, 0);
	} else {
		rcode = TRUE;
	}
	
	if (rcode) rcode = GVSetValue(varname, value);
	if (! rcode) ERRprintf("ERROR: Bad name or insufficient memory (%s)\n", varname);
	return(rcode);
}
	
/* ===========================================================================
==============================================================================
==============================================================================
=========================================================================== */
/* ----------------------------------------
--     FOR command processing
---------------------------------------- */
PRIVATE BOOL do_for(void) {

	char token[LONG_STR_SIZE], *listptr, *aptr;
	char subst;

	if (! LexGetToken(token, sizeof(token))) goto BadFormat;
	if (*token == '%' && strlen(token) == 2) {
		subst = tolower(token[1]);							/* Normally %f */
		if (! LexGetToken(token, sizeof(token))) goto BadFormat;
		if (stricmp(token, "in") != 0) goto BadFormat;
		LexGetRestNT(token, sizeof(token));				/* Get all of it */
		aptr = token;
		while (isspace(*aptr)) aptr++;
		if (*(aptr++) != '(') goto BadFormat;
		listptr = aptr;
		while (*aptr && *aptr != ')') aptr++;
		if (*aptr != ')') goto BadFormat;
		*(aptr++) = '\0';										/* End of the list */
		while (isspace(*aptr)) aptr++;
		if (strnicmp(aptr, "do", 2) != 0) goto BadFormat;
		aptr += 2;
		if (! isspace(*(aptr++))) goto BadFormat;
		while (isspace(*aptr)) aptr++;
		if (*aptr == '\0') goto BadFormat;
		LexFMMake(listptr, subst, aptr);
		return(TRUE);
	} else if (LexEqual(token, "repeat", 3)) {		/* Repeat processing only */
		return(do_repeat());
	}

BadFormat:
	ERRprintf("ERROR: Invalid form of the FOR command -- get with the game, man!\n"
		       "       Obscure syntax: \"FOR %%f IN (*.asm a.f77) DO some %%C and %%F\"\n");
	return(FALSE);
}

/* ----------------------------------------
--     REPEAT command processing
---------------------------------------- */
PRIVATE BOOL do_repeat(void) {
	int count;
	char token[LONG_STR_SIZE];

	if ( (count=LexGetInt(0, "Number of times to repeat (abort): ")) <= 0)
		return(TRUE);
	if (LexChkToken(token, sizeof(token))) {
		if (stricmp(token,"times") == 0 || stricmp(token,"do") == 0) 
			LexGetToken(token, sizeof(token));
	}
	LexGetRestNT(token, sizeof(token));
	LexFMCount(count, token);
	return(TRUE);
}

/* ----------------------------------------
--     REPEAT command processing
---------------------------------------- */
extern BOOL SysInsertMode;					/* Unpublished private extern */

PRIVATE BOOL do_insertmode(void) {
	SysInsertMode = TRUE;
	return(TRUE);
}

PRIVATE BOOL do_replacemode(void) {
	SysInsertMode = FALSE;
	return(TRUE);
}


/* ============================================================================
-- Routine to handle solving an equation
--
-- SOLVE <fnc> function
--
-- Usage: BOOL do_solve(void)
--
-- Inputs: none (all used locally)
--
-- Output: Success of finding root
============================================================================ */
#define	DEFAULT_MAXITER		100
#define	DEFAULT_EPSILON		4*REAL_EPSILON

static char SolveHelp[] = 
" Solve finds the root of an arbitrary equation given two bounding\n"
" limits.  It may be used to fill in an array under suitable conditions.\n"
"\n"
" Usage: solve <function> [ for <var> ] <lower_bound> <upper_bound>\n"
"                         [ -SILent | -Quiet ]\n"
"                         [ -ITERate <max_iterations> ]\n"
"                         [ -EPSilon <frac_eps_to_quit> ]\n"
"                         [ -GUEss   <initial_guess> ]\n"
"                         [ -NOERRors\n"
"                         [ -RESult  <array_or_real_var> ]\n"
"\n"
" Unless specified, assumes \"for x\", -ITER 100, -EPS 4E-7\n"
"\n"
" If a -RESult variable is specified, solve is repeated with the root\n"
" placed in the variable/array each time.  Other variables are assumed\n"
" to be arrays whose values are changing.  The bounds must be valid for\n"
" all conditions.  The -noerrors options allows automatic run without\n"
" aborting when no root can be found in the specified interval.  Returned\n"
" value in such cases is in the center of the specified interval.\n"
" See example below for difficult way to do y = sqrt(x).\n"
"\n"
" Examples: solve cos(x)-0.3 0 pi/2\n"
"           create y = x -range 0 5 -points 200\n"
"           solve y**2-x for y 0 3 -result y\n"
"           create y = sqrt(25-x**2) -range 0 5\n"
"           solve y*tan(theta)-x for theta -1.57 1.57 -result y\n";

PRIVATE BOOL do_solve(void) {

	BOOL tellinfo = TRUE;								/* Do we print the result?		*/
	static double result=0.0;
	int  i, iter, rc;
	char arg[VARNAME_STR_SIZE],						/* Argument to solve for		*/
		  fnc[LONG_STR_SIZE],							/* Function to solve				*/
		  token[DFLT_STR_SIZE];							/* Random text token				*/
	SOLVE_PARMS parms;									/* Parameters to GVSolve		*/
	int   varlen = 1, vartype;							/* Request result type/len		*/
	REAL  *varptr = NULL;								/* Request result address		*/
	BOOL ReportErrors;									/* Should we report errors		*/

	GVLinkDouble("RESULT$", GVF_USER, &result);		/* Link result first	*/
	
/* Check for help */
	if (LexCheckHelp("Solve", SolveHelp, NULL)) return(TRUE);
	
/* Get the function */
	if (! LexGetMathP(fnc, sizeof(fnc), "Function: (abort) ")) return(TRUE);

/* ... Check for an optional "for <var>" or "-for <var>" format */
	strcpy(arg, "x");										/* Search variable */
	if (LexChkToken(token, sizeof(token))) {
		if (stricmp(token, "for") == 0 || stricmp(token, "-for") == 0) {
			LexGetToken(token, sizeof(token));
			if (! LexGetTokenP(arg, sizeof(arg), "Variable to solve for? (abort) "))
				return(TRUE);
			if (strpbrk(arg, "+-*=/!^()[]{}<>.,") != NULL) {
				ERRprintf("ERROR: Invalid variable specified - go ahead and try again later, okay? (%s)\n", arg);
				return(FALSE);
			}
		}
	}

/* Create a "function of one variable" $va(x) consistent with the solve routine */
	sprintf(token, "$va(%s)", arg);
	if (! GVAllocFnc(token, GVF_USER, fnc)) {
		ERRprintf("ERROR: Function or argument invalid [fnc: %s] [arg: %s]", fnc, arg);
		return(FALSE);
	}

	parms.LowerBound = LexGetReal(0.0f, "Lower bound: (0) ");
	parms.UpperBound = LexGetReal(1.0f, "Upper bound: (1) ");
	parms.Guess      = 0;
	parms.epsilon    = DEFAULT_EPSILON;
	parms.MaxIterate = DEFAULT_MAXITER;

/* Now, parse for options */
	ReportErrors = TRUE;
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-silent", 4) || LexEqual(token, "-quiet", 2)) {
			tellinfo = FALSE;
		} else if (LexEqual(token, "-noerrors", 6)) {
			ReportErrors = FALSE;
		} else if (LexEqual(token, "-iterate", 5)) {
			parms.MaxIterate = LexGetInt(DEFAULT_MAXITER, "Maximum number of iterations (default): ");
		} else if (LexEqual(token, "-epsilon", 4)) {
			parms.epsilon = LexGetReal(DEFAULT_EPSILON, "<epsilon> quit criteria (default): ");
		} else if (LexEqual(token, "-guess", 4)) {
			parms.Guess = LexGetReal(0.0f, "Initial guess for root (0): ");
		} else if (LexEqual(token, "-result", 4)) {
			if (! LexGetTokenP(token, sizeof(token), "Variable or array for result(s): ")) return(TRUE);
			if (LexEscape(TRUE)) return(TRUE);
			if (! GVGetAdrInfo(token, &vartype, (void **) &varptr, &varlen)) {
				ERRprintf("ERROR: Variable does not exist (%s)\n", token);
				return(FALSE);
			} else if (vartype != GV_ARRAY && vartype != GV_REAL) {
				ERRprintf("ERROR: Specified name is neither a variable nor an array (%s)\n", token);
				return(FALSE);
			}
		} else {
			ERRprintf("ERROR: Illegal option to SOLVE function (%s)\n", token);
			return(FALSE);
		}
	}

	if ( (parms.cmds = GVParse("$va(RESULT$)", NULL)) == NULL) {
		ERRprintf("ERROR: Expression would not parse (SOLVE)\n");
		return(FALSE);
	}

	rc = 0;
	for (i=0; i<varlen; i++) {
		rc = GVSolve(&result, i, &parms, &iter);
		if (varptr != NULL) varptr[i] = (REAL) result;
		parms.Guess = result;
		if ((rc == -1 || rc == -3) && ! ReportErrors) continue;
		if (rc != 0) break;
	}
	if (rc == -1 && ReportErrors) ERRputs("ERROR: Root not bracketed by specified range (SOLVE)\n");
	if (rc == -2)                 ERRputs("ERROR: Function could not be evaluated (SOLVE)\n");
	if (rc == -3 && ReportErrors) ERRputs("ERROR: No solution found in maximum iterations (SOLVE)\n");

	if (rc == 0 && varlen == 1 && tellinfo)
		TTYprintf("Root found in %i iterations at %s = %.7g\n", iter, arg, result);

	GVDeallocate("$va");								/* And the function			 */
	return(rc==0 || ! ReportErrors);
}


/* ============================================================================
-- Routine to modify priority of execution thread
--
-- SOLVE <fnc> function
--
-- Usage: BOOL do_solve(void)
--
-- Inputs: none (all used locally)
--
-- Output: Success of finding root
============================================================================ */
static char PriorityHelp[] = 
" Modifies execution priority of current thread in an OS dependent manner.\n"
"\n"
" Usage: SETPRiority <boost>\n"
"\n"
" Boost defaults to zero, reseting normal priority.  On NT, values of\n"
" -2, -1, 0, +1, and +2 are allowed.  Result undefined on UNIX and OS/2.\n";									  

PRIVATE BOOL do_setpriority(void) {

	int boost;
#ifdef NT
	static int levels[5] = {THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST};
#endif

/* Check for help */
	if (LexCheckHelp("Setpriority", PriorityHelp, NULL)) return(TRUE);

	boost = LexGetInt(0, "Priority boost (0): ");
	if (LexEscape(TRUE)) return(TRUE);

#ifdef NT
	if (boost < -2) boost = -2;
	if (boost > +2) boost = 2;
	boost = levels[boost+2];
	SetThreadPriority(GetCurrentThread(), boost);
#endif

	return(TRUE);
}


/* ============================================================================
-- Routine to set a registry entry string value (only string values)
--
-- SOLVE <fnc> function
--
-- Usage: BOOL do_solve(void)
--
-- Inputs: none (all used locally)
--
-- Output: Success of finding root
============================================================================ */
#ifdef NT
static char SetRegistryHelp[] = 
										 " Sets a string value into the registry for NT.\n"
										 "\n"
										 " Usage: SET_Registry <key> <value>\n"
										 "\n"
										 " Key must be in HKEY_LOCAL_MACHINE with the fully qualified name and value.\n"
										 " The final component on the path is taken as the key and it's value will be set\n"
										 " as a REG_SZ string given by the value.\n";

PRIVATE BOOL do_setregistry(void) {

	char dir[DFLT_STR_SIZE],							/* Directory		*/
		  *key,												/* And the key		*/
		  value[DFLT_STR_SIZE];							/* And the value	*/
	char *aptr;
	HKEY hkey;
	BOOL rc;

/* Check for help */
	if (LexCheckHelp("Set_Registry", SetRegistryHelp, NULL)) return(TRUE);

	if (! LexGetTokenP(dir, sizeof(dir), "Registry key (from HKEY_LOCAL_MACHINE): "))
		return(TRUE);
	if (! LexGetStrExpr(value, sizeof(value))) LexPromptStr(value, sizeof(value), "Value: ");

/* Separate the directory and key values */
	key = dir;
	while ( (aptr = strchr(key, '\\')) != NULL) key = aptr+1;
	if (key == dir) {
		ERRprintf("ERROR: Not a valid registry entry request\n");
		return(FALSE);
	}
	*(key-1) = '\0';						/* Terminate the directory */
	
	rc = TRUE;
	if (RegCreateKey(HKEY_LOCAL_MACHINE, dir, &hkey) != ERROR_SUCCESS) {
		ERRprintf("ERROR: Failed to open/create the registry directory (%s)\n", dir);
		rc = FALSE;
	} else {
		if (! (rc = RegSetValueEx(hkey, key, 0, REG_SZ, value, (int) strlen(value)+1) == ERROR_SUCCESS)) 
			ERRprintf("ERROR: Failed to set registry.  dir=%s  key=%s  value=%s\n", dir, key, value);
		RegCloseKey(hkey);
	}
	return(rc);
}
#endif
