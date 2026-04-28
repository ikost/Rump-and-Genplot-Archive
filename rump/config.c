/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/*  config.c */
/* ===========================================================================
=========================================================================== */

#define	DEFAULT_BUFFER_SIZE	2048

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#if defined CSET2
	#define	INCL_WINSHELLDATA
	#include <os2.h>
#elif defined MSC70
	#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	CONFIG_C_SOURCE
#include "rump.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

typedef enum _OPS1 {
	CF_HELP, CF_PARM, CF_RETURN, CF_STOPLOAD, CF_STOPTABLE, CF_STOPTYPE,
	CF_BUFFERS, CF_DENSITY, CF_FRES, CF_PROMPT, CF_AUTORETURN,
	CF_DATAPATH, CF_DATAEXTS, CF_TABCOMPLETION, CF_EXITQUERY, CF_WRITELEVEL, CF_STOPP,
   CF_USERSTOP, CF_USERCROSS
} OPS1;

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1 rcode;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void LoadAtomicData(void);
static void FreeStopTables(void);

#ifdef NT
static int LoadMyDLL(HMODULE *hmod, void **routine, char *entry, char *prompt);
static HMODULE hmod_UserZStop=NULL,				/* Handle for stopping power */
					hmod_UserCrossSection=NULL;	/* For cross section         */
#endif

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */
EXPORT char  RbsAtomicDataFile[PATH_MAX]="";	/* Atomic info data file			*/
char *RbsConfigPath=NULL;					/* Default config search path		*/
#if (defined OS2 || defined NT)
	char  RbsSearchPath[LONG_STR_SIZE]="";	/* Default data file/ext searchs	*/
	char  RbsSearchExts[DFLT_STR_SIZE] =".rbs;.fres;.pixe";
#else
	char  RbsSearchPath[LONG_STR_SIZE]="";	/* Default data file/ext searchs	*/
	char  RbsSearchExts[DFLT_STR_SIZE] =".rbs:.fres:.pixe";
#endif


EXPORT SPECTRUM **RbsBuffers=NULL;				/* Index to buffers			*/
EXPORT SPECTRUM *RbsTempBuf=NULL;				/* Temporary work buffer	*/
EXPORT SPECTRUM *RbsActiveBuf=NULL;				/* Current active buffer	*/
EXPORT int		 RbsNumBuf=20;						/* # of buffers configured	*/

int RbsRecoilZLimit=1;						/* Only handle H/D recoils	*/

BOOL        RbsAutoReturn  = TRUE;		/* Autoreturn from sub-process	*/
RBSPROMPT   RbsPromptMode  = ABUSIVE;	/* Prompting mode (nice/abusive) */
BOOL	      RbsQueryOnExit = TRUE;		/* Query before exit if mods		*/
DENSITYCALC RbsDensityCalc = IMPROVED;	/* Mode for density calculations	*/

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static CMTYPE cmlist[] = {
	{"?",			 	 1, CF_HELP},			/*  Command list				*/
	{"help",			 4, CF_HELP},
	{"parameters",	 4, CF_PARM},			/*  Parameter listing		*/
	{"parms", 		-3, CF_PARM},			/*  Synonym						*/
	{"status",		-4, CF_PARM},		
	{"buffers",		 3, CF_BUFFERS},
	{"density_calc",8, CF_DENSITY},
	{"fres_limit",	 4, CF_FRES},
	{"stop_load",	 6, CF_STOPLOAD},
	{"stop_read",	-9, CF_STOPLOAD},
	{"stop_table",	 9, CF_STOPTABLE},
	{"stop_type",   9, CF_STOPTYPE},

	{"user_stopping",6, CF_USERSTOP},	{"stop_user",   -6, CF_USERSTOP},
	{"user_xsect",  6, CF_USERCROSS},	{"user_crosssection", -7, CF_USERCROSS},

	{"prompt",		 6, CF_PROMPT},
	{"autoreturn",	 7, CF_AUTORETURN},
	{"datapath",	 4, CF_DATAPATH},		{"dpath", -5, CF_DATAPATH},
	{"dataexts",	 5, CF_DATAEXTS},
	{"tabcompletion",3, CF_TABCOMPLETION},
	{"exitquery",	 5, CF_EXITQUERY},
	{"writelevel",	 8, CF_WRITELEVEL},	{"write_level", -7, CF_WRITELEVEL},
	{"stopp",		 5, CF_STOPP},
	{"return",		 3, CF_RETURN},
	{NULL,       	 0, CF_RETURN}
};


