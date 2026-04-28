/* IO_channel routines */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef CSET2									/* Needed for spool routines at end */
	#define INCL_WINSHELLDATA				/* Need more procedures					*/
	#define INCL_DOS
	#define INCL_SPL							/* For any of the Listxxxx procedures */
	#define INCL_BASE
	#define INCL_SPLDOSPRINT
	#define INCL_ERRORS
	#define INCL_DEV
	#include <os2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "io_chan.h"

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
#ifdef CSET2
	PRIVATE FILE *IO_SPL_Open(char *Parms);
#endif

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ===========================================================================
-- The IO_OpenChannel opens an I/O channel for output of plot vectors to the
-- physical device.  This is commonly a connection to either a pipe for 
-- spooling or to a file.  However, may also handle asynchronous devices 
-- and special spoolers.
--
-- Usage:  IO_BLOCK *IO_OpenChannel(char *iochan, int iparms);
--
-- Inputs: iochan - I/O channel information from the device info tables
--         iparms - Initial status words describing desired status
--
-- Output: none
--
-- Returns: Pointer to IO_BLOCK structure with all information and status
--          parameters properly set and opened for the device.  On failure,
--          will return NULL.
=========================================================================== */
IO_BLOCK *IO_OpenChannel(char *program, char *iochan, int options, int flow) {

	IO_BLOCK *io;

#if (defined NT || defined OS2)
	static char *fmode[] = {"w",  "wb"};	/* Open mode (text/bin) files		*/
	static char *dmode[] = {"w+", "wb+"};	/* Open mode (text/bin) devices	*/
#else	/* UNIX has no concept of BINARY versus ASCII */
	static char *fmode[] = {"w",  "w"};		/* Open mode (text/bin) files		*/
	static char *dmode[] = {"w+", "w+"};	/* Open mode (text/bin) devices	*/
#endif
	int imode=0;									/* Index into mode					*/
	int bufrequest=-1;							/* Requested buffer size			*/

	int cnt;
	struct stat buf;
	char *aptr, *bptr, achr;
	char szTmp[DFLT_STR_SIZE];
	
	static struct _list_chan {					/* List of known I/O channels */
		char *name;
		int	type;
		int	Abilities;
	} ChanList[] = {
		{"pipe",		IO_PIPE, 0},	{"lpr",		IO_LPR,	0},
		{"asnyc",	IO_RS232, 0},
		{"file",		IO_FILE, 0},	{"disk",		IO_FILE, 0},	{"dev", IO_FILE, 0},
		{"gpib",		IO_GPIB, 0},
		{"spool",	IO_SPOOL, 0},
		{NULL, 0, 0} };
	struct _list_chan *lptr=ChanList;;

	if ( (io = (IO_BLOCK *) malloc(sizeof(IO_BLOCK))) == NULL) return(NULL);
	
	io->type        = 0;						/* Unrecognized type					*/
	strscpy(io->Program, program, sizeof(io->Program));	/* Program (< 20 chars)				*/
	strscpy(io->Channel, "file", sizeof(io->Channel));		/* Default is type file				*/
	io->funit		 = NULL;					/* No unit on file					*/
	io->FlowControl = flow;					/* Flow control						*/
	io->Options     = options;				/* Requested options					*/
	io->Abilities   = 0;						/* Assume no abilities				*/
	io->BufSize		 = BUFSIZ;				/* Assume it is full size			*/
	
	aptr = io->Parms; cnt = 0;
	do {
		if (*iochan != '*') {					/* Still looking for break point */
			if (++cnt < sizeof(io->Parms)) *(aptr++) = *iochan;
		} else {
			*aptr = '\0';
			if (stricmp(io->Parms, "dtr") == 0) 
				io->FlowControl = IOF_DTR;
			else if (stricmp(io->Parms, "xon") == 0) 
				io->FlowControl = IOF_XON;
			else if (stricmp(io->Parms, "enq") == 0) 
				io->FlowControl = IOF_ENQ;
			else if ( (stricmp(io->Parms, "none") == 0) || (stricmp(io->Parms, "noenq") == 0) )
				io->FlowControl = IOF_NOFLOW;
			else if (strnicmp(io->Parms, "len=", 4) == 0) {
				bufrequest = atoi(io->Parms+4);
			} else {											/* This is really the name	*/
				strscpy(io->Channel, io->Parms, sizeof(io->Channel));		
				strscpy(io->Parms, ++iochan, sizeof(io->Parms));
				break;
			}
			aptr = io->Parms; cnt=0;				/* Reset to load again		*/
		}
	} while (*(iochan++) != '\0');

	achr = *io->Parms;									/* Check for quoted string */
	if (achr == '"' || achr == 0x27) {				/* Quoted string */
		aptr = io->Parms+1; bptr = io->Parms;
		while (*aptr != achr && *aptr) *(bptr++) = *(aptr++);
		*bptr = '\0';
	}

/* ----------------------------
--- Open the I/O channel ---
---------------------------- */
/* Find the I/O channel in list of valid names */
	do {
		if (stricmp(io->Channel, lptr->name) == 0) break;
	} while ((++lptr)->name != NULL);

	io->type      = lptr->type;						/* Copy over value		 */
	if (io->Options & IOC_BINARY) imode++;			/* Request switch to "wb" */

Recycle:
	switch (io->type) {

		case IO_LPR:										/* LPR command */
			strcpy(szTmp, io->Parms);
			if (*szTmp == '\0' || stricmp(szTmp, "default") == 0) {
				strcpy(io->Parms, "lpr -h");
			} else if (*szTmp != '-') {
				sprintf(io->Parms, "lpr -P%s", szTmp);
			} else {
				sprintf(io->Parms, "lpr %s", szTmp);
			}
#ifndef NT
			io->type = IO_PIPE;
			goto Recycle;
#else
			io->Abilities |= (IOA_WRITE | IOA_FCLOSE | IOA_SEEK);
			io->funit = SysTmpFile(szTmp, NULL, ".plt", fmode[imode]);
			strcat(io->Parms, " "); strcat(io->Parms, szTmp);
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to open file from name/dir (%s)\n", io->Parms);
			break;
#endif

		case IO_PIPE:
			io->Abilities |= (IOA_WRITE | IOA_PCLOSE);		/* Common abilities */
			io->funit = (FILE *) popen(io->Parms, fmode[imode]);
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to open pipe with command %s\n",io->Parms);
			break;

		case IO_FILE:
			io->Abilities |= (IOA_WRITE | IOA_FCLOSE);		/* Common abilities */

			if (*io->Parms == '\0' || stricmp(io->Parms, "default") == 0) {
				io->funit = SysTmpFile(io->Parms, NULL, ".plt", fmode[imode]);
				io->Abilities |= IOA_SEEK;

			} else if (stricmp(io->Parms, "tty") == 0) {		/* Request to TTY */
				if ( (aptr=ttyname(fileno(stdout))) == NULL) {
					ERRputs("ERROR: Current <stdout> does not have an associated ttyname()\n");
				} else {
					io->funit      = fopen(aptr, dmode[imode]);
					io->Abilities |= (IOA_TERMINAL|IOA_DEVICE|IOA_IGNORE_SETVBUF);
				}

			} else {
				if (stat(io->Parms, &buf) == 0) {				/* Does it exist?	*/
					if (S_ISDIR(buf.st_mode)) {
						io->funit = SysTmpFile(io->Parms, io->Parms, ".plt", fmode[imode]);
						io->Abilities |= IOA_SEEK;
					} else if (S_ISCHR(buf.st_mode)) {			/* It's a device */
						io->funit = fopen(io->Parms, dmode[imode]);
						io->Abilities |= (IOA_READ | IOA_DEVICE);
					} else {
						io->funit = fopen(io->Parms, fmode[imode]);
						io->Abilities |= IOA_SEEK;
					}
				} else {
					io->funit = fopen(io->Parms, fmode[imode]);
					io->Abilities |= IOA_SEEK;
				}
			}
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to open file from name/dir (%s)\n", io->Parms);
			break;

		case IO_GPIB:					/* Must exist and appear to be a device */
			io->Abilities |= (IOA_WRITE | IOA_READ | IOA_DEVICE | IOA_FCLOSE);
			if ((stat(io->Parms, &buf) == 0) && S_ISCHR(buf.st_mode))
				io->funit = fopen(io->Parms, dmode[imode]);
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to find/open GPIB device (%s)\n",io->Parms);
			break;

		case IO_RS232:
			io->Abilities |= (IOA_WRITE | IOA_READ | IOA_DEVICE | IOA_FCLOSE);
			if ((stat(io->Parms, &buf) == 0) && S_ISCHR(buf.st_mode))
				io->funit = fopen(io->Parms, dmode[imode]);
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to find/open asynchronous device (%s)\n", io->Parms);
			break;

		case IO_SPOOL:					/* For simplicity, convert to a pipe model */
#ifdef OS2
			io->Abilities |= (IOA_WRITE | IOA_FCLOSE);		/* Common abilities */
			io->funit = IO_SPL_Open(io->Parms);
			if (io->funit == NULL)									/* Check for errors */
				ERRprintf("ERROR: Unable to open spool queue (%s)\n", io->Parms);
#else
			io->type = IO_PIPE;
			if (*io->Parms == '\0' || stricmp(io->Parms, "default") == 0) {
				strcpy(io->Parms, "lpr -h");
			} else {						/* It is a printer name */
				sprintf(szTmp, "lpr -P%s", io->Parms);
				strscpy(io->Parms, szTmp, sizeof(io->Parms));
			}
			goto Recycle;
#endif	/* CSET2 */
			break;

		default: 
			ERRputs("ERROR: Not only is I/O channel unimplemented - I don't even recognize it\n");
			break;
	}

	if (io->funit == NULL) {						/* Was anything opened? */
		free(io);
		return(NULL);
	}

	IO_SetBuffer(io, bufrequest);
	return(io);
}

