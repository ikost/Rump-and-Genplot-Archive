/* fft2c.c */

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
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "fft2c.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef TMPREAL MYTMP;

#ifdef TMPREAL_IS_LONG
	#define	SIN(x)	sinl(x)
	#define	LOG10(x)	((MYTMP) ( ((x) != 0) ? log10l(fabsl(x)) : -40.0))
	#define	FABS(x)	fabsl(x)
	#ifndef PI
		#define	PI			3.14159265358979323846L
	#endif
#else
	#define	SIN(x)	sin(x)
	#define	LOG10(x)	((MYTMP) ( ((x) != 0) ? log10(fabs(x)) : -40.0))
	#define	FABS(x)	fabs(x)
	#ifndef PI
		#define	PI			3.14159265358979323846
	#endif
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void bit_revr(REAL *rdata, REAL *idata, int npt);
static void fft_4(REAL *rdata, REAL *idata, int npt);
static void fft_m(REAL *rdata, REAL *idata, int npt);
static void y_pack(REAL *y, int npt, int dir);
static MYTMP window_p(int type, REAL *array, int npt);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static double rmin=1.0E-20;							/* Minimum for deconvolution */

/* ============================================================================
-- Routine to compute the fast fourier transform of a complex valued function
-- with exactly 2**M points.
--
-- Usage:  fft2c(REAL *rbuf,REAL *ibuf,int npt, int dir)
--
-- Inputs: rbuf - array containing real part of the data (NPT)
--	        ibuf - array containing imaginary part of the data (NPT)
--         npt  - Number of points.  Must be even power or 2.	
--	        dir  - Direction of FFT. +=>forward,-=>inverse
--
-- Output: rbuf,ibuf - Normalized fourier transform.  On inverse, data is
--	                    normalized by 1/N.  Don't like that choice, but 
--		                 standard convention.
--                                              -1          *  * 
-- Note: Inverse FFT done by symmetry:  f(x) = F  [f(w)] = F [f(w)]
---------------------------------------------------------------------------- */
void fft2c(REAL *rdata, REAL *idata, int npt, int dir) {

	int i;

	if (dir < 0) {						/* Inverse operation? */
		for (i=0; i<npt; i++) idata[i] = -idata[i];
	}
	bit_revr(rdata, idata, npt);				/* Do BIT reversal addressing */
	fft_4(rdata, idata, npt);					/* Do first 4 bit FFT */
	fft_m(rdata, idata, npt);					/* And the rest */
	if (dir < 0) {									/* Inverse? */
		for (i=0; i<npt; i++) {					/* Normalize and conjugate */
			rdata[i] /= npt;
			idata[i] /= -npt;
		}
	}
	return;
}


/* ----------------------------------------------------------------------------
-- Usage:  call bit_revr(rdata,idata,npt)
--
-- Inputs: rdata - data array containing real values      REAL*4 (0:NPT-1)
--	        idata - data array containing imaginary values REAL*4 (0:NPT-1)
--         npt   - number of points (must be 2**M)        INT*2
--
-- Output: rdata - bit reversed storage order
--         idata - bit reversed storage order
--
-- Note: Bit reversal by example:  RDATA[010110] <==> RDATA[011010]
----------------------------------------------------------------------------- */
static void bit_revr(REAL *rdata, REAL *idata, int npt) {

	int i, ib, m;
	REAL temp;

	ib = npt/2;										/* IB => bit reverse of I */

	for (i=1; i<npt-1; i++) {					/* Start from first element */
		if (ib > i) {								/* Need to do? */
			temp = rdata[ib]; rdata[ib] = rdata[i]; rdata[i] = temp;
			temp = idata[ib]; idata[ib] = idata[i]; idata[i] = temp;
		}
		m = npt/2;
		while (m > 1 && ib >= m) {ib -= m; m /= 2;}
		ib += m;
	}
	return;
}


