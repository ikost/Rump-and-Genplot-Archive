/*  anlyz.c */

/*  ------------------------------------------------------------------------- */
/*  ---------                                              ------------------ */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  ---------                                              ------------------ */
/*  ---------    The source code to RUMP may be freely     ------------------ */
/*  ---------  modified as long as this copyright notice   ------------------ */
/*  ---------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

#define STRAGGLE

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "sample.h"									/* Need SimStopperFoilProc() */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define DBUG(x)
#undef counts
#define counts(i,ibf) (ibf->counts)[i]

#if TIMING_CODE
#define ADD_FLOPS(n) flops += (n);
#else
#define ADD_FLOPS(n)
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void SimAnlyz3(REAL height, REAL ener, REAL de, REAL sig);
static void SimAnlyz4(REAL hback, REAL eback, REAL hfront, REAL efont);
static REAL SimStragf(REAL x, REAL sig);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


static FILE *TOF_funit = NULL;						/* Unit for TOF output */

void SimTOFInitFillSpectrum(SPECTRUM *buf) {

	if (TOF_funit != NULL) fclose(TOF_funit);
	TOF_funit = fopen("tof.data", "w");
	if (TOF_funit == NULL) {
		ERRprintf("ERROR: %s failed to open\n", "tof.data");
	} else {
		fprintf(TOF_funit, "# %f %d %f %d      E0, Z, M, Charge\n",
			buf->e0, buf->zbeam, buf->mbeam, buf->cbeam);
		fprintf(TOF_funit, "# %f %f            Q, I\n",
			buf->q, buf->current);
		fprintf(TOF_funit, "# %d %f %f %f      GEOM, phi, theta, psi\n",
			buf->geom, buf->phi, buf->theta, buf->psi);
		fprintf(TOF_funit, "# %f               omega\n",
			buf->omega);
	}
	return;
}

void SimTOFTermFillSpectrum(SPECTRUM *buf) {
	if (TOF_funit != NULL) fclose(TOF_funit);
	TOF_funit = NULL;
	return;
}

void SimTOFFillSpectrum(int z, REAL mass,
					REAL efront, REAL eback, REAL hfront, REAL hback,
					REAL qqq, REAL sigf, REAL sigb) {

	if (TOF_funit != NULL) {
		fprintf(TOF_funit, "%2d %6.2f  %7.1f %7.1f  %9.4f %9.4f  %10.4f  %7.2f %7.2f\n",
			z, mass, efront, eback, hfront, hback, qqq, sigf, sigb);
	}
	SimAnlyz(z, mass, efront, eback, hfront, hback, qqq, sigf, sigb);
	return;
}