/* ===========================================================================
-- The IO_ModifyBuffer attempts to modify the buffer for input and output to
-- the values requested by the module.  It simply closes down an I/O channel
-- opened for output of plot vectors.  Should handle clean closing of buffered
-- output, etc.
--
-- Usage:  void IO_ModifyBuffer(IO_BLOCK *io, int bufsize);
--
-- Inputs: io - pointer to IO_BLOCK
--         ibufsize - if  0, request no buffering
--                    if >0, request specified size buffer
--
-- Output: requests modification of buffer size if device is a TTY
--
-- Returns: none
=========================================================================== */
void IO_SetBuffer(IO_BLOCK *io, int bufsize) {

/* Only modify characteristics on real devices and if we haven't set ignore */
	if ( (io->Abilities & IOA_DEVICE) && !(io->Abilities & IOA_IGNORE_SETVBUF)) {
		if ( (io->Options & IOC_NOBUFFER) || (bufsize == 0) ) {
			setvbuf(io->funit, NULL, _IONBF, 0);
			io->BufSize = 0;
		} else if (io->BufSize > 0) {
			setvbuf(io->funit, NULL, _IOFBF, bufsize);
			io->BufSize = bufsize;
		}
	}
	return;
}

/* ===========================================================================
-- The IO_CloseChannel closes down an I/O channel opened for output of plot
-- vectors.  Should handle clean closing of buffered output, etc.
--
-- Usage:  void IO_CloseChannel(IO_BLOCK *io);
--
-- Inputs: io - pointer to IO_BLOCK obtained from call to IO_OpenChannel.
--
-- Output: *io reset to NULL
--
-- Returns: none
=========================================================================== */
void IO_CloseChannel(IO_BLOCK *io) {

	if (io == NULL) return;
	
	if (io->Abilities & IOA_PCLOSE) {
#ifdef HP_C
		fputc(0x04, io->funit);							/* Send a ^D to end stream */
#endif
		pclose(io->funit);
	} else if (io->Abilities & IOA_FCLOSE) {
		fclose(io->funit);
	}

#ifdef NT
	if (io->type == IO_LPR) {
		char *aptr;
		TTYprintf("Executing: %s\n", io->Parms);
		SysSystem(io->Parms);				/* Execute the command */
		aptr = io->Parms + strlen(io->Parms) - 1;
		while (aptr > io->Parms && ! isspace(*(aptr-1))) aptr--;
		remove(aptr);
	}
#endif

	free(io);									/* And free the control block */
	return;
}

