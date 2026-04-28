/* subroutines from main genplot */

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
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "gptxtrn.h"
#include "gptdef.h"

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
static int InitRefl(void);
static int InitChi (void);

static int RumpUserInit(void);
static int RumpUserExit(void);
static int RumpUserRead(char *Filename, char *Curve);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */
LOGICAL	GptDefaultUserFnc  (char *UseCurve);
LOGICAL	GptDefaultUserRead (char *Filename, char *UseCurve);
LOGICAL	GptDefaultUserWrite(char *Filename, char *UseCurve);

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
static struct {
	char *name;
	LOGICAL (*UserInit) (void);
	LOGICAL (*UserExit) (void);
	LOGICAL (*UserCmd)  (int key, char *cmd, char *Curve);
	LOGICAL (*UserFnc)  (char *Curve);
	LOGICAL (*UserRead) (char *Filename, char *Curve);
	LOGICAL (*UserWrite)(char *Filename, char *Curve);
} *GptDLL, modules[] = {
	{"RUMP",	RumpUserInit, RumpUserExit, NULL, NULL, RumpUserRead, NULL},
	{"RBS",	RumpUserInit, RumpUserExit, NULL, NULL, RumpUserRead, NULL},
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};


/* ===========================================================================
-- Routines to load/free a loaded User DLL module
=========================================================================== */
int GptFreeUserDLL(LOGICAL CallExit) {

	if (CallExit && *GptUserModule && GptDLL!=NULL && GptDLL->UserExit!=NULL)
	   (*GptDLL->UserExit)();

	GptDLL       = NULL;
	GptUserCmd   = NULL;							/* Clear functions now		*/
	GptUserFnc   = GptDefaultUserFnc;		/* Reset all to defaults	*/
	GptUserRead  = GptDefaultUserRead;
	GptUserWrite = GptDefaultUserWrite;
	*GptUserModule = '\0';						/* And mark as unloaded		*/

	return(0);
}

/* ===========================================================================
-- Routine to load and initialize a user DLL module
=========================================================================== */
int GptLoadUserDLL(char *name) {

	GptFreeUserDLL(TRUE);					/* Free existing module */

/* ... Search through modules for one matching this name */
	GptDLL = modules;
	while (GptDLL->name != NULL && stricmp(GptDLL->name, name) != 0) GptDLL++;

/* ... If invalid, print error message and return */
	if (GptDLL->name == NULL) {
		GptDLL = NULL;
		ERRprintf("ERROR: %s is not among modules prebuilt for UNIX.\n", name);
		return(NOMORE);
	}

/* ... If an initialization exists, initialize the module */
	if (GptDLL->UserInit != 0) {
		if ( (*GptDLL->UserInit)() != 0 ) {
			ERRprintf("Module %s failed to successfully initialize\n");
			return(NOMORE);
		}
	}

	strcpy(GptUserModule, name);
	GptUserCmd   = GptDLL->UserCmd;
	GptUserFnc   = GptDLL->UserFnc;
	GptUserRead  = GptDLL->UserRead;
	GptUserWrite = GptDLL->UserWrite;
	return(OKAY);
}


/* ===========================================================================
-- Routine to load and initialize a user MDL module
=========================================================================== */
int GptLoadUserMDL(char *module) {

	if (stricmp(module, "refl") == 0) {
		InitRefl();
		return(OKAY);
	} else if (stricmp(module, "chi_ni") == 0) {
		InitChi();
		return(OKAY);
	} else {
		TTYputs("ERROR: This version can't load user modules\n");
		return(UNIMPLEMENTED);
	}
}


/* ===========================================================================
Biao's REFLECTIVITY calculations
=========================================================================== */

#define	pi				3.141592654				/* Guess							*/
#define	TABLESIZE	10000						/* Maximum depth valid		*/

/* -- Index of refraction functions -- */
#define	ren1(T)		(4.39+5.0e-4*(T))		/* Real/imag index for a-Si	*/
#define	imn1			(-0.61)
#define	ren2(T)		(3.72+4.88e-4*(T))	/* Real/imag index for c-Si	*/
#define  imn2			(-0.018)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
void InitVars(void);
TMPREAL refl(TMPREAL z);

TMPCOMPLEX CADD(TMPCOMPLEX a, TMPCOMPLEX b);
TMPCOMPLEX CSUB(TMPCOMPLEX a, TMPCOMPLEX b);
TMPCOMPLEX CMUL(TMPCOMPLEX a, TMPCOMPLEX b);
TMPCOMPLEX CDIV(TMPCOMPLEX a, TMPCOMPLEX b);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static	TMPCOMPLEX  one={1,0};
static	TMPCOMPLEX  n1, n2, rho01, rho12, rho02, midexpr;
static	TMPREAL     k, alpha;

static	REAL			temp, lambda;				/* Linked to GENPLOT evaluator */
static	INT			numgrid = 100;				/* # steps in convolution	*/

/* ---------------------------------------------------------------------------
-- Routine to initialize internal variables - called if anything chages
--
-- Common sub-expressions are extracted here and precomputed to save time
-- in the actual refl() routine.  Since these depend on the temperature and
-- wavelength, the values are regenerated if they have been modified.
--------------------------------------------------------------------------- */
void InitVars(void) {

	static REAL temp_hold=-1, lambda_hold=-1;
	
	if (lambda == lambda_hold && temp == temp_hold) return;

	n1.x  = ren1(temp); n1.y = imn1;	/* Complex index of film		*/
	n2.x  = ren2(temp); n2.y = imn2;	/* Complex index of substrate	*/

	alpha = -(4*pi/lambda)*n1.y;
	k     =  (2*pi/lambda)*n1.x;
	rho01   = CDIV(CSUB(one,n1), CADD(one,n1) );	/* rho01=(1-n1)/(1+n1)	 */
	rho12   = CDIV(CSUB(n1,n2),  CADD(n1,n2) );	/* rho12=(n1-n2)/(n1+n2) */
	rho02   = CDIV(CSUB(one,n2), CADD(one,n2) );	/* rho02=(1-n2)/(1+n2)	 */
	midexpr = CMUL(CSUB(one, CMUL(rho01,rho01)), rho12);

	temp_hold   = temp;
	lambda_hold = lambda;
	return;
}

/* ---------------------------------------------------------------------------
-- Actual routine to calculate the reflectivity and return only real value.
--
-- Most of the "pre-calculations" are done in InitVars routine.  It will
-- return if nothing has changed since last time.
--------------------------------------------------------------------------- */
TMPREAL refl(TMPREAL z) {

	TMPCOMPLEX tmp;

	if (z < 0.0) z = 0.0;							/* Limit it to zero			*/
	InitVars();											/* Make sure initialized	*/
	
	tmp.x  = cos(-2*k*z);							/* Phase factor e^ikz		*/
	tmp.y  = sin(-2*k*z);	
	tmp    = CMUL(midexpr, tmp);					/* Continue expr from here */
	tmp.x  = tmp.x*exp(-alpha*z) + rho01.x;
	tmp.y  = tmp.y*exp(-alpha*z) + rho01.y;

	return(tmp.x*tmp.x + tmp.y*tmp.y);			/* |R|^2 */
}
	