/* ----------------------------------------------------------------------------
-- Usage:  call fft_4(rdata,idata,npt)
--
-- Inputs: rdata - data array containing real values      REAL*4 (0:NPT-1)
--	        idata - data array containing imaginary values REAL*4 (0:NPT-1)
--         npt   - number of points (must be 2**M)        INT*2
--
-- Output: rdata - 4 number FFT
--         idata - 4 number FFT
--
-- WARNING: NPT must be >= 4.  This is not checked.
--
-- A four point FFT done in place as given below.  All kept on 80287 stack
--
-- F0 = (f0+f1) + (f2+f3)     r0 = (r0+r1) + (r2+r3)	i0 = (i0+i1) + (i2+i3)
-- F1 = (f0-f1) +i(f2-f3)     r1 = (r0-r1) - (i2-i3)	i1 = (i0-i1) + (r2-r3)
-- F2 = (f0+f1) - (f2+f3)     r2 = (r0+r1) - (r2+r3)	i2 = (i0+i1) - (i2+i3)
-- F3 = (f0-f1) -i(f2-f3)     r3 = (r0-r1) + (i2-i3)	i3 = (i0-i1) - (r2-r3)
---------------------------------------------------------------------------- */
static void fft_4(REAL *rdata, REAL *idata, int npt) {

	MYTMP rt[4],it[4];							/* Temporary storage */
	int i,j;

	for (i=0; i<npt/4; i++) {
		for (j=0; j<4; j++) {rt[j] = rdata[j]; it[j] = idata[j];}
		*(rdata++) = (REAL) (rt[0]+rt[1]+rt[2]+rt[3]);
		*(idata++) = (REAL) (it[0]+it[1]+it[2]+it[3]);
		*(rdata++) = (REAL) (rt[0]-rt[1]-it[2]+it[3]);
		*(idata++) = (REAL) (it[0]-it[1]+rt[2]-rt[3]);
		*(rdata++) = (REAL) (rt[0]+rt[1]-rt[2]-rt[3]);
		*(idata++) = (REAL) (it[0]+it[1]-it[2]-it[3]);
		*(rdata++) = (REAL) (rt[0]-rt[1]+it[2]-it[3]);
		*(idata++) = (REAL) (it[0]-it[1]-rt[2]+rt[3]);
	}
	return;
}

/* ============================================================================
-- Usage:  call 	near ptr fft_m
--
-- Inputs: rdata - data array containing real values      REAL*4 (0:NPT-1)
--	        idata - data array containing imaginary values REAL*4 (0:NPT-1)
--         npt   - number of points (must be 2**M)        INT*2
--         merges - level of merging already done
--
-- Output: rdata - finished merges
--         idata - finished merges
--
-- The remainder of the merges not done explicitly is now done.  The level
-- of the merge is assumed to be 2.  This is defined as
-- the size of the FFT last merges, ie. [merge] = 16 implies that we just
-- finished merging two 16 point FFTs to generate a 32 point FFT.  The process
-- continues through [merge] of NPT/2.
--
-- Counting: For a [merge] level, have to do [merge] different angle pairs on
--           a total of NPT/(2*merge) sets.
============================================================================ */
static void fft_m(REAL *rdata, REAL *idata, int npt) {

	int i,j,m,istep,mmax;
	MYTMP tempr,tempi,theta,wr,wi,wpr,wpi,wtemp;

	mmax = 4;												/* Start from first 4 done */

	while (npt > mmax) {									/* And folding */
		istep = 2*mmax;
		theta = PI/mmax;
		wpr = -2*SIN(theta/2)*SIN(theta/2);			/* Really [COS(theta)-1.0] */
		wpi = SIN(theta);									/* SIN(theta) */
		wr  = 1.0;											/* Starting cos(2*pi*i/n) */
		wi  = 0.0;											/* Starting sin(2*pi*i/n) */
		for (m=0; m<mmax; m++) {
			for (i=m; i<npt; i+=istep) {
				j = i+mmax;
				tempr    = wr*rdata[j]-wi*idata[j];
				tempi    = wr*idata[j]+wi*rdata[j];
				rdata[j] = (REAL) (rdata[i]-tempr);
				idata[j] = (REAL) (idata[i]-tempi);
				rdata[i] = (REAL) (rdata[i]+tempr);
				idata[i] = (REAL) (idata[i]+tempi);
			}
			wtemp = wr;
			wr = wr + wr*wpr - wi*wpi;					/* Updated cos(2*pi*i/n) */
			wi = wi + wi*wpr + wtemp*wpi;				/* Updated sin(2*pi*i/n) */
		}
		mmax = istep;
	}
	return;
}