/* ============================================================================
-- Subroutine to reset some important stuff.  Resets changable parameters
-- in the common block.
--
-- Usage: int RumpConfig(int key)
--
-- Inputs: key - Normal key codes
--               0       - enter command entry mode
--               U_INIT  - initialize
--               U_RESET - reset
--               U_HELP  - ignored (internal process has help list)
--               U_PARM  - parameters listing
--               U_QUIT  - prepare to quit
--
-- Returns: 0 => success
--
-- Output: Reset common block variables to initial values
============================================================================ */
BOOL RbsConfig(int key) {

	int i, itmp;
#ifdef NT
	int rcode;
#endif
	BOOL ltmp;
	char	 token[LONG_STR_SIZE], *ext;
	CMTYPE *citem;

	if (key == U_INIT || key == U_RESET) {				/* Login prompt */
		RbsPrintCopyright();
		GVLinkReal("$RUMP",         GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, (REAL *) &RumpVersionNumber);
		GVLinkReal("$RUMP_VERSION", GVF_HIDDEN | GVF_INTERNAL | GVF_CONSTANT, (REAL *) &RumpVersionNumber);
		GVLinkReal("$SYMSIZ",		 GVF_HIDDEN | GVF_INTERNAL,                         &Rmp->SymSize);	/* Only there for old compatability */
	}

	switch (key) {
		case U_INIT:											/* First time init */
		case U_RESET:
			if (key == U_INIT) {
				RbsConfigPath = RbsGetSearchPath();
				LoadAtomicData();
				RbsLoadZieglerData("pscoef.dat");
				RbsLoadKalbitzerData("newstop.kal");
				SimLoadDensityTable("density.tab");
				TTYprintf("\n");
			}
			if (RbsBuffers != NULL) {
				for (i=0; i<=RbsNumBuf; i++) RbsFreeSpectrum(RbsBuffers[i]);
				free(RbsBuffers);
			}
			RbsBuffers = calloc(RbsNumBuf+1, sizeof(*RbsBuffers));
			for (i=0; i<=RbsNumBuf; i++) RbsBuffers[i] = RbsAllocateSpectrum(NULL, CMAX);
			RbsActiveBuf = MAINBUF;

			RbsFreeSpectrum(RbsTempBuf);
			RbsTempBuf = RbsAllocateSpectrum(NULL, CMAX);
			break;

		case U_PARM:
			TTYprintf(" Config path:   %s\n", RbsConfigPath);
			TTYprintf(" RBS data path: %s\n", RbsSearchPath);
			TTYprintf(" RBS file exts: %s\n", RbsSearchExts);
			TTYprintf(" # of buffers:  %d    FRES Z limit: %d\n",
				RbsNumBuf, RbsRecoilZLimit);
			TTYprintf(" Prompts: %s   Autoreturn mode: %s\n", 
				(RbsPromptMode == NICE) ? "Polite" : "Abusive",
				(RbsAutoReturn) ? "Automatic" : "RETURN only");
			break;
	}
	if (key != 0) return(TRUE);

	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), "Configuration subcommand: ")) {
			continue;
		} else if (LexAlias(token, sizeof(token))) {
			continue;							/* Check for alias */
		} else if ( (citem = LexCmdl(token, cmlist, sizeof(*cmlist))) == NULL) {
			if (LexSystem(0, token)) continue;

			if (RbsAutoReturn) {
				LexBackup();
				return(TRUE);
			}
			ERRprintf("ERROR: %s unrecognized CONFIGURE sub-command\n", token);
			LexFlush();
			continue;
		}

		switch (citem->rcode) {				/* Process commands */
			case CF_HELP:
				LexCmdlPrintEx(cmlist, sizeof(*cmlist), 13, "Configure Subcommands: ");
				LexSystem(U_HELP, NULL);
				break;
			case CF_PARM:
				TTYprintf(" Config path:   %s\n", RbsConfigPath);
				TTYprintf(" Tab completion mode: %s\n", (TabCompletionMode == FULL_COMPLETE) ? "Full" : "Partial");

				TTYprintf(" RBS data path: %s\n", RbsSearchPath);
				TTYprintf(" RBS file exts: %s\n", RbsSearchExts);
				TTYprintf(" Write level:   %d (minor revision level)\n", RbsSetFileWriteVersion(-1));
				TTYprintf(" # of buffers:  %d\n", RbsNumBuf);
				TTYprintf(" Stopping fits: %s\n", (stop_type == STOP_SQRT) ? "SQRT" : "LINEAR");
				TTYprintf(" Density calcs: %s\n", (RbsDensityCalc == IMPROVED) ? "Improved" : "Compatible (Version 1)");
				TTYprintf(" FRES Z limit:  %d\n", RbsRecoilZLimit);
				TTYprintf(" Prompt mode:   %s\n", 
							 (RbsPromptMode == NICE) ? "Polite" : "Abusive");
				TTYprintf(" Autoreturn:    %s\n", 
							 RbsAutoReturn ? "Automatic" : "RETURN only");
				break;
			case CF_RETURN:
				return(TRUE);
				
			case CF_DENSITY:
				if (! LexGetTokenP(token, sizeof(token), "Density calc mode [Improved | Compatible | ?] (abort): ")) break;
				if (strcmp(token, "?") == 0 || strcmp(token, "-?") == 0) {
					TTYputs("\n"
"  The density approximation for compounds, used by SIM to convert physical\n"
"  thicknesses into areal densities, was changed between version 1.0 (DOS) of\n"
"  RUMP and version 2.0.  The new method is more accurate but will change the\n"
"  result of old simulations.  Specifying COMPATIBLE as the calculation mode\n"
"  will force SIM to use the old calculations.  Thicknesses are still really\n"
"  only valid for compounds if the density is specified (via SETDENSITY) or\n"
"  if you work in atoms/cm^2 (cm2) or molecules/cm^2 (m/cm2).\n\n");
				} else if (LexEqual(token, "IMPROVED", 1) || LexEqual(token, "NEW", 3)) {
					RbsDensityCalc = IMPROVED;
				} else if (LexEqual(token, "COMPATIBLE", 1) || LexEqual(token, "OLD", 3)) {
					RbsDensityCalc = COMPATIBLE;
				} else {
					ERRprintf("ERROR: %s is an invalid response to DENSITY_CALC - try ?\n", token);
				}
				break;

			case CF_BUFFERS:
				itmp = LexGetInt(RbsNumBuf, "Number of buffers desired (no change): ");
				if (LexEscape(TRUE) || itmp == RbsNumBuf || itmp <= 0) continue;
				if (itmp < RbsNumBuf) {
					for (i=RbsNumBuf; i>itmp; i--) RbsFreeSpectrum(RbsBuffers[i]);
				} else {
					RbsBuffers = realloc(RbsBuffers, (itmp+1)*sizeof(*RbsBuffers));
					for (i=RbsNumBuf+1; i<=itmp; i++) RbsBuffers[i] = RbsAllocateSpectrum(NULL, CMAX);
				}
				RbsNumBuf = itmp;
				RbsActiveBuf = MAINBUF;
				TTYprintf("Now have %d buffers.  First buffer is active\n", RbsNumBuf);
				break;

			case CF_FRES:
				itmp = LexGetInt(RbsRecoilZLimit, "Highest Z to be simulated in forward recoils (no change): ");
				if (! LexEscape(TRUE) && itmp >= 0) RbsRecoilZLimit = itmp;
				break;

			case CF_STOPP:
				RbsStopp();
				break;

			case CF_STOPLOAD:
				if (! LexGetFileP(token, sizeof(token), "Stopping power (exception, Kalbitzer format) file: (-unload to release): ")) break;
				if (LexEqual(token, "-unload", 3)) {
					RbsLoadKalbitzerData(NULL);
				} else {
					SysAddExt(token, ".stp");
					ext = strchr(token, '.');			/* Find extension point */
					if (ext != NULL && stricmp(ext, ".kal") == 0) {
						if (! RbsLoadKalbitzerData(token)) LexFlush();
					} else {
						if (! RbsLoadStopTable(token)) LexFlush();
					}
				}
				FreeStopTables();
				break;

			case CF_STOPTYPE:
				stop_type = 
					LexChoice(FALSE, "SQRT", "LINEAR", "Stopping power fit mode: SQRT or LINEAR (LINEAR): ")
					? STOP_SQRT : STOP_LINEAR ;
				Rmp->autsim = -1;							/* Flag obsolete simulation */
				break;

			case CF_STOPTABLE:
				{
					REAL low, high, cut, mb;
					int zb;

					if (! LexGetTokenP(token, sizeof(token), "Particle - He,3H... (abort): ")) break;
					if (LexEqual(token, "D",   1)) strcpy(token, "2H" );
					if (LexEqual(token, "HE3", 3)) strcpy(token, "3HE");
					if (! RbsIdentp(token, &zb, &itmp, NULL)) {
						ERRprintf("ERROR: Particle %s unrecognized\n", token);
						break;
					}
					mb = RbsGetRealMass(zb, itmp);
					low  = LexGetReal(0.10f,  "Lower energy (MeV) of fit to analytical models (0.10 MeV): ");
					high = LexGetReal(3.50f,  "Upper energy (MeV) of fit to analytical models (3.50 MeV): ");
					cut  = LexGetReal(low/2, "Lower cutoff for simulations (emin/2): ");
					if (! RbsStpCreate(zb,mb, low,high,cut)) LexFlush();
					Rmp->autsim = -1;						/* Flag obsolete simulation */
				}
				break;
				
			case CF_USERSTOP:								/* User stopping routine */
#ifdef NT
				rcode = LoadMyDLL(&hmod_UserZStop, (void **) (&UserZStop), "ZStop", "stopping power");
				if (rcode == 2) LexFlush();
				if (rcode == 0) FreeStopTables();
				break;
#else
				ERRprintf("ERROR: User stopping power DLL supported only for NT\n");
				LexFlush();
				break;
#endif
				
			case CF_USERCROSS:
#ifdef NT
				rcode = LoadMyDLL(&hmod_UserCrossSection, (void **) (&UserCrossSection), "CrossSection", "cross section");
				if (rcode == 2) LexFlush();
				break;
#else
				ERRprintf("ERROR: User cross section DLL supported only for NT\n");
				LexFlush();
				break;
#endif
				
			case CF_PROMPT:
				if (! LexGetTokenP(token, sizeof(token), "Prompting mode (nice | abusive): (no change) ")) break;
				if (LexEscape(TRUE)) break;
				if ( (i = LexSelect(token, "nice polite abusive")) <= 0) {
					ERRprintf("ERROR: %s invalid mode - mode unchanged\n", token);
				} else {
					RbsPromptMode = (i == 1 || i == 2) ? NICE : ABUSIVE ;
				}
				break;
				
			case CF_AUTORETURN:
				if (! LexGetTokenP(token, sizeof(token), "Autoreturn from sub-modules? (ON | OFF): (no change) ")) break;
				if (LexEscape(TRUE)) break;
				if ( (i = LexSelect(token, "on yes true off no false")) <= 0) {
					ERRprintf("ERROR: %s invalid mode - mode unchanged\n", token);
				} else {
					RbsAutoReturn = (i <= 3) ? TRUE : FALSE ;
				}
				break;

			case CF_DATAPATH:
				if (LexGetSearchPathP(token, sizeof(token), "Search path(s) for reading data files (unchanged): ")) {
					if (! LexEscape(TRUE)) strcpy(RbsSearchPath, token);
				}
				break;

			case CF_DATAEXTS:
				if (LexGetSearchPathP(token, sizeof(token), "Extension list for data file search (unchanged): ")) {
					if (! LexEscape(TRUE)) strcpy(RbsSearchExts, token);
				}
				break;

			case CF_TABCOMPLETION:
				if (LexGetTokenP(token, sizeof(token), "Tab filename completion mode (Partial | Full): ")) {
					if (LexEqual(token, "Partial", 1)) {
						TabCompletionMode = PARTIAL_COMPLETE;
					} else if (LexEqual(token, "Full", 1)) {
						TabCompletionMode = FULL_COMPLETE;
					} else {
						ERRprintf("ERROR: Invalid type of filename completion modes (%s)\n", token);
					}
				}
				break;

			case CF_EXITQUERY:
				ltmp = LexYesNo(TRUE, "Query on exit if buffers unwritten? (YES): ");
				if (! LexEscape(TRUE)) RbsQueryOnExit = ltmp;
				break;

			case CF_WRITELEVEL:
				itmp = LexGetInt(RbsSetFileWriteVersion(-1), "Minor file write level (0=DOS level, 1=better compress): (no change): ");
				if (! LexEscape(TRUE)) RbsSetFileWriteVersion(itmp);
				break;

			default:
				ERRprintf("ERROR: Developers blew it again (PERT)\n");
				break;
		}
	}
	panic; return(FALSE);
}