/* ===========================================================================
-- The IO_fprintf and related commands do the equivalent operations out to the
-- I/O channel specified by the IO_BLOCK *io parameter.  Most are specified
-- as macros in io_chan.h.
--
-- Usage:  int IO_fprintf(IO_BLOCK *io, const char *format, ...);
--
-- Inputs: io     - pointer to IO_BLOCK structure
--         format - as in fprintf
--
-- Output: to I/O channel
--
-- Returns: Number of converted items
=========================================================================== */
int IO_fprintf(IO_BLOCK *io, const char *format, ...) {
	int rcode;
	va_list var1;

	va_start(var1, format);
	rcode = vfprintf(io->funit, format, var1);
	va_end(var1);

	return(rcode);
}

/* ===========================================================================
-- The IO_GetInput is the generic read from a device.  It allows for a prompt
-- string to be output to the device, and returns the response from the device.
-- Note that many channels will not be able to implement this function, so must
-- be ready to handle that possibility.
--
-- Usage:   int IO_GetInput(IO_BLOCK *io, char *buf, int nchars, int term, 
--                          char *prompt);
--
-- Inputs:  io     - pointer to IO_BLOCK structure
--          buf    - pointer to a char string for storage of message received
--          nchars - number of characters expected back (maximum)
--          term   - character to be used as the terminator.  If -1, no
--                   terminator is specified and exactly nchars will be
--                   returned.  If not -1, characters will be read, filling
--                   up to nchars-1 positions, until the terminator is seen.
--                   Extra characters are simply dumped.  The final terminator
--                   will be replaced with the '\0' character.
--          prompt - If not NULL, points to a string to be IO_fputs before
--                   requesting input.  If channel cannot do input, prompt will
--                   not be output to the device.
--
-- Output:  buf    - filled with response string
--
-- Returns: Number of characters received or -1 if device is unable to handle
--          input.
=========================================================================== */
int IO_GetInput(IO_BLOCK *io, char *buf, int nmax, int term, char *prompt) {
	
	int i,ngot;

	if (! (io->Abilities & IOA_READ)) return(-1);

	IO_fflush(io);
	if (prompt != NULL) IO_fputs(prompt, io);
	IO_fflush(io);

#ifdef USE_F_READS
	if (term == -1) {
		for (ngot=1; ngot<=nmax; ngot++) *(buf++) = (char) IO_fgetc(io);
	} else {
		ngot = 0;
		term &= 0xFF;
		while ( (*buf = (char) IO_fgetc(io)) != (char) term) 
			if (ngot<nmax) {buf++; ngot++;}
		*buf = '\0';
	}

#else

	tcdrain(fileno(io->funit));					/* Make sure really drained */
	tcflush(fileno(io->funit), TCIFLUSH);		/* And flush anything incoming */

	if (term == -1) {									/* Read unbuffered input		*/
		ngot = nmax;									/* Will have read all of them	*/
		while (nmax > 0) {
			i = read(fileno(io->funit), buf, nmax);
			nmax -= i;
			buf  += i;
		}
	} else {									
		ngot = 0;
		term &= 0xFF;
		while (TRUE) {
			if (read(fileno(io->funit), buf, 1) != 0) {
				if (*buf == (char) term) break;
				if (ngot<nmax) {buf++; ngot++;}
			}
		}
		*buf = '\0';
	}
#endif

	return(ngot);
}

