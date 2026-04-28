/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/*  rump.c -- ported from rump.f77 -- renamed from "TRACOR" 5/3/84 */
/* ===========================================================================
--  Originated by:
--      Michael O. Thompson
--      Dept. of Material Science
--      Bard Hall, Cornell University
--      Ithaca, N.Y.  14853
--
--  Current implementation:
--      Lawrence R. Doolittle     ( doolittle@cebaf )
--
--  With help from:
--      Michael Thompson          Dept. of Material Science
--      Peter Revesz              Bard Hall, Cornell University
--      Gyorgy Vizkelethy         Ithaca, N.Y.   14853
--      Mike Uttormark
--
-- Just for laughs, the original introduction to the code from spring 1982:
--  PROGRAM FOR GRAPHIC ANALYSIS OF RBS DATA.  SHOULD USE MAINLY THE
--  TEKTRONIX GRAPHICS UNIT.  ALLOWS FOR EXPANSION OF PROGRAM AT LATER DATE.
=========================================================================== */

/* ===========================================================================
--  Usage Guide:
--
--      PROGRAM RUMP
--     This is where it all begins.  The commands are prompted for and
--     read by this program, then passed along to the subprocessors.
--     Commands ?, PAR, and QUIT recieve individual attention here.
--     The PAUSE command has now moved to more comfortable living
--     quarters in the SYSTEM subprocessor.
--     This "Program" is now a subroutine, to allow for consistent
--     invocation, command line parsing, and error reporting among
--     all the different computers involved.
--  Quick: Where it all begins
--
--     INPUTS:   INPSTR   Command line argument string, if any.
--                        Currently used to designate a macro file to be run.
--
--     OUTPUTS:  RCODE    Allows for error reporting - basically not used
--
--     COMMON BLOCKS:     None
--     CALLED FROM:       General purpose front end routine (System dependant)
--     CALLS:             SYSTEM, ANLYTC, BMANIP, APLOT, SITE
--
--  5/6/86 - MOT
--    Modified code handling of an initialization file.  Previously configured
--    to use command line.  Now defaults to RUMP.INI if command line is NULL.
=========================================================================== */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#define	RUMP_NOTICE_NAME		"rump.notice"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
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
#define	RUMP_C_SOURCE
#include "rump.h"
#include "tplot.h"
#include "../genplot/genplot.h"			/* For GENPLOT */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	int  rcode;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void set_return_code(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */
EXPORT RMPTYPE *RbsDataBlock=NULL;			/* Primary information block - aliased as Rmp */


/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static int ReturnCode=0;

static char *cmprmt[] = {
	"Your wish? ",				"Ready if you are! ",	"Yes Master? ",
	"You called? ",			"Yes dear? ",				"At your service! ",
	"Beam me up Scottie! ",	"Up periscope! ",			"Feed me! ",
	"Here I am! ",				"Next? ",					"Hey, man, what next? ",
	"Next command: ",			"Whoopee! ",				"Go for it! ",
	"I want a cookie!! ",	"Igen, uram! "
};
#define NPROMPT (sizeof(cmprmt)/sizeof(cmprmt[0]))

#define	RB_GENPLOT		1
#define	RB_RETURN		2
#define	RB_RUMP			3
#define	RB_RUMPQUIET	4
#define	RB_CONFIGURE	5
#define	RB_SIM			6
#define	RB_PERT			7
#define	RB_VERSION		8

