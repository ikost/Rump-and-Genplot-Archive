/* curfit.c - Subroutine to perform a non-linear least squares fit on curve */
/* SEE /code/newlab/lsa/lasgo/curfit.c for a version suitable for standalone use */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#undef	DEBUG
#define	NORMALIZE_MATRIX				/* Do we normalize matrix b4 call */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <limits.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "curfit.h"
#include "helper.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	MY_MAGIC_COOKIE	0x31415926

typedef struct _CURFIT_DATA {
	double **alpha;				/* [PARMS][PARMS] Curvature matrix row ptrs	*/
	double *alpha_v;				/* [PARMS][PARMS] Actual data for alpha		*/
	double **array;				/* [PARMS][PARMS] Modified matrix row ptrs	*/
	double *array_v;				/* [PARMS][PARMS] Actual data for array		*/
	double *beta;					/* [PARMS] result vector							*/
	double *da;						/* [PARMS] change in A elements					*/
	REAL   *deriv;					/* Vector for d/da functions						*/
} CURFIT_DATA;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Subroutine to perform a non-linear least squares fit on curve
--
-- Usage:  int CurveFit(int key, int iter, NLS_DATA *nls)
--
-- Inputs: key   NKEY_INIT        -> initialize and return 1st value of chisqr
--               NKEY_TRY_VERBOSE -> process and output informational messages
--               NKEY_TRY_SILENT  -> process but be silent except errors
--               NKEY_TRY_DEBUG   -> process in debug mode (lots!!!)
--               NKEY_EXIT        -> cleanup after completion (free space)
--         iter  iteration count.  Should be zero on initialization and
--               incremented on each call.  Ignored in this code.
--         nls   Internal information about the fit.
--
-- Structure NLS_DATA
--	  REAL *data;			Ptr to the dependent variable array (compare value)
--	  REAL *error;			If not NULL, ptr to array with "error" of each point
--	  int  npt;          Number of points in *data, *error and each **xy
--	  REAL **xy;			Ptr to array of ptrs containing independent variables
--                      used by the fitting function.  Can be NULL if unneeded
--								by evalfnc() or fderiv();
--   int  nvars;			Number of parameters which are to be varyied
--   REAL **vars;		   Ptr to array of ptrs with actual variables
--   REAL *sigma;		   If not NULL, ptr to vector to receive sigma estimate
--   REAL chisqr;		   Chi-square value from the fit
--   REAL flamda;		   Size of change parameter (if 0 on key=0, set to reasonable value)
--   REAL *yfit;			Array ptr receiving fits (if NULL, alloc on key=0)
--   void *workspace;	Ptr to workspace (should be NULL on key=0)
--   LOGICAL (*evalfnc)(struct _NLS_DATA *nls);
--                      Ptr to function which evaluates the function with the
--                      current values of the parameters, filling in nls->yfit
--	  LOGICAL (*fderiv)(REAL *deriv, struct _NLS_DATA *nls, int ipt);
--                      Ptr to function which evaluates derivations of the
--                      function at the <ipt> point with respect to each of the
--                      varying parameters.  Fills in the vector <deriv>.
--	  void    (*evalchi)(struct _NLS_DATA *nls);
--                      If not NULL, routine which estimates the chi-square
--                      fit value and sets nls->chisqr.  CurveFit() will 
--                      minimize nls->chisqr with respect to each of the 
--                      parameters.  If specified as NULL, a default routine 
--                      calculates this parameter.
--
-- Returns:  0       - all okay
--          +1       - okay, and fit appears complete
--          -1       - too many parameters for # of data points
--          -2       - unable to evaluate function
--          -3       - unable to allocate temporary memory for inversion
--          -5       - user pressed ^C during fitting algorithm
--          -6       - improperly initialized structures
--          -7       - bad matrix values
--
-- Main program must provide functions to perform two operations:
--   (1) LOGICAL evalfnc(nls);         --- fills in nls->yfit for current parms
--   (2) void fderiv(deri, nls, ipt);  --- returns vector deriv with nvars
--                                         derivatives at point x[i].
--   (3) If the error correlation is required, it is hidden in the array[][]
--       array in the workspace.  This ends up proportional to the error
--       correlation, but is not normalized.
--
-- Documentation:
--  (1) Basic concept and code formulation based on Bevington, Statistical
--      Treatment of Experimental Data, 1986.   Section 11.5.
--  (2) From Ola, the code might be called the Levenberg-Marquardt method in
--      the literature.
============================================================================ */
int CurveFit(int key, int iter, NLS_DATA *nls) {

	double	tmp;
	REAL		xtmp;
	int		i,j,k, rcode;
	int		nvars, npt, nfree;
	LOGICAL  verbose, debug;
	BOOL		use_valid, use_errorbar;	/* Do we have entries to use */
	BOOL     NormalizeMatrix;				/* Should we normalize prior to inversion */
	CURFIT_DATA *lv;							/* Local variables pointer */

/* ... Work arrays for CURFIT */
	double **alpha;				/* Curvature matrix row pointers	*/
	double **array;				/* Modified matrix row pointers	*/
	double *beta;					/* Result vector						*/
	double *da;						/* Change in A elements				*/
	REAL   *deriv;					/* Vector for d/da functions		*/

/* ---------------------------------------------------------------------
-- Initially check validity, evaluate function and chisqr if not before
---------------------------------------------------------------------- */
	nvars = nls->nvars;					/* Copy these to local variables		*/
	npt   = nls->npt;						
	use_valid    = nls->valid    != NULL;
	use_errorbar = nls->errorbar != NULL;
	
	if (key == NKEY_INIT) {				/* Initialization code */
		nls->workspace = NULL;			/* Make sure we know nothing started */
		nls->magic_cookie = 0;

/* Make a quick check on the variables and their limits */
		if (nls->lower != NULL && nls->upper != NULL) {
			for (i=0; i<nvars; i++) {
				if (nls->upper[i] < nls->lower[i]) {
					xtmp = nls->lower[i];
					nls->lower[i] = nls->upper[i];
					nls->upper[i] = xtmp;
					ERRprintf("WARNING: Limits for var %d have been reversed [CurveFit]\n", i);
				}
			}
		}
		if (nls->lower != NULL) for (i=0; i<nvars; i++) {
			if (*nls->vars[i] < nls->lower[i]) {
				ERRprintf("WARNING: Var %d below lower limit.  Has been changed. [CurveFit]\n", i);
				*nls->vars[i] = nls->lower[i];
			}
		}
		if (nls->upper != NULL) for (i=0; i<nvars; i++) {
			if (*nls->vars[i] > nls->upper[i]) {
				ERRprintf("WARNING: Var %d beyond upper limit.  Has been changed. [CurveFit]\n", i);
				*nls->vars[i] = nls->upper[i];
			}
		}

		nfree = npt - nvars;				/* Number of degrees of freedom		*/
		if (use_errorbar || use_valid) {
			for (i=0; i<npt; i++) {
				if ( (use_errorbar &&   nls->errorbar[i] == 0) ||
					  (use_valid    && ! nls->valid[i]) ) nfree--;
			}
		}
		nls->dof = nfree;					/* Store as # of degrees of freedom	*/
		nls->sigmaest = 1.0;				/* Just in case we exit early			*/
		
		if (nfree <= 0) return(-1);	/* And had better be 1 or above		*/

/* Allocate some spaces and set parameters if not valid before */
		if (nls->yfit == NULL) {
			nls->yfit = calloc(npt, sizeof(*nls->yfit));
			if (nls->yfit == NULL) return(-3);
		}

		if (nls->flamda <= 0) nls->flamda = 0.001f;
		if (nls->evalchi == NULL) nls->evalchi = EvalChiGauss;

/* ---------------------------------------------------------------------------
-- Allocating the space individually is rather inefficient, but must be done
-- to avoid alignment errors if variable have different lengths.
--------------------------------------------------------------------------- */
		if ( (lv = malloc(sizeof(*lv))) == NULL) return(-3);
		lv->alpha   = calloc(nvars, sizeof(*alpha));				/* Row ptrs		*/
		lv->alpha_v = calloc(nvars*nvars, sizeof(**alpha));	/* Alpha array	*/
		lv->array   = calloc(nvars, sizeof(*array));				/* Row ptrs		*/
		lv->array_v = calloc(nvars*nvars, sizeof(**array));	/* Array array	*/
		lv->beta    = calloc(nvars, sizeof(*beta));				/* rslt vector	*/
		lv->da      = calloc(nvars, sizeof(*da));					/* dv vector	*/
		lv->deriv   = calloc(nvars, sizeof(*deriv));				/* d/da vector	*/

		nls->workspace    = (void *) lv;				/* So I get back each time	*/
		nls->magic_cookie = MY_MAGIC_COOKIE;		/* And I know it is there	*/

/* Evaluate function and return error level if user requests simple trial */
		if ( (*nls->evalfnc)(nls) != 0) return(-2);
		(*nls->evalchi)(nls);
		nls->chiold = nls->chisqr;
		return(0);

/* Free memory allocated in the workspace */
	} else if (key == NKEY_EXIT) {
		if (nls->magic_cookie == MY_MAGIC_COOKIE) {
			lv = (CURFIT_DATA *) nls->workspace;
			if (lv != NULL) {
				free(lv->alpha);				/* Free the allocated arrays */
				free(lv->alpha_v);
				free(lv->array);
				free(lv->array_v);
				free(lv->beta);
				free(lv->da);
				free(lv->deriv);
				free(nls->workspace);		/* And the workspace itself */
			}
			nls->workspace = NULL;
			nls->magic_cookie = 0;
		}
		return(0);
	}


/* ---------------------------------------------------------------------------
-- All attempts to actually modify the parameters pass through here
--------------------------------------------------------------------------- */
	if (nls->magic_cookie != MY_MAGIC_COOKIE) {		/* Not initialized */
		ERRprintf("OUCH! Expected initialized structures were not there!\n");
		return(-6);
	}
	verbose = (key == NKEY_TRY_VERBOSE || key == NKEY_TRY_DEBUG);
	debug   = (key == NKEY_TRY_DEBUG);

	nls->chiold = nls->chisqr;

/* -------------------------------------------------------------------------
-- ... Make local copy of pointers for workspaces, and initialize 
-------------------------------------------------------------------------- */
	lv = (CURFIT_DATA *) nls->workspace;
	alpha = lv->alpha;
	array = lv->array;
	beta  = lv->beta;
	da    = lv->da;
	deriv = lv->deriv;
	for (i=0; i<nvars; i++) alpha[i] = &lv->alpha_v[i*nvars];
	for (i=0; i<nvars; i++) array[i] = &lv->array_v[i*nvars];

	for (i=0; i<nvars; i++) {							/* Clear out everything */
		beta[i] = 0.0f;
		for (j=0; j<=i; j++) alpha[j][i] = 0.0f;
	}

/*-------------------------------------------------------------------------
-- ... For every point, BETA(J) = @SUM(<res(i)>*deriv(j,i)) (N vector ops)
-- ...                  ALPHA(J,K) = SUM(DERIV(I,J)*DERIV(I,K))
--
-- ... In vector format
-- ... A(J,K) = derivj x derivk                  -- Dot product --
-- ... betaj  = (y-yfit) x derivj
--
-- Note: nls->fderiv is guarenteed to be called for first point of data set
--       but will only be called on subsequent points if errorbar is non-zero.
------------------------------------------------------------------------- */
/*	if (verbose) CONputs("Curvature matrix ... "); */
	for (i=0; i<npt; i++) {
		xtmp = use_errorbar ? nls->errorbar[i] : 1.0f;
		if (xtmp != 0 && xtmp != 1) xtmp = 1/(xtmp*xtmp);	/* Weighting factor */
		if (xtmp == 0 || (use_valid && ! nls->valid[i]) ) {
			if (i == 0) {
				rcode = (*nls->fderiv)(deriv, nls, i);			/* Get df/da|x(i)	  */
				if (rcode != 0) return(rcode);					/* Error condition  */	
			}
			continue;
		}
		rcode = (*nls->fderiv)(deriv, nls, i);					/* Get df/da|x(i)	  */
		if (rcode != 0) return(rcode);							/* Error condition  */	
		for (j=0; j<nvars; j++) {
			beta[j] += (nls->data[i]-nls->yfit[i])*deriv[j]*xtmp;
			for (k=0; k<=j; k++) alpha[k][j] += deriv[j]*deriv[k]*xtmp;
		}
	}
	for (j=0; j<nvars; j++) {							/* Create symmetric matrix */
		for (k=0; k<j; k++) alpha[j][k] = alpha[k][j];
	}

	if (debug) {
		TTYprintf("Curvature matrix computed:\n");
		for (i=0; i<nvars; i++) TTYprintf("   df/da[%d] = %g   beta[%d] = %g   alpha[%d][%d]=%g\n", i, deriv[i], i,beta[i], i,i,alpha[i][i]);
	}

/*	if (verbose) {CONputc('\r'); ScrEraseLine(0);} */

/* ------------------------------------------------------
-- ... Invert the curvature matrix to find new parameters
------------------------------------------------------ */
/* - First verify that the matrix does not have a zero element - this will crash */
#ifdef NORMALIZE_MATRIX
	NormalizeMatrix = TRUE;
#else
	NormalizeMatrix = FALSE;
#endif
	for (i=0; i<nvars && NormalizeMatrix; i++) {								/* If zero on diagonal, cannot use normalization */
		if (alpha[i][i] == 0) {
			NormalizeMatrix = FALSE;
			if (verbose) ERRprintf("WARNING: Alpha matrix diagonal zero on element %d.  Skipping normalization.\n", i);
		}
	}
					
	while (TRUE) {
		for (j=0; j<nvars; j++) {
			if (NormalizeMatrix) {
				for (k=0; k<nvars; k++) array[k][j] = alpha[k][j] / sqrt(alpha[k][k]*alpha[j][j]);
				array[j][j]  = 1.0f + nls->flamda;
			} else {
				for (k=0; k<nvars; k++) array[k][j] = alpha[k][j];
				array[j][j] *= 1.0f + nls->flamda;
			}
		}
		matinv(array, nvars);							/* Invert it */
		for (j=0; j<nvars; j++) {
			da[j] = 0.0f;
			if (NormalizeMatrix) {
				for (k=0; k<nvars; k++) da[j] += beta[k] * array[k][j]/sqrt(alpha[k][k]*alpha[j][j]);
			} else {
				for (k=0; k<nvars; k++) da[j] += beta[k] * array[k][j];
			}
		}

		if (debug) {
			TTYprintf("Matrix inverted with flamda: %f\n",nls->flamda);
			for (i=0; i<nvars; i++) TTYprintf("   da[%d] = %f\n", i, da[i]);
		}

/* ... If CHISQR increases, increase flamda and try again */
		if (nls->flamda < 1E-7f || nls->flamda > 1E15f) {
			if (verbose) TTYprintf(
				"WARNING: Internal parameter out of range (%g).  Strange function indicated.\n"
				"          Additional iterations may/may not improve the fit.\n", nls->flamda);
			nls->chisqr = nls->chiold;
			break;
		}
		for (i=0; i<nvars; i++) {						/* Modify the parms			*/
			tmp   = *nls->vars[i] + da[i];			/* New value					*/
			if (nls->lower != NULL && tmp < nls->lower[i]) tmp = nls->lower[i];
			if (nls->upper != NULL && tmp > nls->upper[i]) tmp = nls->upper[i];
			da[i] = *nls->vars[i];						/* Save to restore laster	*/
			*nls->vars[i] = (REAL) tmp;				/* And set in value			*/
		}
/*      if (verbose) CONputs("Function ... "); */
		if (SysChkBreak(TRUE)) return(-5);
		if ( (*nls->evalfnc)(nls) != 0) return(-2);
/*      if (verbose) {CONputc('\r'); ScrEraseLine(0);} */
      (*nls->evalchi)(nls);
		if (debug) {
			TTYprintf("Evaluating chisqr at modified values:\n");
			for (i=0; i<nvars; i++) TTYprintf("  var[%d]: %g -> %g\n", i, da[i], *nls->vars[i]);
			TTYprintf("   chisqr(old), chisqr: %g %g\n", nls->chiold, nls->chisqr);
		}

		if ( (nls->chisqr - nls->chiold)/nls->chiold > 1E-7f) {
			for (i=0; i<nvars; i++) *nls->vars[i] = (REAL) da[i];	/* Change back */
			nls->flamda *= 10;										/* Scale up		*/
			if (debug) TTYprintf(" Change in chisqr too small, trying large flambda perturbation (%g)\n", nls->flamda);
			continue;
		} else {
			break;
		}
	}

/* -------------------------------------------------------------------------------
-- ... Return with estimate of the sigma elements and/or error correlation matrix
--
-- First operations give the curvature of the inversion matrix.  These
-- are the true sigma's if we have been given proper estimates of the
-- error on y (errorbar).  If errorbar is not given (NULL), then we rescale
-- by estimating the uniform +/- error neede to give a reduced chisqr of
-- 1.0.  This estimate is returned in nls->sigmaest.  If errorbar is given,
-- then nls->sigmaest returns an estimate of the scaling on errors needed to
-- give exactly a reduced chi-square of 1.
------------------------------------------------------------------------------- */
	if (nls->sigma != NULL || nls->correlate != NULL) {
		for (j=0; j<nvars; j++) {
			if (NormalizeMatrix) {
				for (k=0; k<nvars; k++) array[k][j] = alpha[k][j] / sqrt(alpha[k][k]*alpha[j][j]);
				array[j][j] = 1.0f;
			} else {
				for (k=0; k<nvars; k++) array[k][j] = alpha[k][j];
			}
		}

		matinv(array, nvars);							/* Invert it */

		if (NormalizeMatrix) {
			for (j=0; j<nvars; j++) {
				for (k=0; k<nvars; k++) array[k][j] /= sqrt(alpha[k][k]*alpha[j][j]);
			}
		}

		if (nls->correlate != NULL) {					/* Correlation matrix wanted */
			for (j=0; j<nvars; j++) {
				for (k=0; k<nvars; k++) {
					if (array[j][j]*array[k][k] > 0) {
						nls->correlate[j*nvars+k] = (REAL) (array[j][k] / sqrt(array[j][j]*array[k][k]));
					} else {
						nls->correlate[j*nvars+k] = 1.0;
					}
				}
			}
		}

		if (nls->sigma != NULL) {						/* Error estimates wanted */
			for (j=0; j<nvars; j++) {					/* Calculate true sigmas */
				nls->sigma[j] = (REAL) sqrt(array[j][j]);
			}
			nls->sigmaest = (REAL) sqrt(nls->chisqr);		/* Estimate scaling needed */

/* -- Correct errors if we must use our estimate of the error bars -- */
			if (nls->errorbar == NULL) {				/* Must estimate sigma	*/
				for (i=0; i<nvars; i++) nls->sigma[i] *= nls->sigmaest;
			}
		}
	}

	nls->flamda /= 10;

/* -----------------------------------------------
-- ... Check for completion
----------------------------------------------- */
	if (fabs(nls->chisqr-nls->chiold) < nls->EpsCrit*nls->chiold) return(1);
	return(0);
}


