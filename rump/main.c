/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1999-2010 (c) Computer Graphics Service ------------- */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/*  main.c -- initializes code and calls rump */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#if (defined OS2 && !defined NO_EA_MODE)	/* For setting extended attributes */
	#include <ea.h>
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"

#define	MINIMAL_RUMP_INCLUDE
#include "rump.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#ifndef MAIN
	#define	MAIN	main
#endif

typedef enum _FILETYPE {F_SPECTRUM, F_HCOPY, F_MACRO, F_NONE} FILETYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void copyright(int quiet);				/* Draw the copyright notice */
static int CheckEndian(void);
static FILETYPE CheckFileType(char *path, char *ext);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static jmp_buf signal_jmp_ret;					/* Return from exception */

#ifdef XVERS											/* For the XGENPLOT and XRUMP version */
#include <windows.h>

	static char *msgs[] = {
		"Enter any 11-digit prime number to continue ...",
		"Press any key to continue or any other key to quit.",
		"BREAKFAST.SYS halted... Cereal port not responding.",
		"Runtime Error 6D at 417A:32CF: Incompetent User.",
		"WinErr 16547: LPT1 not found. Use backup. (PENCIL & PAPER.SYS)",

		"Three things are certain:\n" "Death, taxes, and lost data.\n" "Guess which has occurred.\n",
		"Everything is gone;\n" "Your life's work has been destroyed.\n" "Squeeze trigger (yes/no)?",
		"Errors have occurred.\n" "We won't tell you where or why.\n" "Lazy programmers.\n",
		"Seeing my great fault\n" "Through darkening blue windows\n" "I begin again\n",
		"The code was willing,\n" "It considered your request,\n" "But the chips were weak.\n",
		"Chaos reigns within.\n" "Reflect, repent, and reboot.\n" "Order shall return.\n",
		"wind catches lily\n" "scatt'ring petals to the wind:\n" "segmentation fault\n",
		"ABORTED effort:\n" "Close all that you have.\n" "You ask way too much.\n",
		"First snow, then silence.\n" "This thousand dollar screen dies\n" "so beautifully.\n",
		"A crash reduces\n" "your expensive computer\n" "to a simple stone.\n",
		"There is a chasm\n" "of carbon and silicon\n" "the software can't bridge\n",
		"Yesterday it worked\n" "Today it is not working\n" "Windows is like that\n",
		"To have no errors\n" "Would be life without meaning\n" "No struggle, no joy\n",
		"No keyboard present\n" "Hit F1 to continue\n" "Zen engineering?\n",
		"Hal, open the file\n" "Hal, open the -?X?!!!- file, Hal\n" "open the, please Hal\n",
		"Out of memory.\n" "We wish to hold the whole sky,\n" "But we never will.\n",
		"Having been erased,\n" "The document you're seeking\n" "Must now be retyped.\n",
		"Serious error.\n" "All shortcuts have disappeared\n" "Screen. Mind. Both are blank.\n",
		"A file that big?\n" "It might be very useful.\n" "But now it is gone.\n",
		"Windows NT crashed.\n" "I am the Blue Screen of Death.\n" "No one hears your screams.\n",
		"Stay the patient course\n" "Of little worth is your ire\n" "The program is down\n",
		"You step in the stream,\n" "but the water has moved on.\n" "This page is not here.\n",
		"Rather than a beep\n" "Or a rude error message,\n" "These words: \"Nothing found.\"\n"

		"Managing senior programmers\n"	"is like herding cats.\n\n"	"Dave Platt\n",
		"There is no snooze button\n"	"on a cat who wants breakfast.\n\n"	"Unknown\n",
		"Thousands of years ago, cats were worshipped as gods.\n"	"Cats have never forgotten this.\n\n"	"Anonymous\n",
		"Cats are smarter than dogs.\n"	"You can't get eight cats to pull a sled through snow.\n\n"	"Jeff Valdez\n",
		"In a cat's eye,\n"	"all things belong to cats.\n\n"	"English proverb\n",
		"As every cat owner knows,\n"	"nobody owns a cat.\n\n"	"Ellen Perry Berkeley\n",
		"One cat just leads to another.\n\n"	"Ernest Hemingway\n",
		"Dogs come when they're called;\n"	"cats take a message and get back to you later.\n\n"	"Mary Bly\n",
		"Cats are rather delicate creatures and\n"	"they are subject to a good many ailments,\n"	"but I never heard of one who suffered from insomnia.\n\n"	"Joseph Wood Krutch\n",
		"People that hate cats,\n"	"will come back as mice in their next life.\n\n"	"Faith Resnick\n",
		"There are many intelligent species in the universe.\n"	"They are all owned by cats.\n\n"	"Anonymous\n",
		"I have studied many philosophers and many cats.\n"	"The wisdom of cats is infinitely superior.\n\n"	"Hippolyte Taine\n",
		"There are two means of refuge from the miseries of life:\n"	"music and cats.\n\n"	"Albert Schweitzer\n",
		"The cat has too much spirit\n"	"to have no heart.\n\n"	"Ernest Menaul\n",
		"Dogs believe they are human.\n"	"Cats believe they are God.\n\n"	"Anonymous\n",
		"Some people say that cats are sneaky, evil, and cruel.\n"	"True, and they have many other fine qualities as well.\n\n"	"Missy Dizick\n",
		"You will always be lucky\n"	"if you know how to make friends with strange cats.\n\n"	"Colonial American proverb\n",
		"Cats seem to go on the principle\n"	"that it never does any harm to ask for what you want.\n\n"	"Joseph Wood Krutch\n",
		"Cats aren't clean,\n"	"they're just covered with cat spit.\n\n"	"John S. Nichols\n"
	};

	#define	NUMMSGS	(sizeof(msgs) / sizeof(msgs[0]))