/* ===========================================================================
-- Routine to query the RBS search path from the init file
=========================================================================== */
char *RbsGetSearchPath(void) {

	static char *path=NULL;								/* Make it static */

	if (path == NULL) {
		path = getenv("RUMP_PATH");					/* environment can override */
#ifdef OS2
		if (path == NULL) {
			path = calloc(PATH_MAX,1);
			if (! PrfQueryProfileString(HINI_PROFILE, "RUMP", "PATH", NULL, path, PATH_MAX)) {
				free(path);
				path = NULL;
			}
		}
#elif defined NT
		if (path == NULL) {
			LONG rc;
			HKEY hkey;
			DWORD isize, itype;
			BOOL GotHive;
			char data[LONG_STR_SIZE], dataex[LONG_STR_SIZE];

			/* Look in both HKEY_CURRENT_USER and HKEY_LOCAL_MACHINE for serial number key */
			GotHive = FALSE;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
				isize = sizeof(data);
				GotHive = RegQueryValueEx(hkey, "Serial", NULL, &itype, data, &isize) == ERROR_SUCCESS;
				if (! GotHive) RegCloseKey(hkey);
			}
			if (! GotHive && RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Computer Graphic Service\\Genplot/Rump\\2.0", 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
				isize = sizeof(data);
				GotHive = RegQueryValueEx(hkey, "Serial", NULL, &itype, data, &isize) == ERROR_SUCCESS;
				if (! GotHive) RegCloseKey(hkey);
			}
			if (GotHive) {
				isize = sizeof(data);
				rc = RegQueryValueEx(hkey, "RUMP_PATH", NULL, &itype, data, &isize);
				RegCloseKey(hkey);
				if (rc == ERROR_SUCCESS && itype == REG_SZ) {
					ExpandEnvironmentStrings(data, dataex, sizeof(dataex));
					path = strdup(dataex);
				}
			}
		}
#endif
		if (path==NULL) path = (char *) RumpDefaultConfigPath;
	}
	return(path);
}