/* ===========================================================================
-- Routines to determine reduced chi-square of data set, and optionally the
-- point by point chi deviation.  Routines provide Poisson or Gaussian
-- statistics as desired.   Compares the fit (nls->yfit) with actual data
-- nls->data.
--
-- Usage:  int EvalChiGauss(NLS_DATA *nls);
--         int EvalChiPoiss(NLS_DATA *nls);
--
-- Inputs: NLS_DATA structure
--         nls->yfit      theory
--         nls->data      actual data
--         nls->valid     Marks valid data points (if not NULL)
--         nls->errorbar  Error bars or window flag (see notes)
--         nls->npt       Number of points in above arrays
--
-- Output: nls->chisqr          reduced Chi^2 = raw Chi^2 / degrees_of_freedom
--         nls->outchi          raw Chi vector (optional)
--
-- Return: number of errors found
--
-- Notes:  This routine can calculate either Gaussian or Poisson
--         statistics on the data.  In either case, the reduced chi-square
--         value is returned (chisqr / degrees_of_freedom)
--         If vector mode is selected, the point-by-point square root of
--         Chi^2 is also stored in an array.
--
-- Notes for EvalChiGauss() only:
--         If the nls->errorbar array is used, any zero entry will be
--         "window"ed out of the data set.  NULL errorbar array causes all
--         points to use an assumed sigma of unity.  No error return.
--
-- Notes for EvalChiPoiss() only:
--         The input theory must be positive (this routine will check and
--         make noise if condition is not met).  The input data must be
--         non-negative integer for statistics to be valid - somebody else
--         should check).  The nls->errorbar array is used as a flag for
--         data "window"ing.  Any non-zero errorbar[i] says causes point to
--         be included in chisqr calculation.  NULL errorbar includes all
--         points in the calculation.
--
-- In the absence of error estimators, the returned value is the variance
-- defined as SUM([y-y_ave]^2) / (n-dof)
=========================================================================== */
int EvalChiGauss( NLS_DATA *nls ) {

	int  i, npt;
	REAL chi, chisqr;
   REAL *dpnt, *yfit, *error, *outchi;
	BOOL *valid;

   chisqr = 0.0;
	npt    = nls->npt;
	error  = nls->errorbar;
	valid  = nls->valid;
	dpnt   = nls->data;
	yfit   = nls->yfit;
	outchi = nls->outchi;

/* Zero out outchi so don't have to worry about in loop */
	if (outchi != NULL) for (i=0; i<npt; i++) outchi[i] = 0.0f;

/* Start summing */
	for (i=0; i<npt; i++) {
		if (valid != NULL && ! valid[i]) continue;		/* Explicit valid flag	*/
      if (error != NULL && error[i] == 0.0) continue;	/* Flag for "don't use	*/
		chi = yfit[i] - dpnt[i];								/* Absolute deviation	*/
		if (error  != NULL) chi /= error[i];				/* Normalize by sigma	*/
		if (outchi != NULL) outchi[i] = chi;				/* Prog wants dev's		*/
		chisqr += chi*chi;
	}

   nls->chisqr = chisqr / nls->dof;
   return(0);
}