/* ============================================================================
-- Routine to take the FFT of a real function.  This is an in place algorithm
-- which returns the symmetric REAL and COMPLEX FFT back into data buffer.
--
-- Usage: CALL FFT_REAL(y,npt,dir)
--
-- Inputs: RDATA,IDATA - Y data 				REAL*4 (0:NPT-1)
--         NPT         - total number of points
--         DIR         - +1 => forward transform
--                       -1 => inverse transform w/ 1/NPT normalization
--
-- Output: Y(0)            - 0 frequency value.
--         Y(NPT/2)        - highest positive frequency (aliased point)
--         Y(1:NPT/2-1)    - REAL component of positive FFT frequency
--         Y(NPT/2+1:NPT)  - IMAG component of positive FFT frequency
--
-- WARNING: NPT must be an even power of 2 at least 8 in size.
--
-- Other routines: FFT_FULL - Converts symetric FFT into FULL complex FFT
============================================================================ */
void fft_real(REAL *y, int npt, int dir) {

	if (dir >= 0) y_pack(y, npt, dir);			/* Pack data */
	fft_r2(y, y+(npt/2), npt, dir);				/* Do the FFT */
	if (dir <  0) y_pack(y, npt, dir);			/* Unpack data */
	return;
}

/* ============================================================================
-- Routine to take the FFT of a real function already divided in memory.  This
-- is an in place algorithm which returns the symmetric REAL FFT into real and
-- complex buffers of size NPT/2.
--
-- Usage: CALL FFT_R2(rdata,idata,npt,dir)
--
-- Inputs: RDATA,IDATA - Y data stored with even points in RDATA, odd in IDATA
--		        RDATA(0)     = Y(0)       IDATA(0)     = Y(1)
--                       RDATA(1)     = Y(2)       IDATA(1)     = Y(3)
--                            .....                  ......
--                       RDATA(NPT/2) = Y(NPT-2)   IDATA(NPT/2) = Y(NPT-1)
--         NPT         - total number of points
--         DIR         - +1 => forward transform
--                       -1 => inverse transform w/ 1/NPT normalization
--
-- Output: RDATA(0)    - contains the 0 frequency value.
--         IDATA(0)    - contains highest positive frequency (aliased point)
--         RDATA       - REAL component of positive FFT frequency   (0:NPT/2-1)
--         IDATA       - IMAG component of positive FFT frequency   (0:NPT/2-1)
--                       Negative frequencies may be obtained by symmetry.
--                       (See FFT_FULL)
--
-- WARNING: NPT must be an even power of 2 at least 8 in size.
--
-- Other routines: Y_PACK   - Split data w/ even/odd for truely in-place FFT
--                 FFT_FULL - Converts symetric FFT into FULL complex FFT
--
-- Algorithm: Basically, the first even/odd butterfly merge is done here after
--            an NPT/2 transform on the set.  Just have to work out the
--            symmetry of the dual transform and apply
--
--            F(k) = Feven(k) + e{2*pi*i*k/NPT} Fodd(k)
============================================================================= */
void fft_r2(REAL *rdata, REAL *idata, int npt, int dir) {

	MYTMP wr,wi,wpr,wpi,wtemp,theta,h1r,h1i,h2r,h2i;
	int i,j;

	if (dir >= 0) fft2c(rdata, idata, npt/2, +1);			/* Forward transform */

/* ... Do the last merge locally (or inverse merge as case may be) */
	theta = 2*PI/npt;								/* 2*pi/npt */
	if (dir < 0) theta = -theta;				/* Inverse? */
	wpr = -2*SIN(theta/2)*SIN(theta/2);
	wpi = SIN(theta);
	wr = 1+wpr;
	wi = wpi;

/* ... First and middle points will be handled later */
	for (i=1; i<=npt/4; i++) {
		j = (npt/2)-i;										/* Symmetric position */
		h1r =  (rdata[i]+rdata[j]) / 2;				/* Real part FFT(E) */
		h1i =  (idata[i]-idata[j]) / 2;				/* Imaginary part FFT(E) */
		h2r =  (idata[i]+idata[j]) / 2;				/* Real part FFT(O) */
		h2i =  (rdata[j]-rdata[i]) / 2;				/* Imaginary part FFT(O) */
		if (dir < 0) {										/* Simple house keeping */
			h2r = -h2r;
			h2i = -h2i;
		}
		rdata[i] = (REAL) (h1r +wr*h2r-wi*h2i);	/* FFT(E) + e^iwt*FFT(O) */
		idata[i] = (REAL) (h1i +wr*h2i+wi*h2r);
		rdata[j] = (REAL) (h1r -wr*h2r+wi*h2i);	/* FFT(N/2-k) = (fe-e^fo)* */
		idata[j] = (REAL) (-(h1i -wr*h2i-wi*h2r));
		wtemp = wr;
		wr = wr + wr*wpr-wi*wpi;
		wi = wi + wi*wpr+wtemp*wpi;
	}

	h1r = rdata[0];										
	rdata[0] = (REAL) (h1r+idata[0]);				/* FFT(E)+FFT(O) */
	idata[0] = (REAL) (h1r-idata[0]);				/* FFT(NPT) */
	if (dir <= 0) {										/* Reverse time? */
		rdata[0] /= 2;
		idata[0] /= 2;
		fft2c(rdata,idata,npt/2,-1);					/* Do the reverse FFT */
	}
	return;
}

