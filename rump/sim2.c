/*  sim.c */

/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ----------------- */
/*  ---------                                              ----------------- */
/*  ---------    The source code to RUMP may be freely     ----------------- */
/*  ---------  modified as long as this copyright notice   ----------------- */
/*  ---------          is included and unchanged.          ----------------- */
/*  ------------------------------------------------------------------------ */

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
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "sample.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#if (defined OS2 || defined NT)
	#define SIM_FILE_EXTS ".lcm;.sim"
#else
	#define SIM_FILE_EXTS ".lcm:.sim:.LCM:.SIM"
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static UNITS *SimLookupUnit(char *token);
static LAYER *SimDeleteLayer(LAYER *layer);
static LAYER *SimAllocLayer(void);
static BOOL  SimLayerEmpty(LAYER *layer);
static LAYER *SimCheckLayer(LAYER *layer);
static BOOL  SimWriteSample(void);
static void	 samout2(int nlines, char *token);
static void	 RbsSplot(void);
static void  ShowDetail(SAMPLE *sample, int ilayer);

static BOOL	SimGlobalEmpty(GLOBAL_LAYER *g_layer);
static GLOBAL_LAYER *SimDeleteGlobal(GLOBAL_LAYER *g_layer);
static GLOBAL_LAYER *SimAllocGlobal(void);
static int SimGetGlobalNum(GLOBAL_LAYER *g_layer);
static GLOBAL_LAYER *SimCheckGlobal(GLOBAL_LAYER *layer);


/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
static SAMPLE *sample = NULL;
SAMPLE *SimDefaultSample = NULL;

/* Set values for sigtab/coffe2 to use unscaled internal routines */
REAL   sigtab[2] = {-1.0, -1.0};			/* Hydrogen constant scattering terms */
REAL   coffe2[2] = { 0.0,  0.0};			/* Hydrogen coefficients for 1/E^2    */

void (*SimPileup)(SPECTRUM *buf) = SimNewPileup;	/* Use new (good) one */

void (*SimInitFillSpectrum)(SPECTRUM *buf) = NULL;
void (*SimTermFillSpectrum)(SPECTRUM *buf) = NULL;
void (*SimFillSpectrum)(int z, REAL mass, REAL efront, REAL eback,
		REAL hfront, REAL hback, REAL qqq, REAL sigf, REAL sigb) = SimAnlyz;

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static UNITS angstroms = {"A",		ANGSTROMS,	1.0};
static UNITS atoms_cm2 = {"/cm2",	ATOMIC,		1.0};
static char   SimDensityFile[PATH_MAX]="";
static UNITS *SimDensityTable=NULL;
static int    SimDensityTableSize=0;
static int    SimDensityTableCount=0;

static SIMEQN eqlist[] = {
	{"None",				2, EQ_NONE,			0, {-1,-1,-1,-1,-1},	NULL,		   -1,
		"Turn off the equations"},
	{"Constant",		2, EQ_CONST,		1, { 0,-1,-1,-1,-1}, NULL,			 5,
		"Constant fraction of species in matrix"},
	{"Linear",			1,	EQ_LINEAR,		2, {12,13,-1,-1,-1}, NULL,			10,
		"Linearly varying fraction of species"},
	{"ERFC",				2, EQ_ERFC,			3, { 1, 2, 3,-1,-1}, NULL,			10,
		"Error function diffusion from constant surface conc"},
	{"Error",		  -3, EQ_ERFC,			3, { 1, 2, 3,-1,-1}, NULL,			20,
		"same as ERFC"},
	{"Exponential",	2, EQ_EXP,			3, { 1, 2, 4,-1,-1}, NULL,			20,
		"Diffusion in front of a moving interface (D/v form)"},
	{"Semi-infinite", 1,	EQ_SEMI_INF,	4, { 5, 2, 3, 6,-1}, NULL,			20,
		"Diffusion from a semi-infinite bulk layer"},
	{"Thinfilm",		4,	EQ_THINFILM,	3, { 7, 2, 3,-1,-1}, &angstroms,	30,
		"Diffusion from a thin surface layer"},
	{"BuriedThinFilm",3, EQ_BURIED,		4, { 7, 2, 3, 6,-1}, &angstroms,	30,
		"Diffusion from a thin buried layer"},
	{"ThickFilm",		4,	EQ_THICFILM,	5, { 9,10, 2, 3,11}, NULL,			20,
		"Diffusion from a thick surface layer"},
	{"Thicfilm",	  -4,	EQ_THICFILM,	5, { 9,10, 2, 3,11}, NULL,			20,
		"same as Thick film"},
	{"Timedependent", 2,	EQ_TIMEDEPE,	4, { 1, 2, 3, 8,-1}, NULL,			20,
		"Time dependent diffusion for polymers"},
	{"Gaussian",		1,	EQ_GAUSS,		3, {14,15,16,-1,-1}, &atoms_cm2,	20,
		"Gaussian profile from specified peak and FWHM"},
	{"Implant",			2,	EQ_GAUSS,		3, {14,15,16,-1,-1}, &atoms_cm2,	20,
		"Gaussian implant profile from peak and FWHM"},
	{"Edgeworth",		4, EQ_EDGEWORTH,	5, {17,18,19,20,21}, &atoms_cm2,	30,
		"Edgeworth implant profile w/ skewness and kurtosis"},
	{"Spline",			3, EQ_SPLINE,		5, {12,22,23,24,13}, NULL,			20,
		"Spline profile from knots"},
	{"UserEqn",			1, EQ_USER,			5, {25,25,25,25,25}, NULL,			20,
		"Arbitrary user equation for profile"},
	{NULL,				0,	EQ_NONE,			0, {-1,-1,-1,-1,-1}, NULL,			-1,
		NULL}
};

static char *eq_queries[] = {
	"Constant concentration (atomic fraction): ",				/* 0 */
	"Initial concentration (atomic fraction): ",					/* 1 */
	"Diffusion constant (cm^2/sec): ",								/* 2 */
	"Time (sec): ",														/* 3 */
	"Velocity (cm/sec): ",												/* 4 */
	"Maximum concentration (atomic fraction): ",					/* 5 */
	"Initial interface location (A or cm from front): ",		/* 6 */
	"Thickness of film (dflt units = Angstrom): ",				/* 7 */
	"Front velocity (cm/sec): ",										/* 8 */
	"Surface concentration (atomic fraction): ",					/* 9 */
	"Concentration in substrate (atomic fraction): ",			/* 10 */
	"Thickness of film (cm or A): ",									/* 11 */
	"Front edge concentration (atomic fraction): ",				/* 12 */
	"Back edge concentration (atomic fraction): ",				/* 13 */
	"Integral of Gaussian (dflt units = 1E15 atoms/cm^2): ",	/* 14 */
	"Depth of layer (Angstroms): ",									/* 15 */
	"FWHM of Gaussian (Angstroms): ",								/* 16 */
	"Integrated dose (dflt units = 1E15 atoms/cm^2): ",		/* 17 */
	"Ion Range (Angstroms): ",											/* 18 */
	"Standard deviation/straggle (Angstroms): ",					/* 19 */
	"Skewness (dimensionless): ",										/* 20 */
	"Kurtosis (Gaussian=0): ",											/* 21 */
	"Knot value at 1/4 through layer: ",							/* 22	*/
	"Knot value at 2/4 through layer: ",							/* 23	*/
	"Knot value at 3/4 through layer: ",							/* 24	*/
	"Parameter value: "													/* 25 */
};

typedef enum _OPS1 {
	SIM_HELP,		/*  Long help */

	SIM_USE,			/*  Choose the sample description to use	*/
	SIM_NEW,			/*  Open a new sample description			*/
	SIM_FORGET,		/*  Forget the current sample description	*/
	SIM_DESCRIBE,	/*  Set the description of the simulation */

	SIM_SUBLAYER,	/*  Modify # sublayers */
	SIM_THICKNES,	/*  Modify thickness of layer */
	SIM_STHICKNE,	/*  Modify sublayer thickness */
	SIM_COMPOSIT,	/*  Modify baseline comp. */
	SIM_SPECIES,	/*  Modify diffusant comp. */

	SIM_BACKGROUND,		/* Specify a buffer which is "added" to spectrum as background */
	
	SIM_GLOBAL_SPECIES,	/* Global diffusing species						*/
	SIM_GLOBAL_CURVE,		/* Spline curve for global diffusant species */
	SIM_GLOBAL_START,		/* Starting layer for global diffusant			*/
	SIM_GLOBAL_MODE,		/* Starting layer for global diffusant			*/
	SIM_GLOBAL_NEXT,		/* Go to next global diffusant					*/
	SIM_GLOBAL_DELETE,	/* Delete this global diffusing layer			*/
	SIM_GLOBAL_LAYER,		/* Go to a specific global diffusant spec		*/

	SIM_NORMALIZE,	/*  Normalize concentrations to some number */
	SIM_NEXT,		/*  Layer */
	SIM_SHOW,		/*  Sample structure */
	SIM_PROFILE,	/*  Profile the composition */
	SIM_WRITEPROFILE,/*  Write an excel compatible file with elemental profiles */
	SIM_SAVE,		/*  Sample desc. to file */
	SIM_GET,			/*  Sample desc. from file */
	SIM_LAYER,		/*  Point to new layer */
	SIM_DENSITY,	/*  Display density table */
	SIM_SETDENS,	/*  Modify density table */
	SIM_STATUS,		/*  Display */
	SIM_MATCH,		/*  Match simulation and current buffer setting CORR */
	SIM_SCALE,		/*  Mess with stopping powers */
	SIM_LOAD,		/*  Atomic data read */
	SIM_RESET,		/*  Cancel all simulations */
	SIM_RETURN,		/*  Normal way back to RUMP */
	SIM_ABORT,		/*  Back to RUMP */
	SIM_STRAGGLE,	/*  Activate straggling option */
	SIM_MULTIPLE,	/*  Activate multiple scattering option */
	SIM_NOISE,		/*  Add statistical noise */
	SIM_OPEN,		/*  Insert new layer */
	SIM_FUZZ,		/*  Nonuniform samples */
	SIM_FUZZY,		/*  Nonuniform samples (new version handling) */
	SIM_UNFUZZY,	/*  Turn off fuzz on this layer */
	SIM_SIM,			/*  Redundant */
	SIM_MAXPTH,		/*  Max path auto-sublayers */
	SIM_SPLOT,		/*  Selective plot */
	SIM_ABSORBER,	/*  Turn on absorber foil */
	SIM_FRESABSORBER,  /*  Use absorber on FRES only */
	SIM_QUEST,	 	/*  Short command list */
	SIM_XSECT,		/*  Cross section for P and D */
	SIM_E2COF,		/*  Second order cross section */
	SIM_EQUATION,	/*  Set equation type + parms */
	SIM_OVERLAY,	/*  Recreation of RUMP command */
	SIM_COMPARE,	/*  Recreation of RUMP command */
	SIM_RECALCUL,
	SIM_DELETE,		/*  Delete the current layer		*/
	SIM_PILEUP,		/*  Set the pileup method			*/
	DEFAULT_MAXPATH,	/* Default maxpath for simulations */
	RES_READ,		/*  Read a resonance data file	*/
	SIM_INFO,		/*  Calculate info on this element */

	FOIL_SIM,		/*  Simualte energy loss/straggling in foil */
	SIM_TOF,			/*  Time-of-flight simulation (in development) */
	SIM_NULL
} OPS1;

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	enum _OPS1 rcode;
} CMTYPE;

static CMTYPE cmlist1[] = {						/* Global control */
	{"?",					1,		SIM_QUEST},
	{"help",				2,		SIM_HELP},
	{"sim",				2,		SIM_SIM},
	{"get",				2,		SIM_GET},
	{"save",				2,		SIM_SAVE},
	{"recalculculate",5,		SIM_RECALCUL},
	{"return",			3,		SIM_RETURN},
	{"abort",			5,		SIM_ABORT},
	{"overlay",			2,		SIM_OVERLAY},
	{"compare",			5,		SIM_COMPARE},
	{"pileup",			4,		SIM_PILEUP},
	{"splot",			2,		SIM_SPLOT},
	{NULL,				0,		SIM_NULL}
};

