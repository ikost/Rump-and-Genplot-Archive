/* pert.c */

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
#include <limits.h>
#include <unistd.h>
#include <float.h>
#include <limits.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "sample.h"
#include "pert.h"
#include "../genplot/curfit.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#define DBUG(x)

typedef enum _METHOD {DOOLITTLE, THOMPSON} METHOD;

#if (defined OS2 || defined NT)
	#define PERT_FILE_EXTS ".per;.pert"
#else
	#define PERT_FILE_EXTS ".pert:.PERT"
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* File LOCMIN.F77: */
void PertFitByLocmin( struct _NLS_DATA *nls, int vol);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void    PertHelpList(void);
static BOOL PertSetAdrs(void);
static BOOL set_adr(PERT_ADR *adr);
static BOOL PertAddVar(void);
static BOOL PertAddEqn(void);
static BOOL PertGetVariable(PERT_ADR *adr);
static BOOL PertFit(BOOL silent);

static BOOL PertWriteParms(void);

static int	PertFitByMethod(METHOD method, NLS_DATA *nls, int volume);
static void PertEvalEqns(void);
static int  PertEvalSim(NLS_DATA *nls);
static int  PertEvalDeriv(REAL *deriv, NLS_DATA *nls, int ipt);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
PERT_NORM pert_norm;
PERT_VAR *pert_vars=NULL;				/* List of varying parameters		*/
PERT_EQN *pert_eqns=NULL;				/* List of equations to be set	*/
PERT_ERR pert_err[NUM_ERR_WINS+1];

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static int	PertVolume=0;					/* Pert information volume */
static METHOD PertMethod = THOMPSON;
static REAL  LastCorrFactor=0;			/* Last corr factor from normalize */

static REAL  EpsCrit = 1E-3f;				/* CurveFit stopping criteria */
static int   MaxIterations = 10;			/* Maximum # of iterations		*/
static REAL  DerivFraction = 0.01f;		/* Fraction for numerical deriv's */


/*      BOOL FUNCTION PERTRB() */
/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION PERTRB()
--     Function PERTRB performs the RUMP command PERTURB.
--     One of several actions is performed according to
--     subcommands (listed in PERTRB HELP), until control is
--     relinquished to RUMP by using the RETURN subcommand.
--
--  Quick: Does user's commands within PERT
--
=========================================================================== */
typedef enum _OPS1 {
	PE_HELP,			PE_CMDLIST,    PE_SAVE,    PE_GET,
	PE_WINDOW,		PE_NORMALIZE,	PE_GO,
	PE_SINGLE,		PE_MULTIPLE,	PE_VOLUME,
	PE_STATUS,		PE_RESET,		PE_RETURN,
	PE_SHOW,			PE_ACTIVE,		PE_COMPARE,
	PE_DEF_VARY,	PE_VARY,			PE_EXPRESSION,
	PE_FREE,			PE_FREE_ALL,   PE_METHOD,
	PE_MAXITER,
	PE_INTERNAL
} OPS1;

typedef struct _CMTYPE1 {
	const char *command;					/* User command			*/
	int			minlen;					/* # of chars required	*/
	OPS1			rcode;					/* key for processing	*/
} CMTYPE1;

static CMTYPE1 cmlist1[] = {
	{"?",				 1, PE_CMDLIST},
	{"help",		 	 1, PE_HELP},
	{"reset",		 5, PE_RESET},
	{"status",		 4, PE_STATUS},	{"parms", 		-2, PE_STATUS},
												{"parameters",	-4, PE_STATUS},
	{"get",			 2, PE_GET},
	{"save",        2, PE_SAVE},
	{"volume", 		 3, PE_VOLUME},
   {"method",		 4, PE_METHOD},

	{"show",			-4, PE_SHOW},			{"active",		-3, PE_ACTIVE},
	{"compare",		-4, PE_COMPARE},

	{"window",	 	 3, PE_WINDOW},		{"error",		-3, PE_WINDOW},
	{"normalize",	 4, PE_NORMALIZE},
	{"vary",			 4, PE_VARY},
	{"expression",	 3, PE_EXPRESSION},
	{"free",			 4, PE_FREE},			{"remove",		-3, PE_FREE},
	{"unvary",		-3, PE_FREE},
	{"freeall",		 5, PE_FREE_ALL},		{"empty",		-5, PE_FREE_ALL},
	{"go", 			 2, PE_GO},				{"fit",			-3, PE_GO},
	{"return", 		 3, PE_RETURN},

	{"mev",			 3, PE_DEF_VARY},		{"energy",		-4, PE_DEF_VARY},
	{"theta",		 5, PE_DEF_VARY},		{"phi",			 3, PE_DEF_VARY},
	{"psi",			 3, PE_DEF_VARY},		{"fwhm",			 4, PE_DEF_VARY},
	{"tau",			 3, PE_DEF_VARY},		{"kev/ch",		 4, PE_DEF_VARY},
	{"kev(0)",		 4, PE_DEF_VARY},		{"correction",	 4, PE_DEF_VARY},
	{"current",		 7, PE_DEF_VARY},

	{"thickness",	 2, PE_DEF_VARY},		{"composition", 2, PE_DEF_VARY},
	{"species",		 2, PE_DEF_VARY},		{"equation",	 2, PE_DEF_VARY},
	{"straggle",	 5, PE_DEF_VARY},
	{"multiple_scatter",	 8, PE_DEF_VARY},
	{"fuzz",			 4, PE_DEF_VARY},
	{"variable",	 3, PE_DEF_VARY},

	{"max_iterate", 5, PE_MAXITER},
	{"maxiterate",	-5, PE_MAXITER},

	{"internal",	 6, PE_INTERNAL},
	
	{"single", 		-2, PE_SINGLE},		/* Obsolete function */
	{"multiple",	-3, PE_MULTIPLE},		/* Obsolete function */

	{NULL,			 0, PE_VARY}
};