/* ===========================================================================
-- Spooler access routines.  Only useful under CSET/2 as far as I know.
=========================================================================== */
#ifdef CSET2

#ifndef NERR_BufTooSmall
	#define NERR_BufTooSmall	2123			/* Error messages not in OS2.H */
	#define NERR_QNotFound		2150
#endif

#define	STACKSIZE		2048
#define	BUFFERSIZE		16384					/* Big buffer */
#define	MINBUFFERSIZE	4096					/* Minimum for each read	*/

PRIVATE	void		IO_SpoolWork(void *buf2);
PRIVATE	PRQINFO3 *GetQueueInfo(char *pszQueueName);
PRIVATE	char		*GetDefaultQueue(char *queue);

/* ===========================================================================
=========================================================================== */
PRIVATE FILE *IO_SPL_Open(char *Parms) {			

	int *buffer   = NULL;						/* Not allocated   */
	int fildes[2] = {-1, -1};					/* Not loaded		 */
	HSPL hspl     = SPL_ERROR;					/* Not initialized */

	FILE *fc;
	DEVOPENSTRUC data;							/* Pointer to a DEVOPENSTRUC	*/
	PRQINFO3 *prq;
	PSZ pszToken = "*";							/* Spooler info identifier		*/
	char Queue[DFLT_STR_SIZE];					/* Spooler queue					*/

/* ... Look up the correct queue and get parameters from the OS */
	if (Parms != NULL && *Parms != '\0') {
		strscpy(Queue, Parms, sizeof(Queue));
	} else {
		if (GetDefaultQueue(Queue) == NULL) {
			ERRputs("SPOOL: Unable to determine default spool queue\n");
			return(NULL);
		}
	}
	if ( (prq = GetQueueInfo(Queue)) == NULL) {
		ERRprintf("SPOOL: Printer does not exist (%s)\n", Queue);
		return(NULL);
	}

/* Initialize the DEVOPENSTRUC for the queue from info gotten above */
/* Must output as raw data since I don't know the format of the independent */
	data.pszLogAddress      = prq->pszName;			/* Queue name	 */
	data.pszDriverName      = prq->pszDriverName;	/* Driver name	 */
	data.pdriv              = prq->pDriverData;		/* Driver data	 */
	data.pszDataType        = "PM_Q_RAW";				/* Output raw	 */
	data.pszComment         = "Output from Genplot";
	data.pszQueueProcName   = NULL;		/* (IGNORE HERE ON OUT) */
	data.pszQueueProcParams = NULL;		/* (IGNORE HERE ON OUT) */
	data.pszSpoolerParams   = NULL;		/* (IGNORE HERE ON OUT) */
	data.pszNetworkParams   = NULL;		/* (IGNORE HERE ON OUT) */
	
/* ... Now, just get everybody set up and let's go! */
	if ((hspl = SplQmOpen(pszToken, 5L, (PQMOPENDATA) &data)) == SPL_ERROR) {
		ERRputs("SPOOL: SplQmOpen failed to open queue\n");
	} else if ( (buffer=malloc((size_t) BUFFERSIZE)) == NULL) {
		ERRputs("SPOOL: Unable to allocate work buffer\n");
	} else if (pipe(fildes) != 0) {
		ERRputs("SPOOL: Unable to open pipes for spooling process\n");
	} else {
		buffer[0] = fildes[0];						/* Read handle (needed) */
		buffer[1] = fildes[1];						/* Write handle (info)	*/
		*( (HSPL *) (buffer+2)) = hspl;			/* Pass hspl as arg 2	*/
		if (_beginthread(IO_SpoolWork, NULL, STACKSIZE, buffer) == -1) {
			ERRputs("SPOOL: Unable to start work thread\n");
		} else if ( (fc = fdopen(fildes[1], "wb")) == NULL) {
			ERRputs("SPOOL: Unable to convert pipe to stream\n");
		}
	}
	if (fc == NULL) {
		if (hspl != SPL_ERROR) SplQmClose(hspl);
		if (fildes[0] != -1)   close(fildes[0]);
		if (fildes[1] != -1)   close(fildes[1]);
		if (buffer != NULL)    free(buffer);
	}
	return(fc);										/* Return write end of routine */
}

