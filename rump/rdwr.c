/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/* rdwr.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifdef NT
	#include <windows.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	int  rcode;
} CMTYPE;

typedef struct _RDWR_PROC {
	char path[PATH_MAX];							/* DLL path when loaded				*/
	char *ID;										/* Short ID text						*/
	char *Description;							/* Basic description of routine	*/
	char *ExtList;									/* ; delimited extension list		*/
	int   bOptions;								/* Binary options list				*/
	SPECTRUM *(*ReadProc)(FILE *funit, CHAR *path, SPECTRUM *spectra, int *err);
	int (*WriteProc)(FILE *funit, CHAR *path, SPECTRUM *spectra);
	struct _RDWR_PROC *next;
} RDWR_PROC;

static RDWR_PROC *RdwrProcList=NULL,		/* List of known rdwr procedures		*/
					  *DefaultRdwrProc=NULL,	/* procedure to use if ext unknown	*/
					  *ForceRdwrProc=NULL,		/* override via command line option	*/
					  *ASCIIRdwrProc=NULL,		/* ASCII procedures info				*/
					  *XLSRdwrProc=NULL,			/* Excel (tab) procedures info		*/
					  *RUMPRdwrProc=NULL;		/* RUMP binary procedures info		*/

#define	PIPE_CHAR	'!'		/* Replacement char for pipe reads */

#define	SIMPLEFILE	0			/* Constants for specifying file type */
#define	PIPEFILE		1

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void LoadRdwrProcs(void);
static void FreeRdwrProcs(void);
static FILE *MyOpenFileForRead(char *path, char *mode, int *err, int *FileType);
static FILE *MyOpenFileForWrite(char *path, char *mode, int *err, int *FileType);
static void MyCloseFile(FILE *funit, int mode);

static void GetExtType(char *path, char *ext, int len);
static BOOL IsExtInList(char *ext, char *list);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Routine to find a given pathname within the already defined spectra.
-- Returns integer index in buffer to the spectrum if found, or -1 if
-- it doesn't exist.
--
-- Inputs: key - 0 ==> Must be exact match of full pathname
--         key - 1 ==> Filename component match only required
=========================================================================== */
int RbsFindBuffer(int key, char *path) {

	int i;
	char *fptr, *fptr_path;

	if (key == 0) {
		for (i=1; i<=RbsNumBuf; i++) {
			if (RbsBuffers[i] == NULL) continue;
			if (stricmp(RbsBuffers[i]->filename, path) == 0) return(i);
		}
	} else {
		SysMarkPath(path, &fptr_path, NULL);
		for (i=1; i<=RbsNumBuf; i++) {
			if (RbsBuffers[i] == NULL) continue;
			SysMarkPath(RbsBuffers[i]->filename, &fptr, NULL);
			if (stricmp(fptr, fptr_path) == 0) return(i);
		}
	}
	return(-1);
}

static char WriteHelp[]=
"\n"
" Command to write user RBS data files in various formats.\n"
"\n"
"   WRITE <path | [buffer name]> [-options]\n"
"\n"
" Buffer names:\n"
"    /                       Default return value\n"
"    main                    Buffer 1\n"
"    last                    Current buffer\n"
"    active | alternate      Current buffer\n"
"    simulation | theory     Simulation buffer\n"
"    splot | tmpbuf | -1     Temporary buffer\n"
"\n"
" Options:\n"
"   -binary                  Write using default RUMP format (default)\n"
"   -ascii [-1col | -2col]   Write using ASCII format reader\n"
"   -txt | -xls              Write using tab delimited Excel format\n"
"   -format <module ID>      Write using specific module ID\n"
"   -nq | -noquery           Overwrite silently if file exists\n"
"   -force | -silent         Overwrite silently if file exists\n"
"\n"
" The format of the write will be determined by (1) option specifying a format,\n"
" (2) default based on extension of filename, (3) format file first read, or (4)\n"
" RUMP program default.\n";

static char ReadHelp[]=
"\n"
" Primary command to read user RBS data files - in various formats.\n"
"\n"
"   READ <path | [buffer name]> [-options]\n"
"\n"
" Buffer names:\n"
"    /                       Default return value\n"
"    main                    Buffer 1\n"
"    last                    Current buffer\n"
"    active | alternate      Current buffer\n"
"    simulation | theory     Simulation buffer\n"
"    splot | tmpbuf | -1     Temporary buffer\n"
"\n"
" Options:\n"
"   -binary                  Read using default RUMP format (default)\n"
"   -ascii [-1col | -2col]   Read using ASCII format reader\n"
"   -txt | -xls              Read using tab delimited format reader\n"
"   -format <module ID>      Read using specific module ID\n"
"   -configuration           Enumerate all available modules\n"
"   -release | -free         Release and reload all read/write modules\n"
"\n"
" Filenames may be specified as piped, compressed, or inline format.  gzip'd\n"
" files (extension .gz) are automatically recognized and read as via the pipe.\n"
" An .rbs extension will be assumed if the file does not exist and no extension\n"
" present.\n";


