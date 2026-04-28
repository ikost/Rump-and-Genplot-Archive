/* OS/2 routines for console reads */

/* ===========================================================================
-- This program tests the POSIX terminal control capability of a UNIX
-- system.  GENPLOT/RUMP require the ability to turn off canonical input
-- processing, and to utilize the read timer capabilities.  Major work
-- will be required in the low_io.c routine if these capabilities fail
-- to exist.  Better go find a compliant OS or compiler instead.
-- 
-- First section will simply set the terminal into character by character
-- input mode.  If the characters are not echoed as typed, then the
-- test fails.  The second section tests the read-timer mode for input
-- control.  You should receive a message if no characters are typed
-- for 1 second, otherwise characters should be returned unmodified
--
-- Compiling:
--    OS/2     icc -Gm+ -DTESTING -DCSET2 -DOS2 con_os2.c
=========================================================================== */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#ifndef TESTING
	#include "preload.h"
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef OS2
	#define	INCL_DOS
	#define	INCL_DOSERRORS
	#include <os2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#if defined TESTING && (defined CSET2 || defined WATCOM || defined MSC70)
	#include <conio.h>
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#ifndef TESTING
	#include "mytypes.h"
	#include "extends.h"
#else
	#define	TRUE	1
	#define	FALSE	0
	#if defined(AIX_C) || (defined(DECalpha) && defined(OSF1_C)) || (defined(GNU_C) && defined(RISC6000))
		typedef sig_atomic_t					VOLATILE_SIG_ATOMIC_T;
	#else
		typedef volatile sig_atomic_t		VOLATILE_SIG_ATOMIC_T;
	#endif
	void  CONInitialize(void);
	void  CONSuspend(void);
	void  CONRestart(void);
	int   CONgetc(void);
	int	CONungetc(int achr);
	int   CONchkchr(void);
	int	CONwaitchr(int msecs);
	char *CONgets(char *string, int n);
	int	CONBreakNotify(VOLATILE_SIG_ATOMIC_T *flag, int action);
	void	CONBreakClear(void);
#endif

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	FAKE_CTRL_C	0x1F

typedef struct termios TERMIOS;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void my_SIGINT(int sig);			/* My signal handler SIG_INTERRUPT	*/

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
#if ! defined OS2 || defined GNU_C
/* --	Keyboard scanning routines -- */
		int	Kgetent(char *termtype, void **entry);
		int	Kfree(void **entry);
		void	Kinit(void *entry);
		int   Kgetkey(void *entry);
static void *KeyboardTable=NULL;
#endif

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static unsigned char CONungetchar = 0;

static int ConIsPipe = TRUE;
static int ConIsInitialized = FALSE;

static void (*old_SIGINT)(int sig)  = NULL;		/* Old signal handlers */

#define	MAX_BREAK_NOTIFY	5					/* Max # of semaphores handled */

static VOLATILE_SIG_ATOMIC_T *BreakNotifyList[MAX_BREAK_NOTIFY] = {
	NULL, NULL, NULL, NULL, NULL};
static VOLATILE_SIG_ATOMIC_T LocalBreakSeen=FALSE;


/* ============================================================================
-- The routines CONInitialize(), CONSuspend() and CONRestart() handle system
-- dependent initialization related to the Console driver.  These should set
-- the terminal for no echo and no buffering in order to allow command line
-- editing to work.  CONSuspend temporarily removes any processing and
-- CONRestart reinstalls the support.  Calls to system() are bracketed by
-- these routines.
--
-- Syntax: void CONInitialize(void);
--         void CONSuspend(void);
--         void CONRestart(void);
--
-- Actions: 1) none on OS/2 systems
--
-- Usage: void ConInitialize(void);
--
-- Inputs: none
--
-- Output: none
--
-- Return: none
--
-- Notes:  Routine should be called first to initialize routine.  Maintains 
--         internal flag so multiple calls are safe.
============================================================================ */
void CONInitialize(void) {

	if (ConIsInitialized) return;					/* No need to repeat */
	ConIsInitialized = TRUE;

/* Install immediately the SIGINT (control C) handler */
	if ( (old_SIGINT = signal(SIGINT, my_SIGINT)) == SIG_ERR)
		fputs("WARNING: Break Handler could not be installed\n", stderr);

/* Determine if console really connected to a TTY - behavior depends */
	ConIsPipe = ! isatty(fileno(stdin));
   if (ConIsPipe) return;							/* Nothing more if not a tty */

	CONRestart();										/* Okay, and start first time */
	return;
}


