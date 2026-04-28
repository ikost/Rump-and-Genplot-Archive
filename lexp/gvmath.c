/* gvmath.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _spline_element {
	REAL cf[3];								/* Coefficients of the spline */
	REAL x,y;								/* x,y values at the starting knot */
} SPLINE;

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


/* ===========================================================================
-- Heap sort for spline coefficients 
=========================================================================== */
static void spl_sort(SPLINE *spl, int npt) {

	INT i,j,hire,ir;
	SPLINE tmp;

/* s is the array being sorted (normally X).  This allows treating all arrays
   symmetrically in the copy operations.  Note that the "s" array is always
	left in a sorted order */
	hire = npt/2;												/* Center of data set	*/
	ir   = npt-1;												/* On the way down		*/

	while (TRUE) {												/* Repeat forever			*/
		if (hire > 0) {										/* Hiring, walk down		*/
			hire--;
			tmp = spl[hire];
		} else {
			tmp = spl[ir];
			spl[ir] = spl[0];
			ir--;
			if (ir == 0) {
				spl[0] = tmp;
				return;
			}
		}

		i = hire;
		j = 2*hire+1;

		while (TRUE) {
			if (j > ir) {								/* Last element? */
				spl[i] = tmp;
				break;
			}
			if (j < ir) {								/* Repeat to put this in place */
				if (spl[j].x < spl[j+1].x) j++;
			}
			if (tmp.x < spl[j].x) {
				spl[i] = spl[j];
				i = j;
				j = 2*j+1;
			} else {
				j = ir+1;
			}
		}
	}
}