#endif


/* ===========================================================================
   ===========================================================================
                       SIGNAL HANDLER ROUTINES
   ===========================================================================
=========================================================================== */
#if defined __linux__

/* --------------------------------------------------
   __linux__ passes registers on stack in the order
   as gs,fs,es,ds, edi,esi, ebp,fp, ebx,edx,ecx,eax,
   trap_no, error_code, eip,cs, eflags, esp,ss
-------------------------------------------------- */
static volatile unsigned long state[20];
static const char regs[20][4]={"sig"," gs"," fs"," es"," ds","edi","esi","ebp",
 "*fp","ebx","edx","ecx","eax","trp","err","eip"," cs","efl","esp"," ss" };

static void SIGHANDLERTYPE Sig_Handler(int sig, ...) {
	va_list arg_ptr;
	int i;
	va_start(arg_ptr, sig);
	for (i=0; i<20; i++) state[i] = va_arg(arg_ptr, unsigned long);
	longjmp(signal_jmp_ret, sig);
}

#elif defined NT

static int FPE_ErrorType=0;
static void SIGHANDLERTYPE Sig_Handler(int sig, int type) {
	if (sig == SIGFPE) {
		_fpreset();							/* Reset coprocessor */
		FPE_ErrorType = type;
	}
	longjmp(signal_jmp_ret, sig);
}

#else   /* __linux__ */

/* --------------------------------------------------
   Others only give signal of error, nothing else!
-------------------------------------------------- */
static void SIGHANDLERTYPE Sig_Handler(int sig) {
#if defined NT || defined OS2
	if (sig == SIGFPE) _fpreset();				/* Reset coprocessor */
#endif
	longjmp(signal_jmp_ret, sig);
}

#endif   /* __linux__ */