/* ===========================================================================
-- Restart CON processing from a temporary suspend.  Typically used internally
-- only to re-establish console processing after returning from a ^Z pause.
--
-- Usage: void ConRestart(void);
--
-- Inputs: none
--
-- Output: none
--
-- Return: none
=========================================================================== */
void CONRestart(void) {
	return;
}


/* ===========================================================================
-- Temporary suspend CON processing.  Typically used internally before 
-- shell to operating system so not trapped in GENPLOT processing.
--
-- Usage: void ConSuspend(void);
--
-- Inputs: none
--
-- Output: none
--
-- Return: none
=========================================================================== */
void CONSuspend(void) {
	return;
}


/* ============================================================================
-- CONgetc    - routine to retrieve next character from console.
-- CONungetc  - routine to push back a character for read again later.
-- CONchkchr  - non-blocking version of CONgetc.  Returns if nothing present.
-- CONwaitchr - partial blocking version.  Waits specified time before return.
-- CONgets    - routine to return a string of specified length or to first <nl>
--
-- Routines to get generalized key character from the keyboard.  A generalized
-- key includes extended codes such as arrows, insert, delete etc.  
-- Break processing is handled internal to these routines.  If a ^C is
-- pressed while in any one of these routines, the routine will return
-- immediately with a unique code FAKE_CTRL_C.
--
-- Syntax: int   CONgetc()
--         int   CONungetc(int achr);
--         int   CONchkchr();
--         int   CONwaitchr(int msecs);
--         char *CONgets(char *string, int n);
--
-- Output: CONgetc     returns next character code.
--         CONungetc  pushes one character back for subsequent read
--         CONchkchr   next character or -1 if none waiting
--         CONwaitchr  next character or -1 if none waiting
--         CONgets     returns next n-1 characters or up to first <nl> read.
--                     <nl> is stripped from returned string
--
-- Note: (1) Characters are not echoed to the screen
--       (2) Strange machines (PRIME!) must strip parity bit (if stupid)
--       (3) Newline pair CR/LF, if read, should be returned as '\n' single
============================================================================ */



/* ============================================================================
-- Routine to read a <nl> terminated string from the console.
--
-- Syntax: char *CONgets(char *str, int len);
--
-- Input:  str - pointer to a character buffer for results
--         len - maximum number of character that can be stored in str
--
-- Output: str - filled with incoming string
--
-- Returns: pointer to str
--
-- Notes: (1) Up to n-1 character, or until a <nl> is seen, will be read
--        (2) <nl> is stripped if it terminates the read
============================================================================ */
char *CONgets(char *str, int len) {
	int achr;
	while (len-- > 1) {								/* Must leave space for EOS */
		if ( ((achr=CONgetc()) == EOF) || (achr == '\n') ) break;
		*(str++) = (char) achr;
	}
	*str = '\0';
	return(str);
}


/* ============================================================================
-- Routine to read the next character from the console
--
-- Syntax: int = CONgetc(void);
--
-- Input:  none
--
-- Output: none
--
-- Returns: (1) next character from input stream
--          (2) EOF if stream closed
--          (3) FAKE_CTRL_C if ^C pressed
============================================================================ */