BOOL RbsPertMain(int key) {

	PERT_VAR *var, *last;
	PERT_ERR *win;
	PERT_EQN *eqn, *elast;
	char	 token[OPTION_STR_SIZE], filename[PATH_MAX];
	REAL  x1, x2;
	int i, opkey;
	BOOL silent;
	CMTYPE1 *citem;
	
	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), "PERT subcommand: ")) {
		  continue;
		} else if (LexAlias(token, sizeof(token))) {
			continue;							/* Check for alias */
		} else if ( (citem = LexCmdl(token, cmlist1, sizeof(*cmlist1))) == NULL) {
			if (LexSystem(0, token)) continue;

			if (RbsAutoReturn) {
				LexBackup();
				return(TRUE);
			}
			ERRprintf("ERROR: %s unrecognized PERT sub-command\n", token);
			LexFlush();
			continue;
		}

		switch (citem->rcode) {				/* Process commands */
			case PE_CMDLIST:
				LexCmdlPrint(cmlist1, sizeof(*cmlist1), "PERT Subcommands: ");
				LexSystem(U_HELP, NULL);
				break;
			case PE_HELP:						/* Simple help listing	*/
				PertHelpList();
				break;

			case PE_SAVE:
				if (! PertWriteParms()) LexFlush();
				break;
			case PE_GET:
				if (LexGetFileP(filename, sizeof(filename), "Pert description file to load: ")) {
					if (! SysFindFile(filename, filename, RbsSearchPath, PERT_FILE_EXTS, R_OK)) {
						ERRprintf("ERROR: PERT structure file %s not found\n", filename);
						LexFlush();
						break;
					}
					LexExecFile(filename);				/* Start file		*/
					LexSetLocalNoEcho(TRUE);			/* Disable noise	*/
				}
				break;

			case PE_STATUS:					/* List parameters */
				PertStatus();
				break;
			case PE_SHOW:						/* Show sample structure */
				SimShowSample(NULL, NULL, -1, FALSE);
				break;
			case PE_ACTIVE:					/* Show current buffer	*/
				RbsActive(ibuf);
				break;
			case PE_COMPARE:					/* Temporary halt			*/
				LexInsText("COMPARE PERT");
				return(TRUE);
			case PE_RETURN:					/* Return to RUMP level	*/
				TTYputs("You are returning to RUMP.\n");
				return(TRUE);

			case PE_WINDOW:					/* Error window			*/
				opkey = 1;
				win = &pert_err[0];
				while (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-add", 2) || LexEqual(token, "-multiple", 2)) {
						while (win->low >= 0) win++;
					} else if (LexEqual(token, "-reset", 2)) {
						for (i=0; i<=NUM_ERR_WINS; i++) pert_err[i].low = -1;
						win = NULL;
					} else if (LexEqual(token, "-nocursor", 3) || LexEqual(token, "-entry", 2)) {
						opkey = 0;
					} else {
						ERRprintf("ERROR: Illegal option %s ignored\n", token);
					}
				}
				if (win == NULL) break;						/* On a "-reset" */
				if (win >= &pert_err[NUM_ERR_WINS]) win = &pert_err[NUM_ERR_WINS-1];

				x1 = LexGetReal(-987.125, "Channel range of error window (cursor): ");
				if (x1 == -987.125) {
					x1 = RbsGetMeV(&opkey, 200.0, "Define error window with cursor",
						"Window for error computations (200,600) ");
					x2 = RbsGetMeV(&opkey, 600.0, NULL, "Upper error window edge (600)? ");
					x1 = RBSCNNLE(x1, ibuf);
					x2 = RBSCNNLE(x2, ibuf);
					if (opkey == 1) 
						TTYprintf(" Error range specified: %f to %f\n", x1,x2);
				} else {
					x2 = LexGetReal(600.0, "Upper channel of error window (600): ");
				}
				win->low  = (int) (min(x1,x2)+0.5);
				win->high = (int) (max(x1,x2)+0.5);
				win++; win->low = win->high = -1;		/* Disable next */
				break;

			case PE_NORMALIZE:				/* Normalization window	*/
				if (pert_norm.mode == VARY) {
					ERRprintf("ERROR: You cannot set a normalization window if fitting the correction factor.\n");
					LexFlush();
					break;
				}
				opkey = 1;
				while (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-nocursor", 3) || LexEqual(token, "-entry", 2)) {
						opkey = 0;
					} else {
						ERRprintf("ERROR: Illegal option %s ignored\n", token);
					}
				}
				x1 = LexGetReal(-987.125, "Channel range for normalization window (cursor): ");
				if (x1 == -987.125) {
					x1 = RbsGetMeV(&opkey, 100.0, "Define normalization window with cursor",
						"Normalization window (100,200)? ");
					x2 = RbsGetMeV(&opkey, 200.0, NULL, "Upper normalization window edge (200)? ");
					x1 = RBSCNNLE(x1, ibuf);
					x2 = RBSCNNLE(x2, ibuf);
					if (opkey == 1) 
						TTYprintf(" Normalization range specified: %f to %f\n", x1,x2);
				} else {
					x2 = LexGetReal(200.0, "Upper channel of normalization window (200): ");
				}
				pert_norm.low  = (int) (min(x1,x2)+0.5);
				pert_norm.high = (int) (max(x1,x2)+0.5);
				pert_norm.mode = SET;
				break;

			case PE_RESET:
				PertReset(U_RESET);			/* Reset all parameters	*/
				break;

			case PE_GO:							/* Begin fit				*/
				silent = FALSE;
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-silent", 2)) {
						silent = TRUE;
					} else {
						ERRprintf("Unrecognized option (%s) ignored\n", token);
					}
				}
				if (! PertFit(silent)) LexFlush();
				break;

			case PE_SINGLE:					/* Do a quick single var operation	*/
				var = pert_vars; pert_vars = NULL;		/* Save existing varies */
				eqn = pert_eqns; pert_eqns = NULL;		/* Both eqns and vars	*/	
				if (! PertAddVar() || ! PertFit(FALSE))	/* Add var and do a fit	*/
					LexFlush();
				PertFreeAll();									/* Delete temporary var	*/
				pert_vars = var; var = NULL;				/* Restore old settings	*/
				pert_eqns = eqn; eqn = NULL;
				break;

			case PE_MULTIPLE:					/* These are now no-ops	*/
				break;

			case PE_VOLUME:					/* Set level of info	*/
				PertVolume = LexGetInt(2, "Printout Volume during searches? (2) ");
				break;
			case PE_METHOD:
				if (! LexGetTokenP(token,sizeof(token),
							"Search method [Thompson|Doolittle]: ")) break;
				if ( (i = LexSelect(token, "Thompson Doolittle")) <= 0) {
					ERRprintf("ERROR: Let %s write a minimization routine and we might add it\n", token);
				} else if (i == 1) {
					PertMethod = THOMPSON;
				} else {
					PertMethod = DOOLITTLE;
				}
				break;

			case PE_DEF_VARY:					/* Directly state vary operation	*/
			case PE_VARY:						/* Old request a vary operation	*/
				if (citem->rcode == PE_DEF_VARY) LexInsText(citem->command);
				if (! PertAddVar()) LexFlush();
				break;

			case PE_EXPRESSION:
				if (! PertAddEqn()) LexFlush();
				break;
				
			case PE_FREE_ALL:
				PertFreeAll();
				break;
				
			case PE_FREE:
				if (LexGetOption(token, sizeof(token))) {
					if (LexEqual(token, "-all", 4)) {
						PertFreeAll();
						break;
					} else {
						ERRprintf("ERROR: Illegal option %s ignored\n", token);
					}
				}

				i = LexGetInt(-1, "Parameter/Expression to remove from vary list (abort): ");
				if (i <= 0) break;

				last = NULL;
				var  = pert_vars;				/* Start with first varying */
				while (--i && var != NULL) {last = var; var = var->next;}
				if (var != NULL) {
					if (var->adr.type == LAYER_VAR) var->adr.layer->locked--;
					if (last != NULL) {
						last->next = var->next;
					} else {
						pert_vars = var->next;
					}
					free(var);
					break;
				}

				i++;								/* Restore last count		*/
				elast = NULL;					/* Continue with equations */
				eqn  = pert_eqns;
				while (--i && eqn != NULL) {elast = eqn; eqn = eqn->next;}
				if (eqn != NULL) {
					if (eqn->adr.type == LAYER_VAR) eqn->adr.layer->locked--;
					if (elast != NULL) {
						elast->next = eqn->next;
					} else {
						pert_eqns = eqn->next;
					}
					free(eqn);
					break;
				}
				ERRprintf("ERROR: There aren't that many parameters in the list\n");
				LexFlush();
				break;
				
			case PE_MAXITER:
				MaxIterations = LexGetInt(MaxIterations, "Maximum # of iterations (no change / 10=initial): ");
				break;
				
			case PE_INTERNAL:
				EpsCrit = LexGetReal(EpsCrit, "Epsilon stopping criteria (no change): ");
				MaxIterations = LexGetInt(MaxIterations, "Maximum # of iterations (no change / 10=initial): ");
				DerivFraction = LexGetReal(DerivFraction, "Fraction of allowed for derivatives (no change): ");
				break;

			default:
				ERRprintf("ERROR: Developers blew it again (PERT)\n");
				break;
		}
	}
	panic; return(FALSE);
}