/* ===========================================================================
--  Usage Guide:
--
--  Quick: Scan input for a valid buffer or filename
--
-- Usage:  SPECTRUM *RbsGetBuf(char *prompt, SPECTRUM *idflt);
--
-- Inputs: prompt - prompt to user for file or buffer
--         idflt  - default buffer to return for no response or "/"
--
-- Output: none - Error messages to screen only
--
-- Return: Returns pointer to last spectrum requested (if it existed).
--           o If file already exists in buffer, that buffer pointer returned
--           o If file is read, buffers scrolled with this becoming MAINBUF
--           o If wildcard files read, last valid returned as MAINBUF
--           o ERRORS: Returns NULL
--
-- Errors:  Error messages are reported directly to screen.  Only return
--          error code is the NULL pointer.
--
--     COMMON BLOCKS:     RUMP, GRAPHICS
--
--     CALLED FROM:       APLOT, BMANIP, BFPROC
--
--     CALLS:             RDFILE
=========================================================================== */
SPECTRUM *RbsGetBuf(char *prompt, SPECTRUM *idflt) {

	static CMTYPE cmlist[] = {
		{"/",				1, 0},					/* Default value			*/
		{"main",			2, 1},					/* Use buffer 1			*/
		{"last",			2, 2},					/* Use current buffer	*/
		{"now",			1, 2},					/* Use current buffer	*/
		{"active",		3, 2},					/* Use current buffer	*/
		{"alternate",	3, 3},					/* Use alternate buffer */
		{"simulation",	3, 3},					/* Use simulation buffer */
		{"theory",		2, 3},					/* Use theory buffer		*/
		{"splot",		5, 4},					/* Get TMPBUF buffer		*/
		{"tmpbuf",		6, 4},					/* Get TMPBUF buffer		*/
		{"-1",			2, 4},					/* Get TMPBUF buffer		*/
		{NULL,			0, 0} };
	CMTYPE *citem;

	int i, NumRead, GotOne;
	char achr, *aptr, *fileptr, *dirend, *searchdir;
	char path[PATH_MAX], wild[PATH_MAX], newpath[PATH_MAX], orgpath[PATH_MAX];
	char token[OPTION_STR_SIZE];
	SPECTRUM *ibf;

	DIR *dir;
	struct dirent *entry;

/* First, look for a help request */
	if (LexCheckHelp("Read", ReadHelp, NULL)) return(NULL);

/* If not done before, load all valid read/write procedures */
	if (RdwrProcList == NULL) LoadRdwrProcs();

/* Get a "path" and check quickly for defaults */
	if (! LexGetFileP(path, sizeof(path), prompt)) return(idflt);
	if (*path == '-' && stricmp(path, "-1") != 0) {
		LexBackup();
		*path = '\0';
	}

/* Check for options */
	ForceRdwrProc = NULL;
	while (LexGetOptionEx(token, sizeof(token), "-")) {
		if (LexEqual(token, "-ASCII", 4)) {
			ForceRdwrProc = ASCIIRdwrProc;
		} else if (LexEqual(token, "-BINARY", 4)) {
			ForceRdwrProc = RUMPRdwrProc;
		} else if (LexEqual(token, "-XLS", 4) || LexEqual(token, "-Excel", 6)) {
			ForceRdwrProc = XLSRdwrProc;
		} else if (LexEqual(token, "-FORMAT", 5)) {
			if (LexGetTokenP(token, sizeof(token), "Data file format (use READ -CONFIG to list): ")) {
				for (ForceRdwrProc = RdwrProcList; ForceRdwrProc!=NULL; ForceRdwrProc = ForceRdwrProc->next) {
					if (stricmp(token, ForceRdwrProc->ID) == 0) break;
				}
				if (ForceRdwrProc == NULL) {
					ERRprintf("ERROR: \"%s\" does not exist as a read/write format.\n", token);
					return(NULL);
				}
			}
		} else if (LexEqual(token, "-RELEASE", 8) || LexEqual(token, "-FREE", 5)) {
			FreeRdwrProcs();
			return(NULL);
		} else if (LexEqual(token, "-CONFIGURATION", 7)) {
			RDWR_PROC *tmp;
			if (RdwrProcList == NULL) LoadRdwrProcs();
			for (tmp=RdwrProcList; tmp!=NULL; tmp=tmp->next) {
				TTYprintf("  Routine ID: %s\n"
							 "     Description: %s\n"
							 "     Extensions: %s\n"
							 "     Options: %-4x    Read/Write:  %p/%p\n"
							 "     DLL Source: %s\n",
							 tmp->ID, tmp->Description, 
							 tmp->ExtList, 
							 tmp->bOptions, 
							 tmp->ReadProc, tmp->WriteProc, 
							 (*tmp->path == '\0') ? "<internal default>" : tmp->path);
			}
			TTYprintf("  Default routine is %s at %p\n", DefaultRdwrProc->ID, DefaultRdwrProc->ReadProc);
			return(NULL);
		} else {
			LexBackup();
			break;
		}
	}

/* If don't have a name yet, try once more */
	if (*path == '\0' && ! LexGetFileP(path, sizeof(path), prompt)) return(idflt);

/* Check for special names.  Notice new lengths in parse interpreter */
	if ( (citem = LexCmdl(path, cmlist, sizeof(CMTYPE))) != NULL) {
		switch (citem->rcode) {
			case 0:
				return(idflt);							/* Default			*/
			case 1:
				return(MAINBUF);						/* use mainbuffer	*/
			case 2:
				return(ibuf);							/* reuse current	*/
			case 3:
				return(ALTBUF);						/* use theory		*/
			case 4:
				return(TMPBUF);						/* use temporary	*/
		}
	}

/* Try it as a buffer number (0-n) */
	i = strtol(path, &aptr, 10);
	if (*aptr == '\0') {								/* Was it a number? */
		if (i >= 0 && i <= RbsNumBuf) return(RbsBuffers[i]);
	}

/* First, handle pipes if anyone is so foolish */
	if (*path == '|' || *path == PIPE_CHAR) {
		if ( (ibf = RbsRdFile(path)) == NULL) {
			ERRprintf("OUCH! Pipe mode read %s requested but did not succeed\n", path);
			return(NULL);
		}
		RbsBufferScroll(ibf);
		return(MAINBUF);
	}
	
/* Otherwise, split between wild cards and no wildcards/archive */
	SysMarkPath(path, &fileptr, &dirend);
	if (SysFileCheck(fileptr, NULL, NULL) != 3) {		/* Not wildcarded */
		strcpy(orgpath, path);
		if (access(path, R_OK) != 0) SysAddExt(path, ".rbs");
		SysQualifyPath(path, path, sizeof(path));
		if ( (i = RbsFindBuffer(0, path)) != -1) {
			ibf = MAINBUF;							/* Exchange main and found */
			MAINBUF = RbsBuffers[i];			/* Okay even if the same	*/
			RbsBuffers[i] = ibf;
			TTYprintf("File active: %s\n", path);
			return(MAINBUF);
		} else if (strstr(path, "::") != NULL) {
			if ( (ibf = RbsRdFile(path)) == NULL) {
				ERRprintf("OUCH! ZIP archive read %s requested but did not succeed\n", path);
				return(NULL);
			}
			RbsBufferScroll(ibf);
			return(MAINBUF);
		} else if (SysFindFileGz(newpath, orgpath, RbsSearchPath, RbsSearchExts, R_OK)) {
			SysQualifyPath(newpath, newpath, sizeof(newpath));
			if ( (i = RbsFindBuffer(0, newpath)) != -1) {
				ibf = MAINBUF;
				MAINBUF = RbsBuffers[i];
				RbsBuffers[i] = ibf;
				TTYprintf("File active: %s\n", newpath);
				return(MAINBUF);
			} else {
				if ( (ibf = RbsRdFile(newpath)) == NULL) {
					ERRprintf("OUCH! \"%s\" exists, but read failed\n", newpath);
					return(NULL);
				}
				RbsBufferScroll(ibf);
				return(MAINBUF);
			}
		} else if ( (i = RbsFindBuffer(1, path)) != -1) {	/* Try name only */
			ibf = MAINBUF;
			MAINBUF = RbsBuffers[i];
			RbsBuffers[i] = ibf;
			TTYprintf("Filename match: %s\n", MAINBUF->filename);
			return(MAINBUF);
		} else {										/* RbsRdFile puts out error msg	*/
			if ( (ibf = RbsRdFile(orgpath)) == NULL) {
				ERRprintf("ERROR: %s not found\n", orgpath);
				return(NULL);							/* Invalid name						*/
			}
			RbsBufferScroll(ibf);
			return(MAINBUF);
		}
	}
		
/* Here for any wildcard read -- open directory and check all */
	SysAddExt(path, ".rbs");					/* Wildcards must assume .rbs		*/
	SysMarkPath(path, &fileptr, &dirend);	/* Locate file and directory end */
	strcpy(wild, fileptr);						/* Wild card string					*/ 
	achr = *dirend;								/* Save character at end of dir	*/
	*dirend = '\0';								/* Mark end of directory			*/
	searchdir = (*path != '\0') ? path : ".";
	if ( (dir = opendir(searchdir)) == NULL) {
		ERRprintf("ERROR: Unable to open %s for directory search\n", searchdir);
		return(NULL);
	}
	*dirend = achr;								/* Restore character					*/

	NumRead = 0;									/* Number of files read				*/
	GotOne  = FALSE;
	while ( (entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||  strcmp(entry->d_name, "..") == 0 
			 || (! SysCheckMatch(entry->d_name, wild)) ) continue;
		strcpy(fileptr, entry->d_name);		/* Construct full name				*/
		SysQualifyPath(newpath, path, sizeof(newpath));
		if ( (i = RbsFindBuffer(0, newpath)) != -1) {
			ibf = MAINBUF;							/* Exchange main and found */
			MAINBUF = RbsBuffers[i];			/* Okay even if the same	*/
			RbsBuffers[i] = ibf;
			GotOne  = TRUE;						/* Admit finding one here	*/
			TTYprintf("File active: %s\n", newpath);
		} else if (NumRead >= RbsNumBuf) {
			ERRprintf("WARNING: Maximum number of RBS files read\n");
			break;
		} else {
			if ( (ibf = RbsRdFile(newpath)) != NULL) {
				NumRead++;
				GotOne  = TRUE;
				RbsBufferScroll(ibf);
			}
		}
	}
	closedir(dir);
	if (GotOne) return(MAINBUF);
		ERRprintf("ERROR: No files matching specification (%s)\n", wild);
		return(ibuf);
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE SCROLL
--  Quick: Scrolls buffers forward, leaving MAIN as last
--
--     INPUTS:   Pointer to replace MAIN (can be NULL)
--
--     OUTPUTS:  Scrolled buffers.
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       BMANIP, RDFILE, TRANSF
--     CALLS:             None
--
-- Scroll will now scroll the RbsNumBuf buffers constituting user space.  If 
-- the last one is defined, it will be deleted.  First one will be set to the
-- passed SPECTRUM pointer.
=========================================================================== */
void RbsBufferScroll(SPECTRUM *ibf) {

	int i;

	if (RbsBuffers[RbsNumBuf-1] != NULL) RbsFreeSpectrum(RbsBuffers[RbsNumBuf]);
	for (i=RbsNumBuf; i>1; i--) RbsBuffers[i] = RbsBuffers[i-1];
	MAINBUF = ibf;
	return;
}

/* ===========================================================================
-- Usage Guide:
--
--  SUBROUTINE RbsActive(ibf)
--
--  Quick: Routine to print out the header from specified spectrum structure
--
--  Usage:  void RbsActive(SPECTRUM *spectra)
--
--  Inputs: *spectra - spectrum to print information concerning
--
--  Output: Prints header block to standard output
--     Filename:    c:\rump\example.rbs
--     Identifier:  Ni/NiSi/Si Annealed 90 min 295^~o^+C
--     LTCT Text:   LT= 857 CT= 860
--     Date:        18-JUN-1985 12:33:48.48
--     Beam:        3.000 MeV   4He++   xxxx.yy uCoul  @ xx.yy nA
--     Geometry:    General  Theta: -180.0  Phi: -109.0  Psi: +000.0
--     MCA:         Econv: xxx.yyy  xxx.yyy  First chan:  0.0  NPT: 1024
--     Detector:    FWHM: 35.0 keV   Omega: 3.400
--     Correction:  1.0041
--
--  Common Blocks:     RUMP
--  Called From:       BMANIP
--  Calls:             None
=========================================================================== */
void RbsActive(SPECTRUM *buf) {

	char *geometry, *filetype;
	char beam_string[10];

/* Create string of form 4He++ for the incident beam */
	RbsBeamCode(buf, beam_string);

/* Look and convert geometry ID into the appropriate string */
	if (buf->geom == CORNELL) {
		geometry = "Cornell";
	} else if (buf->geom == IBM) {
		geometry = "IBM";
	} else if (buf->geom == GENERAL) {
		geometry = "General";
	} else {
		geometry = "Unknown";
	}

/* Look and convert spectrum type into an appropriate string */
	if (buf->type == RBS) {
		filetype = "RBS";
	} else if (buf->type == FRES) {
		filetype = "FRES";
	} else if (buf->type == PIXE) {
		filetype = "PIXE";
	} else {
		filetype = "Unknown";
	}

	TTYprintf(
		"  %-4s File:   %s\n"
		"  Identifier:  %s\n"
		"  LTCT Text:   %s\n"
		"  Date:        %s\n"
/**     Beam:        3.000 MeV   4He++   xxxx.yy uCoul  @ xx.yy nA         **/
		"  Beam:       %6.3f MeV   %-7s% 7.2f uCoul  @ %5.2f nA\n"
		"  Geometry:    %7s  Theta: %7.2f  Phi: %7.2f  Psi: %7.2f\n"
		"  MCA:         Econv: %7.3f  %7.3f  First chan: %4.1f  NPT: %4d\n"
		"  Detector:    FWHM: %4.1f keV  Tau: %4.1f   Omega: %5.3f\n"
		"  Correction:  %6.4f\n",
			filetype,
			buf->filename,	buf->id,      buf->ltct,  buf->date,
			buf->e0,			beam_string,
			buf->q,			buf->current,
			geometry,		buf->theta,   buf->phi,   buf->psi,
			buf->kevch,	   buf->kev0,    buf->first, buf->npt,
			buf->fwhm,		buf->tau,	  buf->omega,
			buf->corr);
	return;
}

/* ===========================================================================
--  Usage:  call beamcod(ibf, buff)
--
--  Inputs: ibf - buffer number to encode
--
--  Output: buff - character descriptor of incident beam	char *
--				zbeam = 2, mbeam = 4, cbeam = 2 ==> 2He++
--		      Result must be readable by routine identp
=========================================================================== */
char *RbsBeamCode(SPECTRUM *buf, char *buffer) {

	int i;
	char *aptr;

	sprintf(buffer,"%d%s",
		(int) (buf->mbeam+0.5),							/* Nearest mass range	*/
		atomic_symbol( buf->zbeam ) );								/* Atom symbol				*/
	aptr = buffer + strlen(buffer);
	if (buf->cbeam > 0) {for (i=0; i<+buf->cbeam; i++) *aptr++ = '+';}
	if (buf->cbeam < 0) {for (i=0; i<-buf->cbeam; i++) *aptr++ = '-';}
	*aptr = '\0';
	return(buffer);
}	


/* ===========================================================================
-- Usage: BOOL RbsWriteFile(SPECTRUM *ibf, BOOL IsRewrite)
--
-- Handles the rewriting of RBS spectra to the disk.
--
-- Inputs: ibf       - pointer to valid spectrum
--         IsRewrite - indicates request for rewrite.  Bypasses the
--                     request to confirm an overwrite unless there
--                     are other changes occuring.
--
-- Output: data potentially written to disk
--
-- Return: TRUE  - if the write was accepted and successful
--         FALSE - if the write was aborted by user or failed
=========================================================================== */
BOOL RbsWriteFile(SPECTRUM *ibf, BOOL IsRewrite) {

	char token[OPTION_STR_SIZE];
	char *mode, ext[32];						/* File open mode and extension of file */
	int rcode, FileType;
	FILE *handle;
	RDWR_PROC *RdwrProc;
	BOOL query=TRUE, verify=FALSE;

/* First, look for a help request */
	if (LexCheckHelp("Write", WriteHelp, NULL)) return(TRUE);

/* Check for an option to ignore warning messages */
	RdwrProc = NULL;												/* No default routine */
	while (LexGetOptionEx(token, sizeof(token), "-")) {
		if (LexEqual(token, "-ASCII", 4)) {
			RdwrProc = ASCIIRdwrProc;
		} else if (LexEqual(token, "-BINARY", 4)) {
			RdwrProc = RUMPRdwrProc;
		} else if (LexEqual(token, "-TXT", 4) || LexEqual(token, "-XLS", 4) || LexEqual(token, "-Excel", 6)) {
			RdwrProc = XLSRdwrProc;
		} else if (LexEqual(token, "-FORMAT", 5)) {
			if (LexGetTokenP(token, sizeof(token), "Data file format (use READ c1 -CONFIG to list): ")) {
				for (RdwrProc = RdwrProcList; RdwrProc!=NULL; RdwrProc = RdwrProc->next) {
					if (stricmp(token, RdwrProc->ID) == 0) break;
				}
				if (RdwrProc == NULL) {
					ERRprintf("ERROR: \"%s\" does not exist as a read/write format.\n", token);
					return(FALSE);
				}
			}
		} else if (LexEqual(token, "-nq", 3) || LexEqual(token, "-noquery", 4) ||
					  LexEqual(token, "-nowarning", 4) || LexEqual(token, "-silent", 4) ||
					  LexEqual(token, "-force", 2) || LexEqual(token, "-yes", 2)) {
			query = FALSE;
		} else {
			LexBackup();
			break;
		}
	}

/* Check -- unless silenced -- for a few conditions */
	if (! IsRewrite && query && access(ibf->filename, R_OK) == 0) {
		ERRprintf("WARNING: File %s exists.\n", ibf->filename);
		verify = TRUE;
	}

	if (query && ibf->dirty) {
		ERRprintf("WARNING: The actual numerical counts in %s\n"
					 "         has been changed (smoothing or otherwise).  Saving modified \n"
                "         experimental data is generally a bad idea.  Proceed with caution!\n", ibf->filename);
		verify = TRUE;
	}

/* --------------------------------------------------------
-- Determine which read routine to use to write the file
--
-- Step 1 - Use forced type from above
-- Step 2 - Use type defined by the filename extension
-- Step 3 - Use type set by the read of that buffer
-- Step 4 - Use default type for RUMP (DefaultRdwrProc)
-------------------------------------------------------- */
	if (RdwrProc == NULL) {
		GetExtType(ibf->filename, ext, sizeof(ext));
		RdwrProc = RdwrProcList;
		while (RdwrProc != NULL && ! IsExtInList(ext,RdwrProc->ExtList)) RdwrProc = RdwrProc->next;
	}
	if (RdwrProc == NULL) RdwrProc = (RDWR_PROC *) ibf->RdwrProc;
	if (RdwrProc == NULL) RdwrProc = DefaultRdwrProc;

/* Can the selected routine write the data? */
	if ( ! (RdwrProc->bOptions & R_HAS_WRITE) || RdwrProc->WriteProc == NULL) {
		if (query) {
			ERRprintf(
						 "WARNING: The read/write module for %s\n"
						 "              %s: %s\n"
						 "         (based either on how it was read, or its extension) does not support\n"
						 "         writing.  The default (probably RUMP) format will be used instead.\n"
						 "         Unless you change the extension (and/or name), this file may not be\n"
						 "         readable and certainly the original format will be lost.\n",
						 ibf->filename, RdwrProc->ID, RdwrProc->Description);
			verify = TRUE;
		}
		RdwrProc = DefaultRdwrProc;
	}
	if ( ! (RdwrProc->bOptions & R_HAS_WRITE) || RdwrProc->WriteProc == NULL) RdwrProc = RUMPRdwrProc;

/* Okay, last chance for user to abort out! */
	if (verify && ! LexYesNo(FALSE, "*** Do you still want to proceed and write the file (NO)? "))
			return(FALSE);

/* Open the complex name and write the data */
	if (! (RdwrProc->bOptions & R_DONT_OPEN)) {
		mode = (RdwrProc->bOptions & R_USE_BINARY) ? "wb" : "w";
		if ( (handle = MyOpenFileForWrite(ibf->filename, mode, &rcode, &FileType)) == NULL) return(FALSE);
	} else {
		handle = NULL;													/* Routine will open */
	}
	rcode = (*RdwrProc->WriteProc)(handle, ibf->filename, ibf);
	if (handle != NULL) MyCloseFile(handle, FileType);

	if (rcode != 0) {
		ERRprintf("ERROR: Error writing %s.  File may now be bad. (rcode=%d)\n", ibf->filename, rcode);
		remove(ibf->filename);
	} else {
		ibf->RdwrProc = RdwrProc;							/* Now in a new format */
		ibf->dirty    = FALSE;
		ibf->modify   = FALSE;
	}

	return(rcode == 0);
}

/* ===========================================================================
-- Usage: SPECTRUM *RbsRdFile(char *path)
--
-- Reads a single file specified by the path, returning a pointer to a valid
-- SPECTRUM structure containing the data.  Multiple formats are permitted,
-- but only a single file will be read.
--
-- Inputs: path - filename to be opened (may potentially not exist)
--
-- Output: none
--
-- Return: Pointer to allocated spectrum containing the data.  If any error
--         occurs, will return NULL instead.
=========================================================================== */
SPECTRUM *RbsRdFile(char *path) {

   SPECTRUM *tmp;
	FILE *handle;
	int rcode, FileType;
	RDWR_PROC *RdwrProc;
	char *mode, ext[32];						/* File open mode and extension of file */

	if ( (tmp = RbsAllocateSpectrum(MAINBUF, CMAX)) == NULL) {
		ERRprintf("ERROR: Unable to allocate new RBS spectrum buffer\n");
		return(NULL);
	} 

/* If not done before, load all valid read/write procedures */
	if (RdwrProcList == NULL) LoadRdwrProcs();

/* Determine which read routine to use to open file */
	if ( (RdwrProc = ForceRdwrProc) == NULL) {
		GetExtType(path, ext, sizeof(ext));
		RdwrProc = RdwrProcList;
		while (RdwrProc != NULL && ! IsExtInList(ext,RdwrProc->ExtList)) RdwrProc = RdwrProc->next;
	}
	if (RdwrProc == NULL) RdwrProc = DefaultRdwrProc;

/* Two modes - open handle and read, or just give pathname and read */
	strscpy(tmp->filename, path, sizeof(tmp->filename));			/* Copy in the name */
	if (! (RdwrProc->bOptions & R_DONT_OPEN)) {
		mode = (RdwrProc->bOptions & R_USE_BINARY) ? "rb" : "r";
		if ( (handle = MyOpenFileForRead(path, mode, &rcode, &FileType)) == NULL) goto ExitNow;
		if (FileType == PIPEFILE && *tmp->filename == '|') *tmp->filename = PIPE_CHAR;
	} else {
		handle = NULL;													/* Routine will open */
	}

	if ((*RdwrProc->ReadProc)(handle, path, tmp, &rcode) == NULL && rcode == 0) rcode = -99;
	if (handle != NULL) MyCloseFile(handle, FileType);
	tmp->RdwrProc = RdwrProc;

	switch (rcode) {
		case 0:
			TTYprintf(" File: %s\n Id:   %s\n", tmp->filename, tmp->id);
			break;
		case 1:
			ERRprintf("ERROR: Unable to allocate buffer for reading data file\n");
			break;
		case -1:
			ERRprintf("ERROR: Fatal error in RBS data file\n");
			break;
		case -2:
			if (FileType == SIMPLEFILE && RbsRdFileOld(path,tmp)) rcode = 0;
			if (rcode != 0) ERRprintf("ERROR: File not in RBS interchange format\n");
			break;
		case -3:
			ERRprintf("ERROR: Unable to allocate buffer space for count data\n");
			break;
		default:
			ERRprintf("ERROR: Unknown error reading RBS data file (%i)\n", rcode);
	}

ExitNow:
	if (rcode != 0) {RbsFreeSpectrum(tmp); tmp = NULL;}
	return(tmp);
}


/* ===========================================================================
-- Routine to scan the path and return best guess of the file extension.  Will
-- properly handle formatted zip file extractions and .gz files.
=========================================================================== */
static void GetExtType(char *path, char *ext, int len) {
	char *aptr, *endptr;

	*ext = '\0';
	if (*path == '\0') return;

	endptr = path + strlen(path);
	while (endptr != path) {
		aptr = endptr-1;											/* Find the last extension */
		while (*aptr != '.' && aptr != path) aptr--;
		if (strnicmp(aptr, ".gz", 3) != 0) break;			/* Skip over .gz exts */
		endptr = aptr;
	}
	
	if (endptr != aptr) {										/* Copy extension as wanted */
		len = min(len-1, (int) (endptr-aptr));
		strncpy(ext, aptr, len);
		ext[len] = '\0';
		if (ext[len-1] == '"') ext[len-1] = '\0';			/* Strip trailing quote */
	}

	return;
}

/* ===========================================================================
-- Routine to return TRUE if an extension is among the semicolon delimited
-- list of extensions.
=========================================================================== */
static BOOL IsExtInList(char *ext, char *list) {
	int len;
	char *aptr, *bptr;

	len = (int) strlen(ext);									/* Required length */
	for (aptr=list; *aptr!='\0'; aptr=bptr) {
		while (*aptr == ';') aptr++;							/* Skip over delimiter */
		bptr = aptr;
		while (*bptr != ';' && *bptr != '\0') bptr++;	/* Find end of ext */
		if (bptr-aptr == len && strnicmp(aptr, ext, len) == 0) return(TRUE);
	}
	return(FALSE);
}
	



/* ===========================================================================
-- Function to allocate space for a spectrum structure, and initialize to
-- either default parameters or a copy of an existing spectrum.  On return,
-- the buf->count array is ready to be filled with spectral data.
--
--  Usage:  SPECTRUM *RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels);
--
--  Inputs: proto       - prototype for spectrum header information.  If 
--                        NULL, default values will be used for all entries.
--          NumChannels - number of channels for which to allocate space.
--                        If 0 or negative, buf->counts will be left NULL.
--
--  Returns: Pointer to properly initialized structure or NULL on failure.
=========================================================================== */
SPECTRUM *RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels) {
	
	SPECTRUM *buf;

	if ( (buf = malloc(sizeof(SPECTRUM))) == NULL) return(NULL);

	if (proto != NULL) {
		*buf = *proto;									/* Copy most everything		*/
	} else {
		buf->type    = RBS;							/* That's what most are		*/
		buf->e0      = 3.0f;							/* 3.0 MeV accelerator		*/
		buf->zbeam   = 2;								/* Assume 4He++				*/
		buf->mbeam   = 4.0f;							/* Close enough				*/
		buf->cbeam   = 2;								/* Doubly charged				*/
		buf->q       = 10;							/* 10 uC							*/
		buf->current = 0;								/* Beam current off			*/
		buf->kevch   = 4.0f;							/* 4 keV/channel				*/
		buf->kev0    = 0.0f;							/* Perfect system				*/
		buf->first   = 0;
		buf->fwhm    = 20.0f;						/* Tolerable detector		*/
		buf->tau     = 5.0f;							/* Shaping time 5 uS			*/
		buf->geom    = CORNELL;						/* CORNELL geometry			*/
		buf->phi     = 9.0f;							/* Nearly backscattering	*/
		buf->theta   = 7.0f;							/* Sample slightly tilted	*/
		buf->psi     = 0.0f;							/* Unnecessary in CORNELL	*/
		buf->omega   = 4.0f;							/* 4 msr detector				*/
		buf->corr    = 1.0f;							/* Assume data valid			*/
	}

/* Modify the things that aren't inherited */
	strcpy(buf->filename, "dummy.rbs");
	strcpy(buf->id, "Initialized buffer");
	*buf->date   = '\0';								/* Null these out				*/
	*buf->ltct   = '\0';
	buf->nspectra = 1;								/* Assume 1 spectrum			*/
	buf->npt     = 0;									/* And no data					*/
	buf->counts  = NULL;								/* And buffer empty			*/
	buf->dirty   = TRUE;								/* Assume worst case			*/
	buf->modify  = TRUE;								/* Parameters changed		*/
	buf->iddone  = FALSE;							/* No ID done for spectrum	*/
	buf->extra   = NULL;								/* Potentially user def'n	*/
	buf->RdwrProc = NULL;							/* Don't know who read it	*/

/* ... Now, do we want to allocate for him as well? */
	buf->nptmax  = max(0, NumChannels);			/* How many will we have?	*/
	if (NumChannels > 0) {
		if ( (buf->counts = calloc(NumChannels, sizeof(*buf->counts))) == NULL) {
			free(buf);
			return(NULL);
		}
	}

	return(buf);
}