/* ============================================================================
-- FFT_FULL - expand a packed transform of a real function into full complex
--            FFT.  Only involves copying the data blocks using the symmetry
--            f(-w) = f*(w)
--
-- Usage: call fft_full(rdata,idata,npt)
--
-- Inputs: rdata - half filled buffer w/ REAL part of transform    (0:NPT/2-1) 
--         rdata - half filled buffer w/ IMAG part of transform    (0:NPT/2-1) 
--         npt   - number of points total.
--
-- Output: rdata - filled w/ wrap around format of complex FFT     (0:NPT-1)
--	  idata - filled w/ wrap around format of complex FFT     (0:NPT-1)
============================================================================ */
void fft_full(REAL *rdata, REAL *idata, int npt) {

	int i;
	for (i=npt/2+1; i<npt; i++) {			/* Fill out the transform */
		rdata[i] =  rdata[npt-i];			/* Complex conjugate */
		idata[i] = -idata[npt-i];
	}
	rdata[npt/2] = idata[0];				/* Undo special packing */
	idata[npt/2] = 0;
	idata[0]     = 0;
	return;
}


/* ===========================================================================
-- Subroutine to take the autocorrelation of a data set with itself as
-- efficiently as possible.
--
-- Usage: CALL FFT_AUTO(Y,NPT)
--
-- Inputs: Y   - data set containing the values		REAL*4 (0:NPT-1)
--         NPT - number of points.   MUST BE 2**M	INTEGER*2
--
-- Output: Y   - Autocorrelation.  Y(NPT/2-1)     <== 0 lag
--                                 Y(0:NPT/2-2)   <== negative lags
--				  Y(NPT/2:NPT-1) <== positive lag
--
-- Note: The Y buffer itself is used as the workspace and the autocorrelation
--       overwrites the buffer.  The data is symmetrized so that 0 lag
--       corresponds to the center point with positive lag above.  NPT/2
--       postive lags, NPT/2-1 negative lags and the 0 lag value.  Entire
--       operation is done in place.
============================================================================ */
void fft_auto(REAL *y, int npt) {

	int i,n2;

/* ... Pack array in even/odd format and do the FFT */
	n2 = npt/2;										/* Half of NPT */
	y_pack(y,npt,+1);								/* Pack data */
	fft_r2(y,y+n2,npt,+1);						/* And do the FFT */

/* ... a(w) = g(w) x g*(w) - compute packed transform of autocorrelation */
	y[0]  = y[0]*y[0];							/* Zero frequency */
	y[n2] = y[n2]*y[n2];							/* Central aliased point */
	for (i=1; i<npt/2; i++) {					/* Remainder of positive terms */
		y[i] = y[i]*y[i]+y[i+n2]*y[i+n2];
		y[i+n2] = 0.0f;
	}

/* ... Invert back to real space and unpack.  Roll data so zero lag at npt/2-1. */
	fft_r2(y, y+n2, npt, -1);					/* Invert the FFT */
	y_pack(y, npt, -1);							/* Unpack the data */
	fft_y_roll(y, npt, n2-1);					/* Roll data by NPT/2-1 */
														/* 0 lag at center w/ alias + */
	return;
}


