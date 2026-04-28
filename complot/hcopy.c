/* hcopy.c */

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
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "complot.h"
#include "plotdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic					SysPanic(__FILE__, __LINE__)

typedef enum _RETCODE {
	HC_HELP, HC_ON, HC_OFF, HC_APPEND, HC_PAUSE, HC_END, HC_RESET,
	HC_STATUS, HC_REPLOT, HC_DEVICE, HC_PLOT, HC_UNDO, HC_SAVE, HC_MARK,
	HC_LABEL, HC_LIST, HC_BACKUP
} RETCODE;

typedef struct _CMTYPE {
	CHAR *command;
	INTEGER minlen;
	RETCODE cmd;
} CMTYPE;

typedef struct _SAVE SVTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
void sv_undo(void);						/* Only LEXP and I knows his structure */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE void hc_open_file(SVTYPE *SV);
PRIVATE void hc_reset(SVTYPE *SV, LOGICAL flag);
PRIVATE void hc_parms(SVTYPE *SV);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

PRIVATE const CMTYPE cmlist[] = {
	{"help",		 2,	HC_HELP},		{"?",			-1,	HC_HELP},
	{"on",		 2,	HC_ON},			{"off",		 3,	HC_OFF},
	{"append",	 2,	HC_APPEND},		{"pause",	 2,	HC_PAUSE},
	{"end",		 3,	HC_END},			{"reset",	 3,	HC_RESET},
	{"status",	 4,	HC_STATUS},
	{"replot",	 3,	HC_REPLOT},
	{"device",	 3,	HC_DEVICE},
	{"plot",		 2,	HC_PLOT},
	{"undo",		 4,	HC_UNDO},
	{"save",		 3,	HC_SAVE},
	{"mark",		 2,	HC_MARK},
	{"label",	 3,	HC_LABEL},
	{"list",		 4,	HC_LIST},
	{"backup",	 3,	HC_BACKUP},		{"backspace",-5,	HC_BACKUP},
	{NULL,		 0,	HC_ON} };