/*      SUBROUTINE ANLYZ(EFRONT,EBACK,HFRONT,HBACK,QQQ,SIGF,SIGB) */
/* ===========================================================================
--  Usage Guide:
--     Subroutine ANLYZ cumulates a segment of a backscattering spectrum
--     into an array (the ALT part of COUNTS).  The parameters of the
--     call describe this segment: the endpoints of a curve are given by
--     their energy in keV and their height in counts/keV/uCoulomb/mSter.
--     The curve inbetween is close to a trapezoid, but the program
--     does one better: given the area, it finds the parabola with
--     the proper endpoints and area.  The parabola has the form
--     A + 2*B*E + 3*C*E**2   since we are interested in integrating
--     the thing to get counts in each channel.  To eliminate
--     complications from roundoff error, we revert to a linear fit
--     if the parabola is "narrow" (less than one channel wide).
--
-- Usage: void SimAnlyz(int z, REAL mass, 
--             REAL efront, REAL eback, REAL hfront, REAL hback,
--					REAL qqq, REAL sigf, REAL sigb);
--
-- Inputs: z,mass - Identification if particle being detected
--         efront - energy of front of 'trapezoid' (KeV)
--         eback  - energy of back  of 'trapezoid' (KeV)
--         hfront - height of front of 'trapezoid'
--         hback  - height of back  of 'trapezoid'
--         qqq    - area of the parabola
--         sigf   - sigma squared due to straggling (keV**2)
--         sigb   - straggling at back (zero for normal case)
--
-- Output: Fills in COUNTS array with yield from this piece of curve
--
--
-- COMMON BLOCKS:     RUMP
-- CALLED FROM:       CIDEAL
-- CALLS:             ANLYZ3
--
--   IMPORTANT DEFINITIONS!!!!!!!!!!!!!
--     Given a channel number N, not necessarily integral, the number
--     of counts is that number falling into the energy range
--     (N-.5)*kevch+kev0 > Energy > (N+.5)*kevch+kev0
--     Element J in the array COUNTS represents channel J+FIRST
--
--     The variable E is used to represent Energy, in keV, WITH AN
--     OFFSET such that EBACK ends up as zero.
--
--     Modified 10/3/83 for new version of RUMP which has always
--     raw data stored in the array counts.  Means a multiplication
--     by kevch(ALT) to get absolute counts.
--
=========================================================================== */
void SimAnlyz(int z, REAL mass,
					REAL efront, REAL eback, REAL hfront, REAL hback,
					REAL qqq, REAL sigf, REAL sigb) {

	REAL a, b, de;

/* ------------------------------------------------------------------------
-- Deal with potential user calibration of a stopper foil.  We simply look
-- to see if the function SimStopperFoilProc is defined. If yes, efront/eback
-- are modified accordingly.  The trapezoidal heights are scaled by the change
-- in the "energy width" so total integrated counts remains constant.  The
-- straggling through the stopper foil is added quadratically with straggle,
-- possibly turning on straggle if not otherwise on.
--------------------------------------------------------------------------- */
	if (SimStopperFoilProc != NULL) {
		de = efront - eback;							/* Save the initial width */
		(*SimStopperFoilProc)(z, mass, &efront, &a);
		if (efront < 0) return;						/* Nothing to do		*/
		(*SimStopperFoilProc)(z, mass, &eback,  &b);
		de = (efront - eback) / de;				/* Width scaling		*/
		sigf = (REAL) (sigf*pow(de,2) + a*a);	/* Additional straggle */
		sigb = (REAL) (sigb*pow(de,2) + b*b);
		hfront /= de;									/* These get larger	*/
		hback  /= de;
	}

/* ------------------------------------------------------------------------
-- Straggline calculations are checked first in this section.  We divide
-- the trapezoid (yes, giving up the parabolic nature) into two triangles
-- and call ANLYZ3 for each of them.
--
-- MOT - 7/9/94
-- Removed convolution of detector with straggle at this point.  Now, even
-- with straggle, will have convolution in main routine.  Necessary to deal
-- with additional straggling introduced in the stopper foil above.
--		a = 0.18031*pow(ALTBUF->fwhm, 2);		-- Detector intrinsic width --
--    sqrt(sigb) -> sqrt(sigb+a);
--    sqrt(sigf) -> sqrt(sigf+a);
------------------------------------------------------------------------- */

	if (sigb == 0.0 && sigf == 0.0) {			/* No straggle, do simply */
		SimAnlyz4(hback, eback, hfront, efront);

	} else {
		de = efront - eback;
		SimAnlyz3(hback,  eback,   de, (REAL) sqrt(sigb));
#ifdef CSET2											/* Deal with compiler bug		*/
		b = sqrt(sigf);								/* Effective width				*/
		SimAnlyz3(hfront, efront, -de, b);		/* call->jmp optimize error	*/
#else
		SimAnlyz3(hfront, efront, -de, (REAL) sqrt(sigf));
#endif

	}
	return;

}