static CMTYPE cmlist[] = {
	{"?",			 	 1, U_HELP},			/*  Command list				*/
	{"PARAMETERS",	 4, U_PARM},			/*  Parameter listing		*/
	{"PARMS", 		-3, U_PARM},			/*  Synonym						*/
	{"STATUS",		-4, U_PARM},		
	{"RESET",	    5, U_RESET},			/*  Reset the world			*/
	{"RESTART", 	 7, U_INIT},			/*  Full restart command	*/
	{"CONFIGURE",	 4, RB_CONFIGURE},	/*  Configure special opts	*/
	{"VERSION",     3, RB_VERSION},     /*  Print out the current version */
	{"GENPLOT:",    4, RB_GENPLOT},		/*  GENPLOT with a buffer	*/
	{"GENPLT:",		-6, RB_GENPLOT},
	{"RUMP",			-4, RB_RUMP},
	{"RUMP:",		-5, RB_RUMPQUIET},
	{"RUMP_LEVEL",	-5, RB_RUMPQUIET},	{"RUMPLEVEL:",	-5, RB_RUMPQUIET},
	{"sim",			 3, RB_SIM},
	{"pert",			 3, RB_PERT},
	{"RETURN",		 3, RB_RETURN},
	{"QUIT", 		 1, U_QUIT},			/*  Full quit */
	{"BYE", 			-2, U_QUIT},			/*  Synonym */
	{NULL,       	 0, 0}
};


/* ============================================================================
-- Shell program for dealing with upper level of RUMP.
--
-- Inputs: text and command line arguments
--
-- Output: none
--
-- Returns: 0xHHLL - LL is the normal return code.  Values normally given as
--                      EXIT_FAILURE (-fail option)
--                      EXIT_SUCCESS (-succeed and default)
--                      user value   (-rcode option)
--                   HH is the source of the exit.
--                      00 => return via a "RETURN" command
--                      01 => return via a "QUIT" command
============================================================================ */
#define NRND  2									/*  Weight factor for prompts */