static CMTYPE cmlist2[] = {						/* Layer editing */
	{"show",				2,		SIM_SHOW},
	{"writeprofile",	7,		SIM_WRITEPROFILE},
	{"write_profile",-8,		SIM_WRITEPROFILE},
	{"wr_profile",	  -5,		SIM_WRITEPROFILE},
	{"profile",		   4,		SIM_PROFILE},
	{"description",	4,		SIM_DESCRIBE},		{"describe", -8, SIM_DESCRIBE},
	{"layer",			2,		SIM_LAYER},
	{"next",				2,		SIM_NEXT},
	{"open",				2,		SIM_OPEN},
	{"delete",			3,		SIM_DELETE},
	{"close",		  -5,		SIM_DELETE},
	{"reset",			5,		SIM_RESET},
	{"absorber",		3,		SIM_ABSORBER},
	{"fres_absorber", 5,    SIM_FRESABSORBER},

	{"fuzzy",			5,		SIM_FUZZY},			/* New version */
	{"unfuzzy",			6,		SIM_UNFUZZY},
	{"fuzzing",		  -5,		SIM_FUZZY},
	{"fuzz",			  -4,		SIM_FUZZ},			/* Old format	*/
	{"thickness",		2,		SIM_THICKNES},
	{"composition",	1,		SIM_COMPOSIT},
	{"equation",		2,		SIM_EQUATION},
	{"eqn",			  -3,		SIM_EQUATION},
	{"species",			1,		SIM_SPECIES},
	{"sublayers",		2,		SIM_SUBLAYER},
	{"sthickness",		3,		SIM_STHICKNE},
	{"normalize",		4,		SIM_NORMALIZE},

	{"background",		5,		SIM_BACKGROUND},
		
	{"g_composition",			 6,	SIM_GLOBAL_SPECIES},
	{"g_species",				 6,	SIM_GLOBAL_SPECIES},
	{"g_curve",					 5,	SIM_GLOBAL_CURVE},
	{"g_profile",			   -6,	SIM_GLOBAL_CURVE},
	{"g_start",					 7,	SIM_GLOBAL_START},
	{"g_mode",					 6,	SIM_GLOBAL_MODE},
	{"g_next",					 6,	SIM_GLOBAL_NEXT},
	{"g_layer",					 5,	SIM_GLOBAL_LAYER},
	{"g_delete",				 5,	SIM_GLOBAL_DELETE},
	{"global_composition", -11,	SIM_GLOBAL_SPECIES},
	{"global_species",	  -11,	SIM_GLOBAL_SPECIES},
	{"global_curve",		  -10,	SIM_GLOBAL_CURVE},
	{"global_profile",	  -10,	SIM_GLOBAL_CURVE},
	{"global_start",		  -12,	SIM_GLOBAL_START},
	{"global_mode",		  -11,	SIM_GLOBAL_MODE},
	{"global_next",		  -11,	SIM_GLOBAL_NEXT},
	{"global_layer",		  -10,	SIM_GLOBAL_LAYER},
	{"global_delete",		  -10,	SIM_GLOBAL_DELETE},

	{NULL,						 0,	SIM_NULL}
};

static CMTYPE cmlist3[] = {					/* Eventually going to configure */
	{"status",			2,		SIM_STATUS},
	{"match",			4,		SIM_MATCH},
	{"straggle",		3,		SIM_STRAGGLE},
	{"multiple_scatter",		8, SIM_MULTIPLE},
	{"noise",			5,		SIM_NOISE},
	{"maxpth",			3,		SIM_MAXPTH},
	{"maxpath",		  -4,		SIM_MAXPTH},
	{"density",			2,		SIM_DENSITY},
	{"setdensity",		2,		SIM_SETDENS},
	{"scale",			2,		SIM_SCALE},
	{"load",				2,		SIM_LOAD},
	{"xsect",			4,		SIM_XSECT},
	{"e2cof",			3,		SIM_E2COF},
	{"resread",			4,		RES_READ},
	{"info_element",	4,		SIM_INFO},
	{"foil",				4,		FOIL_SIM},
	{"tof",				3,		SIM_TOF},
	{"dflt_maxpath",	8,		DEFAULT_MAXPATH},
	{NULL,				0,		SIM_NULL}
};

static REAL default_maxpath = DFLT_SUBLAYER_THICK;		/* 200 /cm2 default max path */

static char XSectHelp[] = 
"\n"
" Usage: XSECT <value for H> <value for D>\n"
"\n"								  
" This command modifies the cross section for Hydrogen and Deuterium in forward\n"
" scattering.  If negative, then the internal routines are used to calculate the\n"
" cross section and the values are scaled by the magnitude of XSECT.  On startup,\n"
" cross sections are set to -1 (which means internal only).  If XSECT is set as\n"
" positive, then it is the constant part of the cross section in strange internal\n"
" unit (multiply barns/sr by 6.2415 to get internal units).  The constant part may\n"
" also be linked with a 1/E^2 term using E2COF.  (Use of internal units is left\n"
" only for compatibility.  REAL PAIN IN THE CODE ALSO!\n"
"\n"								  
"   XSECT  -1.0  2.68   /* H unchanged, D sigma set to 0.429 b/sr (constant)\n"
"   XSECT  -1.13 2.68   /* Scales H by factor of 1.13, sets D to constant 0.429\n"
"\n"
" The default is -1 -1 and thus uses internal routines (close to correct).\n"
"\n"
" Serious users who need to modify the cross sections should contact the\n"
" authors of this buggy program and learn to write extension modules\n";
static char E2CofHelp[] = 
"\n"
" Usage: E2COF <value for H> <value for D>\n"
"\n"								  
" This command is closely coupled to XSECT to set cross section for Hydrogen\n"
" and Deuterium in forward scattering.  If XSECT is positive, then this term\n"
" adds a 1/E^2 term to the cross section calculation (with E in keV).  In all\n"
" other cases, the value of E2COF is ignored.  The cross section is given as:\n"
"          sigma (b/sr)  =   SIGMA + E2COF / E^2\n"
" as appropriate for H or D.  While the SIGMA value must be positive, the\n"
" E2COF can be positive or negative.  On initialization, E2COF is zero.\n"
"\n"
"    XSECT  2.00 2.68   /* Sets the constant part of the cross sections\n"
"    E2COF  1283 8000   /* Adds a 1/E^2 term to have some energy dependence\n";

/* ============================================================================
-- Usage Guide:
--
-- Usage:   BOOL FUNCTION SIM()
--
-- Quick:   Processor for user's SIM level commands
--
-- Inputs:  via LEXP
--
-- Output:  Mostly common block /sample.h/
--
-- Return:  TRUE  -> everything okay
--          FALSE -> something failed (file does not exist?)
--
-- Global Vars:  sample.h, RUMP, ATOMS
--
-- Called By:    ANLYTC
--
-- Calls:        SIMGET to retrieve a structure from a file
--               SAMOUT to send a copy of structure to
--                      a file or display it on the terminal
--               SIMRES to put everything in an initial state
--               LCHECK to verify a layer is OK
--               LOCNAM to find the space for an element in a table
--               RESTAT to print status of the resonance
--
-- Description:
--     Function SIM performs the RUMP command of the same name.  The routine
--     is the main program for all of the basic simulation package, and 
--     directly processes the commands for editing the sample's structure.
--     It has a number of auxiliary commands for messing with stopping
--     powers and compound densities.  It also has the capability to use
--     diffusion equations to simulate the sample's structure.  Presently 
--     the available diffusion equations are (1) constant concentration, 
--     (2) complimentary error function, (3) exponential function, (4) semi-
--     infinite film equation, and (6) time dependent equation (Case II).
--     The actual simulation is done via CHKSIM when people try to look at
--     look at the ALT buffer, hence we keep the flag AUTSIM around to
--     help decide if the simulation is out of date.
--
--     The package really is no more than be a very complicated, specialized
--     editor.  It allows editing of the sample structure: layer by layer
--     description of thickness, substrate composition, diffusing species
--     composition, the number or thickness of sublayers, and a diffusion
--     equation.  Density tables, stopping power scaling, and fuzzing
--     parameters are also available.
--
-- Notes:
============================================================================ */