/* ===========================================================================
=========================================================================== */
void IO_SpoolWork(void *buf2) {

	char *buffer;
	int  *ibuf;
	int lunit;
	HSPL hspl;
	unsigned icnt, itotal=0;					/* Read/Write counters			*/

/* ... Copy the file units */
	ibuf  = buf2;									/* Convert to (int *)	*/
	lunit = ibuf[0];								/* Get read pipe handle */
	hspl  = *( (HSPL *) (ibuf+2));			/* Get spooler handle	*/

/* ... Name the output file */
	SplQmStartDoc(hspl, "GENPLOT Output");	/* Mark document name */

/* ... Read from input, copy to output using local buffering */
	buffer = buf2;									/* Convert to (char *) */
	do {
		itotal += (icnt = read(lunit, buffer+itotal, (unsigned) (BUFFERSIZE-itotal)));
		if ( itotal && ( icnt==0 || (BUFFERSIZE-itotal)<MINBUFFERSIZE ) ) {
			if (! SplQmWrite(hspl, itotal, buffer)) {
				fprintf(stderr,"lpr: unable to write to spool file\n");
				return;								/* Thread will exit */
			}
			itotal = 0;
		}
	} while (icnt != 0);

/* ... Close the various units and end spooling */
	close(lunit);									/* Close both sides	*/
	SplQmEndDoc(hspl);							/* End the document	*/
	SplQmClose(hspl);								/* Close the queue	*/
	return;											/* And return			*/
}