/* ===========================================================================
-- Gets name and adds (or modifies) a variable to the list of parameters that
-- are varied during a fit search.
--
-- Usage: BOOL PertAddVar(void)
--
-- Inputs: none - taken from user input stream
--
-- Output: Modifies or adds a new variable onto the list
--
-- Returns: TRUE if successful
=========================================================================== */
static BOOL PertAddVar(void) {

	PERT_VAR *var, *iv;

	var = malloc(sizeof(PERT_VAR));
	var->next       = NULL;
	var->min        = -REAL_MAX;
	var->max        =  REAL_MAX;
	var->adr.type   = INVALID;
	var->adr.layer  = NULL;
	var->adr.x      = NULL;
	*var->adr.desc  = '\0';
	*var->adr.brief = '\0';
	*var->adr.write = '\0';

	if (! PertGetVariable(&var->adr)) {free(var); return(FALSE);}
	var->min = LexGetReal(var->min, "Minimum valid value to search: ");
	var->max = LexGetReal(var->max, "Maximum valid value to search: ");

	for (iv=pert_vars; iv!=NULL; iv=iv->next) {	/* Search if just modify	*/
		if (iv->adr.type != var->adr.type) {		/* Differ because of type	*/
			continue;
		} else if (iv->adr.type == LAYER_VAR) {	/* Different var addresses	*/
			if (iv->adr.x != var->adr.x) continue;
		} else if (iv->adr.type == VARIABLE) {		/* Different var addresses	*/
			if (stricmp(iv->adr.desc, var->adr.desc) != 0) continue;
		}
		iv->adr = var->adr;								/* Copy new values in		*/
		iv->min = var->min;
		iv->max = var->max;
		free(var);
		return(TRUE);
	}

/* It doesn't already exist, so add - choose to add at end instead of front */
	if ( (iv = pert_vars) == NULL) {					/* Will it be the first one */
		pert_vars = var;
	} else {													/* Go until at end of list	 */
		while (iv->next != NULL) iv = iv->next;
		iv->next = var;
	}

/* If LAYER variable, lock the layer so cannot be deleted from under us */
	if (var->adr.type == LAYER_VAR) var->adr.layer->locked++;

	return(TRUE);
}


/* ===========================================================================
-- Gets name and adds (or modifies) an expression to the list of equations
-- that are processed before every function evaluation.  These can contain
-- more general expressions for composition.
--
-- Usage: BOOL PertAddEqn(void)
--
-- Inputs: none - taken from user input stream
--
-- Output: Modifies or adds a new equation onto the pert_eqn list
--
-- Returns: TRUE if successful
=========================================================================== */
static BOOL PertAddEqn(void) {

	PERT_EQN *eqn, *iv;

	eqn = malloc(sizeof(PERT_EQN));
	eqn->next       = NULL;
	*eqn->eqn       = '\0';
	eqn->adr.type   = INVALID;
	eqn->adr.layer  = NULL;
	eqn->adr.x      = NULL;
	*eqn->adr.desc  = '\0';
	*eqn->adr.brief = '\0';

	if (! PertGetVariable(&eqn->adr)) {free(eqn); return(FALSE);}
	if (! LexGetTokenP(eqn->eqn, sizeof(eqn->eqn), "Expression to set (abort): "))
		{free(eqn); return(FALSE);}

	for (iv=pert_eqns; iv!=NULL; iv=iv->next) {	/* Search if just modify	*/
		if (iv->adr.type != eqn->adr.type) {		/* Differ because of type	*/
			continue;
		} else if (iv->adr.type == LAYER_VAR) {	/* Different var addresses	*/
			if (iv->adr.x != eqn->adr.x) continue;
		} else if (iv->adr.type == VARIABLE) {		/* Different var addresses	*/
			if (stricmp(iv->adr.desc, eqn->adr.desc) != 0) continue;
		}
		iv->adr = eqn->adr;								/* Copy new values in		*/
		strncpy(iv->eqn, eqn->eqn, sizeof(iv->eqn));
		free(eqn);
		return(TRUE);
	}

/* It doesn't already exist, so add - choose to add at end instead of front */
	if ( (iv = pert_eqns) == NULL) {					/* Will it be the first one */
		pert_eqns = eqn;
	} else {													/* Go until at end of list	 */
		while (iv->next != NULL) iv = iv->next;
		iv->next = eqn;
	}

/* If LAYER variable, lock the layer so cannot be deleted from under us */
	if (eqn->adr.type == LAYER_VAR) eqn->adr.layer->locked++;

	return(TRUE);
}


/* ===========================================================================
-- Routine to generally query either a parameter that is to be directly
-- varied by PERT, or where the result of an equation evaluated during PERT
-- is to be placed.
--
-- Usage: BOOL PertGetVariable(PERT_ADR *adr);
--
-- Inputs: adr - address of block ready for assigment of a "vary" parameter
--
-- Output: filled in adr block
--
-- Returns: TRUE if successful and valid.
--
-- Note: If variable is a LAYER_VAR type, this routine *does not* increment
--       the lock count.
--
-- Sub-format of the vary command is:
--      thickness   <layer_num>           <low> <high>
--      composition <layer_num> <element> <low> <high>
--      species     <layer_num> <element> <low> <high>
--      equation    <layer_num> <parm #>  <low> <high>
--      variable    <var_name>            <low> <high>
--      energy | mev                      <low> <high>
--      theta                             <low> <high>
--      phi                               <low> <high>
--      psi                               <low> <high>
--      fwhm                              <low> <high>
--      tau                               <low> <high>
--      kev/ch                            <low> <high>
--      kev(0)                            <low> <high>
--      current                           <low> <high>
--      correction                        <low> <high>
=========================================================================== */
typedef enum _OPS2 {
	VA_THICKNESS,			/* Layer thickness					*/
	VA_COMPOSIT,			/* Element composition				*/
	VA_SPECIES,				/* Diffusing specie composition	*/
	VA_EQUATION,			/* Parameter in a diffusion eqn	*/
	VA_STRAGGLE,			/* Straggle parameter				*/
	VA_MULTIPLE,			/* Multiple scatter parameter		*/
	VA_FUZZ,					/* Fuzz layer constant				*/
	VA_VARIABLE,			/* General variable modification	*/
	VA_BUFFER				/* Buffer (MeV, theta, phi, ...)	*/
} OPS2;

#define	GET_LAYER	0x01
#define	GET_ELEM		0x02
#define	GET_PARM		0x04
#define	GET_VAR		0x08

typedef struct _CMTYPE2 {
	const char *command;					/* User command			*/
	int			minlen;					/* # of chars required	*/
	OPS2			rcode;					/* key for processing	*/
	int			opts;						/* Information options	*/
	PERT_TYPE	type;						/* Var type (VA_BUFFER) */
	char		  *desc;						/* Desribe (VA_BUFFER)	*/
} CMTYPE2;