/* ===========================================================================
-- Main, ugly routine for SIM.
=========================================================================== */
int RbsSimMain(int key) {

/*  -- Local variables -- */
	int i, j, k, l, pnt, ilay, need_show, level;
	REAL thickness, density, x;
	char token[LONG_STR_SIZE];				/* For equation */
	char filename[PATH_MAX];
	BOOL detail, silent;
	FILE *funit;

	ATOMS		 *atomp;
	CMTYPE	 *citem;
	SPECTRUM	 *sbuf;
	SIMEQN	 *eq;
	LAYER		 *layer, *tmp;
	GLOBAL_LAYER *g_layer, *gtmp;
	UNITS		 *nit;

/*  Equation handling */
	static int auto_show=FALSE;

/*  -- Code begin -- */
	need_show = 1;							/*  First time, need! */
	layer   = sample->layer;
	g_layer = sample->g_layer;

/* ... Come here for processing of next command */
	while (TRUE) {
		if (! LexGetToken(token, sizeof(token))) {	/* Anything waiting?		*/
			if (auto_show && need_show != 0)   {
				ScrGetPos(&i,&k,&l);						/* Current position?			*/
				j = 0;
				SimShowSample(layer, g_layer, j, FALSE);
				j = min(20,j);
				ScrScroll(0,j+1,0);						/* Set scrolling region		*/
				i = max(i,j+1);
				ScrSetPosn(i,k,l);						/* Back to where we were	*/
			}
			need_show = 0;
			if (! LexGetTokenP(token, sizeof(token), "SIM Command: ")) continue;
		}

/* Look up the command - if not there, some sort of return is in order */
		if ( ((citem = LexCmdl(token, cmlist1, sizeof(CMTYPE))) == NULL) &&
			  ((citem = LexCmdl(token, cmlist2, sizeof(CMTYPE))) == NULL) &&
			  ((citem = LexCmdl(token, cmlist3, sizeof(CMTYPE))) == NULL)) {
			if (LexSystem(0,token)) continue;

			if (RbsAutoReturn) {						/* Do implicit "return" command */
				LexBackup();
				layer = SimCheckLayer(layer);
				g_layer = SimCheckGlobal(g_layer);
				if (Rmp->autsim < 0) SimSetIdent();	/*  Keep the name right always */
				if (auto_show) ScrScroll(1,0,0);		/*  Reset full scrolling region */
				sample->layer   = layer;
				sample->g_layer = g_layer;
				return(TRUE);
			}
			ERRprintf("ERROR: %s unrecognized SIM sub-command\n", token);
			LexFlush();
			continue;
		}

		switch (citem->rcode) {				/* Process commands */

/* ... SUBLAYER - set number of sublayers within a layer */
		 	case SIM_SUBLAYER:
				i = LexGetInt(DFLT_SUBLAYER_NUM, "Number of sublayers (default)? ");
				layer->num_sublayers = abs(i);	/* Set # of sub-layers	*/
				need_show =  1;						/* Reshow needed			*/
				Rmp->autsim = -1;						/* Simulation obsolete	*/
				break;

/* ... THICKNESS - set thickness of layer */
			case SIM_THICKNES:
				need_show =  1;						/* Reshow needed			*/
				thickness = LexGetReal(layer->thick.magn, "Thickness of layer (unchanged)? ");
				if (! LexGetTokenP(token, sizeof(token), "Unit of thickness (angstroms)? ")) {
					nit = &angstroms;
				} else if ( (nit = SimLookupUnit(token)) == NULL) {
					ERRprintf("ERROR: Unit not found in table (%s)\n", token);
					LexFlush();
					break;
				}
				layer->thick.magn = (REAL) fabs(thickness);	/* Put thickness in		*/
				layer->thick.units = nit;							/* Units now known		*/
				Rmp->autsim = -1;										/* Simulation obsolete	*/
				break;

/* ... NORMALIZE - normalize constants in this layer to specified sum */
			case SIM_NORMALIZE:
				{
					int do_spc;
					do_spc = LexGetOption(token, sizeof(token));
					thickness = LexGetReal(100.0, "Sum of compositions (100): ");
					if (thickness <= 0) break;
					need_show =  1;						/* Reshow needed			*/

					if (do_spc) {
						for (x=0,i=0; i<MAXEL; i++) x += layer->species[i];
					} else {
						for (x=0,i=0; i<MAXEL; i++) x += layer->matrix[i];
					}
					if (x == 0) {
						ERRprintf("ERROR: Nothing in layer, can't normalize\n");
						LexFlush(); break;
					}
					if (do_spc) {
						for (i=0; i<MAXEL; i++) layer->species[i] *= thickness/x;
					} else {
						for (i=0; i<MAXEL; i++) layer->matrix[i] *= thickness/x;
					}
				}
				break;
				
/* ... BACKGROUND - specify a spectrum which will be added to the data */
			case SIM_BACKGROUND:
				sample->background = RbsGetBuf("Background buffer (none)? ", (SPECTRUM *) 1);
				if (sample->background == NULL) {
					ERRprintf("ERROR: Invalid buffer specified\n");
					LexFlush();
				} else if (sample->background == (SPECTRUM *) 1) {
					sample->background = NULL;
				}
				Rmp->autsim = -1;											/* Simulation obsolete	*/
				break;
			
/* ... SUBLAYER THICKNESS - the thickness for the sublayers of a layer */
			case SIM_STHICKNE:
				need_show =  1;						/* Reshow needed			*/
				thickness = LexGetReal(DFLT_SUBLAYER_THICK, "Thickness of sublayer (default)? ");
				if (! LexGetTokenP(token, sizeof(token), "Unit of thickness (angstroms)? ")) {
					nit = &angstroms;
				} else if ( (nit = SimLookupUnit(token)) == NULL) {
					ERRprintf("ERROR: Unit not found in table (%s)\n", token);
					LexFlush();	break;
				}
				layer->thisub.magn  = (REAL) fabs(thickness);	/* Put thickness in		*/
				layer->thisub.units = nit;								/* Units now known		*/
				layer->num_sublayers = 0;								/* If sthick, subl off	*/
				Rmp->autsim = -1;											/* Simulation obsolete	*/
				break;

/* ... COMPOSITION - composition of the substrate */
			case SIM_COMPOSIT:
				need_show =  1;						/* Reshow needed			*/
				while (LexGetTokenP(token, sizeof(token), "Element name (/ to end)? ")) {
					if (stricmp(token, "/") == 0) break;
					if ( (pnt = SimLocateName(token,TRUE)) == ELEM_INVALID) {
						gen_err("Element name error");
						LexFlush();
						continue;
					}
					layer->matrix[pnt] = LexGetReal(1.0, "Relative amount of element (1)? ");
					Rmp->autsim = -1;					/* Simulation obsolete	*/
				}
				break;

/* ... SPECIES - composition of the diffusing species */
			case SIM_SPECIES:
				need_show =  1;						/* Reshow needed			*/
				while (LexGetTokenP(token, sizeof(token), "Element name (/ to end)? ")) {
					if ( stricmp(token, "/") == 0) break;
					if ( (pnt = SimLocateName(token,TRUE)) == ELEM_INVALID) {
						gen_err("Element name error");
						LexFlush();
						continue;
					}
					layer->species[pnt] = LexGetReal(1.,"Relative amount of element (1)? ");
					Rmp->autsim = -1;					/* Simulation obsolete	*/
				}
				break;

/* ... GLOBAL_SPECIES - composition of the global diffusing species */
			case SIM_GLOBAL_SPECIES:
				need_show =  1;						/* Reshow needed			*/
				while (LexGetTokenP(token, sizeof(token), "Element name (/ to end)? ")) {
					if ( stricmp(token, "/") == 0) break;
					if ( (pnt = SimLocateName(token,TRUE)) == ELEM_INVALID) {
						gen_err("Element name error");
						LexFlush();
						continue;
					}
					g_layer->species[pnt] = LexGetReal(1.,"Relative amount of element (1)? ");
					Rmp->autsim = -1;					/* Simulation obsolete	*/
				}
				break;

/* ... GLOBAL_CURVE - curve giving global diffusant concentration */
			case SIM_GLOBAL_CURVE:
				need_show =  1;						/* Reshow needed			*/
				*g_layer->curve = '\0';
				if (LexGetStrExprP(token, sizeof(token), 
						"Curve giving atomic fraction versus depth (in 10^15 at/cm^2): (none) ")) {
					if ( stricmp(token, "/") != 0) strscpy(g_layer->curve, token, sizeof(g_layer->curve));
				}
				Rmp->autsim = -1;					/* Simulation obsolete	*/
				break;

/* ... GLOBAL_START - starting layer number for global diffusant */
			case SIM_GLOBAL_START:
				need_show = 1;
				g_layer->start = LexGetInt(0, "Layer where global profile starts (surface): ");
				if (g_layer->start < 0) g_layer->start = 0;
				Rmp->autsim = -1;
				break;

/* ... GLOBAL_MODE - control options for global diffusant */
			case SIM_GLOBAL_MODE:
				need_show = 1;
				g_layer->mode = LexGetInt(0, "Global diffusant option flags (0 -> none): ");
				Rmp->autsim = -1;
				break;

/* ... GLOBAL_NEXT */
			case SIM_GLOBAL_NEXT:
				need_show =  1;							/* Reshow needed				*/
				if (SimGlobalEmpty(g_layer)) {
					TTYprintf("Global layer %d, which was empty, has been deleted\n", SimGetGlobalNum(g_layer));
					if (g_layer->locked != 0) {
						ERRprintf("OOPS! Nope - layer is locked by PERT, can't be deleted\n");
					} else if (g_layer->next == NULL && g_layer->previous == NULL) {
						SimDeleteLayer(layer);
						sample->g_first = g_layer = SimAllocGlobal();
					} else {
						gtmp = g_layer->next;
						g_layer = SimDeleteGlobal(g_layer);	/* Okay, delete this one	*/
						if (gtmp == NULL) {						/* There was not a next		*/
							gtmp = SimAllocGlobal();
							gtmp->previous = g_layer;
							g_layer->next   = gtmp;
							g_layer = gtmp;						/* And shift to this new one */
						}
					}
				} else {
					if (g_layer->next == NULL) {			/* Is there already a next? */
						gtmp = SimAllocGlobal();			/* No, allocate one then    */
						gtmp->previous = g_layer;			/* And link it in here		 */
						g_layer->next   = gtmp;
					}
					g_layer = g_layer->next;				/* And shift to next one    */
				}
				if (LexQueryActiveInputID(NULL) == 0) 
					TTYprintf("You are now working on global layer #%2d\n", SimGetGlobalNum(g_layer));
				break;

/* ... GLOBAL_LAYER choose a layer to modify */
			case SIM_GLOBAL_LAYER:
				need_show =  1;							/* Reshow needed			*/
				if ( (ilay = LexGetInt(0, "Global diffusant def'n to be modified? ")) <= 0) {
					gen_err("Invalid layer number");
					LexFlush();
					break;
				}
				if (ilay == SimGetGlobalNum(g_layer)) break;
				SimCheckGlobal(g_layer);
				g_layer = sample->g_first;
				while (--ilay && g_layer->next != NULL) g_layer = g_layer->next;

				if (ilay == 1) {							/* Really want next */
					gtmp = SimAllocGlobal();
					g_layer->next  = gtmp;
					gtmp->previous = g_layer;
					g_layer = gtmp;
				} else if (ilay != 0) {
					ERRprintf("WARNING: Specified global diffusant beyond reasonable limits - now on last global layer\n");
					LexFlush();
				}
				break;

/* ... GLOBAL_DELETE - Delete current layer */
			case SIM_GLOBAL_DELETE:
				need_show =  1;							/* Reshow needed			*/
				if (g_layer->locked != 0) {
					ERRprintf("ERROR: Layer cannot be deleted because it is locked by PERT\n");
					LexFlush();
				} else {
					g_layer = SimDeleteGlobal(g_layer);
					if (g_layer == NULL)
						sample->g_first = g_layer = SimAllocGlobal();
					Rmp->autsim = -1;						/* Simulation obsolete	*/
				}
				break;

/* ... NEXT */
			case SIM_NEXT:
				need_show =  1;							/* Reshow needed				*/
				if (SimLayerEmpty(layer)) {
					TTYprintf("Current layer %d, which was empty, has been deleted\n", SimGetLayerNum(layer));
					if (layer->locked != 0) {
						ERRprintf("OOPS! Nope - layer is locked by PERT, can't be deleted\n");
					} else if (layer->next == NULL && layer->previous == NULL) {
						SimDeleteLayer(layer);
						sample->first = layer = SimAllocLayer();
					} else {
						tmp = layer->next;
						layer = SimDeleteLayer(layer);	/* Okay, delete this one	*/
						if (tmp == NULL) {					/* There was not a next		*/
							tmp = SimAllocLayer();
							tmp->previous = layer;
							layer->next   = tmp;
							layer = tmp;						/* And shift to this new one */
						}
					}
				} else {
					if (layer->next == NULL) {			/* Is there already a next? */
						tmp = SimAllocLayer();			/* No, allocate one then    */
						tmp->previous = layer;			/* And link it in here		 */
						layer->next   = tmp;
					}
					layer = layer->next;					/* And shift to next one    */
				}
				if (LexQueryActiveInputID(NULL) == 0) 
					TTYprintf("You are now working on layer #%2d\n", SimGetLayerNum(layer));
				break;

/* ... OPEN Open up a new layer in the middle */
			case SIM_OPEN:
				need_show =  1;						/* Reshow needed			*/
				if (SimLayerEmpty(layer)) {
					ERRprintf("ERROR: You don't have anything in this layer, why open another?\n");
				} else {
					tmp = SimAllocLayer();
					tmp->previous   = layer->previous;
					tmp->next       = layer;
					layer->previous = tmp;
					if (tmp->previous == NULL) {
						sample->first = tmp;
					} else {
						tmp->previous->next = tmp;
					}
					layer = tmp;
					TTYprintf("You are now working on a fresh layer #%2d\n", SimGetLayerNum(layer));
				}
				break;

/* ... DELETE - Delete current layer */
			case SIM_DELETE:
				need_show =  1;							/* Reshow needed			*/
				if (layer->locked != 0) {
					ERRprintf("ERROR: Layer cannot be deleted because it is locked by PERT\n");
					LexFlush();
				} else {
					layer = SimDeleteLayer(layer);
					if (layer == NULL)
						sample->first = layer = SimAllocLayer();
					Rmp->autsim = -1;						/* Simulation obsolete	*/
				}
				break;

/* ... SHOW  display the present data file */
			case SIM_SHOW:
				detail = FALSE;
				while (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-AUTO", 2)) {
						if (auto_show) ScrScroll(1,0,0);	/*  Reset full scrolling region */
						auto_show = ! auto_show;
						need_show = 1;
					} else if (LexEqual(token, "-DETAIL", 2)) {
						detail = TRUE;
					}
				}
				SimShowSample(layer, g_layer, -1, detail);
				break;

/* ... WRITEPROFILE display the present data file */
			case SIM_PROFILE:
				SimWriteProfile(NULL, NULL);
				break;
			case SIM_WRITEPROFILE:
				layer = SimCheckLayer(layer);
				g_layer = SimCheckGlobal(g_layer);
				if (LexGetFileP(filename, sizeof(filename), "SIM concentration file output (.cnc assumed) (screen): ")) {
					SysAddExt(filename, ".cnc");
					if ( (funit = fopen(filename, "w")) != NULL) {
						SimWriteProfile(NULL, funit);
						fclose(funit);
					} else {
						ERRprintf("ERROR: %s failed to open for writing\n", filename);
						LexFlush();
					}
				} else {
					SimWriteProfile(NULL, NULL);
				}
				break;

/* ... SIMSAVE writes the present data into a given [filename] */
			case SIM_SAVE:
				layer = SimCheckLayer(layer);
				g_layer = SimCheckGlobal(g_layer);
				if (! SimWriteSample()) LexFlush();
				break;

/* ... GET */
			case SIM_GET:
				Rmp->autsim = -1;	/*  Flag sim as bad always! */
				if (LexGetFileP(filename, sizeof(filename), "Simulation file to load: ")) {
					if (! SysFindFile(filename, filename, RbsSearchPath, SIM_FILE_EXTS, R_OK)) {
						ERRprintf("ERROR: SIM structure file %s not found\n", filename);
						LexFlush();
						break;
					}
					LexExecFile(filename);				/* Start file		*/
					LexSetLocalNoEcho(TRUE);			/* Disable noise	*/
				}
				break;

/* ... LAYER choose a layer to modify */
			case SIM_LAYER:
				need_show =  1;							/* Reshow needed			*/
				if ( (ilay = LexGetInt(0, "Layer to be modified? ")) <= 0) {
					gen_err("Invalid layer number");
					LexFlush();
					break;
				}
				if (ilay == SimGetLayerNum(layer)) break;
				SimCheckLayer(layer);
				layer = sample->first;
				while (--ilay && layer->next != NULL) layer = layer->next;

				if (ilay == 1) {							/* Really want next */
					tmp = SimAllocLayer();
					layer->next   = tmp;
					tmp->previous = layer;
					layer = tmp;
				} else if (ilay != 0) {
					ERRprintf("WARNING: Specified layer beyond reasonable limits - now on last layer\n");
					LexFlush();
				}
				break;

#define	DENSITY_FORMAT	" Density of compound: %4s is %6.4f E23 atoms/cm3\n"

/* ... DENSITY displays densities stored in table */
			case SIM_DENSITY:
				if (! LexGetTokenP(token, sizeof(token), "Compound name (ALL)? "))
				   strcpy(token, "ALL");

				if (stricmp(token, "all") == 0) {
					for (i=0; i<SimDensityTableCount; i++) {
						if (SimDensityTable[i].type != ABSOLUTE) continue;
						TTYprintf(DENSITY_FORMAT, SimDensityTable[i].ident, SimDensityTable[i].density);
					}
				} else if ( (nit = SimLookupUnit(token)) != NULL) {
					TTYprintf(DENSITY_FORMAT, nit->ident, nit->density);
				} else {
					ERRprintf("ERROR: Unit not found in table (%s)\n", token);
				}
				break;

/* ... SETDENSITY makes temporary changes to density table */
			 case SIM_SETDENS:
				if (! LexGetTokenP(token, sizeof(token), "Name of new unit? (<CR> aborts) "))
					break;

				if ( (nit = SimLookupUnit(token)) != NULL) {
					if (nit->type != ABSOLUTE) {
						ERRprintf("DRUG WARNING: Don't even think it - you're not gonna get away with that!\n");
						break;
					}
					ERRprintf("WARNING: Unit already in table - new value will temporarily override\n");
				} else if (SimDensityTableCount >= SimDensityTableSize) {
					ERRprintf("ERROR: Density table is full: no new compounds allowed\n");
					LexFlush();
					break;
				}
				density = LexGetReal(0.0, "Density of compound (units of 1E23 atoms/cm3) (ABORT): ");
				if (density <= 0.0) break;

				if (nit == NULL) {										/* Is it new? */
					nit = SimDensityTable+SimDensityTableCount;
					SimDensityTableCount++;
				}
				strscpy(nit->ident, token, sizeof(nit->ident));
				nit->type    = ABSOLUTE;								/* Absolute type	*/
				nit->density = density;									/* Density			*/
				TTYprintf(" New compound: %s = %6.4f E23 atoms/cm3\n"
							 " To insert permanently, add to configuration file: %s\n",
							 nit->ident, nit->density, SimDensityFile);
				break;

/* ... MATCH Calculate integral between two limits and cause match via CORR */
/*  .. Channel by channel: Round to nearest data point */
			case SIM_MATCH:
				RbsSetCorrByMatchCounts();
				break;

/* ... Calculate in/outgoing stopping powers for specified element */
			case SIM_INFO:
				if (! LexGetTokenP(token, sizeof(token), "Element: (abort) ")) break;
				if (! RbsIdent(token, &atomp, NULL)) {
					ERRprintf("ERROR: Element %s does not exist\n", token);
					break;
				}
				SimCalcInfo(atomp);				/* Print out useful info */
				break;


/* ... STATUS Print the various experimental parameters */
			case SIM_STATUS:
				level = 0;								/* Partial info level */
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-all", 2) || LexEqual(token, "-full", 2)) {
						level = 1;
					} else if (LexEqual(token, "-level", 2)) {
						level = LexGetInt(0, "Reset level (0-n): ");
					} else {
						ERRprintf("ERROR: %s Unrecognized option to STATUS\n", token);
						break;
					}
				}

				if (sample->straggle != 0.0) TTYprintf("Straggling computed as %.2f times the Bohr theory\n", sample->straggle);
				if (sample->multiple != 0.0) TTYprintf("Multiple computed as %.2f times adhoc theory\n", sample->multiple);
				if (sample->noise)			  TTYprintf("Statistical noise added to spectrum\n");

				RbsScalePrint();

				TTYprintf(
					"Auto-division of layers gives%7.1f /CM2 maximum sublayer pathlength\n",
					sample->maxpth);

				if (sample->absorber_layers != 0) {
					TTYprintf(" First%3d layers comprise absorber foil\n",sample->absorber_layers);
					if (sample->fres_only_absorber)
						TTYprintf("    Absorber layers will be used when simulating FRES spectra\n");
				}

				ResStatus(level);
				break;