/* ---------------------------------------------------------------------------
-- Routine to translate OS/2 extended codes into pseudo-keys
--------------------------------------------------------------------------- */
static int translate(int code) {

#ifndef TESTING
	typedef struct _TRANSLATE {
		int scan;
		int virtual;
	} TRANSLATE;

	static TRANSLATE table[] = {
		{0x49, VIRTUAL_PAGEUP},			{0x51, VIRTUAL_PAGEDOWN},
		{0x4F, VIRTUAL_END},				{0x47, VIRTUAL_HOME},
		{0x52, VIRTUAL_INSERT},			{0x53, VIRTUAL_DELETE},
		{0x4B, VIRTUAL_LEFT},			{0x48, VIRTUAL_UP},
		{0x4D, VIRTUAL_RIGHT},			{0x50, VIRTUAL_DOWN},

		{0x84, VIRTUAL_CTRL_PAGEUP},	{0x76, VIRTUAL_CTRL_PAGEDOWN},
		{0x75, VIRTUAL_CTRL_END},		{0x77, VIRTUAL_CTRL_HOME},
		{0x92, VIRTUAL_CTRL_INSERT},	{0x93, VIRTUAL_CTRL_DELETE},
		{0x73, VIRTUAL_CTRL_LEFT},		{0x8D, VIRTUAL_CTRL_UP},
		{0x74, VIRTUAL_CTRL_RIGHT},	{0x91, VIRTUAL_CTRL_DOWN},

		{0x3B, VIRTUAL_F1},				{0x3C, VIRTUAL_F2},
		{0x3D, VIRTUAL_F3},				{0x3E, VIRTUAL_F4},
		{0x3F, VIRTUAL_F5},				{0x40, VIRTUAL_F6},
		{0x41, VIRTUAL_F7},				{0x42, VIRTUAL_F8},
		{0x43, VIRTUAL_F9},				{0x44, VIRTUAL_F10},
		{0, 0}
	};

	TRANSLATE *entry;

	for (entry=table; entry->scan!=0; entry++) {
		if (entry->scan == code) return(entry->virtual);
	}
#endif

	return(code | 0x100);				/* Just mark as extended */
}


int CONgetc(void) {

	int rcode;
	
	fflush(stdout);								/* Make sure stdout is clean first */

	if (ConIsPipe) {								/* Simple pipe so no work */
		rcode = fgetc(stdin);
		if (rcode == 0 || rcode == 0xE0) 	/* Extended code */
			rcode = translate(fgetc(stdin));

	} else if (CONungetchar != 0) {			/* Pending CONungetc() character */
		rcode = CONungetchar;
		CONungetchar = 0;

	} else {
		rcode = getch();
		if (rcode == 0 || rcode == 0xE0)		/* Extended code */
			rcode = translate(getch());
	}
	return(rcode);
}


/* ============================================================================
-- Routine to put back a character into the read stream.  One is guarenteed.
--
-- Syntax: int = CONungetc(int achr);
--
-- Input:  achr - character to be pushed back on read stack
--
-- Output: none
--
-- Returns: character is successful, EOF if not
--
-- Notes: Only one can be pushed, subsequent calls before a CONgetc fail
============================================================================ */
int CONungetc(int achr) {
	if (CONungetchar != 0) return(EOF);
	CONungetchar = (unsigned char) achr;
	return(CONungetchar);
}


/* ============================================================================
-- Routine to read the next character from the console, non-blocking
--
-- Syntax: int = CONchkchr(void);
--
-- Input:  none
--
-- Output: none
--
-- Returns: -1 if no character present, otherwise same as CONgetc
--
-- Notes: Under UNIX, immediately is defined as within 20 milliseconds.
--        Calls CONwaitchr.  Under OS/2, operation is atomic.
============================================================================ */
int CONchkchr(void) {
	int rcode;

	if (CONungetchar != 0) {				/* Pending CONungetc() character */
		rcode = CONungetchar;
		CONungetchar = 0;
	} else if (ConIsPipe) {					/* Not sure I can do */
		rcode = -1;
	} else if (kbhit() == 0) {
		rcode = -1;
	} else {
		rcode = getch();
	}
	return(rcode);
}


/* ============================================================================
-- Routine to read next character from the console with finite time limit
--
-- Syntax: int = CONwaitchr(int msecs);
--
-- Input:  msecs - maximum time to wait for a character to become available
--                 If <=0, call equivalent to CONgetc()
--
-- Output: none
--
-- Returns: -1 if no character present, otherwise same as CONgetc
============================================================================ */
typedef struct _WAITINFO {
	int rcode;
	HEV sem;
} WAITINFO;