static void PrintExceptionMessage(int rcode, LEXFLUSHMODE mode) {

	const struct {int signal; char *msg;} excepts[] = {
		{SIGFPE,	 "Floating point error"},	{SIGILL, "Illegal instruction"},
		{SIGSEGV, "Segment violation"},		{-999,	 "Unknown signal"}
	};

	int i=0;
	while (i<sizeof(excepts)/sizeof(excepts[0])-1 && rcode != excepts[i].signal) i++;

#ifdef NT
	ERRprintf("\n"
		"****************************************************************************\n"
		"      Execution Exception: %s (%i)\n"
		, excepts[i].msg, rcode);
	if (rcode == SIGFPE) {
		const struct {int signal; char *msg;} fpe_type[] = {
			{_FPE_INVALID,				"Invalid operation"},
			{_FPE_DENORMAL,			"Denormalized result"},
			{_FPE_ZERODIVIDE,			"Zero divide"},
			{_FPE_OVERFLOW,			"Overflow"},
			{_FPE_UNDERFLOW,			"Underflow"},
			{_FPE_INEXACT,				"Inexact result"},
			{_FPE_UNEMULATED,			"Unemulated operation"},
			{_FPE_SQRTNEG,				"Square root of negative number"},
			{_FPE_STACKOVERFLOW,		"Stack Overflow"},
			{_FPE_STACKUNDERFLOW,	"Stack Underflow"},
			{_FPE_EXPLICITGEN,		"Explicit SIGFPE signal"},
			{-999,						"Cannot be determined"}
		};
		i = 0;
		while (i<sizeof(fpe_type)/sizeof(fpe_type[0])-1 && FPE_ErrorType != fpe_type[i].signal) i++;
		ERRprintf("      Floating Point type: %s\n", fpe_type[i].msg);
	}
	ERRprintf(
		"****************************************************************************\n"
		"ERROR: An exception has occurred somewhere in the execution of the current  \n"
		"       command.  I cannot recover that command and continue with execution, \n"
		"       but I can simply restart at the GENPLOT main command.  Be aware      \n"
		"       that no cleanup has been performed, we simply jumped back to the     \n"
		"		  beginning of the code.  Good Luck.                                   \n"
		"****************************************************************************\n"
		"\n");
	#ifdef XVERS
	if (mode == FLUSH_QUERY) {
		i = (int) (NUMMSGS*((REAL) rand())/RAND_MAX);				/* Next index	*/
		MessageBox(NULL, msgs[i], "Zen Error Message", MB_ICONHAND | MB_OK);
	}
	#endif /* XVERS */


#elif ! defined __linux__
	ERRprintf("\n"
		"****************************************************************************\n"
		"      Execution Exception: %s (%i)\n"
		"****************************************************************************\n"
		"ERROR: An exception has occurred somewhere in the execution of the current  \n"
		"       command.  I cannot recover that command and continue with execution, \n"
		"       but I can simply restart at the GENPLOT main command.  Be aware      \n"
		"       that no cleanup has been performed, we simply jumped back to the     \n"
		"		  beginning of the code.  Good Luck.                                   \n"
		"****************************************************************************\n"
		"\n"
		,excepts[i].msg, rcode);
#else
      printf("\n >>> Execution exception: %s (%i) at eip=%8.8lx\n",
			excepts[i].msg, rcode, state[15]);
		for (i=0; i<20; i++) {
         printf("%s=%8.8lx ",regs[i],state[i]);
			if (i%4==3) printf("\n");
		}
		printf("<c>ontinue, <q>uit, <Q>uit ?"); fflush(stdout);
		if ( (i = getchar()) == 'q')  exit(1);
		if   (i ==              'Q') _exit(1);
#endif

	return;
}


/* ============================================================================
-- Shell program for calling RUMP.
--
-- Inputs: text and command line arguments
--
-- Output: none
--
-- Does not really like a return to this routine.  Forces return with NASTY
-- message.
============================================================================ */
#define	TOKEN_SIZE	1024
#define	USAGE_TEXT	"RUMP [-Inifile <file>] [-Atomic <file>] [-Quiet] [-Debug <n>] [-Help]"