/* ===========================================================================
-- Routine to determine the convolution or cross-correlation or two real
-- functions.  These differ only in whether the second is conjugated before
-- multiplying together.  In frequency space,
--
--           corr(f,g) = f x g*            conv(f,g) = f x g
--                                       deconv(f,g) = f / g
--
-- Usage: CALL FFT_CONV(buf1,buf2,npt,mode)
--
-- Inputs: BUF1 - first function  (f)			REAL*4 (0:npt-1)
--         BUF2 - second function (g)			REAL*4 (0:npt-1)
--         NPT  - number of points			INTEGER
--         MODE - +1 => convolution			INTEGER
--		  0 => deconvolution
--                -1 => correlation
--
-- Output: BUF1 - real space correlation f with g
--         BUF2 - undefined
--
-- Notes: 1) NPT must be an even power of two and both arrays defined over the
--           full length.  This routine will not blank pad.
--        2) Unlike the autoconvolution, the data is returned with zero lag at
--           y(0) with increasing lags to NPT-1.  Since wrapped around, can
--           consider from NPT/2+1 to NPT-1 as negative lags.
--        3) In deconvolution, we consider any zero in the transform of g(w)
--           to be a zero in it's inverse also.  This avoids blowup problems
--	          but let the user beware.  No warning is given.
--
-- Issues with overflow when values are extremely large cropped up. Solve by 
-- normalizing both buffers to range 0,1 at beginning
============================================================================ */
void fft_conv(REAL *buf1, REAL *buf2, int npt, int mode) {

	int i,j;
	MYTMP h1r,h1i,h2r,h2i;
	double f_max, g_max;

/* Normalize the data */
	f_max = fabs(buf1[0]); g_max = fabs(buf2[0]);
	for (i=0; i<npt; i++) {
		if (fabs(buf1[i]) > f_max) f_max = fabs(buf1[i]);
		if (fabs(buf2[i]) > g_max) g_max = fabs(buf2[i]);
	}
	if (f_max == 0) f_max = 1.0;
	if (g_max == 0) g_max = 1.0;
	for (i=0; i<npt; i++) {
		buf1[i] /= (REAL) f_max;
		buf2[i] /= (REAL) g_max;
	}

/* Do the FFTs now */
	fft2c(buf1, buf2, npt, +1);			/* Do the FFT's */

/* .. Now, f(w)xg*(w).  Store as positive elements only in buf1 and buf2 */
	j = npt/2;									/* Highest frequency */
	if (mode == 0) {							/* Special for deconvolution */
		buf1[0] = (REAL) (buf1[0] * buf2[0] / (buf2[0]*buf2[0]+rmin));
		buf2[0] = (REAL) (buf1[j] * buf2[j] / (buf2[j]*buf2[j]+rmin));
	} else {										/* Convolution/Correlation */
		buf1[0] = buf1[0]*buf2[0];			/* Zero frequency */
		buf2[0] = buf1[j]*buf2[j];			/* Highest frequency */
	}
	for (i=1; i<npt/2; i++) {
		j = npt-i;								/* Corresponding negative freq */
		h2r =  (buf2[i]+buf2[j]) / 2;		/* Real part of g(w) */
		h2i =  (buf1[j]-buf1[i]) / 2;		/* Imaginary part of g(w) */
		if (mode == 0) {						/* Deconvolution? */
			h1r = h2r*h2r+h2i*h2i+rmin;	/* Use f(w)*[1/g(w)] */
			h2r =  h2r/h1r;
			h2i = -h2i/h1r;
		} else if (mode < 0) {				/* Correlation? */
			h2i = -h2i;
		}
		h1r =  (buf1[i]+buf1[j]) / 2;				/* Real part of f(w) */
		h1i =  (buf2[i]-buf2[j]) / 2;				/* Imaginary part of f(w) */
		buf1[i] = (REAL) (h1r*h2r-h1i*h2i);		/* REAL(h1*h2) */
		buf2[i] = (REAL) (h1i*h2r+h2i*h1r);		/* IMAG(h1*h2) */
	}

	for (i=0; i<npt/2; i++) {				/* Put imaginary at end */
		buf1[i+npt/2] = buf2[i];
	}

	if (mode == 0) fft_filt(buf1, npt);	/* Allow a final filter W(f) */
	fft_real(buf1, npt, -1);				/* Invert buffer 1 */

/* Correct for the normalization */
	for (i=0; i<npt; i++) {
		buf1[i] *= (REAL) (f_max*g_max);
	}

	return;
}


