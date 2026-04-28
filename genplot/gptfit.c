/* FIT$.F77 */

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
#if (defined LINUX && ! defined __USE_XOPEN)	/* Need prototype for erfc()			*/
	#define	__USE_XOPEN								/* Must be placed just before math.h */
#endif
#include <math.h>
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define GV_MATH_EXTENSIONS					/* Need ndtri & q_chi */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "gptdef.h"							/* Needed for gpt_do_lsqfit */

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
static int polyfit(int i);
static int constant_fit(void);
static int proportional_fit(void);
static int linear_fit(void);
static int correlate(void);
static int surf_fit(void);
static int surf_fit_quad(void);
static int spline(CURVE *cv);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static LOGICAL		UseWeighting, UseSigmaWeighting, BeVerbose;
static CHAR			Weighting[LONG_STR_SIZE];
static REAL			cf[12],sigma[12];

/* -- Parameters for Inrange -- */
static REAL	   RangeXmin, RangeXmax, RangeYmin, RangeYmax;
static GVCMDS *for_condition_cmds=NULL;
static LOGICAL GetFitOptions(void);
static LOGICAL InRange(REAL *x, REAL*y, int i);


/* ============================================================================
--     Routine to determine least square fit to the data and draw a line. 
--     Simple only.  Use FIT LINEAR etc. if someone wants more.
--
--     Usage: log = my_lsqf(x,y,npt,nptmax,ids)
--
--     Inputs: x,y    - X and Y pairs of current data
--             npt    - Number of valid points
--	      nptmax - maximum number of points
--	      ids    - character descriptor
--
--     Output: my_lsqf      - Success of the determination
--
--     Optional range specification via:
--           { [X]RANGE XMIN XMAX | YRANGE YMIN YMAX | CURSOR }
--
-- Note: Moved from gptsubs.c to gptfit.c so can set CF$ and SIGMA$ values.
============================================================================ */
static char LsqfitHelp[]=
"The LSQFIT command is a quick alternative to the more powerful and detailed\n"
"FIT command.  It runs a simple least squares linear fit to the data, reports\n"
"the slope and intercept, and draws a line on the graph corresponding to the fit.\n"
"For slope and intercept error bars, or more options for selecting data to be\n"
"included, see the FIT command.\n"
"\n"
"Usage: LSQFIT [<curve>] [-options]\n"
"\n"
"If an optional curve is specified after the cursor command, those data will\n"
"be used instead of the main plot buffer.\n"
"\n"
"Options:\n"
"   -help | -?        Prints this help message\n"
"\n"
"   -xrange <xmin> <xmax>                    Limit included data to X range\n"
"   -yrange <xmin> <xmax>                    Limit included data to Y range\n"
"   -range   <xmin> <xmax> <ymin> <ymax>     Limit on both X,Y ranges\n"
"   -xyrange <xmin> <xmax> <ymin> <ymax>     Limit on both X,Y ranges\n"
"   -cursor                                  Use box cursor to select data\n"
"\n"
"Output: Prints slope/intercept and draws a line on the graph\n"
"\n"
"Notes:\n"
"  (1) If the -cursor option is specified, the arrays XBOX$ and YBOX$ will\n"
"      be defined with the corners of the included dataset\n"
"  (2) LSQFIT gets upset if you try to fit a line to less than 3 points\n"
"\n"
" Examples: lsqfit                     /* Basic command\n"
"           lsqfit -cursor             /* Fit a subset of the data\n"
;

int gpt_do_lsqfit(void) {

	REAL xxmin=-1.0e37f,xxmax=1.0e37f,yymin=-1.0e37f,yymax=1.0e37f;	/* Range */
	double slope, intercept, s,sx,sxx,sy,sxy;
	char token[OPTION_STR_SIZE];
	int i;
	REAL *x, *y;

/* Check if option is for help */
	if (LexCheckHelp("Lsqfit", LsqfitHelp, NULL)) return(OKAY);

	GptSetRange();																/* Set range */

/*-----------------------------------------------------------------------------
-- ... Handle the options first
----------------------------------------------------------------------------- */
	while (LexGetOption(token, sizeof(token))) {
		i = LexSelect(token, "-RANGE -XRANGE -YRANGE -XYRANGE -CURSOR");
		switch(i) {
			case 1:
			case 2:
			case 3:
			case 4:
				if (i == 1 || i == 2 || i == 4) {
					xxmin = LexGetReal(xxmin, "Minimum X value: ");
					if (LexEscape(TRUE)) return(OKAY);
					xxmax = LexGetReal(xxmax, "Maximum X value: ");
					if (LexEscape(TRUE)) return(OKAY);
				}
				if (i == 3 || i == 4) {
					yymin = LexGetReal(yymin, "Minimum Y value: ");
					if (LexEscape(TRUE)) return(OKAY);
					yymax = LexGetReal(yymax, "Maximum Y value: ");
					if (LexEscape(TRUE)) return(OKAY);
				}
				break;
			case 5:
				TTYputs("Define allowed window with cursor\n");	/* -CURSOR */
				TTYflush();
				PlotBoxCursor(&xxmin, &yymin, &xxmax, &yymax, NULL);
				gpt_SetBoxCursorCoords(xxmin, yymin, xxmax, yymax);
				break;
			default:
				ERRprintf("ERROR: %s is an unrecognized LSQFIT option\n", token);
				return(NOMORE);
		}
	}
	OrderPair(&xxmin, &xxmax);
	OrderPair(&yymin, &yymax);

	x = GptCurve->x; y = GptCurve->y;
	s = sx = sxx = sy = sxy = 0.0;
	for (i=0; i<GptCurve->npt; i++) {
		if (x[i] >= xxmin && x[i] <= xxmax && y[i] >= yymin && y[i] <= yymax) {
			s   += 1;
			sx  += x[i];
			sxx += x[i]*x[i];
			sy  += y[i];
			sxy += x[i]*y[i];
		}
	}

	if (s <= 2) {
		ERRprintf("ERROR: Less than 3 points.  LSQFIT not possible\n");
		return(NOMORE);
	} else {
		slope = s*sxx - sx*sx;				/* Determinant */
		if (fabs(slope) < 1E-35) {
			gen_err("LSQFIT matrix singular (ie. data isn't even close to a line)");
			return(NOMORE);
		}
		intercept = (sxx*sy - sx*sxy) / slope;
		slope = (s*sxy - sx*sy) / slope;
	}
	cf[0] = (REAL) intercept;
	cf[1] = (REAL) slope;

	GVLinkArray("cf$",			GVF_USER, cf,    2, NULL);	/* Real coefficients */
	GVAllocFnc ("fit(x)",		GVF_USER, "poly(x,cf$)");	/* Fit function		*/

/* For this one, dealloc since users may become confused */
	GVDeallocate("sigma$");
	GVDeallocate("variance$");
	GVDeallocate("chisqr$");
	GVDeallocate("quality$");
	GVDeallocate("Q$");
	GVDeallocate("c_error$");
	GVDeallocate("sigdat$");

/* ... Output the result to screen and draw the line */
	TTYprintf(" Slope:    %14.7g     Intercept: %14.7g\n", slope, intercept);
	if (PlotSystem(2, NULL, NULL)) {
		PlotSetLineType(max(1, abs(Gpt->linetype)), 0.0f);
		PlotMove(Gpt->xmin, (REAL) (Gpt->xmin*slope+intercept), 3);
		PlotMove(Gpt->xmax, (REAL) (Gpt->xmax*slope+intercept), 2);
		PlotSetLineType(1, 0.0f);
		PlotFlush();
	}

	return(OKAY);
}

/* ============================================================================
-- Processor to handle various fitting processes (hopefully will work?)
--
-- Usage: LOGICAL = FIT$(x,y,npt)
--
-- Inputs: x,y,npt - Curve to work fit on
--
-- Output: FIT$ - Success of fit
--
-- Each procedure should generate a "FIT" function equal to the fitting fnc.
============================================================================ */
typedef enum _OPS1 {
	HELP, LIST, ABORT, 
	CONSTANT, PROPORTIONAL, LINEAR, CORRELATE, POLYNOMIAL, SPLINE, NLSFIT, PLANE_3D, SURF_3D
} OPS1;
	
typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1  rcode;
} CMTYPE;

PRIVATE const CMTYPE cmlist[] = {	
	{"?",					1, LIST},				{"-?",			  -2, HELP},
	{"list",				3, LIST},				
	{"abort",			5,	ABORT},
	{"constant",		3, CONSTANT},
	{"proportional",	4, PROPORTIONAL},
	{"linear",			3, LINEAR},
	{"correlate",		3, CORRELATE},			{"correlation",  -9,	CORRELATE},
	{"polynomial",		4, POLYNOMIAL},
	{"spline",			2, SPLINE},
	{"nlsfit",			3, NLSFIT},				{"nonlinear",	  -4, NLSFIT},
	{"plane",			5, PLANE_3D},
	{"surface",			4, SURF_3D},
	{NULL,				0, ABORT}
};