/* ============================================================================
-- ... Simple spline determination
--
-- Usage:   void *GVFitSpline(void *work, REAL *x, REAL *y, int npt, int opts);
--
-- Inputs:  X    - X coordinates - normally strictly sorted, but not always
--          Y    - Y coordinates
--          npt  - number of elements in x and y.  Must be greater than 3
--          work - pointer to memory block to receive SPLINE description
--                 structures.  Basically knots and coefficients.
--          opts - bit-wise options
--                   0x01 -> do piecewise linear instead of cubic, but
--                           keep full structure so can be used elsewhere.
--
-- Output:  work - filled with series of SPLINE structures with X,Y and
--                 cubic coefficients.  This array of structures is to be
--                 passed to evaluation the spline.
--
-- Returns: pointer to work NULL, or an malloc'd space for coefficients.  On
--          error, returns NULL.
--
-- Notes: (1) The work array (or the malloc'd replacement) contains the
--            spline coefficients and data.  Each element of the array
--            points to a structure containing:
--              struct _spline_element {
--					    REAL x, y, c[3];
--  				  }
--            where the value of the spline at T Is 
--		           s(T) = (((elem.c[2]*d + elem.c[1])*d + elem.c[0])*d) + elem.y;
--					  elem.x <= T < (elem+1).x
--               d = T-elem.x
--        (2) The X value of the last point is reset to be +REAL_MAX so that
--            any X given to FitEvalSpline will correctly stop.
============================================================================ */
void *GVFitSpline(void *work, REAL *x, REAL *y, int npt, int opts) { 

	SPLINE *spl;
	double t1,t2,tm0,tm1,tm2,tmp1,tmp2;			/* Bunch of temp variables */
	BOOL bad_sort=FALSE;
	int i;

/* Validity tests */
	if (npt < 3) return(NULL);
	
/* Create valid pointer to an array of spline structures */
	spl = (SPLINE *) work;
	if (spl == NULL) spl = malloc(npt*sizeof(*spl));

/* First elements */
	for (i=0; i<npt; i++) {									/* Copy over the x,y			*/
		if (i>0 && x[i]<=x[i-1]) bad_sort = TRUE;
		spl[i].x = x[i];
		spl[i].y = y[i];
	}
	if (bad_sort) {
		spl_sort(spl, npt);									/* Try to fix */
		for (i=1; i<npt; i++) { if (spl[i].x<=spl[i-1].x) break; }
		if (i != npt) {
			ERRprintf("ERROR: Data for spline fit must have unique X coordinates.\n");
			if (work == NULL) free(spl);
			return(NULL);
		}
	}

/* If faking as piecewise linear, do now */
	if (opts & 0x01) {										/* Piecewise linear only */
		for (i=0; i<npt-1; i++) {
			spl[i].cf[0] = (spl[i+1].y-spl[i].y)/(spl[i+1].x-spl[i].x);	/* Start of RHS solution	*/
			spl[i].cf[1] = spl[i].cf[2] = 0;
		}
		spl[npt-1].x = REAL_MAX;							/* Terminating condition */
		spl[npt-1].cf[0] = spl[npt-2].cf[0];
		spl[npt-1].cf[1] = spl[npt-1].cf[2] = 0;
		return(spl);
	}

/* ... Compute not-a-knot spline */
	for (i=1; i<npt; i++) {									/* Fill in matrix first		*/
		spl[i].cf[0] = 0.0f;									/* Just so has some value	*/
		spl[i].cf[1] = spl[i].x-spl[i-1].x;				/* h(j) in Stoer notation	*/
		spl[i].cf[2] = (spl[i].y-spl[i-1].y)/(spl[i].x-spl[i-1].x);	/* Start of RHS solution	*/
	}

/* Continue with not-a-knot spline - complete first block */
	spl[0].cf[2] = spl[2].cf[1];							/* Duplicate 2nd point */
	spl[0].cf[1] = spl[1].cf[1] + spl[2].cf[1];		/* And sum of first two */
	spl[0].cf[0] = ((spl[1].cf[1]+2*spl[0].cf[1])*spl[1].cf[2]*spl[2].cf[1] +
		              spl[1].cf[1]*spl[1].cf[1]*spl[2].cf[2])/spl[0].cf[1];

	tm1 = spl[npt-1].cf[1];
	tm2 = spl[npt-1].cf[2];

	for (i=1; i<npt-2; i++) {
		t1 = -spl[i+1].cf[1]/spl[i-1].cf[2];
		spl[i].cf[0] = GVTrimToReal(t1*spl[i-1].cf[0] + 3*(spl[i].cf[1]*spl[i+1].cf[2] + spl[i+1].cf[1]*spl[i].cf[2]));
		spl[i].cf[2] = GVTrimToReal(t1*spl[i-1].cf[1] + 2*(spl[i].cf[1]+spl[i+1].cf[1]));
	}

	t1  = -tm1/spl[npt-3].cf[2];
	spl[npt-2].cf[0] = GVTrimToReal(t1*spl[npt-3].cf[0] + 3*(spl[npt-2].cf[1]*tm2 + tm1*spl[npt-2].cf[2]));
	spl[npt-2].cf[2] = GVTrimToReal(t1*spl[npt-3].cf[1] + 2*(spl[npt-2].cf[1]+tm1));
	t1  = spl[npt-2].cf[1] + tm1;
	tm0 = ((tm1+2*t1)*tm2*spl[npt-2].cf[1]+tm1*tm1*(spl[npt-2].y-spl[npt-3].y)/spl[npt-2].cf[1])/t1;
	t1  = -t1/spl[npt-2].cf[2];
	tm2 = spl[npt-2].cf[1];
	tm2 = t1*spl[npt-2].cf[1]+tm2;
	tm0 = (t1*spl[npt-2].cf[0]+tm0)/tm2;
	spl[npt-2].cf[0] = GVTrimToReal((spl[npt-2].cf[0]-spl[npt-2].cf[1]*tm0)/spl[npt-2].cf[2]);

	for (i=npt-3; i>=0; i--) 
		spl[i].cf[0] = GVTrimToReal((spl[i].cf[0]-spl[i].cf[1]*spl[i+1].cf[0]) / spl[i].cf[2]);

	for (i=1; i<npt-1; i++) {
		t2   = spl[i].cf[1];
		tmp1 = (spl[i].y-spl[i-1].y)/t2;
		tmp2 = spl[i-1].cf[0]+spl[i].cf[0]-2*tmp1;	
		spl[i-1].cf[1] = GVTrimToReal((tmp1-spl[i-1].cf[0]-tmp2)/t2);
		spl[i-1].cf[2] = GVTrimToReal(tmp2/t2/t2);
	}

	t2   = tm1;
	tmp1 = (spl[npt-1].y-spl[npt-2].y)/t2;
	tmp2 = spl[npt-2].cf[0]+tm0-2*tmp1;
	spl[npt-2].cf[1] = GVTrimToReal((tmp1-spl[npt-2].cf[0]-tmp2)/t2);
	spl[npt-2].cf[2] = GVTrimToReal(tmp2/t2/t2);

	spl[npt-1].x = REAL_MAX;							/* Terminating condition */
	return(spl);
}