static CMTYPE2 cmlist2[] = {
	{"thickness",	 2, VA_THICKNESS, GET_LAYER,				 INVALID, NULL},
	{"composition", 2, VA_COMPOSIT,	GET_LAYER | GET_ELEM, INVALID, NULL},
	{"species",		 2, VA_SPECIES,	GET_LAYER | GET_ELEM, INVALID, NULL},
	{"equation",	 2, VA_EQUATION,	GET_LAYER | GET_PARM, INVALID, NULL},
	{"straggle",	 5, VA_STRAGGLE,	0,							 INVALID, NULL},
	{"multiple_scatter", 8, VA_MULTIPLE, 0,					 INVALID, NULL},
	{"fuzz",			 4, VA_FUZZ,		GET_LAYER,				 INVALID, NULL},
	{"variable",	 3, VA_VARIABLE,	GET_VAR,					 INVALID, NULL},
	{"mev",			 3, VA_BUFFER,		0,							 MEV,		"Incident energy (MeV)"},
	{"phi",			 3, VA_BUFFER,		0,							 PHI,		"Scattering angle"},
	{"theta",		 5, VA_BUFFER,		0,							 THETA,	"Sample tilt theta"},
	{"psi",			 3, VA_BUFFER,		0,							 PSI,		"General angle psi"},
	{"fwhm",			 4, VA_BUFFER,		0,							 FWHM,	"Detector FWHM (keV)"},
	{"tau",			 3, VA_BUFFER,		0,							 TAU,		"MCA time constant (us)"},
	{"kev/ch",		 4, VA_BUFFER,		0,							 KEVCH,	"MCA energy/chan (keV)"},
	{"kev(0)",		 4, VA_BUFFER,		0,							 KEV0,	"MCA chan 0 energy (keV)"},
	{"current",		 7, VA_BUFFER,		0,							 CURRENT,"Beam current (nA)"},
	{"correction",	 4, VA_BUFFER,		0,							 CORR,	"Correction factor"},

	{NULL,			 0, VA_BUFFER,		0,							 INVALID, NULL}
};

static BOOL PertGetVariable(PERT_ADR *adr) {

	SAMPLE *sample=SimDefaultSample;
	LAYER *layer=NULL;
	REAL  *adrptr=NULL;
	int elx=0;
	int eqn_parm=0;

	int i, type;
	char token[DFLT_STR_SIZE], varname[DFLT_STR_SIZE];
	CMTYPE2 *citem;

	while (TRUE) {
		if (! LexGetTokenP(token, sizeof(token), "Parameter to vary: (abort|list) ")) {
			return(FALSE);
		} else if (LexEqual(token, "help", 1) || LexEqual(token, "list", 1) ||
			        strcmp(token, "?") == 0) {
			LexCmdlPrint(cmlist2, sizeof(*cmlist2), "Parameters to vary or set");
			continue;
		} else if ((citem = LexCmdl(token, cmlist2, sizeof(*cmlist2))) != NULL) {
			break;
		} else {
			ERRprintf("ERROR: %s cannot be varied.  Limit your choice to:\n", token);
			LexCmdlPrint(cmlist2, sizeof(*cmlist2), NULL);
			return(FALSE);
		}
	}

/* Do I need a layer number? */
	if (citem->opts & GET_LAYER) {
		i = LexGetInt(1, "Layer number (1): ");
		layer = sample->first;			/* Start with first layer */
		while (--i && layer != NULL) layer = layer->next;
		if (layer == NULL) {
			ERRprintf("ERROR: Specified layer is beyond end of the sample\n");
			return(FALSE);
		}
	}

/* How about an element from SIM? */
	if (citem->opts & GET_ELEM) {
		if (! LexGetTokenP(token, sizeof(token), "Element name (abort): ")) {
			return(FALSE);
		} else if ( (elx = SimLocateName(token,FALSE)) == ELEM_INVALID) {
			ERRprintf("ERROR: %s in not in the sample structure\n", token);
			return(FALSE);
		}
	}

/* How about parameter from equation of a layer? */
	if (citem->opts & GET_PARM) {
		if (layer == NULL || layer->eqn == NULL) {
			ERRprintf("ERROR: Specified layer does not have an associated diffusion equation\n");
			return(FALSE);
		}
		eqn_parm = LexGetInt(1, "Parameter of equation (1=first): ");
		if (eqn_parm > layer->eqn->npar) {
			ERRprintf("ERROR: Equation on specified layer only has %d parameters\n",
				layer->eqn->npar);
			return(FALSE);
		}
	}

/* How about a general variable? */
	if (citem->opts & GET_VAR) {
		if (! LexGetTokenP(varname, sizeof(varname), "Variable name (abort): ")) {
			return(FALSE);
		} else if (! GVGetAdrInfo(varname, &type, (void **) &adrptr, NULL)) {
			ERRprintf("ERROR: %s does not exist yet.  Use SETVAR to set first.\n", token);
			return(FALSE);
		} else if (type != GV_REAL && type != GV_REAL_LINK) {
			ERRprintf("ERROR: %s is not a simple real variable\n", token);
			return(FALSE);
		}
		if (type == GV_REAL_LINK) adrptr = *((REAL **) adrptr);
	}


	switch (citem->rcode) {
		case VA_FUZZ:
			if (layer->fuzzs <= 0) {
				ERRprintf("ERROR: You haven't set up any fuzzing on this layer.  Try again\n");
				return(FALSE);
			}
			adr->type  = LAYER_VAR;
			adr->layer = layer;
			adr->x     = &layer->fuzzd;
			strcpy(adr->desc, "Fuzz of layer %d");
			strcpy(adr->brief, "fuzzing");
			strcpy(adr->write, "fuzzing %d");
			break;
		case VA_THICKNESS:
			adr->type  = LAYER_VAR;
			adr->layer = layer;
			adr->x     = &layer->thick.magn;
			strcpy(adr->desc, "Thickness of layer %d");
			strcpy(adr->brief, "thickness");
			strcpy(adr->write, "thickness %d");
			break;
		case VA_COMPOSIT:
			adr->type = LAYER_VAR;
			adr->layer = layer;
			adr->x    = &layer->matrix[elx];
			sprintf(adr->desc, "Layer %%d, %s fraction", sample->elem_names[elx]);
			sprintf(adr->brief, "comp %s", sample->elem_names[elx]);
			sprintf(adr->write, "composition %%d %s", sample->elem_names[elx]);
			break;
		case VA_SPECIES:
			adr->type = LAYER_VAR;
			adr->layer = layer;
			adr->x    = &layer->species[elx];
			sprintf(adr->desc, "Layer %%d, %s diffusant", sample->elem_names[elx]);
			sprintf(adr->brief, "spec %s", sample->elem_names[elx]);
			sprintf(adr->write, "species %%d %s", sample->elem_names[elx]);
			break;
		case VA_EQUATION:
			adr->type = LAYER_VAR;
			adr->layer = layer;
			adr->x    = &layer->par[eqn_parm-1];
			sprintf(adr->desc, "Layer %%d, parameter %d of diffusion eqn", eqn_parm);
			sprintf(adr->brief, "eqnpar %d", eqn_parm);
			sprintf(adr->write, "equation %%d %d", eqn_parm);
			break;
		case VA_STRAGGLE:
			adr->type = STRAGGLE;
			adr->x    = &sample->straggle;
			strcpy(adr->desc, "Straggle constant");
			strcpy(adr->brief, "straggle");
			strcpy(adr->write, "straggle");
			break;
		case VA_MULTIPLE:
			adr->type = MULTIPLE;
			adr->x    = &sample->multiple;
			strcpy(adr->desc, "Multiple scattering");
			strcpy(adr->brief, "multiple");
			strcpy(adr->write, "Multiple_Scatter");
			break;
		case VA_VARIABLE:
			adr->type = VARIABLE;
			adr->x    = adrptr;
			strscpy(adr->desc,  varname, sizeof(adr->desc));
			strscpy(adr->brief, varname, sizeof(adr->brief));
			sprintf(adr->write, "variable %s", varname);
			break;
		case VA_BUFFER:
			adr->type = citem->type;
			strscpy(adr->desc, citem->desc, sizeof(adr->desc));
			strscpy(adr->brief, citem->command, sizeof(adr->brief));
			strscpy(adr->write, citem->command, sizeof(adr->write));
			break;
		default:
			ERRprintf("ERROR: Call the developers and give them some abuse\n");
			return(FALSE);
	}
	return(TRUE);
}