/* ===========================================================================
-- Routine to free the stopping power tables currently loaded in RUMP
=========================================================================== */
static void FreeStopTables(void) {
	STOPPING_TABLE *tab,*tab2;
	tab = stop_tables;					/* Free all tables */
	while (tab != NULL) {tab2=tab; tab=tab->next; free(tab2);}
	stop_tables = NULL;
	Rmp->autsim = -1;						/* Flag obsolete simulation */
	return;
}


/* ===========================================================================
-- Routine to load the initial atomic data such as density, name, etc.
=========================================================================== */
static void LoadAtomicData(void) {

	char *aptr;

	if (*RbsAtomicDataFile == '\0') {			/* Read atomic data file */
		if ( (aptr=getenv("ATOM4.DAT")) == NULL) aptr = getenv("atom4.dat");
		if (aptr == NULL) aptr = "atom4.dat";
		strcpy(RbsAtomicDataFile, aptr);
	}

	if (! RbsLoadAtomicData(RbsAtomicDataFile))
		ERRprintf("WARNING: Atomic data %s failed to load\n", RbsAtomicDataFile);

	return;
}

/* ===========================================================================
=========================================================================== */
void RbsPrintCopyright(void) {
	TTYprintf("\n"
"/=============================================================================\\\n"
"|         RUMP - RBS Analysis and Simulation Package [v. %.2f(beta)]          |\n"
"|      (c) 1988-2002 Michael Thompson, Larry Doolittle                        |\n"
"|      (c) 1988-2002 Computer Graphic Service, Ltd.  All rights reserved      |\n"
"|         Serial Number:  %-17s                                   |\n"
"|         Revision Level: %-25s                           |\n"
"|         Revision Date:  %-25s                           |\n"
"|         Compile Date:   %-25s                           |\n"
"\\=============================================================================/\n"
"\n", RumpVersionNumber, SysSerialNumber, RumpRevisionLevel, RumpRevisionDate, RumpLinkDate);
	return;
}