/* ============================================================================
-- Cubic spline data smoother
--
-- Usage: void *GVFitSmoothSpline(void *spl, REAL *x, REAL *y, int npt, REAL error, BOOL silent)
--
-- Inputs: X,Y    - curve of data (x sorted strictly)
--         npt    - number of points
--         error  - error coefficient which determines smoothing.  Spline set 
--                  to least curved set such that sigma is less than ERROR.
--         spl    - pointer to workspace to hold spline data and coefficients
--                  or NULL if routine is to allocate independently.
--         silent - if FALSE, prints iteration count as spline is calculated.
--
-- Output: spl[].cf[3] - Spline coefficients
--         spl[].x     - Original knots (X data)
--         spl[].y     - Smoothed Y values
--
-- Return: Returns pointer to spl or malloc'd space if successful.  Returns
--         false on error conditions.
--
-- 12/28/92 - MOT
--    Changed internal variables to double.  Had underflow condition occur
--    on ff in summing loop at end.  Painful.  This allows spline -smooth to
--    get to equivalent of spline with small error, but still may crash with
--    math exception error for error >> reasonable.  Too bad.  Would probably
--    require making wk double also -- doubling memory requirements.
============================================================================ */
#define	MAXITER	50							/* Maximum of 50 iterations */

void *GVFitSmoothSpline(void *work, REAL *x, REAL *y, int npt, REAL error, BOOL silent) {

	INT  i,IterationCount;
	double e,ff,f2,g,h,hmg,p;
	double (*wk)[7];							/* Pointer to array w/ 7 elements */
	REAL *xtmp=NULL;
	SPLINE *spl;
	
/* Validity tests */
	if (npt < 3) return(NULL);
	if (error <= 0) return(GVFitSpline(work, x,y,npt, 0));

/* Create valid pointer to an array of spline structures */
	spl = (SPLINE *) work;
	if (spl == NULL) spl = calloc(npt,sizeof(*spl));
	
/* Allocate additional workspace for this routine. */
	wk = (double (*)[7]) malloc(sizeof(*wk)*(npt+2));

/* Check the entering x,y data validity */
	for (i=1; i<npt; i++) {
		if (x[i] <= x[i-1]) break;
	}
	if (i != npt) {								/* Data sort was bad */
		for (i=0; i<npt; i++) { spl[i].x = x[i]; spl[i].y = y[i]; }
		spl_sort(spl, npt);
		for (i=0; i<npt; i++) { if (spl[i].x <= spl[i-1].x) break; }
		if (i != npt) {							/* Terribly bad data */
			if (work == NULL) free(spl);
			free(wk);
			ERRprintf("ERROR: Data for spline fit must have unique X coordinates.\n");
			return(NULL);
		}
		xtmp = malloc(2*npt*sizeof(*x));
		for (i=0; i<npt; i++) {					/* Use my copied data - free later	*/
			xtmp[i]     = x[i];					/* X immediate								*/
			xtmp[i+npt] = y[i];					/* Y in later half of array			*/
		}
		x = xtmp;
		y = xtmp+npt;
	}

/* And check */
	if (wk == NULL || spl == NULL) {
		if (work == NULL) free(spl);
		free(wk);
		free(xtmp);
		ERRprintf("ERROR: Unable to allocate space for the smoothing spline\n");
		return(NULL);
	}

	wk[0][0]     = 0.0;							/* Initialize the arrays */
	wk[1][0]     = 0.0;
	wk[npt][1]   = 0.0;
	wk[npt][2]   = 0.0;
	wk[npt+1][2] = 0.0;
	wk[0][5]     = 0.0;
	wk[1][5]     = 0.0;
	wk[npt][5]	 = 0.0;
	wk[npt+1][5] = 0.0;

	p = 0.0f;
	h = x[1]-x[0];									/* Fill in the matrix for start */
	f2 = -error;
	ff = (y[1]-y[0])/h;
	for (i=2; i<npt; i++) {
		g = h;
		h = x[i]-x[i-1];
		e = ff;
		ff = (y[i]-y[i-1])/h;
		spl[i].y = GVTrimToReal(ff-e);
		wk[i][3] = 2*(g+h)/3;
		wk[i][4] = h/3;
		wk[i][2] = 1/g;							/* df(i-2)/g */
		wk[i][0] = 1/h;							/* df(i)/h */
		wk[i][1] = -1/g-1/h;						/* -df(i-1)*(1/g+1/h) */
	}
	for (i=2; i<npt; i++) {
		spl[i-1].cf[0] = GVTrimToReal(wk[i][0]*wk[i][0]+wk[i][1]*wk[i][1]+wk[i][2]*wk[i][2]);
		spl[i-1].cf[1] = GVTrimToReal(wk[i][0]*wk[i+1][1]+wk[i][1]*wk[i+1][2]);
		spl[i-1].cf[2] = GVTrimToReal(wk[i][0]*wk[i+2][2]);
	}

/* ... NEXT ITERATION */
	for (IterationCount=0; IterationCount < MAXITER; IterationCount++) {
		if (! silent) TTYprintf("[%i]", IterationCount);

		for (i=2; i<npt; i++) {
			wk[i-1][1] = ff*wk[i-1][0];
			wk[i-2][2] = g*wk[i-2][0];
			wk[i][0] = 1.0f/(p*spl[i-1].cf[0]+wk[i][3]-ff*wk[i-1][1]-g*wk[i-2][2]);
			wk[i][5] = spl[i].y-wk[i-1][1]*wk[i-1][5]-wk[i-2][2]*wk[i-2][5];
			ff = p*spl[i-1].cf[1]+wk[i][4]-h*wk[i-1][1];
			g = h;
			h = spl[i-1].cf[2]*p;
		}
		for (i=npt-1; i>=2; i--)
			wk[i][5] = wk[i][0]*wk[i][5]-wk[i][1]*wk[i+1][5]-wk[i][2]*wk[i+2][5];
		e = 0.0f;
		h = 0.0f;
/* Compute U and accumulate E */
		for (i=1; i<npt; i++) {
			g = h;
			h = (wk[i+1][5]-wk[i][5])/(x[i]-x[i-1]);
			hmg = h-g;
			wk[i][6] = hmg;							/* hmg*df(i-1)*df(i-1) */
			e += hmg*hmg;
		}

		g = -h;											/* *df(npt)*df(npt) */
		wk[npt][6] = g;
		e = e-g*h;
		g = f2;
		f2 = e*p*p;
		if (f2 >= error || f2 <= g) break;
		ff = 0.0f;
		h = (wk[2][6]-wk[1][6])/(x[1]-x[0]);
		for (i=2; i<npt; i++) {
			g = h;
			h = (wk[i+1][6]-wk[i][6])/(x[i]-x[i-1]);
			g = h-g-wk[i-1][1]*wk[i-1][0]-wk[i-2][2]*wk[i-2][0];
			ff += g*wk[i][0]*g;
			wk[i][0] = g;
		}
		h = e-p*ff;

/* ... Either update the LAGRANGE multiplier P for next iteration and continue
   ... or compute coefficients and return here when E is <= to S */
		if (h < 0.0f) break;					/* No update of Lagrange multiplier */
		p += (error-f2)/((sqrt(error/e)+p)*h);
	}
	if (! silent) TTYprintf("\n");
	if (IterationCount >= MAXITER)
		ERRprintf("WARNING: Too many iterations for SPLINE -SMOOTH\n");

/* ... Done, copy over the coefficients */
	for (i=0; i<npt-1; i++) {
		spl[i].y    = GVTrimToReal(y[i]-p*wk[i+1][6]);
		spl[i].cf[1] = GVTrimToReal(wk[i+1][5]);
		wk[i][0]    = spl[i].y;
	}
	wk[npt-1][0] = y[npt-1]-p*wk[npt][6];
	spl[npt-1].y = GVTrimToReal(wk[npt-1][0]);
	spl[npt-1].x = x[npt-1];

	for (i=1; i<npt; i++) {									/* Store the results */
		spl[i-1].x     = x[i-1];
		h              = x[i]-x[i-1];
		spl[i-1].cf[2] = GVTrimToReal((wk[i+1][5]-spl[i-1].cf[1])/(3*h));
		spl[i-1].cf[0] = GVTrimToReal((wk[i][0]-spl[i-1].y)/h-(h*spl[i-1].cf[2]+spl[i-1].cf[1])*h);
	}
	free(wk);
	free(xtmp);

	spl[npt-1].x = REAL_MAX;							/* Terminating condition */
	return(spl);
}


