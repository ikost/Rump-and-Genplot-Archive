/* Coordinate system notes:
	user coordinate system:       Compositions, thicknesses, etc.
	reduced coordinate system:    -1 to 1, scaled from user c.s.
	auxiliary coordinate system:  centered at iteration with lowest Chi^2,
			with unit vectors pointing to each other iteration

Array notes:  Array indices are kept in mathematical order, thus
 A*b where A is a matrix and b is a vector is sum_over_i A[j][i]*b[i]
 A*B where A is a matrix and B is a matrix is sum_over_i A[j][i]*B[i][k]

Thompson notes: (1) code was non-reentrant due to static variables.  Moved
                    static vars to structure (lv - for local vars).
*/

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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

#define	MY_MAGIC_COOKIE	0x27182818

/* #define INCLUDE_DEBUG_CODE */			/* Must be defined if DEB(x) defined */
#define 	DEB(x)
#define 	XMIN		(-1.0f)
#define 	XMAX		(1.0f)
#define 	SUCCESS	(0)
#define 	FAILURE	(-6)     /* Keeps nlsfit() quiet */
#define 	DONE		(1)
#define 	NEXT		(0)

/* ---------------------------------------------------------------------------
-- The following info needed from iteration to iteration.  Arrays are pointers
-- into nls->workspace area.  nls->workspac has this structure at the front,
-- followed immediately by the actual arrays.  The structure is allocated and
-- initialized during key=0 call to LocateMin().  Because LOCMIN_DATA structure
-- is at the front of nls->workspac, a simple pointer cast works to set lv in
-- the routines below.  lv must be passed between subroutines.
--------------------------------------------------------------------------- */
typedef struct _LOCMIN_DATA {
	int NextGuess;
	double *Grad, *rGrad, *x0, *uhold, *nv, *tmpv;		/* Simple vectors */
	double **Hess,  **tHess,  **rHess,  **cmap;			/* Row pointers	*/
	double  *Hess_v, *tHess_v, *rHess_v, *cmap_v;		/* Actual arrays	*/
	double **prod,  **x;
	double  *prod_v, *x_v;
	REAL   **ChiV;
	REAL    *ChiV_v;
} LOCMIN_DATA;


/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void mat_mul_v(int dim, double *result, double **mat, double *vec);
static void matT_mul_v(int dim, double *result, double **mat, double *vec);
static void mat_mul_mat(int dim, double **result, double **matl, double **matr);
static void matT_mul_mat(int dim, double **result, double **matl, double **matr);
#ifdef INCLUDE_DEBUG_CODE
	static void vec_print(int dim, double *vector);
	static void mat_print(int dim, double **matrix);
#endif

static int LocminGuess(int *next_index, LOCMIN_DATA *lv, NLS_DATA *nls);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */



/* ============================================================================
-- Call sequence made compatible with Mike Thompson's CurveFit() 
--
-- Usage:  LocateMin(int key, int iter, NLS_DATA *nls)
--
-- Inputs: key   NKEY_INIT        -> initialize and return 1st value of chisqr
--               NKEY_TRY_VERBOSE -> process and output informational messages
--               NKEY_TRY_SILENT  -> process but be silent except errors
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
--
-- Main program must provide functions to perform two operations:
--   (1) LOGICAL evalfnc(nls);         --- fills in nls->yfit for current parms
--   (2) void fderiv(deri, nls, ipt);  --- returns vector deriv with nvars
--                                         derivatives at point x[i].
=========================================================================== */
int LocateMin(int key, int iter, NLS_DATA *nls) {

   int i, j, k, rcode, next, useless, nfree, prodmax;
   double sum;
   REAL scale;
	int dim,npt;							/* # of varying parms, # of data points  */
	BOOL use_valid, use_errorbar;		/* Do we have entries to use */
	LOCMIN_DATA *lv;						/* Static local variables needed for fit */

/* ---------------------------------------------------------------------
-- Initially check validity, evaluate function and CHISQ1 if not before
---------------------------------------------------------------------- */
	dim = nls->nvars;						/* Copy these to local variables		*/
	npt = nls->npt;
	lv = (LOCMIN_DATA *) nls->workspace;
	use_valid    = nls->valid    != NULL;
	use_errorbar = nls->errorbar != NULL;
	
	if (key == NKEY_INIT) {				/* Initialization code */

		nfree = npt - dim;				/* Number of degrees of freedom		*/
		if (use_errorbar || use_valid) {
			for (i=0; i<npt; i++) {
				if ( (use_errorbar &&   nls->errorbar[i] == 0) ||
					  (use_valid    && ! nls->valid[i]) ) nfree--;
			}
		}
		nls->dof = nfree;					/* Store as # of degrees of freedom	*/

		if (nfree <= 0) return(-1);	/* And had better be 1 or above		*/

/* Allocate some spaces and set parameters if not valid before */
		if (nls->yfit == NULL) {
			nls->yfit = (REAL *) malloc(npt*sizeof(REAL));
			if (nls->yfit == NULL) return(-3);
		}

		if (nls->evalchi == NULL) nls->evalchi = EvalChiGauss;

/* ---------------------------------------------------------------------------
-- Allocating the space individually is rather inefficient, but must be done
-- to avoid alignment errors if variable have different lengths.
--------------------------------------------------------------------------- */
		dim = nls->nvars;						/* Copy these to local variables		*/
		npt = nls->npt;

		if ( (lv = malloc(sizeof(*lv))) == NULL) return(-3);

		#define	SPACE_V(p, length) \
			p = calloc(length, sizeof(*p));
		#define	SPACE_M(p,p_v,rows,cols) \
			p = malloc((rows)*sizeof(*p)); \
			p_v = calloc((rows)*(cols), sizeof(*p_v)); \
			for (i=0; i<(rows); i++) p[i] = &p_v[i*(cols)];

		SPACE_V(lv->Grad, dim)
		SPACE_V(lv->rGrad,dim)
		SPACE_V(lv->x0,   dim)
		SPACE_V(lv->uhold,dim)
		SPACE_V(lv->nv,   dim)
		SPACE_V(lv->tmpv, dim)
		SPACE_M(lv->Hess,	lv->Hess_v, dim,dim)
		SPACE_M(lv->tHess,lv->tHess_v,dim,dim)
		SPACE_M(lv->rHess,lv->rHess_v,dim,dim)
		SPACE_M(lv->cmap,	lv->cmap_v, dim,dim)
		SPACE_M(lv->prod,	lv->prod_v, dim+1,dim+1)
		SPACE_M(lv->x,		lv->x_v,    dim+1,dim)
		SPACE_M(lv->ChiV,	lv->ChiV_v, dim+1,npt)

		#undef  SPACE_V
		#undef  SPACE_M

		nls->workspace    = (void *) lv;				/* So I get back each time	*/
		nls->magic_cookie = MY_MAGIC_COOKIE;		/* And I know it is there	*/

/* Evaluate function and return error level if user requests simple trial */
		if ( (rcode = (*nls->evalfnc)(nls)) != 0) return(rcode);
		(*nls->evalchi)(nls);
		nls->chiold = nls->chisqr;
		return(0);
	} else if (key == NKEY_EXIT) {
		if (nls->magic_cookie == MY_MAGIC_COOKIE) {
			lv = (LOCMIN_DATA *) nls->workspace;
			free(lv->Grad);
			free(lv->rGrad);
			free(lv->x0);
			free(lv->uhold);
			free(lv->nv);
			free(lv->tmpv);
			free(lv->Hess);	free(lv->Hess_v);
			free(lv->tHess);	free(lv->tHess_v);
			free(lv->rHess);	free(lv->rHess_v);
			free(lv->cmap);	free(lv->cmap_v);
			free(lv->prod);	free(lv->prod_v);
			free(lv->x);		free(lv->x_v);
			free(lv->ChiV);	free(lv->ChiV_v);
			free(nls->workspace);		/* And the workspace itself */
			nls->workspace = NULL;
		}
		nls->magic_cookie = 0;
		return(0);
	}

	if (nls->magic_cookie != MY_MAGIC_COOKIE) {		/* Not initialized */
		ERRprintf("OUCH! Expected initialized structures were not there!\n");
		return(-6);
	}
	nls->chiold = nls->chisqr;

   /*  Randomize starting points, eval function,
    *  eval Chi, and generate product array.
    *  This is where we can also put in different types of guesses
    */
   if (iter == 0) {
      for (j=0; j<dim; j++) {
         lv->uhold[j] = *(nls->vars[j]);		/* Remember where we came from */
         lv->x[iter][j] = (*(nls->vars[j])-nls->lower[j])/
                   (nls->upper[j]-nls->lower[j])*(XMAX - XMIN) + XMIN;
		}
      next = 0;
	} else if (iter < dim+1) {
      for (j=0; j<dim; j++) {
#if 0
         lv->x[iter][j] = XMIN + (XMAX - XMIN)*rand()/((REAL) RAND_MAX);
#else
         lv->x[iter][j] = lv->x[0][j];
         if (j==(iter-1)) {   /* This "0.05" should really be user-settable */
            if (lv->x[iter][j] > (XMAX + XMIN)*0.5) {
               lv->x[iter][j] -= 0.05*(XMAX - XMIN);
            } else {
               lv->x[iter][j] += 0.05*(XMAX - XMIN);
            }
         }
#endif
		}
      next = iter;
   } else {							/* end of first n+1 point conditional */
      next = lv->NextGuess;
	}
/*  Merged now between first n+1 and subsequent cases:
 *  in either case, "next" is the point we are working on.
 *  Start by copying it into user space and user units.
 */
   for (j=0; j<dim; j++) {
      *(nls->vars[j]) = (REAL) (nls->lower[j] + 
                (nls->upper[j]-nls->lower[j])*(lv->x[next][j]-XMIN)/(XMAX - XMIN));
   }
   DEB(vec_print(dim,lv->x[next]);)
   if ( (rcode = (*nls->evalfnc)(nls)) != 0 ) { rcode = -2; goto Abort; }
   nls->outchi = lv->ChiV[next];
   (*nls->evalchi)(nls);
   prodmax = min( iter+1, dim+1 );
   for (j=0; j<prodmax; j++) {
      sum = 0;
      for (k=0; k<npt; k++) {
         sum += lv->ChiV[next][k]*lv->ChiV[j][k];
      }
      lv->prod[next][j] = lv->prod[j][next] = sum;
   }

#ifdef LARRY_TO_FIX
/*
LARRY - I DON'T KNOW WHAT REALLY SHOULD BE HERE.  THERE IS A PROBLEM IN
        THAT NexGuess HAS NOT BEEN SET WHEN FINISHING ITERATION dim (WHICH
        HAS THE CENTER POINT AND DIM OUTER POINTS).  I'M GUESING THAT AT
        THAT POINT WE ARE CLOSE ENOUGH TO NOT BOTHER GOING FURTHER
*/
   if (iter >= dim+1 ) {   /* Then we have enough to go on */
#endif
	if (iter >= dim ) {   /* Then we have enough to go on */
      DEB(TTYprintf("product array:\n"); mat_print(dim+1,lv->prod);)
      rcode = LocminGuess(&next, lv, nls);
      lv->NextGuess = next;
      useless = (rcode == FAILURE);
      for (j=0; j<dim; j++) { useless |= (lv->x0[j] < XMIN || lv->x0[j] > XMAX);}
	} else {
      rcode = 0;
      useless = TRUE;
   }
/*  Now generate the results in case we have the real minimum,
 *  or stop after this iteration for other reasons.
 *  Output value of x_vector, and standard deviation information
 */
   if ( useless ) {
      for (j=0; j<dim; j++) {
         *(nls->vars[j])  = (REAL) lv->uhold[j];
         nls->sigma[j]    = 0.0;
		}
   } else {
      for (j=0; j<dim; j++) {
         scale = (REAL) ((nls->upper[j]-nls->lower[j])/(XMAX - XMIN));
         *(nls->vars[j])  = (REAL) (nls->lower[j] + (lv->x0[j] - XMIN)*scale);
         nls->sigma[j]    = (REAL) (lv->rHess[j][j]*scale*scale);
      }
   }
	nls->sigmaest = (REAL) sqrt(nls->chisqr);

Abort:
   if (rcode < 0) TTYprintf("LocateMin() returning with code = %d\n",rcode);
   return (rcode);
}

/*===============================================================*
  LocminGuess possible outcomes:
    FAILURE - problems doing matrix math
    NEXT    - we have a next guess set in *next_index
    DONE    - we pass the exit condition
 *===============================================================*/
static int LocminGuess(int *next_index, LOCMIN_DATA *lv, NLS_DATA *nls) {

/*	Input data:
	dim         dimension of the problem
	x[i][j]     Reduced Coordinates of the i'th (of dim+1) iteration.
	            j is the parameter index.
	prod[i][j]  Symmetric matrix of order dim+1 with cross-product sums
	            of Chi's from the data.
  Result: *next_index is the index of the vector which has been replaced
          with the next-guess coordinates.  x[*next_index]
*/
   REAL chimin, chimax;
   REAL t, r, u, lambda, xx, xd;
   int   jmin, jmax;
   int   i, i_1, i_2, j, j_1, j_2;
	int dim;
	double Q0, delta;						/* Should be external settable */

	dim   = nls->nvars;					/* Number of variables being modified */

/* Find the point with minimum Chi^2, we will refer our
   auxiliary coordinate system to this origin.  The maximum
   point is also needed, in case we iterate and need to know
   which point to throw away.
*/
   chimin = chimax = (REAL) lv->prod[0][0];
   jmin   = jmax   = 0;
   for (j=1; j<dim+1; j++) {
      xx = (REAL) lv->prod[j][j];
      if (xx < chimin) { chimin = xx; jmin = j; }
      if (xx > chimax) { chimax = xx; jmax = j; }
   }
   DEB(TTYprintf("Max chi[%d] = %14.5f      Min chi[%d] = %14.5f\n",
             jmax, chimax, jmin, chimin);)
   
/* Now convert the existing information to Q0, gradient, and Hessian form
   still within the auxiliary coordinate system.
   We also have some coordinate mapping to do.  First construct a coordinate
   transform array from reduced coordinates to auxiliary coordinates,
   then invert it.   Weird loop indexing has to do with skipping over
   the minimum (origin) point on the input arrays of size dim+1, but
   not on the output arrays which have size dim.
   cmap is defined such that:
         reduced_vector[i] = sum_over_j(lv->cmap[i][j]*auxiliary_vector[j])
*/
   Q0 = chimin;
   for (i_1=0,i_2=0; i_2<dim; i_1++,i_2++) {
           if (i_1==jmin) i_1++;
	   lv->Grad[i_2] = lv->prod[i_1][jmin] - Q0;
	   for (j_1=0,j_2=0; j_2<dim; j_1++,j_2++) {
                   if (j_1==jmin) j_1++;
                   lv->Hess[i_2][j_2] =  lv->prod[i_1][j_1]   - lv->prod[i_1][jmin]
                                 - lv->prod[jmin][j_1] + Q0;
		   lv->cmap[j_2][i_2] = lv->x[i_1][j_2] - lv->x[jmin][j_2];
	   }
   }
   DEB(TTYprintf("Coordinate array:\n"); mat_print(dim,lv->cmap);)
   if ( matinv(lv->cmap,dim) != 0 ) {
      ERRprintf("Error inverting coordinate map matrix.\n");
      goto NoCanDo;
   }
   DEB(TTYprintf("Inverted Coordinate array:\n");   mat_print(dim,lv->cmap);)
   
/* Now that it is inverted, cmap is defined such that:
         auxiliary_vector[j] = sum_over_i(cmap[j][i]*reduced_vector[i])
   Use this mapping function to take Gradient and Hessian information
   into the reduced coordinate system.
*/
   matT_mul_v(    dim,  lv->rGrad,  lv->cmap,  lv->Grad  );  /* rGrad = cmap * Grad */
   DEB(TTYprintf("Hessian (Auxiliary space):\n");   mat_print(dim,lv->Hess);)
   mat_mul_mat(  dim,  lv->tHess,  lv->Hess,  lv->cmap  );  /* tHess = Hess * cmap */
   matT_mul_mat( dim,  lv->rHess,  lv->cmap,  lv->tHess );  /* rHess = cmap(T) * tHess */
   DEB(TTYprintf("Hessian (Reduced space):\n");   mat_print(dim,lv->rHess);)

/* Invert the Hessian (in reduced coordinate system) and use result to
   find the predicted minimum of the Chi^2 function.   This minimum, x0,
   is in reduced coordinates.
*/
   if ( matinv(lv->rHess,dim) != 0 ) {
      ERRprintf("Error inverting Hessian matrix.\n");
      goto NoCanDo;
   }
   DEB(TTYprintf("Inverted Hessian (Reduced space):\n");   mat_print(dim,lv->rHess);)
   mat_mul_v( dim, lv->x0, lv->rHess, lv->rGrad );       /* x0 = rHess * rGrad */
   for (i=0; i<dim; i++) {
      lv->x0[i] = lv->x[jmin][i] - lv->x0[i];            /* x0 = x[jmin] - rHess * rGrad */
   }
   DEB(TTYprintf("Interpolated min (R-space):  ");   vec_print(dim,lv->x0);)

/* Normal vector to plane containing all points _except_ the maximum,
   Should be just a row (or is it column?) of the cmap array.
   If a row, it would work to copy a pointer without data - let's be
   slow and obvious for the moment, though.
*/
   j = jmax; if (j>jmin) j--;
   for (i=0; i<dim; i++) {
      lv->nv[i] = lv->cmap[j][i];
   }

/* Use this to determine the ideal location for next iteration.
*/
   DEB(TTYprintf("normal vector:\n");  vec_print(dim,lv->nv);)
   mat_mul_v( dim, lv->tmpv, lv->rHess, lv->nv );   /* tmpv = rHess * nv */
   DEB(TTYprintf("Hessian*normal_vector:\n");  vec_print(dim,lv->tmpv);)
   lambda = (REAL) g_sdot(dim, lv->nv, lv->x[jmin]);     /* scaling constant = nv * x[jmin] */
   r      = (REAL) g_sdot(dim, lv->nv, lv->tmpv);
   u      = (REAL) g_sdot(dim, lv->nv, lv->x0);
   DEB(TTYprintf("lambda = %f   r = %f   u = %f\n",lambda,r,u);)
   if (r<=0) {
      ERRprintf("Inconsistency in vector algebra: R=%e\n",r);
      goto NoCanDo;
   }

/* delta should at least be a user settable parameter.
   Better would be a function (implemented via gv_calc)
   which could depend on chimin, chimax, and Q0.
   Something like 1+log(chimax-Q0)
   In the linear case, our chosen point will have Chi^2 == Q0+delta
*/
   delta = 2.0;
   t = (REAL) sqrt(delta/r);
   if ( (u-lambda)*r < 0) {t = -t;}
   for (i=0; i<dim; i++) {
      lv->tmpv[i] = lv->tmpv[i] * t;
   }
   DEB(TTYprintf("tmpv:\n");  vec_print(dim,lv->tmpv);)

/* Ideal point is now x0 + tmpv.  If this is out of range, try something
   else known to be good.  t is a clipping parameter: t=1 means no clipping,
   t=0 is completely clipped, heading for disaster.  Fortran took the
   span of the line for evaluating clipping to start at x[jmin]+tmpv.
   This code is more conservative, and starts at x[jmin].  This is actually
   easier, since it is safe to assume that x[jmin] is in bounds.
*/
   t = 1.0;
   for (i=0; i<dim; i++) {
      xx = (REAL) lv->x[jmin][i];
      xd = (REAL) (lv->x0[i]+lv->tmpv[i]);
      if (xd > XMAX) { t = (REAL) min(t, (XMAX-xx)/(xd-xx) ); }
      if (xd < XMIN) { t = (REAL) min(t, (XMIN-xx)/(xd-xx) ); }
   }
   if (t < 0.01) {              /* Have to give up sometime! */
      ERRprintf("Minimum seems to be out of range! (t=%f)\n",t);
      goto NoCanDo;
   } else if (t != 1.0) {
      TTYprintf("Clip! t=%6.4f\n",t);
	}

/* Simple cheat to keep the new point off the actual edge:
   We can try other things if this performs suboptimally.
   It has never had a good test; the old Fortran code to perform
   this cheat was bungled.
   In the long run this, too, could be made a user-defined function.
*/
   t = (REAL) (1.0 - sqrt(1.0-t));

/* And apply the clip parameter to *really* generate the next point:
*/
   for (i=0; i<dim; i++) {
      lv->x[jmax][i] = t*(lv->x0[i]+lv->tmpv[i]) + (1.0-t)*lv->x[jmin][i];
   }
   DEB(TTYprintf("Next iteration   (R-space):  "); vec_print(dim,lv->x[jmax]);)

   *next_index = jmax;
/* Really crude termination condition to get us going.
   That 4.0 should be either user-settable itself, K*delta
   with K fixed and delta from above, or K user-settable.
*/
   if (chimax < (Q0+4.0) ) return(DONE);
   return(NEXT);

NoCanDo:
   return(FAILURE);
}


/* ===========================================================================
-- Some generally useful matrix manipulation routines
--
-- Included in this file since
--    (1) Only routines currently using these functions
--    (2) Allows compiler inlining optimization since short
=========================================================================== */


static void mat_mul_v(int dim, double *result, double **mat, double *vec) {
   double sum;
   int i, j;
   for (i=0; i<dim; i++) {
      sum = 0.0;
      for (j=0; j<dim; j++) sum += mat[i][j]*vec[j];
      result[i] = sum;
   }
	return;
}

static void matT_mul_v(int dim, double *result, double **mat, double *vec) {
   double sum;
   int i, j;
   for (i=0; i<dim; i++) {
      sum = 0.0;
      for (j=0; j<dim; j++) sum += mat[j][i]*vec[j];
      result[i] = sum;
   }
	return;
}

static void mat_mul_mat(int dim, double **result, double **matl, double **matr) {
   double sum;
   int i, j, k;
   for (i=0; i<dim; i++) {
      for (j=0; j<dim; j++) {
         sum = 0.0;
			for (k=0; k<dim; k++) sum += matl[i][k]*matr[k][j];
			result[i][j] = sum;
      }
   }
	return;
}

#ifdef WHEN_NEEDED_UNCOMMENT
static void mat_mul_matT(int dim, double **result, double **matl, double **matr);

static void mat_mul_matT(int dim, double **result, double **matl, double **matr) {
   double sum;
   int i, j, k;
   for (i=0; i<dim; i++) {
      for (j=0; j<dim; j++) {
         sum = 0.0;
         for (k=0; k<dim; k++) sum += matl[i][k]*matr[j][k];
			result[i][j] = sum;
      }
   }
	return;
}

#endif

static void matT_mul_mat(int dim, double **result, double **matl, double **matr) {
   double sum;
   int i, j, k;
   for (i=0; i<dim; i++) {
      for (j=0; j<dim; j++) {
         sum = 0.0;
         for (k=0; k<dim; k++) sum += matl[k][i]*matr[k][j];
			result[i][j] = sum;
		}
	}
	return;
}

#ifdef INCLUDE_DEBUG_CODE

static void vec_print(int dim, double *vector) {
   int i;
   for (i=0; i<dim; i++) TTYprintf(" %14.5e",vector[i]);
   TTYprintf("\n");
	return;
}

static void mat_print(int dim, double **matrix) {
   int i;
   for (i=0; i<dim; i++) vec_print(dim,matrix[i]);
	return;
}

#endif
