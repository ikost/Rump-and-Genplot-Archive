/* **************************************************************** */
/* xdriver.c - Mike Uttormark 11/15/91                              */
/*                                                                  */
/* driver end of driver/slave pair to drive X Window display        */
/*                                                                  */
/* relatively straight-forward.  employs buffers for points and     */
/* and lines in order to reduce number of pipe reads/writes.        */
/*                                                                  */
/* Now fully re-entrant so "hc dev X" succeeds, though meaningless  */
/* **************************************************************** */

#if (defined OS2 || defined NT)

	#define _POSIX_SOURCE						/* Always require POSIX standard */
	#include "preload.h"

	#include <stdlib.h>
	#include <stdio.h>
	#include "mytypes.h"
	#include "extends.h"
	#include "complot.h"

	typedef struct _DRVBLOCK DRVBLOCK;

	LOGICAL X_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {
		if (key == INIFNC) ERRputs(
			"ERROR: If you really have a X-window hooked up to an OS/2 system\n"
			"       give me a call and maybe we will enable the X-driver\n");
		return(FALSE);
	}

#else

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#define XDRIVER_SLAVE  "xslave.drv"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "complot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
/* #define NOISY */

#ifdef NOISY
	#define REPORT(s,n) fprintf(stderr,s,n);fflush(stderr);
#else
	#define REPORT(s,n)
#endif

#define	POINT_BUF_SIZE	(PIPE_BUF/sizeof(DspPntfnc) - 2)
#define	LINE_BUF_SIZE		(PIPE_BUF/sizeof(DspLinfnc) - 2)

typedef struct _DRVBLOCK {					/* These need to be quasi-static */
	int  running, valid;						/* Is this driver block active?	*/
	FILE *fwrite;								/* Stream converted for writing	*/
	FILE *fread;								/* Stream converted for reading	*/
	pid_t child_id;							/* Child ID of process started	*/

	int point_cnt;								/* # points waiting to be sent	*/
	int line_cnt;								/* # points waiting to be sent	*/
	DspPntfnc points[POINT_BUF_SIZE];
	DspLinfnc lines[LINE_BUF_SIZE];
} DRVBLOCK;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
LOGICAL X_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int pipe2(char *prg, int *io_desc, pid_t *child_id);

