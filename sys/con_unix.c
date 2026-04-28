/* UNIX routines for console reads */

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

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
   #define CONflush() fflush(stdout)
#endif

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	FAKE_CTRL_C	0x1F

typedef struct termios TERMIOS;

/* THIS IS A REAL SCREWUP WITH MANY GNU IMPLEMENTATIONS! */
#if (defined GNU_C)
	#define	FFLUSHALL	(fflush(stdout),fflush(stderr),fflush(stdin))
#else
	#define	FFLUSHALL	fflush(NULL)
#endif

#ifndef OS2
	#define	HAS_CTRL_Z			/* Define if ^Z is caught as SIGTSTP/SIGCONT */
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void my_SIGINT(int sig);			/* My signal handler SIG_INTERRUPT	*/
static void my_exit(void);					/* Routine at exit */
#ifdef HAS_CTRL_Z
static void my_SIGTSTP(int sig);			/* My signal handler SIG_STOP			*/
static void my_SIGCONT(int sig);			/* My signal handler SIG_CONTINUE	*/
#endif

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
#ifdef HAS_CTRL_Z
static void (*old_SIGTSTP)(int sig) = NULL;
static void (*old_SIGCONT)(int sig) = NULL;
#endif

static TERMIOS *InitialTerminalConfig = NULL;	/* Terminal configs */
static TERMIOS *MyTerminalConfig      = NULL;

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
-- Actions: 1) save initial setting of terminal as InitialTerminalConfig
--          2) modifies and installs MyTerminalConfig as characteristics
--          3) registers atexit function to un-do at program exit
--          4) installs interrupt handlers for SIGTSTP (^Z)
--          5) changes stdin/stdout to be un-buffered for command editing
--          6) look up the terminal type in termkey file
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

	char *tty;
	int  rcode;

	if (ConIsInitialized) return;					/* No need to repeat */
	ConIsInitialized = TRUE;

/* Install immediately the SIGINT (control C) handler */
	if ( (old_SIGINT = signal(SIGINT, my_SIGINT)) == SIG_ERR)
		fputs("WARNING: Break Handler could not be installed\n", stderr);

/* Determine if console really connected to a TTY - behavior depends */
	ConIsPipe = ! isatty(fileno(stdin));
   if (ConIsPipe) return;							/* Nothing more if not a tty */

/* Allocate space for TERMIOS structures, get initial values, and copy */
	MyTerminalConfig      = malloc(sizeof(TERMIOS));
	InitialTerminalConfig = malloc(sizeof(TERMIOS));
	if (tcgetattr(fileno(stdin), InitialTerminalConfig) != 0) {
		fputs("Unable to get initial terminal settings.\n", stderr);
		free(InitialTerminalConfig);
		free(MyTerminalConfig);
		MyTerminalConfig = InitialTerminalConfig = NULL;
		return;
	}

/* Copy terminal settings & turn off input processing. See POSIX 7.1.1.9ff*/
	memcpy(MyTerminalConfig, InitialTerminalConfig, sizeof(TERMIOS));
	MyTerminalConfig->c_lflag &= ~(ICANON | ECHO);	/* byte-by-byte, no echo */
	MyTerminalConfig->c_cc[VMIN]  = 1;					/* 1 character at a time */
	MyTerminalConfig->c_cc[VTIME] = 0;					/* Wait forever for one	 */

/* Install atexit() routine so all this is undone on exit */
   if (atexit(my_exit) != 0)
      fputs("WARNING: Unable to atexit() terminal restore function.  Strange\n"
            "         terminal settings may remain after exit.\n", stderr);

/* And install SIGSTP handler so ^Z can temporarily disable my stuff */
#ifdef HAS_CTRL_Z
	if ( (old_SIGTSTP = signal(SIGTSTP, my_SIGTSTP)) == SIG_ERR)
      fputs("WARNING: Unable to register ^Z handlers.  Strange terminal\n"
            "         settings may exist if suspended by ^Z.\n", stderr);
#endif

/* Reopen stdin and stdout so can set to unbuffered input/output */
	if ( (tty = ttyname(fileno(stdin))) != NULL )  {
		if (freopen(tty, "r", stdin) == NULL)
			fputs("WARNING: Failed to reopen stdin for unbuffered input\n", stderr);
		if (setvbuf(stdin,  NULL, _IONBF, 0) != 0)
			fputs("WARNING: buffering may be active on stdin\n", stderr);
	}

#ifdef UNBUFFERED_OUTPUT
	if ( (tty = ttyname(fileno(stdout))) != NULL )  {
		if (freopen(tty, "w", stdout) == NULL)
			fputs("WARNING: Failed to reopen stdout for unbuffered output.\n", stderr);
		if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
			fputs("WARNING: buffering may be active on stdout.\n", stderr);
	}
#endif	/* UNBUFFERED_OUTPUT */