/* ---------------------------------------------------------------------------
-- External function to be linked into the function evaluator
--
-- Usage: int <fnc>(int itype, TMPREAL *result, TMPREAL *args);
--
-- Inputs: itype -  0 ==> Real call,    result & args are    TMPREAL *
--                  1 ==> Complex call, result & args really TMPCOMPLEX *
--         result - pointer to where result should be stored
--         args   - pointer to array of arguments from call
--
--
-- Returns: 0 ==> everything is okay
--         !0 ==> function is not implemented (only valid for type=1)
--
-- Notes: The same call is used for both REAL format and COMPLEX function
--        evaluations.  The pointers result and args are typecast to TMPREAL *
--        but are really TMPCOMPLEX * if itype = 1.  
--
--        If function does not want to deal with complex arguments, return
--        -1 and function will be recalled with the real part of each arg
--        only, and will set the real part of a complex as the result.
--
-- Call Sequence: GVLinkFnc("name", int flags, int nargs, &fnc));
--                GVLinkFnc("square", 0, 1, &square);
--
--------------------------------------------------------------------------- */
static int linkrefl(int itype, TMPREAL *result, TMPREAL *args) {
	if (itype != 0) return(-1);				/* Don't handle complex */
	*result = refl(*args);
	return(0);
}

/* ---------------------------------------------------------------------------
-- Gaussian roughness.  Probability of interface at position z given mean
-- interface at z0 and sigma of s is
--
--   P(z) =  A exp(-0.5*((z-z0)/s)^2)
--
-- Finite roughness (1%) exists over range of [-3s,3s]
--------------------------------------------------------------------------- */
static int grough(int itype, TMPREAL *result, TMPREAL *args) {

	TMPREAL z, z0, width, wgt, wgtsum, rflsum, dw;

	int i;
	
	if (itype != 0) return(-1);

	z0 = args[0]; width = args[1];					/* Center, roughness */

	rflsum = 0; wgtsum = 0;

	for (i=0; i<numgrid; i++) {
		dw = -3.0 + 6.0*i/(numgrid-1.0);				/* Convert to interval [-3/3] */
		wgt = exp(-dw*dw/2);
		z = z0+dw*width;
		wgtsum += wgt;
		rflsum += refl(z)*wgt;
	}
	*result = rflsum / wgtsum;
	return(0);
}

/*============================================================================
-- User function initialization procedure.  If defined, this routine is
-- called just after the module is loaded.  Run time per-instance
-- initialization should be performed by this routine.  At minimum, the
-- routine should print a message indicating successful load and version.
--
-- Usage:  Initialize the dynamic link module (local control)
--
-- Syntax: int UserInit(void);
--
-- Inputs: none
--
-- Output: local control only
--
-- Returns:  0 ==> all is okay
--          !0 ==> error, abort and free this module
--
-- Notes: UserInit() will be called before any other procedures in the module
============================================================================ */
static int InitRefl(void) {

	TTYprintf("Dynamic user module for complex reflectivity loaded successfully\n");
	TTYprintf("Linking refl(z):        %d\n", GVLinkFnc("REFL",  GVF_USER, 1, linkrefl));
	TTYprintf("Linking rough(z,sigma): %d\n", GVLinkFnc("ROUGH", GVF_USER, 2, grough));
	TTYprintf("Linking Temperature:    %d\n", GVLinkReal("Temperature", GVF_USER, &temp));
	TTYprintf("Linking Lambda:         %d\n", GVLinkReal("Lambda",      GVF_USER, &lambda));

	temp    = LexGetReal(600.0, "Temperature (600 C): ") + 273;
	lambda  = LexGetReal(6328.0, "Laser wavelength (6328 nm): ");
	numgrid = LexGetInt(20,      "# divisions in roughness calcs (20): ");

	InitVars();
	
	printf("/* T       = %f (K)\n", (double) temp);
	printf("/* n1,n2   = %f %f   %f %f\n", n1.x, n1.y, n2.x, n2.y);
	printf("/* alpha,k = %f %f\n", (double) alpha, (double) k);

	return(0);
}

/* ===========================================================================
-- Complex addition, subtraction, etc.
=========================================================================== */
TMPCOMPLEX CADD(TMPCOMPLEX a, TMPCOMPLEX b) {
	TMPCOMPLEX result;
	result.x = a.x+b.x;
	result.y = a.y+b.y;
	return(result);
}
	
TMPCOMPLEX CSUB(TMPCOMPLEX a, TMPCOMPLEX b) {
	TMPCOMPLEX result;
	result.x = a.x-b.x;
	result.y = a.y-b.y;
	return(result);
}
	
TMPCOMPLEX CMUL(TMPCOMPLEX a, TMPCOMPLEX b) {
	TMPCOMPLEX result;
	result.x = a.x*b.x - a.y*b.y;
	result.y = a.x*b.y + a.y*b.x;
	return(result);
}

TMPCOMPLEX CDIV(TMPCOMPLEX a, TMPCOMPLEX b) {
	TMPCOMPLEX result;
	TMPREAL magn;
	magn = b.x*b.x+b.y*b.y;
	result.x =  ( a.x*b.x + a.y*b.y) / magn ;
	result.y =  (-a.x*b.y + a.y*b.x) / magn ;
	return(result);
}


/* ===========================================================================
-- Routines for Dieckmann's group
=========================================================================== */
typedef struct _MY_SOLVE_PARMS {
	double LowerBound, UpperBound;			/* Bound on root position		*/
	double epsilon;								/* Precision required			*/
	int    MaxIterate;							/* Maximum # of iterations		*/
	double (*Fnc)(double x, int *ierr);		/* Function commands				*/
	double root;									/* Initial/final guess			*/
	int	 iter;									/* Number of iterations used	*/
	int	 rc;										/* Return code (0 ==> okay)	*/
} MY_SOLVE_PARMS;

int chi_ni(int type, TMPREAL *result, TMPREAL *args);
static TMPREAL MyFindZero(MY_SOLVE_PARMS *parms);

/*============================================================================
-- User function initialization procedure.  This routine is called as the
-- module is loaded.  Run time per-instance initialization should be
-- performed by this routine.  At minimum, the routine should print a
-- message indicating successful load and version.  May also set a global
-- parameter to prevent reloading.
--
-- Usage:  Initialize the dynamic link module (local control)
--
-- Syntax: int Init(void);
--
-- Inputs: none
--
-- Output: local control only
--
-- Returns:  0 ==> all is okay
--          !0 ==> error, (but GENPLOT will ignore it!)
============================================================================ */
static int InitChi(void) {

	if (! GVLinkFnc("CHI_NI", GVF_USER, 3, chi_ni)) {		/* 3 arguments */
		ERRprintf("Failed to link routine CHI_NI() - GVLinkFnc() returned error");
		return(-1);
	} else {
		TTYprintf("Dynamic user module for chi_ni(a,b,a_o2) loaded successfully\n");
		TTYputs("\nThis routine implements the function chi_ni(a,b,a_o2) which\n"
			     "returns the solution to the transcendental function\n"
				  "       x = a*exp(b*x)*a_o2\n"
			     "as long as x lies in the interval [0,1].\n\n"
				  "Cost: One bottle wine.\n");
	}
	return(0);
}