static void    shut_down(DRVBLOCK *blk, LOGICAL deallocate);
static LOGICAL flush_buffers(DRVBLOCK *blk, LOGICAL do_pts, LOGICAL do_lines);
static LOGICAL send_key(int key, char *type, DRVBLOCK *blk);
static LOGICAL send_msg(void *buf, size_t size, char *type, DRVBLOCK *blk);
static LOGICAL get_msg(void *buf, size_t size, char *type, LOGICAL flush, DRVBLOCK *blk);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ---------------------------------------------------------------------------
-- X_Driver - Driver end of driver/slave pair to drive X Window display
--            
--
-- Usage: LOGICAL X_Driver
--
-- Inputs: cmd   - Command - See COMPLOT.INS for definitions
--         PARMS - Variable dimensioned array with parameters for transfer
--                 Type and direction depend on command.
--
-- Output: X_Driver - Success of operation
--
-- NOTE: X_Driver is special device.  Parameters handled by special 
--       dispensation.
--
-- Original Coding:  Mike Uttormark 11/15/91
--
-- Notes: Relatively straight-forward.  Employs buffers for points and
--        and lines in order to reduce number of pipe reads/writes.
--------------------------------------------------------------------------- */
LOGICAL X_Driver(INTEGER key, DRVBLOCK *DriverBlock, DSP *dsp) {

	char path[PATH_MAX];
	LOGICAL result, rcode;
	DRVBLOCK *blk;
	int  ierr, desc[2];

	if (key != INIFNC) {
		blk = DriverBlock;							/* Get my local copy */
		if (! blk->valid) return(FALSE);
		if (key != ENDFNC && ! blk->running) return(FALSE);
	}

/* --------------------------------------------------------------------------
-- simple case statement exchanging messages with slave
--
-- In the corresponding slave (xslave.c), only the 
-- following messages will generate a "reply".  Others
-- can simply be "sent" and one merrily marches back.
--   INIFNC  - returns rcode + data block if successful
--   CURFNC  - standard messages
--   CURBOX  - standard messages
--
-- Following messages just assume the data will be there correctly
--   LINFNC, PNTFNC, COLFNC, LWFNC, ERSFNC, FLSFNC, ENDFNC
-------------------------------------------------------------------------- */
	switch (key) {

		case INIFNC:
			blk = malloc(sizeof(DRVBLOCK));			/* Per-instance data */
			if (blk == NULL) return(FALSE);			/* Better succeed		*/

			SysResolveDyntName(path, XDRIVER_SLAVE, sizeof(path));
			if ( (ierr = pipe2(path, desc, &blk->child_id)) != 0) {
				if (ierr == -1) {
					ERRprintf("ERROR: Access X_OK to %s denied (%d)\n", path,errno);
				} else {
					ERRprintf("ERROR: spawn(%s) failed (%d/%d)\n", path,ierr,errno);
				}
				free(blk);
				return(FALSE);
			}
			blk->fread  = fdopen(desc[0], "r");		/* fdopen convert to streams */
			blk->fwrite = fdopen(desc[1], "w");
			blk->valid  = TRUE;							/* Block is allocated/valid */
			blk->running = TRUE;							/* Mark it operational */
			*dsp->ini.DriverBlock = blk;				/* And return pointer */

			result = send_key(key, "init", blk);	/* Send INIT operation */
			if (result) result=send_msg(dsp,sizeof(dsp->ini),"init",blk);
			if (result) result=get_msg(&rcode, sizeof(rcode), "stat", TRUE, blk);
			if (result) result=rcode;
			if (result) result=get_msg(dsp, sizeof(dsp->ini), "init", FALSE, blk);
			if (! result) {
				shut_down(blk, TRUE);					/* Try to kill the child */
				return(FALSE);
			}

			blk->point_cnt = 0;							/* No data pending */
			blk->line_cnt  = 0;
			blk->running   = TRUE;						/* And we are running */
			break;

		case ENDFNC:										/* Shutdown operations */
			shut_down(blk, FALSE);						/* Try to kill the child */
			waitpid(blk->child_id, NULL, 0);			/* Wait for fork() to end */
			free(blk);
			break;

		case LINFNC:
			if (blk->line_cnt >= LINE_BUF_SIZE)		/* output if buffer full */
				flush_buffers(blk, FALSE, TRUE);
			blk->lines[blk->line_cnt++] = dsp->line;
			break;
		
		case PNTFNC:
			if (blk->point_cnt >= POINT_BUF_SIZE)	/* output if buffer full */
				flush_buffers(blk, TRUE, FALSE);
			blk->points[blk->point_cnt++] = dsp->point;
			break;
		
		case COLFNC:
			flush_buffers(blk, TRUE, TRUE);
			send_key(key, "color", blk);
			send_msg(dsp, sizeof(dsp->col), "color" , blk);
			break;

		case LWFNC: /* change line width */
			flush_buffers(blk, TRUE, TRUE);
			send_key(key, "linewidth", blk);
			send_msg(dsp, sizeof(dsp->line), "line width", blk);
			break;

		case ERSFNC:	/* erase screen */
			blk->point_cnt = blk->line_cnt  = 0;
			send_key(key, "erase", blk);
			break;

		case ANMFNC:
			flush_buffers(blk, TRUE, TRUE);
			send_key(key, "anmode", blk);
			break;

		case FLSFNC:	/* flush graphics buffers */
			flush_buffers(blk, TRUE, TRUE);
			send_key(key, "flush", blk);
			if (! get_msg(&result, sizeof(LOGICAL), "status", TRUE, blk))
				return(FALSE);
			if (! result) return(FALSE);
			break;

		case CURFNC:	/* read cursor position */
			if (! send_key(key, "cursor", blk)) return(FALSE);
			if (! get_msg(&result, sizeof(LOGICAL), "status", TRUE, blk))
				return(FALSE);
			if (! result) return(FALSE);
			return(get_msg(dsp, sizeof(dsp->cur), "cursor", FALSE, blk));

		case CURBOX:	/* cursor box */
			if (! send_key(key, "curbox", blk)) return(FALSE);
			if (! get_msg(&result, sizeof(LOGICAL), "status", TRUE, blk))
				return(FALSE);
			if (! result) return(FALSE);
			return(get_msg(dsp, sizeof(dsp->curbox), "cursor box", FALSE, blk));

/* Unimplemented functions - all default FALSE -> unimplemented */
		case FRMFNC:	/* frame */
		case CURTRK:	/* cursor track */
		case IOCTL:		/* special device control */
		case VISFNC:	/* change visibility */
		case TXTFNC:	/* draw text at given position */
		case GRPFNC:	/* graphics screen dump */
		case PAGFNC:	/* new page */
		default:
			return(FALSE);
	}

	return(TRUE);							/* Allows simple break to return okay */
}