static char FitHelp[]=
	 "The FIT command is used to model data to one of several analytical expressions.\n"
	 "These include linear and polynomial fits that are commonly required.  For more\n"
	 "complex and arbitrary fitting functions, the NLSFIT command can be used.\n"
	 "\n"
	 "Usage: FIT [<curve>] <type> [-options]\n"
	 "\n"
	 "If an optional curve is specified after the cursor command, those data will\n"
	 "be used instead of the main plot buffer.\n"
	 "\n"
    "Fit types:\n"
    "   ? | LISt        - List all possible responses for <type>\n"
	 "   ABORT           - Abandon command without any fit\n"
    "   CONstant        - Fit data to a constant (y = C)\n"
    "   PROPortional    - Fit data as a proportional relationship (y = mx)\n"
    "   LINear          - Fit data to a linear relationship (y = mx+b)\n"
	 "   POLYnomial      - Fit data to a polynomial of order n (y = poly(x, array))\n"
    "   SPline          - Spline fit to the data (y = spline(x))\n"
    "   NLSfit          - Alias for just NLSFIT to enter non-linear fitting module\n"							 
    "   NONLinear       - Equivalent to NLSFIT\n"
    "   CORrelate       - Correlation between x and y\n"  
	 "   CORRELATIon     - Equivalent to CORRELATE\n"  
    "   PLANE           - Fit 3D data to a plane (z = ax+by+c)\n"
    "   SURFace         - Fit 3D data to a quadratic surface (z = ax^2+by^2+cxy+dx+ey+f)\n"
    "\n"							  
    "Options: Each fit module may have its own options.  With the exception of spline,\n"
	 "         most include the following options\n"
	 "   -xrange <xmin> <xmax>                  Limit included data to X range\n"
	 "   -yrange <xmin> <xmax>                  Limit included data to Y range\n"
	 "   -range   <xmin> <xmax> <ymin> <ymax>   Limit on both X,Y ranges\n"
	 "   -xyrange <xmin> <xmax> <ymin> <ymax>   Limit on both X,Y ranges\n"
	 "   -cursor                                Use box cursor to select data\n"
    "   -for <condition_expr>                  Expression defining allowed data\n"
    "   -sigma <expr> | -erry <expr>           Set uncertainty on each point\n"
    "   -weight <expr>                         Weight each data point by expression\n"
    "   -silent | -quiet                       Run silent mode.  Vars return values\n"
	 "\n"
	 "Output: In general, fit parameters and statistics are printed to screen.\n"
    "   fit(x) or fit(x,y)          Defined function for the fit\n"
    "   cf$[]                       Array with fit parameters\n"
    "   sigma$[]                    Array with fit parameter uncertainty\n"
    "   Statistical variables varying with the type of fitting and options\n"							 
	 "\n"
	 "Notes:\n"
	 "  (1) If the -cursor option is specified, the arrays XBOX$ and YBOX$ will\n"
	 "      be defined with the corners of the included dataset\n"
	 "\n"
	 " Examples: fit linear                     /* Basic command\n"
	 "           fit proportional -silent -sigma 1/sqrt(y)\n"
	 ;

/* ----------------------------------------------------------------------- */
int GptFit(void) {

	CMTYPE *citem;
	char token[OPTION_STR_SIZE];
	int i;

	if (LexCheckHelp("Fit", FitHelp, NULL)) return(0);

	if (! LexGetTokenP(token, sizeof(token), "Type of fit: Use ? for list -? for help (abort): "))
		return(0);
	if (LexEscape(TRUE)) return(1);

	if ( (citem=LexCmdl(token, cmlist, sizeof(CMTYPE))) == NULL) {
		ERRprintf("ERROR: Invalid fit selection (%s)\n", token);
		return(-1);
	} else switch (citem->rcode) {
		case HELP:
			LexInsText("-?");
			LexCheckHelp("Fit", FitHelp, NULL);
			return 0;
		case LIST:
			LexCmdlPrint(cmlist, sizeof(CMTYPE), "FIT functions available:");
			return(0);
		case ABORT:
			return(1);
		case CONSTANT:
			return constant_fit();
		case PROPORTIONAL:
			return proportional_fit();
		case LINEAR:								/* Linear LSQFIT (simple) */
			return(linear_fit());
		case CORRELATE:
			return(correlate());
		case POLYNOMIAL:							/* Polynomial fit */
			i = LexGetInt(2, "Order of fit (2): ");
			if (LexEscape(TRUE)) return(1);
			return(polyfit(i));
		case PLANE_3D:
			return(surf_fit());
		case SURF_3D:
			return(surf_fit_quad());
		case SPLINE:								/* Spline fit */
			return(spline(GptCurve));
		case NLSFIT:								/* Nonlinear least squares */
			return(nlsfit(GptCurve));
	}
	return(-1);
}

/* ============================================================================
-- Subroutine for linear least squares fit (with lots of statistics)
--
-- Usage:  int constant_fit(void);
--
-- Inputs: internal variables
--
-- Output: Average and uncertainty in average.  Either based on given
--         sigma, or calculated sigmas from standard deviation
============================================================================ */
static int constant_fit(void) {

	int num_valid, i, dof, ierr;
	double weight,sigdat;					/* Do in double precision */
	GVCMDS *weightcmds=NULL;

	double s=0.0, sy=0.0, syy=0.0, r=0.0, ry=0.0, ryy=0.0;

	REAL var, chisqr, q;

	REAL *x, *y;
	int  npt;

/* -- Code begin -- */
	if (! GetFitOptions()) return(-1);					/* Do we limit, or full? */
	num_valid = 0;										/* Number of points used */
	weight    = 1.0;									/* In case no weighting */
	x = GptCurve->x;
	y = GptCurve->y;
	npt = GptCurve->npt;

	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.  We do
-- two passes through the data to eliminate the numerical instability
-- associated with the matrix determinate in the straightforward version.  See
-- Numerical Recipies section 14.2 for description.                        
---------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;
		if (weightcmds != NULL) {						/* Weighting			*/
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr != 0) weight = 1.0;
			if (UseSigmaWeighting) weight = 1/weight/weight;
		}
		r   += 1;
		s   += weight;
		ry  += y[i];
		sy  += y[i]*weight;
		ryy += y[i]*y[i];
		syy += y[i]*y[i]*weight;
	}
	dof = max(num_valid-1, 1);				/* Number degrees of freedom */

	if (num_valid < 1) {
		ERRprintf("What drugs are you on?? Fit a constant with no data?\n");
		return(-1);
	} else if (num_valid == 1) {
		cf[0] = (REAL) y[0];
		sigma[0] = 0.0f;
		var      = 0.0f;
		chisqr   = 0.0f;
		sigdat   = 0.0f;
		q        = 0.0f;
	} else {
		cf[0]    = (REAL) (sy/s);										/* Mean										*/
		sigma[0] = (REAL) (1/sqrt(s));								/* Variance of weighted sum			*/
		chisqr   = (REAL) ((syy-s*pow(sy/s,2)));					/* Chisqr X^2								*/
		var      = (REAL) (r/(r-1.0)*((syy/s-pow(sy/s,2))));	/* Variance s_y^2							*/
		sigdat   = (REAL) sqrt(var);									/* Single value pt error estimate	*/

		if (UseSigmaWeighting) {										/* Which statistics to report			*/
			q = (REAL) q_chi(chisqr, dof);							/* X^2 distribution probability		*/
			chisqr /= dof;													/* And returned reduced chi^2			*/
		} else {
			sigma[0] = (REAL) (sigma[0]*sigdat);					/* SDOM(data) */
			q = 0.5;															/* Using estimated sigma, would get X^2_\nu = 1 */
		}
	}