/* ... SCALE Change scale factors for stopping powers. */
			case SIM_SCALE:
				if (! LexGetTokenP(token, sizeof(token),
					"Scale stopping power of which element? (abort) ")) break;

				if ((atomp = RbsIdentRaw(token)) == NULL) {
					gen_err2("SCALE aborts for nonexistent element",token);
					LexFlush();
					break;
				}

				TTYprintf(" Stopping power of %2s is currently scaled by %5.3f\n",
							 token, atomp->scale);
				x = LexGetReal(atomp->scale,"New scale factor? (unchanged) ");
				if (x != atomp->scale) Rmp->autsim = -abs(Rmp->autsim);	/*  Simulation obsolete */

				RbsSetSScale (atomp,x);		/* Now change coefficients by factor */
				break;

/* ... LOAD * * * * Load a new set of atomic data from disk */
			case SIM_LOAD:
				if (! LexGetFileP(token,sizeof(token),
					"File with new atomic data? (abort) ")) break;

				RbsLoad1(token);
				Rmp->autsim = -abs(Rmp->autsim);	/*  Any simulations are now obsolete */
				break;

/* ... Set the description */
			case SIM_DESCRIBE:
				if (! LexGetToken(sample->desc, sizeof(sample->desc)))
					LexPromptStr(sample->desc, sizeof(sample->desc), "Simulation description: ");
				break;

/* ... RESET */
			case SIM_RESET:
				level = U_RESET;					/* Partial reset level */
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-all", 2) || LexEqual(token, "-full", 2)) {
						level = U_INIT;
					} else if (LexEqual(token, "-level", 2)) {
						level = LexGetInt(0, "Reset level (0-n): ");
					} else {
						ERRprintf("ERROR: %s Unrecognized option to RESET\n", token);
						break;
					}
				}
				if (PertFreeAll() != 0)
					TTYprintf("WARNING: Implicit PERT FREEALL performed to release SIM locks\n");
				SimReset(level);				/* Reset sim sample structure -> null */
				ResReset(level);				/* Reset level 1 is ignored	*/
				layer = sample->layer;		/* Update local layer pointer	*/
				g_layer = sample->g_layer;	/* Update local layer pointer	*/
				break;

/* ... RETURN  returns to RUMP */
			case SIM_RETURN:
				layer = SimCheckLayer(layer);
				g_layer = SimCheckGlobal(g_layer);
				if (Rmp->autsim < 0) SimSetIdent();	/*  Keep the name right always */
				if (auto_show)  ScrScroll(1,0,0);	/*  Reset full scrolling region */
				sample->layer = layer;
				return(TRUE);

/* ... ABORT */
			case SIM_ABORT:
				gen_warn("Living dangerously, aren\'t you?");
				sample->layer = layer;
				return (FALSE);

/* ... STRAGGLE */
			case SIM_STRAGGLE:
				sample->straggle = LexGetReal(0.0, "Straggling constant? (0) ");
				Rmp->autsim = -abs(Rmp->autsim);
				break;

/* ... STRAGGLE */
			case SIM_MULTIPLE:
				sample->multiple = LexGetReal(0.0, "Multiple scattering constant? (none) ");
				Rmp->autsim = -abs(Rmp->autsim);
				break;

/* ... NOISE */
			case SIM_NOISE:
				if (! LexGetTokenP(token, sizeof(token), "Random noise addition (no)? ")) {
					sample->noise = FALSE;
				} else if (LexEqual(token, "yes", 1) || LexEqual(token, "on", 2)) {
					sample->noise = TRUE;
				} else if (LexEqual(token, "no", 1) || LexEqual(token, "off", 2)) {
					sample->noise = FALSE;
				} else {
					ERRprintf("ERROR: It's an easy question to answer - try again\n");
					LexFlush();
					break;
				}
				Rmp->autsim = -abs(Rmp->autsim);
				break;
				
/* ... UNFUZZY Un-fuzz the current layer */
			case SIM_UNFUZZY:
				need_show = 1;								/* Reshow needed */
				layer->fuzzs = 0;
				layer->fuzzd = 0.0f;
				Rmp->autsim = -1;
				break;

/* ... FUZZY Fuzz the current layer */
			case SIM_FUZZY:
				need_show = 1;								/* Reshow needed */
				layer->fuzzs = 0;
				layer->fuzzd = LexGetReal(0.0f, "Amount to fuzz this layer (thickness units) [none]: ");
				layer->fuzzs = LexGetInt(3, "Number of interations (3)? ");
				if (layer->fuzzd <= 0.0f || layer->fuzzs <= 0) {
					layer->fuzzs = 0;						/* Make clean */
					layer->fuzzd = 0.0f;
				} else if (layer->fuzzs == 1) {
					ERRprintf("WARNING: Fuzzing with 1 iteration does essentially nothing\n");
				}
				Rmp->autsim = -1;
				break;

/* ... FUZZ  Enter the fuzz factors */
			case SIM_FUZZ:
				need_show = 1;								/* Reshow needed */
				while ( (ilay = LexGetInt(0, "Layer # to fuzz? (done) ")) > 0) {
					tmp = sample->first;
					for (i=1; i<ilay && tmp!=NULL; i++) tmp=tmp->next;
					if (tmp == NULL) {
						ERRprintf("ERROR: Layer %d is beyond current set of definitions\n", ilay);
						LexFlush();
						break;
					}
					tmp->fuzzd = LexGetReal(0.0f, "Amount to fuzz this layer? ");
					tmp->fuzzs = LexGetInt(3, "Number of iterations? (3) ");
				}
				Rmp->autsim = -1;							/* Flag obsolete simulation */
				break;

/* ... SIM - Ignore multiple requests for SIM program */
			case SIM_SIM:
				break;

/* ... MAX SUBLAYER THICKNESS */
			case SIM_MAXPTH:
				sprintf(token, "Maximum sublayer path length (%f x 10^15 at/cm^2)? ", default_maxpath);
				sample->maxpth = LexGetReal(default_maxpath, token);
				Rmp->autsim = -abs(Rmp->autsim);	/*  Flag obsolete simulation */
				break;

/* ... Default maximum path */
			case DEFAULT_MAXPATH:
				default_maxpath = LexGetReal(DFLT_SUBLAYER_THICK, "Default MAXPATH to be used for new sample definitions (10^15 at/cm2): ");
				break;

/* ... SPLOT - Individual element plot */
			case SIM_SPLOT:
				RbsSplot();
				break;

/*  ... ABSORBER - Modify number of sample layers comprising absorber foil */
/*  ... FRES_ABSORBER - Modify number of sample layers comprising absorber foil */
			case SIM_ABSORBER:
			case SIM_FRESABSORBER:
				sample->absorber_layers = abs(LexGetInt(0,"# of layers in absorber foil? (0) "));
				sample->fres_only_absorber = (citem->rcode == SIM_FRESABSORBER);
				Rmp->autsim = -abs(Rmp->autsim);	/*  Flag obsolete simulation */
				break;

/*  * * * * * * * HELP * * * * * * * * * * */
/*  ... ? - Quick list of commands */
			case SIM_HELP:
			case SIM_QUEST:
				LexCmdlPrint(cmlist1, sizeof(CMTYPE), "Sim general commands:");
				LexCmdlPrint(cmlist2, sizeof(CMTYPE), "Sample editing:");
				LexCmdlPrint(cmlist3, sizeof(CMTYPE), "Parameter & Configuration:");
				LexSystem(U_HELP,NULL);
				break;

/*  ... XSECT - Modify Scattering cross sections for Deuterium and Hydrogen */
			case SIM_XSECT:
				if (LexCheckHelp("XSECT", XSectHelp, NULL)) break;
				TTYprintf("Old values are:    1H=%15.6g   2H=%15.6g\n", sigtab[0], sigtab[1]);
				sigtab[0] = LexGetReal(sigtab[0],"New 1H linear cross section value? (unchanged) ");
				sigtab[1] = LexGetReal(sigtab[1],"New 2H linear cross section value? (unchanged) ");
				Rmp->autsim = -1;	/*  Flag obsolete simulation */
				break;