/* ===========================================================================
-- Function to resize count buffer space in a spectrum structure.
--
--  Usage:  SPECTRUM *RbsResizeSpectrum(SPECTRUM *ibf, int size);
--
--  Inputs: ibf  - existing spectrum structure (or NULL to create new)
--          size - number of channels now desired.  If smaller than
--                 the current max_channels, the data will be truncated.
--                 truncated and NPT adjusted (if necessary).  If
--                 more than currently, will just be zeroed out and
--                 to change made to NPT.  Zero, or negative, will have
--                 the counts array released and npt/nptmax set to zero.
--
--  Returns: Pointer to properly initialized structure or NULL on failure.
=========================================================================== */
SPECTRUM *RbsResizeSpectrum(SPECTRUM *buf, int size) {

	int i;

/* Trivial case */
	if (buf == NULL) {
		buf = RbsAllocateSpectrum(NULL, size);

/* Release all of the memory? */
	} else if (size <= 0) {
		free(buf->counts);
		buf->counts = NULL;
		buf->npt = buf->nptmax = 0;

/* More complex - but only handle if changing */
	} else if (buf->nptmax != size) {
		if ( (buf->counts = realloc(buf->counts, size*sizeof(*buf->counts))) == NULL) {
			ERRprintf("HOLY SHIT: I can't believe I'm out of memory!\n");
			return(NULL);
		}
		for (i=buf->nptmax; i<size; i++) buf->counts[i] = 0.0f;
		buf->nptmax = size;
		if (buf->npt > buf->nptmax) buf->npt = buf->nptmax;
	}

	return(buf);
}	