/* ---------------------------------------------------------------------------
Average:      0.5002892 +/- 0.02052427      Degrees of Freedom: 199
                                            Root Mean Variance: 0.2902569
                                            Reduced chi-square: 128.2288
                                            Q(chi^2):           0.9129976
                                            Error correlation:  0.000
---------------------------------------------------------------------------- */
	if (BeVerbose) {
		TTYprintf(" Average: %14.7g +/- %-14.7g  Degrees of Freedom: %i\n"
					 "                                             Root Mean Variance: %-14.7g\n",
					 cf[0], sigma[0], dof, sqrt(var));
		if (UseSigmaWeighting) {
			TTYprintf("                                             Reduced chi-square: %-14.7g\n"
						 "                                             Q(chi-squared):     %-10.7f\n",
						 chisqr, q);
		} else {
			TTYprintf("                                             chi-square:         %-14.7g\n"
						 "                                             Estimated Y sigma:  %-14.7g\n",
						 chisqr, sigdat);
		}
	}

	GVLinkArray("cf$",			GVF_USER, cf,    1, NULL);	/* Real coefficients */
	GVLinkArray("sigma$",		GVF_USER, sigma, 1, NULL);	/* Link the sigma		*/
	GVAllocReal("variance$",	GVF_USER, var);				/* Variance				*/
	GVAllocReal("chisqr$",		GVF_USER, chisqr);			/* Chi-squared value	*/
	GVAllocReal("quality$",		GVF_USER, q);					/* Quality of fit		*/
	GVAllocReal("Q$",				GVF_USER, q);					/* Quality of fit		*/
	GVDeallocate("c_error$");
	GVAllocReal("sigdat$",     GVF_USER, (REAL) sigdat);	/* Estimate pt error	*/
	GVAllocFnc ("fit(x)",		GVF_USER, "cf$[0]");			/* Fit function		*/
	return(0);
}

/* ============================================================================
-- Subroutine for linear least squares fit (with lots of statistics)
--
-- Usage:  int proportional_fit(void);
--
-- Inputs: internal variables
--
-- Output: Average and uncertainty in average.  Either based on given
--         sigma, or calculated sigmas from standard deviation
============================================================================ */
static int proportional_fit(void) {

	int num_valid, i, dof, ierr;
	double weight,sigdat;					/* Do in double precision */
	GVCMDS *weightcmds=NULL;

	double s=0.0, sx=0.0, sxx=0.0, sxy=0.0, sy=0.0, syy=0.0, r=0.0, rx=0.0, rxx=0.0, ry=0.0, rxy=0.0, ryy=0.0;

	REAL var, chisqr, q;

	REAL *x, *y;
	int  npt;

/* -- Code begin -- */
	if (! GetFitOptions()) return(-1);					/* Do we limit, or full? */
	num_valid = 0;										/* Number of points used */
	weight    = 1.0;									/* In case no weighting */
	x = GptCurve->x;
	y = GptCurve->y;
	npt = GptCurve->npt;

	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.  We do
-- two passes through the data to eliminate the numerical instability
-- associated with the matrix determinate in the straightforward version.  See
-- Numerical Recipies section 14.2 for description.                        
---------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;
		if (weightcmds != NULL) {						/* Weighting			*/
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr != 0) weight = 1.0;
			if (UseSigmaWeighting) weight = 1/weight/weight;
		}
		r   += 1;
		s   += weight;
		rx  += x[i];
		sx  += x[i]*weight;
		rxx += x[i]*x[i];
		sxx += x[i]*x[i]*weight;
		ry  += y[i];
		sy  += y[i]*weight;
		rxy += x[i]*y[i];
		sxy += x[i]*y[i]*weight;
		ryy += y[i]*y[i];
		syy += y[i]*y[i]*weight;
	}
	dof = max(num_valid-1, 1);				/* Number degrees of freedom */

	if (num_valid < 1) {
		ERRprintf("What drugs are you on?? Fit proportional with no data?\n");
		return(-1);
	} else if (num_valid == 1) {
		cf[0]    = (REAL) ((x[0] == 0.0) ? 0.0 : y[0]/x[0]);
		sigma[0] = 0.0f;
		var      = 0.0f;
		chisqr   = 0.0f;
		sigdat   = 0.0f;
		q        = 0.0f;
	} else {
		cf[0]    = (REAL) (sxy/sxx);									/* Mean slope								*/
		sigma[0] = (REAL) (1/sqrt(sxx));								/* Variance of weighted sum			*/
		chisqr   = (REAL) (syy-sxy*sxy/sxx);						/* Chisqr X^2								*/
		var      = (REAL) (r/(r-1.0)*(chisqr/s));					/* Variance s_y^2							*/
		sigdat   = (REAL) sqrt(var);									/* Single value pt error estimate	*/

		if (UseSigmaWeighting) {										/* Which statistics to report			*/
			q = (REAL) q_chi(chisqr, dof);							/* X^2 distribution probability		*/
			chisqr /= dof;													/* And returned reduced chi^2			*/
		} else {
			sigma[0] = (REAL) (sigma[0]*sigdat);					/* SDOM(data) */
			q = 0.5;															/* Using estimated sigma, would get X^2_\nu = 1 */
		}
	}

/* ---------------------------------------------------------------------------
Slope:    0.2470705E-01 +/- 0.1218636E-01   Degrees of Freedom:  198
                                            Reduced chi-square: 128.2288
                                            Q(chi^2):           0.9129976
                                            Error correlation:  0.000
---------------------------------------------------------------------------- */
	if (BeVerbose) {
		TTYprintf(" Slope: %16.7g +/- %-16.7gDegrees of Freedom: %i\n"
					 "                                             Root Mean Variance: %-14.7g\n",
					 cf[0], sigma[0], dof, sqrt(var));
		if (UseSigmaWeighting) {
			TTYprintf("                                             Reduced chi-square: %-14.7g\n"
						 "                                             Q(chi-squared):     %-10.7f\n",
						 chisqr, q);
		} else {
			TTYprintf("                                             chi-square:         %-14.7g\n"
						 "                                             Estimated Y sigma:  %-14.7g\n",
						 chisqr, sigdat);
		}
	}

	GVLinkArray("cf$",			GVF_USER, cf,    1, NULL);	/* Real coefficients */
	GVLinkArray("sigma$",		GVF_USER, sigma, 1, NULL);	/* Link the sigma		*/
	GVAllocReal("variance$",	GVF_USER, var);				/* Variance				*/
	GVAllocReal("chisqr$",		GVF_USER, chisqr);			/* Chi-squared value	*/
	GVAllocReal("quality$",		GVF_USER, q);					/* Quality of fit		*/
	GVAllocReal("Q$",				GVF_USER, q);					/* Quality of fit		*/
	GVDeallocate("c_error$");
	GVAllocReal("sigdat$",     GVF_USER, (REAL) sigdat);	/* Estimate pt error	*/
	GVAllocFnc ("fit(x)",		GVF_USER, "cf$[0]*x");		/* Fit function		*/
	return(0);
}

/* ============================================================================
-- Subroutine for linear least squares fit (with lots of statistics)
--
-- Usage:  LOGICAL linear_fit(void)
--
-- Inputs: X,Y,NPT - Curve to fit
--
-- Output: Set of coefficients CF(I), [0,1] for fit and FIT function
--	  LINFIT$ - Ability to actually do the fit
============================================================================ */
static int linear_fit(void) {

	int num_valid, i, dof, ierr;
	double X0,X1,Y0,Y1;
	double weight,sigdat;					/* Do in double precision */
	GVCMDS *weightcmds=NULL;

	double s=0.0, sx=0.0, sy=0.0, sxx=0.0, sxy=0.0, syy=0.0,
			 r=0.0, rx=0.0, ry=0.0, rxx=0.0, rxy=0.0, ryy=0.0;
	double stt=0, sty=0;								/* Reliable measurements */

	REAL var, chisqr, q, rab;

	REAL *x, *y;
	int  npt;
	
/* -- Code begin -- */
	if (! GetFitOptions()) return(-1);					/* Do we limit, or full? */
	num_valid = 0;										/* Number of points used */
	weight    = 1.0;									/* In case no weighting */
	x = GptCurve->x;
	y = GptCurve->y;
	npt = GptCurve->npt;

	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.  We do
-- two passes through the data to eliminate the numerical instability
-- associated with the matrix determinate in the straightforward version.  See
-- Numerical Recipies section 14.2 for description.                        
---------------------------------------------------------------------------- */
	X1 = Y1 = 0;											/* For initialization */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;
		X0=X1; Y0=Y1; X1=x[i]; Y1=y[i];				/* Keep for 2 points */
		if (weightcmds != NULL) {						/* Weighting			*/
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr != 0) weight = 1.0;
			if (UseSigmaWeighting) weight = 1/weight/weight;
		}
		r   += 1;
		s   += weight;
		rx  += X1;
		sx  += X1*weight;
		rxx += X1*X1;
		sxx += X1*X1*weight;
		ry  += Y1;
		sy  += Y1*weight;
		ryy += Y1*Y1;
		syy += Y1*Y1*weight;
		rxy += X1*Y1;
		sxy += X1*Y1*weight;
	}