/* ============================================================================
-- External function to be linked into the function evaluator
--
-- Usage: int <fnc>(int itype, TMPREAL *result, TMPREAL *args);
--
-- Inputs: itype -  0 ==> Real call,    result & args are    TMPREAL *
--                  1 ==> Complex call, result & args really TMPCOMPLEX *
--         result - pointer to where result should be stored
--         args   - pointer to array of arguments from call
--
-- Returns: 0 ==> everything is okay
--         !0 ==> function is not implemented (only valid for type=1)
--
-- Notes: The same call is used for both REAL format and COMPLEX function
--        evaluations.  The pointers result and args are typecast to TMPREAL *
--        but are really TMPCOMPLEX * if itype = 1.  
--
--        If function does not want to deal with complex arguments, return
--        -1 and function will be recalled with the real part of each arg
--        only, and will set the real part of a complex as the result.
--
-- Call Sequence: GVLinkFnc("name", int flags, int nargs, &fnc));
--                GVLinkFnc("square", 0, 1, &square);
--
-- ---------------------------------------------------------------------------
-- Routine whose root is to be found is passed as argument to MyFindZero.  
-- It is called with a single argument X.  All other parameters must be 
-- handled by global variables established prior to ZeroFind call.
--
-- Usage: double = fnc(double x, int *ierr);
--
-- Inputs: x - guess for the root coordinate
--
-- Output: *ierr - set to 0 if no error,
--                 non-zero ==> function cannot be evaluated
--
-- Returns: Function value.  FindZero will try to get this to zero.
============================================================================ */
static double	a_arg,					/* Common arguments between calling */
					b_arg,					/* routine chi_ni and the evaluation */
					act_o2;					/* routine my_problem					 */

/* ===========================================================================
-- Routine to evaluate function f(x) = a_arg*exp(b_arg*x)*act_o2 - x
=========================================================================== */
static double my_problem(double x, int *ierr) {

	double value;

	*ierr = 0;													/* Never any error */
	value = a_arg * exp(b_arg * x) * act_o2 - x ;	/* Function to be zeroed */
	return(value);
}

/* ===========================================================================
-- Main chi_ni "handler" routine to be linked into GENPLOT function evaluator 
=========================================================================== */
int chi_ni(int type, TMPREAL *result, TMPREAL *args) {

	MY_SOLVE_PARMS parms;								/* Parameters to FindRoot() */

	if (type != 0) return(-1);							/* Don't deal w/ complex	*/

/* --------------------------------------------------------------------------
-- Set up the MY_SOLVE_PARMS parameter block.  All the information must be in
-- this structure.  Root must exist between LowerBound and UpperBound.  Root
-- may be set LowerBound if no better guess available.  The Epsilon parameter
-- may be set to 4*REAL_EPSILON if REAL  precision is adequate.  MaxIterate 
-- may be set quite large, typically 100.  Normally, only 8-10 iterations 
-- are necessary to converge.
-------------------------------------------------------------------------- */
	parms.LowerBound = 0;								/* Lower bounding limit		*/
	parms.UpperBound = 1.0;								/* Upper bounding limit		*/
	parms.epsilon    = 4*REAL_EPSILON;				/* Quit when delta < eps	*/
	parms.MaxIterate = 100;								/* Abort after n attempts	*/
	parms.Fnc        = my_problem;					/* Function routine			*/
	parms.root       = 0;								/* Best initial guess		*/

	a_arg = args[0];										/* First argument given		*/
	b_arg = args[1];										/* Second argument given	*/
	act_o2 = args[2];										/* Third arguemnt given		*/
	*result = MyFindZero(&parms);

/* I'm responsible for printing error messages if appropriate */
	if (parms.rc == -1) gen_err("Root not bracketed by specified range (chi_ni)");
	if (parms.rc == -2) gen_err("Function could not be evaluated (chi_ni)");
	if (parms.rc == -3) gen_err("No solution found in maximum iterations (chi_ni)");

#ifdef DEBUG
	if (parms.rc == 0)
		TTYprintf("Root %g found in %i iterations\n", parms.root, parms.iter);
#endif

	return(0);												/* Must return okay here	*/
}

/* ===========================================================================
-- Van Wijngaarden-Dekker-Brent Method
--
-- Given a function f(x) and a range [xl,xh] containing a root, determines
-- the zero of the function using a combination of bisection, secant and
-- false position methods.
--
-- Reference: Press et.al Sec 9.3ff
--
-- Returns:  0  ==> Root successfully found
--          -1  ==> Root not bracketed within interval
--          -2  ==> Invalid function
--          -3  ==> No solution found within maxiter iterations
=========================================================================== */
static TMPREAL MyFindZero(MY_SOLVE_PARMS *parms) {

	int    iter=0, ierr, rcode;

	TMPREAL a,b,c;					/* Coordinates										*/
	TMPREAL fa,fb,fc;				/* Function value at a,b,c						*/
	TMPREAL delta,					/* Correction to root estimate				*/
		     l_delta;				/* Last correct to root estimate				*/
	TMPREAL xm;						/* Bounding interval width						*/
	TMPREAL P,Q,R,S,T;			/* Arguments for quadratic interpolation	*/

	TMPREAL tol1, tol=1000*REAL_MIN;						/* REALLY IS REAL_MIN */

	a = min(parms->LowerBound, parms->UpperBound);	/* Start a < c as limits */
	c = max(parms->LowerBound, parms->UpperBound);
	b =     parms->root;										/* And b is initial guess */
	if (b < a || b > c) b = a;								/* But limit to valid	  */

	fa = (*parms->Fnc)(a, &ierr);							/* Lower limit value	*/
	if (ierr == 0) fc = (*parms->Fnc)(c, &ierr);		/* And upper value	*/

	if (ierr != 0)	{rcode = -2;        goto AllReturn;}	/* Evaluate error? */
	if (fa == 0)	{rcode =  0; b = a; goto AllReturn;}	/* Root at edge?	 */
	if (fc == 0)	{rcode =  0; b = c; goto AllReturn;}	/* Other edge?		 */
	if (fa*fc > 0) {rcode = -1;        goto AllReturn;}	/* No root?			 */

	if (b > a && b < c) {											/* Check guess		 */
		fb = (*parms->Fnc)(b, &ierr);
		if (ierr != 0) {rcode = -2; goto AllReturn;}
	} else {																/* Else, use upper */
		b  = c;
		fb = fc;
	}

/* Now, start the iteration process */
	for (iter=1; iter<=parms->MaxIterate; iter++) {

/* ... Keep root between b&c.  If not, move a to c and continue linear */
		if (fb*fc > 0) {						/* b&c same sign, dump c */
			c = a; fc = fa;					/* Leaves values a,b,a	 */
			l_delta = delta = b-a;			/* Last change intervals */
		}
/* ... b must be close to root than c.  If not, exchange b/c and forget a */
		if (fabs(fc) < fabs(fb)) {			/* Put nearest value in center */
			a = b; fa = fb;
			b = c; fb = fc;
			c = a; fc = fa;
		}
		xm = (c-b);								/* Interval width		*/

/* ... Check the convergence criteria, possibly done or minimal change? */
		tol1 = parms->epsilon*fabs(b) + tol;
		if ( (fabs(xm) <= tol1) || (fb == 0) ) break;	/* Are we done? */

/* Attempt inverse quadratic (linear) as long as b is closer to root than a */
		if ( (fabs(l_delta) >= tol1/2) && (fabs(fa) > fabs(fb)) ) {
			S = fb/fa;							/* Notation of eqns 9.3.1ff		*/
			if (a == c) {						/* Two point secant method only	*/
				P = xm*S;						/* Next estimate is x + P/Q		*/
				Q = S-1;
			} else {								/* Three point interpolation		*/
				T = fa/fc;						/* Notation of eqns 9.3.1ff		*/
				R = fb/fc;
				P = S*(xm*T*(R-T)-(b-a)*(1-R));
				Q = (T-1)*(R-1)*(S-1);
			}
			if (P<0) {Q=-Q; P=-P;}			/* Invert so compare below safe	*/
			if (2*P < min(3*xm*Q-fabs(tol1*Q/2), fabs(l_delta*Q))) {
				l_delta = delta;				/* Save old for superlinearity	*/
				delta = P/Q;					/* Accept as going superlinear	*/
			} else {
				l_delta = delta = xm/2;		/* Interpolate failed, bisect		*/
			}
		} else {									/* Limit changing slowly, bisect	*/
			l_delta = delta = xm/2;
		}
		a  = b; fa = fb;						/* Move previous best guess to a */
		if (fabs(delta) > tol1/2) {		/* Estimate large enough, do it! */
			b = b+delta;
		} else {									/* Correction too small			 */
			if (xm <  0) b = b-tol1/2;		/* add tol1/2*sign(xm) instead */
			if (xm >= 0) b = b+tol1/2;
		}
		fb = (*parms->Fnc)(b, &ierr);		/* New guess						*/
		if (ierr != 0) break;
	}

	if (iter >= parms->MaxIterate) {		/* Potential error returns */
		rcode = -3;
	} else if (ierr != 0) {					/* Bad function				*/
		rcode = -2;
	} else {										/* All successful				*/
		rcode = 0;
	}

AllReturn:
	parms->rc   = rcode;						/* Return error code					*/
	parms->iter = iter;						/* Return number of iterations	*/
	parms->root = b;							/* Best guess for the root			*/
	return(b);									/* And return error code	*/
}