int MAIN(int argc, char *argv[]) {

	char token[TOKEN_SIZE], *aptr;				/* Command line vars		*/
	char tmpstr[PATH_MAX];							/* Temporary string		*/
	int  quiet=0, rcode;								/* Copyright verbose?	*/
	char inifile[PATH_MAX]="";						/* Initialization file	*/

/* Handling of Zen traps (processor faults) */
	int		 Sig_Num_Traps  = 0;					/* Number of sig traps		*/
	LEXFLUSHMODE Sig_Reply_Mode=FLUSH_QUERY;	/* How to ask on responses	*/

	CONInitialize();									/* Initialize everything first */
	ScrClearAttrib(D_NORMAL);						/* Clearing the screen also */
	LexInitialize();									/* Including LEXP */
	PlotInitialize();
	srand( (unsigned)time( NULL ) );				/* Randomize the random number generator */

	if (CheckEndian()  != 0) abort();

/* .. Check for a valid serial number module .. */
	if (! SysCheckLicense("RUMP")) abort();

/* .. Process command line options */
	aptr = token; *aptr = '\0';				/* Mark EOF first */
	while (--argc != 0) {						/* Push arguments on stack */
		argv++;
		if (aptr != token) *(aptr++) = ' ';
		if (strchr(*argv, ' ') == NULL) {	/* Deal w/ possible blanks in args */
			strcpy(aptr, *argv); 
			aptr += strlen(aptr);
		} else {										/* This is with blanks, use "'s */
			*aptr++ = '"';
			strcpy(aptr, *argv);
			aptr += strlen(aptr);
			*aptr++ = '"';
		}
	}
	*aptr = '\0';									/* Terminate the string */
	LexInsText(token);
	if ( (aptr=getenv("RUMP")) == NULL) aptr = getenv("rump");
	if (aptr != NULL) LexInsText(aptr);
	
	while (LexGetToken(token, TOKEN_SIZE)) {
		if (*token != '-' && *token != '/') {
			LexBackup();
			break;
		}
		aptr = token+1;
		if (LexEqual(aptr, "help", 1) || strcmp(aptr,"?")==0) {
			TTYprintf("Usage: " USAGE_TEXT "\n");
			return(EXIT_SUCCESS);
		} else if (LexEqual(aptr, "debug", 1)) {
			SysDebugFlag = LexGetInt(0, "Debug flag (bit pattern): ");
			continue;
		} else if (LexEqual(aptr, "quiet", 1)) {
			quiet = 1;
			continue;
		} else if (LexEqual(aptr, "inifile", 1)) {
			LexGetFileP(inifile, sizeof(inifile), "Initialization file: ");
			continue;
		} else if (LexEqual(aptr, "atomic", 1)) {
			LexGetFileP(RbsAtomicDataFile, sizeof(RbsAtomicDataFile), "Atomic data file: ");
			continue;
		} else if (*token == '-') {
			ERRprintf("\n",
				"ERROR: %s is an invalid option for RUMP invocation.\n\n"
				"\n"
				"       " USAGE_TEXT "\n"
				"\n"
				" Press <CR> to exit and return to command level ... ", token);
			TTYflush();
			CONgets(token, TOKEN_SIZE);
			TTYputc('\n');
			return(EXIT_FAILURE);
		} else {
			LexBackup();									/* Treat as possible filename */
			break;
		}
	}

/* Look at the next token and deal with if a valid filename */
	if (LexGetFile(token, TOKEN_SIZE)) {			/* Treat rest */
		if (access(token, R_OK) != 0) {				/* If not, just leave on stack */
			LexBackup();
		} else {												/* Look at last extension */
			char dir[PATH_MAX],fname[PATH_MAX],ext[PATH_MAX],fullname[PATH_MAX];

			SysSplitPath(token, dir, fname, ext);	/* Find the extension */
			SysMakePath(fullname, NULL, fname, ext);

			switch (CheckFileType(token, ext)) {
				case F_SPECTRUM:
					sprintf(tmpstr, "plot '%s'", fullname);
					break;
				case F_HCOPY:
					sprintf(tmpstr, "dev -check hc plot '%s'", fullname);
					break;
				default:
					sprintf(tmpstr, "xeq '%s'", fullname);
					break;
			}
			LexInsText(tmpstr);							/* Actual command */
			if (*dir != '\0') {							/* CD to the directory also */
				sprintf(tmpstr, "dcd '%s'", dir);
				LexInsText(tmpstr);
			}
		}
	}

/* ... Print welcome screens */
	copyright(quiet);							/* Draw the copyright notice */
	if (quiet == 0) {
	  ScrClearAttrib(D_NORMAL);
	} else {
	  TTYputs("\n");
	}
	TTYputs("RUMP Shell for NT, OS/2 & Posix  [Rev. 1.02 12/28/98 (c) CGS]\n");

/* ... Link critical variables for user use */
	GVLinkInt("$Zen_Traps", GVF_INTERNAL | GVF_HIDDEN, &Sig_Num_Traps);
	GVLinkInt("$Zen_Reply", GVF_INTERNAL | GVF_HIDDEN, (INT *) &Sig_Reply_Mode);

/* ... Start up the initialization file if configured */
	if (*inifile == '\0') {
		if ( (aptr=getenv("RUMP.INI")) == NULL) aptr = getenv("rump.ini");
		if (aptr == NULL) aptr = "rump.ini";
		strcpy(inifile, aptr);
	}
	SysFindFile(inifile, inifile, RbsGetSearchPath(), NULL, R_OK);
	if (access(inifile, R_OK) == 0) {
		if (LexExecFile(inifile)) {
			LexSetLocalNoEcho(TRUE);				/*  Turn echo off for it */
			TTYprintf("Executing initialization file: %s\n",inifile);
		}
	}

/* Signal trapping - SIGINT trapped by posix\sys.c for keyboard control */
	if (! (SysDebugFlag & 0x0001)) {
		if (signal(SIGFPE,  (void (*)(int )) Sig_Handler)==SIG_ERR) ERRputs("WARNING: Unable to trap SIGFPE\n");
		if (signal(SIGILL,  (void (*)(int )) Sig_Handler)==SIG_ERR) ERRputs("WARNING: Unable to trap SIGILL\n");
		if (signal(SIGSEGV, (void (*)(int )) Sig_Handler)==SIG_ERR) ERRputs("WARNING: Unable to trap SIGSEGV\n");
	}

	if ( (rcode = setjmp(signal_jmp_ret) ) != 0) {
		Sig_Num_Traps++;										/* Identify that we have trapped */
		PrintExceptionMessage(rcode, Sig_Reply_Mode);
		REsignal(rcode, (void (*)(int )) Sig_Handler);		
/*		TTYprintf("MSG: Re-initializing signal handler for signal %d\n", rcode); */
		LexFlushEx(Sig_Reply_Mode);
	}

	while (TRUE) {
		rcode = Rump();
		if (rcode & 0xFF00) break;				/* If high byte set, quit */
		ERRputs("ERROR: QUIT must be used to exit RUMP, not RETURN\n");
	}
	return(rcode & 0xFF);
}