/* Look up terminal type and create keyboard table for Kgetkey() */
	rcode = Kgetent(NULL, &KeyboardTable); 
	if (rcode == -1) {
	  	fputs("WARNING: Unable to find termkey file. Ask your GENPLOT administrator\n"
				"         to copy it to search path. No special keys defined for now.\n", stderr);
		sleep(3);
	} else if (rcode == 0) {
	  	fputs("WARNING: Keyboard/terminal type not found in termkey file.  Ask your GENPLOT\n"
				"         administrator to add your terminal type.  Assuming VT100 for now.\n", stderr);
		if (Kgetent("vt100", &KeyboardTable) == 0) 
		  	fputs("ERROR: Have him add the VT100 also while he is at it\n", stderr);
		sleep(3);
	}
	Kinit(KeyboardTable);								/* Initialize keyboard */

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

	FFLUSHALL;												/* Flush all buffers		*/	
	Kinit(KeyboardTable);								/* Re-initialize keyboard */
	if (MyTerminalConfig == NULL) return;			/* Nothing to replace?	*/
	if (tcsetattr(fileno(stdin), TCSAFLUSH, MyTerminalConfig) != 0)
		fputs("WARNING: Unable to reset terminal settings for unbuffered input.\n"
			   "         Behavior will likely be a bit strange.\n", stderr);
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

	FFLUSHALL;												/* Flush all buffers		*/	
	if (InitialTerminalConfig == NULL) return;	/* Nothing to replace?	*/
	if (tcsetattr(fileno(stdin), TCSAFLUSH, InitialTerminalConfig) != 0)
		fputs("WARNING: Unable to restore original terminal settings on suspend.\n"
			   "         Definite weirdness may result.\n", stderr);
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
int CONgetc(void) {
   CONflush();
	return(Kgetkey(KeyboardTable));
}

/* and note that Kgetkey() calls CONgetcRaw() */

int CONgetcRaw(void) {

   unsigned char achr;
   int rcode;

	if (ConIsPipe) {								/* Simple pipe so no work */
		rcode = fgetc(stdin);

	} else if (CONungetchar != 0) {			/* Pending CONungetc() character */
		rcode = CONungetchar;
		CONungetchar = 0;

	} else {
		while ( (rcode = read(fileno(stdin), &achr, 1)) != 1) {
			if (rcode == 0) {						/* Read from a closed handle */
				rcode = EOF;
				break;
			} else if (LocalBreakSeen) {		/* Control C interrupted process */
				rcode = FAKE_CTRL_C;
				break;
			}
		}
		if (rcode == 1) rcode = achr;
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
#define	CON_CHKCHR_TIMEOUT	20			
int CONchkchr(void) {
	return(CONwaitchr(CON_CHKCHR_TIMEOUT));
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
int CONwaitchr(int msecs) {

	unsigned char achr;
	int hold, rcode;

/* Default if no timeout */
	if (msecs == 0) return CONgetc();

	if (ConIsPipe) {
		rcode = fgetc(stdin);

	} else if (CONungetchar != 0) {
		rcode = CONungetchar;
		CONungetchar = 0;

	} else {

/* Set terminal session characteristics.  See POSIX 7.1.1.9ff*/
		if (msecs < 100) msecs = 100;

		hold = MyTerminalConfig->c_cc[VTIME];				/* Save value		  	*/
		MyTerminalConfig->c_cc[VTIME] = msecs/100;		/* Set wait time		*/
		MyTerminalConfig->c_cc[VMIN]  = 0;					/* No required chars */
		tcsetattr(fileno(stdin), TCSANOW, MyTerminalConfig);

		if (read(fileno(stdin), &achr, 1) == 1) {
			rcode = achr;
		} else if (LocalBreakSeen) {
			rcode = FAKE_CTRL_C;
		} else {
			rcode = -1;
		}
		MyTerminalConfig->c_cc[VTIME] = hold;				/* Restore value		*/
		MyTerminalConfig->c_cc[VMIN]  = 1;   				/* Restore value		*/
		tcsetattr(fileno(stdin), TCSANOW, MyTerminalConfig);
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

	if (LocalBreakSeen) {							/* Second time, abort		 */
		signal(SIGINT, old_SIGINT);				/* By restoring old handler */
		raise(sig);										/* And reraising the signal */
		abort();
	}

	for (i=0; i<MAX_BREAK_NOTIFY; i++) {
		if (BreakNotifyList[i] != NULL) *BreakNotifyList[i] += 1;
	}
	LocalBreakSeen = TRUE;

	REsignal(SIGINT, my_SIGINT);						/* Reset handler again */
	return;
}


/* ===========================================================================
-- Interrupt handler for SIGTSTP (^Z)
=========================================================================== */
#ifdef HAS_CTRL_Z
static void my_SIGTSTP(int sig) {

	CONSuspend();											/* Disable our terminal mods */
	if ( (old_SIGCONT = signal(SIGCONT, my_SIGCONT)) == SIG_ERR)
		fputs("WARNING: Will be unable to trap return from ^Z\n", stderr);
	signal(SIGINT, old_SIGINT);						/* Disable our ^C handling */
	signal(SIGTSTP, old_SIGTSTP);
	raise(sig);
	return;
}

/* ===========================================================================
-- Interrupt handler for SIGCONT (return from ^Z)
=========================================================================== */
static void my_SIGCONT(int sig) {
  	
	CONRestart();											/* Enable our terminal mods */
	if ( (old_SIGTSTP = signal(SIGTSTP, my_SIGTSTP)) == SIG_ERR)
		fputs("WARNING: Unable to retrap ^Z for next suspend\n", stderr);
	signal(SIGINT,  my_SIGINT);					/* Enable our ^C handling */
	signal(SIGCONT, old_SIGCONT);
	raise(sig);
	return;
}
#endif

/* ===========================================================================
-- Exiting routine.  Reset terminal and remove handlers
=========================================================================== */
static void my_exit(void) {
	
	if (ConIsInitialized) {
		CONSuspend();
#ifndef TESTING
		Kfree(&KeyboardTable);						/* Deallocate keyboard */
#endif
#ifdef HAS_CTRL_Z
		signal(SIGTSTP, old_SIGTSTP);
		signal(SIGINT,  old_SIGINT);
#endif
	}
	ConIsInitialized = FALSE;
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
		if ( (achr = CONgetcRaw()) == -1) {
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