static void getme(void *ptr) {
	WAITINFO *wait;
	wait = (WAITINFO *) ptr;								/* Cast the pointer */
	wait->rcode = ConIsPipe ? fgetc(stdin) : getch();
	if (wait->rcode == 0 || wait->rcode == 0xE0)	{	/* Extended code */
		wait->rcode = ConIsPipe ? fgetc(stdin) : getch();
		wait->rcode = translate(wait->rcode);
	}
	DosPostEventSem(wait->sem);
	return;
}

int CONwaitchr(int msecs) {

	int rcode;
	WAITINFO info = {-1,0} ;
	TID tid;

	if (msecs == 0) {								/* Really equivalent to CONgetc() */
		rcode = CONgetc();
	} else {											/* Check if already waiting */
		rcode = CONchkchr();
		if (rcode == -1) {
			DosCreateEventSem(NULL, &info.sem, 0, FALSE);
			tid = _beginthread(getme, NULL, 8192, &info);
			if (DosWaitEventSem(info.sem, msecs) == ERROR_TIMEOUT)
				DosKillThread(tid);
			DosCloseEventSem(info.sem);
			rcode = info.rcode;
		}
	}
	return(rcode);
}


/* ===========================================================================
-- Routine to clear internal break flag and avoid second ^C crashing system.
--
-- Usage:  void CONBreakClear();
--
-- Inputs: none
--
-- Output: none
--
-- Return: none
--
-- Note: Clears internal flag used to indicate a break condition.
=========================================================================== */
void CONBreakClear(void) {
	LocalBreakSeen=FALSE;
	return;
}


/* ===========================================================================
-- Routine to add an address to the list notified when a ^C is caught from
-- keyboard.  Up to some maximum, routines can register a VOLATILE_SIG_ATOMIC_T
-- variable to be incremented on each ^C caught.
--
-- Usage:  int CONBreakNotify(VOLATILE_SIG_ATOMIC_T *flag, int action);
--
-- Inputs: flag   - address of the variable to be incremented on ^C detection
--         action - TRUE -> add to list, FALSE -> remove from list 
--
-- Output: none
--
-- Returns: 0 if successful, -1 if not in list or could not add.
--
-- Notes: This routine is closely linked to the mySIGINT() handler later.
=========================================================================== */
int CONBreakNotify(VOLATILE_SIG_ATOMIC_T *flag, int action) {

	int i;

	if (! ConIsInitialized) CONInitialize();	/* Make sure have ^C handler */

	for (i=0; i<MAX_BREAK_NOTIFY; i++) {		/* Check if already exists */
		if (BreakNotifyList[i] == flag) break;
	}

	if (action == FALSE) {
		if (i < MAX_BREAK_NOTIFY) {					/* Was in the list		*/
			BreakNotifyList[i] = NULL;					/* So now delete it		*/
			return(0);
		}
	} else {													/* Add requested			*/
		if (i < MAX_BREAK_NOTIFY) return(0);		/* Already in list		*/
		for (i=0; i<MAX_BREAK_NOTIFY; i++) {		/* Find an empty item	*/
			if (BreakNotifyList[i] == NULL) {
				BreakNotifyList[i] = flag;				/* Store this address	*/
				return(0);									/* And return ok			*/
			}
		}
	}

	return(-1);										/* No space left! */
}

/* ===========================================================================
------------------ SIGNAL HANDLERS ---------- SIGNAL HANDLES -----------------
==============================================================================
-- SIGINT  - this handler sets the local variable LocalBreakSeen if not already
--           set.  If set (indicating two SIGINTs), raises original handler.
-- SIGTSTP - On ^Z, disable our terminal modifications by calling CONSuspend()
--           and then just call old SIGTSTP.
-- SIGCONT - On return from ^Z, re-enable our terminal modifications by calling
--           CONRestart() and then call old SIGCONT.
=========================================================================== */
static void my_SIGINT(int sig) {

	int i;

#ifdef TESTING
   fputs("-> BreakHandler invoked <-\n", stderr);
#endif

#ifdef MSC60
	signal(SIGINT, SIG_ACK);						/* Acknowledging this one done */
#endif

	if (LocalBreakSeen) {							/* Second time, abort		 */
		signal(SIGINT, old_SIGINT);				/* By restoring old handler */
		raise(sig);										/* And reraising the signal */
		abort();
	}
#if defined MSC60 || defined MSC70
	ungetch(FAKE_CTRL_C);
	ungetch(FAKE_CTRL_C);
#elif defined CSET2 || defined WATCOM
/*	ungetch(FAKE_CTRL_C); */
#else
	ungetc(FAKE_CTRL_C, stdin);					/* Fake control C */
#endif

	for (i=0; i<MAX_BREAK_NOTIFY; i++) {
		if (BreakNotifyList[i] != NULL) *BreakNotifyList[i] += 1;
	}
	LocalBreakSeen = TRUE;

	signal(SIGINT, my_SIGINT);						/* Reset handler again */
	return;
}