/* ---------------------------------------------------------------------------
-- Second run through -- collect the "normalized" sums for X to avoid det.
-- Collect ti^2, ti*y/oi, where ti = (xi-Sx/S)/oi, Sx and S determined above
---------------------------------------------------------------------------- */
	if (num_valid > 2) {									/* Don't bother for 1 or 2 */
		for (i=0; i<npt; i++) {
			if (! InRange(x,y,i)) continue;
			if (weightcmds != NULL) {					/* Weighting			*/
				weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
				if (ierr != 0) weight = 1.0;
				if (UseSigmaWeighting) weight = 1/weight/weight;
			}
			stt += (x[i]-sx/s) * (x[i]-sx/s) * weight ;		/* A bit changed */
			sty += (x[i]-sx/s) * y[i] * weight;					/* from Num Recipes */
		}
	}

	dof = max(num_valid-2, 1);				/* Number degrees of freedom */

	if (num_valid < 2) {
		ERRprintf("What drugs are you on?? Fit a line through 1 point?\n");
		return -1;
	} else if (num_valid == 2) {
		if (X1 == X0) X0 = (X0==0) ? -1E-10 : 0.9999999*X0;	/* Very bad boys like Shaoyin - infinite slope */
		cf[1] = (REAL) ((Y1-Y0)/(X1-X0));		/* Simple line */
		cf[0] = (REAL) (Y1 - cf[1]*X1);			/* Simple offset */
		sigma[0] = 0.0f;
		sigma[1] = 0.0f;
		rab      = 0.0f;
		q        = 0.0f;
		var      = 0.0f;
		chisqr   = 0.0f;
	} else if (stt == 0 || s == 0 || sxx == 0) {
		ERRprintf("ERROR: Have a glass of wine instead.  Can't fit if all X values are the same.\n");
		return -1;
	} else {
		cf[1]    = (REAL) (sty/stt);								/* M */
		cf[0]    = (REAL) ((sy-sx*cf[1]) / s);					/* B */
		sigma[1] = (REAL) sqrt(1/stt);							/* sigma(m) */
		sigma[0] = (REAL) sqrt((1+sx*sx/s/stt) / s);			/* sigma(b) */
		rab      = (REAL) (-sx/sqrt(s*sxx));					/* Error correlation */
		var      = (REAL) (ryy - 2*cf[1]*rxy - 2*cf[0]*ry + cf[1]*cf[1]*rxx + 2*cf[1]*cf[0]*rx + cf[0]*cf[0]*r);
		chisqr   = (REAL) (syy - 2*cf[1]*sxy - 2*cf[0]*sy + cf[1]*cf[1]*sxx + 2*cf[1]*cf[0]*sx + cf[0]*cf[0]*s);
		sigdat   = (REAL) sqrt(var/dof);							/* Single value pt error estimate */
		if (! UseSigmaWeighting) {									/* Estimate sigma's */
			sigma[0] = (REAL) (sigma[0]*sigdat);
			sigma[1] = (REAL) (sigma[1]*sigdat);
/* ... If use the estimated sigma, chisqr would be dof and q = 0.5 */
/*			chisqr   = dof;	*/
			q = 0.5;
		} else {
			q = (REAL) q_chi(chisqr, dof);
			chisqr /= dof;												/* And returned reduced chi^2 */
		}
	}

/* ---------------------------------------------------------------------------
Slope:       0.2470705E-01 +/- 0.1218636E-01   Degrees of Freedom:  198
Intercept:  -0.4587527E-01 +/- 0.7071068E-01   Root Mean Variance: 0.1218636E-0
                                               Reduced chi-square: 128.2288
                                               Q(chi^2):           0.9129976
                                               Error correlation:  0.000
---------------------------------------------------------------------------- */
	if (BeVerbose) {
		TTYprintf(" Slope:    %16.7g +/- %-16.7gDegrees of Freedom: %i\n"
					 " Intercept:%16.7g +/- %-16.7gRoot Mean Variance: %-14.7g\n"
				    "                                                Error correlation: %7.4f\n",
					 cf[1], sigma[1], dof, cf[0], sigma[0], sqrt(var/dof), rab);
		if (UseSigmaWeighting) {
			TTYprintf("                                                Reduced chi-square: %-14.7g\n"
						 "                                                Q(chi-squared):     %-10.7f\n",
						 chisqr, q);
		} else {
			TTYprintf("                                                chi-square:         %-14.7g\n"
						 "                                                Estimated Y sigma:  %-14.7g\n",
						 chisqr, sigdat);
		}
	}

	GVLinkArray("cf$",			GVF_USER, cf,    2, NULL);	/* Real coefficients */
	GVLinkArray("sigma$",		GVF_USER, sigma, 2, NULL);	/* Link the sigma		*/
	GVAllocReal("variance$",	GVF_USER, var);				/* Variance				*/
	GVAllocReal("chisqr$",		GVF_USER, chisqr);			/* Chi-squared value	*/
	GVAllocReal("quality$",		GVF_USER, q);					/* Quality of fit		*/
	GVAllocReal("Q$",				GVF_USER, q);					/* Quality of fit		*/
	GVAllocReal("c_error$",		GVF_USER, rab);				/* Error correlation	*/
	GVAllocReal("sigdat$",     GVF_USER, (REAL) sigdat);	/* Estimate pt error	*/
	GVAllocFnc ("fit(x)",		GVF_USER, "poly(x,cf$)");	/* Fit function		*/
	return 0;
}


/* ============================================================================
-- Subroutine for linear correlation fit (no error bars, but some statistics)
--
-- Usage:  int correlate(x,y,npt)
--
-- Inputs: X,Y,NPT - Curve to fit
--
-- Output: 
============================================================================ */
static int correlate(void) {

	int nv, i,j, dof;
	double rx, ry, rxy, rxx, ryy, stt, sty;
	double fm, gm, d2, d_0, std_d, n3n;
	REAL *xtmp, *ytmp, prob_random;

	REAL rab;

	REAL *x, *y;
	int  npt;
	
/* -- Code begin -- */
	if (! GetFitOptions()) return(-1);					/* Do we limit, or full? */

	if (UseWeighting || UseSigmaWeighting) {	/* Don't allow */
		ERRprintf("ERROR: Don't know how to handle error bars with correlation determination\n");
		return(-1);
	}

	x = GptCurve->x;
	y = GptCurve->y;
	npt = GptCurve->npt;

/* For correlation, no weighting is allowed - don't really know how to handle */
	nv = 0;
	rx = rxx = ry = ryy = rxy = stt = sty = 0.0;
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		nv  += 1;
		rx  += x[i];
		rxx += x[i]*x[i];
		ry  += y[i];
		ryy += y[i]*y[i];
		rxy += x[i]*y[i];
	}
	if (nv <= 2) {										/* User is really bad */
		ERRprintf("ERROR: How should I know if there is a correlation with only %d points?\n", npt);
		return(-1);
	}
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		stt += (x[i]-rx/nv) * (x[i]-rx/nv);		/* A bit changed */
		sty += (x[i]-rx/nv) * y[i];				/* from Num Recipes */
	}

	dof = nv-2;											/* Number degrees of freedom */

	cf[1]    = (REAL) (sty/stt);					/* M */
	cf[0]    = (REAL) ((ry-rx*cf[1]) / nv);	/* B */
	rab      = (REAL) ((nv*rxy-rx*ry) / sqrt((nv*rxx-rx*rx)*(nv*ryy-ry*ry)));

	if (BeVerbose) {
		TTYprintf(" Traditional correlation coeff: %6.4f for %d degrees of freedom\n"
					 "    Best linear estimate slope: %g    intercept: %g\n",
			rab, dof, cf[1], cf[0]);
	}

	GVLinkArray("cf$",			 GVF_USER, cf, 2, NULL);		/* Real coefficients */
	GVAllocReal("correlation$", GVF_USER, rab);					/* Correlation coeff */
	GVAllocFnc ("fit(x)",		 GVF_USER, "poly(x,cf$)");		/* Fit function		*/