int Rump(void) {

/*  -- Local variables -- */
	char *aptr, token[OPTION_STR_SIZE];		/* For command inputs			*/
	static int FirstTime=TRUE;					/* Indicate initialize done	*/
	static int jcli;
	int i, itype, key;							/* Random variables				*/
	int FullReset;
	BOOL do_channel;
	CURVE *curve=NULL;
	CMTYPE *lcmd;
	SPECTRUM *ibf;

/* ... Code begin */
	if (FirstTime) {										/* Is this the first time */
		FILE *funit;										/* For the welcome file */
		char filename[PATH_MAX];						/* And file/text output	*/

		FirstTime = FALSE;								/* So now we've done it */

		CONInitialize();									/* Make sure all initialized */
		LexInitialize();									/* All the way up */
		PlotInitialize();
		LexSystem(U_INIT, NULL);
		PlotSystem(U_INIT, NULL, NULL);

		SysResolveDyntName(filename, RUMP_NOTICE_NAME, sizeof(filename));
		if ( (funit=fopen(filename, "r")) != NULL) {
 		   while (fgets(filename, sizeof(filename), funit) != NULL) 
 			   TTYputs(filename);
 		   fclose(funit);
		}

		Rmp = malloc(sizeof(RMPTYPE));

		Rmp->trtype = 0;
		Rmp->autsim = 0;
		Rmp->taplot = 0;
		Rmp->raw    = FALSE;

		Rmp->chrsiz=0.16f;				  	/* = 0.16 What, me worry?			*/

		Rmp->ymin=0;	Rmp->ymax=3000;	/* Min and Max counts on plot		*/
  		Rmp->emin=0.5;	Rmp->emax=3.0;		/* Min and Max energy on plot		*/
  		Rmp->chmin=100;Rmp->chmax=600;	/* User input Channel min/max		*/
		Rmp->cospec=0;							/* User input max Counts			*/

		Rmp->forcex=FALSE;					/* Force X axis (Channels)?		*/
   	Rmp->autoy=TRUE;						/* Autoscale counts?					*/
	   Rmp->mtick=TRUE;						/* Minor ticks?						*/
      Rmp->autoid=FALSE;					/* Is AUTOID enabled					*/
		Rmp->raw=FALSE;						/* TRUE => Raw Data					*/

		Rmp->AutoLineType = FALSE;
		Rmp->AutoSymbols  = FALSE;
		Rmp->SymSize  = 0.10f;				/* Default is 0.10"					*/
		Rmp->linetype = Rmp->linetypestart = 0;
		Rmp->symtype  = Rmp->symtypestart  = 0;
		Rmp->npoint = 1;

		Rmp->xaxis_mode = X_ENERGY;		/* Default plot versus energy		*/
		Rmp->linear=GR_LIN;					/* Vertical scale style				*/
		Rmp->labels=GR_ON;					/* Labels mode?						*/

		GptRumpLink = Rump;				/* Register ourselves now with GENPLOT */

		RbsConfig(U_INIT);				/* Set parameters (default)			*/
		SimReset(U_INIT);					/* Initialize simulation structure	*/
		ResReset(U_INIT);					/* Initialize resonance package		*/
		PertReset(U_INIT);				/* Initialize pert package				*/

		RbsBmanip(U_INIT, "RUMP");		/* Init sub-processors					*/
		RbsAnlytc(U_INIT, "RUMP");
		RbsAplot (U_INIT, "RUMP");
		RbsSite  (U_INIT, "RUMP");

		jcli = (int) time(NULL) % NPROMPT;
	}

/* ********************** COMMAND PROCESSING ***************************** */

	while (TRUE) {

		if (RbsPromptMode == NICE) {								/* Which prompt */
			aptr = "RUMP: ";
		} else {
			aptr = cmprmt[jcli];
			jcli = (int) (NPROMPT*pow(((REAL) rand())/RAND_MAX, NRND));	/* Next index	*/
		}

		if (! LexGetTokenP(token, sizeof(token), aptr) ) continue;
		if (LexAlias(token, sizeof(token))) continue;			/* Alias check	*/
		GVLinkArray("Y", GVF_INTERNAL, ibuf->counts, ibuf->nptmax, &ibuf->npt);
		GVLinkInt("NPT", GVF_INTERNAL, &ibuf->npt);

/*  ... Commands above 5 are handled by this routine */
		if ( (lcmd = LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL )   {
			key = lcmd->rcode;
			*token = '\0';
		} else if (RbsBmanip(0, token)) {
			continue;
		} else if (RbsAnlytc(0, token)) {
			continue;
		} else if (RbsAplot(0, token)) {
			continue;
		} else if (LexSystem(0, token)) {
			continue;
		} else if (PlotSystem(0, token, NULL)) {
		   continue;
		} else if (RbsSite(0, token)) {
			continue;
		} else {
			ERRprintf("ERROR: (%s) Unrecognized RUMP command\n", token);
			LexFlush();						/*  Warn of line termination */
			continue;
		}

/* ... Here for basic general commands.  Have everyone else do first */
/*		if (key == U_HELP && isatty(fileno(stdin))) ScrClearAttrib(D_NORMAL); */
		switch (key) {
		   case U_INIT:
				LexSystem (key, NULL);
				PlotSystem(key, NULL, NULL);

				RbsConfig(key);
				SimReset(key);
				ResReset(key);
				PertReset(key);

				RbsBmanip(key, NULL);
				RbsAnlytc(key, NULL);
				RbsAplot (key, NULL);
				RbsSite  (key, NULL);

				TTYprintf("Type ? for a list of the commands.\n");
				TTYputs("\n");
				break;

			case U_RESET:
				FullReset = FALSE;
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-full", 2)) {
						FullReset = TRUE;
					} else if (LexEqual(token, "-partial", 2)) {
						FullReset = FALSE;
					} else {
						ERRprintf("ERROR: Unrecognized RESET option ignored: %s\n", token);
					}
				}

				LexReset(FullReset);				/* Handles LexSystem also	*/
				PlotReset(FullReset);			/* Does PlotSystem also		*/

				RbsConfig(key);
				SimReset(key);
				ResReset(key);
				PertReset(key);

				RbsBmanip(key, NULL);
				RbsAnlytc(key, NULL);
				RbsAplot (key, NULL);
				RbsSite  (key, NULL);
				break;

			case U_HELP:
				LexCmdlPrint(cmlist, sizeof(CMTYPE), "Main Level Commands");
				RbsBmanip(key, NULL);
				RbsAnlytc(key, NULL);
				RbsAplot (key, NULL);
				RbsSite  (key, NULL);
				LexSystem (key, NULL);
				PlotSystem(key, NULL, NULL);
				TTYputs("\n");
				break;

			case U_PARM:
				RbsBmanip(key, NULL);
				RbsAnlytc(key, NULL);
				RbsAplot (key, NULL);
				RbsSite  (key, NULL);
				RbsConfig(key);
				LexSystem (key, NULL);
				PlotSystem(key, NULL, NULL);
				TTYputs("\n");
				break;

			case U_QUIT:
				if (RbsShutDown() != 0) break;	/* Do local stuff and check */

				RbsSite  (key, NULL);		/* Called in reverse order for quit */
				RbsAplot (key, NULL);
				RbsAnlytc(key, NULL);
				RbsBmanip(key, NULL);
				RbsConfig(key);
				LexSystem (key, NULL);
				PlotSystem(key, NULL, NULL);
				set_return_code();
				ReturnCode |= 0x0100;		/* Indicate return via QUIT */

				GptShutDown();					/* Does everything I need	 */
				return(ReturnCode);

			case RB_RETURN:
				set_return_code();
				return(ReturnCode);

			case RB_RUMP:
				TTYprintf("RUMP: Rutherford Universal Manipulation Program\n");
				TTYprintf("RUMP: Really Ugly and Mangled Procrastination tool\n");
				TTYprintf("RUMP: Reasonably United Management Principles\n");
				TTYprintf("RUMP: Release Unconventional Masters from Prison\n");
				break;

			case RB_RUMPQUIET:			/* Just gets us to RUMP level commands */
				break;

			case RB_CONFIGURE:
				if (! RbsConfig(0)) LexFlush();	/* User configuration commands */
				break;

			case RB_SIM:
				if (! RbsSimMain(0)) LexFlush();
				break;

			case RB_PERT:
				if (! RbsPertMain(0)) LexFlush();
				break;

			case RB_VERSION:
				RbsPrintCopyright();
				break;
				
			case RB_GENPLOT:
				if ( (ibf = RbsGetBuf("Buffer (active): ", RbsActiveBuf)) == NULL)
					break;

				do_channel = FALSE;
				while (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-channel", 2)) {
						do_channel = TRUE;
					} else if (LexEqual(token, "-energy", 2)) {
						do_channel = FALSE;
					} else {
						ERRprintf("ERROR: Unrecognized GENPLOT transfer option ignored: %s\n", token);
					}
				}

				if (! GVGetInfo("PLOT", &itype, (void **) &curve) ||
					(itype != GV_2DCURVE && itype != GV_3DCURVE) ) {
					GVAlloc2DCurve("PLOT", 0, CMAX);
					if (! GVGetInfo("PLOT", &itype, (void **) &curve) || itype != GV_2DCURVE)
						break;
				}
				curve = *( (CURVE **) curve);

				curve->npt = ibf->npt;
				
				if (curve->npt > curve->nptmax) {
					GVResize("PLOT", curve->npt);
					curve->npt = curve->nptmax;
				}
				strscpy(curve->ids, ibf->id, sizeof(curve->ids));
				for (i=0; i<curve->npt; i++) {
					curve->x[i] = (REAL) ((do_channel) ? (i+ibf->first) : (1000.0*RBSENERGY(i+ibf->first,ibf)));
					curve->y[i] = ibf->counts[i];
				}
				
				GVLinkString("rbs:filename", 0, ibf->filename, sizeof(ibf->filename));
				GVLinkString("rbs:date",	0, ibf->date, sizeof(ibf->date));
				GVLinkString("rbs:ltct",	0, ibf->ltct, sizeof(ibf->ltct));
				GVLinkString("rbs:id",		0, ibf->id,   sizeof(ibf->id));

				GVLinkInt ("rbs:type",		0, (int *) &ibf->type);			/* Actually enum - tough */
				GVLinkReal("rbs:e0",			0, &ibf->e0);
				GVLinkInt ("rbs:zbeam",		0, &ibf->zbeam);
				GVLinkReal("rbs:mbeam",		0, &ibf->mbeam);
				GVLinkInt ("rbs:cbeam",		0, &ibf->cbeam);
				GVLinkReal("rbs:q",			0, &ibf->q);
				GVLinkReal("rbs:current",	0, &ibf->current);
				GVLinkReal("rbs:kevch",		0, &ibf->kevch);
				GVLinkReal("rbs:kev0",		0, &ibf->kev0);
				GVLinkReal("rbs:first",		0, &ibf->first);
				GVLinkReal("rbs:fwhm",		0, &ibf->fwhm);
				GVLinkReal("rbs:tau",		0, &ibf->tau);

				GVLinkInt ("rbs:geom",		0, (int *) &ibf->geom);			/* Actually enum - tough */
				GVLinkReal("rbs:phi",		0, &ibf->phi);
				GVLinkReal("rbs:theta",		0, &ibf->theta);
				GVLinkReal("rbs:psi",		0, &ibf->psi);
				GVLinkReal("rbs:omega",		0, &ibf->omega);
				GVLinkReal("rbs:corr",		0, &ibf->corr);
				GVLinkArray("rbs:counts",	0, ibf->counts, ibf->nptmax, &ibf->npt);
				GVLinkInt ("rbs:npt",		0, &ibf->npt);
				GVLinkInt ("rbs:modify",	0, &ibf->modify);
				GVLinkInt ("rbs:dirty",		0, &ibf->dirty);

				PlotSetRange(Rmp->emin,Rmp->emax,Rmp->ymin,Rmp->ymax);		/* just to start */

				ReturnCode = Genplot("PLOT");				/* It may "QUIT! */
				if (ReturnCode & 0x0100) return(ReturnCode);
				break;

			default:
				gen_err("Unknown error in module RUMP");
		}
	}

	return(EXIT_SUCCESS);
}


