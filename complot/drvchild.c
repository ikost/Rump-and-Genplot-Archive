/* child driver (receives commands from PIPE and sends them onward */

/* ===========================================================================
-- Implementation notes:
--
-- Unix, OS/2: Child responds to startup by creating a named pipe and passing
--             the name back to the the Parent through stdin (run via popen).
--             Standard C read/writes are used throughout.
--
-- Windows NT: Under NT, we still create a Named Pipe.  However, Windows 
--             returns a HANDLE rather than standard C file descriptors.  The
--             routine _open_osfhandle() used to convert HANDLE to descriptor.
--
-- Windows 95: Implemented as a sub-case under Windows NT.  95 does not
--             support NamedPipes, so the pipes are opened for us first by
--             the parent and we simply inherit them.
=========================================================================== */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#ifndef ROUTINE								/* ROUTINE is the actual device driver */
	#define ROUTINE SlaveDriver			/* Unlikely this is overridden			*/
#endif

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if (defined CSET2)
	#define INCL_DOS							/* Needed for Screen driver versions */
	#define INCL_DOSNMPIPES
	#define INCL_DOSPROCESS
	#define INCL_DOSQUEUES
	#include <os2.h>
#elif defined NT
	#include <windows.h>
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "complot.h"
#include "pipe.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic			SysPanic(__FILE__, __LINE__)
#define  ERRLOGNAME	"drvchild.err"

#undef DEBUG

#ifdef DEBUG
	FILE *fdebug;
	#define	DBG_FPUTS(msg)	(fdebug=fopen("debug.out","a"),fputs(msg,fdebug),fclose(fdebug))
	#define	DBG_FPRINTF_1(fmt,item)	(fdebug=fopen("debug.out","a"),fprintf(fdebug,fmt,item),fclose(fdebug))