/*  ... E2COF - Modify coefficient for 1/E^2 dependence on cross sections */
/*              for Deuterium and Hydrogen (Patrick S.) */
			case SIM_E2COF:
				if (LexCheckHelp("E2COF", E2CofHelp, NULL)) break;
				TTYprintf("Coeff 1/E^2 are:   1H=%15.6g   2H=%15.6g\n", coffe2[0], coffe2[1]);
				coffe2[0] = LexGetReal(coffe2[0],"New 1H coeff for 1/E(keV)^2 term? (unchanged) ");
				coffe2[1] = LexGetReal(coffe2[1],"New 2H coeff for 1/E(keV)^2 term? (unchanged) ");
				Rmp->autsim = -1;	/*  Flag obsolete simulation */
				break;

/*  ... PILEUP - how to handle pileup */
			case SIM_PILEUP:
				if (LexGetTokenP(token, sizeof(token),
					"Pileup calculation mode [none | fast | full]: (full) ")) {
					if (LexEqual(token, "none", 1) || stricmp(token, "off") == 0) {
						SimPileup = NULL;
					} else if (LexEqual(token, "fast", 1) || stricmp(token, "old") == 0 || stricmp(token, "DOS") == 0) {
						SimPileup = SimOldPileup;
					} else if (LexEqual(token, "correct",  1) || stricmp(token, "full") == 0 ||
						        LexEqual(token, "accurate", 3) || stricmp(token, "new") == 0) {
						SimPileup = SimNewPileup;
					} else {
						ERRprintf("ERROR: Invalid pileup mode.  Specify [FULL|OLD|OFF]\n");
						LexFlush();
					}
				}
				Rmp->autsim = -1;
				break;
					

/* ... FOIL - enable/disable and read foil data file */
			case FOIL_SIM:
				if (! LexGetTokenP(token, sizeof(token), "Foil [OFF | on | {-load} <file> | -generate ...]: ")) {
					SimStopperFoilProc = NULL;
				} else if (stricmp(token, "OFF") == 0 || LexEqual(token, "Disable", 3)) {
					SimStopperFoilProc = NULL;
				} else if (stricmp(token, "ON")  == 0 || LexEqual(token, "Enable",  2)) {
					SimStopperFoilProc = SimCalcStopFoil;
					if (SimStopperFoilDataTable == NULL) {
						ERRprintf("INFO: FOIL ON ain't much good until you do a FOIL <file> data load also.\n");
					}
				} else if (LexEqual(token, "-generate", 4)) {
					int z,m,npt;
					REAL e_min,e_inc;

					if (! LexGetTokenP(token, sizeof(token), "Incident particle (abort): ")) break;
					if (LexEscape(TRUE)) break;
					if (! RbsIdentp(token, &z, &m, NULL)) {
						ERRprintf("ERROR: Element not recognized: %s\n", token);
						break;
					}
					e_min = LexGetReal(100.0, "Lower energy limit (100 keV): ");
					if (LexEscape(TRUE)) break;
					e_inc = LexGetReal( 20.0, "Energy increment (20 keV): ");
					if (LexEscape(TRUE)) break;
					npt   = LexGetInt(300, "Number of points (300): ");
					if (LexEscape(TRUE)) break;

					if (! GenerateStopFoilTable(z,m,e_min,e_inc,npt))
						ERRprintf("GenerateStopFoilTable returned false\n");
				} else {
					if (LexEqual(token, "-load", 2)) {
						if (! LexGetTokenP(token, sizeof(token), "Filename: (abort) "))
							*token = '\0';
					}
					if (*token != '\0') SimReadStopFoilData(token);
				}
				Rmp->autsim = -1;
				break;

/*  ... TOF set */
			case SIM_TOF:
				if (LexOnOff(FALSE, "Turn TOF debug file on/OFF: ")) {
					SimInitFillSpectrum = SimTOFInitFillSpectrum;
					SimTermFillSpectrum = SimTOFTermFillSpectrum;
					SimFillSpectrum     = SimTOFFillSpectrum;
				} else {
					SimInitFillSpectrum = NULL;
					SimTermFillSpectrum = NULL;
					SimFillSpectrum     = SimAnlyz;
				}
				Rmp->autsim = -1;
				break;

/*  ... RES_READ - read a resonance data file */
			case RES_READ:
				if (LexGetFileP(token, sizeof(token), "Resonance data file: "))
					if (! ResRead(token)) LexFlush();
				Rmp->autsim = -1;
				break;

/*  ... EQUATION - set the diffusion equation for the layer */
			case SIM_EQUATION:
				reget_sim_equation:
				if (! LexGetTokenP(token, sizeof(token), "DIFF Equation (? for list | <cr> to abort): ")) break;
				if (LexEqual(token, "help", 4) || LexEqual(token, "-help", 5) || *token == '?' || stricmp(token, "-?") == 0) {
					TTYprintf("Equation types available: \n");
					for (i=0; eqlist[i].name != NULL; i++) {
						if(eqlist[i].minlen > 0) 
							TTYprintf("  %-16s %s\n", eqlist[i].name, eqlist[i].description);
					}
					goto reget_sim_equation;
				}
				if ( (eq = LexCmdl(token, eqlist, sizeof(SIMEQN))) == NULL) {
					ERRprintf("ERROR: %s is an ambiguous or unrecognized equation\n"
								 "       Type EQUATION ? for a listing\n", token);
					LexFlush();
					break;
				}
				need_show   =  1;						/* Reshow needed					*/
				Rmp->autsim = -1;						/* New simulation					*/

				if (eq->type == EQ_NONE) {			/* Quick way out of this loop	*/
					layer->eqn = NULL;
					break;
				}
				layer->eqn = eq;						/* And new equation			*/

/*  ... EQ_USER is very special.  It can specify an arbitrary function of */
/*  ...         the depth and the five parameters here.  Save fnc as strdup() */
				if (eq->type == EQ_USER) {			/* Okay, here we go! */
					if (! LexGetMath(token, sizeof(token))) {	/* Long prompt */
						TTYputs(" Specify a function taking parameters x (in angstroms), x0 (total thickness),\n"
								  " and layer parameters p[0]-p[4], and returning the atomic fraction of the\n"
								  " diffusing species.\n"
								  "    Example: p[0]*abs(1-x/p[1])\n"
								  " Specifying / will reuse the previous equation if defined.\n");
						if (! LexGetMathP(token, sizeof(token), "Equation (abort): ")) {
							layer->eqn = NULL;
							break;
						}
					}
					if (strcmp(token, "/") != 0) layer->diff_eqn = strdup(token);
					if (layer->diff_eqn == NULL) layer->diff_eqn = strdup("0.01");
				}

/*  ... Read any parameters from the user for the equations */
/*  ... Possibly also get units of the dose values				*/
				layer->eqn_units = eq->dflt_units;
				for (i=0; i<eq->npar; i++) {
					layer->par[i] = LexGetReal(layer->par[i], eq_queries[eq->prompts[i]]);
					if (i != 0 || eq->dflt_units == NULL) continue;
					if (! LexChkToken(token, sizeof(token))) continue;
					if ( (nit = SimLookupUnit(token)) != NULL) {
						layer->eqn_units = nit;
						LexGetToken(token, sizeof(token));
					}
				}

/*  ... Some special checks, eqn mode off */
				if (layer->eqn->type == EQ_CONST && layer->par[0] == 0.0) {
					layer->eqn = NULL;
				}
				break;

/*  * * * * * * * OVERLAY - recreation of RUMP command * * * * * */
			case SIM_OVERLAY:
				layer = SimCheckLayer(layer);
				g_layer = SimCheckGlobal(g_layer);
				sbuf = ibuf;
				if (! RbsPlot(PLT_QY+PLT_OV, sbuf)) {
					LexFlush();
					break;
				}
				if (sbuf != ALTBUF) ibuf = sbuf;
				break;

/*  * * * * * * * COMPARE - recreation of RUMP command * * * * * */
			case SIM_COMPARE:
				LexInsText("RETURN COMPARE SIM");	/*  Do compare and return */
				break;

/*  * * * * * * * RECALCULATE - mark sim as requiring recalculation * * * * * */
			case SIM_RECALCUL:
				silent = FALSE;
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-silent", 2)) {
						SimSilent = TRUE;
					} else {
						LexBackup();
					}
				}
				Rmp->autsim = -1;						/* Mark as invalid */
				SimCheck(sample);						/* And remake now! */
				SimSilent = FALSE;					/* Reset silent of changed */
				break;

			default:
				ERRprintf("ERROR: Unrecognized SIM command - should not happen\n");
				break;
		}
	}		/* while (TRUE) loop */
	panic; return(FALSE);
}

/* ============================================================================
-- Usage Guide:
--
-- Usage:   SUBROUTINE SIMRES
--
-- Quick:   Resets the simulation structure
--
-- Inputs:  none
--
-- Output:  none
--
-- Return:  none
--
-- Global Vars:  rump, sample.h
--
-- Called By:    anlytc, simdif
--
-- Calls:        None
--
-- Description:
--     The only function of SIMRES
--     the routine is to zero / set to default all of the arrays in
--     the common block /sample.h/.
--
-- Notes: This routine also creates the SAMPLE *sample structure during
--        early RUMP initialization (anlytc(U_INIT))
============================================================================ */
void SimReset(int level) {

	int i;
	LAYER *layer;

/* Decide if we can re-initialize the structures */
	if (sample != NULL) {
		for (layer=sample->first; layer!=NULL; layer=layer->next) {
			if (layer->locked != 0) {
				ERRprintf("ERROR: Reset of SIM not possible until PERT releases locks on layers\n");
				LexFlush();
				return;
			}
		}
	}

	sample = SimCreateEmptySample(sample);	/* Empty and/or recreate */
	SimDefaultSample = sample;
	Rmp->autsim = 0;								/* No simulation to recalc		*/
	SimStopperFoilProc = NULL;					/* Disable the stopping foil	*/

/* Do more thorough reset */
	if (level == U_INIT) {
		if (SimDensityTable != NULL && SimDensityTableSize > MAXDEN) {	/* Clear density tables */
			for (i=SimDensityTableSize-MAXDEN; i<SimDensityTableSize; i++)
				*SimDensityTable[i].ident = '\0';
			SimDensityTableCount = SimDensityTableSize-MAXDEN;
		}
	}

	return;
}

/* ============================================================================
-- Subroutine to read the density tables from data directory
--
-- Usage: int SimLoadDensityTable(char *filename)
--
-- Inputs: filename - file to open and read (assumed to exist)
--
-- Output: Creates table SimDensityTable[] containing all known units.
--
-- Returns: -1 => failure.  Reason will be printed
--           0 => success
--
-- Notes: Extra space will be allocated in the SimDensityTable for up to
--        MAXDEN new values at run time.
============================================================================ */
int SimLoadDensityTable(char *fname) {

	FILE *funit;
	char inbuf[DFLT_STR_SIZE], *aptr, *bptr;
	int  icnt;

	static UNITS StaticTable[] = {
		{"A",			ANGSTROMS,		1.0},			/* Base unit (default)			*/
		{"nm",		ANGSTROMS,		10.0},		/* Wish it were base unit		*/
		{"um",		ANGSTROMS,		10000.0},	/* Multiply by 1E4 to A			*/
		{"/CM2",		ATOMIC,			1.0},			/* x10^15 atoms/cm^3				*/
		{"M/CM2",	MOLECULAR,		1.0}			/* x10^15 molecules/cm^3		*/
	};
	#define	STATIC_LIST_LENGTH	(sizeof(StaticTable)/sizeof(StaticTable[0]))

/* Free previous space if allocated */
	if (SimDensityTable != NULL) {
		free(SimDensityTable);
		SimDensityTableCount = SimDensityTableSize = 0;
	}

/* Scan file first to determine number of entries */
	icnt = 0;
	funit = NULL;

	SysFindFile(SimDensityFile, fname, RbsConfigPath, NULL, R_OK);

	if (*SimDensityFile != '\0') {
		if ( (funit = fopen(SimDensityFile, "r")) == NULL) {
			ERRprintf("ERROR: %s density table file could not be opened for reading\n", SimDensityFile);
		} else {
			TTYprintf("Loading atomic density table: %s\n", SimDensityFile);
			while (fgets(inbuf, sizeof(inbuf), funit) != NULL) {
				aptr = inbuf;
				while (isspace(*aptr)) aptr++;
				if (*aptr == '#' || *aptr == '\0') continue;
				icnt++;
			}
			fseek(funit, 0L, SEEK_SET);					/* Rewind the file */
		}
	}

/* Allocate the density table and preload with permanent values */
	SimDensityTableSize = icnt + STATIC_LIST_LENGTH + MAXDEN;
	SimDensityTable = calloc(SimDensityTableSize, sizeof(*SimDensityTable));
	for (icnt=0; icnt<STATIC_LIST_LENGTH; icnt++) SimDensityTable[icnt]=StaticTable[icnt];

/* And now rescan the file, interpreting the data this time */
	if (funit != NULL) {
		while (fgets(inbuf, sizeof(inbuf), funit) != NULL) {
			aptr = inbuf;												/* Skip leading <sp>	*/
			while (isspace(*aptr)) aptr++;						/* Any space char		*/
			if (*aptr == '#' || *aptr == '\0') continue;		/* Check if comment	*/
			bptr = aptr;												/* Scan to name end	*/
			while (!isspace(*bptr) && *bptr!='\0') bptr++;	/* Walking forward	*/
			if (*bptr == '\0') continue;							/* Something left?	*/
			*bptr++ = '\0';											/* Terminate string	*/
			strscpy(SimDensityTable[icnt].ident, aptr, sizeof(SimDensityTable->ident));
			while (isspace(*bptr)) bptr++;						/* Skip space again	*/
			if (*bptr == '\0') continue;							/* Nothing there		*/
			SimDensityTable[icnt].type    = ABSOLUTE;
			SimDensityTable[icnt].density = (REAL) atof(bptr);
			icnt++;
		}
		fclose(funit);
	}
	SimDensityTableCount = icnt;

/*	TTYprintf("INFO: Density tables loaded (%d, %d permanent)\n", SimDensityTableCount, STATIC_LIST_LENGTH); */
	return(0);
}


