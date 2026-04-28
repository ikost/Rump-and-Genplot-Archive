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
#ifdef NT
	#include <windows.h>
#endif

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
-- Subroutine to perform common /HELP and /? help message responses.
--
-- Usage: BOOL LexCheckHelp(char *msg, void (*LongHelp)(void));
--
-- Input: msg - message to be printed via simple TTYputs().  If NULL, then
--              more detailed info will be handled via LongHelp
--        LongHelp - pointer to function to print a help message unique to
--                   the routine.  Only used if msg is NULL.
--         
-- Output: none
--
-- Return: TRUE if the request was for help.  FALSE otherwise.
--
-- Notes:  Grabs next token from the command line and checks to see if it
--         is either -HELP of -? form of help request.  If true, prints the
--         message and returns TRUE.  Otherwise, ungets the token and returns
--         FALSE.
--     (2) If both msg and LongHelp are NULL, this becomes just a query
--         as to if a help would have been processed.  TRUE is help,
--         FALSE that it wouldn't be a help.
============================================================================ */
#ifndef NT

BOOL LexCheckHelp(char *name, char *text, void (*LongHelp)(void)) {
	char token[OPTION_STR_SIZE];

	if (LexGetOption(token, sizeof(token))) {
		if (stricmp(token, "-help") == 0 || strcmp(token, "-?") == 0) {
			if (text != NULL) {
				TTYputs(text);
			} else if (LongHelp != NULL) {
				(*LongHelp)();
			} else {
				LexBackup();							/* This was a dummy query */
			}
			return(TRUE);
		}
		LexBackup();
	}
	return(FALSE);
}



#else

static void HelpMessageThread(void *mymsg);

typedef struct _MSGPASS {
	char *title;
	char *text;
} MSGPASS;


BOOL LexCheckHelp(char *name, char *text, void (*LongHelp)(void)) {
	char token[OPTION_STR_SIZE];
	MSGPASS *msg;

	if (LexGetOption(token, sizeof(token))) {
		if (stricmp(token, "-help") == 0 || strcmp(token, "-?") == 0) {
			if (text != NULL) {
				if (SysMainWindowHwnd != NULL) {
					msg = malloc(sizeof(MSGPASS));
					msg->title = name;
					msg->text  = text;
					_beginthread(HelpMessageThread, 4096, (void *) msg);
				} else {
					TTYputs(text);
				}
			} else if (LongHelp != NULL) {
				(*LongHelp)();
			} else {
				LexBackup();							/* This was a dummy query */
			}
			return(TRUE);
		}
		LexBackup();
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
-- Actual routine for printing out the text messgae
--------------------------------------------------------------------------- */
static void HelpMessageThread(void *mymsg) {

	MSGPASS *msg;

	msg = (MSGPASS *) mymsg;
	
	MessageBox(NULL, msg->text, msg->title, MB_ICONINFORMATION);
	return;
}



#endif