/* ===========================================================================
-- (1) The SPECTRUM structure contains all of the information concerning
--     the spectrum.  Recommend using these routines to manipulate the
--     buffer instead of local malloc() since variable may not be initialized.
--     If not, new elements added to this structure may make old programs fail.
-- (2) RumpReadSpectrum will allocate everything is you want it to.
-- (3) For RumpWriteSpectrum, all elements of RUMP_SPECTRUM must be valid and
--     defined.  No checking.
-- (4) If the number of channels is specified as 0 or negative in 
--     RumpAllocateSpectrum, the count buffer will be left NULL, allowing
--     automatic allocation later.  All other components are initialized.
--
-- Definition of MCA properties.
--     o counts[i] (0<=i<npt) stores all events with energy in the window
--           E[i]-scale1/2 < E < E[i]+scale1/2
--     o The center of energy window for E[i] is
--           E[i] = (i + ALTBUF->first) * scale1 + scale2
=========================================================================== */
#define	IDCHR	80					/* Maximum size of identifiers */

typedef enum _SPECTRUM_TYPE {RBS, FRES, PIXE, NUCLEAR, OTHER} SPECTRUM_TYPE;
typedef enum _GEOMETRY_TYPE {
	CORNELL =  0,					/* Cornell theta/phi definitions	*/
	IBM     =  1,					/* IBM theta/phi definitions		*/
	GENERAL = -1					/* Full theta/phi/psi definition	*/
} GEOMETRY_TYPE;

typedef struct _SPECTRUM {
	char filename[PATH_MAX];				/* Filename                         */
	char date[IDCHR];							/* Date spectrum collected          */
	char ltct[IDCHR];							/* Live time/Clock time information */
	char id[IDCHR];							/* Identifier string                */
	SPECTRUM_TYPE type;						/* Type of spectrum						*/
	REAL e0;										/* Incident energy            (MeV) */
	int  zbeam;									/* Atomic Z of incident             */
	REAL mbeam;									/* Atomic mass of incident    (amu) */
	int  cbeam;									/* |Charge| state of beam           */
	REAL q;										/* Total accumulated charge    (uC) */
	REAL current;								/* Average beam current        (nA) */
	REAL scale1,scale2;						/* Conversion MCA chan # -> keV     */
	REAL first;									/* Channel number of first data pt  */
	REAL fwhm;									/* Detector resolution        (keV) */
	REAL tau;									/* MCA shaping time constant   (uS) */
	GEOMETRY_TYPE geom;						/* Geometry identifier              */
	REAL phi,theta,psi;						/* Scattering angles      (degrees) */
	REAL omega;									/* Detector solid angle       (mSr) */
	REAL corr;									/* Random correction factor         */
	int  nspectra;								/* Number of spectra in data			*/
	int  npt;									/* Number of data points            */
	int  nptmax;								/* Dimensioned size of counts			*/
	REAL *counts;								/* Pointer to actual data           */
	int  dirty;									/* Has data changed since read		*/
	int  modify;								/* Have parameters been modified		*/
	int  iddone;								/* Is ID been done for this spectra	*/
} SPECTRUM ;

static	SPECTRUM *RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels);
static	     int	 RbsFreeSpectrum(SPECTRUM *spectra);
static   SPECTRUM *RbsReadSpectrum(char *file, SPECTRUM *spectra, int *err);

#define VERSION_ID "RUMP data via POSIX C [v 1.1 - 8/1/94 MOT]"

#define RUMP_ID          0x10211210L				/* Rump ID string       */
#define MAJOR_REV_LEVEL  0x0001                 /* Major revision level */
#define MINOR_REV_LEVEL  0x0001                 /* Minor revision level */

#define PROGRAM_ID_REC   0x00000000             /* Record types        */
#define COMMENT_REC      0x00000001             /* Comment             */
#define SILENT_REC       0x00000002             /* Silent comment      */
#define DATA_INIT_REC    0x00000010             /* Start data records  */
#define ARRAY_INIT_REC	 0x00000020             /* Start array records */
#define DATA_REC         0x00000011             /* Actual data records */
#define DATA_REC_0		 0x00000012					/* Data in compress 0  */
#define DATA_REC_1		 0x00000013					/* Data in compress 1  */
#define DATA_REC_2		 0x00000014					/* Data in compress 2  */
#define DATA_REC_3		 0x00000015					/* Data in compress 3  */
#define ID_REC           0x00000101             /* ID string record    */
#define LTCT_REC         0x00000102             /* LTCT string record  */
#define DATE_REC         0x00000103             /* DATE string record  */
#define CORR_REC         0x00000110             /* Correction factor   */
#define ACCEL_REC        0x00000111             /* Accelerator record  */
#define MCA_REC          0x00000112             /* MCA parameters      */
#define RBS_REC          0x00000120             /* RBS parameters      */
#define FRES_REC         0x00000121             /* FRES parameters     */
#define PIXE_REC         0x00000122             /* PIXE parameters     */
#define NREAC_REC        0x00000123             /* Nuclear reactions   */

/* Space needed for buffer to hold at least one block */
#define	BUFFER_SPACE_NEEDED	4*1027		/* 1024 plus checksum longs */
#define	BUFFER_WRITE_NEEDED	4*1027+7		/* Extra for compress overwrite */

#ifdef DEBUG_MODE
	static struct {
		int id;
		char *name;
	} types[] = {	{PROGRAM_ID_REC,	"ID record"},
						{COMMENT_REC,		"Comment record"},
						{SILENT_REC,		"Silent comment"},
						{DATA_INIT_REC,	"Data start record"},
						{ARRAY_INIT_REC,	"Array start record"},
						{DATA_REC,			"Data record"},
						{DATA_REC_0,		"Data record (compression 0)"},
						{DATA_REC_1,		"Data record (compression 1)"},
						{DATA_REC_2,		"Data record (compression 2)"},
						{DATA_REC_3,		"Data record (compression 3)"},
						{ID_REC,				"Ident record"},
						{LTCT_REC,			"LTCT record"},
						{DATE_REC,			"Date record"},
						{CORR_REC,			"Correction record"},
						{ACCEL_REC,			"Accelerator record"},
						{MCA_REC,			"MCA record"},
						{RBS_REC,			"RBS record"},
						{FRES_REC,			"FRES record"},
						{PIXE_REC,			"PIXE record"},
						{NREAC_REC,			"NUCL record"},
						{-1,					"Unknown record"} };

	static char *gettype(int type) {
		int i;
		for (i=0; types[i].id != -1; i++) {
			if (types[i].id == type) break;
		}
		return(types[i].name);
	}