/* ===========================================================================
-- Routine to take a token and return the closest matching unit within the
-- density table.  Returns NULL if nothing can be found.
=========================================================================== */
static UNITS *SimLookupUnit(char *token) {
	int i;
	for (i=0; i<SimDensityTableCount; i++) {
		if (stricmp(token, SimDensityTable[i].ident)==0) return(SimDensityTable+i);
	}
	return(NULL);
}

/* ===========================================================================
-- Routine returns integer for the ordinal position of a layer.  1 is top.
-- Returns 0 if layer cannot be found.
=========================================================================== */
int SimGetLayerNum(LAYER *layer) {

	int i;
	LAYER *tmp;

	for (i=1,tmp=sample->first; tmp!=NULL; i++,tmp=tmp->next) {
		if (layer == tmp) return(i);
	}
	return(0);									/* Invalid layer descriptor */
}

/* ===========================================================================
-- Routine to allocate and initialize a global layer with defaults.  Linked 
-- list entries are empty, must be inserted after return;  NULL indicates 
-- failure to allocate more memory.
=========================================================================== */
int SimGetGlobalNum(GLOBAL_LAYER *g_layer) {

	int i;
	GLOBAL_LAYER *gtmp;

	for (i=1,gtmp=sample->g_first; gtmp!=NULL; i++,gtmp=gtmp->next) {
		if (g_layer == gtmp) return(i);
	}
	return(0);									/* Invalid layer descriptor */
}

static GLOBAL_LAYER *SimAllocGlobal(void) {

	GLOBAL_LAYER *tmp;

	if ( (tmp = calloc(1,sizeof(*tmp))) == NULL) {
		ERRprintf("Oh SHIT! Unable to allocate layer memory.  I suggest quiting *NOW*!!!\n");
		TTYflush();
		return(NULL);
	}

	tmp->previous = NULL;
	tmp->next     = NULL;
	return(tmp);
}


static GLOBAL_LAYER *SimDeleteGlobal(GLOBAL_LAYER *g_layer) {

	GLOBAL_LAYER *rcode=NULL;
	
	if (g_layer != NULL) {
		if (g_layer->locked != 0) {
			ERRprintf("ERROR: Layer cannot be deleted because it is locked by PERT\n");
			return(g_layer);
		}
		if ( (rcode = g_layer->next) == NULL) rcode = g_layer->previous;
		if (g_layer->next     != NULL) (g_layer->next)->previous = g_layer->previous;
		if (g_layer->previous != NULL) (g_layer->previous)->next = g_layer->next;
		if (g_layer->previous == NULL) sample->g_first = g_layer->next;
		free(g_layer);
	}
	return(rcode);
}

static BOOL SimGlobalEmpty(GLOBAL_LAYER *g_layer) {
	BOOL rcode;

	rcode = g_layer == NULL || *g_layer->curve == '\0';
	return(rcode);
}

static GLOBAL_LAYER *SimCheckGlobal(GLOBAL_LAYER *g_layer) {

	int i;

/* ... Never delete top layer, even if empty! */
	if ( (g_layer == NULL) ||											/* Nothing		*/
		  (g_layer->next==NULL && g_layer->previous==NULL) ||	/* Only one		*/
		  (! SimGlobalEmpty(g_layer)) || (g_layer->locked != 0)) /* Not empty	*/
		return(g_layer);

/* ... Okay, delete it */
	i  = SimGetGlobalNum(g_layer);
	g_layer = SimDeleteGlobal(g_layer);
	TTYprintf(" Global diffusant definition %2d has been deleted\n", i);
	return(g_layer);
}

/* ===========================================================================
-- Routine to delete layer specified.  Will modify the linked list as needed.
-- Returns layer below unless it is the bottom layer, then returns next up.
--
-- Also responsible for freeing up any space allocated within the layer.
=========================================================================== */
static LAYER *SimDeleteLayer(LAYER *layer) {
	LAYER *rcode=NULL;

	if (layer != NULL) {
		if (layer->locked != 0) {
			ERRprintf("ERROR: Layer cannot be deleted because it is locked by PERT\n");
			return(layer);
		}
		if ( (rcode = layer->next) == NULL) rcode = layer->previous;
		if (layer->next     != NULL) (layer->next)->previous = layer->previous;
		if (layer->previous != NULL) (layer->previous)->next = layer->next;
		if (layer->previous == NULL) sample->first = layer->next;
		if (layer->diff_eqn != NULL) free(layer->diff_eqn);	/* String from strdup() */
		free(layer);
	}
	return(rcode);
}

/* ===========================================================================
-- Routine to duplicate a sample structure, returning the copy
=========================================================================== */
SAMPLE *SimDuplicateSample(SAMPLE *orig) {

	SAMPLE *new;
	LAYER *l_orig, *l_new, *hold;
	GLOBAL_LAYER  *g_orig, *g_new, *g_hold;

	new = SimCreateEmptySample(NULL);
	if (orig == NULL) return(new);

/* Copy the layers, being careful of the linked lists */
	l_orig = orig->first; l_new = new->first;
	while (l_orig != NULL) {
		hold = l_new->previous;					/* Save the backlink */
		*l_new = *l_orig;							/* Copy everything over (careful) */
		l_new->previous = hold;					/* Restore it */
		if ( (l_orig = l_orig->next) != NULL) {
			l_new->next = SimAllocLayer();
			l_new->next->previous = l_new;
			l_new = l_new->next;
		}
	}

/* Copy the global layers, being careful of the linked lists */
	g_orig = orig->g_first; g_new = new->g_first;
	while (g_orig != NULL) {
		g_hold = g_new->previous;					/* Save the backlink */
		*g_new = *g_orig;								/* Copy everything over (careful) */
		g_new->previous = g_hold;					/* Restore it */
		if ( (g_orig = g_orig->next) != NULL) {
			g_new->next = SimAllocGlobal();
			g_new->next->previous = g_new;
			g_new = g_new->next;
		}
	}

	return(new);
}

/* ===========================================================================
-- Routine to initialize an allocated sample structure to some defaults.
-- If the passed structure is not NULL, it will be free'd and then reset to
-- the original status
=========================================================================== */
SAMPLE *SimCreateEmptySample(SAMPLE *samp) {

	int i;
	
	if (samp == NULL) {
		samp = calloc(1, sizeof(*samp));
	} else {
		while (samp->first   != NULL) SimDeleteLayer(samp->first);
		while (samp->g_first != NULL) SimDeleteGlobal(samp->g_first);
	}

/* Allocate the first specific layer and the first global diffusant layer */
	samp->first   = samp->layer   = SimAllocLayer();
	samp->g_first = samp->g_layer = SimAllocGlobal();

/* Clear other structures */
	samp->nel   = 0;							/* No elements in structure */
	for (i=0; i<MAXEL; i++) samp->z2[i] = samp->nukem[i] = 0;

	*samp->desc  = '\0';
	samp->absorber_layers  = 0;
	samp->fres_only_absorber = FALSE;
	samp->straggle = 0.0;						/* Disable straggling */
	samp->multiple = 0.0;						/* Multiple scattering disabled */
	samp->noise    = FALSE;						/* Disable noise addition */
	samp->background = NULL;					/* No background structure */
	samp->maxpth = default_maxpath;

	return(samp);
}
		
	


/* ===========================================================================
-- Routine to allocate and initialize a layer with defaults.  Linked list
-- entries are empty, must be inserted after return;  NULL indicates failure
-- to allocate more memory.
=========================================================================== */
static LAYER *SimAllocLayer(void) {

	LAYER *tmp;
	int i;

	if ( (tmp = calloc(1,sizeof(*tmp))) == NULL) {
		ERRprintf("Oh SHIT! Unable to allocate layer memory.  I suggest quiting *NOW*!!!\n");
		TTYflush();
		return(NULL);
	}

	tmp->previous = NULL;
	tmp->next     = NULL;
	tmp->thick.magn   = 0.0;
	tmp->thick.units  = &angstroms;				/* Visible as Angstroms */
	tmp->num_sublayers = 0;							/* Clear the new layer	*/
	tmp->thisub.magn   = 0.0;						/* Possibly inactive		*/
	tmp->thisub.units  = &atoms_cm2;				/* Default units			*/
	tmp->eqn       = NULL;							/* No equation				*/
	tmp->eqn_units = NULL;							/* Units meaningless		*/
	tmp->diff_eqn  = NULL;							/* No diffusion equation */
	tmp->fuzzd     = 0.0f;							/* No fuzzing				*/
	tmp->fuzzs     = 0;								/* NOo fuzzing				*/
	tmp->locked    = 0;								/* Layer is not locked	*/
	for (i=0; i<MAXPAR; i++) tmp->par[i]  = 0.0;
	for (i=0; i<MAXEL;  i++) tmp->matrix[i] = tmp->species[i] = 0.0;
	return(tmp);
}


/* ===========================================================================
-- Returns TRUE if layer has zero thickness, or no composition.
-- FALSE indicates that layer is not empty.
=========================================================================== */
static BOOL SimLayerEmpty(LAYER *layer) {
	
	int i;

	int rcode = TRUE;
	if (layer->thick.magn > 0.0) {
		for (i=0; i<MAXEL && rcode; i++) rcode = (layer->matrix[i] > 0.0);
	}
	return(rcode);
}


/* ===========================================================================
-- Routine checks to see if layer must be kept, either not empty or only
-- one.  Otherwise, the layer is deleted.  Returns pointer to valid layer,
-- next one in stack if available, or previous if it was the last one.
=========================================================================== */
static LAYER *SimCheckLayer(LAYER *layer) {

	int i;

/* ... Never delete top layer, even if empty! */
	if ( (layer == NULL) ||											/* Nothing		*/
		  (layer->next==NULL && layer->previous==NULL) ||	/* Only one		*/
		  (! SimLayerEmpty(layer)) || (layer->locked != 0)) /* Not empty	*/
		return(layer);

/* ... Okay, delete it */
	i  = SimGetLayerNum(layer);
	layer = SimDeleteLayer(layer);
	TTYprintf(" Layer %2d has been deleted\n", i);
	return(layer);
}


/* ===========================================================================
-- Usage Guide:
--
-- Usage:   SUBROUTINE SimShowSample(layer, g_layer, j, detail)
--
-- Quick:   Output routine for the SHOW command
--
-- Inputs:  Common block /sample.h/ for sample information
--          Common block /ATOMS/ for element names
--          nlines - -1 --> just output via TYPER
--                   >0 --> Output with nlines lines already used
--
-- Output:  Directed towards terminal
--
-- Return:  none
--
-- Global Vars:  sample.h, atoms
--
-- Called By:    SIM
--
-- Calls:        None
--
-- Description:
--     Subroutine SAMOUT does the output for SHOW.  We display
--     the following information for each layer:
--        LAYER   SUBL  THICK  UNIT
--               EQUATION  (Parameter amount)
--               SUBSTRATE (Element name  AMT; for each element)
--               SPECIES   (Element name  AMT; for each element)
--     The equation, parameter, and species information will be
--     eliminated from the layers for which an equation has not
--     been specified.
--
-- Notes:
============================================================================ */