/* ===========================================================================
-- Routine to multiply a real transform by a filter component.  This is
-- internal to FFT_CONV in deconvolution mode.  In F77 mode, this is a simple
-- cut filter.
--
-- Usage: CALL FFT_FILT(buffer,npt)
--
-- Input: buffer - data buffer				REAL*4 (0:NPT-1)
--        npt    - number of points			INTEGER
--
-- Output: buffer*filter
--
-- Note: The data is scaled by filter function given by the user.  This is
--       generally a cut at a specific frequency to prevent the DECONVOLUTION
--       from blowing up.
============================================================================ */
void fft_filt(REAL *buffer,int npt) {

	return;
}


/* ============================================================================
-- Routine to roll a 2**N point data set in place by a specified number of 
-- points.  Useful to move the strange ordering given by an FFT into a more
-- normal sort order with zero frequency or lag in the center.  My preference
-- is for the zero lag to be at point NPT/2 (in a 1-NPT set) with the single
-- aliased point left at high positive frequency.  This involves rolling the
-- normal output by NPT/2-1 points.
--
-- Usage: CALL FFT_Y_ROLL(Y,NPT,IOFF)
--
-- Inputs: y    - initial buffer				REAL*4 (0:NPT-1)
--         npt  - number of points in buffer
--         ioff - Offset to roll the buffer
--
-- Output: y    - Rolled buffer
--
-- Routine operates by picking up one point at I, exchanging it with the point
-- where it is supposed to be MOD(I+IOFF,NPT), and repeating the entire
-- operation NPT times.  If we return to the starting point, we just increment
-- the starting point and continue on the next cylic loop.
============================================================================ */
void fft_y_roll(REAL *y, int npt, int iroll) {

	int i,ipt=0,istart=0;						/* Start with first point */
	REAL y2, yhold;

	while (iroll<0)    iroll+=npt;			/* get iroll in [0,npt) */
	while (iroll>=npt) iroll-=npt;
	if (iroll == 0) return;

	yhold = *y;
	for (i=0; i<npt; i++) {
		ipt = (ipt+iroll)%npt;					/* Next point number */
		y2 = y[ipt]; y[ipt] = yhold; yhold = y2;
		if (ipt == istart) {ipt = ++istart; yhold = y[ipt];}
	}
	return;
}