/* ===========================================================================
-- Routine to list the help for PERT commands
--
-- Usage: PertHelpList(void);
--
-- Inputs: none
--
-- Output: screen only
--
-- Returns: nothing
=========================================================================== */
static void PertHelpList(void) {

	TTYprintf(" PERT subcommands are:\n"
				 "    ?                          quick list of valid commands\n"
				 "    HELP                       types this listing\n"
             "    RESET                      free all vars and set to default\n"
				 "    PARMS | STATUS             current parameters and settings\n"
             "    SAVE                       write pert parameters to file\n"
             "    RETURN                     exit and return to RUMP\n"
             "    WINDOW [-ADD] [-NOCURSOR]  set error window (or multiple ones)\n"
				 "    NORMALIZE [-NOCURSOR]      set normalization window\n"
             "    VARY {...}                 specify parameter to be varied\n"
				 "       thickness   <layer>           <min> <max>\n"
				 "       composition <layer> <element> <min> <max>\n"
				 "       species     <layer> <element> <min> <max>\n"
				 "       equation    <layer> <parm #>  <min> <max>\n"
				 "       straggle                      <min> <max>\n"
				 "       multiple_scatter              <min> <max>\n"
				 "       fuzz        <layer>           <min> <max>\n"
				 "       variable    <varname>         <min> <max>\n"
				 "       mev  | phi | theta  | psi     <min> <max>\n"
				 "       fwhm | tau | kev/ch | kev(0)  <min> <max>\n"
				 "    EXPREssion {...}           set a parameter via a general expression\n"
				 "       thickness <layer> <general_expression>\n"
				 "       ... similar to vary above ...\n"
				 "    FREE <num>                 free parameter or expression n from list\n"
				 "    FREEALL                    free all parameters & expressions\n"
				 "    GO | FIT                   Starts variable fit analysis\n"
				 "\n"
				 " COMPARE, SHOW, ACTIVE perform their expected function.\n"
             " Commands MEV, THETA, etc. aliased to VARY MEV ...\n"
				 "\n"
		);
	return;
}


/* ===========================================================================
-- Routine to write the PERT structure to a file which can subsequently be
-- gotten - similar to SIM GET/SAVE pairs.
--
-- Usage: PertWriteParms();
--
-- Inputs: none
--
-- Output: file or screen
--
-- Returns: success
=========================================================================== */
static BOOL PertWriteParms(void) {

/*  -- Local variables -- */
	char file[PATH_MAX];
	FILE *lun;

	char token[DFLT_STR_SIZE];
	PERT_ERR *win;
	PERT_VAR	*var;
	PERT_EQN *eqn;

/*  -- Code begin -- */
	if (! LexGetFileP(file, sizeof(file), "Pert (.pert) output filename (console): ")) {
		lun = stdout;
	} else {
		SysAddExt(file, ".pert");
		if ( (lun = fopen(file, "w")) == NULL) {
			ERRprintf("ERROR: %s failed to open for writing\n", file);
			return(FALSE);
		}
	}

/* Reset the world */
	fputs("Pert Reset\n", lun);
	fprintf(lun, " Method %s\n", (PertMethod == DOOLITTLE) ? "Doolittle" : "Thompson");
	fprintf(lun, " Volume %d\n", PertVolume);
	fprintf(lun, " /* Internal %g %d %g      /* Internal use values\n", EpsCrit, MaxIterations, DerivFraction);
	fputs("\n", lun);

/* Set the normalization window */
	if (pert_norm.mode == SET) fprintf(lun, " Normalize %d %d\n", pert_norm.low, pert_norm.high);

/* Set the error windows */
	for (win=pert_err; win->low>=0; win++) {
		fprintf(lun, " Window%s %d %d\n", (win==pert_err) ? "" : " -add",
			win->low, win->high);
	}

/* Set the varying parameters */
	for (var=pert_vars; var!=NULL; var=var->next) {
		if (var->adr.type == LAYER_VAR) {			/* Needs the layer number */
			sprintf(token, var->adr.write, SimGetLayerNum(var->adr.layer));
		} else {
			strscpy(token, var->adr.write, sizeof(token));
		}
		fprintf(lun, " Vary %s %g %g\n", token, var->min, var->max);
	}

	if (pert_eqns != NULL) {
		for (eqn=pert_eqns; eqn!=NULL; eqn=eqn->next) {
			if (eqn->adr.type == LAYER_VAR) {			/* Needs the layer number */
				sprintf(token, eqn->adr.write, SimGetLayerNum(eqn->adr.layer));
			} else {
				strscpy(token, eqn->adr.write, sizeof(token));
			}
		fprintf(lun, " Expression %s \"%s\"\n", token, eqn->eqn);
		}
	}

/*  .. Shut off the file */
	if (lun != stdout) fclose(lun);

	return(TRUE);
}


/* ===========================================================================
-- Routine to list the PERT structure, all varying parameters and all
-- windows.
--
-- Usage: PertStatus();
--
-- Inputs: none
--
-- Output: screen only
--
-- Returns: nothing
--
--  ----------------------------------------------------------------------
--  Var  Min value    Max value    Description
--  ----------------------------------------------------------------------
--    1  1234567890   1234567890   abcdefghijklmnopqrstuvwxyzn
--    2  1234567890   1234567890   abcdefghijklmnopqrstuvwxyz
--  -----------------------------------------------------------------------
--
=========================================================================== */
void PertStatus(void) {

	char token[DFLT_STR_SIZE];
	int i;
	PERT_ERR *win;
	PERT_VAR	*var;
	PERT_EQN *eqn;
	REAL value;

	PertSetAdrs();					/* Make sure all are updated now */

	TTYputs("\n");

	for (i=1,win=pert_err; win->low>=0; i++,win++)
		TTYprintf(" Error window (%1d):     %4d %4d\n", i, win->low, win->high);

	if (pert_norm.mode == SET) {
		TTYprintf(" Normalization window: %4d %4d\n", pert_norm.low, pert_norm.high);
	} else if (pert_norm.mode != VARY) {
		TTYprintf(" No normalization window defined.  Correction factor must be exact.\n");
	}
	TTYprintf(" %s's code used for fitting.  Volume level=%d\n",
			  (PertMethod == DOOLITTLE) ? "Doolittle" : "Thompson", PertVolume);

	TTYprintf("\n"
		" ----------------------------------------------------------------------\n"
		" Var   Min val    Max val    Current  Description\n"
/*    " 01 12345678.0 12345678.0 12345678.0  Layer 3, composition of Ni\n" */
		" ----------------------------------------------------------------------\n"
		);
	for (i=1,var=pert_vars; var!=NULL; i++,var=var->next) {
		if (var->adr.type == LAYER_VAR) {
			sprintf(token, var->adr.desc, SimGetLayerNum(var->adr.layer));
		} else if (var->adr.type == VARIABLE) {
			sprintf(token, "General var: %s", var->adr.desc);
		} else {
			strscpy(token, var->adr.desc, sizeof(token));
		}
		value = (var->adr.x == NULL) ? 0.0f : *var->adr.x ;
		TTYprintf(" %2d %10.4g %10.4g %10.4g  %s\n",	i, var->min, var->max, value, token);
	}

	if (pert_eqns != NULL) {

		TTYprintf("\n"
			" ----------------------------------------------------------------------\n"
			" Eqn     Value  Description            Expression\n"
/*			" 07 12345678.0  abcdefghijklmnopqrstuv abcdefghijklmnopqrstuv\n" */
			" ----------------------------------------------------------------------\n"
			);
		for (eqn=pert_eqns; eqn!=NULL; i++,eqn=eqn->next) {
			if (eqn->adr.type == LAYER_VAR) {
				sprintf(token, eqn->adr.desc, SimGetLayerNum(eqn->adr.layer));
			} else if (eqn->adr.type == VARIABLE) {
				sprintf(token, "Var: %s", eqn->adr.desc);
			} else {
				strscpy(token, eqn->adr.desc, sizeof(token));
			}
			value = (eqn->adr.x == NULL) ? 0.0f : *eqn->adr.x ;
			TTYprintf(" %2d %10.4g  %-22s %-22s\n", i, value, token, eqn->eqn);
		}
	}
	TTYputs("\n");

	return;
}


