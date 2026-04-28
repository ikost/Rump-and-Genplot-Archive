/* DSPTCH - Dispatch routine through named pipes  */
/* ============================================================================
--     Pipes_Driver - COMPLOT driver for Named Pipes
--
--     Usage: LOGICAL Pipes_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);
--
--     Inputs: cmd   - Command - See driver.ins for definitions
--             PARMS - Variable dimensioned array with parameters for transfer
--                     Type and direction depend on command.
--
--     Output: drvps - Success of operation
--
-- Implementation notes:
--
-- OS/2/Unix:  Child launched via popen call.  Child makes up pipe names and
--             returns names to stdout.  We simply have to open() them to make
--             the connection.  All other responsibility lies with drvchild().
--
-- Windows NT: Identical to OS/2 and UNIX.  But because 95 and NT are common
--             code, have to initially test which operating system.
--
-- Windows 95: Multiple problems.  (1) Named pipes do not exist, so we as the
--             parent create the pipes outselves.  (2) popen() does not pass
--             open file descriptors so have to manually spawn().  Child
--             notifies us that it is ready by returning message "hi" in the
--             pipe.
============================================================================ */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#define	BUFFER_PIPES						/* Locally buffer pipe writes	*/

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "complot.h"
#include "extends.h"
#include "pipe.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef DEBUG
/*	#define	DBG_FPUTS(str)					(TTYputs(str),TTYflush()) */
/*	#define	DBG_FPRINTF_1(fmt,item)		(TTYprintf(fmt,item),TTYflush()) */
	#define	DBG_FPUTS(str)
	#define	DBG_FPRINTF_1(fmt,item)
#else
	#define	DBG_FPUTS(str)
	#define	DBG_FPRINTF_1(fmt,item)
#endif

#define	FL_RETURN_STRUCT		0x01
#define	FL_RETURN_RCODE		0x02
#define	FL_FLUSH					0x80

#define	DPI			1000							/* Pixel resolution/inch		*/

#if defined OS2
	#define	DEBUGPROGRAM	"ipmd "				/* Debug program for OS/2		*/
	#define	SHELLPROGRAM	""						/* Shell run version				*/
	#define	DEFAULTPROGRAM	""						/* Default run version			*/
	#define	DRV_EXTENSION	".drv"
#elif defined NT
	#define	DEBUGPROGRAM	"ipmd "				/* Debug program for NT			*/
	#define	SHELLPROGRAM	""						/* Shell run version				*/
	#define	DEFAULTPROGRAM	""						/* Default run version			*/
	#define 	DRV_EXTENSION	".exe"
#else
	#define	DEBUGPROGRAM	"csd "				/* Unix standard C debugger	*/
	#define	SHELLPROGRAM	"csh "				/* Pass to special shell		*/
	#define	DEFAULTPROGRAM	""						/* Default run version			*/
	#define	DRV_EXTENSION	".drv"
#endif