/* =============================================================================
--     HCOPY - [ON|OFF|SAVE|PLOT DEV|REDRAW]
--             This routine handles many hardcopy functions for copying
--             graphics screens to another device.
--
--     Usage:     LOG = HCOPY(KEY)
--     Old usage: LOG = HCOPY(KEY,LDEV,LDEVON) - WILL NOT CRASH
--
--     Inputs: KEY    -  0       => Process commands
--                    - U$INIT   => Initialize (does nothing)
--                    - U$RST    => Reset (do nothing)
--                    - U$HELP   => List commands
--                    - U$PARM   => Parameter output
--                    - U$QUIT   => Close off the world - Deletes temp files
--
-- Nov. 16, 1984 - MOT
--     Random blitherings at this point.  What is needed to do this business
--     correctly.  1. At each MARK command, it is necessary to fully save
--     the status of the common blocks so can reestablish the mode.  Probably
--     can be written on the file stack making things a bit more difficult.
--     Alternately, the entire file can be re-executed to return to original
--     status.  2. Backup has to properly handle these strange goings on.
--     3. After a HCOPY PLOT or a HCOPY DEV command, must reestablish the
--     appropriate common block variables.
--
-- Nov. 21, 1984 - MOT
--     SAVPOS array moved to common block in SVKEYS.INS so SAVE routine can
--     reset all pointers to 0 after an erase.
--
-- Dec. 16, 1984 - MOT
--     Added function in HCOPY APPEND to mark any IN command blocks as MARK
--      points.  Can backspace through the file thus!
--     Added the LIST and LABEL commands for user interface.  MARK is obsolete.
--
-- Mar. 4, 1985 - MOT
--     Changed philosophy on handling EOF in the file.  Changed to backspacing
--     back over them before moving on in the code.  Necessitated by the IBMPC.
--
-- Mar. 21, 1985 - MOT
--     Modified END to query for file deletion
--
-- Sep. 10, 1985 - MOT
--     Eliminated call to GPUNIT - ATTDEV in GUNT$F added
--
-- May 9, 1987 - MOT
--     Removed PRIME specific code for handling backups - Fails at Rev. 21.
============================================================================ */
LOGICAL HardCopy(INTEGER key) {

	int i;
	char token[DFLT_STR_SIZE], pathname[PATH_MAX];
	CMTYPE *citem;
	LOGICAL CurrentDevice, WasOn;
	PLT_DEVICEINFO *OldDevice;
	PLT_WINDOWINFO *OldWindow;
	FILE *iunit;
	SVTYPE *SV;
	long i4;

	SV = &PlotWindow->Save;

	switch (key) {
		case U_INIT:		/* Initialize and reset */
			return(TRUE);

		case U_RESET:		/* Reset parameters */
			hc_reset(SV, FALSE);
			return(TRUE);

		case U_HELP:	/* Out portion of the help list */
			LexCmdlPrint(cmlist, sizeof(CMTYPE), "HCOPY commands:");
			return(TRUE);
				
		case U_PARM:	/* Our portion of the parameter list */
			hc_parms(SV);
			return(TRUE);
			
		case U_QUIT:	/* Out shutdown responsibility */
			HardCopyCloseSave();
			return(TRUE);

		case 0:
			if (! LexGetTokenP(token, sizeof(token), "HCOPY: (Help) ")) 
				strcpy(token, "HELP");
			if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) == NULL) {
				gen_err2("Bad HCOPY command", token);
				return(FALSE);
			}
			switch (citem->cmd) {

/* -------------------------------------------
-- ... Help command - Again as usual for me, minimal
------------------------------------------- */
				case HC_HELP:	/* Help command - Again as usual for me, minimal */
					LexCmdlPrint(cmlist, sizeof(CMTYPE), "HCOPY commands:");
					return(TRUE);

/* -------------------------------------------
-- ... Here for on
------------------------------------------- */
				case HC_ON:					/* Here for on */
					if (SV->Unit != NULL) {
						strcpy(SV->savcur, "HCOPY back on") ;
						SV_Request(SVKEY_ON | SVKEY_WRITE_COMMON, NULL);
					} else {
						hc_open_file(SV);
					}
					return(SV->On);
						
/* -------------------------------------------
-- ... Here for off
------------------------------------------- */
				case HC_OFF:					/* Here for off */
				case HC_PAUSE:					/* Pause is equivalent */
					SV_Request(SVKEY_OFF, NULL);
					return(TRUE);

/* -------------------------------------------
-- ... Here for end of hcopy - Close down file 
-- ... completely for later restart of a new one
------------------------------------------- */
				case HC_END:						/* Here for closing file */
					HardCopyCloseSave();
					return(TRUE);
					
/* -------------------------------------------
-- ... Here for a reset
------------------------------------------- */
				case HC_RESET:						/* Here for a reset */
					hc_reset(SV, TRUE);			/* Reset and reopen if not */
					return(SV->Unit != NULL);

/* -------------------------------------------
-- ... Here to plot another file on this unit
------------------------------------------- */
				case HC_PLOT:					/* Here to plot a file on this device */
					if (! DEVICE->Initialized) {		/* Check if device on */
						gen_err("No device initialized.  Initialize one first");
						return(FALSE);
					} else if (! LexGetFileP(pathname, sizeof(pathname), "File: (abort) ")) {
						return(TRUE);
					}

					if (access(pathname, R_OK) != 0) {
						SysAddExt(pathname, ".hcp");
						if (access(pathname, R_OK) != 0) {
							gen_err2("Requested HCOPY file does not exist", pathname);
							return(FALSE);
						}
					}
					if ( (iunit = fopen(pathname, "rb")) == NULL) {
						gen_err2("Error opening HCOPY plot file",pathname);
						return(FALSE);
					}
					fprivate(iunit);
					
					OldWindow = PlotWindow;
					PlotWindow = NULL;
					if (! PlotAllocateWindow((void **)&PlotWindow)) {
						gen_err("Unable to allocate new plot window");
						fclose(iunit);
						PlotWindow = OldWindow;
						return(FALSE);
					}

					SV_Plot(iunit, TRUE);						/* Plot with new frame */

					fclose(iunit);
					free(PlotWindow);								/* And reset window */
					PlotWindow = OldWindow;
					PlotConformDevice();
					return(TRUE);
					
/* ------------------------------------------------------------
-- ... Here to close the file, and save the vectors elsewhere.
-- ... If the HC was on, it will be reinitialized.
------------------------------------------------------------ */
				case HC_SAVE:				/* Here to close file and save vectors */

					if (SV->Unit == NULL) {
						gen_err("No HCOPY file to save"); 
						return(FALSE);
					}

					WasOn = SV->On;
					SV_Request(SVKEY_SAVE, NULL);			/* Save the sucker */

					if (LexGetFileP(pathname, sizeof(pathname), "Filename: (default) ") &&
						strcmp(pathname, "/") != 0) {
						SysAddExt(pathname, ".hcp");
						switch (SysMoveFile(SV->Name, pathname)) {
							case 0:
								break;
							case 1:
								ERRprintf("ERROR: Cannot overwrite existing file %s\n"
											 "       File saved as: %s\n", pathname, SV->Name);
								break;
							default:
								ERRprintf("ERROR: Both rename() and copy() failed on %s\n", 
											 "       File saved as: %s\n", pathname, SV->Name);
						}
					} else {
						TTYprintf("INFO: Hard copy file saved as %s\n", SV->Name);
					}

					if (WasOn) hc_open_file(SV);				/* Just restart it	*/
					return(TRUE);

/* ------------------------------------------------------------------
-- ... APPEND - Open a file which already has vectors and continue on
------------------------------------------------------------------ */
				case HC_APPEND:					/* Here to open file for append */
					if (SV->Unit != NULL) {
						ERRprintf("ERROR: HCOPY already collecting %s\n"
							       "       Use HCOPY END to terminate current file first\n",
									 SV->Name);
						return(FALSE);
					}
					if (! LexGetFileP(SV->Name, sizeof(SV->Name), "Append File: (abort) "))
						return(TRUE);
					SysAddExt(SV->Name, ".hcp");
					if ( (iunit = fopen(SV->Name, "a+b")) == NULL) {
						gen_err2("Failed opening HCOPY APPEND file", SV->Name);
						return(FALSE);
					}
					fprivate(iunit);
					SV_Request(SVKEY_APPEND, iunit);
					return(SV->On);

/* ----------------------------------------------------------------------
-- ... Replot on current DEVICE - only goes to current device.
---------------------------------------------------------------------- */
				case HC_REPLOT:
					if (SV->Unit == NULL) {
						gen_err("No HCOPY file exists"); 
						return(FALSE);
					}
					SV_Plot(SV->Unit, TRUE);		/* Replot it				*/
					return(TRUE);

/* ----------------------------------------------------------------------
-- ... Replot on another DEVICE - closes down current device for another.
-- ... Resets device to this one after done so can continue.
---------------------------------------------------------------------- */
				case HC_DEVICE:			/* Here to replot on another device */

					if (SV->Unit == NULL) {
						gen_err("No HCOPY file exists"); 
						return(FALSE);
					}

					CurrentDevice = TRUE;
					if (LexGetTokenP(token, sizeof(token), "Plotter (SAME): "))
						CurrentDevice = stricmp(token, "/") == 0;

					if (! CurrentDevice) {
						OldDevice = DEVICE;
						DEVICE    = NULL;
						PlotAllocateDevice((void **) &DEVICE);		/* Ignore error */
						if (PlotSelectDevice(token, NULL) != 0) {
							gen_err("Bad device name or none specified");
							free(DEVICE);
							DEVICE = OldDevice;
							return(FALSE);
						}
					}
					SV_Plot(SV->Unit, TRUE);		/* Replot it				*/
					if (! CurrentDevice) {
						PlotCloseDevice();
						free(DEVICE);
						DEVICE = OldDevice;
					}
					return(TRUE);

/* --------------------------------------------------------------
-- ... Mark the file so can return to this same point again later
-- ... Mark and label the point (text 10 characters saved here)
-------------------------------------------------------------- */
				case HC_MARK:			/* Here to mark the file */
				case HC_LABEL:			/* Here to label and mark the file */
					if (SV->Unit == NULL) {
						gen_err("No active HCOPY file");
						return(FALSE);
					}
					if (citem->cmd != HC_LABEL ||
						(! LexGetTokenP(SV->savcur, sizeof(SV->savcur), "Label of current mark: ")))
						strcpy(SV->savcur, "Unlabeled mark");

					SV_Request(SVKEY_FLUSH, NULL);
					if (SV->Marks[SVNSAVES-1].posn != -1) 
						gen_warn("Oldest HCOPY MARK fell off the end of the earth");
					for (i=SVNSAVES-1; i>=1; i--) SV->Marks[i] = SV->Marks[i-1];
					SV->Marks[0].posn = ftell(SV->Unit);
					strcpy(SV->Marks[0].savcur, SV->savcur);
					TTYprintf("  Position: %8.8lx Text: %s\n", SV->Marks[0].posn, SV->Marks[0].savcur);
					SV_Request(SVKEY_WRITE_COMMON, NULL);
					return(TRUE);

/* --------------------------------------------
-- ... Backup the file to last saved position
-------------------------------------------- */
				case HC_BACKUP:			/* Here to backup file to saved position */
					if (SV->Unit == NULL) {
						gen_err("You is crazy - No HCOPY file exists");
						return(FALSE);
					}
					SV_Request(SVKEY_FLUSH, NULL);		/* Close out current			*/ 
					i4 = ftell(SV->Unit);					/* Get Current position		*/
					if (i4 == SV->LastRestorePosn) {		/* Same as at last backup?	*/ 
						for (i=0; i<SVNSAVES-1; i++) 
							SV->Marks[i] = SV->Marks[i+1];
						SV->Marks[SVNSAVES-1].posn = -1;
					}
					fseek(SV->Unit, SV->Marks[0].posn, SEEK_SET);
					SV_Request(SVKEY_READ_COMMON, NULL);
					ftrunc(SV->Unit, -1L);
					SV->LastRestorePosn = ftell(SV->Unit);
					TTYprintf(" Returning from %8.8lx to %8.8lx : %s\n",
						i4, SV->Marks[0].posn, SV->Marks[0].savcur);
					return(TRUE);

/* ---------------------------------------------------------
-- ... List report - All marks and current text associated
--------------------------------------------------------- */
				case HC_LIST:					/* List all marks and text in file */
					TTYputs("\nCurrent saved points and text: \n");
					for (i=0; i<SVNSAVES; i++) {
						if (SV->Marks[i].posn == -1) break;
						TTYprintf("  Position: %8.8lx Text: %s\n", SV->Marks[i].posn, SV->Marks[i].savcur);
					}
					TTYputs("\n");
					return(TRUE);
					
/* ---------------------------------------------------------
-- ... Status report
--------------------------------------------------------- */
				case HC_STATUS:					/* Here for status report */
					hc_parms(SV);
					return(TRUE);
					
/* ----------------------
-- ... Undo last command
---------------------- */
				case HC_UNDO:					/* Here to undo the last command */
					if (SV->Unit!=NULL && SV->undo_pos>=0) {			/* Enabled? */
						fseek(SV->Unit, SV->undo_pos, SEEK_SET);		/* Reset posn */
						ftrunc(SV->Unit, -1L);								/* Truncate */
						SV->svptr = SV->block;								/* Erase SVPTR */
						SV->curr_pos = -1;									/* Current position */
						sv_undo();												/* MUST LOAD HERE! */
						if (! LexGetOption(token, sizeof(token))) 
							SV_Plot(SV->Unit, TRUE);
					} else {
						ERRprintf("CHITCHAT: Let's talk - I allow UNDO only once and only if HCOPY is on\n");
					}
					return(TRUE);

				default:
					return(FALSE);
			}
	}
	return(FALSE);
}