#endif

#ifdef LOCAL_MODE
	#define	ERRprintf	printf
	#define	TTYprintf	printf
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static  int   read_record(int *item_cnt);
static  void  get_str(char *result, size_t len);
static  INT32  get_int32(void);
static  INT16  get_int16(void);
static  REAL get_real(void);

static  int read_data_records(REAL *counts, int npt, int compress, char **errmsg);
static  int read_compress(REAL *counts,int npt,int *npt_read);

static  int check_c_reals(void);
static  UINT32 MakeCheckSum(int num_elem);

static int UnZeroCompress(void *data, int length);

#if defined(CONVERT_IEEE)
 static  REAL32 ieee_to_float(UINT32 value);
 static  UINT32 float_to_ieee(REAL32 value);
#endif

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static FILE *handle = NULL;						/* Handle to open file      */
static unsigned char *buffer, *bufptr;			/* Buffer/pointer to buffer */


/* ===========================================================================
-- Function to allocate space for a spectrum structure, and initialize
--
--  Usage:  int RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels);
--
--  Inputs: proto       - prototype for spectrum header information if
--                        not NULL.  Defaults used otherwise.
--          NumChannels - number of channels for which to allocate space.
--                         if 0 or negative, buf->counts left NULL.
--
--  Output: Pointer to structure properly initialized or NULL if failed.
=========================================================================== */
static SPECTRUM *RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels) {
	
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
		buf->scale1  = 4.0f;							/* 4 keV/channel				*/
		buf->scale2  = 0.0f;							/* Perfect system				*/
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
-- Function to allocate space for a spectrum structure, and initialize
--
--  Usage:  int RbsFreeSpectrum(SPECTRUM *buf);
--
--  Inputs: *buf - Rump spectra structure
--
--  Output: Clears all memory used by that spectra.  buf invalid on return.
=========================================================================== */
static int RbsFreeSpectrum(SPECTRUM *buf) {
	
	if (buf != NULL) {
		if (buf->counts != NULL) free(buf->counts);
		free(buf);
	}
	return(0);
}

/* ===========================================================================
-- Function to read structure from disk
--
--  Usage:  SPECTRUM *RbsReadSpectrum(char *filename, SPECTRUM *spectra, int &err);
--
--  Inputs: filename - file to be read
--          spectra  - pointer to spectrum.  If NULL, one will be allocated.
--
--  Output: err - If not NULL, error code on return.
--                0 - successful read of data
--                1 - unable to open file
--                2 - unable to allocate buffer for reading
--               -3 - unable to allocate buffers space for counts
--               -2 - bad file format from the beginning
--               -1 - bad file format somewhere during reading (after headers)
--
--  Returns: pointer to spectra, or NULL if fatal errors
--
--  Notes:  The spectra->counts pointer is handled differently depending
--          on its initial value.  If spectra->counts is initially NULL
--          pointer, then sufficient space is allocated to hold the
--          incoming data.  However, if not, the data is loaded into the
--          existing buffer if spectra->nptmax is adequate.  Otherwise, the
--          existing buffer if free()'d, and new space allocated.
=========================================================================== */
static SPECTRUM *RbsReadSpectrum(char *filename, SPECTRUM *spectra, int *err) {

	struct {												/* Version numbers */
		int major;
		int minor;
	} vers;

	int npt, compression, record_type, item_cnt;
	char result[80];
	char *errmsg;
        
	check_c_reals();									/* Check format #'s	*/

/* Allocate buffer space and open file  */
	if ( (handle=fopen(filename,"rb")) == NULL) {
		if (err != NULL) *err = 1;
		return(NULL);
	} else if ( (buffer = malloc(BUFFER_SPACE_NEEDED)) == NULL) {
		fclose(handle);
		if (err != NULL) *err = 2;
		return(NULL);
	} else if (spectra == NULL && (spectra=RbsAllocateSpectrum(NULL, 0)) == NULL) {
		fclose(handle);
		free(buffer);
		if (err != NULL) *err = 3;
		return(NULL);
	}
	strscpy(spectra->filename, filename, sizeof(spectra->filename));

/* Check for valid RUMP format */
	if ((record_type = read_record(NULL)) < 0) goto bad_file;
	if (record_type != PROGRAM_ID_REC)         goto bad_file;
	if (get_int32()  != RUMP_ID)                goto bad_file;

	vers.major = get_int16();
	vers.minor = get_int16();
	if (vers.major != MAJOR_REV_LEVEL) {
		ERRprintf("WARNING: Major revision level for file is different\n");
	} else if (vers.minor > MINOR_REV_LEVEL) {
		ERRprintf("WARNING: Minor revision is beyond this read code\n");
	}
/*	printf("RBS file header.  [v. %d.%2.2d]\n", vers.major, vers.minor); */

/* Read records until we get the DATA record */
	while (TRUE) {
		if ((record_type = read_record(&item_cnt)) < 0) {
			errmsg = "Premature end of data file\n";
			goto bad_data_file;
		}
		switch(record_type) {
			case PROGRAM_ID_REC:				/* program specifier record			*/
				if (get_int32() != RUMP_ID) {
					errmsg = "Invalid RUMP ID record\n"; 
					goto bad_data_file;
				}
				break;
			case COMMENT_REC:					/* printed comment record				*/
				get_str(result,sizeof(result));
				TTYprintf("%s\n",result);
				break;
			case SILENT_REC:					/* unprinted comment record			*/
				break;
			case ID_REC:						/* id string record						*/
				get_str(spectra->id, sizeof(spectra->id));
				break;
			case LTCT_REC:						/* live time/clock time record		*/
				get_str(spectra->ltct, sizeof(spectra->ltct));
				break;
			case DATE_REC:						/* date record								*/
				get_str(spectra->date, sizeof(spectra->date));
				break;
			case CORR_REC:						/* correction factor record			*/
				spectra->corr = get_real();			/* Correction value			*/
				break;
			case ACCEL_REC:					/* accelerator parms record			*/
				spectra->e0   =       get_real();	/* Beam Energy (MeV)			*/
				spectra->zbeam= (int) get_int32();	/* Z of incident beam		*/
				spectra->mbeam=       get_real();	/* Mass of incident beam	*/
				spectra->cbeam= (int) get_int32();	/* Charge of incident beam	*/
				spectra->q    =       get_real();	/* Total integrated charge	*/
				spectra->current =    get_real();	/* Beam current				*/
				break;
			case MCA_REC:						/* mca parms record						*/
				spectra->scale1  =  get_real();		/* keV/channel on MCA		*/
				spectra->scale2  =  get_real();		/* keV of channel 0			*/
				spectra->first   =  get_real();		/* Starting channel on MCA	*/
				spectra->fwhm    =  get_real();		/* Detector resolution		*/
				if (item_cnt > 4)
					spectra->tau  =  get_real();		/* MCA shaping constant		*/
				break;
			case RBS_REC:						/* Normal RBS scattering record		*/
				spectra->type  = RBS;					/* Now sure we are RBS     */
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle) */
				spectra->phi   = get_real();			/* (180-scattering angle)  */
				spectra->psi   = get_real();			/* (exit target angle)     */
				spectra->omega = get_real();			/* Detector solid angle    */
				break;
			case FRES_REC:						/* forward recoil record				*/
				spectra->type  = FRES;					/* Now sure we are FRES		*/
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle)	*/
				spectra->phi   = get_real();			/* (180-scattering angle)	*/
				spectra->psi   = get_real();			/* (exit target angle)		*/
				spectra->omega = get_real();			/* Detector solid angle		*/
				break;
			case PIXE_REC:							/* xray emission record				*/
				spectra->type  = PIXE;					/* Now sure we are PIXE		*/
				spectra->geom  = (GEOMETRY_TYPE) get_int32();
				spectra->theta = get_real();			/* (incident target angle)	*/
				spectra->phi   = get_real();			/* (180-scattering angle)	*/
				spectra->psi   = get_real();			/* (exit target angle)		*/
				spectra->omega = get_real();			/* Detector solid angle		*/
				break;
			case DATA_INIT_REC:					/* data initialization record		*/
			case ARRAY_INIT_REC:					/* array initialization record	*/
				compression  = (int) get_int32();		/* Type compression	*/
				if (compression < 0 || compression > 3) {
					errmsg = "Unrecognized data compression format\n";
					goto bad_data_file;
				}

/* Get the number of points per spectrum and # of spectrum stored */
				spectra->npt      = (int) get_int32();
				spectra->nspectra = (record_type == ARRAY_INIT_REC) ? get_int32() : 1 ;

				npt = spectra->npt * spectra->nspectra;		/* Number needed in data */

				if (spectra->counts == NULL || npt > spectra->nptmax) { 
					if (spectra->counts != NULL) free(spectra->counts);
					spectra->nptmax = 0;
					spectra->counts = calloc(npt, sizeof(*spectra->counts));
					if (spectra->counts == NULL) {
						ERRprintf("HOLY SHIT: I can't believe I'm out of memory!\n");
						free(buffer);
						fclose(handle);
						if (err != NULL) *err = -3;
						return(NULL);
					}
					spectra->nptmax = npt;
				}

				if (read_data_records(spectra->counts, npt, compression, &errmsg) != 0)
					goto bad_data_file;
				
				spectra->dirty  = FALSE;			/* Now clean					*/
				spectra->modify = FALSE;			/* Parameters unchanged		*/
				free(buffer);							/* Close down and go!		*/
				fclose(handle);
				if (err != NULL) *err = 0;
				return(spectra);

			case DATA_REC:							/* Data record (not allowed!!)	*/
			case DATA_REC_0:
			case DATA_REC_1:
			case DATA_REC_2:
			case DATA_REC_3:
				errmsg = "Data record read before initialized\n";
				goto bad_data_file;

			case NREAC_REC:						/* nuclear reaction record			*/
				spectra->type = NUCLEAR;
				ERRprintf("Nuclear reaction records type not implemented yet\n");
				break;

			default:									/* don't recognize					*/
				ERRprintf("Unimplemented record type: %8.8lx\n",record_type);
		}
	}