/* ---------------------------------------------------------------------------
-- And now robust estimation of correlation using Spearman Rank-Order
--  Concept: Replace all values with sequential order values
--           Calculate correlation on these data
--             0.17 0.88   ==> 0 2
--             0.82 0.15   ==> 2 1
--             0.52 0.08   ==> 1 0
--           Doesn't require data to come from a normal distribution
--
-- See Section 13.7-13.8 in Numerical Recipes
--------------------------------------------------------------------------- */
	xtmp = malloc(nv*sizeof(*xtmp));						/* We know how many we will have */
	ytmp = malloc(nv*sizeof(*ytmp));						/* We know how many we will have */
	for (i=0,j=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		xtmp[j] = x[i]; ytmp[j] = y[i]; j++;
	}

	heap_sort(xtmp, ytmp, NULL, nv, SORT_ON_X | SORT_DOY);		/* Sort on X */
	fm = 0;
	for (i=0; i<nv; ) {
		if (i != nv-1 && xtmp[i+1] == xtmp[i]) {			/* Have duplicates */
			j = i;
			while (i<nv && xtmp[i]==xtmp[j]) i++;
			fm += (i-j)*(i-j)*(i-j) - (i-j);					/* fm sum			*/
			xtmp[i-1] = (j+i-1)/2.0f;							/* Average value	*/
			while (j < i) xtmp[j++] = xtmp[i-1];
		} else {
			xtmp[i] = (REAL) i;
			i++;
		}
	}

	heap_sort(xtmp, ytmp, NULL, nv, SORT_ON_Y | SORT_DOX);		/* Repeat on y */
	gm = 0;
	for (i=0; i<nv; ) {
		if ((i != nv-1) && (ytmp[i+1] == ytmp[i])) {		/* Have duplicates */
			j = i;
			while (i<nv && ytmp[i]==ytmp[j]) i++;
			gm += (i-j)*(i-j)*(i-j) - (i-j);					/* fm sum			*/
			ytmp[i-1] = (j+i-1)/2.0f;							/* Average value */
			while (j < i) ytmp[j++] = ytmp[i-1];
		} else {
			ytmp[i] = (REAL) i;
			i++;
		}
	}

/* Create the d = sum-square-difference-of-ranks */
	for (d2=0,i=0; i<nv; i++) d2 += pow(xtmp[i]-ytmp[i],2);
	free(xtmp); free(ytmp);

/* Now we can get the correlation coefficient rab */
	n3n = pow(nv,3)-nv;							/* Do as double for range problems */
	rab = (REAL) ((1-6/n3n*(d2+fm/12+gm/12)) / sqrt((1-fm/n3n)*(1-gm/n3n)));

/* Calculate expectation value of D, standard deviation, and probability */
	d_0   = n3n/6 - fm/12 - gm/12;
	std_d = (nv-1.0)*((double) nv)*((double) nv)*(nv+1.0)*(nv+1)/36.0 * (1-fm/n3n)*(1-gm/n3n);
	d2 = fabs(d2-d_0) / sqrt(std_d);	/* # of standard deviations			 */
	prob_random = (REAL) erfc(d2/sqrt(2.0));	/* probability random would cause it */

/*	t = rab*sqrt(dof/(1-rab)/(1+rab)); */

	if (BeVerbose) {
		TTYprintf(" Spearman Rank-Order correlation coefficient: %6.4f\n", rab);
		TTYprintf("    Probability of random distrib generating: %f\n", prob_random);
	}

	GVAllocReal("rank_correlation$", GVF_USER, rab);	/* Correlation coeff */
	GVAllocReal("P_random$", GVF_USER, prob_random);	/* Quality of fit		*/

	return(0);
}


/* ============================================================================
-- Subroutine for polynomial least squares fit
--
-- Usage:  LOGICAL POLFIT$(x,y,npt,order)
--
-- Inputs: X,Y,NPT - Curve to fit
--         ORDER   - Order to fit
--
-- Output: Set of coefficients CF(I), [0,n+1] for fit and FIT function
--              POLFIT$ - Ability to actually do the fit
--
-- Calls:  SPPFA, SPPSL (LINPACK linear algebra package)
--
-- Modification: X and Y coordinates normalized to [0,1] to avoid overflow
============================================================================ */
#define MAXFIT 12

static int polyfit(int order) {

/* -- Local Variables -- */
	double sumx[2*MAXFIT+1],							/* Sum X**N coefficients */
			 sumy[2*MAXFIT+1],							/* Sum Y*X**N coefficients */
			 ar[(MAXFIT+1)*(MAXFIT+2)/2],				/* Symmetric storage coeff's */
			 temp;											/* And temporary sum values */
	REAL	 xmin, xmax, ymin, ymax, ynorm, var;
	double weight,ax,bx,xtmp,ytmp,syy,det,r;

	REAL *x, *y;
	LOGICAL fSensitive;
	int  npt;
	int i,j,k,nfit,nmax,ier,num_valid, ierr;
	GVCMDS *weightcmds=NULL;

/* -- Code begin -- */
	x = GptCurve->x;									/* Local use copy */
	y = GptCurve->y;
	npt = GptCurve->npt;

	order = min(MAXFIT, max(0, order));			/* Constrain and limit */
	nfit = order+1;									/* My order (effective) */
	nmax = 2*nfit-1;									/* Number of terms to collect */

	for (i=0; i<nmax; i++) sumx[i] = 0.0;
	for (i=0; i<nfit; i++) {
		sigma[i] = 0.0f;
		sumy[i]  = 0.0;
	}
	syy = 0.0;											/* Sum of Y*Y */
	
	if (! GetFitOptions()) return(-1);					/* Do we limit, or full? */

	ArrayMinMax(x, npt, &xmin, &xmax);			/* Determine min/max values	*/
	xmin = max(xmin, RangeXmin);					/* Best range						*/
	xmax = min(xmax, RangeXmax);					/* Best range						*/

	if (xmin == xmax) {								/* ERROR!!!							*/
		ERRprintf("ERROR: Check skull for grey matter.  Same X for all data - no fit possible\n");
		return(-1);
	}

	if (order > 1) {									/* Rescale if above linear		*/
		ax = 2.0/(xmax-xmin);						/* Slope for normed data [-1,1] */
		bx = -1.0-ax*xmin;							/* Xnorm= ax*Xreal+bx			*/
		ArrayMinMax(y, npt, &ymin, &ymax);		/* Will normalize on Y also	*/
		ymin = max(ymin, RangeYmin);				/* Best range						*/
		ymax = min(ymax, RangeYmax);				/* Best range						*/
	} else {
		ax = 1.0;
		bx = 0.0;
		ymin = 0.0f;
		ymax = 1.0f;
	}
	ynorm = (ymax == ymin) ? 1 : ymax-ymin ;	/* Normalization on offset Y	*/

	if (order >= 1) {									/* Ignore on constant */
		fSensitive = (xmax-xmin) < ( fabs(xmax+xmin)/2 / pow(10,4.5/order) ) ;
		if (fSensitive && BeVerbose) ERRprintf(
			"WARNING: The coefficients reported for this polynomial fit are subject to\n"
			"         roundoff errors and should not be trusted.  This is because the X\n"
			"         coordinates have a large zero-offset compared to the overall range of\n"
			"         %g %g.  Suggest subtracting %g from X before fitting.\n",
			xmin, xmax, (xmax+xmin)/2);
	}
	weight = 1.0;                              /* In case no weighting */
	num_valid = 0;

	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ... Create the necessary sums of x**n, x**n*y, etc. */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;

		if (weightcmds != NULL) {						/* Weighting */
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr == 0) {
				if (UseSigmaWeighting) weight = 1/weight/weight;
			} else {
				weight = 1.0;
			}
		}
		xtmp = x[i]*ax+bx;								/* Scaled coordinates */
		temp = weight;
		for (j=0; j<nmax; j++) {						/* Generate sums (x**n) */
			sumx[j] += temp;
			temp    *= xtmp;
		}
		temp = weight*(y[i]-ymin)/ynorm;				/* Scaled Y							*/
		syy  += temp*temp;								/* Special sum of Y*Y			*/
		for (j=0; j<nfit; j++) {						/* Generate the sums y*(x**n) */
			sumy[j] += temp;
			temp *= xtmp;
		}
	}

	if (num_valid < nfit) {
		ERRprintf("ERROR: Please check skull for grey matter - Order of fit > # points!\n");
		return(-1);
	}