/* ============================================================================
-- Routine to take a linearly packed array and split it into even/odd points.
-- All even in first half of array and all odd in second half of array.  Done
-- in place with minimal temporary storage.  This routine is necessary to do
-- a real FFT in place for the first "merge", splitting data into REAL/IMAG
-- arrays.  The REAL array starts at Y(0) and the IMAG at Y(NPT/2).  This then
-- sets the correct format for calling FFT_R2.
--
-- Routine requires N ln(N) pair interchanges in an elegant (if I may say so
-- myself) algorithm.  Basically another butterfly game of sorting 4, then
-- joining two 4's to make an 8, joining two 8's to get a 16 etc.  Reverse
-- ordering to recover data is almost as easy.
--
-- 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15   ==> Exchange 1,2 5,6 9,10 13,14
-- 0 2 1 3 4 6 5 7 8 10 9 11 12 14 13 15   ==> Exchange 2,4 3,5 10,12 11,13
-- 0 2 4 6 1 3 5 7 8 10 12 14 9 11 13 15   ==> Exchange 4,8 5,9 6,10 7,11
-- 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15   ==> DONE!
-- REAL(0:7)          IMAG(0:7)            ==> Ready for FFT_R2 routine.
--
-- Usage: CALL Y_PACK(y,npt,dir)
--
-- Inputs: y   - array to pack even/odd 		REAL*4 (0:NPT-1)
--	  npt - number of points in y 
--	  dir - +1 => pack into even/odd
--		-1 => unpack from even/odd
--
-- Output: y   - packed or unpacked array as necessary
--
-- WARNING: It is assumed that NPT is an even power of 2.  Don't fail me!
============================================================================ */
static void y_pack(REAL *y, int npt, int dir) {

	REAL tmp;
	int i,j,idist;

	idist = (dir > 0) ? 1 : npt/4;		/* Forward or reverse packing */

	while (idist < npt/2 && idist > 0) {
		for (i=idist; i<npt; i+=4*idist) {	/* Start at same, go by 4*distance */
			for (j=0; j<idist; j++) {			/* Have dist exchanges at each point */
				tmp = y[i+j];						/* Do exchange */
				y[i+j] = y[i+idist+j];
				y[i+idist+j] = tmp;
			}
		}
		idist = (dir > 0) ? 2*idist : idist/2 ;
	}
	return;
}


/* ============================================================================
-- Routine to set the SNR value.  RMIN is 1/SNR in our case.
--
-- Usage: CALL FFT_SNR(snr)
--
-- Inputs: snr - signal to noise ratio to use on deconvolutions
--
-- Output: none
--
-- Note: Sets internal variable only.
============================================================================= */
void fft_snr(REAL snr) {
	
	snr = max(snr, 1.0E-6f);					/* SNR can't be less than 1E-6 */
	rmin = 1.0f/snr;
	return;
}


/* ============================================================================
-- Routine to estimate the power spectra.  Allows various windowing factors.
--
-- Usage: CALL POW_EST(type,y,n)
--
-- Inputs: type - type of windowing  1 => square			INTEGER
--				    2 => PARZEN window
--				    3 => WELCH  window
--         y     - array of points				REAL*4 (0:N-1)
--         n     - number of points
--
-- Output: y     - 10*log(estimate of the power spectra)		REAL*4 (0:N-1)
--	  n     - new number of points = npt/2+1
============================================================================ */
void fft_pow_est(int type, REAL *y, int *npt) {

	int i,j;
	MYTMP tmp,tmp1;

	tmp = window_p(type,y,*npt);			/* Modify window as necessary */
	fft_real(y,*npt,+1);						/* Do the transform */

/* --------------------------------------------------------------------------
-- ... Return power in db instead of linear.  More natural units.  In linear:
--
--     TMP = normalization constant from WINDOW operation.
--     y(0) = tmp*y(0)**2             y(npt/2) = tmp*y(npt/2)**2
--     y(i) = tmp * 2 * [y(i)**2+y(i+npt/2)**2]
--     Factor of 2  ^ comes from accounting for positive/negative frequencies
--
--     Below, when going to dB, tmp gets combined with the log and the factor
---------------------------------------------------------------------------- */
	j = *npt/2;
	tmp  = 10*LOG10(tmp);								/* Working in DB mode */
	y[j] = (REAL) (20*LOG10(y[j]) + tmp);			/* tmp*y(j)**2 */
	y[0] = (REAL) (20*LOG10(y[0]) + tmp);			/* tmp*y(0)**2 */
	y++;														/* Start with first element */
	tmp  = 10*LOG10(2.0) + tmp;						/* Account for +/- freq */
	for (i=1; i<j; i++) {
		tmp1 = y[0]*y[0] + y[j]*y[j];
		*(y++) = (REAL) (10*LOG10(tmp1) + tmp);
	}
	*npt = j+1;										/* Final number of points */
	return;
}