/* ===========================================================================
=========================================================================== */
static LOGICAL flush_buffers(DRVBLOCK *blk, LOGICAL do_pnt, LOGICAL do_line) {

	LOGICAL result;
	INTEGER key, cnt;
	size_t nbytes;
	void *buf;

/* Send as long as result TRUE.  Continue running but don't send one FALSE */
	result = blk->running;					/* Start w/ check that we're running */

/* Do the points first */
	if (do_pnt && blk->point_cnt > 0) {			
		key    = PNTFNC;
		buf    = blk->points;
		cnt    = blk->point_cnt;
		nbytes = cnt * sizeof(*blk->points);
		blk->point_cnt = 0;

		if (result) result = send_key(key, "points", blk);
		if (result) result = send_msg(&cnt,sizeof(cnt),"point cnt",blk);
		if (result) result = send_msg(buf, nbytes, "points", blk);
	}

/* And then the lines */
	if (do_line && blk->line_cnt > 0){
		key    = LINFNC;
		buf    = blk->lines;
		cnt    = blk->line_cnt;
		nbytes = cnt * sizeof(*blk->lines);
		blk->line_cnt = 0;

		if (result) result = send_key(key, "lines", blk);
		if (result) result = send_msg(&cnt,sizeof(cnt),"line cnt",blk);
		if (result) result = send_msg(buf, nbytes, "lines", blk);
	}
	return(result);
}


/* ===========================================================================
=========================================================================== */
static void shut_down(DRVBLOCK *blk, LOGICAL deallocate) {

	INTEGER key;

	if (! blk->valid) return;						/* Block is not valid */

	if (blk->running) {								/* Close down if running */
		key = ENDFNC;
		fwrite(&key, sizeof(key), 1, blk->fwrite);
		fclose(blk->fwrite);
		fclose(blk->fread);
		blk->running  = FALSE;						/* Now not running */
	}

	if (deallocate) {									/* Deallocate space used */
	  blk->valid = FALSE;
	  free(blk);
	}
	return;
}
		
/* ===========================================================================
=========================================================================== */
static LOGICAL send_key(int key, char *type, DRVBLOCK *blk) {

	if (! blk->running) return(FALSE);

	if (fwrite(&key, sizeof(int), 1, blk->fwrite) <= 0) {
		ERRprintf(
        "ERROR: Write %s to XSLAVE failed.  Driver shutting down.\n", type);
		shut_down(blk, FALSE);
		return(FALSE);
	}
	fflush(blk->fwrite); 
	REPORT("xdriver: request %s\n", type); 
	return(TRUE);
}


/* ===========================================================================
=========================================================================== */
static LOGICAL send_msg(void *buf, size_t size, char *type, DRVBLOCK *blk) {

	if (! blk->running) return(FALSE);

	if (fwrite(buf, size, 1, blk->fwrite) <= 0) {
		ERRprintf(
        "ERROR: Write %s to XSLAVE failed.  Driver shutting down.\n", type);
		shut_down(blk, FALSE);
		return(FALSE);
	}
	fflush(blk->fwrite);
	REPORT("xdriver: send %s packet\n", type);
	return(TRUE);
}