/* ===========================================================================
-- Function to free all memory used by a SPECTRUM structure, including the
-- structure itself (assuming it was malloc'd).
--
--  Usage:  int RbsFreeSpectrum(SPECTRUM *buf);
--
--  Inputs: *buf - Rump spectrum structure
--
--  Output: Free's spectrum data buffer and the structure itself.  The pointer
--          buf will no longer be valid after this call.
--
--  Returns: 0
=========================================================================== */
int RbsFreeSpectrum(SPECTRUM *buf) {
	
	if (buf != NULL) {
		if (buf->extra  != NULL) free(buf->extra);
		if (buf->counts != NULL) free(buf->counts);
		free(buf);
	}
	return(0);
}

/* ===========================================================================
-- Function to copy an existing spectrum structure to a new structure.
--
--  Usage:  SPECTRUM *RbsCopySpectrum(SPECTRUM *dest, SPECTRUM *source);
--
--  Inputs: *source - spectrum to be copied
--
--  Output: *dest   - spectrum where data will be copied.  If NULL, then
--                    a new structure will be allocated for the copy.
--
--  Return: dest, or newly allocated SPECTRUM structure if dest was NULL
--          NULL indicates error - usually unable to allocate space
=========================================================================== */
SPECTRUM *RbsCopySpectrum(SPECTRUM *dest, SPECTRUM *source) {

	int isize, nptmax;
	REAL *counts;

	if (dest == NULL && (dest = RbsAllocateSpectrum(NULL, source->nptmax)) == NULL)
		return(NULL);

/* Save the allocated buffer counts, and copy the rest */
	nptmax       = dest->nptmax;
	counts       = dest->counts;
	
/* Copy source to destination and restore critical information */
	*dest        = *source;						/* Copy everything over			*/
	dest->counts = counts;						/* Restore the count pointer	*/
	dest->nptmax = nptmax;						/* Restore the array size		*/

/* And maybe copy data if necessary */
	if (source->nptmax > 0) {							/* May have to resize */
		isize = source->nptmax * sizeof(*source->counts);
		if (dest->counts == NULL) {
			dest->counts = malloc(isize);
			dest->nptmax = source->nptmax;
		} else if (dest->nptmax < isize) {
			dest->counts = realloc(dest->counts, isize);
			dest->nptmax = source->nptmax;
		}
		memcpy(dest->counts, source->counts, isize);
	}

	return(dest);
}