/* ===========================================================================
--  Usage Guide:
--      SUBROUTINE ANLYZ3(HEIGHT,ENER,DE,SIG)
--     Subroutine Anlyz3 adds a rounded triangle to the ALT buffer.
--     The peak of the unadulterated triangle is HEIGHT high, and
--     occurs at energy ENER.  The distance to its base is DE, and
--     the triangle is convoluted with a Gaussian of width SIG.
--  Quick: Straggling version of ANLYZ
--
--     INPUTS:   HEIGHT   Height of triangle
--               ENER     Peak energy of triangle (keV)
--               DE       Distance to base of triangle (keV)
--               SIG      Sigma for Gaussian
--
--     OUTPUTS:  Summation into array COUNTS of the yield of this curve.
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       ANLYZ
--     CALLS:             STRAGF
--
=========================================================================== */
static void SimAnlyz3(REAL height, REAL ener, REAL de, REAL sig) {

	REAL asig,gral,gral2,a,b,c,fact;
	int j,jmin,jmax;
/*	static FILE *funit=NULL; */


/*	if (funit == NULL) funit = fopen("g:/xxx", "w"); */
	
/*	fprintf(funit, "Calling SimAnlyz3 with height: %.3f  ener: %.3f  de: %.3f  sig: %.3f\n", height, ener, de, sig); */

	asig = (REAL) (1.414214 * sig / fabs(de));

	fact = (REAL) (1.0 / (fabs(de) + 4.2426402 * sig));
	a = fact * ALTBUF->kevch;
	b = ((0.5f + ALTBUF->first)*ALTBUF->kevch + ALTBUF->kev0 - ener) * fact;
	c = (REAL) (height * fabs(de));
	jmin = (int) (-(1+b)/a + 1);
	jmax = (int) ( (1-b)/a);
	DBUG(printf("Index range for triangle: [%d,%d]",jmin,jmax);)

	if (jmin < 0) {
		jmin = 0;
		gral = SimStragf(b-a, asig);
	} else {
		gral = -0.25;
	}
	jmax = min(jmax, ALTBUF->npt-1);
	DBUG(printf(" -> [%d,%d]\n",jmin,jmax);)

	if (de >= 0.0) {

/*  First case: DE greater than zero, channel loop scans from X=-1 to 1 */
		for (j=jmin; j<=jmax ; j++) {
			gral2 = SimStragf(a*j+b,asig);
			ALTBUF->counts[j] += c*(gral2-gral);
/*			fprintf(funit, "  %d  %f\n", j, c*(gral2-gral)); */
			gral = gral2;
		}

/*  Second case: DE less than zero, channel loop scans from X=1 to -1 */
	} else {
		for (j=jmin; j<=jmax ; j++) {
			gral2 = -SimStragf(-a*j-b,asig);
			ALTBUF->counts[j] += c*(gral2-gral);
/*			fprintf(funit, "  %d  %f\n", j, c*(gral2-gral)); */
			gral = gral2;
		}
	}

/* Either case: touch up the end */
	if (jmax >= 0 && jmax+1 < ALTBUF->npt) {
		ALTBUF->counts[jmax+1] += c*(0.25f - gral);
/*		fprintf(funit, "  %d  %f\n", jmax+1, c*(0.25f - gral)); */
	}

	return;
}


static void SimAnlyz4(REAL hback, REAL eback, REAL hfront, REAL efront) {

	int i, k0, k1;
	double slope, h0, h1, e0, e1;

	if (eback >= efront) return;										/* Avoid divide by zero */
	
	slope = (hfront-hback) / (efront-eback);						/* Slope of function		*/

/* Calculate channel corresponding to highest energy */
	k1 = (int) ((efront-ALTBUF->kev0)/ALTBUF->kevch - ALTBUF->first + 1);
	if (k1 >= ALTBUF->npt) {
		TTYprintf("WARNING: Energy out of range on exit: %6.1f keV\n", efront);
		return;
	} else if (k1 < 0) {
		return;
	}

/* Now, calculate lowest channel - checking if less than zero */
	e0 = ALTBUF->first*ALTBUF->kevch + ALTBUF->kev0;				/* Energy of channel zero	*/
	if (eback < e0) {															/* Shift to channel zero	*/
		hback = (REAL) (hback + slope*(e0-eback));					/* Go up the slope level	*/
		eback = (REAL) e0;
	}
	k0 = (int) ((eback -ALTBUF->kev0)/ALTBUF->kevch - ALTBUF->first);

	e0 = eback;																	/* Energy at k0			*/
	h0 = hback;																	/* Height at back edge	*/
	e1 = (k0+1+ALTBUF->first)*ALTBUF->kevch + ALTBUF->kev0;		/* Energy at k0+1			*/
	for (i=k0; i<k1; i++) {
		if (e1 > efront) e1 = efront;										/* Screws up incrementing of e1, but only when ready to exit */
		h1 = hback + slope*(e1-eback);
		ALTBUF->counts[i] += (REAL) ((e1-e0)*(h1+h0)/2.0);			/* Area of trapezoid		*/
		h0 = h1;																	/* Height next = last	*/
		e0 = e1;																	/* Energy next = last	*/
		e1 += ALTBUF->kevch;													/* Energy next +1			*/
	}

	return;
}