#ifdef NT

/* ===========================================================================
-- Routine for general handling of replacement routine loading.  This routine
-- handles most of the prompting and NT system calls to establish alternate
-- routines.
--
-- Usage: LoadMyDLL(HMODULE *hmod, void **routine, char *entry, char *prompt);
--
-- Inputs: hmod    - pointer to a hmodule describing the DLL.  Must be 
--                   returned valid if unload is to work on same modules.
--         routine - pointer to the variable of a function call.  Actual def
--                   of routine is more likely int (*routine)(...)
--         entry   - the specific entry of the DLL which will become routine
--         prompt  - generic info name of the load (ie. stopping power)
--
-- Output: *hmod    - new dynamic load module handle
--         *routine - new routine entry
--
-- Return: 0 -> all okay
--         1 -> abort via user request
--         2 -> failure (message printed)
=========================================================================== */
static int LoadMyDLL(HMODULE *hmod, void **routine, char *entry, char *prompt) {

	char text[DFLT_STR_SIZE], name[DFLT_STR_SIZE], token[DFLT_STR_SIZE];

	sprintf(text, "User %s DLL (<cr> aborts, - closes current): ", prompt);
	if (! LexGetFileP(token, sizeof(token), text)) return(1);

/* Check for options, allowing only -cancel, or simple - */
	if (strcmp(token, "-") == 0) strcpy(token, "-cancel");		/* New default */
	if (*token == '-' && ! LexEqual(token, "-cancel", 2)) {
		ERRprintf("ERROR: Invalid option (%s).  Use -cancel to unload.\n", token);
		return(2);
	}

/* Free the current module if it is defined */
	if (*routine != NULL) {
		if (*hmod != NULL) FreeLibrary(*hmod);
		*hmod = NULL;
		*routine = NULL;
		TTYprintf("MSG: Previous user %s routine has been released\n", prompt);
	}
	if (*token == '-') return(0);						/* Time to return */

/* Try to find this DLL */
	if (! SysFindFile(name, token, RbsConfigPath, ".dll", R_OK)) {
		ERRprintf("ERROR: User file %s could not be found\n", name);
		LexFlush();
		return(2);
	}

#if 0
	if (! SysResolveDyntName(name, token, sizeof(name))) {
		SysAddExt(token, ".dll");
		if (! SysResolveDyntName(name, token, sizeof(name))) {
			ERRprintf("ERROR: User file %s could not be found\n", name);
			LexFlush();
			return(2);
		}
	}
#endif

/* Load the library */
	if ( (*hmod=LoadLibrary(name)) == NULL) {
		ERRprintf("ERROR: User module not loaded (rc=%i, fail=%s)\n", GetLastError(), name);
		LexFlush();
		return(2);
	}

/* And get the procedure address */
	*routine = (void *) GetProcAddress(*hmod, entry);
	if (*routine == NULL) {
		ERRprintf("ERROR: Export routine %s() not found in %s\n", entry, name);
		FreeLibrary(*hmod);
		*hmod = NULL;
		LexFlush();
	} else {
		TTYprintf("MSG: %s() from %s will be used for %s\n", entry, name, prompt);
	}

	return(0);
}