/* ============================================================================
-- Evaluate a spline function
--
-- Usage: void *GVEvalSpline(spl, x);
--
-- Inputs: x   - x value to evaluate spline
--         spl - pointer to workspace with spline data and coefficients
--
-- Output: none
--
-- Return: Returns value of spline at specified point
============================================================================ */
REAL GVEvalSpline(void *work, REAL x) {
	
	SPLINE *spl;

	spl = (SPLINE *) work;						/* Just rename it */
	while (x > spl[1].x) spl++;				/* Should terminate by REAL_MAX */

	x = x - spl->x;								/* Distance from knot */
	return ( ((spl->cf[2]*x + spl->cf[1])*x + spl->cf[0])*x + spl->y );
}


/* ============================================================================
-- Evaluate the integral of a spline function
--
-- Usage: void *GVEvalSplineIntegral(spl, xlow, xhigh);
--
-- Inputs: spl - pointer to workspace with spline data and coefficients
--         xlow  - lower limit of integral
--         xhigh - upper limit of integral
--
-- Output: none
--
-- Return: Returns integral of spline between limits (fully valid)
============================================================================ */
REAL GVEvalSplineIntegral(void *work, REAL xlow, REAL xhigh) {
	
	SPLINE *spl;
	BOOL invert;
	double x1, x2, integral;

/* Check for trivial/reversed arguments */
	if (xlow == xhigh) {
		return(0.0f);

	} else if ( (invert = (xhigh < xlow)) ) {
		x1 = xlow;
		xlow = xhigh;
		xhigh = (REAL) x1;
	}

/* Locate which segment each point lies within */
	spl = (SPLINE *) work;						/* Just rename it */
	while (xlow > spl[1].x) spl++;			/* Should terminate by REAL_MAX */

/* Now, just sum across elements */
	for (integral=0.0f; xlow<xhigh; spl++) {
		x1 = xlow - spl->x;
		x2 = ((xhigh < spl[1].x) ? xhigh : spl[1].x) - spl->x;
		integral += spl->cf[2]*(pow(x2,4)-pow(x1,4))/4.0 + 
						spl->cf[1]*(pow(x2,3)-pow(x1,3))/3.0 +
						spl->cf[0]*(x2*x2-x1*x1)        /2.0 +
						spl->y    *(x2-x1);
		xlow = spl[1].x;
	}

	if (invert) integral = -integral;
	return(GVTrimToReal(integral));
}