/* ===========================================================================
--  Usage Guide:
--      REAL FUNCTION STRAGF(X,SIG)
--     This function computes, rapidly and accurately, the integral of
--     the convolution of a triangle with a Gaussian.  The triangle has
--     a height of 1 for x=0, and height of 0 for x<0 or x>1.  The Gaussian
--     has a standard deviation SIG.
--  Quick: Numerical evaluation code for ANLYZ3
--
--     INPUTS:   X        Point to evaluate function
--               SIG      Standard deviation of Gaussian
--
--     OUTPUTS:  Function value
--
--     COMMON BLOCKS:     None
--     CALLED FROM:       ANLYZ3
--     CALLS:             EXP - Lots and lots
--
-- MOT - 12/18/93 - There has been some loss of "efficiency" possibly due
--       to rewriting this routine to obvious flow patterns.  Original coding
--       made use of jumps to low probability paths with minimal tests, new
--       version has straight series of comparison.  Maintainability wins.
=========================================================================== */
#define FXMAX 1E20					/* Maximum range */
#define EXMAX 78.0					/* Was 81.0 */

static REAL SimStragf( REAL pass_x, REAL pass_sig) {

	double x, sig, s1,s2,newx,ex,fs1,fs2,gs2,sign,c,x2,result;

	x = pass_x;							/* Convert to double precision */
	sig = pass_sig;

	if (sig == 0.0) {					/* Special case */
		result = -.25;
		if (x > 0.) result = x - .5*x*x - .25;
		return((REAL) result);
	}

	newx = x * (1.0+3.0*sig);
	s1 = (newx-1.0)/sig;
	s2 = newx / sig;

	if (s1+s2 >= 0) {
		c = .25;
		sign = -1.0;
	} else {
		c = -.25;
		sign = 1.0;
		s1 = -s1;
		s2 = -s2;
	}

/*  EVALUATION of f(S1) ************************** */
	x = (s1 < -FXMAX) ? -FXMAX : (s1 > FXMAX) ? FXMAX : s1 ;
	x2 = x*x;

	if (x <= -3.3) {
		fs1 = x2 + .5;
	} else if (x <= -1.65) {
		fs1 = x2 + .5 - 4.843118e-03 * exp(-x2) *
			(x+ 9.289928e00)
			/((x+ 7.112969e-01)*x+ 8.014199e-01);
	} else if (x <= 0) {
		fs1 = 8.425989e00 *
			((((x-4.127972e00)*x+8.483711e00)*x-8.990629e00)*x+
			3.986747e00)
			/(((x+1.763421e01)*x+2.169319e-01)*x+1.343694e02);
	} else if (x <= 1.65) {
		fs1 =  1.786449e-03 * exp(-x2) *
			((((x-1.106165e01)*x+5.626102e01)*x-1.699317e02)*x+
			3.936660e02)
			/(((x+3.547880e00)*x+5.134192e00)*x+2.813056e00);
	} else if (x <= 3.3) {
		fs1 =  3.479073e-04 * exp(-x2) *
			(((x-1.599709e01)*x+ 5.094402e02)*x-7.972751e01)
			/(((x+6.123506e-01)*x+ 2.125519e00)* x * x);
	} else {
		ex = 0.;
		if (x2 < EXMAX) ex = exp(-x2);
		fs1 =  1.204197e00 * .1410474 * ex *
			(1. + x2*(6.173817e-01 + x2*7.557103e-02))
			/((1. + x2*(7.434484e-01 + x2*9.100243e-02)) * x * (x2+3.));
	}

/*  EVALUATION of f(S2) and g(S2) ****************************** */
	x = (s2 < -FXMAX) ? -FXMAX : (s2 > FXMAX) ? FXMAX : s2 ;
	x2 = x*x;
	ex = 0.;

	if (x <= -3.3) {
		fs2 = x2 + .5;
		gs2 = -2.*x;
	} else if (x <= -1.65) {
		ex = exp(-x2);
		fs2 = x2 + .5 - 4.843118e-03 * ex *
			(x+ 9.289928e00)
			/((x+ 7.112969e-01)*x+ 8.014199e-01);
		gs2 = -2.*x - 4.682351e-04 * ex *
			(((-x- 1.583414e01)*x- 1.053951e02)*x- 9.761400e02)
			/((x- 1.159636e00)*x+ 8.771139e-01);
	} else if (x <= 0) {
		fs2 = 8.425989e00 *
			((((x-4.127972e00)*x+8.483711e00)*x-8.990629e00)*x+
			3.986747e00)
			/(((x+1.763421e01)*x+2.169319e-01)*x+1.343694e02);
		gs2 = -2.*x + 1.896751e-03 * exp(-x2) *
			((((x+1.184974e01)*x+ 6.605484e01)*x+7.836549e01)*x+
			1.119263e03)
			/(((-x+ 4.050289e00)*x- 6.406059e00)*x+ 3.762856e00);
	} else if (x <= 1.65) {
		ex = exp(-x2);
		fs2 =  1.786449e-03 * ex *
			((((x-1.106165e01)*x+5.626102e01)*x-1.699317e02)*x+
			3.936660e02)
			/(((x+3.547880e00)*x+5.134192e00)*x+2.813056e00);
		gs2 =  1.896751e-03 * ex *
			((((x-1.184974e01)*x+ 6.605484e01)*x-7.836549e01)*x+
			1.119263e03)
			/(((x+ 4.050289e00)*x+ 6.406059e00)*x+ 3.762856e00);
	} else if (x <= 3.3) {
		ex = exp(-x2);
		fs2 =  3.479073e-04 * ex *
			(((x-1.599709e01)*x+ 5.094402e02)*x-7.972751e01)
			/(((x+6.123506e-01)*x+ 2.125519e00)* x2);
		gs2 = -4.682351e-04 * ex *
			(((x- 1.583414e01)*x+ 1.053951e02)*x- 9.761400e02)
			/((x+ 1.159636e00)*x+ 8.771139e-01);
	} else {
		if (x2 < EXMAX) ex = exp(-x2);
		fs2 =  1.204197e00 * .1410474 * ex *
			(1. + x2*(6.173817e-01 + x2*7.557103e-02))
			/((1. + x2*(7.434484e-01 + x2*9.100243e-02)) * x * (x2+3.));
		gs2 =  1.356946e-01 * ex *
			(1+x2*(-1.126888e00+x2*( 1.280896e00+x2 * 5.320120e-01)))
			/((1. + x2*2.559110e-01)*x2*x2*x2);
	}

/*  We now have FS1 as f(S1), FS2 as f(S2), and GS2 as g(S2) */
/*  Evaluate STRAGF in terms of these. */
	result = .5 * sig * (sig * sign * (fs1 - fs2) + gs2) + c;
	return((REAL) result);
}