bad_data_file:
	ERRprintf("ERROR: %s\n", errmsg);
	free(buffer);
	fclose(handle);
	if (err != NULL) *err = -1;
	return(NULL);

bad_file:
	free(buffer);
	fclose(handle);
	if (err != NULL) *err = -2;
	return(NULL);
}

/* ===========================================================================
-- Function to read the next record off disk into rump_record
--
-- Usage:  int = read_record(int *item_cnt)
--
-- Inputs: none
--
-- Output: item_cnt - If not NULL, number of 4-byte data items in record
--
-- Returns: read_record - +n ==> Record type N
--                        -1 ==> Bad byte count
--                        -2 ==> Bad checksum
--
-- Action:  Reads next record from open disk file into buffer.
=========================================================================== */
static int read_record(int *item_cnt) {

	unsigned long CheckSum=0;						/* Check sum counter    */
	size_t NumRead, NumWant;						/* Number read, want    */
	int RecordLength, RecordType;					/* Record length (read) */
	int rcode=0;										/* Return code				*/

/* Read record length and type */
	if ( (NumRead = fread(buffer, 4, 2, handle)) != 2) return(-1);
	bufptr  = buffer;									/* Set pointer for get_xxxx */
	RecordLength = (int) get_int32();				/* First element is length  */
	RecordType   = (int) get_int32();				/* Followed by record type  */
	CheckSum     = MakeCheckSum(2);				/* And start the check sum  */

/* Record length must be between 3 and 1027 to be valid */
	if ( (RecordLength < 3) || (RecordLength > 1027)) return(-2);

	NumWant = RecordLength-2;						/* # elements left to read  */
	if ( (NumRead = fread(buffer, 4, NumWant, handle)) != NumWant)
		rcode = -3;

	if (rcode == 0) {
		CheckSum = (CheckSum + MakeCheckSum(NumWant)) & 0xFFFFFFFFL ;
		if (CheckSum != 0) rcode = -4;
	}

	bufptr = buffer;									/* Set pointer for get_xxxx */
	if (rcode == 0) rcode = RecordType;			/* No errors, return type   */
	if (item_cnt != NULL)							/* Does user want the #     */
		 *item_cnt = NumRead-1;						/* Don't count the checksum */

#ifdef DEBUG_MODE
	TTYprintf("read_record: type=%s  size=%d\n", gettype(rcode), NumRead-1);
#endif

	return(rcode);
}

/* ===========================================================================
-- Function to retrieve packed string buffer
--
-- Usage:  void get_str(char *result, int len)
--
-- Inputs: bufptr      - pointer to next character in r/w buffer
--
-- Output: bufptr      - pointer to next character in r/w buffer
--         get_int32    - unpacked long integer
=========================================================================== */
static void get_str(char *result, size_t maxlen) {

	size_t i,j;

	i = (size_t) get_int32();					/* Read string length */
	j = (size_t) min(i, maxlen-1);
	memcpy(result, bufptr, j);
	result[j] = '\0';
	bufptr += (i+3)/4;							/* Skip over elements */
	return;
}

/* ===========================================================================
--  Function to retrieve next packed long integer from buffer
--
--  Usage:  long = get_int32()
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_int32     - unpacked long integer
=========================================================================== */
static INT32 get_int32() {

	register INT32 val;

	val = (((long) bufptr[0]) << 24) | (((long) bufptr[1]) << 16) |
			(((long) bufptr[2]) <<  8) |  ((long) bufptr[3]) ;
	bufptr += 4;
	return (val);
}

/* ===========================================================================
--  Function to convert packed short integer in buffer to 2 byte int
--
--  Usage:  short = get_int16(unsigned char buffer[], int *ptr)
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_int16     - unpacked short integer
=========================================================================== */
static INT16 get_int16() {

	register INT16 val;

	val = (((INT16) bufptr[0]) << 8) | bufptr[1];
	bufptr += 2;
	return (val);
}

/* ===========================================================================
--  Function to convert packed IEEE 4 byte real number in the read
--  buffer into the default REAL representation of the machine
--
--  Usage:  REAL = get_real()
--
--  Inputs: bufptr      - pointer to next character in r/w buffer
--
--  Output: bufptr      - pointer to next character in r/w buffer
--          get_real     - unpacked real number
=========================================================================== */
static REAL get_real() {

	union {
		REAL32 result;
		UINT32 val;
	} tmp;

	tmp.val = get_int32();

#if defined(CONVERT_IEEE)
	tmp.result = ieee_to_float(tmp.val);
#endif

	return (tmp.result);
}