/* ... Now, split off if we have a simple or linear fit problem */
	if (order == 0) {
		cf[0] = (REAL) (sumy[0]/sumx[0]);								/* y = b					*/

	} else if (order == 1) {												/* y = mx+b				*/
		det = sumx[0]*sumx[2]-sumx[1]*sumx[1];
		if (det < 1E-35) goto SingularMatrix;
		cf[1] = (REAL) ((sumx[0]*sumy[1]-sumx[1]*sumy[0])/det);	/* m (s0*sxy-sx*sy) */
		cf[0] = (REAL) ((sumx[2]*sumy[0]-sumx[1]*sumy[1])/det);	/* b (sxx*sy-sx*sxy) */

/* ... Setup up a symmetric storage of the matrix a in solving AR*X=SUMY */
	} else {
		k = 0;
		for (i=0; i<nfit; i++) {
			for (j=0; j<=i; j++) ar[k++] = sumx[i+j];				/* x**(i+j) */
		}
		g_sppfa(ar,nfit,&ier);						/* Use LINPACK solutions */
		if (ier != 0) goto SingularMatrix;
		g_sppsl(ar,nfit,sumy);						/* Result to SUMY */

/* Correct for linear transform made on initial data. Xreal = a*x(fit) + b
   Keep CF(I) in SUX(I+1) till end so maintain double precision */
		for (i=0; i<=order; i++) sumx[i] = 0.0;		/* Clear CF(i) to start */
		sumx[0] = sumy[order];								/* Initial conditions */
		for (i=order-1; i>=0; i--) {						/* Loop down */
			for (j=order-i; j; j--)							/* Loop down */
				sumx[j] = bx*sumx[j] + ax*sumx[j-1];	/* AX+B operation */
			sumx[0] = bx*sumx[0] + sumy[i];
		}
		for (i=0; i<=order; i++) 							/* Copy & correct Y */
			cf[i] = (REAL) (sumx[i]*ynorm);				/* Final values */
		cf[0] = cf[0]+ymin;
	}

/* ---------------------------
... Calculate the residual
--------------------------- */
	xtmp = 0.0;												/* Just a summing register */
	x = GptCurve->x; y = GptCurve->y; npt = GptCurve->npt;
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		ytmp = 0.0;
		for (j=order; j>=0; j--) ytmp = ytmp*x[i] + cf[j];
		xtmp += (y[i]-ytmp)*(y[i]-ytmp);
	}

/* ... Calculate the variance and possibly the errors */
	if (num_valid != nfit) {									/* Is it possible? */
		var = (REAL) (xtmp/(num_valid-nfit));
		if (order == 0) {
			sigma[0] = (REAL) sqrt(var);
		} else if (order == 1) {
			sigma[1] = (REAL) sqrt(var*sumx[0]/det);		/* Error in slope */
			sigma[0] = (REAL) sqrt(var*sumx[2]/det);		/* Error in offset */
			r = (sumx[0]*sumy[1]-sumx[1]*sumy[0]) / sqrt(det*(sumx[0]*syy-sumy[0]*sumy[0]));
		}
	} else {
		var = 0.0f;
	}

/* ... Output to the user? */
	if (BeVerbose) {
		if (order == 0) {
			TTYprintf("   Intercept     sigma(intercept)\n"
						 "%13.6g %15.4g\n", cf[0], sigma[0]);
		} else if (order == 1) {
			TTYprintf("   Slope         Intercept       sigma(slope)  sigma(intercept)    R\n"
						 "%13.6g%15.6g   %14.4g%15.4g   %8.4f\n", 
						 cf[1],cf[0],sigma[1],sigma[0],r);
		} else {
			TTYputs(" CF$(0-n): ");
			for (i=0; i<=order; i++) TTYprintf("%13.5g", cf[i]);
			TTYprintf("\n Variance: %14.7g\n", var);
		}
	}

	GVLinkArray("cf$",			GVF_USER, cf,    nfit, NULL);	/* Real coefficients */
	GVLinkArray("sigma$",		GVF_USER, sigma, nfit, NULL);	/* Link the sigma		*/
	GVAllocReal("variance$",	GVF_USER, var);					/* Variance				*/
	GVAllocFnc ("fit(x)",		GVF_USER, "poly(x,cf$)");		/* Fit function		*/
	return(0);

SingularMatrix:
	ERRprintf("ERROR: Unable to fit data - matrix was singular\n");
	return(-1);
}



/* ============================================================================
-- Subroutine to 3D fit data to a planar surface
--
-- Usage:  LOGICAL surf_fit(void)
--
-- Inputs: GptCurve - name of curve
--
-- Output: Set of coefficients CF(I), [0,2] for fit and FIT function
--
-- Calls:  SPPFA, SPPSL (LINPACK linear algebra package)
--
-- Warning: 1. Very simple, no renormalization of x,y data buffers.
--          2. Only handles x+y+c
============================================================================ */
static int surf_fit(void) {

	REAL *x, *y, *z;
	int npt;

	int num_valid,i,ierr;

	double var;
	double s=0,sx=0,sy=0,sxx=0,syy=0,sxy=0;	/* Matrix elements */
	double b[3]={0,0,0};								/* Vector sum */
	double ar[3*4/2];									/* Real symmetric storage array */
	double tmp,weight;								/* And temporary sum values */

	GVCMDS *weightcmds=NULL;

/* -- Code begin -- */
	x = GptCurve->x;
	y = GptCurve->y;
	z = GptCurve->z;
	npt = GptCurve->npt;

	if (npt < 4 || z == NULL) {
		ERRprintf("ERROR: Surface fit requires minimum of 4 points in 3D mode\n");
		return(-1);
	}

	if (! GetFitOptions()) return(-1);			/* Do we limit, or full? */
	num_valid = 0;								/* Number of points used */
	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.        
---------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;

		if (weightcmds != NULL) {						/* Weighting			*/
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr != 0) weight = 1.0;
			if (UseSigmaWeighting) weight = 1/weight/weight;
		} else {
			weight = 1;
		}

		s   += weight;
		sx  += weight*x[i];
		sy  += weight*y[i];
		sxx += weight*x[i]*x[i];
		syy += weight*y[i]*y[i];
		sxy += weight*x[i]*y[i];

		weight = z[i]*weight;
		b[0] += weight*x[i];					/* Result vector */
		b[1] += weight*y[i];
		b[2] += weight;
	}

	if (num_valid < 4) {
		ERRprintf("ERROR: Fewer than 4 points matched the windowing criteria\n");
		return(-1);
	}

/* Fitting equation z = a_0*x^2 + a_1*y^2 + a_2*x*y + a_3*x + a_4*y + a_5 */
	ar[0] = sxx;					/* x2	*/
	ar[1] = sxy;					/* xy	*/
	ar[2] = syy;					/* y2	*/
	ar[3] = sx;						/* x	*/
	ar[4] = sy;						/* y	*/
	ar[5] = s;						/* 1	*/

	g_sppfa(ar, 3, &ierr);							/* Use LINPACK solutions	*/
	if (ierr != 0) {
		ERRprintf("Matrix was singular (huh)?\n");
		return(-1);
	}
	g_sppsl(ar, 3, b);								/* Result to b vector		*/

/* ---------------------------
... Calculate the residual
--------------------------- */
	for (var=0.0,i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		tmp = b[0]*x[i]+b[1]*y[i]+b[2];
		var += (tmp-z[i])*(tmp-z[i]);
	}
	var /= (num_valid-3);
	for (i=0; i<3; i++) cf[i] = (REAL) b[i];		/* Copy over the ocefficients */

/* ... Output to the user? */
	if (BeVerbose) {
		TTYprintf(" Surface: z = %13.5g x + %13.5g y + %13.5g\n", b[0],b[1],b[2]);
		TTYprintf("\n Variance: %14.7g\n", var);
	}

	GVLinkArray("cf$",		GVF_USER, cf, 3, NULL);		/* Coefficients */
	GVAllocReal("variance$",GVF_USER, (REAL) var);		/* Variance     */
	GVAllocFnc ("fit(x,y)", GVF_USER, "cf$[0]*x+cf$[1]*y+cf$[2]");

	return(0);
}
	