/* ------------------------------------------------------------------------- */
PRIVATE void hc_open_file(SVTYPE *SV) {

	FILE *iunit;

	SV_Request(SVKEY_CLEAR, NULL);				/* Make sure it thinks it is off */
	if ( (iunit = SysTmpFile(SV->Name, NULL, ".hcp", "w+b")) == NULL) {
		ERRprintf("ERROR: Failed to open HCOPY file (%s)\n", SV->Name);
		return;
	}
	fprivate(iunit);
	SysQualifyPath(SV->Name, SV->Name, sizeof(SV->Name));
	SV_Request(SVKEY_INITIALIZE, iunit);
	return;
}


/* ------------------ Status Report ------------------ */
PRIVATE void hc_parms(SVTYPE *SV) {

	if (SV->Unit != NULL) {
		TTYprintf(" Hard copy file: %s\n", SV->Name);
		if (SV->On) 
			TTYputs("   Save of vectors enabled\n");
		else
			TTYputs("   Save of vectors disabled\n");
	} else
		TTYputs(" Hard copy not initialized\n");
	return;
}

/* ------------------------------------------------------------------------- */
PRIVATE void hc_reset(SVTYPE *SV, LOGICAL flag) {
	
	if (SV->Unit != NULL) {											/* Have a file set? */
		if (SV_Request(SVKEY_INITIALIZE, SV->Unit) != 0) {	/* Open initialize */
			gen_err2(" Unable to reinitialize hard copy file", SV->Name);
			SV_Request(SVKEY_CLEAR, NULL);						/* Turn off hcopy */
		} else {
			TTYprintf(" Hard copy file reset: %s\n", SV->Name);
		}
	} else if (flag) {
		hc_open_file(SV);
	}
	return;
}

/* ------------------------------------------------------------------------- 
-- PlotShutDown uses this call also
--
-- 4/22/94 - removed query if HCOPY file is really to be closed and deleted.
--             if (! LexYesNo(TRUE, "Delete HCOPY file? (YES) "))
--			         TTYprintf("HCOPY file saved as %s\n",SV->Name);
--
-- Modified to make two attempts, with a 1 second sleep, due to NT problems.
--------------------------------------------------------------------------- */
void HardCopyCloseSave(void) {

	SVTYPE *SV;
	int rcode;

	SV = &PlotWindow->Save;

	if (SV->Unit != NULL) {
		SV_Request(SVKEY_SAVE, NULL);				/* Close down and save file */

		rcode = remove(SV->Name);
		if (rcode != 0) {sleep(1); rcode = remove(SV->Name);}
		if (rcode != 0) {
			ERRprintf("WARNING: Unable to remove HCOPY file %s\n", SV->Name);
		} else {
			TTYprintf("Deleted HCOPY file %s\n", SV->Name);
		}
	}
	return;
}