#endif
				
#ifdef UNUSED_CODE_REPLACED_WITH_BETTER

				if (LexGetFileP(token, sizeof(token), "User stopping power DLL (<cr> aborts, - terminates current): ")) {
					char name[DFLT_STR_SIZE];

					if (UserZStop != NULL) {			/* Free current module */
						if (hmod != NULL) FreeLibrary(hmod);
						hmod = NULL;
						UserZStop = NULL;
						TTYprintf("MSG: Previous user stopping power routine has been released\n");
					}

					FreeStopTables();

					if (*token == '-') break;

					if (! SysResolveDyntName(name, token, sizeof(name))) {
						SysAddExt(token, ".dll");
						if (! SysResolveDyntName(name, token, sizeof(name))) {
							ERRprintf("ERROR: User file %s could not be found\n", name);
							LexFlush();
							break;
						}
					}
					if ( (hmod=LoadLibrary(name)) == NULL) {
						ERRprintf("ERROR: User module not loaded (rc=%i, fail=%s)\n", GetLastError(), name);
						LexFlush();
						break;
					}

					UserZStop = (int (*)(int,double,int,double,REAL *,REAL *,int)) GetProcAddress(hmod,"ZStop");
					if (UserZStop == NULL) {
						ERRprintf("ERROR: Export routine ZStop() not found in %s\n", name);
						FreeLibrary(hmod);
						LexFlush();
					} else {
						TTYprintf("MSG: ZStop() from %s will be used for stopping powers\n", name);
					}
				}
				break;
#endif