/* ============================================================================
-- Subroutine to fit data to a quadratic surface
--
-- Usage:  LOGICAL surf_fit_quad(void)
--
-- Inputs: GptCurve - name of curve
--
-- Output: Set of coefficients CF(I), [0,5] for fit and FIT function
--
-- Calls:  SPPFA, SPPSL (LINPACK linear algebra package)
--
-- Warning: 1. Very simple, no renormalization of x,y data buffers.
--          2. Only handles x^2+y^2+...+c
--
-- Very similar routines are used gpt_3d for fitting surface to data set.
============================================================================ */
static int surf_fit_quad(void) {

	REAL *x, *y, *z;
	int npt;

	double var;
	int num_valid,i,j,ierr;

	double s=0,y3x=0;							/* Deal with 1, y^3x */
	double xi[4]={0,0,0,0};					/* Deal with x^i, i=0,...,4	*/
	double yi[4]={0,0,0,0};					/* Deal with y^i, i=1,...,4	*/	
	double yxi[3]={0,0,0};					/* Deal with yx^i, i=1,2,3		*/
	double y2xi[2]={0,0};					/* Deal with y^2x^i, i=1,2		*/
	double b[6]={0,0,0,0,0,0};				/* Resultant vector				*/
	double ar[6*7/2];							/* Real symmetric storage array */
	double tmp,weight;						/* And temporary sum values */

	GVCMDS *weightcmds=NULL;

/* -- Code begin -- */
	x = GptCurve->x;
	y = GptCurve->y;
	z = GptCurve->z;
	npt = GptCurve->npt;

	if (npt < 7 || z == NULL) {
		ERRprintf("ERROR: Surface fit requires minimum of 7 points in 3D mode\n");
		return(-1);
	}

	if (! GetFitOptions()) return(-1);			/* Do we limit, or full? */
	num_valid = 0;								/* Number of points used */
	if (UseWeighting || UseSigmaWeighting) weightcmds = GVParse(Weighting, NULL);

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.        
---------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		num_valid++;

		if (weightcmds != NULL) {						/* Weighting			*/
			weight = GVTrimToDouble(GVEvalCmdsI(weightcmds, i, &ierr));
			if (ierr != 0) weight = 1.0;
			if (UseSigmaWeighting) weight = 1/weight/weight;
		} else {
			weight = 1;
		}

		s += weight;
		for (tmp=weight,          j=0; j<=3; j++) {tmp *= x[i]; xi[j]   += tmp;}
		for (tmp=weight,          j=0; j<=3; j++) {tmp *= y[i]; yi[j]   += tmp;}
		for (tmp=weight*y[i],     j=0; j<=2; j++) {tmp *= x[i]; yxi[j]  += tmp;}
		for (tmp=weight*y[i]*y[i],j=0; j<=1; j++) {tmp *= x[i]; y2xi[j] += tmp;}
		y3x += weight*y[i]*y[i]*y[i]*x[i];

		weight = z[i]*weight;
		b[0] += weight*x[i]*x[i];					/* Result vector */
		b[1] += weight*y[i]*y[i];
		b[2] += weight*x[i]*y[i];
		b[3] += weight*x[i];
		b[4] += weight*y[i];
		b[5] += weight;
	}

	if (num_valid < 7) {
		ERRprintf("ERROR: Fewer than 7 points matched the windowing criteria\n");
		return(-1);
	}

/* Fitting equation z = a_0*x^2 + a_1*y^2 + a_2*x*y + a_3*x + a_4*y + a_5 */
	ar[0] = xi[3];					/* x4		*/
	ar[1] = y2xi[1];				/* x2y2	*/
	ar[2] = yi[3];					/* y4		*/
	ar[3] = yxi[2];				/* x3y	*/
	ar[4] = y3x;					/* xy3	*/
	ar[5] = y2xi[1];				/* x2y2	*/
	ar[6] = xi[2];					/* x3		*/
	ar[7] = y2xi[0];				/* xy2	*/
	ar[8] = yxi[1];				/* x2y	*/
	ar[9] = xi[1];					/* x2		*/
	ar[10] = yxi[1];				/* x2y	*/
	ar[11] = yi[2];				/* y3		*/
	ar[12] = y2xi[0];				/* xy2	*/
	ar[13] = yxi[0];				/* xy		*/
	ar[14] = yi[1];				/* y2		*/
	ar[15] = xi[1];				/* x2		*/
	ar[16] = yi[1];				/* y2		*/
	ar[17] = yxi[0];				/* xy		*/
	ar[18] = xi[0];				/* x		*/
	ar[19] = yi[0];				/* y		*/
	ar[20] = s;						/* s		*/

	g_sppfa(ar, 6, &ierr);							/* Use LINPACK solutions	*/
	if (ierr != 0) {
		ERRprintf("Matrix was singular (huh)?\n");
		return(-1);
	}
	g_sppsl(ar, 6, b);								/* Result to b vector		*/

/* ---------------------------
... Calculate the residual
--------------------------- */
	for (var=0.0,i=0; i<npt; i++) {
		if (! InRange(x,y,i)) continue;
		tmp = b[0]*x[i]*x[i]+b[1]*y[i]*y[i]+b[2]*x[i]*y[i]+b[3]*x[i]+b[4]*y[i]+b[5];
		var += (tmp-z[i])*(tmp-z[i]);
	}
	var /= (num_valid-6);
	for (i=0; i<6; i++) cf[i] = (REAL) b[i];	/* Copy over the ocefficients */

/* ... Output to the user? */
	if (BeVerbose) {
		TTYprintf(" Surface: z = %13.5g x^2 + %13.5g y^2 + %13.5g xy +\n"
			       "              %13.5g x   + %13.5g y   + %13.5g\n",
			b[0],b[1],b[2],b[3],b[4],b[5]);
		TTYprintf("\n Variance: %14.7g\n", var);
	}

	GVLinkArray("cf$",		GVF_USER, cf, 6, NULL);		/* Coefficients */
	GVAllocReal("variance$",GVF_USER, (REAL) var);		/* Variance     */
	GVAllocFnc ("fit(x,y)", GVF_USER, "cf$[0]*x*x+cf$[1]*y*y+cf$[2]*x*y+cf$[3]*x+cf$[4]*y+cf$[5]");

	return(0);
}
	

/* ============================================================================
-- Subroutine to determine value of the knots for a SPLINE fit to a curve
--
-- Usage:  LOGICAL = SPLINE$(x,y,npt)
--
-- Inputs: x,y,npt - curve
--
-- Output: Allocates dynamic memory SPLINE_DATA to hold vectors X and 3N knots
--
-- Note: See SPL$EVAL(x) in LEXP\INTERP code (F77$CALC last time I looked)
============================================================================ */
static int spline(CURVE *cv) {

	int opts;
	BOOL rcode, smooth=FALSE, silent=FALSE;
	REAL error, *spl;									/* spl points to a big matrix */
	char token[OPTION_STR_SIZE];
	ARRAY **aptr=NULL;

/* ... Initial validity checks */
	opts = 0;
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-SMOOTH", 3)) {
			smooth = TRUE;
			error  = LexGetReal(0.1f, "RMS deviation/point: ");
		} else if (LexEqual(token, "-SILENT", 3) || LexEqual(token, "-QUIET", 2)) {
			silent = TRUE;
		} else if (LexEqual(token, "-LINEAR", 4)) {
			opts |= 0x01;								/* Do piecewise linear on spline */
		} else {
			ERRprintf("ERROR: Illegal option (%s) specified\n", token);
			return(-1);
		}
	}
	
/* If too few points, we abort anyway */
	if (cv->npt < 3) {							/* Use POLY for < 3 points */
		ERRprintf("ERROR: Too few points (%d) available for SPLINE fit\n", cv->npt);
		return(-1);
	}

	if (! GVAllocArray("SPL$DATA", GVF_USER, 5*cv->npt)) {
		ERRprintf("ERROR: Unable to allocate working space for SPLINE\n");
		return(-1);
	}
	GVGetInfo("SPL$DATA", NULL, (void **) &aptr);
	spl = (*aptr)->x;											/* Pointer to array */

	if (smooth) {
		rcode = GVFitSmoothSpline(spl, cv->x, cv->y, cv->npt, cv->npt*error*error, silent) != NULL;
	} else {
		rcode = GVFitSpline(spl, cv->x, cv->y, cv->npt, opts) != NULL;
	}
	if (! rcode) {
		ERRprintf("ERROR: Spline fit failed on current data (consider SORT -STRICT).\n");
		GVDeallocate("SPL$DATA");
		return(-1);
	} 

	GVAllocFnc("fit(x)", GVF_USER, "spline(x)");
	return(0);
}