#else
	#define	DBG_FPUTS(msg)
	#define	DBG_FPRINTF_1(fmt,item)
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
int drvchild(void *args);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
STATIC void get_bytes(CHAR *buffer, unsigned int length);
STATIC void AbandonedChild(int key);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
extern LOGICAL ROUTINE(INTEGER key, void *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
STATIC int InboundPipe,OutboundPipe;				/* In/outbound pipe handles */

#ifdef MSC70
	STATIC HANDLE API_handle;							/* API pipe handle	*/
#endif


/* ---------------------------------------------------------------------------
-- Main routine - gains control on entry if needed (#ifdef MAIN)
--
-- Usage: int main(int argc, char *argv[]);
--
-- Inputs: argc - number of arguments on command line ( >= 1 )
--         argv - command line arguments ([0] is program itself)
--
-- Output: none
--
-- Returns: EXIT_SUCCESS or EXIT_FAILURE depending on initialization only
--
-- Notes: Converts parsed tokens to full line and calls drvchild()
--------------------------------------------------------------------------- */
#ifdef MAIN
int main(int argc, char *argv[]) {

	char cmdline[80],*aptr;
	int i;

/* Re-encode command line to a single string.  Necessity for NT screen */
	*cmdline = '\0';
	for (i=1,aptr=cmdline; i<argc; i++) {
		if (i != 1) *aptr++ = ' ';
		strcpy(aptr, argv[i]);
		aptr += strlen(aptr);
	}
	return (drvchild(cmdline));
}	
#endif

/* ---------------------------------------------------------------------------
-- Child driver -- main routine for handling client/server interaction
--
-- Usage: int drvchild(void *args);
--
-- Inputs: args - pointer to a character string with the command line
--                arguments.  Only WinNT checks this -- if first two 
--                tokens are numbers, then assumes it is in Win95 mode.
--
-- Output: none
--
-- Returns: EXIT_SUCCESS or EXIT_FAILURE depending on initialization only
--
-- Notes: call format is designed so can be started as a thread passing args
--------------------------------------------------------------------------- */
int drvchild(void *args) {

	char *cmdline=args;

	struct {
		unsigned char key;						/* Key operation */
		unsigned char DspLength;				/* Size of DSP block */
		unsigned char StrLength;				/* Length of string to follow */
		unsigned char Flags;						/* Flags */
	} PipeInfo;
	
	void	  *DriverBlock=NULL;					/* Really static inside do while */
	int		mypid;
	int		rcode;
	DSP		dsp;
	CHAR		LocalString[256];
	char		InboundName[PATH_MAX];
	char		OutboundName[PATH_MAX];
	struct	utsname SystemNames;				/* Return from uname() */

	DBG_FPUTS("[c0]");							/* Tell world I'm started */

/* I am the server, so I make the pipe which the client (COMPLOT) uses */
/* If pipes are not duplex, create two, one for reading, one for writing */
/* Determine the Process ID and node name to create pipe names */
	mypid = getpid();
	uname(&SystemNames);
	sprintf(InboundName,  INBOUND_NAME_FORM,  mypid, SystemNames.nodename);
	sprintf(OutboundName, OUTBOUND_NAME_FORM, mypid, SystemNames.nodename); 

/* ------------------------------------------------------------------------ */
#ifdef CSET2								/* Superceeds POSIX compatibility */

	DosSetPriority(PRTYS_PROCESS, PRTYC_REGULAR, +2, 0);		/* Boost priority */
	if ( (rcode = DosCreateNPipe(InboundName, (HPIPE *) &InboundPipe, 
					  NP_ACCESS_DUPLEX | NP_NOINHERIT,
					  NP_WAIT | NP_TYPE_BYTE | 1,			/* Allow only 1 occurances */
					  PIPE_BUF, PIPE_BUF,					/* Big in, small out */
					  500L)) != 0) {
		DBG_FPRINTF_1("ERROR: Unable to create pipe (%i)\n", rcode);
		return(EXIT_FAILURE);
	}
	OutboundPipe = InboundPipe;										/* In/out same */

/* ... tell parents name of the pipe so they can open also */
	fprintf(stdout, "%s %s\n", InboundName, OutboundName);	/* Tell parent */
	fflush(stdout);
	DBG_FPUTS("[c1]");
	if ( (rcode = DosConnectNPipe(InboundPipe)) != 0) {		/* Wait for use */
		DBG_FPRINTF_1("ERROR: Nobody connected to my pipe (%i)\n", rcode);
		return(EXIT_FAILURE);
	}

/* ------------------------------------------------------------------------ */
#elif defined MSC70

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);		/* Boost priority */

/* Decide if in Win95 operational mode, or full blown NT */
	if ( sscanf(cmdline, "%d %d",  &OutboundPipe, &InboundPipe) == 2) {
		strcpy(OutboundName, "fucked up Win95");				/* 95 limited mode */
		strcpy(InboundName,  "fucked up Win95");

		if (OutboundPipe==0 || InboundPipe==0) {		/* Just decide 0 invalid */
			DBG_FPRINTF_1("ERROR: Bad command line: %s\n", cmdline);
			return(EXIT_FAILURE);
		}
		write(OutboundPipe, "hi", 2);						/* Acknowledge my presence */
		DBG_FPUTS("[c1]");
		API_handle = INVALID_HANDLE_VALUE;				/* So know we are 95 version */

	} else {														/* Full NT safe version	*/
		if ( (API_handle = CreateNamedPipe(InboundName, 
							PIPE_ACCESS_DUPLEX, 				/* Duplex mode (NO_INHERIT?) */
							PIPE_TYPE_BYTE | PIPE_WAIT,	/* Byte mode w/ blocking	*/
							1,										/* Only one instance			*/
							PIPE_BUF, PIPE_BUF,				/* Big enough both ways		*/
							500, 									/* 1/2 second timeout		*/
							NULL) )								/* No security descriptor	*/
				 			== INVALID_HANDLE_VALUE) {
			fprintf(stderr, "ERROR: Unable to create pipe (%i)\n", GetLastError());
			return(EXIT_FAILURE);
		} else if ( (InboundPipe = _open_osfhandle((long) API_handle, _O_BINARY)) == -1) {
			fprintf(stderr, "ERROR: Could not convert HANDLE into C file descriptor\n");
			return(EXIT_FAILURE);
		}
		OutboundPipe = InboundPipe;									/* In/out same */
			
/* ... tell parents name of the pipe so they can open also */
		fprintf(stdout, "%s %s\n", InboundName, OutboundName);	/* Tell parent */
		fflush(stdout);
		DBG_FPUTS("[c1]");
		if (! ConnectNamedPipe(API_handle, NULL)) {
			fprintf(stderr, "ERROR: Nobody connected to my pipe (%i)\n", GetLastError());
			return(EXIT_FAILURE);
		}
	}

/* ------------------------------------------------------------------------ */
#else						/* !OS2 (POSIX compatibility) */

/* Due to scam about pipe opening (sec 5.3.1.2), must open read NONBLOCK first,
 * then tell parent the names so it can open both safely.  Must reset the
 * NONBLOCK flag so normal processing runs as expected.
 * 
 * (1) Open incoming FIFO for reading using NONBLOCK mode.
 * (2) Reset structure to BLOCKING for later reads.
 * (3) Give parent names to go ahead and open (both should succeed easily!).
 */

	if (mkfifo(InboundName, FIFO_MODE) != 0 || mkfifo(OutboundName, FIFO_MODE) != 0) {
		fprintf(stderr, "ERROR: Unable to create pipe (%s or %s\n", InboundName, OutboundName);
		return(EXIT_FAILURE);
	}

/* Open our share, and then wait for parent to open the other share */
	if ( (InboundPipe = open(InboundName, O_RDONLY | O_NONBLOCK)) == -1 ) {
		fprintf(stderr, "ERROR: Unable to open pipe %s\n", InboundName);
		return(EXIT_FAILURE);
	}
	fcntl(InboundPipe, F_SETFL, 0);								/* Back to blocking */
	fprintf(stdout,"%s %s\n", InboundName, OutboundName);	/* Tell parent names */
	fflush(stdout);

	DBG_FPUTS("[c1]");

	if ( (OutboundPipe = open(OutboundName, O_WRONLY)) == -1) {	/* BLOCKED! */
		fprintf(stderr, "ERROR: Unable to open pipe %s\n", OutboundName);
		return(EXIT_FAILURE);
	}

#endif
/* ------------------------------------------------------------------------ */

	DBG_FPUTS("[c2]");

/* ... Continue loop to transfer commands to child process */
	do {
		get_bytes((CHAR *) &PipeInfo, sizeof(PipeInfo));
		get_bytes((CHAR *) &dsp,      PipeInfo.DspLength);
		if (PipeInfo.StrLength != 0xFF) {					/* Not a blank string */
			get_bytes(LocalString, PipeInfo.StrLength+1);
			dsp.ini.IO_Channel = LocalString;
		}

		if (PipeInfo.key == INIFNC) dsp.ini.DriverBlock = &DriverBlock;

		rcode = ROUTINE(PipeInfo.key, DriverBlock, &dsp);
		
		if (PipeInfo.Flags & 0x01) write(OutboundPipe, &dsp, PipeInfo.DspLength);
		if (PipeInfo.Flags & 0x02) write(OutboundPipe, &rcode, sizeof(int));

	} while (PipeInfo.key != ENDFNC);

#ifdef CSET2
	write(OutboundPipe, &mypid, sizeof(mypid));
	DosDisConnectNPipe(OutboundPipe);				/* Just disconnect */
#elif defined MSC70
	write(OutboundPipe, &mypid, sizeof(mypid));
	if (API_handle != INVALID_HANDLE_VALUE) {		/* Just disconnect */
		DisconnectNamedPipe(API_handle);
	} else {													/* Or close if Win95 */
		close(InboundPipe); close(OutboundPipe);
	}
#else
	close(InboundPipe); close(OutboundPipe);
#endif
	return(EXIT_SUCCESS);

}