/* ===========================================================================
-- Routine to open the complex name file.  Opens as pipe or handle as
-- appropriate.
--
-- Usage: FILE *MyOpenFileForRead(char *path, char *mode, int *err, int *type)
--
-- Inputs: path   - file or pipe to open
--         mode   - "rb" or "r" for opening as binary or ASCII
--
-- Outputs: *err  - filled with error code if return is NULL
--          *type - filled with constant indicating whether file or pipe
--
-- Return:  Returns either a valid FILE * structure or NULL on error.
=========================================================================== */
static FILE *MyOpenFileForRead(char *path, char *mode, int *err, int *type) {

	FILE *handle;
	int FileType, rcode;

	rcode = 0;
	FileType = SIMPLEFILE;

	if (*path == '|' || *path == PIPE_CHAR) {		/* Is a pipe request */
		FileType = PIPEFILE;
		if ( (handle = popen(path+1, mode)) == NULL) rcode = 2;
	} else if (strstr(path, "::") != NULL) {
		char inbuf[2*PATH_MAX], tmpname[PATH_MAX], *aptr;
		FileType = PIPEFILE;
		strcpy(tmpname, path);
		aptr = strstr(tmpname, "::");
		*aptr = '\0';
		sprintf(inbuf, "unzip -p %s %s", tmpname, aptr+2);
		if ( (handle = popen(inbuf, mode)) == NULL) rcode = 2;
	} else if (strlen(path) > 3 && stricmp(path+strlen(path)-3, ".gz") == 0) {
		char inbuf[PATH_MAX+9];
		FileType = PIPEFILE;
		strcat(strcpy(inbuf, "gzip -dc "), path);
		if ( (handle = popen(inbuf, mode)) == NULL) rcode = 2;
	} else if ( (handle=fopen(path,mode)) == NULL) {
		rcode = 3;
	} 

	switch (rcode) {									/* Error messages */
		case 2:
			ERRprintf("ERROR: Requested pipe read, or automatic pipe to gzip, failed\n");
			break;
		case 3:
			ERRprintf("ERROR: Filename %s failed to open\n", path);
			break;
	}

	if (err  != NULL) *err  = rcode;
	if (type != NULL) *type = FileType;
	return((rcode == 0) ? handle : NULL);
}

/* ===========================================================================
-- Routine to open the complex name file for write.  Opens as pipe or handle as
-- appropriate.
--
-- Usage: FILE *MyOpenFileForWrite(char *path, char *mode, int *err, int *type)
--
-- Inputs: path   - file or pipe to open
--         mode   - "wb" or "w" for opening as binary or ASCII
--
-- Outputs: *err  - filled with error code if return is NULL
--          *type - filled with constant indicating whether file or pipe
--
-- Return:  Returns either a valid FILE * structure or NULL on error.
=========================================================================== */
static FILE *MyOpenFileForWrite(char *path, char *mode, int *err, int *type) {

	FILE *handle;
	int FileType, rcode;

	rcode = 0;
	FileType = SIMPLEFILE;

	if (*path == PIPE_CHAR) {
		rcode = 1;
	} else if (*path == '|') {
		FileType = PIPEFILE;
		if ( (handle = popen(path+1, mode)) == NULL) rcode = 2;
	} else if (strlen(path) > 3 && stricmp(path+strlen(path)-3, ".gz") == 0) {
		char inbuf[PATH_MAX+9];
		FileType = PIPEFILE;
		strcat(strcpy(inbuf, "gzip -9 > "), path);
		if ( (handle = popen(inbuf, mode)) == NULL) rcode = 2;
	} else if ( (handle=fopen(path, mode)) == NULL) {
		rcode = 3;
	}

	switch (rcode) {									/* Error messages */
		case 1:
			ERRprintf("ERROR: Filename suggests the original was a pipe - can't rewrite\n");
			break;
		case 2:
			ERRprintf("ERROR: Pipe request (implicit or explicit) failed\n");
			break;
		case 3:
			ERRprintf("ERROR: Filename %s failed to open\n", path);
			break;
	}

	if (err  != NULL) *err  = rcode;
	if (type != NULL) *type = FileType;
	return((rcode == 0) ? handle : NULL);
}