/* ===========================================================================
--  Function to read sequence of data records
--
--  Usage:  int = read_data_records(REAL data[], int npt, int compress, char **errmsg);
--
--  Inputs: data      - buffer to store read data (better be big enough!)
--          npt       - number of data points we will eventually read
--          compress  - default compression method
--
--  Output: *errmsg   - pointer to error message if problems
--
--  Returns:  0 if successful
--           -1 on errors
=========================================================================== */
static int read_data_records(REAL *counts, int npt, int compress, char **errmsg) {

	int i, item_cnt, npt_read, record_type;

	npt_read = 0;
	while (npt_read != npt) {
		if ((record_type = read_record(&item_cnt)) < 0) {
			if (errmsg != NULL) *errmsg = "Premature end of data\n"; 
			return(-1);
		}

		if (record_type == DATA_REC) {		/* Convert to specific */
			if      (compress == 0)	record_type = DATA_REC_0;
			else if (compress == 1) record_type = DATA_REC_1;
			else if (compress == 2) record_type = DATA_REC_2;
			else if (compress == 3) record_type = DATA_REC_3;
		}

		switch (record_type) {
			case DATA_REC_0:
				for (i=min(1024,npt-npt_read) ; i ; i--) 
					counts[npt_read++] = get_real();
				break;
			case DATA_REC_1:
				for (i=min(1024,npt-npt_read) ; i ; i--) 
					counts[npt_read++] = (REAL) get_int32();
				break;
			case DATA_REC_2:
			case DATA_REC_3:
				if (record_type == DATA_REC_3) UnZeroCompress(bufptr, 4*item_cnt);
				if (read_compress(counts, npt, &npt_read)) {
					if (errmsg != NULL) *errmsg = "Bad data compression\n";
					return(-1);
				}
				break;
			default:
				if (errmsg != NULL) *errmsg = "Holy shit!  Record was not data as expected\n";
				return(-1);
		}
	}
	return(0);
}

/* ===========================================================================
--  Function to uncompress "differential" integral format
--
--  Usage:  int = read_compress(REAL data[], int npt, int *npt_read)
--
--  Inputs: npt       - number of data points we will eventually read
--          *npt_read - number of data points read so far
--
--  Output: data[]    - data buffer with uncompressed real data points
--          bufptr    - pointer to next character in r/w buffer
--          read_compress  - success (0 ==> okay)
=========================================================================== */
static int read_compress(REAL counts[], int npt, int *npt_read) {
        
	signed char  byte;
	signed short word;
	signed long  last;
	int i=1;

	last  = get_int32();										/* Read first value */
	counts[(*npt_read)++] = (REAL) last;

	while ( (*npt_read < npt) && (i++ != 1024) ) {
		byte = *(bufptr++);
		if (((unsigned char) byte) != 0x80) {        /* Avoid sign error */
			last += byte;										/* problems in test */
		} else {
			word = get_int16();
			if (((unsigned short) word) != 0x8000) {  /* Avoid sign error */
				last += word;	                        /* problems in test */
			} else {
				last = get_int32();
			}
		}
		counts[(*npt_read)++] = (REAL) last;
	}
	return(0);
}

/* ===========================================================================
-- Routines to handle zero compression/uncompression of byte stream (after
-- delta compression).  Multiple zeros in stream are compressed to a pair
-- marker <FLAG><count>.  <count> can be 0x01-0xFF.  The special case
-- <FLAG><00> indicates to literally include <FLAG> in the stream.
--
-- Compression is indicated in the stream by an initial byte of 0x80 followed
-- by the <FLAG> byte.  Default is for the <FLAG> to be 0x81 since -127 should
-- not be a common delta.  
--
--  Usage:  int TryZeroCompress(void *data, int length);
--          int UnZeroCOmpress(void *data, int length);
--
--  Inputs: data   - pointer to the current data (will be overwritten)
--          length - number of bytes in the stream initially
--
--  Output: data  - replaced with compressed stream if fewer (compress)
--                  or replaced by the expanded stream (uncompress).
--
--  Returns: New number of bytes in the stream.
=========================================================================== */
#define	ZERO_COMPRESS_FLAG	0x80			/* Flag indicating compress data */
#define	DEFAULT_REPEAT_BYTE	0x81			/* Byte indicating repeat			*/
#define	MAX_REPEAT_COUNT		0xFF			/* Maximum # of 00 to compress	*/

static int UnZeroCompress(void *data, int length) {

	unsigned char achr, repeat_byte, *iptr, *optr, tmp[BUFFER_SPACE_NEEDED];
	int len;
	
	iptr = data;									/* Local copies */
	len  = length;

	if (*iptr == ZERO_COMPRESS_FLAG) {		/* Flag indicating zero compressed */
		optr = tmp;

		repeat_byte = iptr[1];					/* Second byte is repeat marker */
		iptr += 2; len -= 2;						/* First 2 have been used */

		while (len > 0) {							/* Walk through all elements */
			achr = *iptr++; len--;
			if (achr != repeat_byte || len == 0) {
				*optr++ = achr;
			} else {
				achr = *iptr++; len--;
				if (achr == 0x00) {				/* Stream really had the byte */
					*optr++ = repeat_byte;
				} else {
					while (achr--) *optr++ = 0;
				}
			}
		}
		length = optr-tmp;						/* New stream length */
		memcpy(data, tmp, length);
	}

	return(length);
}

/* ===========================================================================
--  Function to generate the checksum of a given number of elements
--  from the buffer area.
--
--  Usage:  UINT32 = MakeCheckSum(int num_elem)
--
--  Inputs: num_elem - number of elements to sum at this time
--
--  Output: check sum
=========================================================================== */
static UINT32 MakeCheckSum(int num_elem) {

	register unsigned char *ptr;
	UINT32 result=0;

	ptr = buffer;
	while (num_elem--) {
		result += (((unsigned long) ptr[0]) << 24) | (((unsigned long) ptr[1]) << 16) |
					 (((unsigned long) ptr[2]) <<  8) |  ((unsigned long) ptr[3]) ;
		ptr += 4;
	}
	return(result);
}

/* ===========================================================================
--  Routine to check if number representations in this version of C are
--  correct.  Will generate error message on either of the following:
--     1. sizeof(UINT32) != sizeof(REAL32)
--     2. (REAL32) 93.375 not IEEE format (should be exact representation)
--
--  Usage:  int check_c_reals()
--
--  Inputs: none
--
--  Output: check_c_reals 0 ==> all okay
--                        1 ==> size of (long != float) or (byte != 1)
--                        2 ==> not IEEE format
--
--  Note: If CONVERT_IEEE is defined, the conversion routines will be
--        exercised as well.  Error is type 2 for these as well.
=========================================================================== */
#define CHK_NUMBER_REAL 93.375f                 /* Equivalent value of real */
#define CHK_NUMBER_HEX  0x42BAC000L             /* and long in IEEE format  */

static int check_c_reals() {
	union {													/* Equivalence the code */
		UINT32 dummy_long;
		REAL32 dummy_float;
	} tmp;
        
	if ((sizeof(REAL32) != sizeof(UINT32)) || (sizeof(char) != 1)) {
		ERRprintf("WARNING: sizeof(REAL32) != sizeof(UINT32) or sizeof(char) != 1\n");
		return(1);
	}

#if defined(CONVERT_IEEE)
	if ((float_to_ieee(CHK_NUMBER_REAL) != CHK_NUMBER_HEX) ||
		(ieee_to_float(CHK_NUMBER_HEX)  != CHK_NUMBER_REAL)) {
		ERRprintf("WARNING: Conversion routines for internal (float) to IEEE fail\n");
		return(2);
	}
#else
	tmp.dummy_float = CHK_NUMBER_REAL;              /* Exact value */
	if (tmp.dummy_long != CHK_NUMBER_HEX) {
		ERRprintf("WARNING: (REAL32) is not IEEE format.  Internal value %8.8lX vs %8.8lX\n",
			tmp.dummy_long,CHK_NUMBER_HEX);
		return(2);
	}
#endif

	return(0);
}