typedef struct _DRVBLOCK {							/* These need to quasi-static */
	FILE *ChildHndl;									/* Handle of popen to child   */
	CHAR	InboundName[PATH_MAX];	  				/* Name of incoming data pipe */
	CHAR	OutboundName[PATH_MAX];					/* Name of outgoing data pipe */
	int	InboundPipe, OutboundPipe;				/* File descriptors of pipes	*/
	int	bufcnt;										/* Local buffering of pipe		*/
	CHAR *bufptr;
	CHAR	buffer[PIPE_BUF];
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL Pipes_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE int PipeMessage(DRVBLOCK *blk, INTEGER key, DSP *dsp, size_t cnt, 
								CHAR  *str, unsigned char flags);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ===========================================================================
-- Routine to initialize using Named Pipes, for OS/2, UNIX and WinNT
--
-- Usage: Ini_Win_PipeDriver(int key, DRVBLOCK *blk, DSP *dsp);
--
-- Inputs: key - function key.  Will be INIFNC or TABINIFNC only.
--         blk - pointer to driver information block
--         dsp - parameters from/to calling procedure
--
-- Output: Sets paraemters in blk and/or dsp, initializing as needed.
--
-- Returns: TRUE if initialization succeeded, FALSE otherwise
--
-- Notes: May be called by Ini_Win_Pipedriver if running NT.
=========================================================================== */
static BOOL Ini_PipeDriver(int key, DRVBLOCK *blk, DSP *dsp) {

	int			i;
	void		 **fptr;
	CHAR			program[DFLT_STR_SIZE], driver[DFLT_STR_SIZE];
	CHAR			buf[DFLT_STR_SIZE];
	CHAR		  *cptr, *header;

/*
 * THE PLAN:  Fire up the child using "popen".  Communication with the
 *            child will be through 2 named pipes.  The child will determine
 *            name of the pipes and we just read them from opened handle.
 *            If second name is <duplex>, assume first name is O_RDWR capable.
 */
	cptr = dsp->ini.Driver;					/* Program name string */
	if (*cptr == '!') cptr++;
	if (*cptr == '!') {						/* !! gives cvp debugger!		*/
		cptr++;
		header = DEBUGPROGRAM;
	} else if (*cptr == '+') {				/* !+ gives shell execution	*/
		cptr++;
		header = SHELLPROGRAM;
	} else {										/* ! alone gives default		*/
		header = DEFAULTPROGRAM;
	}

	sprintf(driver,"%s%s", cptr, DRV_EXTENSION);		/* Make the driver name			*/
	SysResolveDyntName(driver, driver, sizeof(driver));
	SysQualifyPath(driver, driver, sizeof(driver));
#ifdef NT
	sprintf(program,"%s\"%s\" Complot_Request", header, driver);
#else
	sprintf(program,"%s%s Complot_Request", header, driver);
#endif

	if ( (blk->ChildHndl = (FILE *) popen(program, "r")) == NULL) {
		ERRprintf("ERROR: Couldn't start device driver child (%s)\n", program);
		free(blk); return(FALSE);
	}

/* ... Wait for child to send us pipe names through the <stdout> route */
	DBG_FPUTS("[p0]");
	fcntl(fileno(blk->ChildHndl), F_SETFL, O_NONBLOCK);	/* Unblock */
	i = MAXSLEEPS;
	while (fgets(buf, sizeof(buf), blk->ChildHndl) == NULL) {
		DBG_FPRINTF_1("[w%1.1i]",i);
		if (i-- == 0) { 
			ERRprintf("ERROR: Device driver child seems to be stillborn (%s)\n", program);
			free(blk); return(FALSE);
		}
		sleep(SLEEPTIME); 
		continue;
	}
	if ( (cptr = strchr(buf, '\n')) != NULL)  *cptr = '\0';

	if ( (sscanf(buf, "%s %s", blk->OutboundName, blk->InboundName)) != 2) {
		ERRprintf("ERROR: Invalid pipe names returned from child - the ingrate! (%s)\n", buf);
		free(blk); return(FALSE);
	}
	while ( (cptr = strchr(blk->OutboundName, '&')) != NULL) *cptr = ' ';
	while ( (cptr = strchr(blk->InboundName,  '&')) != NULL) *cptr = ' ';

/* ... Child has already opened end O_RDONLY, so O_WRONLY should succeed! */
	DBG_FPUTS("[p1]");
	if (strcmp(blk->InboundName,blk->OutboundName) != 0) {
		if ( (blk->OutboundPipe = open(blk->OutboundName, O_BINARY|O_WRONLY|O_NONBLOCK)) == -1) {
			ERRprintf("ERROR: Unable to open outbound client pipe (%s)\n", blk->OutboundName);
			free(blk); return(FALSE);
		}
		if ( (blk->InboundPipe  = open(blk->InboundName,  O_BINARY|O_RDONLY|O_NONBLOCK)) == -1) {
			ERRprintf("ERROR: Unable to open inbound client pipe (%s)\n", blk->InboundName);
			free(blk); return(FALSE);
		}
		fcntl(blk->InboundPipe,  F_SETFL, 0);		/* Back to blocking */
		fcntl(blk->OutboundPipe, F_SETFL, 0);		/* Back to blocking */
	} else {
		MilliSleep(100);				/* Wait for him first */
		if ( (blk->OutboundPipe = open(blk->OutboundName, O_BINARY | O_RDWR)) == -1) {
			DBG_FPRINTF_1("DUPLEX PIPE OPENING ERROR: %i\n", errno);
			ERRprintf("ERROR: Unable to open duplex client pipe (%s)\n", blk->OutboundName);
			free(blk); return(FALSE);
		}
		blk->InboundPipe = blk->OutboundPipe;
	}
	DBG_FPUTS("[p2]");

	fptr = dsp->ini.DriverBlock;				/* Save this pointer over call */
	PipeMessage(blk, key, dsp, sizeof(DspInifnc), dsp->ini.IO_Channel, FL_RETURN_STRUCT);
	dsp->ini.DriverBlock  = fptr;
	DBG_FPUTS("[p3]\n");

	return(TRUE);
}


/* ===========================================================================
-- Routine to initialize under windows, splitting between WinNT (real multitasking)
-- and Win95 (crapola)
--
-- Usage: Ini_Win_PipeDriver(int key, DRVBLOCK *blk, DSP *dsp);
--
-- Inputs: key - function key.  Will be INIFNC or TABINIFNC only.
--         blk - pointer to driver information block
--         dsp - parameters from/to calling procedure
--
-- Output: Sets parameters in blk and/or dsp, initializing as needed.
--
-- Returns: TRUE if initialization succeeded, FALSE otherwise
--
-- Notes: First thing is to determine if this is NT or 95.   If NT, then revert
--        and use the much safer Named Pipe method implemented Ini_PipeDriver().
--        Only execute this version, which opens the pipes at this end, if the
--        system is Win-95.
=========================================================================== */
#ifdef NT

BOOL Ini_Win_PipeDriver(int key, DRVBLOCK *blk, DSP *dsp) {

	void		 **fptr;
	CHAR			driver[DFLT_STR_SIZE];
	CHAR			buf[DFLT_STR_SIZE];
	CHAR		  *cptr;
	int			p1[2], p2[2];
	char			ClientOutbound[10], ClientInbound[10];
	OSVERSIONINFO Info;

/* 95 or NT?  NT uses NamedPipes.  Win95 uses this non-robust version */
	Info.dwOSVersionInfoSize = sizeof(Info);
	GetVersionEx(&Info);
	if (getenv("FAKE_WINNT") != NULL ||
		(Info.dwPlatformId==VER_PLATFORM_WIN32_NT && getenv("FAKE_WIN95")==NULL)) {
		return(Ini_PipeDriver(key, blk, dsp));
	}

/* Second, note that we are running with local pipes */
	TTYprintf("MSG: Resorting to lame-o Win95 interprocess communication\n");

/*
 * THE PLAN:  Fire up the child using "popen".  Communication with the
 *            child will be through 2 named pipes.  The child will determine
 *            name of the pipes and we just read them from opened handle.
 *            If second name is <duplex>, assume first name is O_RDWR capable.
 *
 * Under other versions, !! should give debugger, and !+ shell execution
 */
	cptr = dsp->ini.Driver;					/* Program name string */
	if (*cptr == '!') cptr++;
	if (*cptr == '!' || *cptr == '+') cptr++;

	sprintf(driver,"%s%s", cptr, DRV_EXTENSION);
	SysResolveDyntName(driver, driver, sizeof(driver));
	SysQualifyPath(driver, driver, sizeof(driver));

	if (_pipe(p1,PIPE_BUF,O_BINARY) != 0 || _pipe(p2,PIPE_BUF,O_BINARY) != 0) {
		ERRprintf("ERROR: Unable to make Win95 pipes (pipedrv.c)\n");
		return(FALSE);
	}

	blk->InboundPipe  = p1[0]; _itoa(p1[1], ClientOutbound, sizeof(ClientOutbound));
	blk->OutboundPipe = p2[1]; _itoa(p2[0], ClientInbound,  sizeof(ClientInbound));

   if (spawnl(P_NOWAIT, driver, driver, ClientOutbound, ClientInbound, 
				 "Complot_Request", NULL) == -1) {
		ERRprintf("ERROR: Couldn't start device driver child (%s)\n", driver);
		goto ExitError;
	}

/* ... Wait for child to send us "hi" in the pipe */
	DBG_FPUTS("[p0-95]");
	sleep(1);										/* Give it time to wake up				*/
	read(blk->InboundPipe, buf, 2);			/* reads always block so can't trap	*/
	if (strnicmp(buf, "hi", 2) != 0) {
		ERRprintf("ERROR: The ingrate child never even said `hi'\n");
		goto ExitError;
	}

/* ... Close our unneeded halves of the pipe */
	close(p1[1]); close(p2[0]);

/* ... Now, just fill pipe with the first message and continue */
	fptr = dsp->ini.DriverBlock;				/* Save this pointer over call */
	PipeMessage(blk, key, dsp, sizeof(DspInifnc), dsp->ini.IO_Channel, 
					FL_RETURN_STRUCT);
	dsp->ini.DriverBlock  = fptr;
	DBG_FPUTS("[p3-95]");
	return(TRUE);


/* Common exit point in case of problems after pipes are created */
ExitError:
	close(p1[0]); close(p1[1]); close(p2[0]); close(p2[1]);
	free(blk); 
	return(FALSE);
}

#endif /* NT */


/* ===========================================================================
-- Routine to implement the pipe driver, passing the information through
-- structures to the appropriate child routine.
--
-- Usage: BOOL Pipes_Driver(key, DriverBlock, dsp) {
--
-- Inputs: key - what to do
--         DriverBlock - pointer to a buffer space containing local defined
--                       and handled data.  Must be returned in dsp during
--                       initialization and released on ENDFNC.
--         dsp - key dependent in/out information
--
-- Output: dsp - possibly parts of the sturcture depending on key
--
-- Returns: TRUE if successful, FALSE otherwise
=========================================================================== */
BOOL Pipes_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	int i, isize, rcode;
	DRVBLOCK	  *blk;

	blk = DriverBlock;								/* Just because I'm lazy */

	rcode = TRUE;										/* Return true unless otherwise */
	switch (key) {

		case INIFNC:									/* Initialization separate		*/
		case TABINIFNC:								/* Or tablet initialization	*/

			if ((blk = *dsp->ini.DriverBlock = malloc(sizeof(DRVBLOCK))) == NULL) 
				return(FALSE);
			 blk->ChildHndl      = NULL;
			*(blk->InboundName)  = '\0';
			*(blk->OutboundName) = '\0';
			 blk->OutboundPipe   = 0;
			 blk->InboundPipe    = 0;
			 blk->bufcnt		   = 0;
			 blk->bufptr         = blk->buffer;

#ifndef NT
			if (! Ini_PipeDriver(key, blk, dsp)) return(FALSE);
#else												/* Must consider possible Win95 crap */
			if (! Ini_Win_PipeDriver(key, blk, dsp)) return(FALSE);
#endif
			break;

		case ENDFNC:
		case TABENDFNC:
			PipeMessage(blk, key, dsp, 0, NULL, FL_FLUSH);
			read(blk->InboundPipe, (char *) &i, sizeof(i));

			close(blk->OutboundPipe);
			if (blk->InboundPipe != blk->OutboundPipe)		/* Are we DUPLEX? */
				close(blk->InboundPipe);

			#ifdef CSET2
				unlink(blk->InboundName);
				unlink(blk->OutboundName);
			#endif

			if (blk->ChildHndl != NULL) pclose(blk->ChildHndl);
			free(blk);
			break;

		case LINFNC:												/* Draw line */
			PipeMessage(blk, key, dsp, sizeof(DspLinfnc), NULL, 0);
			break;
			
		case COLFNC:					/* Set color */
			PipeMessage(blk, key, dsp, sizeof(DspColfnc), NULL, 0);
			break;

		case SPDFNC:					/* Set pen speed */
			PipeMessage(blk, key, dsp, sizeof(DspSpdfnc), NULL, 0);
			break;

		case VISFNC:					/* Set visibility (light, dark, complement) */
			PipeMessage(blk, key, dsp, sizeof(DspVisfnc), NULL, 0);
			break;

		case CURFNC:					/* Read cursor function */
		case TABGETPOSN:
		case TABGETPOINT:
			PipeMessage(blk, key, dsp, sizeof(DspCurfnc), NULL, FL_RETURN_STRUCT);
			break;

		case CURTRK:
			PipeMessage(blk, key, dsp, sizeof(DspCurfnc), NULL, FL_RETURN_STRUCT);
			break;

		case CURBOX:
			PipeMessage(blk, key, dsp, sizeof(DspCurbox), NULL, FL_RETURN_STRUCT);
			break;

		case PNTFNC:												/* Plot a single point */
			PipeMessage(blk, key, dsp, sizeof(DspPntfnc), NULL, 0);
			break;

		case LWFNC:										/* Line width function */
			PipeMessage(blk, key, dsp, sizeof(DspLWfnc), NULL, 0);
			break;

		case DRAWMARKER:
			rcode = PipeMessage(blk, key, dsp, sizeof(DspDrawMarker), NULL, FL_RETURN_RCODE);
			break;

		case DRAWCHAR:						/* Character drawing functions */
			rcode = PipeMessage(blk, key, dsp, sizeof(DspDrawChar), NULL, FL_RETURN_RCODE);
			break;

		case TELLCLIP:
			PipeMessage(blk, key, dsp, sizeof(DspClipSet), NULL, 0);
			break;

		case PAGFNC:
			PipeMessage(blk, key, dsp, sizeof(DspPagfnc), NULL, 0);
			break;
		
		case IOCTL:
			PipeMessage(blk, key, dsp, sizeof(DspIOCTL), dsp->ioctl.str, FL_FLUSH);
			break;
			
		case TXTFNC:
			PipeMessage(blk, key, dsp, sizeof(DspTxtfnc), dsp->text.str, 0);
			break;
			
		case FLSFNC:					/* Flush all buffers */
			PipeMessage(blk, key, dsp, 0, NULL, FL_FLUSH);
			break;

		case FRMFNC:					/* End of frame */
			PipeMessage(blk, key, dsp, sizeof(DspFrmfnc), NULL, 0);
			break;

		case ERSFNC:					/* Erase screen */
			PipeMessage(blk, key, dsp, sizeof(DspErsfnc), NULL, 0);
			break;

		case ANMFNC:					/* Exit plot mode to alphanumerics mode */
			PipeMessage(blk, key, dsp, 0, NULL, 0);
			break;

		case GRPFNC:
			PipeMessage(blk, key, dsp, 0, NULL, FL_FLUSH);
			break;

		case BEGINPATH:
		case ENDPATH:
			PipeMessage(blk, key, dsp, 0, NULL, 0);
			break;
		case FILLPATH:
			PipeMessage(blk, key, dsp, sizeof(DspFillPath), NULL, 0);
			break;
		case STROKEPATH:
			PipeMessage(blk, key, dsp, sizeof(DspStrokePath), NULL, 0);
			break;

		case PANFNC:
		case POFFNC:
		case REGFNC:
		case FILFNC:
			PipeMessage(blk, key, dsp, 0, NULL, 0);
			break;
			
		case AXESLIMIT:
			PipeMessage(blk, key, dsp, sizeof(DspAxesLimit), NULL, 0);
			break;

		case FILLEDRECT:
			PipeMessage(blk, key, dsp, sizeof(DspFillRect), NULL, 0);
			break;

		case PALETTE:
			isize = sizeof(DspPalette)+dsp->palette.num_entries*sizeof(dsp->palette.rgb[0]);
			PipeMessage(blk, key, dsp, isize, NULL, FL_FLUSH);
			break;

		default:
			TTYprintf("WARNING: Message %d is not handled by the pipe driver\n", key);
			return(FALSE);
	}
	return(rcode);
}