/* ===========================================================================
-- Close via either pclose() or fclose() as required.
=========================================================================== */
static void MyCloseFile(FILE *handle, int mode) {

	if (handle != NULL) (mode == PIPEFILE) ? pclose(handle) : fclose(handle);
	return;
}


/* ===========================================================================
-- Usage Guide:
--
-- Usage: SPECTRUM *ASCII_ReadProc(FILE *handle, CHAR *path, SPECTRUM *ibf, int *err);
--
-- Inputs: handle - opened file handle in ASCII mode
--         path   - actual pathname to the file (reopen)
--         ibf    - SPECTRUM structure (initialized) to fill
--         err    - pointer to error return variable
--
-- Output: *err - error code.  0 if successful
--                0 - successful read of data
--                1 - unable to open file
--                2 - unable to allocate buffer for reading
--               -3 - unable to allocate buffers space for counts
--               -2 - bad file format from the beginning
--               -1 - bad file format somewhere during reading (after headers)
--         *ibf - filled spectrum structure
--
-- Return: Returns pointer to ibf if successful, NULL otherwise.
--
-- Quick: Opens a file and reads as ASCII data
=========================================================================== */
static char		 *ASCII_ID = "ASCII";
static char		 *ASCII_Description = "Pure ASCII format read routine";
static char     *ASCII_ExtList  = ".dat;.asc;.ascii";
static int		  ASCII_Options  = R_USE_ASCII | R_HAS_WRITE;

static SPECTRUM *ASCII_ReadProc(FILE *handle, CHAR *path, SPECTRUM *ibf, int *err) {

	int i,j;
	REAL *counts;
	char inbuf[LONG_STR_SIZE], *aptr, *bptr, token[OPTION_STR_SIZE];
	BOOL twocol;

/* Look for the twocolumn option */
	twocol = FALSE;
	if (LexGetOptionEx(token, sizeof(token), "-")) {
		if (LexEqual(token, "-twocolumn", 4) || LexEqual(token, "-2column", 5)) {
			twocol = TRUE;
		} else if (LexEqual(token, "-onecolumn", 4) || LexEqual(token, "-1column", 5)) {
			twocol = FALSE;
		} else {
			ERRprintf("ERROR: Illegal option for ASCII read command (%s)\n", token);
			return(NULL);
		}
	}

	*ibf->id = '\0';									/* Empty for now, fill on read */
	strcpy(ibf->ltct, "ASCII file read");

	counts = ibf->counts;							/* Where I store the data		*/
	for (i=0; i<CMAX; i++) counts[i] = 0;		/* Clear the buffer now			*/

	aptr = inbuf;										/* Initialize the pointers		*/
	*inbuf = '\0';										/* And nothing there for now	*/
	ibf->npt = 0;
	for (i=0; i<CMAX; ) {
		while (*aptr == '\0') {
			if (fgets(inbuf, sizeof(inbuf), handle) == NULL) goto done;
			if (*inbuf == '\0') {aptr = inbuf; continue;}
			aptr = inbuf + strlen(inbuf) - 1;
			while (aptr >= inbuf && isspace(*aptr)) *aptr-- = '\0';
			aptr = inbuf;
			while (isspace(*aptr)) aptr++;
			if (! isdigit(*aptr)) {
				TTYprintf("Info: %s\n", inbuf);
				if (*ibf->id == '\0') strcpy(ibf->id, inbuf);
				*aptr = '\0';
			}
		}
		j = twocol ? strtol(aptr, &aptr, 10) : i ;			/* Channel position	*/
		j = min(max(j,0),CMAX-1);									/* And limit			*/
		counts[j] = (REAL) strtod(aptr, &bptr);
		if (bptr != aptr) {
			aptr = bptr;
			i++;
		} else {
			*aptr = '\0';
		}
		ibf->npt = max(ibf->npt, j+1);
	}

done:
	if (*ibf->id == '\0') sprintf(ibf->id, "! %s", ibf->filename);
	if (ibf->npt == 0) ERRprintf("WARNING: Did not find any data in file %s\n", ibf->filename);
	ibf->dirty  = FALSE;
	ibf->modify = FALSE;

	if (err != NULL) *err = 0;
	return(ibf);
}


/* ===========================================================================
-- Function to write an ASCII RBS file to disk.  The filename is taken from
-- the SPECTRUM structure.  This is a standard write that stores all parameters
-- currently relevent to RBS analysis.
--
--  Usage:  int RbsWriteProc(FILE *funit, CHAR *path, SPECTRUM *spectra)
--
--  Inputs: funit   - points to an open file in "wb" mode for writing
--          path    - (unused) Path of opened file (or to be opened)
--          spectra - pointer to a spectrum structure containing all of
--                    the current parameters and data.
--
--  Output: Outputs record as specified
--
--  Returns:  0 - successful write
--           -1 - unable to allocate buffer space for writing
=========================================================================== */
static int ASCII_WriteProc(FILE *handle, CHAR *path, SPECTRUM *ibf) {

	char *aptr, *bptr, beam[20], token[OPTION_STR_SIZE];
	int rcode=0, i;
	BOOL twocol;

/* Look for the twocolumn option */
	twocol = FALSE;
	if (LexGetOptionEx(token, sizeof(token), "-")) {
		if (LexEqual(token, "-twocolumn", 4) || LexEqual(token, "-2column", 5)) {
			twocol = TRUE;
		} else if (LexEqual(token, "-onecolumn", 4) || LexEqual(token, "-1column", 5)) {
			twocol = FALSE;
		} else {
			ERRprintf("ERROR: %s invalid ASCII write option.  Using defaults.\n", token);
		}
	}

/* ... The ascii version ... */
	RbsBeamCode(ibf, beam);
									  aptr = "??";
	if (ibf->geom == IBM )    aptr = "IBM";
	if (ibf->geom == CORNELL) aptr = "Cornell";
	if (ibf->geom == GENERAL) aptr = "General";
									  bptr = "RBS";
	if (ibf->type == FRES)    bptr = "FRES";
	if (ibf->type == PIXE)    bptr = "PIXE";

	fprintf(handle, "C /* ASCII write format from RUMP */\n"
						 "C @echo off Empty Filename '%s'\n"
						 "C Spectrum    %s\n"
						 "C Identifier '%s'\n"
						 "C Date       '%s'\n"					
						 "C Charge      %f     MeV  %f\n"
						 "C Conversion  %f %f\n"			
						 "C Theta       %f     Phi  %f\n" 
						 "C Omega       %f     Corr %f\n"		
						 "C Choff       %f     FWHM %f\n" 
						 "C Current     %f     Tau  %f\n"					
						 "C Geometry %s        Beam %s\n"
						 "C %s\n",
					ibf->filename, bptr, ibf->id, ibf->date, ibf->q, ibf->e0,
					ibf->kevch, ibf->kev0, ibf->theta, ibf->phi, ibf->omega, 
				   ibf->corr, ibf->first, ibf->fwhm, ibf->current, ibf->tau,
				   aptr, beam, (twocol) ? "Swallow -2col" : "Swallow");

	for (i=0; i<ibf->npt; i++) {
		if (twocol) {
			for (i=0; i<ibf->npt; i++) fprintf(handle, "%d %g\n", i, ibf->counts[i]);
		} else {
			for (i=0; i<ibf->npt; i++) fprintf(handle, "%g\n", ibf->counts[i]);
		}
	}
		
	if (fprintf(handle, "\n") <= 0) {
		ERRprintf("ERROR: Failure during ASCII file write - probable disk full\n");
		rcode = -1;
	}

	return(rcode);
}