/* =========================================================================== */
/* ---------------------------- OBSOLETED CODE ------------------------------- */
/* =========================================================================== */

#if 0	


/* --------------- no longer using Doolittle qqq code ------------------- */

#ifdef STRAGGLE
	if (sigb != 0.0 || sigf != 0.0) {			/* Have to do straggling case */
		de = efront - eback;
		SimAnlyz3(hback,  eback,   de, (REAL) sqrt(sigb));
#ifdef CSET2											/* Deal with compiler bug		*/
		b = sqrt(sigf);								/* Effective width				*/
		SimAnlyz3(hfront, efront, -de, b);		/* call->jmp optimize error	*/
#else
		SimAnlyz3(hfront, efront, -de, (REAL) sqrt(sigf));
#endif
		return;
	}
#endif


REAL gral, gral2, c, e, eoffst
int j, k0, k1, kmin, kmax;

/* Handle non-straggling case here */
	k0 = (int) ((eback -ALTBUF->kev0)/ALTBUF->kevch - ALTBUF->first + 0.5);
	k1 = (int) ((efront-ALTBUF->kev0)/ALTBUF->kevch - ALTBUF->first + 0.5);

	if (k1 >= ALTBUF->npt) {
		TTYprintf("WARNING: Energy out of range on exit: %6.1f keV\n", efront);
		return;
	} else if (k1 < 0) {
		return;
	}