/* ===========================================================================
-- Routine to query the SYS.INI files to determine the default queue to use
--
-- Will return a string pointer corresponding to the default queue, or the
-- NULL value.  String may be static or dynamically allocated.
=========================================================================== */
char *GetDefaultQueue(char *queue) {

	char szQueue[DFLT_STR_SIZE], *aptr;

/* ... First, look at the PRINTER environment variable and use if set */
	if ( (aptr = getenv("PRINTER")) != NULL) {
		strcpy(queue, aptr);
		return(queue);
	}

/* ... Otherwise, query the OS2.INI and OS2SYS.INI files for PM_SPOOLER */
	if (! PrfQueryProfileString(HINI_PROFILE, "PM_SPOOLER", "QUEUE", NULL, szQueue, sizeof(szQueue))) {
		ERRprintf("ERROR: No queue stored under PM_SPOOLER in system ini files\n");
		return(NULL);
	}
	if ( (aptr = strchr(szQueue,';')) != NULL) *aptr = '\0';
	strcpy(queue, szQueue);
	return(queue);
}

/* ===========================================================================
=========================================================================== */
PRQINFO3 *GetQueueInfo(char *pszQueueName) {

	int rc;
	ULONG  cbBuf, cbNeeded ;
	PRQINFO3 *prq;
 
	rc = SplQueryQueue(NULL, pszQueueName, 3L, NULL, 0L, &cbNeeded);
	if (rc == NERR_QNotFound) {
		ERRprintf("SPOOL: Specified queue does not exist (%s)\n", pszQueueName);
		return(NULL);
	} else if (rc != ERROR_MORE_DATA && rc != NERR_BufTooSmall) {
		ERRputs("SPOOL: Something terribly wrong - can't get size of PRQINFO3\n");
		return(NULL);
	}
	prq = malloc(cbNeeded);
	cbBuf = cbNeeded ;
	rc = SplQueryQueue(NULL, pszQueueName, 3L, prq, cbBuf, &cbNeeded);
	if (rc != NO_ERROR) {
		ERRprintf("SPOOL: Unable to find queue (%s)\n", pszQueueName);
		free(prq);
		return(NULL);
	}
	return(prq);
}

#endif	/* CSET2 */