/* ===========================================================================
-- Routine to calculate the Chi^2 deviation between theory and experiment
-- using Poisson statistics.  Adds correction to normal sqrt(N) equation.
-- 
-- Usage:  int EvalChiPoission(NLS_DATA *nls)
--
-- Inputs: INT  nls->npt        - number of expt/theory points
--         INT  nls->dof        - number of degrees of freedom 
--         REAL nls->data[]     - experimental data values
--         REAL nls->yfit[]     - theoretical data values
--         BOOL nls->valid[]    - if ! NULL, then [] determines if point is
--                                included in calculation.
--         REAL nls->errorbar[] - if ! NULL and [] == 0, point is not used in
--                                in chi^2 calculation (same as valid[])
--
-- Output: REAL  nls->chisqr    - reduced chi^2 = chi^2/dof
--         REAL *nls->outchi[]  - if ! NULL, [] receives chi of that point
--
-- Return: Number of invalid points.  If theory value is <= 0.0, then
--         point is considered invalid.  Theory must predict positive
--         value for every channel.
=========================================================================== */
int EvalChiPoisson( NLS_DATA *nls ) {

   REAL theory;   /* Theoretical value */
   REAL data;     /* Expermental data */

   int  i, nbad, npt;
   REAL chi, chisqr, x, s;
   REAL *dpnt, *yfit, *error, *outchi;
	BOOL *valid;

   nbad   = 0;
   chisqr = 0.0;
   npt    = nls->npt;
   error  = nls->errorbar;
	valid  = nls->valid;
   dpnt   = nls->data;
   yfit   = nls->yfit;
   outchi = nls->outchi;

/* Zero out outchi so don't have to worry about in loop */
	if (outchi != NULL) for (i=0; i<npt; i++) outchi[i] = 0.0f;

/* Start summing */
   for (i=0; i<npt; i++) {
		if (valid != NULL && ! valid[i]) continue;		/* Explicit valid flag	*/
      if (error != NULL && error[i] == 0.0) continue;	/* Flag for "don't use	*/
      data   = dpnt[i];
      theory = yfit[i];
      if ( theory <= 0.0 ) {
         nbad++;                  /* Invalidates whole process */
         chi = 0.0;               /* No known direction */
      } else if ( data == 0.0 ) {
         chi = (REAL) sqrt(theory*2);
      } else {
         x = theory/data;
   /* The part in the middle (x between 0.9 and 1.1) is designed to avoid
      roundoff errors, and speed things up a bit.  It is matched at the
      endpoints, and has the proper asymptotic behavior at the origin.
      Maximum error is about 3.6e-6 (occurs at x=0.93 and x=1.07)        */
         if ( x>1.1 ) {
            chi =  (REAL) sqrt(data*2*(x-1-log(x)));
         } else if ( x<0.9 ) {
            chi = (REAL) -sqrt(data*2*(x-1-log(x)));
         } else {
            s   = x-1.0f;
            chi = (REAL) (sqrt(data)*((s*0.19547835-0.33469349)*s+1.0)*s);
         }
      }
		if (outchi != NULL) outchi[i] = chi;				/* Prog wants dev's		*/
      chisqr += chi*chi;
   }
   if (nbad != 0) {
		ERRprintf("%d points found with non-positive theory! Poisson statistics invalid!\n", nbad);
	}

   nls->chisqr = chisqr / nls->dof;
   return(nbad);
}