static LOGICAL get_msg(void *buf, size_t size, char *type, LOGICAL flush, DRVBLOCK *blk) {

	int key, ndum;
	char *abuf;

	if (! blk->running) return(FALSE);

/* ---------------------------------------------------------------------------
-- Don't ask me why this stupid code segment is needed.  As far as I can tell,
-- this seems to be the only way to ensure that the buffers to the pipe are
-- actually flushed and that the driver really gets all of the commands before
-- I try to read the response.  This just has a corresponding segment in the
-- slave code that reads and dumps all these bytes
---------------------------------------------------------------------------- */
	if (flush) {
		ndum = 2*PIPE_BUF;
		key = -1;
		abuf = malloc(ndum);
		if (fwrite(&key, sizeof(int),   1, blk->fwrite) <= 0 || 
			 fwrite(&ndum, sizeof(ndum), 1, blk->fwrite) <= 0 ||
			 fwrite(abuf,  ndum,         1, blk->fwrite) <= 0) {
			ERRprintf(
			 "ERROR: Write %s to XSLAVE failed.  Driver shutting down.\n", type);
			shut_down(blk, FALSE);
			free(abuf);
			return(FALSE);
		}
		free(abuf);
	}

	fflush(blk->fwrite);

	if (fread(buf, size, 1, blk->fread) <= 0) {
		ERRprintf(
		  "ERROR: Read %s from XSLAVE failed.  Driver shutting down.\n",type);
		shut_down(blk, FALSE);
		return(FALSE);
	}
	REPORT("xdriver read %d bytes.\n",size);
	return(TRUE);
}


/* ===========================================================================
-- pipe2.c - Mike Uttormark - 5/23/91
--
-- Function to spawn a sub-process and open pipes to read and write
-- from it.
--
-- Usage: int = pipe2(prg, io_desc, child_id);
--
-- Inputs:   prg = name of program to spawn  (char *)
--
-- Outputs:  io_desc = array of 2 file descriptors for communication
--              io_desc[0] = file descriptor for parent to read from
--              io_desc[1] = file descriptor for parent to write to
--           child_id = process id number of child spawned (pid_t *)
--
-- Return value:  0 if successful
--               -1 access to program failed  (errno set)
--               -2 unable to create pipes    (errno set)
--               -3 fork failed               (errno set)
--               -4 exece failed              (should not happen)
=========================================================================== */
extern char **environ;

static int pipe2(char *prg, int *io_desc, pid_t *child_id) {

   int   filedes1[2];
   int   filedes2[2];
	pid_t child;
   char  *argv[2];

	if (access(prg, X_OK) != 0) {				/* Can I execute it?	*/
		return(-1);
	} else if (pipe(filedes1) != 0) {		/* open one set of pipes */
		return(-2);
	} else if (pipe(filedes2) != 0) {
		close(filedes1[0]);						/* Close unused pipes */
		close(filedes1[1]);
		return(-2);
	}

	*child_id = child = fork();				/* Fork my self */

   if (child == (pid_t) -1) {
		close(filedes1[0]);						/* Close unused pipes */
		close(filedes1[1]);
		close(filedes2[0]);						/* Close unused pipes */
		close(filedes2[1]);
		return(-3);
	}

   /* ************************ */
   /* we're the parent process */
   /* ************************ */
   if (child != (pid_t) 0) {					/* we're the parent      */
      io_desc[0] = filedes1[0];				/* parent reads from 1   */
      io_desc[1] = filedes2[1];				/* parent writes to 2    */
      close(filedes1[1]);						/* close unused ends     */
      close(filedes2[0]);  
      return(0);									/* parent successful     */
	}

   /* *********************** */
   /* we're the child process */
   /* *********************** */
   close(0);										/* child reads from 2    */
   dup(filedes2[0]);
   close(1);										/* child writes to 1     */
   dup(filedes1[1]);  

   close(filedes1[0]);							/* close old descriptors */
   close(filedes1[1]);
   close(filedes2[0]);
   close(filedes2[1]);
      
   argv[0] = prg;									/* construct argv        */
   argv[1] = (char *) 0;

   execve(prg, argv, environ);				/* run child program     */
   return(-4);										/* should never get here */
}

#endif		/* OS2 */