#define	BARLINE	"=============================================================================="

void SimShowSample(LAYER *layer, GLOBAL_LAYER *g_layer, int nlines, BOOL detail) {

/*  -- Local variables -- */
	int ilay, j;
	char outline[81], tmpl[30], *aptr;				/*  80 char line buffer */
	LAYER *tmp;
	GLOBAL_LAYER *gtmp;
	BOOL DoneHeader;
	
/*  -- Code begin -- */
	tmp = sample->first;
	if (tmp == NULL ||
		(tmp->next==NULL && tmp->previous==NULL && SimLayerEmpty(tmp)) ) {
		samout2(nlines,"Vacuum");
		samout2(nlines, BARLINE);
		return;
	} 

	if (*sample->desc != '\0') {
		sprintf(outline, "Simulation Desc: %s", sample->desc);
		samout2(nlines, outline);
	}

	samout2(nlines,
			  "  #         Thickness         Sublayers     Composition  . . .");

	for (ilay=1,tmp=sample->first; tmp!=NULL; tmp=tmp->next,ilay++) {

/*  .. First line:  Layer number, Thickness, Unit */
		sprintf(outline,"%c%3d %3s %10.2f %5s ", (tmp==layer) ? '*' : ' ',
				  ilay, (tmp->locked!=0) ? "(L)" : "   ",
				  tmp->thick.magn, tmp->thick.units->ident);

/*  .. Sublayer mode, three possibilities */
		aptr = outline+strlen(outline);					/* Should be outline+21 */
		if (tmp->num_sublayers != 0) {
			sprintf(aptr, "       %3d     ", tmp->num_sublayers);
		} else if (tmp->thisub.magn != 0.0) {
			sprintf(aptr, "%10.2f %4s", tmp->thisub.magn, tmp->thisub.units->ident);
		} else {
			sprintf(aptr, "       auto    ");
		}

/*  .. Same line: Substrate composition */
		aptr = outline+strlen(outline);					/* Should be outline+35		*/
		for (j=0; j<MAXEL; j++) { 							/* Matrix composition		*/
			if (tmp->matrix[j] == 0.0) continue;		/* Nothing there				*/
			sprintf(tmpl,"  %5s%8.3f", sample->elem_names[j], tmp->matrix[j]);
			if (strlen(outline)+strlen(tmpl) >= sizeof(outline)) {
				samout2(nlines, outline);
				memset(outline, ' ', sizeof(outline));
				*aptr = '\0';									/* Reset starting point		*/
			}
			strcat(outline, tmpl);
		}
		samout2(nlines,outline);
		if (detail) ShowDetail(sample, ilay);

/*  .. Following that: Any fuzzing */
		if (tmp->fuzzs != 0) {				/* Have fuzzing */
			sprintf(outline,"                  with fuzzing of %.2f in %d steps", tmp->fuzzd, tmp->fuzzs);
			samout2(nlines, outline);
		}

/*  .. Following that: Diffusing species composition */
		if (tmp->eqn != NULL) {				/*  diffusion equation and parameters */
			strcpy(outline,"                  with diffusant:");
			aptr = outline+strlen(outline);
			for (j=0; j<MAXEL; j++) {			/*  substrate composition */
				if (tmp->species[j] == 0.0) continue;
				sprintf(tmpl,"  %5s%8.3f", sample->elem_names[j], tmp->species[j]);
				if (strlen(outline)+strlen(tmpl) >= sizeof(outline)) {
					samout2(nlines, outline);
					memset(outline, ' ', sizeof(outline));
					*aptr = '\0';									/* Reset starting point		*/
				}
				strcat(outline, tmpl);
			}
			samout2(nlines,outline);

/*  .. Finally (conditional on equation present): Equation type and parms. */
			sprintf(outline, "        %8s", tmp->eqn->name);
			aptr = outline+strlen(outline);
			if (tmp->eqn->type == EQ_USER) {					/* Special case */
				sprintf(aptr, "  %s", tmp->diff_eqn);
				samout2(nlines, outline);
				sprintf(outline, "              ");
				aptr = outline+strlen(outline);
			}
			for (j=0; j<tmp->eqn->npar; j++) {
				if (j==0 && tmp->eqn->dflt_units != NULL) {
					sprintf(aptr, " %11g %s", tmp->par[j], tmp->eqn_units->ident);
				} else {
					sprintf(aptr, " %11g", tmp->par[j] );
				}
				aptr += strlen(aptr);;
			}
			samout2(nlines,outline);

			samout2(nlines," ");			/*  Blank separating line for equations */
		}
	}

/* Global diffusion equations */
	DoneHeader = FALSE;
	for (ilay=1,gtmp=sample->g_first; gtmp!=NULL; gtmp=gtmp->next,ilay++) {
		if (*gtmp->curve == '\0') continue;
		if (! DoneHeader) {
			samout2(nlines,
				  " Global impurity (diffusant) profiles\n"
				  "  #    Curve/Equation          Start/Mode   Composition  . . .");
			DoneHeader = TRUE;
		}

/*  .. First line:  Curve name, start layer, mode Layer number, Thickness, Unit */
/*  *008 (L) 12345678901234567890123456789012 01/08  */
		sprintf(outline,"%c%3d %3s %-25s %2d/%2d ", (gtmp==g_layer) ? '*' : ' ',
				  ilay, (gtmp->locked!=0) ? "(L)" : "   ",
				  gtmp->curve, gtmp->start, gtmp->mode);

/*  .. Same line: Substrate composition */
		aptr = outline+strlen(outline);					/* Should be outline+35		*/
		for (j=0; j<MAXEL; j++) { 							/* Matrix composition		*/
			if (gtmp->species[j] == 0.0) continue;		/* Nothing there				*/
			sprintf(tmpl,"  %5s%8.3f", sample->elem_names[j], gtmp->species[j]);
			if (strlen(outline)+strlen(tmpl) >= sizeof(outline)) {
				samout2(nlines, outline);
				memset(outline, ' ', sizeof(outline));
				*aptr = '\0';									/* Reset starting point		*/
			}
			strcat(outline, tmpl);
		}
		samout2(nlines,outline);
	}

	samout2(nlines, BARLINE);

	if (sample->background != NULL) {
		sprintf(outline, "Background spectrum: %s", sample->background->id);
		samout2(nlines, outline);
		samout2(nlines, BARLINE);
	}

	return;
}

/* ===========================================================================
=========================================================================== */
static void samout2(int nlines, char *token) {

	if (nlines < 0)   {
		TTYputsnl(token);
	} else {
		nlines = min(nlines+1,23);
		ScrScroll(0,1,nlines);
		ScrPutString(nlines,1,token,1);
	}
	return;
}