/* ===========================================================================
-- Usage Guide:
--
-- Usage: SPECTRUM *XLS_ReadProc(FILE *handle, CHAR *path, SPECTRUM *ibf, int *err);
--
-- Inputs: handle - opened file handle in ASCII mode
--         path   - actual pathname to the file (reopen)
--         ibf    - SPECTRUM structure (initialized) to fill
--         err    - pointer to error return variable
--
-- Output: *err - error code.  0 if successful
--                0 - successful read of data
--                1 - unable to open file
--                2 - unable to allocate buffer for reading
--               -3 - unable to allocate buffers space for counts
--               -2 - bad file format from the beginning
--               -1 - bad file format somewhere during reading (after headers)
--         *ibf - filled spectrum structure
--
-- Return: Returns pointer to ibf if successful, NULL otherwise.
--
-- Quick: Opens a file and reads an XLS file (Rump format only)
=========================================================================== */
static char		 *XLS_ID = "XLS";
static char		 *XLS_Description = "Tab delimited file format (Excel compatible)";
static char     *XLS_ExtList  = ".txt;.xls";
static int		  XLS_Options  = R_USE_ASCII | R_HAS_WRITE;

static SPECTRUM *XLS_ReadProc(FILE *handle, CHAR *path, SPECTRUM *ibf, int *err) {

	int i;
	REAL *counts;
	char inbuf[LONG_STR_SIZE], *aptr;
	FLOAT rval;
	INT ival;
	BOOL data_ready;

	*ibf->id = '\0';									/* Empty for now, fill on read */

/*	Make sure we understand the format */
	if (fgets(inbuf, sizeof(inbuf), handle) == NULL) goto done;
	if ( (aptr = strchr(inbuf, '\n')) != NULL) *aptr = '\0';
	if (stricmp(inbuf, "/* Excel format [1.0] from RUMP */") != 0) {
		ERRprintf("ERROR: RUMP is only read excel format files that were written by RUMP.\n"
					 "       The specified file does not begin with an expected header\n");
		if (err != NULL) *err = 2;
		return(NULL);
	}

#define ISMATCH(text)	(strnicmp(inbuf, text, strlen(text)) == 0)
	data_ready = FALSE;
	while (fgets(inbuf, sizeof(inbuf), handle) != NULL) {
		if ( (aptr = strchr(inbuf, '\n')) != NULL) *aptr = '\0';
		if (*inbuf == '\0') continue;
		if ( (aptr = strchr(inbuf,'\t')) == NULL) {
			ERRprintf("ERROR: Doesn't appear to be a valid RUMP Excel file\n"
						 "       Offending line: %s\n", inbuf);
			if (err != NULL) *err = 2;
			return(NULL);
		}
		aptr++;												/* Point to text where real info */
		rval = (FLOAT) atof(aptr);
		ival = (INT)   atoi(aptr);

		if (ISMATCH("Original File\t")) {
			continue;
		} else if (ISMATCH("Spectrum\t")) {
			ibf->type = RBS;
			if (stricmp(aptr, "FRES") == 0) ibf->type = FRES;
			if (stricmp(aptr, "PIXE") == 0) ibf->type = PIXE;
		} else if (ISMATCH("Identifier\t")) {
			strcpy(ibf->id, aptr);
		} else if (ISMATCH("Date\t")) {
			strcpy(ibf->date, aptr);
		} else if (ISMATCH("LTCT\t")) {
			strcpy(ibf->ltct, aptr);
		} else if (ISMATCH("Charge\t")) {
			ibf->q = rval;
		} else if (ISMATCH("MeV\t")) {
			ibf->e0 = rval;
		} else if (ISMATCH("Conversion\t")) {
			ibf->kevch = (REAL) strtod(aptr, &aptr); ibf->kev0 = (REAL) atof(aptr);
		} else if (ISMATCH("Theta\t")) {
			ibf->theta = rval;
		} else if (ISMATCH("Phi\t")) {
			ibf->phi = rval;
		} else if (ISMATCH("Omega\t")) {
			ibf->omega = rval;
		} else if (ISMATCH("Corr\t")) {
			ibf->corr = rval;
		} else if (ISMATCH("Choff\t")) {
			ibf->first = rval;
		} else if (ISMATCH("FWHM\t")) {
			ibf->fwhm = rval;
		} else if (ISMATCH("Current\t")) {
			ibf->current = rval;
		} else if (ISMATCH("Tau\t")) {
			ibf->tau = rval;
		} else if (ISMATCH("Geometry\t")) {
			if (stricmp(aptr, "IBM")) ibf->geom = IBM;
			if (stricmp(aptr, "Cornell")) ibf->geom = CORNELL;
			if (stricmp(aptr, "General")) ibf->geom = GENERAL;
		} else if (ISMATCH("Beam\t")) {
			int zbeam, mbeam, cbeam;
			if (stricmp(aptr, "D") == 0) strcpy(aptr, "2H");
			if (stricmp(aptr, "HE3") == 0) strcpy(aptr, "3HE");
			if (! RbsIdentp(aptr, &zbeam, &mbeam, &cbeam)) {
				ERRprintf("ERROR: Unrecognized beam (%s)\n", aptr);
				if (err != NULL) *err = 2;
				return(NULL);
			}
			ibf->zbeam  = zbeam;
			ibf->cbeam  = cbeam;
			ibf->mbeam  = RbsGetRealMass(ibf->zbeam, mbeam);
		} else if (ISMATCH("Channel\t")) {
			data_ready = TRUE;
			break;
		} else {
			ERRprintf("ERROR: Unrecognized line in Excel file - aborting\n"
						 "       \"%s\"", inbuf);
			*err = 2;
			return(NULL);
		}
	}

	if (! data_ready) {
		ERRprintf("ERROR: Never got header line marking start of data (Excel format)\n");
		*err = 1;
		return(NULL);
	}

	counts = ibf->counts;							/* Where I store the data		*/
	for (i=0; i<CMAX; i++) counts[i] = 0;		/* Clear the buffer now			*/
	ibf->npt = 0;

	while (fgets(inbuf, sizeof(inbuf), handle) != NULL && ibf->npt < CMAX) {
		if ( (aptr = strchr(inbuf, '\n')) != NULL) *aptr = '\0';
		aptr = strchr(inbuf, '\t');
		if (aptr == NULL) continue;
		counts[ibf->npt] = (REAL) atof(aptr+1);
		ibf->npt++;
	}

done:
	if (*ibf->id == '\0') sprintf(ibf->id, "! %s", ibf->filename);
	if (ibf->npt == 0) ERRprintf("WARNING: Did not find any data in file %s\n", ibf->filename);
	ibf->dirty  = FALSE;
	ibf->modify = FALSE;

	if (err != NULL) *err = 0;
	return(ibf);
}