/* ============================================================================
-- Usage: logical = GetFitOptions()
--
-- Input: none
--
-- Output: Common block data
--         xmin,xmax - limit range for the fit if requested with -XRANGE 
--                     or -RANGE option
--         UseWeighting - Is a weighting function defined?
--         BeVerbose    - Output fitting information to screen (-SILENT)
============================================================================ */
static LOGICAL GetFitOptions(void) {

	char token[OPTION_STR_SIZE];
	char condition[LONG_STR_SIZE];
	int i;

	UseWeighting		 = FALSE;			/* Are we to use weighting? */
	UseSigmaWeighting  = FALSE;			/* How about sigma errors? */
	BeVerbose			 = TRUE;
	RangeXmin			 = -REAL_MAX;		/* Very small number */
	RangeXmax			 =  REAL_MAX;		/* Very large number */
	RangeYmin			 = -REAL_MAX;		/* Very small number */
	RangeYmax			 =  REAL_MAX;		/* Very large number */

	/* Clear the -for condition command stack if it exists */
	if (for_condition_cmds != NULL) { GVFreeCmds(for_condition_cmds); for_condition_cmds = NULL; }

	while (LexGetOption(token, sizeof(token))) {
/*                              1      2      3        4        5     6      7      8      9     10      11 */
		i = LexSelect(token, "-RANGE -XRANGE -YRANGE -XYRANGE -CURSOR -FOR -WEIGHT -SIGMA -ERRY -SILENT -QUIET");
		switch (i) {
			case 1:
			case 2:
			case 3:
			case 4:
				if (i == 1 || i == 2 || i ==4) {
					RangeXmin = LexGetReal(RangeXmin, "Minimum X allowed: ");
					if (LexEscape(TRUE)) return(FALSE);
					RangeXmax = LexGetReal(RangeXmax, "Maximum X allowed: ");
					if (LexEscape(TRUE)) return(FALSE);
				}
				if (i == 3 || i == 4) {
					RangeYmin = LexGetReal(RangeYmin, "Minimum Y allowed: ");
					if (LexEscape(TRUE)) return(FALSE);
					RangeYmax = LexGetReal(RangeYmax, "Maximum Y allowed: ");
					if (LexEscape(TRUE)) return(FALSE);
				}
				break;

			case 5:
				GptSetRange();										/* Set range */
				if (BeVerbose) TTYputs("Define allowed window with cursor\n");	/* -CURSOR */
				TTYflush();
				PlotBoxCursor(&RangeXmin, &RangeYmin, &RangeXmax, &RangeYmax, NULL);
				gpt_SetBoxCursorCoords(RangeXmin, RangeYmin, RangeXmax, RangeYmax);
				break;

			case 6:
				if (! LexGetMathP(condition, sizeof(condition), "-for condition: (abort) ")) return(FALSE);
				if (LexEscape(TRUE)) return(FALSE);
				if ( (for_condition_cmds = GVChkParse(condition, NULL)) == NULL) {
					ERRprintf("ERROR: Invalid -for test function\n");
					return(FALSE);
				}
				break;

			case 7:
				UseWeighting = TRUE;
			case 8:
			case 9:
				if (! LexGetMathP(Weighting, sizeof(Weighting), "Weight/sigma function: (abort) ")) return(FALSE);
				if (LexEscape(TRUE)) return(FALSE);
				if (GVChkParse(Weighting, NULL) == NULL) {
					ERRprintf("ERROR: Illegal weight/sigma function\n");
					return(FALSE);
				}
				UseSigmaWeighting = !UseWeighting;
				break;

			case 10:
			case 11:
				BeVerbose = FALSE;
				break;

			default:
				ERRprintf("ERROR: Illegal fit option (%s)\n", token);
				return(FALSE);
		}
	}
	OrderPair(&RangeXmin, &RangeXmax);
	OrderPair(&RangeYmin, &RangeYmax);

	return(TRUE);
}

static LOGICAL	InRange(REAL *x, REAL *y, int i) {

	if (x[i] < RangeXmin || x[i] > RangeXmax || y[i] < RangeYmin || y[i] > RangeYmax) return FALSE;
	if (for_condition_cmds != NULL && GVEvalCmdsI(for_condition_cmds, i, NULL) == 0) return FALSE;
	return TRUE;
}


/* ===========================================================================
-- General use routine that others can call - basically useful
--
-- Inputs:  x,y   - input data set
--          sigma - if not NULL, standard deviation of each parameter
--          npt   - number of data points
--          order - order of the fit, 1 = linear, 2 = parabolic, ...
--
-- Output:  scaling - if not NULL, the coefficients returned in coeff refer
--                    to scaled X coordinates xs = x*scaling[1]+scaling[0]
--          coeff   - coefficients of the fit, must be size at least order+1.
=========================================================================== */
int FitPolynomial(REAL *x, REAL *y, REAL *sigma_me, int npt, int order, 
                  REAL *scaling, REAL *coeff) {

/* -- Local Variables -- */
	double sumx[2*MAXFIT+1],							/* Sum X**N coefficients */
			 sumy[2*MAXFIT+1],							/* Sum Y*X**N coefficients */
			 ar[(MAXFIT+1)*(MAXFIT+2)/2],				/* Symmetric storage coeff's */
			 temp;											/* And temporary sum values */
	REAL	 xmin, xmax, ymin, ymax, ynorm;
	double weight,ax,bx,xtmp,syy,det;

	int i,j,k,nfit,nmax,ier;

/* -- Code begin -- */
	nfit = order+1;									/* My order (effective) */
	nmax = 2*nfit-1;									/* Number of terms to collect */

/* Some sanity checks */
	if (x == NULL || y == NULL || coeff == NULL) return(-1);
	if (npt < nfit) return(-2);
	if (order > MAXFIT || order < 1) return(-3);

/* Zero the arrays */
	for (i=0; i<nmax; i++) sumx[i] = 0.0;
	for (i=0; i<nfit; i++) sumy[i] = 0.0;
	syy = 0.0;											/* Sum of Y*Y */
	
	ArrayMinMax(x, npt, &xmin, &xmax);			/* Determine min/max values */

	if (xmin == xmax) {								/* ERROR!!!							*/
		ERRprintf("ERROR: Data all have the same X values.  No fit possible.\n");
		return(-1);
	}

	if (order != 1) {									/* Rescale if not linear		  */
		ax = 2.0/(xmax-xmin);						/* Slope for normed data [-1,1] */
		bx = -1.0-ax*xmin;							/* Xnorm= ax*Xreal+bx			*/
		ArrayMinMax(y, npt, &ymin, &ymax);		/* Will normalize on Y also	*/
	} else {
		ax = 1.0;
		bx = 0.0;
		ymin = 0.0f;
		ymax = 1.0f;
	}
	ynorm = (ymax == ymin) ? 1 : ymax-ymin ;	/* Normalization on offset Y	*/

	weight = 1.0;                              /* In case no weighting */

/* ... Create the necessary sums of x**n, x**n*y, etc. */
	for (i=0; i<npt; i++) {
		if (sigma_me != NULL) weight = 1.0/pow(sigma_me[i],2);
		xtmp = x[i]*ax+bx;								/* Scaled coordinates */
		temp = weight;
		for (j=0; j<nmax; j++) {						/* Generate sums (x**n) */
			sumx[j] += temp;
			temp    *= xtmp;
		}
		temp = weight*(y[i]-ymin)/ynorm;				/* Scaled Y							*/
		syy  += temp*temp;								/* Special sum of Y*Y			*/
		for (j=0; j<nfit; j++) {						/* Generate the sums y*(x**n) */
			sumy[j] += temp;
			temp *= xtmp;
		}
	}

/* ... Now, split off if we have a linear fit problem */
	if (order == 1) {
		det = sumx[0]*sumx[2]-sumx[1]*sumx[1];
		if (det < 1E-35) goto SingularMatrix;
		coeff[1] = (REAL) ((sumx[0]*sumy[1]-sumx[1]*sumy[0])/det);	/* m= s0*sxy-sx*sy */
		coeff[0] = (REAL) ((sumx[2]*sumy[0]-sumx[1]*sumy[1])/det);	/* b= sxx*sy-sx*sxy */
		if (scaling != NULL) {								/* No scaling, even if allowed */
			scaling[0] = 0;
			scaling[1] = 1;
		}

/* ... Setup up a symmetric storage of the matrix a in solving AR*X=SUMY */
	} else {
		k = 0;
		for (i=0; i<nfit; i++) {
			for (j=0; j<=i; j++) ar[k++] = sumx[i+j];	/* x**(i+j) */
		}
		g_sppfa(ar, nfit, &ier);							/* Use LINPACK solutions */
		if (ier != 0) goto SingularMatrix;
		g_sppsl(ar, nfit, sumy);							/* Result to SUMY */

/* Correct for linear transform made on initial data. Xreal = a*x(fit) + b */
		if (scaling == NULL) {
			for (i=0; i<=order; i++) sumx[i] = 0.0;		/* Clear CF(i) to start */
			sumx[0] = sumy[order];								/* Initial conditions */
			for (i=order-1; i>=0; i--) {						/* Loop down */
				for (j=order-i; j; j--)							/* Loop down */
				  sumx[j] = bx*sumx[j] + ax*sumx[j-1];	/* AX+B operation */
				sumx[0] = bx*sumx[0] + sumy[i];
			}
			for (i=0; i<=order; i++) sumy[i] = sumx[i];	/* Get result back into sumy */
		} else {														/* Just report scaling law */
			scaling[0] = (REAL) bx;
			scaling[1] = (REAL) ax;
		}
		for (i=0; i<=order; i++)	 							/* Copy & correct Y */
		  coeff[i] = (REAL) (sumy[i]*ynorm);				/* Final values */
		coeff[0] = coeff[0]+ymin;
	}

	return(0);

SingularMatrix:
	ERRprintf("ERROR: Unable to do data - matrix singular\n");
	return(+1);
}