/* ===========================================================================
-- Routine to release all varying parameters in PERT - called implicitely by
-- SIM in a SIM RESET to avoid LOCK problem.
--
-- Usage: int PertFreeAll(void);
--
-- Inputs: none
--
-- Output: Frees all variables that may be in use
--
-- Returns: Number of "LAYER" variables released
=========================================================================== */
int PertFreeAll(void) {
	
	PERT_VAR *var;
	PERT_EQN *eqn;
	int rcode=0;

	while (pert_vars != NULL) {		/* Free all varying parameters */
		var = pert_vars;
		pert_vars = var->next; 
		if (var->adr.type == LAYER_VAR) {var->adr.layer->locked--; rcode++;}
		free(var);
	}

	while (pert_eqns != NULL) {		/* Free all equations listed */
		eqn = pert_eqns;
		pert_eqns = eqn->next; 
		if (var->adr.type == LAYER_VAR) {var->adr.layer->locked--; rcode++;}
		free(var);
	}
	return(rcode);
}

/* ===========================================================================
-- Resets most of Pert's internal structures, freeing all parameters
--
-- Usage: PertReset(int key);
--
-- Input: key - U_UNIT ==> initialize, otherwise just reset
--
-- Output: Frees all variables, resets normalization and error windows
--
-- Returns: <none>
=========================================================================== */
void PertReset(int key) {

	int i;

	if (key == U_INIT) {
		pert_vars = NULL;
		pert_eqns = NULL;
	}

	PertFreeAll();							/* Free all var's and eqn's */

	for (i=0; i<=NUM_ERR_WINS; i++) pert_err[i].low = -1;	/* Turn off */

	pert_norm.mode = UNSET;
	pert_norm.low  = 100;
	pert_norm.high = 200;

	return;
}


/* ===========================================================================
-- Fully qualifies all of the variables in the block to be modified.  Searches
-- the necessary databases to get physical addresses, sets them in place, and
-- returns ready for a listing or to run a fit.
--
-- Usage:  BOOL PertSetAdrs(void);
--
-- Inputs: none
--
-- Output: Sets all addresses in pert_vars[] and pert_eqns[] as necessary
--
-- Returns: TRUE if successful
=========================================================================== */
static BOOL PertSetAdrs(void) {

	PERT_VAR *var;
	PERT_EQN *eqn;
	int rcode=TRUE;

	if (ibuf == NULL) return(FALSE);								/* Fatal error */

	for (var=pert_vars; var!=NULL; var=var->next)
		rcode = rcode && set_adr(&var->adr);

	for (eqn=pert_eqns; eqn!=NULL; eqn=eqn->next)
		rcode = rcode && set_adr(&eqn->adr);
	
	return(rcode);
}


/* ===========================================================================
-- Routine to look up the actual address corresponding to structure *adr
=========================================================================== */
static BOOL set_adr(PERT_ADR *adr) {

	REAL *adrptr=NULL;
	int type;

	switch (adr->type) {
		case STRAGGLE:
		case MULTIPLE:
		case FUZZ:
		case LAYER_VAR:
			break;
		case INVALID:
			ERRprintf("ERROR: Invalid pert variable detected\n");
			break;
		case VARIABLE:						/* These are not a problem */
			if (! GVGetAdrInfo(adr->desc, &type, (void **) &adrptr, NULL) ||
				(type != GV_REAL && type != GV_REAL_LINK) ) {
				ERRprintf("ERROR: \"%s\" no longer valid variable.  It's not nice to fool PERT!\n", adr->desc);
				adrptr = NULL;
			} else if (type == GV_REAL_LINK) {
				adrptr = *((REAL **) adrptr);
			}
			adr->x = adrptr;
			break;
		case MEV:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->e0;
			break;
		case THETA:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->theta;
			break;
		case PHI:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->phi;
			break;
		case PSI:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->psi;
			break;
		case FWHM:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->fwhm;
			break;
		case TAU:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->tau;
			break;
		case KEVCH:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->kevch;
			break;
		case KEV0:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->kev0;
			break;
		case CURRENT:
			adr->x = (ibuf == NULL) ? NULL : &ibuf->current;
			break;
		case CORR:
			if (pert_norm.mode == SET) 
				ERRprintf("ERROR: No normalization window can be set when correction factor varied.\n"
                      "       The existing window has been deleted\n");
			pert_norm.mode = VARY;							/* Can't be also on */
			adr->x = (ibuf == NULL) ? NULL : &ibuf->corr;
			break;
		default:
			TTYprintf("ERROR: Tell developers to stay awake at night\n");
			return(FALSE);
	}
	return(TRUE);
}