/* ---------------------------------------------------------------------------
-- Low level to output a count, the message key, dsp block, string, and read(?)
--
-- Usage: PipeMessage(DRVBLOCK *blk, INTEGER key, DSP *dsp, size_t cnt, 
--					  CHAR  *str, int flags) {
--
-- Inputs:  blk -- driver information block
--          key -- Operation requested from the driver
--          dsp -- structure containing information for the driver
--          cnt -- size of the dsp structure
--          str -- String to be passed to driver (NULL if not necessary)
--          flags -- 0x00 ==> nothing coming back, don't wait
--							0x01 ==> expect back dsp block modified
--							0x02 ==> expect back success code
--							0x80 ==> flush buffer before returning
--------------------------------------------------------------------------- */
static int outpipe;						/* Temporary link for signal from calls */

static void new_handler(int sig) {

	static struct {
		unsigned char key;						/* Key operation */
		unsigned char DspLength;				/* Size of DSP block */
		unsigned char StrLength;				/* Length of string to follow */
		unsigned char Flags;						/* Flags */
	} PipeInfo = {ABORTFNC, 0, 0xFF, 0};

	TTYprintf("WARNING: ^C caught while waiting for response from driver\n");
	write(outpipe, (char *) &PipeInfo, sizeof(PipeInfo));
	signal(sig, new_handler);					/* Reset handler again */

	return;
}