#if defined(CONVERT_IEEE)

/* ===========================================================================
--  Function to convert a "long" 4 byte IEEE format real number into
--  the internal "REAL32" representation.  Bit swapping and ordering is
--  your problem!
--
--  Usage:  REAL32 = ieee_to_float(long ieee_dummy)
--
--  Inputs: ieee_dummy - 4 byte IEEE real number packed in long
--
--  Output: ieee_to_float - internal representaion of same #
=========================================================================== */
static REAL32 ieee_to_float(UINT32 value) {
	union {
		REAL32 dummy_real;
		UINT32 dummy_long;
	} tmp;
	tmp.dummy_long = value;
	return (tmp.dummy_real);
}

/* ===========================================================================
--  Function to convert a "REAL32" internal format real number into the
--  equivalent IEEE real number packed into a 4 byte long integer
--  Functional inverse of routine above.
--
--  Usage:  unsigned long = float_to_ieee(REAL32 dummy)
--
--  Inputs: dummy - real number in internal representation
--
--  Output: float_to_ieee - IEEE representation of real number
=========================================================================== */
static UINT32 float_to_ieee(REAL32 value) {
	union {
		REAL32 dummy_real;
		UINT32 dummy_long;
	} tmp;
	tmp.dummy_real = value;
	return (tmp.dummy_long);
}

#endif


/* Rump user read/write routines */
#define	MODULENAME	"RUMP"

static void RbsActive(SPECTRUM *buf);

/*============================================================================
-- User function initialization procedure.  If defined, this routine is
-- called just after the module is loaded.  Run time per-instance
-- initialization should be performed by this routine.  At minimum, the
-- routine should print a message indicating successful load and version.
--
-- Usage:  Initialize the dynamic link module (local control)
--
-- Syntax: int UserInit(void);
--
-- Inputs: none
--
-- Output: local control only
--
-- Returns:  0 ==> all is okay
--          !0 ==> error, abort and free this module
--
-- Notes: UserInit() will be called before any other procedures in the module
============================================================================ */
static int RbsMode = 0;
#define	NORM		0x01
#define	ENERGY	0x02

static int RumpUserInit(void) {

	char option[OPTION_STR_SIZE];
	
	RbsMode = ENERGY | NORM;				/* Choose normalized versus energy as default */

	while (LexGetOption(option, sizeof(option))) {
		if (LexEqual(option, "-raw", 4)) {
			RbsMode &= ~NORM;
		} else if (LexEqual(option, "-normalized", 5)) {
			RbsMode |= NORM;
		} else if (LexEqual(option, "-energy", 2)) {
			RbsMode |= ENERGY;
		} else if (LexEqual(option, "-channel", 2)) {
			RbsMode &= ~ENERGY;
		} else if (LexEqual(option, "-help", 2) || stricmp(option, "-?") == 0) {
			TTYprintf("\nOptions:  {-RAW | -NORMalized} {-Channel | -Energy}\n\n");
			return(1);
		} else {
			ERRprintf("ERROR: Invalid RUMP option %s - use -? for help\n", option);
		}
	}
		
	TTYputs("RUMP read module loaded successfully\n");
	return(0);
}

/*============================================================================
-- User function termination procedure.  If defined, this routine is
-- called just before the module is released from memory.  Run time
-- per-instance termination should be performed by this routine.  If no
-- termination is required, this routine need not be defined.
--
-- Usage:  De-initialize the dynamic link module (local control)
--
-- Syntax: int UserExit(void);
--
-- Inputs: none
--
-- Output: local control only
--
-- Returns:  0 ==> all is okay
--          !0 ==> error (doesn't matter, but tell me if it makes you happy)
============================================================================ */
static int RumpUserExit(void) {

	TTYputs("Releasing the RUMP read user module\n");
	return(0);
}

/*============================================================================
-- User function to read data files for genplot.  If defined, this routine is
-- called in response to READ <file> -USER or USER -READ <file> commands.
-- The routine should read files by whatever format is desired and return
-- successful.
--
-- Usage:  Read user defined data format files
--
-- Syntax: int UserRead(char *Filename, char *Curve);
--
-- Inputs: Filename - file to read
--	        Curve    - "name of curve"
--
-- Output: Curve:X, Curve:Y, Curve:NPT - filled in
--         Curve:IDS                   - optionally filled in
--
-- Returns:  0 ==> all is okay
--          -1 ==> fatal error during setup or read
--          +1 ==> requested exit from read operation
--
-- Notes: Filename may or may not exist depending on source of call.  
--        Routine should handle missing file names with error message.
============================================================================ */
static int RumpUserRead(char *Filename, char *Curve) {

	REAL *x, *y;
	double corr;
	int i, itype, rcode;
	CURVE **aptr, *curve;
	SPECTRUM *buf;					/* Pointer to spectra structure */

/* ... Obtain pointer to actual curve structure ... */
	if (! GVGetInfo(Curve, &itype, (void **) &aptr)) return(-1);
	if (itype != GV_2DCURVE && itype != GV_3DCURVE)  return(-1);
	curve = *aptr;

/* Create space for a spectrum */
	if ( (buf = RbsAllocateSpectrum(NULL, 0)) == NULL) {
		ERRprintf("ERROR: Can't allocate space for spectra header\n");
		return(-1);
	}
	strcpy(buf->filename, Filename);

	if (RbsReadSpectrum(Filename, buf, &rcode) == NULL) {
		if (rcode == 1) {
			ERRprintf("ERROR: %s failed to open\n", Filename);
		} else {
			ERRprintf("ERROR: Error in RbsReadSpectrum (rc=%d)\n",rcode);
		}
		RbsFreeSpectrum(buf);
		return(-1);
	}

/* Output the ID for the file */
	RbsActive(buf);								           /* Print identifier */
	TTYprintf("\n");

	if (buf->npt > curve->nptmax) {
		if (! GVResize(Curve, buf->npt)) {				/* Try to increase size */
			gen_warn("Unable to increase size of curve -- Using current maximum");
			buf->npt = curve->nptmax;
		}
	}

/* ... Now, scan through CBUF filling in X and Y values from curve */
	x = curve->x;	y = curve->y;	curve->npt = buf->npt;
	strscpy(curve->ids, buf->id, sizeof(curve->ids));

	corr = (buf->corr*buf->cbeam) / (buf->scale1*buf->q*buf->omega+1E-20);

	for (i=0; i<buf->npt; i++) {
		x[i] = i + buf->first;
		y[i] = buf->counts[i];
		if (RbsMode & ENERGY) x[i] = (buf->scale2 + x[i]*buf->scale1) / 1000;
		if (RbsMode & NORM)   y[i] *= corr;
	}

	RbsFreeSpectrum(buf);
	return(0);
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
static void RbsActive(SPECTRUM *buf) {

	char *geometry, *filetype;
	char beam_string[10];

/* Create string of form 4He++ for the incident beam */
	{
		static const char *beams[10] = {
			"H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne" } ;
		char *aptr;
		int i;
		aptr = beam_string;
		sprintf(beam_string,"%1d%s",(int)buf->mbeam, beams[(buf->zbeam)-1]);
		aptr = beam_string + strlen(beam_string);
		for (i=0 ; i < buf->cbeam ; i++) *aptr++ = '+';
		*aptr = '\0';
	}

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
			buf->scale1,	buf->scale2,  buf->first, buf->npt,
			buf->fwhm,		buf->tau,	  buf->omega,
			buf->corr);
	return;
}