/* ===========================================================================
-- Draw the copyright notice
=========================================================================== */
static void copyright(int quiet) {

	if (quiet == 0) {
		ScrClearAttrib(D_NORMAL);
		TTYprintf(
"              |================================================|\n"
"              |    Rutherford Backscattering Data Analysis,    |\n"
"              |         Plotting and Simulation Package        |\n"
"              |================================================|\n"
"\n"
"               RRRRRRR     UU     UU    MMM     MMM    PPPPPPP\n"
"               RR    RR    UU     UU    M MM   MM M    PP    PP\n"
"               RR    RR    UU     UU    MM MM MM MM    PP    PP\n"
"               RRRRRRR     UU     UU    MM  MMM  MM    PPPPPPP\n"
"               RR  RR      UU     UU    MM       MM    PP\n"
"               RR   RR     UU     UU    MM       MM    PP\n"
"               RR    RR    UUUUUUUUU    MM       MM    PP\n"
"\n"
"                          Computer Graphic Service\n"
"                              130 Oakwood Lane\n"
"                           Ithaca, New York 14850\n"
"                         Phone/FAX: (607) 273-4927\n"
"                         email: support@genplot.com\n"
"                         WEB:   www.genplot.com\n");
  }
  TTYprintf(
"\n"
"             -----------------------------------------------------\n"
"             |  Copyright 1985-2010  Computer Graphic Service    |\n"
"             |      All rights reserved (library v. 0.002)       |\n"
"             |          Serial Number: %17s         |\n"
"             -----------------------------------------------------\n"
"                         <say thanks to Larry & Mike!>", SysSerialNumber);
	fflush(stdout);
	if (quiet == 0) sleep(1);
	return;
}