/* ===========================================================================
-- Routine to handle any shutdown operations, such as requesting filenames
-- to save opend files with.
=========================================================================== */
int RbsShutDown(void) {

	int i,j;
	BOOL mods, ltmp;
	char *fname;

	if (RbsQueryOnExit) {
		mods = FALSE;											/* No buffers out-of-date */
		for (i=1; i<=RbsNumBuf; i++) {					/*  Don't include SIM	  */
			if (RbsBuffers[i] == NULL || RbsBuffers[i]->npt == 0) continue;
			if (RbsBuffers[i]->dirty || RbsBuffers[i]->modify) {
				if (! mods)
					TTYprintf("\n Modified buffers which have not been saved\n");
				mods = TRUE;
				j = (int) strlen(RbsBuffers[i]->filename);
				fname = RbsBuffers[i]->filename + max(0,j-20);
				TTYprintf(" %2d %20s  %s\n", i,fname,RbsBuffers[i]->id);
			}
		}
		if (mods) {
			ltmp = LexYesNo(TRUE, "\nExit without saving these buffers? (YES) ");
			if (LexEscape(TRUE) || ! ltmp) return(1);		/* Escape bags out */
		}
	}
	return(0);
}


/* -------- set return code via command options ---------- */
static void set_return_code(void) {
	char token[OPTION_STR_SIZE];

	if (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-fail", 2)) {
			ReturnCode = EXIT_FAILURE;
		} else if (LexEqual(token, "-succeed", 2)) {
			ReturnCode = EXIT_SUCCESS;
		} else if (LexEqual(token, "-rcode", 3)) {
			ReturnCode = LexGetInt(EXIT_SUCCESS, "Return code: ");
		} else {
			ReturnCode = EXIT_FAILURE;
		}
	} else {
		ReturnCode = EXIT_SUCCESS;
	}
	return;
}