PRIVATE int PipeMessage(DRVBLOCK *blk, INTEGER key, DSP *dsp, size_t cnt, 
								CHAR  *str, unsigned char flags) {
				
	struct {
		unsigned char key;						/* Key operation */
		unsigned char DspLength;				/* Size of DSP block */
		unsigned char StrLength;				/* Length of string to follow */
		unsigned char Flags;						/* Flags */
	} PipeInfo;

	void (*old_handler)(int sig);
	int i, ngot, rcode=TRUE, SpaceNeeded;
	CHAR *inptr;
	
	PipeInfo.key        = (unsigned char) key;
	PipeInfo.DspLength  = (unsigned char) (min(254, cnt));
	PipeInfo.StrLength =  (str == NULL) ? (unsigned char) 0xFF : (unsigned char) (min(254,strlen(str)));
	PipeInfo.Flags      = flags;

#ifdef BUFFER_PIPES
	SpaceNeeded = sizeof(PipeInfo) + PipeInfo.DspLength + ((unsigned char)(PipeInfo.StrLength + 1));

	if ( (blk->bufcnt+SpaceNeeded) > (PIPE_BUF-1) ) {
		write(blk->OutboundPipe, (char *) blk->buffer, blk->bufcnt);
		blk->bufptr = blk->buffer;
		blk->bufcnt = 0;
	}

	inptr = (CHAR *) &PipeInfo;
	for (i=sizeof(PipeInfo);   i; i--) {*(blk->bufptr++) = *inptr++; blk->bufcnt++;}
	inptr = (CHAR *) dsp;
	for (i=PipeInfo.DspLength; i; i--) {*(blk->bufptr++) = *inptr++; blk->bufcnt++;}
	if (PipeInfo.StrLength != 0xFF) {
		inptr = (CHAR *) str;
		for (i=PipeInfo.StrLength+1; i; i--) {*(blk->bufptr++) = *inptr++; blk->bufcnt++;}
	}

	if ( (PipeInfo.Flags != 0)) {									/* Any requests? */
		write(blk->OutboundPipe, (char *) blk->buffer, blk->bufcnt);
		blk->bufptr = blk->buffer;
		blk->bufcnt = 0;
	}

#else

	write(blk->OutboundPipe, (char *) &PipeInfo, sizeof(PipeInfo));
	if (PipeInfo.DspLength != 0)
		write(blk->OutboundPipe, (char *) dsp, PipeInfo.DspLength);
	if (PipeInfo.StrLength != 0xFF && PipeInfo.StrLength != 0) {
		write(blk->OutboundPipe, (char *) dsp, PipeInfo.StrLength);

#endif

/* ---------------------------------------------------------------------------
-- If requesting return information, subclass SIGBREAK signal to be able
-- to abort out of a read.  On first ^C, ABORT message will be sent to the
-- driver which should abort current operation and return immediately.  On
-- second ^C, we will kill the driver and return.
--------------------------------------------------------------------------- */
	if (! (PipeInfo.Flags & (FL_RETURN_RCODE | FL_RETURN_STRUCT)) )
		return(rcode);
	
	outpipe = blk->OutboundPipe;								/* Need this static */
	old_handler = signal(SIGINT, new_handler);

	if (PipeInfo.Flags & FL_RETURN_STRUCT) {				/* Full reread */
		ngot = read(blk->InboundPipe, (char *) dsp, PipeInfo.DspLength);
		if (ngot != PipeInfo.DspLength)
			ERRprintf("WARNING: Return struct length mismatch on pipe (expect: %d  got: %d) (PIPEDRV)\n", PipeInfo.DspLength, ngot);
	}
	if (PipeInfo.Flags & FL_RETURN_RCODE)					/* Return TRUE/FALSE	*/
		read(blk->InboundPipe, (char *) &rcode, sizeof(rcode));

	if (old_handler != SIG_ERR) signal(SIGINT, old_handler);
	return(rcode);
}