/* ============================================================================
-- Routine to find an element name within the list of known elements of the
-- sample description (SAMPLE block), or add it if not there.  Returns an
-- index into the element list where it exists.  If there is no more room,
-- a message is pointed and ELEM_INVALID is returned.  Or if add_it is FALSE
-- and the element does not already exist in the list, ELEM_INVALID returned.
--
-- Usage:  int SimLocateName(char *inam, BOOL add_it);
--
-- Inputs: iname  - character element name (like Si28)
--         add_it - flag indicating that element is to be added if not
--                  already there
--
-- Output: potentially adds element to SAMPLE data blocks
--
-- Returns: index to get element from the list in SAMPLE structure
--          or ELEM_INVALID if no room or doesn't exist (add_it FALSE)
============================================================================ */
int SimLocateName(char *inam, BOOL add_it) {

/*  -- Local Variables -- */
	int zz,j,qmass;
	ATOMS *atomp;

/*  Start by parsing out Element symbol and Isotope mass number, if any */
	if (! RbsIdent(inam, &atomp, &qmass)) return(ELEM_INVALID);
	zz = atomp->z;

/*  next search for this in z2(j) and nukem(j) */
	for (j=0; j<MAXEL; j++) {
		if (sample->z2[j] == zz && sample->nukem[j] == qmass) return(j);
		if (sample->z2[j] == 0) break;
	}

	if (! add_it) {							/* Not allowed to insert this one */
		return(ELEM_INVALID);
	} else if (j >= MAXEL) {				/* No space to add more elements	 */
		ERRprintf("ERROR: Too many elements in sample description.  Scales tipped at %s\n", inam);
		return(ELEM_INVALID);
	}

/* Okay, add this one into the list - j is now one beyond last valid one */
	sample->z2[j]    = zz;					/* Insert it */
	sample->nukem[j] = qmass;
	sample->nel      = j+1;
	if (qmass == 0) {
		sprintf(sample->elem_names[j], "%s", atomic_symbol(zz));
	} else {
		sprintf(sample->elem_names[j], "%d%s", qmass, atomic_symbol(zz));
	}
	return(j);
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE PLSHOW
--
--  Quick: Null version, replaces obsolete code
--
--     This copy is nulled out for simplicity, to keep it from getting
--     in the way.  The old code is now in a separate file, in case
--     people actually want to use it.  It's pretty dumb, though.
--
--     Subroutine PLSHOW is derived from SAMOUT, except that the
--     output goes to the plotter.
--
--     INPUTS:   Common block /sample.h/ for sample information
--               Common block /ATOMS/ for element names
--
--     OUTPUT:   Plotted above the graph
--
--     COMMON BLOCKS:     sample.h, GRAPHICS, ATOMS
--     CALLED FROM:       IDS
--     CALLS:             Complot
--
=========================================================================== */
void SimDrawSample(void) {
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE STTOUT
--
--  Quick: Performs the SIM command SAVE
--     Subroutine STTOUT
--
--     INPUTS:   Common block /sample.h/ for sample information
--               Common block /ATOMS/ for element names
--
--     OUTPUT:
--
--     COMMON BLOCKS:     sample.h, ATOMS
--     CALLED FROM:       SIM
--     CALLS:             LCHECK, Lexp
--
=========================================================================== */
#define	LINE_WIDTH		80
#define	MIN_SPACE		20

static BOOL SimWriteSample(void) {

/*  -- Local variables -- */
	int i, itype;
	char file[PATH_MAX];
	char outbuf[LINE_WIDTH], *aptr;
	LAYER *tmp;
	GLOBAL_LAYER *gtmp;
	FILE *lun;
	CURVE *cv, **cv_ptr;

/*  -- Code begin -- */
	if (! LexGetFileP(file, sizeof(file), "Output filename (.lcm): ")) {
		lun = stdout;
	} else {
		SysAddExt(file, ".lcm");
		if ( (lun = fopen(file, "w")) == NULL) {
			ERRprintf("ERROR: %s failed to open for writing\n", file);
			return(FALSE);
		}
	}

	fputs("Sim Reset\n", lun);
	if (*sample->desc != '\0') fprintf(lun, "Description '%s'\n", sample->desc);
	fputs("Layer 1\n", lun);

/*  .. Output the actual layer structure: */
	for (tmp=sample->first; tmp!=NULL; tmp=tmp->next) {
		fprintf(lun, " Thick %g %s\n", tmp->thick.magn, tmp->thick.units->ident);
		if (tmp->num_sublayers != 0) {
			fprintf(lun, "  Sublayer %d\n", tmp->num_sublayers);
		} else if (tmp->thisub.magn != 0)   {
			fprintf(lun, "  Sthickness %g %s\n", tmp->thisub.magn, tmp->thisub.units->ident);
		}
		strcpy(outbuf, " Composition");
		aptr = outbuf+strlen(outbuf);
		for (i=0; i<sample->nel ; i++) {
			if (tmp->matrix[i] != 0) {
				if ( (aptr-outbuf) > sizeof(outbuf)-MIN_SPACE) {
					fprintf(lun, "%s\n", outbuf);
					strcpy(outbuf, "            ");
					aptr = outbuf+strlen(outbuf);
				}
				sprintf(aptr, " %s %g", sample->elem_names[i], tmp->matrix[i]);
				aptr += strlen(aptr);
			}
		}
		fprintf(lun, "%s /\n", outbuf);

		if (tmp->eqn != NULL) {
			fprintf(lun, " Equation %s", tmp->eqn->name);
			if (tmp->eqn->type == EQ_USER) fprintf(lun, "  '%s'\n     ", tmp->diff_eqn);
			for (i=0; i<tmp->eqn->npar; i++) {
				if (i==0 && tmp->eqn->dflt_units != NULL) {
					fprintf(lun, " %g %s", tmp->par[i], tmp->eqn_units->ident);
				} else {
					fprintf(lun, " %g", tmp->par[i] );
				}
			}
			fputs("\n", lun);
			
			strcpy(outbuf, " Species");
			aptr = outbuf+strlen(outbuf);
			for (i=0; i<sample->nel; i++) {
				if (tmp->species[i] != 0)   {
					if ( (aptr-outbuf) > sizeof(outbuf)-MIN_SPACE) {
						fprintf(lun, "%s\n", outbuf);
						strcpy(outbuf, "            ");
						aptr = outbuf+strlen(outbuf);
					}
					sprintf(aptr, " %s %g", sample->elem_names[i], tmp->species[i]);
					aptr += strlen(aptr);
				}
			}
			fprintf(lun, "%s /\n", outbuf);
		}

		if (tmp->fuzzs != 0) fprintf(lun, " Fuzzy %f %d\n", tmp->fuzzd, tmp->fuzzs);

		if (tmp->next != NULL) fputs("Next\n", lun);
	}

/* And the global profile curve(s) */
	for (gtmp=sample->g_first; gtmp!=NULL; gtmp=gtmp->next) {
		if (*gtmp->curve == '\0') continue;
		fprintf(lun, "\nG_Curve %s\n", gtmp->curve);
		if (gtmp->mode  != 0) fprintf(lun, " G_Mode %d\n", gtmp->mode);
		if (gtmp->start != 0) fprintf(lun, " G_Start %d\n", gtmp->start);
		strcpy(outbuf, " G_Composition");
		aptr = outbuf+strlen(outbuf);
		for (i=0; i<sample->nel; i++) {
			if (gtmp->species[i] != 0)   {
				if ( (aptr-outbuf) > sizeof(outbuf)-MIN_SPACE) {
						fprintf(lun, "%s\n", outbuf);
						strcpy(outbuf, "            ");
						aptr = outbuf+strlen(outbuf);
				}
				sprintf(aptr, " %s %g", sample->elem_names[i], gtmp->species[i]);
				aptr += strlen(aptr);
			}
		}
		fprintf(lun, "%s /\n", outbuf);
		if (GVGetInfo(gtmp->curve, &itype, (void **) &cv_ptr) && (itype == GV_2DCURVE || itype == GV_3DCURVE)) {
			cv = *cv_ptr;												/* Get the curve itself */
			fprintf(lun, " Genplot / read << -silent archive %s return sim\n", gtmp->curve);
			for (i=0; i<cv->npt; i++) fprintf(lun, "   %g %g\n", cv->x[i], cv->y[i]);
			fprintf(lun, "@end\n");
		}
		fputs((gtmp->next!=NULL) ? "G_Next\n" : "\n", lun);
	}

/*  .. Miscellaneous: */
	fprintf(lun, "Maxpth %g\n", sample->maxpth);
	if (sample->absorber_layers != 0) {
		fprintf(lun, "%s %d\n",
				  (sample->fres_only_absorber ? "FRES_Absorber" : "Absorber"), 
				   sample->absorber_layers);
	}
	if (sample->straggle != 0.0) fprintf(lun, "Straggle %g\n", sample->straggle);
	if (sample->multiple != 0.0) fprintf(lun, "Multiple_Scatter %g\n", sample->multiple);
	if (sample->noise) fprintf(lun, "Noise On\n");
	fprintf(lun, "Foil %s\n", (SimStopperFoilProc != NULL) ? "enable" : "disable" );

/*  .. Shut off the file */
	if (lun != stdout) fclose(lun);

	return(TRUE);
}

/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE SPLOT
--      Individual element or layer plot.
--  Quick: Performs the SIM command SPLOT (Selective Plot)
--
--     INPUTS:
--
--     OUTPUT:   User interaction, call to PLTIT for actual action
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       SIM
--     CALLS:             LOCNAM, CREATE, PLTIT
--
=========================================================================== */
static void RbsSplot(void) {

	char token[OPTION_STR_SIZE], *aptr;
	int elem, layer;
	SPECTRUM *temp;

	if (Rmp->autsim == 0) {
		ERRprintf("SPLOT ERROR: No simulation structure available\n");
		LexFlush();
		return;
	}	

	if (! LexGetTokenP(token, sizeof(token), "[Element|Sublayer] ")) return;
	layer = strtol(token, &aptr, 10);
	if (*aptr != '\0') {
		layer = LAYER_INVALID;
		if ( (elem = SimLocateName(token,FALSE)) == ELEM_INVALID) {
			ERRprintf("SPLOT ERROR: Element %s does not exist in target\n", token);
			return;
		}
	} else {
		elem = ELEM_INVALID;
	}

   /* TTYprintf("Splot going out for pizza with %d and %d\n",elem,layer); */
	temp   = ALTBUF;
	ALTBUF = TMPBUF;								/*  Put 'simulation' in buffer -1 */
	SimCreateDetails(NULL, elem, layer);	/*  Do it ... */
	ALTBUF = temp;									/*  Restore table integrity */
	RbsPlot(PLT_OV,TMPBUF);						/*  Overlay results on graph */
	Rmp->autsim = -1;								/*  Flag obsolete simulation */

	return;
}


/* ===========================================================================
--  Usage Guide:
--
--  SUBROUTINE NAMEIT
--
--  Subroutine NAMEIT has nothing to do with the spectrum itself.  Rather it
--  is used for convenience in keeping track of the spectra. Specifically, it 
--  looks a the sample structure and makes up a name for the simulation.  The
--  name is of the form  "Simulation of Ni-Si/Si" or whatever is in fact in
--  there.  This is simply a list of (a list of the elements in a layer,
--  separated with -'s), separated with /'s.  If the result is more than 80
--  characters or so, it gets chopped off and ends with "..." .  The result
--  is reasonably workable, although when you are fine tuning a simulation
--  by changing thicknesses and compositions slightly, all of the simulations
--  have the same name. Not much to be done about that.
--
--  Quick: Generates reasonable name for simulated spectrum
--
--  INPUTS:   Sample description in common block sample.h
--
--  OUTPUTS:  String ID(ALTBUF) in RUMP common block
--
--  COMMON BLOCKS:     RUMP, ATOMS, sample.h
--  CALLED FROM:       CHKSIM
--  CALLS:             None
--
=========================================================================== */
#define BUFLEN 78

void SimSetIdent(void) {

/*  -- Local Variables -- */
	int i;
	int dash, slash;
	char *bufp;
	LAYER *layer;

	strcpy(ALTBUF->filename, "temp.rbs");		/* Set name to default */

	if (*sample->desc != '\0') {					/* User set definition?	*/
		strscpy(ALTBUF->id, sample->desc, sizeof(ALTBUF->id));
		return;
	}

	strcpy(ALTBUF->id, "Simulation of ");		/* Start of ID with ... */
	bufp = ALTBUF->id+14;

	slash = FALSE;
	for (layer=sample->first; layer!=NULL; layer=layer->next) {

		if (slash) *bufp++ = '/';		/* Add the slash, when appropriate */
		slash = TRUE;
		dash = FALSE;

		for (i=0; i<sample->nel; i++) {
			if (layer->matrix[i] == 0) continue;
			if (dash) *bufp++ = '-';	/* Add the dash, when appropriate */
			dash = TRUE;

			if (bufp > ALTBUF->id+BUFLEN-5) {		/* Space for next element? */
				strcpy(bufp,"...");						/* No, just put leaders		*/
				return;
			}
			strcpy(bufp, sample->elem_names[i]);
			bufp += strlen(bufp);
		}
	}
	return;
}


/* ===========================================================================
-- This routine takes the UNITS descriptor, the density (in 1E23 units), the
-- number of atoms in the descriptor and returns the conversion factor from
-- the units given to 1E15 atoms/cm^2 units.  If the units is an override
-- density units, then the density may be returned as well.
--
-- Usage: SimThickConvert(UNITS units, REAL density, REAL sum, REAL *pdensity);
--
-- Inputs: units    - pointer to a UNITS structure
--         density  - atomic density in 1E23 atoms/cm^3 (needed for nm, etc).
--         sum      - total number of atoms for molecules->atoms conversion
--         pdensity - pointer to density if value to be returned.  When user
--                    gives unit of SiO2 the density is overriden.
--
-- Output: *pdensity - modified density if not NULL
--
-- Returns: Conversion factor to take user units to 1E15 atoms/cm^2
--
-- Notes:  units->density is a simple power of 10 scaling to get actual 
--         1E15/cm2 from given thickness units (Angstroms assumed above for 
--         all distance units, so to get to microns need factor of 10000).
--         The other case is compound densities which are specified in 1E23 
--         atoms/cm^2.  If the "assumed" user unit is then angstroms of this
--         material, it will naturally convert to 1E15/cm2 as needed.
--
-- IF THIS ROUTINE CHANGES, MODIFY ALSO EQUIVALENT ROUTINES IN CREATR.C
=========================================================================== */
REAL SimThickConvert(UNITS *units, REAL density, REAL sum, REAL *pdensity) {
	REAL thick_to_cm2;

	switch (units->type) {
		case ANGSTROMS:									/* (1E-8) / (1E15)	*/
			thick_to_cm2 = density * units->density;	
			break;
		case ATOMIC:
			thick_to_cm2 = 1;
			break;
		case MOLECULAR:
			thick_to_cm2 = sum;
			break;
		case ABSOLUTE:
			thick_to_cm2 = units->density;
			if (pdensity != NULL) *pdensity = units->density;
			break;
		default:
			thick_to_cm2 = 1;
	}
	return(thick_to_cm2);
}
					
/* ===========================================================================
--  Usage Guide:
--
--     static void ShowaDetail(SAMPLE *sample, int ilayer)
--
--     This routine attempts to show more details concerning the density
--     of the various layers in a "show" command.  It is similar to the
--     calculations in creatr for loading the total structure.
=========================================================================== */
static void ShowDetail(SAMPLE *sample, int ilayer) {

/*  -- Local Variables -- */
	int i, iel;

	REAL	matrix_sum,						/* Sum of matrix composition coeff's	*/
			matrix_density,				/* Weighted matrix density (1E23/cm3)	*/
			thick_to_cm2,					/* Conversion user thick to atoms/cm2	*/
			user_thick,						/* Thickness in user (given) units		*/
			cm2_thick,						/* Thickness in 1E15/cm2 units			*/
			cm_thick;						/* Thickness in centimeters				*/

	LAYER *layer;

/* ---------------------------------------------------------------------------
-- Sum coefficients from composition and species, and determine a weighted
-- atomic density.  For density, we use the idea of hard ball packing with
-- weighted sum of "cm^3/atom" instead of "atoms/cm^3".  Thus, matrix_density
-- below is really weighted sum of inverse density, which is fixed below 
--
-- Prior to 1/97, the sum was done on atomic density rather than inverse
-- density.  This "less correct" mode can be forced if desired.
--------------------------------------------------------------------------- */
	layer = sample->first;
	for (i=1; i<ilayer; i++) {
		if (layer != NULL) layer = layer->next;
	}
	if (layer == NULL) return;

	matrix_sum = matrix_density = 0;	/* Sum compositions	*/

	if (RbsDensityCalc == IMPROVED) {
		for (iel=0; iel<sample->nel; iel++) {
			matrix_sum      += layer->matrix[iel];			/* Matrix sum			*/
			matrix_density  += layer->matrix[iel] /atomic_density(sample->z2[iel]);
		}
		matrix_density = matrix_density / matrix_sum;	/* Average inverse density */
		matrix_density = (1.0f/matrix_density) / 1E23f;	/* Now 1E23 at/cm^3 */
	} else {
		for (iel=0; iel<sample->nel; iel++) {
			matrix_sum      += layer->matrix[iel];			/* Matrix sum			*/
			matrix_density  += layer->matrix[iel]*atomic_density(sample->z2[iel]);
		}
		matrix_density  =  matrix_density/matrix_sum/1E23f;	/* 1E23 at/cm^3 */
	}

/* Determine conversion from user thickness to atoms/cm^2 (thick_to_cm2). */
	thick_to_cm2 = 
		SimThickConvert(layer->thick.units, matrix_density, matrix_sum, &matrix_density);

/* Get the thickness of the layer */
	user_thick = layer->thick.magn;

/* Troll out the number of sublayers */
	cm2_thick = user_thick*thick_to_cm2;			/* Convert to 1E15/cm2	*/
	cm_thick  = cm2_thick/matrix_density/1E8f;	/* Into cm units */
	TTYprintf("      thick = %4.2f at/cm^2 = %4.2f A   (for %6.4f E23 at/cm^3)\n",
				 cm2_thick, cm_thick*1E8, matrix_density);
	return;
}