/* ===========================================================================
-- Routine to build up the NLS_DATA structure and execute the appropriate
-- fitting procedure.
--
-- Usage: BOOL PertFit(BOOL silent)
--
-- Inputs: none
--
-- Output: Builds structure, modifies parameters as necessary
--
-- Returns: TRUE --> Success
=========================================================================== */
static BOOL PertFit(BOOL silent) {

	NLS_DATA nls;						/* Structure passed to fitting routines */
	int i,j, jmin,jmax, nvars, npt, rcode;
	PERT_ERR *win;
	PERT_VAR *iv;
	BOOL noise_hold;
	SAMPLE *sample=SimDefaultSample;

/* Check number of points and parameters - is it tolerably valid? */
	for (nvars=0,iv=pert_vars; iv!=NULL; iv=iv->next) nvars++;
	if ( (npt = ibuf->npt) == 0) {
		ERRprintf("ERROR: Better get a spectrum with some data in it\n");
		return(FALSE);
	} else if (nvars == 0) {
		ERRprintf("ERROR: Try again - at least one vary parameters must be specified for varying\n");
		return(FALSE);
	} else if (pert_err[0].low < 0) {
		ERRprintf("ERROR: You must specify at least one error window\n");
		return(FALSE);
	}
	
/* Update all addresses in the vary blocks */
	PertSetAdrs();
	noise_hold = sample->noise;
	sample->noise = FALSE;										/* Can't be on for PERT */

/* Build up a NLSFIT parameter block */
	memset(&nls, 0, sizeof(nls));								/* Zero out first		*/
	nls.data      = ibuf->counts;								/* Experimental data */
	nls.errorbar  = malloc(npt*sizeof(*nls.errorbar));	/* Must fill this in */
	nls.valid     = malloc(npt*sizeof(*nls.valid));		/* Used for windows	*/
	nls.npt       = npt;											/* # of data points	*/
	nls.xy        = NULL;										/* Not needed by me	*/
	nls.nvars     = nvars;										/* # of parameters	*/
	nls.vars      = malloc(nvars*sizeof(*nls.vars));	/* parameter address	*/
	nls.lower	  = malloc(nvars*sizeof(*nls.lower));	/* lower bounds		*/
	nls.upper	  = malloc(nvars*sizeof(*nls.upper));	/* lower bounds		*/
	nls.sigma     = malloc(nvars*sizeof(*nls.sigma));	/* Rtn'd sigma est	*/
	nls.correlate = calloc(nvars*nvars, sizeof(*nls.correlate));
	nls.chisqr    = 0;											/* Irrelevent			*/
	nls.chiold    = 0;											/* Irrelevent			*/
	nls.EpsCrit   = EpsCrit;										/* Stopping criteria	*/
	nls.flamda    = 0;											/* Let program set	*/
	nls.yfit      = ALTBUF->counts;							/* Fit data buffer	*/
	nls.outchi    = NULL;
	nls.workspace = NULL;										/* Allocated by pgm	*/

	nls.evalfnc   = PertEvalSim;
	nls.fderiv    = PertEvalDeriv;
#ifdef USE_GAUSSIAN_STATISTICS
	nls.evalchi   = EvalChiGauss;
#else
	nls.evalchi   = EvalChiPoisson;
#endif
/*   nls.evalhes   = PertEvalHess; */

/* Convert local varying parameter data structure to curfit version			*/
	for (i=0,iv=pert_vars; i<nvars; i++,iv=iv->next) {	/* Fill in tables		*/
		nls.vars[i]  = iv->adr.x;
		nls.lower[i] = iv->min;
		nls.upper[i] = iv->max;
	}

/* ---------------------------------------------------------------------------
-- Set the appropriate error bars on data points.  The error_bar for points
-- not within the error windows of interest will be left zero, and hence
-- ignored by fitting routines and chi-square calculating routines.
--
-- Change: 3/17/97 - new VALID structure with NLSFIT - use instead
--------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		nls.errorbar[i] = (REAL) sqrt(max(1,ibuf->counts[i]));
		nls.valid[i]    = FALSE;
	}
	for (i=0; i<NUM_ERR_WINS; i++) {
		win = &pert_err[i];
		if (win->low < 0) break;
		jmin = (int) (win->low  - ibuf->first);				/* Actual index */
		jmax = (int) (win->high - ibuf->first);
		jmin = min(ibuf->npt-2, max(jmin, 0));
		jmax = min(ibuf->npt-1, max(jmax, 0));
		for (j=jmin; j<=jmax; j++)	nls.valid[j] = TRUE;	/* Inclusive on boundaries */
	}

/* ---------------------------------------------------------------------------
-- Finally, deal with the normalization problem.  Convert the low/high channel
-- specification in PERT_NORM to actual data array indices.  Sum the data array
-- so calculating normalization factor is easy later.
--------------------------------------------------------------------------- */
	if (pert_norm.mode == SET) {
		pert_norm.imin = (int) (pert_norm.low  - ibuf->first);		/* Actual index */
		pert_norm.imax = (int) (pert_norm.high - ibuf->first);
		pert_norm.imin = min(ibuf->npt-2, max(pert_norm.imin, 0));
		pert_norm.imax = min(ibuf->npt-1, max(pert_norm.imax, 0));
		pert_norm.sum  = 0;
		for (i=pert_norm.imin; i<=pert_norm.imax; i++) pert_norm.sum += ibuf->counts[i];
	}

/* Finally, go ahead and fit it */
	SimSilent = TRUE;
	rcode = PertFitByMethod(PertMethod, &nls, silent ? -1 : PertVolume);
	SimSilent = FALSE;

	free(nls.errorbar);								/* Free allocated workspaces	*/
	free(nls.valid);
	free(nls.vars);
	free(nls.lower);
	free(nls.upper);
	free(nls.sigma);
	free(nls.correlate);
	free(nls.workspace);
	free(nls.outchi);

	sample->noise = noise_hold;
	Rmp->autsim = -1;									/* Force a redo next time */

	return(rcode == 0);
}