/* ============================================================================
-- Function to "window" the data for an FFT power spectrum estimation. 
-- Multiplies the data buffer by either a square, Parzen or Welch window to
-- estimate power spectra.  Normalization is returned by the function.
--
-- Usage:  REAL*4 = window_p(type,array,npt)
--
-- Inputs: type  - type of window.  1 => square window
--		                              2 => Parzen window
--				                        3 => Welch window
--         array - input data
--         npt   - number of data points
--
-- Output: array    - windowed data
--         window_p - normalization constant 1/N/sum(w(j)**2)
--
-- Note: 1) The normalization returned is appropriate for multiplying the power
--          spectrum c(j)**2+c(-j)**2 to obtain power estimate.  For square
--          window, is simply 1/N**2.
--       2) This is defined for all values of NPT, but will return square
--          window for any NPT <4.
--                                                                  2
--                     |2J-(N-1)|                         |2J-(N-1)|
-- Parzen:  w(j) = 1 - |--------|     Welch:   w(j) = 1 - |--------|
--                     | (N+1)  |                         | (N+1)  |
============================================================================ */
static MYTMP window_p(int type, REAL *array, int npt) {

	int i;
	MYTMP sum,tmp;

	if (type == 2 || type == 3) {
		sum = 0;
		for (i=0; i<npt; i++) {
			tmp = (2*i-(npt-1))/(npt+1.0);
			if (type == 2)
				tmp = 1-FABS(tmp);
			else
				tmp = 1-tmp*tmp;
			*array = (REAL) ((*array) * tmp);
			array++;
			sum += tmp*tmp;
		}
	} else {
		sum = npt;
	}
	return( 1/(npt*sum) );
}


/* ============================================================================
-- Routine to phase shift an FFT by a specified time.
--
-- Usage: CALL FFT_SHFT(real,imag,n,phase)
--
-- Inputs: real  - array of points
--	        imag  - array of points
--         n     - number of points
--	        phase - "phase" shift to insert (units of 2*pi)
--
-- Output: real  - array of points
--	        imag  - array of points
--
-- Note:   real/imag(n) = real/imag*exp[2*pi*i*n*phase]
============================================================================ */
void fft_shft(REAL *rdata, REAL *idata, int npt, REAL myphase) {

	int i;
	MYTMP phase,tempr,tempi,wr,wi,wpr,wpi,wtemp;
	
	phase = 2*PI*myphase;						/* Convert to radians */
	wpr = -2*SIN(phase/2)*SIN(phase/2);		/* Really [COS(theta)-1.0] */
	wpi = SIN(phase);								/* SIN(theta) */
	wr  = 1.0+wpr;									/* Real part of exp(iwt) */
	wi  = wpi;										/* Imag part of exp(iwt) */
	for (i=1; i<npt-1; i++) {
		tempr = *rdata;							/* Save the data */
		tempi = *idata;
		*(rdata++) = (REAL) (wr*tempr-wi*tempi);		/* New values */
		*(idata++) = (REAL) (wr*tempi+wi*tempr);
		wtemp = wr;									/* And update sin/cos terms */
		wr = wr + wr*wpr - wi*wpi;
		wi = wi + wi*wpr + wtemp*wpi;
	}
	return;
}

/* ============================================================================
-- Function to return the nearest power of 2 greater than or equal to npt.  If
-- that power is greater than NPTMAX, will return highest power of 2 less than
-- NPTMAX.
--
-- Usage:  INTEGER = fft_power_2(npt,nptmax)
--
-- Inputs: npt     - current number of points
--         nptmax  - maximum number of points in set
--
-- Output: power_2 - 2**M where M is an integer.  POWER_2 will be greater than
--                   NPT if possible but always less than NPTMAX.
--
-- Notes: Since designed for FFT, POWER_2 will try to return at least 8.  Do
--        not call with NPTMAX < 4 if you know what's good for you.
============================================================================ */
int fft_power_2(int npt, int nptmax) {

	int rcode=8;									/* Initial guess */
	while (rcode < npt) rcode = 2*rcode;	/* But less than NPTMAX */
	if (rcode > nptmax) rcode = rcode/2;
	return(rcode);
}
