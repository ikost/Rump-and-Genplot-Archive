/* NLSFIT.C */

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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define GV_MATH_EXTENSIONS					/* Need ndtri & q_chi */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "genplot.h"
#include "gptxtrn.h"

#include "curfit.h"
#include "nlsfit.h"
#include "helper.h"

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
static	int		do_fit(CURVE *cv);
static	int		func_deriv(REAL *results, NLS_DATA *nls, int ipt);
static	int		func_eval(NLS_DATA *nls);
static	int		prog_deriv(REAL *results, NLS_DATA *nls, int ipt);
static	int		prog_eval(NLS_DATA *nls);
static	int		SetExterns(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
typedef enum _OPS1 {
	NLS_HELP,		NLS_QUIT,		NLS_FUNCTION,	NLS_FIT,			NLS_VARY,
	NLS_REMOVE,		NLS_STATUS,		NLS_RESET,		NLS_ACCURACY,	NLS_MAXIT,
	NLS_MODES,		NLS_WEIGHTS,	NLS_SIGMA,		NLS_VERBOSE,	NLS_EXTERNAL,
	NLS_RUMPMACRO,	NLS_GENPLOTMACRO,
	NLS_XRANGE,    NLS_YRANGE,    NLS_ZRANGE,		NLS_XYRANGE,	NLS_XYZRANGE,
	NLS_CURSORRANGE,
	NLS_DELTAFRAC,	NLS_DELTAZERO,	NLS_TRY,			NLS_IGNORE,		NLS_METHOD,
	NLS_ERRORMATRIX
} OPS1;

typedef struct _CMTYPE {
	const char *name;
	int  minlen;
	OPS1 rcode;
} CMTYPE;

static const CMTYPE cmlist[] = {
									{"HELP",			 2,	NLS_HELP},
									{"?",				 1,	NLS_HELP},
									{"RETURN",		 3,	NLS_QUIT},
									{"QUIT",			 2,	NLS_QUIT},
									{"GENPLOT",		-3,	NLS_QUIT},
									{"FUNCTION",	 3,	NLS_FUNCTION},
									{"EQUATION",	 2,	NLS_FUNCTION},
									{"MAIN",			-4,	NLS_FUNCTION},
									{"EXT_MACRO",   3,	NLS_EXTERNAL},
									{"RUMP_MACRO",	 6,	NLS_RUMPMACRO},
									{"GENPLOT_MACRO",8,	NLS_GENPLOTMACRO},
									{"EXTERNAL",   -4,	NLS_EXTERNAL},
									{"WEIGHTS",		 6,	NLS_WEIGHTS},
									{"SIGMA",		 5,	NLS_SIGMA},
									{"FIT",			 3,	NLS_FIT},
									{"TRY",			 3,	NLS_TRY},
									{"VARY",			 4,	NLS_VARY},
									{"REMOVE",		 3,	NLS_REMOVE},
									{"UNVARY",		 -3,	NLS_REMOVE},
									{"RELEASE",		 -3,	NLS_REMOVE},
									{"STATUS",		 4,	NLS_STATUS},
									{"INFORMATION",-3,	NLS_STATUS}, 
									{"RESET",		 5,	NLS_RESET},
									{"ERROR_MATRIX",5,	NLS_ERRORMATRIX},
									{"RANGE",      -3,   NLS_XRANGE},
									{"XRANGE",      4,   NLS_XRANGE},
									{"YRANGE",      4,   NLS_YRANGE},
									{"ZRANGE",		 4,   NLS_ZRANGE},
									{"XYRANGE",     5,   NLS_XYRANGE},
									{"XYZRANGE",    6,   NLS_XYZRANGE},
									{"CURSOR_RANGE",8,   NLS_CURSORRANGE},
									{"ACCURACY",	 3,	NLS_ACCURACY},
									{"MAX_ITERATE", 5,	NLS_MAXIT},
									{"MAXITERATE",	-5,	NLS_MAXIT},
									{"DELTAFRAC",	 5,	NLS_DELTAFRAC},
									{"DELTAZERO",	 6,	NLS_DELTAZERO},
									{"MODES",		 5,	NLS_MODES},
									{"METHOD",		 4,	NLS_METHOD},
									{"ALGORITHM",	-5,	NLS_METHOD},
									{"VERBOSITY",	 4,	NLS_VERBOSE},
									{"VERBOSE",		-7,	NLS_VERBOSE},
									{"NLSFIT",     -3,	NLS_IGNORE},
									{NULL,			 0,	NLS_IGNORE} };

/* --------------------------------------------------
... Character definitions of equation and derivatives
... 0 element of deriva is really the weighting function
 -------------------------------------------------- */
#define	VAR_SPECIAL_LINK	-1			/* vars.type that signifies link  */

static	NLSDATA *ns=NULL;				/* Local copy (shorter name!)			*/
			NLSDATA *GptNLSData=NULL;	/* These should always be identical	*/

/* ===========================================================================
-- Routine to allocate or reset a ns structure from GENPLOT
=========================================================================== */
NLSDATA *GptNLSAllocData(NLSDATA *nns) {

	if (nns == NULL) {
		nns = malloc(sizeof(NLSDATA));
		if (nns == NULL) return(NULL);
		nns->vars = NULL;
		nns->NumAllocated   = 0;
	}

	nns->eqn_mode      = EQUATION;					/* Must be in program mode */
	nns->verbose       = RUNNING;
	nns->weighting     = USE_NONE;
	nns->method        = CURVEFIT;
	nns->NumVars       = 0;								/* No parms yet */
   nns->TryAllocation = TRUE;
	nns->MaxIterations  = 10;
	nns->EpsCrit        = 1E-04f;
	nns->DeltaFrac      = 1E-03f;
	nns->DeltaZero      = 1E-03f;

	nns->xlow  = nns->ylow  = nns->zlow  = -REAL_MAX;	/* Data range unlimited */
	nns->xhigh = nns->yhigh = nns->zhigh =  REAL_MAX;

	*nns->equation      = '\0';						/* No equation				*/
	*nns->ExternMacro   = '\0';						/* No macro					*/
	nns->MacroType      = SIMPLE;						/* And simple if there	*/
	*nns->weight_eqn    = '\0';						/* No weighting			*/
	nns->result_file    = NULL;

	return(nns);
}


/* ===========================================================================
--
=========================================================================== */
NLSVAR *GptNLSAddVar(char *token, int *ierr) {

	NLSVAR *var;
	void *adrptr;								/* Adress pointer from GVGetInfo */
	int idx,type;

/* Locate it's information and addresses */
	if (! GVGetAdrInfo(token, &type, &adrptr, NULL)) {
		if (ierr != NULL) *ierr = 1;
		return(NULL);
	} else if (type != GV_REAL && type != GV_REAL_LINK && type != GV_COMPLEX && type != GV_COMPLEX_LINK) {
		if (ierr != NULL) *ierr = 2;
		return(NULL);
	}
	if (type == GV_REAL_LINK) {
		adrptr = *((REAL **) adrptr);
		type = GV_REAL;
	} else if (type == GV_COMPLEX_LINK) {
		adrptr = *((COMPLEX **) adrptr);
		type = GV_COMPLEX;
	}

/* Decide if modifying an existing variable, otherwise allocate space */
	if ( (idx = GptNLSFindVar(token)) >= 0 && (ns->vars[idx].type != type) ) {
		GptNLSRemoveVar(token); 
		idx = -1;
	}
	if (idx < 0) {
		if (ns->NumVars+1 >= ns->NumAllocated) {	/* Make sure space for COMPLEX */
			ns->NumAllocated += 10;						/* Add in increments of 10 */
			ns->vars = realloc(ns->vars,ns->NumAllocated*sizeof(*ns->vars));
		}
		idx = ns->NumVars;
		ns->NumVars += (type == GV_COMPLEX) ? 2 : 1 ;
	}
	
/* ... Add variable to the end of our list */
	var = &ns->vars[idx];						/* Get pointer to var itself	*/

	strcpy(var->name, token);					/* Put in the name		*/
	var->type = type;								/* Variable type			*/
	var->ptr  = adrptr;							/* Address of variable	*/
	var->delta = -1;								/* Disable					*/
	var->lower = -REAL_MAX;						/* Full range				*/
	var->upper = +REAL_MAX;						/* Full range				*/
	strcpy(var->dbyda, "(numeric)");

	if (type == GV_COMPLEX) {							/* Duplicate for COMPLEX	*/
		ns->vars[idx+1] = ns->vars[idx];				/* Copy exact					*/
		strcat(ns->vars[idx+1].name,"(imag)");
		ns->vars[idx+1].type = VAR_SPECIAL_LINK;	/* LINKED variable */
		ns->vars[idx+1].ptr++;
	}

	if (ierr != NULL) *ierr = 0;
	return(var);
}

/* ============================================================================
-- Subroutine to determine value of arbitrary coefficients in an arbitrary
-- equation describing the data.
--
-- Usage:  LOGICAL = NLSFIT$(x,y,npt)
--
-- Inputs: x,y,npt - curve
--
-- Output: Calculates values of user coefficients for actual function.
============================================================================ */
int nlsfit(CURVE *cv) {

	char		SaveCurveName[VARNAME_STR_SIZE];		/* To save this curve name */
	char		token[LONG_STR_SIZE];
	int		i, type, ierr, rcode, CheckLimits;
	CMTYPE	*citem;
	NLSVAR	*var;
	REAL		tmp;

/* ....................................................................... */
	if (GptNLSData == NULL) GptNLSData = GptNLSAllocData(GptNLSData);
	ns = GptNLSData;
	strcpy(SaveCurveName, GptUseCurve);					/* Save this puppy */
	
	while (TRUE) {

		LexEscape(TRUE);											/* Clear <ESC> condition */
		while (! LexGetTokenP(token, sizeof(token), "NLSFIT: ")) continue;
		if (LexEscape(TRUE)) break;							/* Exit out now	*/
		if (LexAlias(token, sizeof(token))) continue;	/* Check for alias */

		citem = LexCmdl(token, cmlist, sizeof(CMTYPE));
		if (citem == NULL) {								/* Try something different */
			if ( (rcode = GptMainCommands(0, token)) != NOTKNOWN) {
				if (rcode == UNIMPLEMENTED || rcode == NOPLOTTER)
					ERRprintf("ERROR: Something wrong at GENPLOT level with \"%s\" command\n", token);
				if (rcode != OKAY) LexFlush();
				GptLinkXYZ(SaveCurveName);				/* Be sure to reset!!! */
			} else if (! PlotSystem(0, token, 0) && ! LexSystem(0, token)) {
				gen_err2("Unknown NLSFIT command", token);
				LexFlush();
			}
			continue;
		} else switch (citem->rcode) {
			case NLS_HELP:									/* List out available commands */
				LexCmdlPrintEx(cmlist, sizeof(CMTYPE), 13, "NLSFIT commands:");
				TTYputs("\nAll GENPLOT and SYSTEM commands are also visible at this level\n\n");
				break;

			case NLS_QUIT:									/* Abort, return to caller */
				for (i=0; i<ns->NumAllocated; i++) {
					strcpy(token,"$DV00"); 
					token[3] = (char)(i/10+'0');	token[4] = (char)(i%10+'0');
					GVDeallocate(token);
				}
				return(0);

			case NLS_RUMPMACRO:
			case NLS_GENPLOTMACRO:
			case NLS_EXTERNAL:
				if (! LexGetFileP(token, sizeof(token), 
					"External macro to run after arg changes (disable): ")) {
					*ns->ExternMacro = '\0';
				} else if (! SysFindFile(ns->ExternMacro, token, LexMacroSearchPath, 
												 LexMacroExtList, R_OK)) {
					*ns->ExternMacro = '\0';
					ERRprintf("ERROR: %s cannot be found\n", token);
					LexFlush();
					continue;
				}
				if (citem->rcode == NLS_EXTERNAL) {
					ns->MacroType = SIMPLE;
				} else if (citem->rcode == NLS_GENPLOTMACRO) {
					ns->MacroType = GENPLOT;
				} else {
					if (GptRumpLink == NULL) {
						ERRprintf("ERROR: RUMP has not registered it's address - is it running?\n");
						*ns->ExternMacro = '\0';
						LexFlush();
						continue;
					}
					ns->MacroType = RUMP;
				}
				break;

			case NLS_FUNCTION:							/* Specify the equation */
				if (! LexGetMathP(ns->equation, sizeof(ns->equation), "Equation name: ")) break;
				if (stricmp(ns->equation, "PROGRAM") == 0) {
					if (! LexGetTokenP(ns->equation, sizeof(ns->equation),
						"Full command w/ args: ")) break;
					ns->eqn_mode = PROGRAM;
				} else if (stricmp(ns->equation, "PIPE") == 0) {
					if (! LexGetTokenP(ns->equation, sizeof(ns->equation), "Full pipe name: ")) break;
					ns->eqn_mode = PIPE;
				} else {
					if (GVGetInfo(ns->equation, &type, NULL) && (type == GV_FUNCTION))
						strcat(ns->equation, (cv->z == NULL) ? "(x)" : "(x,y)");
					ns->eqn_mode = EQUATION;
				}
				break;
				
			case NLS_VARY:									/* Add a parameter to list to use */
				if (! LexGetTokenP(token, sizeof(token), "Parameter name: ")) break;

/* ... Locate it's information and addresses */
				var = GptNLSAddVar(token, &ierr);
				if (ierr == 1) {
					ERRprintf("Specified variable %s is not currently defined\n",token);
					LexFlush();
					break;
				} else if (ierr == 2) {
					ERRprintf("Specified variable %s is not of real or complex type\n",token);
					LexFlush();
					break;
				}

/* ... Get information on how to handle derivative df/da */
				if (LexGetTokenP(var->dbyda, sizeof(var->dbyda), "Analytic derivative fnc (none): ")) {
					if (*var->dbyda == '/') {
						strcpy(var->dbyda, "(numeric)");
					} else if (var->type == GV_COMPLEX) {
						ERRprintf("WARNING: I understand your ignorance and will forget the derivative spec\n");
						strcpy(var->dbyda, "(numeric)");
					} else if (GVGetInfo(var->dbyda, &type, NULL) && (type == GV_FUNCTION)) {
						strcat(var->dbyda, "(x)");
					}
				} else {
					strcpy(var->dbyda, "(numeric)");
				}

/* ... Check other options ... */
				while (LexGetOption(token, sizeof(token))) {
					CheckLimits = 0;								/* No limits set this time */

					if (LexEqual(token, "-RANGE", 3) || LexEqual(token, "-LIMITS", 4)) {
						i = 1;
					} else if (LexEqual(token, "-LOWER", 4) || LexEqual(token, "-FROM", 3)) {
						i = 2;
					} else if (LexEqual(token, "-UPPER", 3) || LexEqual(token, "-TO", 3)) {
						i = 3;
					} else if (LexEqual(token, "-DELTA", 2)) {
						i = 4;
					} else {
						ERRprintf("ERROR: %s - unrecognized option\n", token);
						continue;
					}
					if (var->type != GV_COMPLEX) {
						if (i == 4) {
							var->delta = LexGetReal(-1.0, "Absolute delta for numerical differential (automatic): ");
						} else {
							if (i == 1 || i == 2) var->lower = LexGetReal(-REAL_MAX, "Lower limit: ");
							if (i == 1 || i == 3) var->upper = LexGetReal(+REAL_MAX, "Upper limit: ");
							CheckLimits = 1;
						}
					} else {
						TMPCOMPLEX a;
						if (i == 4) {
							if (LexGetTokenP(token, sizeof(token), "Absolute delta for numerical differential (automatic): ")) {
								a = GVEvalComplexExpr(token, &ierr);
								var[0].delta = (REAL) a.x;	var[1].delta = (REAL) a.y;
							}
						} else {
							CheckLimits = 2;
							if ( (i == 1 || i == 2) && LexGetTokenP(token, sizeof(token), "Lower limit: ")) {
								a = GVEvalComplexExpr(token, &ierr);
								var[0].lower = (REAL) a.x;	var[1].lower = (REAL) a.y;
							}
							if ( (i == 1 || i == 3) && LexGetTokenP(token, sizeof(token), "Upper limit: ")) {
								a = GVEvalComplexExpr(token, &ierr);
								var[0].upper = (REAL) a.x;	var[1].upper = (REAL) a.y;
							}
						}
					}

					for (i=0; i<CheckLimits; i++) {				/* Verify limits in right order */
						if (var[i].lower > var[i].upper) {
							ERRprintf("WARNING: Upper limit < lower limit.  I'll forgive your foolishness this time.\n");
							tmp = var[i].lower;
							var[i].lower = var[i].upper;
							var[i].upper = tmp;
						}
					}

				} /* while */

				if (ns->verbose >= INFO) TTYprintf("Number of parameters now: %i\n", ns->NumVars);
				break;

			case NLS_REMOVE:
				if (! LexGetTokenP(token, sizeof(token), "Variable to remove: ")) break;
				if (GptNLSRemoveVar(token) != 0) {
					gen_err2("Specified variable was not found to remove", token);
					LexFlush();
				}
				break;
				
/* -- Status listing --
NLSFIT: status

   Equation:  f(x)
   Weighting: 1/sqrt(y)
   Epsilon:     0.0001   Max_Iter:  10   Vector mode: True
   Range limit: %g < [X] < %g         %g < [Y] < %g

 #  Variable Name      Value      dF/dv derivative   lower limit  upper limit
 ------------------------------------------------------------------------------
 0  amp              208.92        (numeric)        -3.4028e+38   3.4028e+38
 1  center           55.982        (numeric)        -3.4028e+38   3.4028e+38
 2  fwhm             3.5776        (numeric)        -3.4028e+38   3.4028e+38
 3  offset           188.24        d4(x)            -3.4028e+38   3.4028e+38
 4  slope            -2.3874       d5(x)            -3.4028e+38   3.4028e+38
 5  quad             0.010365      d6(x)            -3.4028e+38   3.4028e+38
 ------------------------------------------------------------------------------
*/
			case NLS_STATUS:								/* List out information */
				TTYputs("\n");
				if (ns->eqn_mode == EQUATION) {
					TTYprintf("   Equation:  %s\n", ns->equation);
				} else if (ns->eqn_mode == PROGRAM) {
					TTYprintf("   Command as equation:  %s\n", ns->equation);
				} else if (ns->eqn_mode == PIPE) {
					TTYprintf("   Pipe as equation:  %s\n", ns->equation);
				}
				if (*ns->ExternMacro != '\0') {
					if (ns->MacroType == SIMPLE) {
						TTYprintf("   External: %s\n", ns->ExternMacro);
					} else if (ns->MacroType == RUMP) {
						TTYprintf("   External RUMP macro: %s\n", ns->ExternMacro);
					} else if (ns->MacroType == GENPLOT) {
						TTYprintf("   External GENPLOT macro: %s\n", ns->ExternMacro);
					}
				}
				if (ns->weighting == USE_NONE) {
					TTYputs("   Weighting: (disabled)\n");
				} else if (ns->weighting == USE_SIGMA) {
					TTYprintf("   Sigma:     %s\n", ns->weight_eqn);
				} else if (ns->weighting == USE_WEIGHT) {
					TTYprintf("   Weighting: %s\n", ns->weight_eqn);
				}
				TTYprintf("   Epsilon:%11.2g   Max_Iter:%4i   Vector mode: %s\n",
					ns->EpsCrit, ns->MaxIterations, (ns->TryAllocation ? "True" : "False"));
				TTYprintf("   Numerical diff'n defaults.  DeltaFrac: %g  DeltaZero: %g\n",
					ns->DeltaFrac, ns->DeltaZero);
				TTYprintf("   Using %s algorithm (method)\n",
					(ns->method == CURVEFIT) ? "CURVEFIT" : "LOCMIN");
				if (ns->xlow != -REAL_MAX || ns->xhigh != REAL_MAX)
					TTYprintf("   X Range limits: %.4g < X < %.4g\n", ns->xlow, ns->xhigh);
				if (ns->ylow != -REAL_MAX || ns->yhigh != REAL_MAX)
					TTYprintf("   Y Range limits: %.4g < Y < %.4g\n", ns->ylow, ns->yhigh);
				if (ns->zlow != -REAL_MAX || ns->zhigh != REAL_MAX)
					TTYprintf("   Z Range limits: %.4g < Z < %.4g\n", ns->zlow, ns->zhigh);
				TTYputs("\n"
					" #  Variable Name    Value    dF/dv derivative   delta   lower lim   upper lim \n"
					" ------------------------------------------------------------------------------\n");
				for (i=0; i<ns->NumVars; i++) {
					TTYprintf(" %-2d %-14s %-13.5g %-14s %7g %-13.5g%-13.5g\n", i,
					ns->vars[i].name, *ns->vars[i].ptr, ns->vars[i].dbyda, ns->vars[i].delta, ns->vars[i].lower, ns->vars[i].upper);
				}
				TTYputs(
					" ------------------------------------------------------------------------------\n"
					"\n");
				break;
				
			case NLS_ERRORMATRIX:
			{
				ARRAY		**aptr=NULL;
				REAL		*a;									/* Random ptr */
				int i,j;
				if (! GVGetInfo("CORRELATE$", NULL, (void **) &aptr)) {
					ERRprintf("Duh? Haven't done a fit yet - how can there be a correlation matrix?");
				} else {
					a = (*aptr)->x;												/* Get address		*/
					TTYprintf(" Error correlation matrix (CORRELATE$)\n");
					for (i=0; i<ns->NumVars; i++) {
						for (j=0; j<ns->NumVars; j++) {
							TTYprintf("  %7.3f", a[i*ns->NumVars+j]);
						}
						TTYputs("\n");
					}
				}
			}
				break;

			case NLS_RESET:						/* Reset all parameters to original */
				ns = GptNLSData = GptNLSAllocData(GptNLSData);
				break;
					
			case NLS_DELTAZERO:
				ns->DeltaZero = LexGetReal(1E-3f, "Delta used for numerical differential if exactly zero (1E-3): ");
				if (ns->DeltaZero <= 0) ns->DeltaZero = 1E-3f;
				break;

			case NLS_DELTAFRAC:
				ns->DeltaFrac = LexGetReal(1E-3f, "Fraction of value to use as delta in numerical differentiation (1E-3): ");
				if (ns->DeltaFrac <= 1E-7) ns->DeltaFrac = 1E-3f;
				break;

			case NLS_ACCURACY:						/* ... ACCURACY */
				ns->EpsCrit = LexGetReal(1E-04f,"Fractional change criteria for stop: (1E-4) ");
				if (ns->EpsCrit < 1E-07f) {
					gen_warn("Minimum fractional change allowed = 1E-7");
					ns->EpsCrit = 1E-7f;
				}
				break;
				
			case NLS_MAXIT:						/* Max_iterations */
				ns->MaxIterations = LexGetInt(10, "Maximum number of iterations (10): ");
				break;
				
			case NLS_MODES:						/* Special modes */
				ns->TryAllocation = LexYesNo(TRUE,"Attempt allocation for deriv's (Y)? ");
				break;
				
			case NLS_METHOD:
				if (! LexGetTokenP(token, sizeof(token), "Algorithm for fit (curvefit | locmin | ABORT): "))
					break;
				i = LexSelect(token, "curvefit curfit thompson locmin locatemin doolittle abort");
				if (i <= 0) {
					ERRprintf("ERROR: Unrecognized NLSFIT method (%s)\n", token);
					LexFlush();
				} else if (i == 1 || i == 2 || i == 3) {
					ns->method = CURVEFIT;
				} else if (i == 4 || i == 5 || i == 6) {
					ns->method = LOCMIN;
				}
				break;

			case NLS_XRANGE:
				ns->xlow  = LexGetReal(ns->xlow,  "Minimum X allowed: ");
				ns->xhigh = LexGetReal(ns->xhigh, "Maximum X allowed: ");
				OrderPair(&ns->xlow, &ns->xhigh);
				break;

			case NLS_YRANGE:
				ns->ylow  = LexGetReal(ns->ylow,  "Minimum Y allowed: ");
				ns->yhigh = LexGetReal(ns->yhigh, "Maximum Y allowed: ");
				OrderPair(&ns->ylow, &ns->yhigh);
				break;

			case NLS_ZRANGE:
				ns->zlow  = LexGetReal(ns->zlow,  "Minimum Z allowed: ");
				ns->zhigh = LexGetReal(ns->zhigh, "Maximum Z allowed: ");
				OrderPair(&ns->zlow, &ns->zhigh);
				break;

			case NLS_XYRANGE:
				ns->xlow  = LexGetReal(ns->xlow,  "Minimum X allowed: ");
				ns->xhigh = LexGetReal(ns->xhigh, "Maximum X allowed: ");
				ns->ylow  = LexGetReal(ns->ylow,  "Minimum Y allowed: ");
				ns->yhigh = LexGetReal(ns->yhigh, "Maximum Y allowed: ");
				OrderPair(&ns->xlow, &ns->xhigh);
				OrderPair(&ns->ylow, &ns->yhigh);
				break;

			case NLS_XYZRANGE:
				ns->xlow  = LexGetReal(ns->xlow,  "Minimum X allowed: ");
				ns->xhigh = LexGetReal(ns->xhigh, "Maximum X allowed: ");
				ns->ylow  = LexGetReal(ns->ylow,  "Minimum Y allowed: ");
				ns->yhigh = LexGetReal(ns->yhigh, "Maximum Y allowed: ");
				ns->zlow  = LexGetReal(ns->zlow,  "Minimum Z allowed: ");
				ns->zhigh = LexGetReal(ns->zhigh, "Maximum Z allowed: ");
				OrderPair(&ns->xlow, &ns->xhigh);
				OrderPair(&ns->ylow, &ns->yhigh);
				OrderPair(&ns->zlow, &ns->zhigh);
				break;

			case NLS_CURSORRANGE:
				GptSetRange();										/* Set range */
				TTYputs("Define allowed window with cursor\n");	/* -CURSOR */
				TTYflush();
				PlotBoxCursor(&ns->xlow, &ns->ylow, &ns->xhigh, &ns->yhigh, NULL);
				gpt_SetBoxCursorCoords(ns->xlow, ns->ylow, ns->xhigh, ns->yhigh);
				OrderPair(&ns->xlow, &ns->xhigh);
				OrderPair(&ns->ylow, &ns->yhigh);
				break;
				
			case NLS_WEIGHTS:						/* Weighting function */
				ns->weighting = USE_NONE;
				if (LexGetMathP(ns->weight_eqn, sizeof(ns->weight_eqn), "Weighting expression (disable): "))
					ns->weighting = USE_WEIGHT;
				break;
				
			case NLS_SIGMA:						/* Specify SIGMA values for data set */
				ns->weighting = USE_NONE;
				if (LexGetMathP(ns->weight_eqn, sizeof(ns->weight_eqn), "Sigma expression for Y (disable): ")) 
					ns->weighting = USE_SIGMA;
				break;
				
			case NLS_VERBOSE:						/* Set level of noise while fitting */
				if (LexGetTokenP(token, sizeof(token),
					"Verbosity level [0=none, 1=result, 2=info, 3=run, 4=debug] (no change): ")) {
					i = LexSelect(token, "0 NONE 1 RESULT 2 INFO 3 RUNNING 4 DEBUG");
					if (i <= 0) {
						ERRprintf("ERROR: Unrecognized verbosity level %s\n");
						LexFlush();
					} else {
						ns->verbose = (VERBOSE) ((i-1)/2);
					}
				}
				break;

			case NLS_IGNORE:						/* No-op */
				break;
				
			case NLS_FIT:							/* Start a fit now */
				if (do_fit(cv) < 0) LexFlush();	/* Erase rest of buffer */
				break;

			case NLS_TRY:							/* Do the fit setup now */
				i = ns->MaxIterations;
				ns->MaxIterations = 0;
				if (do_fit(cv) < 0) LexFlush();	/* Erase rest of buffer */
				ns->MaxIterations = i;
				break;
				
			default:
				gen_err2("Unexpected but recognized NLSFIT command", token);
				LexFlush();
		}
	}
	return(0);											/* Happens if press <ESC> */
}

/* ===========================================================================
-- Routine to remove a variable from the currently active list.  It does a
-- simple case-insensitive name comparison.  If it matches, and the name is
-- not marked as a "special link variable", then it is deleted along with any
-- associated "special links".
--
-- Usage: int = GptNLSRemoveVar(char *token);
--
-- Inputs: token - variable to be removed
--
-- Returns: 0 --> successful removal
--          1 --> variable not found
--          2 --> variable was a special link
=========================================================================== */
int GptNLSRemoveVar(char *token) {
	int i,j,idelta;
	for (i=0; i<ns->NumVars; i++) {
		if (stricmp(token, ns->vars[i].name) == 0) break;
	}
	if (i == ns->NumVars) return(1);								/* Not found */
	if (ns->vars[i].type == VAR_SPECIAL_LINK) return(2);	/* Subsidary var */
	idelta = (ns->vars[i].type == GV_COMPLEX) ? 2 : 1 ;
	ns->NumVars -= idelta;
	for (j=i; j<ns->NumVars; j++) ns->vars[j] = ns->vars[j+idelta];
	return(0);
}

/* ===========================================================================
-- Routine to find a variable in the currently active list.  It does a
-- simple case-insensitive name comparison.  If it matches, and the name is
-- not marked as a "special link variable", then the index is returned.
-- Otherwise return -1.
--
-- Usage: int = GptNLSFindVar(char *token);
--
-- Inputs: token - variable to be removed
--
-- Returns: 0 --> successful removal
--          1 --> variable not found
--          2 --> variable was a special link
=========================================================================== */
int GptNLSFindVar(char *token) {
	int i;
	for (i=0; i<ns->NumVars; i++) {
		if (stricmp(token, ns->vars[i].name) == 0) break;
	}
	if (i == ns->NumVars) return(-1);							/* Not found */
	if (ns->vars[i].type == VAR_SPECIAL_LINK) return(-1);	/* Subsidary var */
	return(i);
}


/* ---------------------------------------------------------------------------
   Returns:  0  ==> all successful.     Met epsilon criteria for exiting.
             1  ==> mostly successful.  Ran into maximum iterations limit.
            -10 ==> major screwup.      Something totally invalid in structure
				-1  ==> too few points for a fit.
				-2  ==> error evaluation function.
				-3  ==> error allocating memory.
				-4  ==> error allocating memory.
            -5  ==> User aborted by ^C
--------------------------------------------------------------------------- */
static char *fit_eqn="unknown(x)";			/* Should we use fit(x) or fit(x,y) */
static BOOL in_3D_mode;							/* Are we doing 3D? */

static int do_fit(CURVE *cv) {

/* Local variables */
	NLS_DATA	nls;									/* Structure passed to NLSFIT	*/
	int (*FitRoutine)(int key, int iter, NLS_DATA *nls_me);
	int		i, j, k, key, npt;				/* Random integer constants	*/
	int		iter, maxiter, printiter;		/* How many iterations			*/
	char		token[LONG_STR_SIZE];
	int		rcode=0;
	REAL		*xy[3];								/* Array for the dependent vars */
	ARRAY		**aptr=NULL;
	REAL		*a;									/* Random ptr */
	CURVE		*curve=NULL;
	BOOL		DefineFit;							/* Should we define the FIT$ curve */
	int		itype;

	double	quality;								/* True chi-square and Q factor */

	memset(&nls, sizeof(nls), 0);				/* Blank it out completely */
	
/* First, a few sanity checks */
	if (ns->NumVars <= 0) {
		gen_err("You must specify at least one parameter to vary");
		return(-10);
	}

/* Deal with the various solution algorithms */
	if (ns->method == LOCMIN) {
		FitRoutine = LocateMin;
		maxiter   = ns->MaxIterations + ns->NumVars;		/* Need nvars to start */
		printiter = ns->NumVars;
		for (i=0; i<ns->NumVars; i++) {
			if (ns->vars[i].lower == -REAL_MAX || ns->vars[i].upper == REAL_MAX) {
				ERRprintf("ERROR: When using LOCMIN method, all variables must have lower and upper\n"
							 "       limits set.  Use \"VARY <var> / -LIMIT <low_limit> <upper_limit>\"\n");
				return(-10);
			}
		}
	} else if (ns->method == CURVEFIT) {
		FitRoutine = CurveFit;
		maxiter   = ns->MaxIterations;						/* Exact # of iters	*/
		printiter = -1;
	} else {
		ERRprintf("Unexpected error: ns->method invalid value (%d)\n", ns->method);
		return(-10);
	}

/* Deal with the various modes, equation, program or pipe */
	if (ns->eqn_mode == EQUATION && cv->z == NULL) {					/* Equation mode in 2D */
		if (stricmp(ns->equation, "fit(x)") == 0) {
			if (GVChkParse(ns->equation, NULL) == NULL) {
				ERRprintf("ERROR: Specified equation (%s) failed to parse\n", ns->equation);
				return(-10);
			}
		} else if (! GVAllocFnc("fit(x)", GVF_USER, ns->equation)) {
			ERRprintf("ERROR: Sorry Charlie, equation (%s) not well enough defined\n", ns->equation);
			return(-10);
		}
		fit_eqn = "fit(x)";
		in_3D_mode = FALSE;
	} else if (ns->eqn_mode == EQUATION && cv->z != NULL) {			/* Equation mode in 3D */
		if (stricmp(ns->equation, "fit(x,y)") == 0) {
			if (GVChkParse(ns->equation, NULL) == NULL) {
				ERRprintf("ERROR: Specified equation (%s) failed to parse\n", ns->equation);
				return(-10);
			}
		} else if (! GVAllocFnc("fit(x,y)", GVF_USER, ns->equation)) {
			ERRprintf("ERROR: Sorry Charlie, 3D equation (%s) not well enough defined\n", ns->equation);
			return(-10);
		}
		fit_eqn = "fit(x,y)";
		in_3D_mode = TRUE;

	} else if (ns->eqn_mode == PROGRAM) {
		FILE *funit;
		if ( (ns->result_file = SysTmpFilename(NULL, ".dat")) == NULL) {
			ERRprintf("ERROR: PROGRAM mode failed - unable to create temporary file");
			return(-10);
		} else if ( (funit = fopen(ns->result_file, "w")) == NULL) {
			ERRprintf("ERROR: PROGRAM mode failed - unable to open temporary file");
			return(-10);
		} 
		for (i=0; i<cv->npt; i++) {
			if (cv->z != NULL) {
				fprintf(funit, "%.8g %.8g %.8g\n", cv->x[i], cv->y[i], cv->z[i]);
			} else {
				fprintf(funit, "%.8g %.8g\n", cv->x[i], cv->y[i]);
			}
		}
		fclose(funit);
	} else if (ns->eqn_mode == PIPE) {
		gen_err("Pipe mode for NLSFIT is not implemented yet - Sorry");
		return(-10);
	}

/* Now, allocate arrays for derivatives determined by the function evaluator */
	npt = cv->npt;

	for (i=0; i<ns->NumVars; i++) {
		if ( (stricmp(ns->vars[i].dbyda, "(numeric)") != 0) && (GVChkParse(ns->vars[i].dbyda, NULL) == NULL) ) {
			gen_err2("Derivative failed to parse", ns->vars[i].dbyda);
			remove(ns->result_file); free(ns->result_file); ns->result_file = NULL;
			return(-10);
		} 
		if (ns->TryAllocation || (ns->eqn_mode == PROGRAM)) {		/* Vector mode? */
			strcpy(token,"$DV00");
			token[3] = (char)(i/10+'0');	token[4] = (char)(i%10+'0');
			if (GVAllocArray(token, GVF_USER, npt)) {					/* Allocate an array */
				GVGetInfo(token, NULL, (void **) &aptr);
				ns->vars[i].derivptr = (*aptr)->x;
			} else if (ns->eqn_mode == PROGRAM) {
				ERRprintf("ERROR: PROGRAM mode failed - unable to create derivative arrays");
				remove(ns->result_file); free(ns->result_file); ns->result_file = NULL;
				return(-10);
			} else {
				ns->vars[i].derivptr = NULL;
			}
		} else {
			ns->vars[i].derivptr = NULL;
		}
	}
				
/* Initialize the independent variable addresses */
	xy[0]         = cv->x;
	xy[1]         = cv->y;
	xy[2]         = cv->z;

/* Initialize the data structure to nlsfit() now */
	nls.errorbar  = NULL;				/* No error bars							*/
	nls.yfit      = NULL;				/* Let fit routines allocate space	*/
	nls.outchi    = NULL;				/* Let fit allocate space if needed	*/
	nls.correlate = NULL;				/* No correlation matrix wanted		*/
	nls.workspace = NULL;				/* Let fit allocate space if needed	*/
	nls.magic_cookie = 0;				

	nls.data     = (cv->z == NULL) ? cv->y : cv->z;
	nls.npt      = cv->npt;
	nls.valid    = NULL;					/* May be revised below					*/
	nls.xy       = xy;
	nls.nvars    = ns->NumVars;

	nls.flamda   = 0;						/* Let CurveFit() set initial value	*/
	nls.EpsCrit  = ns->EpsCrit;		/* CurveFit() now does completion test */

	if (ns->eqn_mode == EQUATION) {
		nls.evalfnc = func_eval;		/* Functions to evaluate function	*/
		nls.fderiv  = func_deriv;		/* Functions to evaluate derivative	*/
	} else if (ns->eqn_mode == PROGRAM) {
		nls.evalfnc = prog_eval;
		nls.fderiv  = prog_deriv;
	}
	nls.evalchi   = NULL;				/* Use default chisqr evaluation		*/

 	nls.vars      = calloc(ns->NumVars,	sizeof(*nls.vars));
	nls.sigma     = calloc(ns->NumVars,	sizeof(*nls.sigma));
	nls.correlate = calloc(ns->NumVars*ns->NumVars, sizeof(*nls.correlate));
	nls.lower     = calloc(ns->NumVars,	sizeof(*nls.lower));
	nls.upper     = calloc(ns->NumVars,	sizeof(*nls.upper));
	if (nls.vars==NULL  || nls.sigma==NULL || nls.correlate==NULL ||
		 nls.lower==NULL || nls.upper==NULL) {
		rcode = -4;
		goto FitExit;
	}
	for (i=0; i<ns->NumVars; i++) {
		nls.vars[i]  = ns->vars[i].ptr;
		nls.lower[i] = ns->vars[i].lower;
		nls.upper[i] = ns->vars[i].upper;
	}

/* Allocate and evaluate any potential weighting function */
	if (ns->weighting != USE_NONE) {
		nls.errorbar = (REAL *) malloc(npt*sizeof(*nls.errorbar));
		if (nls.errorbar == NULL) {rcode=-4; goto FitExit;}
		if (ns->weighting == USE_SIGMA) {
			strcpy(token, ns->weight_eqn);
		} else {
			strcpy(token,"1/sqrt(abs("); strcat(token, ns->weight_eqn); strcat(token, "))");
		}
		GVEvalArrayExpr(nls.errorbar, npt, token);
	}

/* Check if have to use the VALID data block (limited range) */
	if (ns->xlow != -REAL_MAX || ns->xhigh != REAL_MAX || 
       ns->ylow != -REAL_MAX || ns->yhigh != REAL_MAX ||
		 (cv->z != NULL && (ns->zlow != -REAL_MAX || ns->zhigh != REAL_MAX) ) ) {
		nls.valid = malloc(sizeof(*nls.valid)*npt);
		for (i=0; i<npt; i++) {
			nls.valid[i] = (cv->x[i] >= ns->xlow) && (cv->x[i] <= ns->xhigh) &&
								(cv->y[i] >= ns->ylow) && (cv->y[i] <= ns->yhigh);
			if (cv->z != NULL && nls.valid[i])
				nls.valid[i] = (cv->z[i] >= ns->zlow) && (cv->z[i] <= ns->zhigh);
		}
	}

/* Initialize everything else in CurveFit routine */
	if ((rcode=(*FitRoutine)(NKEY_INIT, 0, &nls)) != 0) goto FitExit;
	if (ns->MaxIterations <= 0) {						/* Just request for trial */
		TTYprintf("Test function yields chi-square: %g\n", nls.chisqr);
		goto LinkStuff;
	}

/* And we are off and running */
	if (ns->verbose >= RUNNING) {						/* What noise level */
		TTYputs("------------------------------------------------------------------------------\n");
		strcpy(token, "    CHISQR ");
		for (i=0; i<ns->NumVars;) {
			TTYputs(token);
			for (j=0; j<6 && i<ns->NumVars; j++) TTYprintf("%11s", ns->vars[i++].name);
			TTYputs("\n");
			strcpy(token, "           ");
		}
		TTYputs("------------------------------------------------------------------------------\n");
	}

/* Set key to be either silent or verbose on fitting */
	key = NKEY_TRY_SILENT;
	if (ns->verbose >= INFO) key = NKEY_TRY_VERBOSE;
	if (ns->verbose == DEBUGALL) key = NKEY_TRY_DEBUG;
	rcode = 0;
	for (iter=0; iter<maxiter; iter++) {			/* Number of reps allowed */
		if (ns->verbose >= RUNNING && iter > printiter) {
			TTYprintf("\r%11.4g", nls.chisqr);
			for (j=0; j<nls.nvars; ) {
				for (k=0; k<6 && j<nls.nvars; k++) TTYprintf("%11.4g", *nls.vars[j++]);
				TTYputc('\n');
				if (j != nls.nvars) TTYputs("           ");
			}
			TTYflush();
		}
		if (nls.chisqr <= 0 || rcode == 1) break;		/* Basically success! */
		if (SysChkBreak(TRUE)) {
			rcode = -5;
			goto FitExit;
		}
		if ((rcode=(*FitRoutine)(key,iter,&nls)) < 0) goto FitExit;		/* Run again */
	}
	if (rcode == 0 && iter >= maxiter) rcode = 2;	/* Run out of time? */

/* Return chi^2 or reduced chi^2, and calculate quality factor (if appropriate) */
	if (ns->weighting == USE_NONE) {
		quality = 0.5;									/* Really meaningless */
	} else {
		quality = q_chi(nls.chisqr*nls.dof, nls.dof);
	}

	if (ns->verbose >= RESULT) {
		TTYputs( "\n"
			"    Variable                Value               Sigma\n"
			"    --------                -----               -----\n");
/*		   "    123456789012345  12345.1234567     123456.1234567 */
		for (i=0; i<nls.nvars; i++) {
			TTYprintf("     %-15s  %13.7g     %14.7g\n", ns->vars[i].name, *nls.vars[i], nls.sigma[i]);
		}
		TTYputs("\n");

		if (ns->weighting == USE_NONE) {
			TTYprintf("     Degrees of Freedom: %d\n", nls.dof);
			TTYprintf("     Root Mean Variance: %g\n", sqrt(nls.chisqr));
			TTYprintf("     Estimated Y sigma:  %g\n", nls.sigmaest);
			TTYputs(  "     WARNING: Error estimates valid only if estimated Y sigma is correct\n");
		} else {
			TTYprintf("     Degrees of Freedom: %d\n", nls.dof);
			TTYprintf("     Reduced chi-square: %f\n", nls.chisqr);
			TTYprintf("     Q(chi-squared):     %f\n", quality);
			if (quality < 0.01 || quality > 0.99)
				TTYprintf("     WARNING: Q out of bounds.  Fit function or Y sigma may be inappropriate.\n");
		}
		TTYputs("\n");
	}

/* Finally, link the results to the function evaluator */
	if (GVAllocArray("CF$",GVF_USER,ns->NumVars)) {			/* Link results	*/
		GVGetInfo("CF$", NULL, (void **) &aptr);
		a = (*aptr)->x;												/* Get address		*/
		for (i=0; i<ns->NumVars; i++) a[i] = *nls.vars[i];
	}
	if (GVAllocArray("SIGMA$",GVF_USER, ns->NumVars)) {	/* Link SIGMA		*/
		GVGetInfo("SIGMA$", NULL, (void **) &aptr);
		a = (*aptr)->x;												/* Get address		*/
		for (i=0; i<ns->NumVars; i++) a[i] = nls.sigma[i];
	}
	if (GVAllocArray("CORRELATE$", GVF_USER, ns->NumVars*ns->NumVars)) {
		GVGetInfo("CORRELATE$", NULL, (void **) &aptr);
		a = (*aptr)->x;												/* Get address		*/
		for (i=0; i<ns->NumVars*ns->NumVars; i++) a[i] = nls.correlate[i];
	}
		
	GVAllocReal("quality$", GVF_USER, (REAL) quality);				/* Quality of fit */
	GVAllocReal("Q$",       GVF_USER, (REAL) quality);

/* ----------------------- */
LinkStuff:
/* ----------------------- */
	GVAllocReal("chisqr$",   GVF_USER, nls.chisqr);				/* chi^2 if valid */
	GVAllocReal("variance$", GVF_USER, nls.chisqr*nls.dof);	/* Variance			*/
	GVAllocInt ("dof$",      GVF_USER, nls.dof);					/* Degrees of freedom */

/* ----------------------- */
FitExit:
/* ----------------------- */
	DefineFit = FALSE;											/* Major problems seen */
	switch (rcode) {
		case -1:
			ERRputs("      GET OFF THE QUAALUDES, MAN!\n"
			     "ERROR: Too many parameters for number of data points\n");
			break;
		case -2:
			gen_err("Unable to properly evaluate function (NLSFIT)");
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
		case -6:				/* Initialization errors - reported by CurveFit() */
			break;
		case 1:
			DefineFit = TRUE;											/* No problems seen */
			break;			/* Success! */
		case 2:
			DefineFit = TRUE;											/* Only minor problems */
			ERRputs("WARNING: Maximum iteration count reached.  A better fit may be obtained\n"
					  "         by running fit again starting from these parameters\n");
			break;
		default:
			if (rcode < 0) gen_err("Function evaluator errors.  (NLSFIT)");
	}

/* Copy the "best" fit values into a new curve called FIT$ */
	if (DefineFit) {
		if (cv->z == NULL) {
			GVAlloc2DCurve("FIT$", GVF_USER, cv->npt);
		} else {
			GVAlloc3DCurve("FIT$", GVF_USER, cv->npt);
		}
		if (GVGetInfo("FIT$", &itype, (void **) &curve)) {
			curve = *((CURVE **) curve);							/* Deal w/ indirection */
			curve->npt = cv->npt;
			strcpy(curve->ids, "Best fit function");
			memcpy(curve->x, cv->x, sizeof(REAL)*cv->npt);
			if (cv->z != NULL) {
				memcpy(curve->y, cv->y, sizeof(REAL)*cv->npt);
				memcpy(curve->z, nls.yfit,    sizeof(REAL)*cv->npt);
			} else {
				memcpy(curve->y, nls.yfit,    sizeof(REAL)*cv->npt);
			}
		}
	}

/* Clean up workspaces and exit */
	(*FitRoutine)(NKEY_EXIT, 0, &nls);			/* Free allocated workspaces	*/
	free(nls.yfit);									/* My responsibility to free	*/
	free(nls.errorbar);								/* Free my allocated spaces	*/
	free(nls.outchi);
	free(nls.vars);
	free(nls.sigma);
	free(nls.correlate);
	free(nls.lower);
	free(nls.upper);
	free(nls.valid);									/* Likely to be NULL  */

	if (ns->result_file != NULL) {				/* Free the temp file */
		remove(ns->result_file);
		free(ns->result_file);
		ns->result_file = NULL;
	}

	return(rcode);
}


/* ============================================================================
-- Routine to run an external macro to do some "lets" which cannot be
-- handled within the NLSFIT structure.  This macro will be run before each
-- attempt to evaluate the function, both for derivatives and function value.
-- The "varying" variables will have been changed before the macro is called.
--
-- Usage: int SetExterns()
--
-- Inputs: none
--
-- Common: ExternMacro - string containing name of external macro
--
-- Output: Runs the macro through LexSystem()
--
-- WARNING: This is a dangerous routine.  Error handling can be poor.
--          Be sure user understands use and limitations.
============================================================================ */
static int SetExterns(void) {

	char token[DFLT_STR_SIZE];
	LOGICAL echo_hold, echo_local_hold;
	int rcode;
	void *old;

	if (*ns->ExternMacro == '\0') return(0);
	
	rcode = 0;
	switch (ns->MacroType) {
		case SIMPLE:
			LexInsText("pp@@ret");			/* For my return */
			LexExecFile(ns->ExternMacro);
			echo_hold       = LexSetNoEcho(TRUE);
			echo_local_hold = LexSetLocalNoEcho(TRUE);

			while (TRUE) {
				if (! LexGetTokenP(token, sizeof(token), "NLS: ")) continue;
				if (LexEscape(TRUE)) {
					rcode = -2;
				} else if (LexSystem(0, token)) {
					continue;
				} else if (stricmp(token, "pp@@ret") == 0) {
					rcode = 0;
				} else if (stricmp(token,"quit")==0 || stricmp(token,"abort")==0) {
					rcode = -2;
				} else {
					printf("%s: not recognized.  Returning unable to complete function\n", token);
					rcode = -2;
				}
				break;
			}
			LexSetNoEcho(echo_hold);
			LexSetLocalNoEcho(echo_local_hold);
			break;
		case GENPLOT:
		case RUMP:
			old = LexSwitchStream(LexCreateStream());
			LexExecFile(ns->ExternMacro);
			echo_hold       = LexSetNoEcho(TRUE);
			echo_local_hold = LexSetLocalNoEcho(TRUE);
			rcode = (ns->MacroType == GENPLOT) ? Genplot(NULL) : (*GptRumpLink)();
			LexDestroyStream(LexSwitchStream(old));
			LexSetNoEcho(echo_hold);
			LexSetLocalNoEcho(echo_local_hold);
			break;
	}
	if (rcode != 0) LexFlush();
	return(rcode);
}

/* ============================================================================
-- func_eval - Fill in YFIT with value of function
--
-- Usage: logical = func_eval(x,yfit,npt)
--
-- Inputs: X   - Array of X points
--         NPT - Number of points to be evaluated
--
-- Common: equation - Definition of the equation to be fit
-- Input:
--
-- Output: YFIT - Curve containing the fit
--
-- WARNING: It is assumed that yfit is already linked to $YFIT from the
--          allocation of the array.  If not, we fail terribly!  Problem with
--          relinking here is that it is first deallocated before linking. 
--          This creates a memory overwrite problem that is very bad!
============================================================================ */
static int func_eval(NLS_DATA *nls) {

	int rcode;

	if ( (rcode = SetExterns()) != 0) return(rcode);
	GVLinkArray("x",0, nls->xy[0], nls->npt, NULL);				/* Link X array */
	if (in_3D_mode) GVLinkArray("y",0, nls->xy[1], nls->npt, NULL);	/* Link Y also */
	if (SysChkBreak(TRUE)) return(-5);
	rcode = GVEvalArrayExpr(nls->yfit,nls->npt,fit_eqn) ? 0 : -2 ;
	return(rcode);															/* Return filled */
}


/* ============================================================================
-- ... Subroutine to determine the derivatives with respect to each of the
-- ... varied parameters.
--
-- Usage: LOGICAL = func_deriv(X,ipt,NPT)
--
-- Inputs: X   - List of X coordinates
--         ipt - Point # at which to evaluate derivatives
--         NPT - Number of points in X
--
-- Common: NTERMS - number of derivatives needed (1 for each varied parameter)
--         DERIVA - Function used to determine derivatives (or none)
--
-- ... If no analytic derivative is defined, we use the finite difference
-- ... method - takes twice as many calculations, but NBD.
-- ... Choose the increment size as the sqrt of the machine precision?
-- ... What to do when actually zero?
--
-- COMMON Output: deriv(i) - Value of the derivatives
-- ========================================================================== */
static int func_deriv(REAL *results, NLS_DATA *nls, int ipt) {

	char token[] ="$dv00";								/* Derivative array */
	char mideqn[40];										/* equation to get deriv ~ ($dv00-fit(x))/(2*$da) */

	int i;
	static REAL delta, xtmp;

/* Choose the right mideqn based on mode */
	sprintf(mideqn, "($dv00-%s)/(2*$da)", fit_eqn);

/* And now go */
	if (ipt == 0) {												/* First entry, set all */
		GVLinkArray("x",0, nls->xy[0], nls->npt, NULL);	/* Array of X values		*/
		GVLinkReal ("$DA",   0, &delta);						/* Link dx for work		*/
		for (i=0; i<ns->NumVars; i++) {							/* And loop through all	*/
			if (SysChkBreak(TRUE)) return(-5);
			if (ns->vars[i].derivptr == NULL) continue;
			mideqn[4] = token[3] = (char) (i/10 + '0');
			mideqn[5] = token[4] = (char) (i%10 + '0');
			if (stricmp(ns->vars[i].dbyda, "(numeric)") != 0) {	/* If not analytic */
				GVSetValue(token, ns->vars[i].dbyda);
			} else {													/* Do as delta fnc */
				if ( (delta = ns->vars[i].delta) <= 0) {
					delta = *ns->vars[i].ptr * ns->DeltaFrac;		/* Small number */
					if (delta == 0) delta = ns->DeltaZero;		/* Arbitrary!!! */
				}
				*ns->vars[i].ptr += delta;
				SetExterns();
				GVSetValue(token, fit_eqn);					/* Eval function (fit(x)) */
				*ns->vars[i].ptr -= 2*delta;						/* To other side */
				SetExterns();
				GVSetValue(token, mideqn);
				*ns->vars[i].ptr += delta;
			}
		}
		GVDeallocate("$DA");
		GVLinkReal("x", 0, &xtmp);
	}

	xtmp = nls->xy[0][ipt];							/* X value I'm searching for */
	for (i=0; i<ns->NumVars; i++) {
		if (ns->vars[i].derivptr != NULL) {
			results[i] = ns->vars[i].derivptr[ipt];
		} else if (stricmp(ns->vars[i].dbyda, "(numeric)") != 0) {
			results[i] = GVTrimToReal(GVEvalExpr(ns->vars[i].dbyda, NULL));
		} else {
			delta = *ns->vars[i].ptr *1E-4f;				/* Arbitrary essentially */
			if (delta == 0) delta = 1E-6f;			/* Very small (error?) */
			*ns->vars[i].ptr += delta;						/* now A(I)-EPS */
			SetExterns();
			results[i] = GVTrimToReal(GVEvalExpr(fit_eqn, NULL));	/* Fill in DV$n with EQ(a-eps) */
			*ns->vars[i].ptr -= 2*delta;
			SetExterns();
			results[i] -= GVTrimToReal(GVEvalExpr(fit_eqn, NULL));
			results[i] /= 2*delta;
			*ns->vars[i].ptr += delta;						/* now A(I)-EPS */
		}
	}
	return(0);
}


/* ============================================================================
-- PROG_EVAL - Fill in YFIT with value of function
--
-- Usage: logical = prog_eval(x,yfit,npt)
--
-- Inputs: X   - Array of X points
--         NPT - Number of points to be evaluated
--
-- Common: equation - Definition of the equation to be fit
-- Input:
--
-- Output: YFIT - Curve containing the fit
============================================================================ */
static int prog_eval(NLS_DATA *nls) {

	char cmdline[LONG_STR_SIZE], *aptr;
	int i, rcode;

	if (SysChkBreak(TRUE)) return(-5);

	SetExterns();
	strcpy(cmdline, ns->equation);						/* The command itself */
	aptr = cmdline + strlen(cmdline);
	sprintf(aptr, " '%s'", ns->result_file);
	aptr += strlen(aptr);
	for (i=0; i<nls->nvars; i++) {
		sprintf(aptr, " %g", *nls->vars[i]);
		aptr += strlen(aptr);
	}
	if (ns->verbose >= DEBUGALL) TTYprintf("Executing: %s\n", cmdline);
	rcode = System(cmdline);
#ifdef OS2
	if (rcode != 0) {
		ERRprintf("ERROR: External function returned non-zero return code: %4.4x\n", rcode);
#else
	if (! WIFEXITED(rcode) || WEXITSTATUS(rcode) != 0) {
		ERRprintf("ERROR: External function failed or returned non-zero codes: %4.4x\n", rcode);
#endif
		return(-2);
	}
	
	if (nls->xy[2] != NULL) {
		i = GptSimpleRead(ns->result_file, nls->npt, nls->yfit, nls->yfit, nls->yfit);
	} else {
		i = GptSimpleRead(ns->result_file, nls->npt, nls->yfit, nls->yfit, NULL);
	}
	if (i < 0) ERRprintf("ERROR: GptSimpleRead failed %d\n", i);
		
	return( (i == nls->npt) ? 0 : -2 );
}


/* ============================================================================
-- ... Subroutine to determine the derivatives with respect to each of the
-- ... varied parameters.
--
-- Usage: LOGICAL = prog_deriv(X,ipt,NPT)
--
-- Inputs: X   - List of X coordinates
--         ipt - Point # at which to evaluate derivatives
--         NPT - Number of points in X
--
-- Common: NTERMS - number of derivatives needed (1 for each varied parameter)
--         DERIVA - Function used to determine derivatives (or none)
--
-- ... If no analytic derivative is defined, we use the finite difference
-- ... method - takes twice as many calculations, but NBD.
-- ... Choose the increment size as the sqrt of the machine precision?
-- ... What to do when actually zero?
--
-- COMMON Output: deriv(i) - Value of the derivatives
-- ========================================================================== */
static int prog_deriv(REAL *results, NLS_DATA *nls, int ipt) {

	char token[] ="$dv00";								/* Derivative array */
	int i,j;
	REAL *yfithold, delta;

/* ------------------------------------------------------------------------
-- yfit is already filled in and valid with value at exact args
-- We use this to avoid duplicating expensive calls, make simple call with
-- each argument shifted small amount away from zero and finite difference
------------------------------------------------------------------------ */
	if (ipt == 0) {												/* First entry, set all */
		GVLinkArray("x",0, nls->xy[0], nls->npt, NULL);	/* Array of X values		*/
		yfithold = nls->yfit;									/* Keep this pointer		*/
		for (i=0; i<ns->NumVars; i++) {							/* And loop through all	*/
			if (stricmp(ns->vars[i].dbyda, "(numeric)") != 0) {	/* If not analytic */
				token[3] = (char) (i/10 + '0');
				token[4] = (char) (i%10 + '0');
				GVSetValue(token, ns->vars[i].dbyda);
			} else {													/* Do as delta fnc */
				if ( (delta = ns->vars[i].delta) <= 0) {
					delta = *ns->vars[i].ptr * ns->DeltaFrac;		/* Small number */
					if (delta == 0) delta = ns->DeltaZero;		/* Arbitrary!!! */
				}
				*ns->vars[i].ptr += delta;
				nls->yfit = ns->vars[i].derivptr;
				if (prog_eval(nls) != 0) return(-2);
				*ns->vars[i].ptr -= delta;
				for (j=0; j<nls->npt; j++) ns->vars[i].derivptr[j] = (ns->vars[i].derivptr[j]-yfithold[j]) / delta;
			}
		}
		nls->yfit = yfithold;									/* Keep this pointer		*/
	}

	for (i=0; i<ns->NumVars; i++) {
		if (ns->vars[i].derivptr != NULL) {
			results[i] = ns->vars[i].derivptr[ipt];
		} else if (stricmp(ns->vars[i].dbyda, "(numeric)") != 0) {
			results[i] = GVTrimToReal(GVEvalExpr(ns->vars[i].dbyda, NULL));
		} else {
			TTYprintf("ERROR: Please tell me this didn't happen (nlsfit-program)\n");
		}
	}
	return(0);
}