/* ===========================================================================
-- Routine to determine the nature of a file passed as a comand line arg.
-- May use any information it finds, including extensions
--
-- Usage: FILETYPE CheckFileType(char *path, char *ext);
--
-- Inputs: path - original token passed as the filename
--         ext  - the extension (from split_path)
--
-- Output: none
--
-- Returns: The best guess FILETYPE (as enumerated in typedef)
--
-- For UNIX, can only look at the extension to determine structure.  For
-- OS/2, we first look at the .TYPE extended attribute for information.
=========================================================================== */
static FILETYPE CheckFileType(char *path, char *ext) {

	FILETYPE ftype=F_NONE;

#if (defined OS2 && !defined NO_EA_MODE)	/* For setting extended attributes */
	char **mylist=NULL;								/* List of information */
	int i,icnt;

	if ( (icnt=EA_QueryAsciiList(path, ".TYPE", &mylist)) > 0) {
		for (i=0; i<icnt; i++) {
			if (stricmp(mylist[i], "RBS Spectrum") == 0) {
				ftype = F_SPECTRUM; break;
			} else if (stricmp(mylist[i], "RUMP Macro") == 0) {
				ftype = F_MACRO; break;
			} else if (stricmp(mylist[i], "Genplot HCOPY") == 0) {
				ftype = F_HCOPY; break;
			}
		}
		free(mylist);
	}
#endif

/* If the type is still not determined, use the extension or assume macro */
	if (ftype == F_NONE) {					/* Keep trying to identify */
		if (stricmp(ext, ".rbs") == 0 || stricmp(ext, ".fres") == 0) {
			ftype = F_SPECTRUM;
		} else if (stricmp(ext, ".hcp") == 0) {
			ftype = F_HCOPY;
		} else if (stricmp(ext, ".mac") == 0) {
			ftype = F_MACRO;
		} else {
			ftype = F_MACRO;
		}
	}
	return(ftype);
}

/* ===========================================================================
-- Routine to check the ENDIAN setting is appropriate to the hardware.
--
-- int CheckEndian(void);
=========================================================================== */
static int CheckEndian(void) {

	union {
		unsigned long val;
		unsigned char cv[4];
	} v;

	v.val = 0x12345678;				/* Pattern to check */

#if BYTE_ENDIAN_ORDER == LITTLE_ENDIAN
	if (v.cv[0]!=0x78 || v.cv[1]!=0x56 || v.cv[2]!=0x34 || v.cv[3]!=0x12) {
		fprintf(stderr, "\n"
			"ERROR: Code compiled for LITTLE_ENDIAN architecture (from machine_type).\n"
			"       Hardware test does not confirm this assumption however.\n"
			"       Data files will be incompatible with other versions of GENPLOT.\n"
			"\n");
		sleep(3);
		return(-1);
	}
#elif BYTE_ENDIAN_ORDER == BIG_ENDIAN
	if (v.cv[0]!=0x12 || v.cv[1]!=0x34 || v.cv[2]!=0x56 || v.cv[3]!=0x78) {
		fprintf(stderr, "\n"
			"ERROR: Code compiled for BIG_ENDIAN architecture (from machine_type).\n"
			"       Hardware test does not confirm this assumption however.\n"
			"       Data files will be incompatible with other versions of GENPLOT.\n"
			"\n");
		sleep(3);
		return(-1);
	}
#else
	#error ** Please tell the compiler whether this is a BIG or LITTLE endian machine **
#endif

	assert(sizeof(UINT16) == 2 && sizeof(INT16)  == 2 &&	/* These better hold */
		    sizeof(UINT32) == 4 && sizeof(INT32)  == 4 &&
			 sizeof(REAL32) == 4 && sizeof(REAL64) == 8);

	return(0);
}