#ifdef TESTING

/* ===========================================================================
-- Standalone routine for testing the console routines.  These can be
-- compiled by:
--    cc -o console -DAIX_C -DTESTING console.c
-- where AIX_C is replaced by the appropriate compiler name from makexxx.h.
--
-- Inputs: none
--
-- Output: testing operation from console
--
-- Return: nothing
=========================================================================== */
int main(void) {

	int achr;

	fputs(
		"\n"
		"This program tests the POSIX terminal control capability of a UNIX\n"
		"system.  GENPLOT/RUMP require the ability to turn off canonical input\n"
		"processing, and to use the read timer capabilities.  Major work\n"
		"will be required in the low_io.c routine if these capabilities fail.\n"
		"Better yet, get a Real OS [TM].\n"
		"\n", stdout);

	CONInitialize();

	fputs(
		"Terminal now configured for char by char.  Press any key and\n"
		"note if it is accepted immediately.  This tests fails if you must\n"
		"press <CR> before any characters are accepted.  Control-C should\n"
      "trigger two events: '-> BreakHandler invoked <-', and 'Received ^C',\n"
      "the latter showing proper return from read().\n"
		"\n"
		"Press the character z (lower case) to exit this test\n"
		"\n", stdout);

	do {
		if ( (achr = CONgetc()) == -1) {
			fprintf(stderr, "ERROR: Failed to get a character - big problem\n");
			return(0);
		}
      if (achr == FAKE_CTRL_C) {
			CONBreakClear();								/* Clear flag */
         printf("Received ^C\n");
		} else {
  		   printf("Received character 0x%.2x", achr);
         if (isprint(achr)) printf(" (%c)", achr);
         printf("\n");
		}
	} while (achr != 'z');


	fputs(
		"\n"
		"Terminal now configured for char by char timed mode.  Keys should be\n"
		"detected as before, but a timeout message printed if nothing received\n"
		"within 1 second of last character\n"
		"\n"
		"Press the character z (lower case) to exit this test\n\n", stdout);
	do {
		achr = CONwaitchr(1000);
		if (achr == -1) {
			printf("<timeout> recognized\n");
		} else {
         if (achr == FAKE_CTRL_C) {
				CONBreakClear();						/* Clear flag */
            printf("Received ^C\n");
		   } else {
  		      printf("Received character 0x%.2x", achr);
            if (isprint(achr)) printf(" (%c)", achr);
            printf("\n");
		   }
		}
	} while (achr != 'z');
	
	return(0);
}

#ifndef OS2
/* Dummy out the keyboard string handling routines */
	int	Kgetent(char *termtype, void **entry)	{return(1);}
	int	Kfree(void **entry)							{return(0);}
	void	Kinit(void *entry)							{return;}
	int   Kgetkey(void *entry)							{return(0);}
#endif

#endif


/* ===========================================================================
-- APPARENTLY SOME ATTEMPT TO USE PIPES TO COMMUNICATE WITH PACKAGE 
=========================================================================== */
#ifdef READ_INPUT_FROM_PIPE

int CONgetc(void) {

	static int ReadPipe=-1;
	int achr;
	
	if (ReadPipe == -1) {									/* Open read pipe */
		if ( (ReadPipe = open("/pipe/gptread", O_BINARY|O_RDONLY)) == -1) panic;
	}

	fflush(stdout);

	read(ReadPipe, &achr, sizeof(achr));					/* Get single character */
	return(achr);
}

#endif		/* READ_INPUT_FROM_PIPE */