/* ===========================================================================
-- Main procedure to take the setup in NLS_DATA structure, and handle the
-- fitting.  Will use the GENPLOT CURFIT() routine with appropriate routines
-- for the evaluation of parameters.
--
-- Usage:  int PertFitByMethod(NLS_DATA *nls)
--
-- Inputs: nls - NLS_DATA structure describing the fit problem
--
-- Output: modifies parameters in nls, and outputs to screen
--
-- Returns: 0 if successful, otherwise error code.
=========================================================================== */
static int PertFitByMethod(METHOD method, NLS_DATA *nls, int volume) {

	int (*FitRoutine)(int key, int iter, NLS_DATA *nlsme);
	int iter, maxiter, printiter;

	int i,j,k, rcode, key;
	char token[DFLT_STR_SIZE];
	PERT_VAR *iv;
	ARRAY	**aptr=NULL;								/* For linking CF$ and SIGMA$ */
	REAL		*a;										/* Random ptr */

	if (method == THOMPSON) {						/* method = CURVEFIT */
		FitRoutine = CurveFit;
		maxiter    = MaxIterations;
		printiter  = -1;
	} else {
		FitRoutine = LocateMin;
		maxiter    = MaxIterations + nls->nvars;
		printiter  = nls->nvars;
	}

/* Initialize everything else in CurveFit routine */
	if ((rcode=(*FitRoutine)(0, 0, nls)) != 0) goto FitExit;

/* Header for the running info */
	if (volume > -1) {
	TTYputs("------------------------------------------------------------------------------\n");
	strcpy(token, "    CHISQR ");
	for (iv=pert_vars; iv!=NULL;) {
		TTYputs(token);
		for (j=0; j<6 && iv!=NULL; j++) {
			TTYprintf("%11s", iv->adr.brief);
			iv = iv->next;
		}
		TTYputs("\n");
		strcpy(token, "           ");
	}
	TTYputs("------------------------------------------------------------------------------\n");
	}

/* And we are off and running */
	key = -1;												/* Set to +1 for noise */
	rcode = 0;
	for (iter=0; iter<maxiter; iter++) {			/* Number of reps allowed */
		if (iter > printiter && volume > -1) {
			TTYprintf("\r%11.4g", nls->chisqr);
			for (j=0; j<nls->nvars; ) {
				for (k=0; k<6 && j<nls->nvars; k++) TTYprintf("%11.4g", *nls->vars[j++]);
				TTYputc('\n');
				if (j != nls->nvars) TTYputs("           ");
			}
			TTYflush();
		}
		if (nls->chiold <= 0 || rcode == 1) break;	/* Basically success! */
		if (SysChkBreak(TRUE)) {
			rcode = -5;
			goto FitExit;
		}
		if ((rcode=(*FitRoutine)(key,iter,nls)) < 0) goto FitExit;		/* Run again */
	}
	if (rcode == 0 && iter >= maxiter) rcode = 2;	/* Run out of time? */

	if (volume > -1) {
		TTYputs( "\n"
			"    Variable                         Value:              Sigma\n"
			"    --------                         ------              -----\n");
/*	    "    1234567890123456789012345 12345.1234567     123456.1234567 */
		for (iv=pert_vars,i=0; i<nls->nvars; i++,iv=iv->next) {
			if (iv->adr.type == LAYER_VAR) {
				sprintf(token, iv->adr.desc, SimGetLayerNum(iv->adr.layer));
			} else if (iv->adr.type == VARIABLE) {
				sprintf(token, "General var: %s", iv->adr.desc);
			} else {
				strscpy(token, iv->adr.desc, sizeof(token));
			}
			TTYprintf("    %-25s %13.7g     %14.7g\n", token, *nls->vars[i], nls->sigma[i]);
		}
		if (pert_norm.mode == SET) {
			TTYprintf("\nEstimated correction factor set for buffer: %g\n\n", LastCorrFactor);
			ibuf->corr = LastCorrFactor;
		}
	}

/* Finally, link the results to the function evaluator */
	if (GVAllocArray("CF$", GVF_USER, nls->nvars)) {			/* Link results	*/
		GVGetInfo("CF$", NULL, (void **) &aptr);
		a = (*aptr)->x;												/* Get address		*/
		for (i=0; i<nls->nvars; i++) a[i] = *nls->vars[i];
	}
	if (GVAllocArray("SIGMA$", GVF_USER, nls->nvars)) {	/* Link SIGMA		*/
		GVGetInfo("SIGMA$", NULL, (void **) &aptr);
		a = (*aptr)->x;												/* Get address		*/
		for (i=0; i<nls->nvars; i++) a[i] = nls->sigma[i];
	}
	if (nls->correlate != NULL) {
		if (GVAllocArray("CORRELATE$", GVF_USER, nls->nvars*nls->nvars)) {
			GVGetInfo("CORRELATE$", NULL, (void **) &aptr);
			a = (*aptr)->x;												/* Get address		*/
			for (i=0; i<nls->nvars*nls->nvars; i++) a[i] = nls->correlate[i];
		}
	}

	GVAllocReal("chisqr$",   GVF_USER, nls->chisqr);				/* chi^2 if valid */
	GVAllocReal("variance$", GVF_USER, nls->chisqr*nls->dof);	/* Variance			*/
	GVAllocInt ("dof$",      GVF_USER, nls->dof);					/* Degrees of freedom */

/* ----------------------- */
FitExit:
/* ----------------------- */
	switch (rcode) {
		case -1:
			ERRputs("      GET OFF THE QUAALUDES, MAN!\n"
			     "ERROR: Too many parameters for number of data points\n");
			break;
		case -2:
			gen_err("Error simulating the spectrum (NLSFIT)");
			break;
		case -3:
			gen_err("Unable to allocate temporary matrix space (NLSFIT)");
			break;
		case -4:
			gen_err("Unable to allocate work spaces.  (NLSFIT)");
			break;
		case -5:
			gen_err("*** Fit aborted by user pressing ^C (NLSFIT) ***");
			break;
		case -6:
								/* Silent error: FitRoutine already made noise */
			break;
		case 1:
			break;			/* Success! */
		case 2:
			ERRputs("WARNING: Maximum iteration count reached.  A better fit may be obtained\n"
					  "         by running fit again starting from these parameters\n");
			break;
		default:
			if (rcode < 0) gen_err("Function evaluator errors.  (NLSFIT)");
	}

	if (rcode == 1) rcode = 0;				/* Clear the termination condition */
	if (rcode == 2) rcode = 0;				/* And don't give error on warning */
	return(rcode);
}


/* ============================================================================
-- PertEvalEqns - Evaluates all eqns in stack
--
-- Usage: int PertEvalEqns(void)
--
-- Inputs: pert_eqns[] structures
--
-- Output: sets internal parameters
--
-- Returns: nothing
--
-- This routine must be called before each evaluation to set equations into
-- place as desired by the user.
============================================================================ */
static void PertEvalEqns(void) {

	PERT_EQN *eqn;
	int ierr;

	for (eqn=pert_eqns; eqn!=NULL; eqn=eqn->next) {
		*eqn->adr.x = GVTrimToReal(GVEvalExpr(eqn->eqn, &ierr));
		if (ierr != 0) ERRprintf("ERROR: Expression evaluation failed (%s)\n", eqn->eqn);
	}
	return;
}


/* ============================================================================
-- PertEvalSim - Does the simulation, filling in ALTBUF
--
-- Usage: int PertEvalSim(NLS_DATA *nls)
--
-- Inputs: see structure information
--
-- Output: nls->yfit - Curve containing the fit
--
-- Returns: 0 if successful, error code otherwise
--
-- This should be simple, all I have to do is start SIM.  But, have to also
-- do the normalization here so CURFIT and LOCMIN have clean data to compare.
============================================================================ */
PRIVATE int PertEvalSim(NLS_DATA *nls) {

	int i;
	REAL corr = 0;

	PertEvalEqns();								/* Possible equations */
	ALTBUF->e0 = 0;
	if (SimCreateDetails(NULL, ELEM_INVALID, LAYER_INVALID) == NULL) return(-2);

/* Do we need to handle normalization? */
	if (pert_norm.mode == SET) {
		corr = 0;										/* Calculate sum over window	*/
		for (i=pert_norm.imin; i<=pert_norm.imax; i++) corr += nls->yfit[i];
	
		corr = pert_norm.sum / corr;				/* Factor data must be scaled */
		LastCorrFactor = ibuf->corr / corr;		/* And tell global world val	*/

		for (i=0; i<nls->npt; i++) nls->yfit[i] *= corr;	/* Finally, correct fit data	*/
	}

	return(0);
}


/* ============================================================================
-- ... Subroutine to determine the derivatives with respect to each of the
-- ... varied parameters.
--
-- Usage: int = PertEvalDeriv(REAL *results, NLS_DATA *nls, int ipt);
--
-- Inputs: nls - data structure describing the fit parameters and data
--         ipt - Point # at which to evaluate derivatives
--
-- Output: result - vector filled in with derivative for each varying parm
--
-- Returns: 0 if successful, error code otherwise.
-- ========================================================================== */
static int PertEvalDeriv(REAL *results, NLS_DATA *nls, int ipt) {

	int i,j,npt,nvars;
	REAL delta, *base, *fptr, *yfit;

	static REAL *data=NULL;									/* Stores derivatives */

	npt   = nls->npt;											/* Need # of pnts	*/
	nvars = nls->nvars;										/* And # of vars	*/

	if (ipt == 0) {											/* First point		*/
		free(data);												/* Free old data	*/
		data = malloc(npt * nvars * sizeof(*data));
		base = malloc(npt * sizeof(*base));
		yfit = nls->yfit;										/* And data location */

		for (i=0; i<npt; i++) base[i] = yfit[i];		/* Copy base result	*/

		for (fptr=data,i=0; i<nvars; i++,fptr+=npt) {
			delta = (nls->upper[i]-nls->lower[i]) * DerivFraction;	/* Delta value		*/
			if (*nls->vars[i]+delta > nls->upper[i]) delta = -delta;
			*nls->vars[i] += delta;
			PertEvalSim(nls);
			*nls->vars[i] -= delta;
			for (j=0; j<npt; j++) fptr[j] = (yfit[j]-base[j])/delta;
		}
		free(base);
	}

/* Now, just return the values */
	for (i=0; i<nvars; i++) results[i] = data[i*npt+ipt];
	return(0);
}