/* ===========================================================================
--  Routine to buffer the incoming piped data.  Rather than fight the I/O time,
--  read big blocks and internally buffer.  Size set to half size of pipe to
--  improve the chance of reading something
--
=========================================================================== */
STATIC void get_bytes(CHAR *buffer, unsigned int length) {

	static CHAR		mybuffer[PIPE_BUF / 2];
	static CHAR	  *myptr;
	static int		mycnt=0;

	while (length != 0) {								/* While more to copy			*/
		if (mycnt == 0) {									/* Do I need more from pipe?	*/
			mycnt = read(InboundPipe, mybuffer, sizeof(mybuffer));
			if (mycnt == -1) {							/* If negative ==> error		*/
				if (errno==EAGAIN) {						/* Should retry again later	*/
					sleep(1);								/* Too bad can't specify less	*/
					mycnt = 0;								/* Reset to zero					*/
					continue;								/* Will come right back			*/
				} 
				AbandonedChild(1);						/* Print message and exit		*/
			} else if (mycnt == 0) {					/* EOF message from everyone	*/
				AbandonedChild(2);						/* EXIT NOW!!!!!					*/
			}
			myptr = mybuffer;
		} else {
			*buffer++ = *myptr++;
			mycnt--; length--;
		}
	}
	return;
}

/* ---------------------------------------------------------------------------
-- This routine is called on really major fuckups in the code.  It primarily
-- prints a message to that effect and does a complete and total exit
--
-- Usage:  AbandonedChild(int key);
--
-- Inputs: key - cause of the abandonment
--
-- Output: none
--
-- Returns: never!
--------------------------------------------------------------------------- */
STATIC void AbandonedChild(int key) {

#ifdef DEBUG

	static FILE *errlog;
	time_t ltime;

	if ( (errlog = fopen(ERRLOGNAME, "a")) != NULL) {
		time(&ltime);
		fprintf(errlog,"Child driver %i abnormal exit at %s", getpid(), ctime(&ltime));
		switch (key) {
			case 1:
				fprintf(errlog,"  Pipe read error (%s)\n", strerror(errno));
				break;
			case 2:
				fputs("  EOF report -- driver abandoned by master\n",errlog);
				break;
			default:
				fputs("  Unknown request to abort\n", errlog);
		}
		fclose(errlog);
	}
#endif

/* --- Have to try to shut down nicely since strange things may be running */
	ROUTINE(ENDFNC, NULL, NULL);				/* Run the ENDFNC function */
#ifdef CSET2
	DosDisConnectNPipe(OutboundPipe);
#elif defined MSC70
	if (API_handle != INVALID_HANDLE_VALUE) {		/* Just disconnect */
		DisconnectNamedPipe(API_handle);
	} else {													/* Or close if Win95 */
		close(InboundPipe); close(OutboundPipe);
	}
#else
	close(InboundPipe); close(OutboundPipe);
#endif	
	exit(EXIT_FAILURE);							/* Hopefully closes everything */
}