/* ===========================================================================
-- Function to write an EXCEL RBS file to disk.  The filename is taken from
-- the SPECTRUM structure.  This is a standard write that stores all parameters
-- currently relevent to RBS analysis, but does so in a tab-delimited format
-- with write of channel, counts, energy, and normalized yield.
--
--  Usage:  int RbsWriteProc(FILE *funit, CHAR *path, SPECTRUM *spectra)
--
--  Inputs: funit   - points to an open file in "w" mode for writing
--          path    - (unused) Path of opened file (or to be opened)
--          spectra - pointer to a spectrum structure containing all of
--                    the current parameters and data.
--
--  Output: Outputs record as specified
--
--  Returns:  0 - successful write
--           -1 - unable to allocate buffer space for writing
=========================================================================== */
static int XLS_WriteProc(FILE *handle, CHAR *path, SPECTRUM *ibf) {

	char *aptr, *bptr, beam[20];
	double r;
	int rcode=0, i;

/* ... The ascii version ... */
	RbsBeamCode(ibf, beam);
									  aptr = "??";
	if (ibf->geom == IBM )    aptr = "IBM";
	if (ibf->geom == CORNELL) aptr = "Cornell";
	if (ibf->geom == GENERAL) aptr = "General";
									  bptr = "RBS";
	if (ibf->type == FRES)    bptr = "FRES";
	if (ibf->type == PIXE)    bptr = "PIXE";

	fprintf(handle, "/* Excel format [1.0] from RUMP */\n");
	fprintf(handle, "Original File\t%s\n"	, ibf->filename);
	fprintf(handle, "Spectrum\t%s\n"			, bptr);
	fprintf(handle, "Identifier\t%s\n"		, ibf->id);
	fprintf(handle, "Date\t%s\n"				, ibf->date);
	fprintf(handle, "LTCT\t%s\n"				, ibf->ltct);
	fprintf(handle, "Charge\t%f\n"			, ibf->q);
	fprintf(handle, "MeV\t%f\n"				, ibf->e0);
	fprintf(handle, "Conversion\t%f\t%f\n"	, ibf->kevch, ibf->kev0);
	fprintf(handle, "Theta\t%f\n"				, ibf->theta);
	fprintf(handle, "Phi\t%f\n"				, ibf->phi);
	fprintf(handle, "Omega\t%f\n"				, ibf->omega);
	fprintf(handle, "Corr\t%f\n"				, ibf->corr);
	fprintf(handle, "Choff\t%f\n"				, ibf->first);
	fprintf(handle, "FWHM\t%f\n"				, ibf->fwhm);
	fprintf(handle, "Current\t%f\n"			, ibf->current);
	fprintf(handle, "Tau\t%f\n"				, ibf->tau);
	fprintf(handle, "Geometry\t%s\n"			, aptr);
	fprintf(handle, "Beam\t%s\n"				, beam);
	fprintf(handle, "\n");
	fprintf(handle, "Channel\tCounts\tEnergy (MeV)\tNormalized Yield\n");

	r = RbsNormK(ibf);				/* Counts to normalized yield */

	for (i=0; i<ibf->npt; i++) 
		fprintf(handle, "%d\t%g\t%g\t%g\n", i, ibf->counts[i], RBSENERGY(i, ibf), r*ibf->counts[i]);
		
	if (fprintf(handle, "\n") <= 0) {
		ERRprintf("ERROR: Failure during XLS file write - probable disk full\n");
		rcode = -1;
	}

	return(rcode);
}


/* ===========================================================================
-- Routine to load all available RBS format read routines.
--
-- Usage: void LoadRdwrProcs();
--
-- Inputs: none
--
-- Output: Creates linked list of *RdwrProcList routines
--
-- Returns: none
=========================================================================== */
static void LoadRdwrProcs(void) {

	RDWR_PROC *tmp;
	if (RdwrProcList != NULL) return;

/* Load the default read procedures */
	tmp = calloc(sizeof(RDWR_PROC), 1);
	*tmp->path        = '\0';								/* Internal default */
	tmp->ID           = RbsRdwrID;
	tmp->Description  = RbsRdwrDesc;
	tmp->ExtList      = RbsRdwrExtList;
	tmp->bOptions     = RbsRdwrOptions;
	tmp->ReadProc     = RbsReadProc;
	tmp->WriteProc    = RbsWriteProc;					/* Also default write proc */
	tmp->next         = NULL;
	RdwrProcList      = tmp;
	RUMPRdwrProc		= tmp;

	DefaultRdwrProc   = RdwrProcList;					/* Initial default choice */

/* Load the ASCII read procedure */
	tmp = calloc(sizeof(RDWR_PROC), 1);
	*tmp->path        = '\0';								/* Internal default */
	tmp->ID           = ASCII_ID;
	tmp->Description  = ASCII_Description;
	tmp->ExtList      = ASCII_ExtList;
	tmp->bOptions     = ASCII_Options;
	tmp->ReadProc     = ASCII_ReadProc;
	tmp->WriteProc    = ASCII_WriteProc;
	tmp->next         = RdwrProcList;
	RdwrProcList      = tmp;
	ASCIIRdwrProc		= tmp;								/* And keep local pointer */

/* Load the Excel read procedure */
	tmp = calloc(sizeof(RDWR_PROC), 1);
	*tmp->path        = '\0';								/* Internal default */
	tmp->ID           = XLS_ID;
	tmp->Description  = XLS_Description;
	tmp->ExtList      = XLS_ExtList;
	tmp->bOptions     = XLS_Options;
	tmp->ReadProc     = XLS_ReadProc;
	tmp->WriteProc    = XLS_WriteProc;
	tmp->next         = RdwrProcList;
	RdwrProcList      = tmp;
	XLSRdwrProc			= tmp;								/* And keep local pointer */

#ifdef NT

/* Find all files within search path that have extension .rdr */
/* Because of load order, handle in reverse rather than forward */
{
#if (defined OS2 || defined NT)
	#define	SEPARATOR	';'			/* Separator on directory lists			*/
#else
	#define	SEPARATOR	':'
#endif

	HMODULE hmod;
	char filename[PATH_MAX], path[PATH_MAX];
	char *dirlist, *aptr, *fileptr;
	char **charptr;
	int  *intptr;
	BOOL bad;
	DIR *dir;
	struct dirent *entry;
	
	dirlist = RbsConfigPath;					/* Paths to search				*/
	while (*dirlist != '\0') {
		aptr = path;
		while (*dirlist != SEPARATOR && *dirlist) *aptr++ = *dirlist++;
		if (*dirlist != '\0') dirlist++;
		*aptr = '\0';								/* Terminate the directory */
		if (*path == '\0') strcpy(path, ".");
		
		if ( (dir = opendir(path)) == NULL) continue;
		strcpy(filename, path);
		fileptr = filename + strlen(filename);
		*fileptr++ = '/';

		while ( (entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 ||  strcmp(entry->d_name, "..") == 0 
				 || (! SysCheckMatch(entry->d_name, "*.rdr")) ) continue;
			strcpy(fileptr, entry->d_name);

			if ( (hmod=LoadLibrary(filename)) == NULL) {
				ERRprintf("ERROR: Dynamic module %s failed to load (rc=%i)\n", filename, GetLastError());
			} else {

				bad = FALSE;
				tmp = calloc(sizeof(RDWR_PROC), 1);

				strcpy(tmp->path, filename);

				if ( (charptr = (char **) GetProcAddress(hmod, "RbsRdwrID")) == NULL) bad = TRUE;
				if (charptr != NULL) tmp->ID = *charptr;
				if ( (charptr = (char **) GetProcAddress(hmod, "RbsRdwrDesc")) == NULL) bad = TRUE;
				if (charptr != NULL) tmp->Description = *charptr;
				if ( (charptr = (char **) GetProcAddress(hmod, "RbsRdwrExtList")) == NULL) bad = TRUE;
				if (charptr != NULL) tmp->ExtList = *charptr;

				if ( (intptr = (int *) GetProcAddress(hmod, "RbsRdwrOptions")) == NULL) bad = TRUE;
				if (intptr != NULL) tmp->bOptions = *intptr;

				tmp->ReadProc  = (void *) GetProcAddress(hmod, "RbsReadProc");
				tmp->WriteProc = (void *) GetProcAddress(hmod, "RbsWriteProc");
				if (tmp->ReadProc == NULL) bad = TRUE;

				if (bad) {
					ERRprintf("ERROR: RBS read module %s failed to load - routines missing\n", filename);
					free(tmp);
					FreeLibrary(hmod);					/* Free the module */
				} else {
					tmp->next = RdwrProcList;
					RdwrProcList = tmp;
					if (tmp->bOptions & R_MAKE_DEFAULT) DefaultRdwrProc = RdwrProcList;
				}
			}
		}
		closedir(dir);
	}
}
#endif

	return;
}


/* ===========================================================================
-- Routine to free all loaded RBS format read routines
--
-- Usage: void FreeRdwrProcs();
--
-- Inputs: none
--
-- Output: Releases modules, memory and resets RdwrProcList
--
-- Returns: none
=========================================================================== */
static void FreeRdwrProcs(void) {

	RDWR_PROC *tmp, *next;
#ifdef NT
	HMODULE hmod;
#endif

	for (tmp=RdwrProcList; tmp!=NULL; tmp=next) {
		next = tmp->next;								/* Since I'll free this memory */
#ifdef NT
		if (tmp->path != NULL && (hmod = GetModuleHandle(tmp->path)) != NULL) FreeLibrary(hmod);
#endif
		free(tmp);
	}

	RdwrProcList = DefaultRdwrProc = ASCIIRdwrProc = RUMPRdwrProc = NULL;
	return;
}