/* ----------------------------------------------------------------------
-- ONE CHANNEL WIDE - VERY EASY.  But, gets us into trouble with those
-- who want their small peaks to move around smoothly so that Pert can
-- find their center accurately.  Those folks should turn on straggling.
---------------------------------------------------------------------- */
	if (k0 == k1) {
		ALTBUF->counts[k1] += (hfront+hback)*0.5f*(efront-eback);
		return;
	}

#define DOOLITTLE_PARABOLIC_FORM
#ifdef DOOLITTLE_PARABOLIC_FORM

/*	TTYprintf("hfront: %g   hback: %g  efront: %g   eback: %g  qqq: %g\n",
				 hfront, hback, efront, eback, qqq);
*/
	de = efront - eback;
	c = (REAL) ((de > ALTBUF->kevch) ? (((hfront+hback)*de-2*qqq)/pow(de,3)) : 0) ;
	b = 0.5f * (hfront-hback) / de - 1.5f*c*de;

	a = hback;
/* ... Refine value for C so that rounding errors don't mess us up. */
	c = (REAL) (( qqq - de*(a+de*b) ) / pow(de,3));
	eoffst = (0.5f + ALTBUF->first) * ALTBUF->kevch + ALTBUF->kev0 - eback;
	kmin = k0;
	e = 0.0;
	if (k0 <= 0) {
		kmin = 1;
		e = eoffst;
	}
	gral = ((c*e + b)*e + a)*e;
	kmax = k1-1;
	ADD_FLOPS( 9*(kmax-kmin+1) )

	for (j=kmin; j<=kmax; j++) {
		e = j * ALTBUF->kevch  +  eoffst;
		gral2 = ((c*e + b)*e + a)*e;
		ALTBUF->counts[j] += gral2 - gral;
		gral = gral2;
	}

	gral = qqq - gral2;
	if (gral >= 0.0) {
		ALTBUF->counts[k1] += gral;
	} else {
/*		TTYprintf("WARNING: Simulation subject to slight roundoff errors\n"); */
	}

#else

/* ===========================================================================
-- Alternate form.  The parabolic has a problem in having to "drop down" under
-- some conditions of rapidly increasing dE/dx with decreasing energy.  Use
-- an alternate form 
--
--    h(x) = h_b + (h_f-h_b)*x^m    x = (E-E_b)/(E_f-E_b)
--
-- Again, it guarentees that the front and back edges are exact, but the
-- shape is now smooth between the limits and is guarenteed to be monotonic
-- in between.  This has the potential problem of not satisfying the area
-- constraint, but that pathological condition should not happen since for
-- m=0 the value is everywhere h_f, while for m=1000, it is h_b everywhere.
-- Negative values of m will be disallowed since it would be ugly.
--
-- The integral of the above function is:
--
--    area = [E_f-E_b] * [H_b + (H_f-H_b)/(m+1)]
*/
	de = efront - eback;
	a = (qqq/de-hback) / (hfront-hback);			/* 1/(m+1) */
	m = (a>1) ? 
	m = (a<1 && a>0) ? 1.0/a-1.0 : 
	if (a>1) a = 0.9999;
	a = 



	c = (de > ALTBUF->kevch) ? (((hfront+hback)*de-2*qqq)/pow(de,3)) : 0 ;
	b = .5 * (hfront-hback) / de - 1.5*c*de;

	a = hback;
/* ... Refine value for C so that rounding errors don't mess us up. */
	c = ( qqq - de*(a+de*b) ) / pow(de,3);
	eoffst = (0.5 + ALTBUF->first) * ALTBUF->kevch + ALTBUF->kev0 - eback;
	kmin = k0;
	e = 0.0;
	if (k0 <= 0) {
		kmin = 1;
		e = eoffst;
	}
	gral = ((c*e + b)*e + a)*e;
	kmax = k1-1;
	ADD_FLOPS( 9*(kmax-kmin+1) )

	for (j=kmin; j<=kmax; j++) {
		e = j * ALTBUF->kevch  +  eoffst;
		gral2 = ((c*e + b)*e + a)*e;
		ALTBUF->counts[j] += gral2 - gral;
		gral = gral2;
	}

	gral = qqq - gral2;
	if (gral >= 0.0) {
		ALTBUF->counts[k1] += gral;
	} else {
		TTYprintf("WARNING: Simulation subject to slight roundoff errors\n"); */
	}
#endif	

	return;
}

#endif /* 0 */

