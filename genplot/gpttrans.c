/* TRANSF.F77 */

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
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"
#include "helper.h"
#include "gptxtrn.h"

#include "fft2c.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#ifndef PI
	#define	PI			3.141592654f
#endif
#define	ESC		0x1B
#define	FLMAGIC	-2187304.0f					/* Magic # (exact for REAL32 cmps) */

typedef enum _OPS1 {
	TR_LIST, TR_ABORT, TR_X, TR_Y, TR_XY, 
     TR_ROTATE, 
     TR_ROLL, 
     TR_SUM, 
     TR_DIFFERENCE, 
	  TR_DYDX, 
	  TR_D2YDX2, 
     TR_INTEGRAL, 
     TR_SMOOTH, 
     TR_SMOOTH_FFT, 
	  TR_SMOOTH_GAUSSIAN,
	  TR_HISTOGRAM, 
	  TR_NHISTOGRAM, 
     TR_CBIN, 
	  TR_KERNAL,
	  TR_CDF,
     TR_FFT, 
     TR_FILTER_FFT, 
     TR_AUTOCORRELATE, 
	  TR_CORRELATION, 
     TR_CONVOLUTION, 
     TR_DECONVOLUTION, 
     TR_SQUISH, 
     TR_COMPRESS, 
     TR_AVERAGE, 
     TR_ZERO_PAD, 
	  TR_VALUE_PAD,
     TR_BASELINE, 
     TR_DEPHASE, 
     TR_CROSSING,           /* Zero crossing */
     TR_CENTER, 
     TR_NORMALIZE, 
     TR_CNORMALIZE,
	  TR_2DHIST,
	  TR_RDF,					/* Radial distribution function */
	  TR_CONTOUR
} OPS1;

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1 rcode;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE LOGICAL corrl(int key, CURVE *cv_a, CURVE *cv_b);
PRIVATE void    dydx(REAL *x, REAL *y, INT npt);
PRIVATE void    d2ydx2(REAL *x, REAL *y, INT npt);
PRIVATE void	 smo_sv(REAL *y, INT npt);
PRIVATE int     hist_1d_me(REAL *x, REAL *w, int npt, int options);
PRIVATE int     hist_2d_me(REAL *x, REAL *y, REAL *z, int npt);
PRIVATE int     cdf_me(REAL *x, REAL *w, int npt);
PRIVATE int     kernal_me(REAL *x, int npt);

/* Support routines */
static void ArrayStats(REAL *x, int npt, double *pxmin, double *pxmax, double *pxave, double *pxsdev);
static double round_me(double x, int n);

/* PRIVATE void	 smo_3d_sv(REAL *z, int nrow, int ncol); */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Routine to handle specific transformations of the data.  May be modified
-- by user if desired.
--
-- Usage: CALL TRANSF$(X,Y,NPT,NPTMAX)
--
-- Inputs: X,Y    - X and Y pairs of current data                REAL*4 (1:N)
--         NPT    - Number of valid points                       INTEGER
--         NPTMAX - Maximum number of allowed points             INTEGER
--
-- Output: X,Y    - Transformed data
--
-- Notes: This routine implements those transforms which are difficult or
--        impossible to execute via the LET processor.
============================================================================ */
PRIVATE const CMTYPE cmlist[] = {
		{"?",				 1,	TR_LIST},				{"-?",			-2,	TR_LIST},
		{"LIST",			 2,	TR_LIST},				{"/",				-1,	TR_LIST},
		{"ABORT",		 5,	TR_ABORT},
		{"X",				 1,	TR_X},
		{"Y",				 1,	TR_Y},
		{"XY",			 2,	TR_XY},
		{"ROTATE",		 3,	TR_ROTATE},
		{"ROLL",			 4,	TR_ROLL},
		{"SUM",			 3,	TR_SUM},
		{"DIFFERENCE",	 3,	TR_DIFFERENCE},
		{"DY/DX",		 5,	TR_DYDX},
		{"D2Y/DX2",		 5,	TR_D2YDX2},
		{"INTEGRAL",	 3,	TR_INTEGRAL},			{"INTEGRATE",	-8,	TR_INTEGRAL},
		{"CROSSING",	 5,	TR_CROSSING},
		{"SMOOTH",		 2,	TR_SMOOTH},
		{"FFT_SMOOTH",	 5,	TR_SMOOTH_FFT},		{"SMOOTH_FFT",	 -7,	TR_SMOOTH_FFT},
																{"FFTSMOOTH",	 -4,	TR_SMOOTH_FFT},
		{"GAUSS_SMOOTH", 7,	TR_SMOOTH_GAUSSIAN}, {"SMOOTH_GAUSSIAN",	-8,	TR_SMOOTH_GAUSSIAN},
		{"SQUISH",		 6,	TR_SQUISH},
		{"COMPRESS",	 4,	TR_COMPRESS},
		{"AVERAGE",		 4,	TR_AVERAGE},
		{"KERNAL",		 6,	TR_KERNAL},
		{"HISTOGRAM",	 4,	TR_HISTOGRAM},
		{"NHISTOGRAM",	 5,	TR_NHISTOGRAM},
		{"BIN",			 3,	TR_HISTOGRAM},
		{"CBIN",			 4,	TR_CBIN},
		{"CENTERBIN",	 7,	TR_CBIN},
		{"CDF",			 3,	TR_CDF},
		{"CENTER",		 6,	TR_CENTER},
		{"NORMALIZE",   4,   TR_NORMALIZE},
		{"CNORMALIZE",  5,   TR_CNORMALIZE},
		{"RDF",			 3,	TR_RDF},
		{"2D_HISTOGRAM",4,	TR_2DHIST},
		{"FFT",			 3,	TR_FFT},
		{"FILTER_FFT",		  6,	TR_FILTER_FFT},		{"FFT_FILTER",			-5,	TR_FILTER_FFT},
		{"AUTOCORRELATION", 5,	TR_AUTOCORRELATE},	{"AUTOCORRELATE",	  -13,	TR_AUTOCORRELATE},
		{"CORRELATION",	  4,	TR_CORRELATION},		{"CORRELATE",			-9,	TR_CORRELATION},
		{"CONVOLUTION",	  4,	TR_CONVOLUTION},		{"CONVOLVE",			-8,	TR_CONVOLUTION},
		{"DECONVOLUTION",	  6,	TR_DECONVOLUTION},	{"DECONVOLVE",		  -10,	TR_DECONVOLUTION},
		{"ZERO_PAD",		  4,	TR_ZERO_PAD},
		{"VALUE_PAD",		  7,	TR_VALUE_PAD},
		{"BASELINE",		  4,	TR_BASELINE},
		{"DEPHASE",			  4,	TR_DEPHASE},
		{"CONTOUR",			  7,	TR_CONTOUR},
		{NULL,				  0,	TR_DEPHASE} };

static char TransformHelp[] = 
"\n"
" Command to execute common transformations on a data set.\n"
"\n"
"   TRansform [X|Y|XY] <type> [-opts]\n"
"\n"
" Some transforms make sense only on an XY curve (rotate), while others can be applied\n"
" to only X or Y.  Default is to perform operations on the normal curve or on the Y data\n"
" set of the curve.  [X|Y|XY] options is ignored for many commands (e.g. contour).\n"
"       Y  - transform only the Y data where appropriate (default)\n"
"       X  - exchange meaning of X and Y, transforming on X (where appropriate, such as HISTOGRAM)\n"
"       XY - transform both X and Y (where appropriate, such as ROLL)\n"
"\n"
" Types of transforms:\n"
"   ROTate <degrees>      - rotates XY curve by specified clockwise angle\n"
"   ROTATE -SURface <s> <deg> - rotates specified surface by clockwise angle\n"
"                               see matrix for more surface rotate options\n"
"   ROLL <points>         - rolls data by number of elements  y[i+n] <-- y[i]\n"
"   SUM                   - sum data points                   y[i]   <-- sum j=0,i y[j]\n"
"   DIFFERENCE            - difference                        y[i]   <-- y[i]-y[i-1]  (i!=0)\n"
"   DY/DX                 - differentiates curve based on 3-point method\n"
"   D2Y/DX2               - second differential (sometimes repeating dy/dx better)\n"
"   INTEGRAL | INTEGRATE  - simple quadrature integration of curve\n"
"   CROSSING <arg> [opt]  - find all points where y-array crosses arg value, interpolating X\n"
"                           default returns both +/- crossings, with +/- based on X/Y curve\n"
"                           [ -+ | -P | -Plus  | -POSitive ] ==> return only + crossings\n"
"                           [ -- | -M | -Minus | -NEGative ] ==> return only - crossings\n"
"                           [ -NOSORT ] ==> ignore X when scanning Y array for +/- crossings\n"
"   SMOOTH                - simple 5-point smoothing algorithm\n"
"   SMOOTH -FFT <width>   - smooth by convolving with an inverted Bell of width (points)\n"
"   SMOOTH -GAussian <width> - smooth by convolving with a Gaussian (points)\n"
"   SMOOTH -SURace <s>    - biaxial applied to specified surface\n"
"   FFT_SMOOTH <width>    - smooth by convolving with an inverted Bell of width (points)\n"
"                           aliases SMOOTH_FFT and FFTSMOOTH\n"
"   GAUSS_SMOOTH <width>  - smooth by convolving with a Gaussian of width (points)\n" 
"                           aliases SMOOTH_GAUSSIAN\n"
"   SQUISH <n>            - reduced data set by keeping only every n-th point\n"
"   COMPRESS <n>          - reduce data set by summing every n points, x gets mid-point\n"
"   AVERAGE <n>           - reduce data set by averaging every n points (both x and y)\n"
"   HISTOGRAM [opts] <dx> - histogram data into intervals [0,dx) [dx,2*dx), ...\n"
"                            -normalized -> normalize so sum(y) = 1\n"
"                            -density    -> per unit dx interval\n"
"                            -weighted   -> bin +x[i] instead of +1 for each value\n"
"                            -center     -> use centered bins (see CBIN)\n"
"                            also alias BIN\n"
"   NHISTOGRAM [opts] <dx> - same as Histogram but by default add -density and -norm options\n"
"   CBIN [opts] <dx>      - histogram data into intevals (-dx/2,dx/2) (dx/2,3dx/2)\n"
"                            options are same as for HISTOGRAM\n"
"                            also alias CENTERBIN\n"
"   2D_HISTOGRAM <surf> [opts] - create a 2D histogram\n"
"                            Use 2D_HIST -? to get usage and options\n"
"   KERNAL                - Generate smooth estimate for probability distribution function\n"
"   CDF [-FULL]           - Generate the cummulative distribution function from the data\n"
"                           option -FULL uses [0,1], otherwise uses [1/2n, 1-1/2n]\n"
"   CENTER                - normalize y values so range is symmetric about 0.0\n"
"   NORMALIZE             - normalize y values so range extends from 0.0 to 1.0\n"
"   CNORMALIZE            - normalize y values so range extends from -1.0 to 1.0\n"
"   RDF <cutoff> <nbins>  - generate radial distribution function from x,y points\n"
"       [-FAST | -MIRROR <3|2|1|0> ]  - limit scanning of image points (speed)\n"
"       [-BOX <length>]               - set periodic boundaries box size\n"
"   FFT                   - very complex function for FFT operations\n"
"   FILTER_FFT <w(f$)>    - transform in frequency space using function w(f$)\n"
"                            also alias FFT_FILTER\n"
"   AUTOCORRELATION       - return FFT autocorrelation on curve\n"
"                            also alias AUTOCORRELATE\n"
"   CORRELATE <cv> [opts] - Return FFT correlation of current curve with <cv>\n"
"                            -PAD                  -> use padding\n"
"                            -NOWARNING | -SILENT | -QUIET  -> suppress warnings\n"
"                            also alias CORRELATION\n"
"   CONVOLVE <cv> [opts]  - Return FFT convolution of current curve with <cv>\n"
"                            -PAD                  -> use padding\n"
"                            -NOWARNING | -SILENT | -QUIET  -> suppress warnings\n"
"                            also alias CONVOLUTION\n"
"   DECONVOLVE <cv> [opt] - Deconvolve (FFT) curve using <cv>\n"
"                            -PAD                  -> use padding\n"
"                            -NOWARNING | -SILENT | -QUIET  -> suppress warnings\n"
"                            -SNR <db>            -> limit to <db> S/N ratio (30 dB)\n"
"                            also alias DECONVOLUTION\n"
"   ZERO_PAD <pts>        - Extend data set to have <pts> points, filling with y=0\n"
"   VALUE_PAD <pts> <val> - Extend data set to have <pts> points, filling with y=<val>\n"
"   BASELINE <x,y> <x,y>  - Subtract off baseline through specified points\n"
"   DEPHASE               - Attempt to unwrap phase into continuous function from [-pi,pi]\n"
"   CONTOUR <surf> <z>    - Returns points corresponding to contour crossings at given z value\n"
"           [-sort]         Sort crossings into closed contours (if possible)\n"
"\n"
" Examples: transform roll 20\n";

int gpt_do_transform(void) {

	CMTYPE *citem;
	OPS1 TransformMode = TR_Y;						/* Default is a Y transform */
	char token[DFLT_STR_SIZE], surf[DFLT_STR_SIZE];
	char SourceName[DFLT_STR_SIZE];
	REAL *x, *y, *z, *xsave, *ysave;
	int   iroll;
	INT	i,j,inc, nnew, npt;
	int	ichr, itype, check_mirror, *mbuf;
	double cutoff, dist2;
	double cosd, sind;
	REAL xtmp, ytmp, tmp, ynow, xlast, ylast, x1,x2,y1,y2,z1,z2,box, t1, t2, dx,dy,dz, a, b;
	BOOL sort;
	int flags;

	int type;
	void **varptr;
	SURFACE *s1;
	double zt;

/* First, look for a help request */
	if (LexCheckHelp("Transform", TransformHelp, NULL)) return(OKAY);

/* ----------------------------------------------------------------------------
... Check for existence of an X or Y qualifier.  If none specified, assume Y
---------------------------------------------------------------------------- */
	while (TRUE) {										/* Loop forever */
		if (! LexGetTokenP(token, sizeof(token), "Transform {X|Y|XY} [type]: Use ? for list (abort): "))
			return(OKAY);
		if (LexEscape(TRUE)) return(OKAY);

		citem = LexCmdl(token, cmlist, sizeof(CMTYPE));
		if (citem == NULL) {
			ERRprintf("ERROR: %s in an unrecognized TRANSFORM operation\n", token);
			return(NOMORE);
		} else switch (citem->rcode) {
			case TR_LIST:
				LexCmdlPrint(cmlist, sizeof(CMTYPE), "\nRecognized TRANSFORM keywords:");
				TTYputc('\n');
				return(OKAY);
			case TR_ABORT:
				return(OKAY);
			case TR_X:
			case TR_Y:
			case TR_XY:
				TransformMode = citem->rcode;
				continue;
			default:											/* Rest are handled later (lint) */
				break;
		}
		break;
	}

	if (TransformMode == TR_X) {						/* If specify X, reverse meaning */
		x = GptCurve->y;
		y = GptCurve->x;
	} else {
		x = GptCurve->x;
		y = GptCurve->y;
	}
	z      = GptCurve->z;
	npt    = GptCurve->npt;

	switch (citem->rcode) {

		case TR_LIST:
		case TR_ABORT:
		case TR_X:
		case TR_Y:
		case TR_XY:
			ERRprintf("ERROR in gpt_do_transform: Not expecting this switch (%d)\n", citem->rcode);
			break;

/* Center the data about 0 */
		case TR_CENTER:
		case TR_NORMALIZE:
		case TR_CNORMALIZE:
			ArrayMinMax(y, npt, &t1, &t2);			/* Find range */
			if ( (t1 == t2) && citem->rcode != TR_CENTER) {
				ERRprintf("ERROR: Data set has no range and cannot be normalized\n");
				return(NOMORE);
			}
			if (citem->rcode == TR_CENTER) {
				t1 = (t1+t2)/2;
				t2 = 1.0;
			} else if (citem->rcode == TR_NORMALIZE) {
				t2 = 1.0f/(t2-t1);						/* Range will become [0,1] */
			} else if (citem->rcode == TR_CNORMALIZE) {
				tmp = t1;
				t1 = (t1+t2)/2;
				t2 = 2.0f/(t2-tmp);						/* Range will become [-1,1] */
			}
			for (i=0; i<npt; i++) y[i] = (y[i]-t1)*t2;
			return(OKAY);
			
/* Radial distribution function */
		case TR_RDF:
			cutoff = LexGetReal(0.0f, "Maximum distance (1.0): ");
			if (LexEscape(TRUE)) return(OKAY);
			nnew = LexGetInt(100, "Number of bins (100): ");
			if (LexEscape(TRUE)) return(OKAY);
			if (nnew <= 0 || cutoff <= 0 || npt < 2) {
				ERRprintf("ERROR: Invalid parameters for radial distribution function\n");
				return(NOMORE);
			}
			check_mirror = 3;									/* Check sides / edges / corners */
			box = -1.0;
			while (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-FAST", 5)) {
					check_mirror = 0;
				} else if (LexEqual(token, "-MIRROR", 7)) {
					check_mirror = LexGetInt(3, "Mirror check mode (2D/3D 3=all, 2=all/face+edge, 1=side/face, 0=none) (3): ");
				} else if (LexEqual(token, "-BOX", 4)) {
					box = LexGetReal(-1.0, "Size of the box for periodic boundary conditions (auto): \n");
				} else {
					ERRprintf("ERROR: Unrecognized option (%s) ignored\n", token);
				}
			}
			mbuf = calloc(sizeof(*mbuf), nnew);
			if (z == NULL) {								/* 2D problem */
				ArrayMinMax(x, npt, &x1, &x2);			/* Find range */
				ArrayMinMax(y, npt, &y1, &y2);			/* Find range */
				if (box < 0) {
					dx = x2-x1;	dy = y2-y1;
				} else {
					dx = dy = box; x2 = x1+box; y2 = y1+box;
				}
				for (i=0; i<npt; i++) {
					for (j=i+1; j<npt; j++) {
						dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j],2);
						if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
						if (check_mirror >= 1) {				/* Check mirrors across the sides */
							if (x[i]-x1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]+dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]-dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
						}
						if (check_mirror >= 2) {				/* Check mirrors across the corners */
							if (x[i]-x1 < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]+dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]+dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]-dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]-dy,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
						}
					}
				}
			} else {
				ArrayMinMax(x, npt, &x1, &x2);			/* Find range */
				ArrayMinMax(y, npt, &y1, &y2);			/* Find range */
				ArrayMinMax(z, npt, &z1, &z2);			/* Find range */
				if (box < 0) {
					dx = x2-x1;	dy = y2-y1; dz = z2-z1;
				} else {
					dx = dy = dz = box; x2 = x1+box; y2 = y1+box; z2 = z1+box;
				}
				for (i=0; i<npt; i++) {
					for (j=i+1; j<npt; j++) {
						dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j],2)+pow(z[i]-z[j],2);
						if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
						if (check_mirror >= 1) {				/* Check mirrors across the faces */
							if (x[i]-x1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
						}
						if (check_mirror >= 2) {				/* Check mirrors across the edges */
							if (x[i]-x1 < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j],2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j],2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z[i]-z1 < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z2-z[i] < cutoff && y[i]-y1 < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z[i]-z1 < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (z2-z[i] < cutoff && y2-y[i] < cutoff) {
								dist2 = pow(x[i]-x[j],2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
						}
						if (check_mirror >= 3) {				/* Check mirrors across the corners */
							if (x[i]-x1 < cutoff && y[i]-y1 < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && y[i]-y1 < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && y2-y[i] < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y[i]-y1 < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x[i]-x1 < cutoff && y2-y[i] < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]+dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y[i]-y1 < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]+dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y2-y[i] < cutoff && z[i]-z1 < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]+dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
							if (x2-x[i] < cutoff && y2-y[i] < cutoff && z2-z[i] < cutoff) {
								dist2 = pow(x[i]-x[j]-dx,2)+pow(y[i]-y[j]-dy,2)+pow(z[i]-z[j]-dz,2);
								if (dist2 < pow(cutoff,2)) mbuf[(int) (nnew*sqrt(dist2)/cutoff)]++;
							}
						}
					}
				}
			}

			if (nnew > GptCurve->nptmax) {										/* Do I need more space? */
				if (! GVResize(GptUseCurve, nnew)) {
					ERRprintf("ERROR: Unable to resize curve to hold result\n");
					free(mbuf);
					return(NOMORE);
				}
				GptLinkXYZ(GptUseCurve);
			}
			for (i=0; i<nnew; i++) {
				GptCurve->x[i] = (REAL) (cutoff*i/(nnew-0.5));				/* Center of the RDF bin */
				GptCurve->y[i] = ((REAL) mbuf[i]);								/* Total number seen     */
				if (GptCurve->z == NULL) {
					GptCurve->y[i] /= (REAL) (npt/2.0*PI*(pow(cutoff*(i+1)/nnew,2)-pow(cutoff*i/nnew,2)));
				} else {
					GptCurve->z[i] = 0;
					GptCurve->y[i] /= (REAL) (npt/2.0*4.0/3.0*PI*(pow(cutoff*(i+1)/nnew,3)-pow(cutoff*i/nnew,3)));
				}
			}
			GptCurve->npt = nnew;
			free(mbuf);
			return(OKAY);
			
/* Find threshold crossings */
		case TR_CROSSING:
			tmp = LexGetReal(0.0f, "Threshold value (0.0): ");
			if (LexEscape(TRUE)) return(OKAY);
			itype = 0x03;											/* Both + and - cross */
			sort  = TRUE;
			while (LexGetOption(token, sizeof(token))) {
				if (strcmp(token, "-+") == 0 || LexEqual(token, "-PLUS", 2) || LexEqual(token, "-POSITIVE", 4)) {
					itype = 0x01;
				} else if (strcmp(token, "-") == 0 || strcmp(token, "--") == 0 || LexEqual(token, "-MINUS", 2) || LexEqual(token, "-NEGATIVE", 4)) {
					itype = 0x02;
				} else if (LexEqual(token, "-nosort", 7)) {
					sort = FALSE;
				} else {
					ERRprintf("ERROR: Invalid crossing option (%s) ignored.  Use -? for help\n", token);
				}
			}
			nnew = 0;												/* No crossings */
			for (i=0; i<npt-1; i++) {
				if (y[i+1]>tmp && y[i]<=tmp) {								/* Y has a jump up across threshold	*/
					if (itype == 0x03 ||											/* Looking for both +/- so definite */
						 ( ! sort && itype == 0x01) ||						/* Not sorting, so just must want + */
						 (   sort && itype == 0x01 && x[i+1]>=x[i]) ||	/* Sorting, and X is increasing		*/
						 (   sort && itype == 0x02 && x[i+1] <x[i]) ) {	/* Sorting, but X is decreasing		*/
						x[nnew] = x[i] + (x[i+1]-x[i]) * (tmp-y[i]) / (y[i+1]-y[i]);
						y[nnew] = tmp;
						nnew++;
					}
				} else if (y[i+1]<tmp && y[i]>=tmp) {						/* Y going down */
					if (itype == 0x03 ||											/* Looking for both +/- so definite */
						 ( ! sort && itype == 0x02) ||						/* Not sorting, so just must want + */
						 (   sort && itype == 0x02 && x[i+1]>=x[i]) ||	/* Sorting, and X is increasing		*/
						 (   sort && itype == 0x01 && x[i+1] <x[i]) ) {	/* Sorting, but X is decreasing		*/
						x[nnew] = x[i] + (x[i+1]-x[i]) * (tmp-y[i]) / (y[i+1]-y[i]);
						y[nnew] = tmp;
						nnew++;
					}
				}
			}
			GptCurve->npt = nnew;
			return(OKAY);

/* ROTATE the data set - in a clockwise direction */
		case TR_ROTATE:										/* Rotate data set */
			s1 = NULL;											/* Not doing a surface rotation */
			if (LexGetOption(SourceName, sizeof(SourceName))) {
				if (! LexEqual(SourceName, "-SURFACE", 4)) {
					LexBackup();
				} else {
					if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface to analyze (abort): ")) return(NOMORE);
					if (! GVGetInfo(SourceName, &type, (void **) &varptr)) {
						ERRprintf("ERROR: Surface %s does not exist\n", SourceName);
						return(NOMORE);
					} else if (type != GV_SURFACE) {
						ERRprintf("ERROR: Variable %s does not appear to be a surface (%d)\n", SourceName, type);
						return(NOMORE);
					}
					s1 = (SURFACE *) *varptr;
				}
			}
				
			tmp = LexGetReal(0.0f, "Clockwise Rotation angle (degrees): ");
			if (LexEscape(TRUE)) return(OKAY);

			/* Handle 3D as call from MATRIX commands, 2D here */
			if (s1 != NULL) {
				Rotate_3D_Surface(s1, SourceName, tmp, TRUE);		/* Angle is in degrees */
			} else {
				cosd = cos(tmp*0.017453292519943);
				sind = sin(tmp*0.017453292519943);
				for (i=0; i<npt; i++) {
					xtmp = *x; ytmp = *y;
					*(x++) = (REAL) ( xtmp*cosd + ytmp*sind);
					*(y++) = (REAL) (-xtmp*sind + ytmp*cosd);
				}
			}
			return(OKAY);

/* ROLL - roll the data set by a specified number of points */
		case TR_ROLL:										/* Roll the data set */
			iroll = LexGetInt(0, "# of points to roll the data right (0): ");
			if (LexEscape(TRUE)) return(OKAY);
			if (iroll == 0 || npt == 0) return(OKAY);
			fft_y_roll(y, npt, iroll);
			if (TransformMode == TR_XY) fft_y_roll(x,npt,iroll);
			return(OKAY);

/* SUM - point by point sum of the data set.  Y(n) = SUM[y(i)] 1<i<=n */
		case TR_SUM:
			ytmp = 0;
			for (i=0; i<npt; i++) {
				ytmp += *y;
				*(y++) = ytmp;
			}
			return(OKAY);
			
/* DIFFERENCE - point by point difference of data set.  Y(n) = Y(n)-Y(n-1) */
		case TR_DIFFERENCE:
			ylast = 0;
			for (i=0; i<npt; i++) {
				ynow = y[i]; y[i] -= ylast; ylast = ynow;
			}
			return(OKAY);
			
/* DY/DX - Point by point differentiation of the data */
		case TR_DYDX:
			dydx(x,y,npt);
			return(OKAY);

/* DY/DX - Point by point differentiation of the data */
		case TR_D2YDX2:
			d2ydx2(x,y,npt);
			return(OKAY);

/* INTEGRATE - Trapezoidal integration of the data */
		case TR_INTEGRAL:
		{
			static DOUBLE result;
			ylast  = *y;										/* Same again		 */
			xlast  = *(x++);
			*(y++) = 0;											/* Sum starts at 0 */
			result = 0;											/* Value is nill	 */
			for (i=1; i<npt; i++) {
				result += (*y+ylast)*(*x-xlast)/2;
				ylast = *y; xlast = *(x++); *(y++) = (REAL) result;
			}
			GVLinkDouble("RESULT$", GVF_USER, &result);
		}
			return(OKAY);

/* DEPHASE - remove any 2*pi ambiguity in the data */
		case TR_DEPHASE:
			tmp = 0.0f;
			for (i=1; i<npt; i++) {
				ylast = *(y++);					/* Current value */
				t1 = (*y+tmp)-ylast;				/* What difference will be */
				if (t1 > PI) 
					tmp -= 2*PI;
				else if (t1 < -PI) 
					tmp += 2*PI;
				*y += tmp;							/* New value of Y */
			}
			return(OKAY);
			
/* ZERO_PAD - pad data with zero's to the specified number of points */
/* VALUE_PAD - pad data with zero's to the specified number of points */
		case TR_ZERO_PAD:
		case TR_VALUE_PAD:
			nnew = LexGetInt(npt, "Number of points to pad to: ");
			if (LexEscape(TRUE)) return(OKAY);
			if (citem->rcode == TR_VALUE_PAD) {
				ytmp = LexGetReal(0.0f, "Value to pad with (0.0): ");
				if (LexEscape(TRUE)) return(OKAY);
			} else {
				ytmp = 0.0f;
			}

			if (nnew > GptCurve->nptmax) {
				if (! GVResize(GptUseCurve, nnew)) {
					gen_err("Unable to resize curve to hold result");
					return(NOMORE);
				}
				GptLinkXYZ(GptUseCurve);

				if (TransformMode == TR_X) {						/* If specify X, reverse meaning */
					x = GptCurve->y;
					y = GptCurve->x;
				} else {
					x = GptCurve->x;
					y = GptCurve->y;
				}
				z      = GptCurve->z;
			}

			dx   = x[npt-1] - x[npt-2];					/* X increment */
			xtmp = x[npt-1];

			for (i=npt; i<nnew; i++) {
				y[i] = ytmp;
				x[i] = (xtmp += dx);
			}
			GptCurve->npt = nnew;
			return(OKAY);

/* Smooth data by a 5 point smooth operation */
		case TR_SMOOTH:
			if (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-FFT", 4)) {							/* Really an FFT type smooth? */
					tmp = LexGetReal(5.0f, "Smoothing width (5.0 pts): ");
					if (LexEscape(TRUE)) return(OKAY);
					return( FFTSmooth(y,npt,GptCurve->nptmax,tmp) ? OKAY : NOMORE);
				} else if (LexEqual(token, "-GAUSSIAN", 2)) {
					tmp = LexGetReal(5.0f, "Smoothing width (5.0 pts): ");
					if (LexEscape(TRUE)) return(OKAY);
					return( FFTGaussSmooth(y,npt,GptCurve->nptmax,tmp) ? OKAY : NOMORE);
				} else if (LexEqual(token, "-SURFACE", 4)) {
					if (! LexGetTokenP(token, sizeof(token), "Surface to analyze (abort): ")) return(NOMORE);
					if (! GVGetInfo(token, &type, (void **) &varptr)) {
						ERRprintf("ERROR: Surface %s does not exist\n", token);
						return(NOMORE);
					} else if (type != GV_SURFACE) {
						ERRprintf("ERROR: Variable %s does not appear to be a surface (%d)\n", token, type);
						return(NOMORE);
					}
					s1 = (SURFACE *) *varptr;
					Smooth_3D_Surface(s1);
					return(OKAY);
				} else {
					LexBackup();
				}
			}
			smo_sv(y, npt);
			return(OKAY);

/* SQUISH   - squish data by keeping only every so often points */
/* COMPRESS - compress data by summing every so often points    */
		case TR_SQUISH:
		case TR_COMPRESS:
		case TR_AVERAGE:
			xsave = x; ysave = y;
			inc = LexGetInt(1, "Every Ith point: ");
			if (LexEscape(TRUE)) return(OKAY);
			if (inc <= 1) return(OKAY);
			nnew = 0;
			for (i=0; i<npt; i+=inc) {
				if (citem->rcode == TR_SQUISH) {				/* Single point */
					xtmp = *x; ytmp = *y;
				} else if (i+inc > npt) {						/* Done anyway */
					break;
				} else if (citem->rcode == TR_AVERAGE) {
					xtmp = ytmp = 0;
					for (j=0; j<inc; j++) {xtmp += x[j]; ytmp += y[j];}
					xtmp /= inc; ytmp /= inc;					/* Average X & Y */
				} else {
					ytmp = 0;
					for (j=0; j<inc; j++) ytmp += y[j];		/* Sum on Y */
					xtmp = x[inc/2];								/* X gets midpoint */
				}
				*xsave++ = xtmp; *ysave++ = ytmp; nnew++;
				x+=inc; y+=inc;
			}
			GptCurve->npt = nnew;
			return(OKAY);

/* BASELINE - linearize a function through two points */
		case TR_BASELINE:
			a = LexGetReal(FLMAGIC, "Enter X,Y for 2 baseline points: (cursor) ");
			if (LexEscape(TRUE)) return(OKAY);
			if (a != FLMAGIC) {
				t1 = LexGetReal(1.0f, "Y: ");
				if (LexEscape(TRUE)) return(OKAY);
				b  = LexGetReal(2.0f, "2nd X: ");
				if (LexEscape(TRUE)) return(OKAY);
				t2 = LexGetReal(2.0f, "2nd Y: ");
				if (LexEscape(TRUE)) return(OKAY);
			} else {
				PlotCursor(&a, &t1, &ichr);
				if (ichr == ESC) return(OKAY);
				PlotCursor(&b, &t2, &ichr);
				if (ichr == ESC) return(OKAY);
			}
			if (b == a) {
				gen_err("Vertical baseline?");
			} else {
				tmp = (t2-t1)/(b-a);					/* Slope */
				b = t1-tmp*a;							/* Offset */
				for (i=0; i<npt; i++) {
					*y = *y - ((*x)*tmp + b);
					x++; y++;
				}
			}
			return(OKAY);

/* BIN the data into intervals */
		case TR_HISTOGRAM:
			return hist_1d_me(y,x, npt, 0);
		case TR_NHISTOGRAM:
			return hist_1d_me(y,x, npt, 2);
		case TR_CBIN:
			return hist_1d_me(y,x, npt, 1);

/* CDF estimate from the data */
		case TR_CDF:
			return cdf_me(x, y, npt);

/* Kernal estimate of probability density */
		case TR_KERNAL:
			return kernal_me(y, npt);

/* ==================================== */
/* 2D Histogram the data into intervals */
/* ==================================== */
		case TR_2DHIST:
			return hist_2d_me(x,y,z, npt);

/* -------------------------------------------------------------------------
-- ... FFT of the data
---------------------------------------------------------------------------- */
		case TR_FFT:
			fft_me(GptCurve, GptUseCurve);
			return(OKAY);

/* --------------------------------------------------------------------------
c ... Autocorrelation function
---------------------------------------------------------------------------- */
		case TR_AUTOCORRELATE:
			return( autocorr(GptCurve, GptUseCurve) ? OKAY : NOMORE);

/* ---------------------------------------------------------------------------
-- ... FFT_FILTER of the data
---------------------------------------------------------------------------- */
		case TR_FILTER_FFT:
			if (! LexGetMathP(token, sizeof(token), "Filter expression/array (f$=freq): ")) return(OKAY);
			if (LexEscape(TRUE)) return(OKAY);
			return( filter(GptCurve, GptUseCurve, token) ? OKAY : NOMORE);

/* --------------------------------------------------------------------------
-- ... Smooth data by a FFT algorithm
--------------------------------------------------------------------------- */
		case TR_SMOOTH_FFT:
			tmp = LexGetReal(5.0f, "Smoothing width (5.0 pts): ");
			if (LexEscape(TRUE)) return(OKAY);
			return( FFTSmooth(y,npt,GptCurve->nptmax,tmp) ? OKAY : NOMORE);

/* --------------------------------------------------------------------------
-- ... Smooth data by a Gaussian convolution
--------------------------------------------------------------------------- */
		case TR_SMOOTH_GAUSSIAN:
			tmp = LexGetReal(5.0f, "Smoothing width (5.0 pts): ");
			if (LexEscape(TRUE)) return(OKAY);
			return( FFTGaussSmooth(y,npt,GptCurve->nptmax,tmp) ? OKAY : NOMORE);

/* --------------------------------------------------------------------------
-- ... Determine the contour points from a surface
--------------------------------------------------------------------------- */
		case TR_CONTOUR:
			if (! LexGetTokenP(surf, sizeof(surf), "Surface to analyze (abort): ")) return(NOMORE);
			if (LexEscape(TRUE)) return(OKAY);
			zt = LexGetReal(0.0f, "Value to search (0.0: ");
			if (LexEscape(TRUE)) return(OKAY);

			flags = 0;
			if (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-nosort", 4)) {
					flags = 0;
				} else if (LexEqual(token, "-sort", 2)) {
					flags |= 0x01;
				} else {
					TTYprintf("ERROR: Unrecognized option (%s)\n", token);
				}
			}

			if (! GVGetInfo(surf, &type, (void **) &varptr)) {
				ERRprintf("ERROR: Surface %s does not exist\n", surf);
				return(NOMORE);
			} else if (type != GV_SURFACE) {
				ERRprintf("ERROR: Variable %s does not appear to be a surface (%d)\n", surf, type);
				return(NOMORE);
			}
			s1 = (SURFACE *) *varptr;
			return (Contour_3D_Surface(s1, zt, flags) < 0) ? NOMORE : OKAY;

/* ---------------------------------------------------------------------------
-- ... Cross-correlation function
-- ... Convolution function
-- ... Deconvolution
---------------------------------------------------------------------------- */
		case TR_CORRELATION:
		case TR_CONVOLUTION:
		case TR_DECONVOLUTION:
		{
			#define	CONVOLVE		0x01
			#define	CORRELATE	0x02
			#define	DECONVOLVE	0x04
			#define	USE_PADDING	0x08
			#define	NOWARN		0x10

			int key=0;
			CURVE *curve_b=NULL;

			if (citem->rcode == TR_CONVOLUTION)
				key |= CONVOLVE;
			else if (citem->rcode == TR_DECONVOLUTION)
				key |= DECONVOLVE;
			else
				key |= CORRELATE;
			if (! LexGetTokenP(token, sizeof(token), "Curve to convolve or correlate: "))
				strcpy(token, GptMainCurve);
			if (LexEscape(TRUE)) return(OKAY);

			if (! GVGetInfo(token, &itype, (void **) &curve_b) || (itype != GV_2DCURVE && itype != GV_3DCURVE) ) {
				gen_err2("Requested curve does not exist", token);
				return(NOMORE);
			}
			curve_b = *((CURVE **) curve_b);

			while (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-PAD", 2)) 
					key |= USE_PADDING;
				else if (LexEqual(token, "-NOWARNING", 3) || LexEqual(token, "-SILENT", 3) || LexEqual(token, "-QUIET", 2)) 
					key |= NOWARN;
				else if (LexEqual(token, "-SNR", 4) && (key & DECONVOLVE)) 
					fft_snr(LexGetReal(30.0f, "Signal to noise ratio (30): "));
				else {
					gen_err2("Unrecognized (de)convolution option", token);
					return(NOMORE);
				}
			}
			return( corrl(key, GptCurve, curve_b) ? OKAY : NOMORE) ;
		}
	}
	return(UNIMPLEMENTED);
}


/* ===========================================================================
-- Routine to return the cross-correlation of a function with another.
--
-- Usage: CALL CORRL(X_A,Y_A,NPT_A,NPTMAX,X_C,Y_C,NPT_C)
--
-- Inputs: KEY     - options.  Bitwise:  0 -->> pad for no overlap
--                                       1 -->> ignore X spacing differences
--                                       8 -->> do convolution instead
--         X_A,Y_A - current data set           [f(x)]   REAL*8 (0:NPT_A-1)
--         NPT_A   - number of valid points              INTEGER
--         NPTMAX  - maximum number of points in _A      INTEGER
--         X_B,Y_B - data set for convolutiont  [g(x)]   REAL*8 (0:NPT_B-1)
--         NPT_B   - number of points in g(x)
--
-- Output: Y_A   - autocorrelation values        (center packed)
--         X_A   - lag value                     (0 at NPT/2)
--         NPT_A - possibly modified # of points
--
-- Note: NPT is worked with as an even power of 2.  If it is not currently, the
--       data will be blank padded or truncated to valid number.  Truncate is
--       straight forward.  Blank padding will occur by centering.
============================================================================ */
PRIVATE LOGICAL corrl(int key, CURVE *cv_a, CURVE *cv_b) {
	
	REAL *x_a=cv_a->x, *y_a=cv_a->y, *x_b=cv_b->x, *y_b=cv_b->y;
	INT npt_a=cv_a->npt, nptmax_a=cv_a->nptmax, npt_b=cv_b->npt;

	INT i, npt;
	int  itype, n_shft, n_off;
	REAL dx,dx2,xa0,xb0,toff,asymm;

/* ---------------------------------------------------------------------------
-- Determine number of points to use.  Normally, just use the biggest curve
-- value.  However, if padding is requested, number must be at least twice the
-- smaller number of points.
---------------------------------------------------------------------------- */
	npt = max(npt_a, npt_b);								/* No padding, use biggest */
	if (key & USE_PADDING) npt = max(npt, 2*min(npt_a, npt_b));
	npt = fft_power_2(min(nptmax_a,npt),nptmax_a);	/* And make a power of 2 */

/* ... Determine point spacing and overlap offset */
	xa0   = x_a[0];
	dx    = (x_a[npt_a-1]-xa0)/(npt_a-1);			/* Point spacing	*/
	asymm = (x_a[1]-x_a[0])-dx;						/* error possible */
	if (dx == 0.0f || fabs(asymm/dx) > 0.01f) {
		ERRprintf(
"ERROR: Can't determine dx for first curve.  Average over npt is %g \n"
"       but %g over first two points.\n", dx, x_a[1]-x_a[0]);
		return(FALSE);
	}

	xb0   = x_b[0];
	dx2   = (x_b[npt_b-1]-xb0)/(npt_b-1);			/* Where is the zero */
	asymm = (x_b[1]-x_b[0])-dx2;						/* error possible */
	if (dx2 == 0.0f || fabs(asymm/dx2) > 0.01f) {
		ERRprintf(
"ERROR: Can't determine dx for second curve.  Average over npt is %g \n"
"       but %g over first two points.\n", dx2, x_b[1]-x_b[0]);
		return(FALSE);
	}

	asymm = (x_b[min(npt_b,npt)-1]+xb0)/2;			/* Assymetry of convolution */
	if (fabs(dx2/dx-1.0) > 0.001f) {					/* Different spacing */
		if (! (key & NOWARN)) gen_warn("You're the boss but spacing of ordinates differ.  Zero pts aligned.");
		xb0   = xb0*dx/dx2;
		asymm = asymm*dx/dx2;
	}

/* ----------------------------------------------------------------------------
-- ... Currently, spectra will start at toff and extend to toff+(npt-1)*dx.
-- ... For CONV, we want it to start at xa0+asymm, which requires a roll of the
-- ... Y data by some fixed number of pixels.  This we handle AFTER the FFT is
-- ... done, just wrap around of the buffer.
---------------------------------------------------------------------------- */
	if (key & CONVOLVE) {								/* Convolution */
		itype  = +1;										/* Select convolution */
		toff   = xa0+xb0;									/* Times are summed */
		n_off  = nint((toff-(xa0+asymm))/dx);		/* # pixels to shift right */
		n_shft = 0;											/* No shift of curves */
	} else if (key & DECONVOLVE) {					/* De-convolution */
		if (! (key & NOWARN)) gen_warn("Even if you are the Boss deconvolution is extremely ill-advised");
		itype  = +0;
		toff   = xa0-xb0;
		n_off  = nint((toff-(xa0-asymm))/dx);
		n_shft = 0;
	} else {													/* Cross correlation */
		itype  = -1;										/* Select cross-correlation */
		n_shft = nint((xb0-xa0)/dx) % npt;			/* Time shift to closest */
		if (n_shft < 0) n_shft += npt;				/* Make sure stays positive */
		toff   = xa0-(xb0-n_shft*dx);					/* Times are subtracted */
		n_off  = npt/2-1;									/* Always shift to center */
	}
	toff = toff-n_off*dx;								/* New starting time */

/* ... Fill out both buffers to chosen 2**M size. */
	while (npt_a < npt) y_a[npt_a++] = 0;			/* Zero pad Y buffer */
	npt_a = npt;											/* Reset # of points */
	for (i=0; i<npt; i++) x_a[i] = 0;				/* Empty X first */
	for (i=0; i<npt_b; i++) x_a[(i+n_shft)%npt] = y_b[i];

/* --------------------------------------------------------
-- ... Finally, do the correlation and then fill in the X lag values
--
-- ... Currently, spectra will start at toff and extend to toff+(npt-1)*dx.
-- ... For CONV, we want it to start at xa0+asymm, which requires a roll of the
-- ... Y data by some fixed number of pixels.  This we handle AFTER the FFT is
-- ... done, just wrap around of the buffer.
---------------------------------------------------------- */
	fft_conv(y_a, x_a, npt, itype);					/* Do the cross correlation */
	fft_y_roll(y_a,npt,n_off);
	for (i=0; i<npt; i++) x_a[i] = toff+i*dx;

	cv_a->npt = npt;
	return(TRUE);
}

/* ============================================================================
-- Subroutine to take the differential dy/dx of a set of points.  Uses a 3
-- point method to return the derivative at each point instead of at the
-- midpoints.
--                       
-- Usage:  call dydx(x,y,npt)
--
-- Inputs: x,y - data set	            			REAL*4 (1:NPT)
--	        npt - number of points in set			INTEGER
--
-- Output: y   - replaced with 3 point estimate of dy/dx.
--
-- Routine is exact for quadratic polynomial fit to data.  End points are
-- handled as constant curvature from closest points.  Data need not be
-- strictly sorted but results may be rediculous if not.
--
-- dxp = x(i+1)-x(i)   dyp = y(i+1)-y(i)   dp = dyp/dxp    dy   dp*dxm + dm*dxp
-- dxm = x(i)-x(i-1)   dym = y(i)-y(i-1)   dm = dym/dxm    -- = ---------------
--							  dx      dxm + dxp
--
-- NAN and infty results are replaced w/ 0.00 - suggest using SORT -STRICT
============================================================================ */
PRIVATE void dydx(REAL *x, REAL *y, INT npt) {

	INT i;
	REAL dxp,dxm,dp,dm,yl;

	yl  = y[npt-3];							/* Save point Y[NPT-3] */
	dxp = x[0]-x[2];							/* Use third point */
	dp  = (dxp != 0.0f) ? (y[0]-y[2])/dxp : 0.0f;
	for (i=0; i<npt-1; i++)	{				/* Do all but last point */
		dxm = dxp;
		dm  = dp;
		dxp = x[i+1]-x[i];
		dp  = (dxp != 0.0f) ? (y[i+1]-y[i]) / dxp : 0.0f;
		y[i] = (dxp+dxm != 0.0f) ? (dxm*dp+dxp*dm) / (dxp+dxm) : 0.0f;
	}
	dxm    = x[npt-3]-x[npt-1];
	yl     = (dxm != 0.0f) ? (yl-y[npt-1]) / dxm : 0.0f;
	y[npt-1] = (dxp+dxm != 0.0f) ? (dxp*yl+dxm*dp) / (dxp+dxm) : 0.0f;
	return;
}

/* ============================================================================
-- Subroutine to take the differential d^2y/dx^2 of a set of points.  More
-- stable than using dy/dx twice.  Uses simple three-points and assumes
-- constant curvature at the end points.
--                       
-- Usage:  call d2ydx2(x,y,npt)
--
-- Inputs: x,y - data set	            			REAL*4 (1:NPT)
--	        npt - number of points in set			INTEGER
--
-- Output: y   - replaced with 3 point estimate of d^2y/dx^2.
--
-- Routine is exact for quadratic polynomial fit to data.  End points are
-- handled as constant curvature from closest points.  Data need not be
-- strictly sorted but results may be rediculous if not.
--
--                  {x[i+1]-x[i]}*{y[i-1]-y[0]} - {x[i-1]-x[i]}*{y[i+1]-y[0]}
-- curvature = 2 * -----------------------------------------------------------
--                 {x[i-1]-x[i]}^2*{x[i+1]-x[0]} - {x[i+1]-x[i]}^2*{x[i-1]-x[0]}
--
--                   ya/xa - yb/xb         xa = x[i-1]-x[i]  ya = y[i-1]-y[i]
-- curvature = 2 * ---------------         xb = x[i+1]-x[i]  yb = y[i+1]-y[i]
--                    (xa - xb)
============================================================================ */
PRIVATE void d2ydx2(REAL *x, REAL *y, INT npt) {

	INT i;
	double xa,xb,ya,yb;

/* Inside loop, let compiler optimize the ?: construct */
	if (npt < 3) return;							/* Really nothing to do :-( */
	xb = x[1]-x[0]; yb = y[1]-y[0];			/* Pretend terminus at i=0 */
	for (i=1; i<npt-1; i++) {
		xa = -xb;		   ya = -yb;			/* Essentially order reverses */
		xb = x[i+1]-x[i]; yb = y[i+1]-y[i];	/* To right */
		if (xa == 0 || xb == 0 || xa==xb) {
			y[i] = 0;
		} else {
			y[i] = (REAL) ( 2*(ya/xa-yb/xb)/(xa-xb) );
		}
	}
	y[0] = y[1];									/* Constant curvature */
	y[npt-1] = y[npt-2];							/* Constant curvature */
	return;
}

/* ***********************************************************************
-- Usage Guide:
--
--    void smo_sv(counts,npt)
--
--    Performs a 5 pt smooth through the data using the Savitsky-Goulay
--    algorithm.  See TRACOR data manuals for more information.
--
--    x(n) = (-3*x(n-2)+12*x(n-1)+17*x(n)+12*x(n+1)-3*x(n+2))/35
--
--    Usage:  call smo_sv(counts,npt)
--
--    Inputs: counts - buffer of data values from 1-npt
--            npt    - number of points in counts
--
--    Output: counts - smoothed data
--
--    COMMON BLOCKS:     none
--    CALLED FROM:       ANLYTC
--    CALLS:             none
--    Notes:             Implemented in assembly language on the PC
*********************************************************************** */
PRIVATE void smo_sv(REAL *y, INT npt) {
	
	REAL tmp[5];
	int i;

	tmp[0] = tmp[1] = tmp[2] = y[0]; 
	tmp[3] = y[1]; tmp[4] = y[2];
	
	for (i=0; i<npt; i++) {
		y[i] = ( -3*tmp[0]+12*tmp[1]+17*tmp[2]+12*tmp[3]-3*tmp[4] )/35;
		tmp[0] = tmp[1]; tmp[1] = tmp[2]; tmp[2] = tmp[3]; tmp[3] = tmp[4];
		if (i+3 < npt) tmp[4] = y[i+3];
	}
	return;
}


/* ***********************************************************************
-- Usage Guide:
--
--    void smo_3d_sv(z, nrow, ncol);
--
--    Performs a similar 5 pt smooth through surface data using an analog
--    to the Savitsky-Goulay algorithm.
--
--    distance     weighting        #        total
--       0            17            1         17
--       1            12            4         48
--     sqrt(2)         5            4         20
--       2            -3            4        -12
--                                           ====
--                                            73
--
--    Usage:  void smo_3d_sv(counts,npt)
--
--    Inputs: counts - buffer of data values from 1-npt
--            npt    - number of points in counts
--
--    Output: Replaces s1->z data with smoothed values
--
-- Notes: edges of the surface are constant extended
*********************************************************************** */
#if 0
PRIVATE void smo_3d_sv(REAL *z, int nrow, int ncol) {

	REAL *nz;
	int i,j, im1,im2,ip1,ip2, jm1,jm2,jp1,jp2;

	nz = calloc(nrow*ncol, sizeof(*nz));
	for (i=0; i<ncol; i++) {
		im1 = max(0,i-1);
		im2 = max(0,i-2);
		ip1 = min(i+1,ncol-1);
		ip2 = min(i+2,ncol-1);
		for (j=0; j<nrow; j++) {
			jm1 = max(0,j-1);
			jm2 = max(0,j-2);
			jp1 = min(j+1,nrow-1);
			jp2 = min(j+2,nrow-1);
			nz[j+i*nrow] = (REAL) (	(  17*z[j+i*nrow]
											 + 12*(z[jm1+i*nrow]+z[jp1+i*nrow]+z[j+im1*nrow]+z[j+ip1*nrow]) +
											 +  5*(z[jm1+im1*nrow]+z[jm1+ip1*nrow]+z[jp1+im1*nrow]+z[jp1+ip1*nrow])
											 -  3*(z[jm2+i*nrow]+z[jp2+i*nrow]+z[j+im2*nrow]+z[j+ip2*nrow])
											) / 73.0	);
		}
	}
	for (i=0; i<nrow*ncol; i++) z[i] = nz[i];
	free(nz);
	return;
}
#endif

/* ===========================================================================
-- Histogramming routine.  Takes x,y pointers and creates a curve 
-- corresponding to the probability (or number) of times Y is in a
-- particular range of values (possibly weighted by the X value)
--
-- Usage:  int hist_2d_me(REAL *x, REAL *y, REAL *z, int npt);
--
-- Inputs: x,y - required pointers to an array of points
--         z   - optional pointer to Z values for weighted histogram
--         npt - number of elements in the array
--
-- Output: Replaces x,y,z,npt with the histogram results
--
-- Return: OKAY if everytyhing succeeds, NOMORE on errors
--
-- Note: Has extensive set of its own command options which are parsed.
=========================================================================== */
static char Hist_Help[] = 
"\n"
"Usage: HISTogram  [-options] <dx>\n"
"       NHISTogram [-options] <dx>\n"
"       CBIN       [-options] <dx> \n"
"\n"
"Will create a curve corresponding to a histogram of the data.  CBIN\n"
"is identical to HISTOGRAM but sets -CENTER as default.  Similarly,\n"
"NHISTOGRAM sets -DENSITY and -NORMALIZE as default options.\n"
"\n"
"Options:\n"
"    -array <var> -> histogram an array or surface rather than y\n"
"       -surface  -> synonymous with -array\n"
"       -source   -> synonymous with -array\n"
"    -center      -> use centered bins (-dx/2,dx/2) rather than (0,dx)\n"
"    -normalized  -> normalize so sum(y) = 1\n"
"    -density     -> per unit dx*dy interval\n"
"    -weighted    -> bin +x[i] instead of +1 for each value\n"
"    -dx <val>    -> set the bin width\n"
"    -width <val> -> synonymous with -dx\n"
"    -nx <ival>   -> set # of bins in X\n"
"    -span <low> <high> -> range of values for bins\n"
"\n"
"Defaults: dx = 1.2*sdev/npt^1/3 where sdev is standard deviation\n";

PRIVATE int hist_1d_me(REAL *x, REAL *w, int npt, int options) {

	INT  nwin, ndx;
	long i,j, ilow, ihigh;
	double a, dx, sum, xmin, xmax, xave, sdev, *bptr;
	char token[DFLT_STR_SIZE], msg[80];
	LOGICAL center=FALSE, weighted=FALSE, normalize=FALSE, densify=FALSE;
	LOGICAL OldFormat = FALSE;

/* First, check for help query */
	if (LexCheckHelp("Histogram", Hist_Help, NULL)) return(OKAY);

/* Definitions: BIN  [i*dx,(i+1)*dx)     CBIN [(i-.5)*dx,(i+.5)*dx) */
	center = (options & 0x01);
	normalize = (options & 0x02);
	densify   = (options & 0x02);

/* Calculate the range (for use in options) and set up default values */
	if (npt > 0) {
		ArrayStats(x,npt, &xmin, &xmax, &xave, &sdev);
		dx = 1.2*sdev/pow(npt,0.3333);			/* Theory says 3.49 -- too wide for me */
		dx = round_me(dx, 2);						/* Rounded off so not rediculous */
		ndx = 0;
	} else {
		dx = xmin = xmax = xave = sdev = 0;
		ndx = 100;
	}

/* Maintain old format of just giving range for a default histogram */
	OldFormat = TRUE;
	if (! LexChkToken(token, sizeof(token))) {
		TTYprintf("Data stats: min=%g max=%g  <ave>=%g  <sigma>=%g\n", xmin, xmax, xave, sdev);
		TTYprintf("  Recommended bin spacing: %g\n", dx);
	} else if (*token == '-' || *token == '/') {
		OldFormat = FALSE;
	}
	if (OldFormat) {
		sprintf(msg, "Bin width (%g): ", dx);
		dx = fabs(LexGetReal((REAL) dx, msg));
		if (LexEscape(TRUE)) return(OKAY);
	}

/* Scan options */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-ARRAY", 2) || LexEqual(token, "-SURFACE", 4) || LexEqual(token, "-SOURCE", 3)) {
			int type;
			void **varptr;
			if (! LexGetTokenP(token, sizeof(token), "Source data array (none): ")) return(NOMORE);
			if (! GVGetInfo(token, &type, (void **) &varptr)) {
				ERRprintf("ERROR: Variable %s does not seem to exist\n", token);
				return(NOMORE);
			}
			switch (type) {
				case GV_ARRAY:
				case GV_ARRAY_LINK:
					x   =  ((ARRAY *) *varptr)->x;
					npt = *((ARRAY *) *varptr)->size;
					break;
				case GV_SURFACE:
					x   = ((SURFACE *) *varptr)->z;
					npt = ((SURFACE *) *varptr)->ncol * ((SURFACE *) *varptr)->nrow;
					break;
				default:
					ERRprintf("ERROR: Variable %s is not an array or surface (type=0x%4.4X)\n", token, type);
					return(NOMORE);
			}
			w = NULL;
			ArrayStats(x,npt, &xmin, &xmax, &xave, &sdev);
			dx = 1.2*sdev/pow(npt,0.3333);			/* Theory says 3.49 -- too wide for me */
			dx = round_me(dx, 2);						/* Rounded off so not rediculous */
			ndx = 0;
		} else if (LexEqual(token, "-WEIGHTED", 2)) {
			weighted = TRUE;
		} else if (LexEqual(token, "-CENTER", 2)) {
			center = TRUE;
		} else if (LexEqual(token, "-NORMALIZED", 2)) {
			normalize = TRUE;
		} else if (LexEqual(token, "-DENSITY", 2)) {
			densify = TRUE;
		} else if (LexEqual(token, "-DX", 3) || LexEqual(token, "-width", 4)) {
			sprintf(msg, "Width of intervals in histogram (%g): ", dx);
			dx = fabs(LexGetReal((REAL) dx, msg));
			ndx = 0;
		} else if (LexEqual(token, "-NX", 3)) {
			ndx = LexGetInt(100, "Number of histogram bins (100): ");
		} else if (LexEqual(token, "-SPAN", 4)) {
			xmin = LexGetReal((REAL) xmin, "Lower band of binning (data): ");
			xmax = LexGetReal((REAL) xmax, "Upper band of binning (data): ");
		} else {
			ERRprintf("ERROR: %s is not a recognized option\n", token);
			return(NOMORE);
		}
	}

/* If there isn't any data at this point, resut is trivial :-) */
	if (npt == 0) return(OKAY);

/* Now, start calculating real ranges */
	a = center ? 0.5 : 0.0;							/* Offset for centering bins */
	if (ndx > 0) dx = fabs(xmax-xmin)/ndx;		/* If specifying ndx */
	if (dx == 0) {
		ERRprintf("ERROR: Bin width is zero size -- I'm crazy, but not stupid!\n");
		return(NOMORE);
	}

/* Calculate number of intervals and where they will be */
	ilow  = (long) floor(xmin/dx+a) - 1;		/* Bin of low to be 1		*/
	ihigh = (long) floor(xmax/dx+a) + 1;		/* Bin of top to npt-2		*/

	if ( (ihigh-ilow+1) > GVI_MAX_LENGTH) {	/* Too many? */
		ERRprintf("ERROR: Number of bins exceeds capability of this version\n");
		return(NOMORE);
	}
	nwin  = (INT) (ihigh - ilow + 1);			/* One below, one above	*/
	if ( (bptr = calloc(nwin, sizeof(*bptr))) == NULL) {
		ERRprintf("ERROR: Unable to allocate space to handle binning of this data\n");
		return(NOMORE);
	} 

	for (i=0; i <npt; i++) {								/* And loop data	*/
		j = (int) (floor(x[i]/dx+a) - ilow);			/* Bin to place	*/
		if (weighted && w != NULL) {
			bptr[j] += w[i];
		} else {
			bptr[j]++;
		}
	}

/* Normalize if required */
	if (normalize) {
		sum = 0;
		for (i=0; i<nwin; i++) sum += bptr[i];
		if (sum != 0) { for (i=0; i<nwin; i++) bptr[i] /= sum; }
	}

/* Densify if required */
	if (densify) {
		if (dx != 0.0) { for (i=0; i<nwin; i++) bptr[i] /= dx; }
	}

/* Increase size if necessary */
	if (nwin > GptCurve->nptmax) {
		if (! GVResize(GptUseCurve, nwin)) {
			ERRprintf("ERROR: Unable to resize curve to hold result of binning\n");
			return(NOMORE);
		}
		GptLinkXYZ(GptUseCurve);
	}

/* Save result */
	for (i=0; i<nwin; i++) {
		GptCurve->x[i] = (REAL) ((ilow+0.5f-a+i)*dx);
		GptCurve->y[i] = (REAL) bptr[i];
	}
	GptCurve->npt = nwin;
	free(bptr);
	
	return(OKAY);
}


/* ===========================================================================
-- 2D Histogramming routine.  Takes x,y and possibly z curve pointers and
-- creates a surface corresponding to the probability (or number) of times
-- the point is in a particular region.
--
-- Usage:  int hist_2d_me(REAL *x, REAL *y, REAL *z, int npt);
--
-- Inputs: x,y - required pointers to an array of points
--         z   - optional pointer to Z values for weighted histogram
--         npt - number of elements in the array
--
-- Output: Requests and creates a surface containing the 2D histogram
--
-- Return: OKAY if everytyhing succeeds, NOMORE on errors
--
-- Note: Has extensive set of its own command options which are parsed.
=========================================================================== */
static char Hist_2D_Help[] = 
"\n"
"Usage: 2D_HISTogram <surfname> [-options]\n"
"\n"
"Will create a surface based on histogram of existing curve in 2D\n"
"\n"
"Options:\n"
"    -center     -> use centered bins (-dx/2,dx/2) rather than (0,dx)\n"
"    -normalized -> normalize so sum(y) = 1\n"
"    -density    -> per unit dx*dy interval\n"
"    -weighted   -> bin +z[i] instead of +1 for each value (only in 3D)\n"
"    -dx <val>   -> set the bin width in X\n"
"    -dy <val>   -> set the bin width in Y\n"
"    -nx <ival>  -> set # of bins in X\n"
"    -ny <ival>  -> set # of bins in Y\n"
"    -xspan <low> <high> -> range of values in X used\n"
"    -yspan <low> <high> -> range of values in Y used\n"
"\n"
"Defaults: nx=ny=100 with span existing over full range of data\n";

PRIVATE int hist_2d_me(REAL *x, REAL *y, REAL *z, int npt) {

	INT  irow,icol, nrow, ncol, ndx, ndy;
	long i, ilow, ihigh, jlow, jhigh;
	REAL a, sum, xmin,xmax, ymin,ymax, dx,dy;
	LOGICAL center=FALSE, weighted=FALSE, normalize=FALSE, densify=FALSE;
	char token[DFLT_STR_SIZE], surfname[DFLT_STR_SIZE];

	SURFACE *s;
	int type;
	void **varptr;

/* First, check for help query */
	if (LexCheckHelp("2D_Histogram", Hist_2D_Help, NULL)) return(OKAY);

	if (! LexGetTokenP(surfname, sizeof(surfname), "Histogram surface to be created (abort): ")) return(NOMORE);

/* Calculate the range (for use in options) and set up default values */
	ArrayMinMax(x, npt, &xmin, &xmax);			/* Find range */
	ArrayMinMax(y, npt, &ymin, &ymax);			/* Find range */
	dx  = dy  = 0;										/* Work with ndx,ndy instead */
	ndx = ndy = 100;									/* Suggested # of intervals */

/* Scan options */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-WEIGHTED", 2)) {
			if (z == NULL) {
				ERRprintf("ERROR: Come on - it's hard to weight if there's no Z data\n");
				return(NOMORE);
			}
			weighted = TRUE;
		} else if (LexEqual(token, "-CENTER", 2)) {
			center = TRUE;
		} else if (LexEqual(token, "-NORMALIZED", 2)) {
			normalize = TRUE;
		} else if (LexEqual(token, "-DENSITY", 2)) {
			densify = TRUE;
		} else if (LexEqual(token, "-DX", 3)) {
			dx = LexGetReal(-1.0, "X increment in histogram (span/100): ");
			ndx = (dx <= 0) ? 100 : 0;
		} else if (LexEqual(token, "-NX", 3)) {
			ndx = LexGetInt(100, "Number of bins along X (100): ");
		} else if (LexEqual(token, "-DY", 3)) {
			dy = LexGetReal(-1.0, "Y increment in histogram (span/100): ");
			ndy = (dy <= 0) ? 100 : 0;
		} else if (LexEqual(token, "-NY", 3)) {
			ndy = LexGetInt(100, "Number of bins along Y (100): ");
		} else if (LexEqual(token, "-XSPAN", 4)) {
			xmin = LexGetReal(xmin, "Lower band of X binning (data): ");
			xmax = LexGetReal(xmax, "Upper band of X binning (data): ");
		} else if (LexEqual(token, "-YSPAN", 4)) {
			ymin = LexGetReal(ymin, "Lower band of Y binning (data): ");
			ymax = LexGetReal(ymax, "Upper band of Y binning (data): ");
		} else {
			ERRprintf("ERROR: %s is not a recognized option for 2D histograms\n", token);
			return(NOMORE);
		}
	}
	a = center ? 0.5f : 0.0f;									/* Offset for centering bins */
	if (ndx > 0) dx = (REAL) (fabs(xmax-xmin)/ndx);		/* If specifying ndx or ndy */
	if (ndy > 0) dy = (REAL) (fabs(ymax-ymin)/ndy);

	if (dx == 0 || dy == 0) {
		gen_err("Bin widths are zero -- I'm crazy, but not stupid!");
		return(NOMORE);
	}

/* Calculate number of intervals and where they will be */
	ilow  = (long) floor(xmin/dx+a) - 1;		/* Bin of low to be 1		*/
	ihigh = (long) floor(xmax/dx+a) + 1;		/* Bin of top to npt-2		*/
	jlow  = (long) floor(ymin/dy+a) - 1;		/* Bin of low to be 1		*/
	jhigh = (long) floor(ymax/dy+a) + 1;		/* Bin of top to npt-2		*/

	if ( (ihigh-ilow+1) > 16384 || (jhigh-jlow+1) > 16384) {	/* Too many? */
		gen_err("Only allow up to 16384 bins in x and y on surface creation");
		return(NOMORE);
	}
	ncol  = (INT) (ihigh - ilow + 1);			/* One below, one above X spacing */
	nrow  = (INT) (jhigh - jlow + 1);			/* One below, one above Y spacing */

/* ... create surface */
	if (! GVAllocSurface(surfname, GVF_USER, nrow, ncol)) {
		ERRprintf("ERROR: Unable to allocate new surface as %s\n", surfname);
		return(NOMORE);
	} else if (! GVGetInfo(surfname, &type, (void **) &varptr) || (type != GV_SURFACE)) {
		ERRprintf("ERROR: Surface variable %s was not allocated\n", surfname);
		return(NOMORE);
	}
	s = (SURFACE *) *varptr;
	for (i=0; i<nrow*ncol; i++) s->z[i] = 0;						/* Empty out the bins */
	for (i=0; i<ncol; i++) s->x[i] = (ilow+0.5f-a+i)*dx;
	for (i=0; i<nrow; i++) s->y[i] = (jlow+0.5f-a+i)*dy;
	strcpy(s->ids, "2D histogram");

/* And now do the data */
#define	PZ(row,col)					(s->z[(row)+(col)*nrow])
	for (i=0; i<npt; i++) {
		icol = (int) (floor(x[i]/dx+a) - ilow);			/* Bin to place	*/
		irow = (int) (floor(y[i]/dy+a) - jlow);			/* Bin to place	*/
		if (icol < 0 || icol >= ncol || irow < 0 || irow >= nrow) continue;
		PZ(irow,icol) += (weighted && z != NULL) ? z[i] : 1 ;	/* Increment bin	*/
	}

/* Normalize if required */
	if (normalize) {
		sum = 0;
		for (i=0; i<nrow*ncol; i++) sum += s->z[i];
		for (i=0; i<nrow*ncol; i++) s->z[i] /= sum;
	}

/* Densify if required */
	if (densify) {
		for (i=0; i<nrow*ncol; i++) s->z[i] /= (dx*dy);
	}

	return(OKAY);
}


/* ===========================================================================
-- Kernal based probability density approximation.  Takes a pointer to
-- observed events and approximates the probability density function based
-- on the kernal method.  Essentially, the kernal is a unit normalized
-- convolution function and
--        f(x) = sum k( (x-x_i)/h )
-- where k(x) is the kernal function, h is the bandwidth, and the sum
-- extends over all of the data points.  There are numerous choices for
-- the kernal function, but most don't really matter.
--
-- Usage:  int kernal_me(REAL *x, int npt);
--
-- Inputs: x   - pointer to an array of points
--         npt - number of elements in the array
--
-- Output: Replaces x,y,npt with an estimate of the probability
--         distribution function (fancy histogram)
--
-- Return: OKAY if everytyhing succeeds, NOMORE on errors
--
-- Note: Has extensive set of its own command options which are parsed.
=========================================================================== */
static char Kernal_Help[] = 
"\n"
"Usage: transform [y | x] kernal [-options] <bandwidth>\n"
"\n"
"Creates a kernal estimate for probability density function corresponding\n"
"to the current array data.  This is an alternative to the histogram and\n"
"creates a smooth function for the probability density.  Only returns the\n"
"normalized probability density.  By default, uses the triangular kernal\n"
"though others may be specified.\n"
"\n"
"Options:\n"
"    -array <var>     -> histogram an array or surface rather than y\n"
"       -surface      -> synonymous with -array\n"
"       -source       -> synonymous with -array\n"
"    -h <val>         -> set the bandwidth to <val>\n"
"    -width <val>     -> synonymous with -h\n"
"    -bandwidth <val> -> synonymous with -h\n"
"    -nx <ival>       -> Use <ival> points rather than default\n"
"\n"
"Defaults: By default, the number of points in the final probability\n"
"          distribution is such that the kernal bandwidth covers at least\n"
"          11 points, and that at least 500 points are used over the full\n"
"          span.  The total number of points may be specified with -nx\n";

PRIVATE int kernal_me(REAL *x, int npt) {

	INT  nwin, ndx, npt_minimum;
	long i,j, ilow, ihigh;
	double bandwidth, dx, xmin, xmax, xave, sdev, zval, *bptr;
	char token[DFLT_STR_SIZE];
	BOOL SawBandwidth;
	enum _KERNAL_TYPE {TRIANGULAR=1, EPANECHNIKOV=2, BIWEIGHT=3, TRIWEIGHT=4, COSINE=5} kernal_type;

/* First, check for help query */
	if (LexCheckHelp("Kernal", Kernal_Help, NULL)) return(OKAY);

/* Scan options */
	SawBandwidth = FALSE;
	bandwidth = 0.0;  npt_minimum = 500;
	kernal_type = EPANECHNIKOV;
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-ARRAY", 2) || LexEqual(token, "-SURFACE", 4) || LexEqual(token, "-SOURCE", 3)) {
			int type;
			void **varptr;
			if (! LexGetTokenP(token, sizeof(token), "Source data array (none): ")) return(NOMORE);
			if (! GVGetInfo(token, &type, (void **) &varptr)) {
				ERRprintf("ERROR: Variable %s does not seem to exist\n", token);
				return(NOMORE);
			}
			switch (type) {
				case GV_ARRAY:
				case GV_ARRAY_LINK:
					x   =  ((ARRAY *) *varptr)->x;
					npt = *((ARRAY *) *varptr)->size;
					break;
				case GV_SURFACE:
					x   = ((SURFACE *) *varptr)->z;
					npt = ((SURFACE *) *varptr)->ncol * ((SURFACE *) *varptr)->nrow;
					break;
				default:
					ERRprintf("ERROR: Variable %s is not an array or surface (type=0x%4.4X)\n", token, type);
					return(NOMORE);
			}
		} else if (LexEqual(token, "-type", 5)) {
			LexGetTokenP(token, sizeof(token), "Type of kernal function (TRIANGULAR, Epanechnikov, Biweight, Triweight, Cosine): ");
			i = LexSelect(token, "TRIANGULAR Epanechnikov Biweight Triweight Cosine");
			if (i <= 0) {
				ERRprintf("ERROR: Unrecognized kernal type (%s)\n", token);
				return NOMORE;
			}
			kernal_type = (enum _KERNAL_TYPE) i;
		} else if (LexEqual(token, "-h", 2) || LexEqual(token, "-width", 4) || LexEqual(token, "-bandwidth", 5)) {
			bandwidth = LexGetReal(0.0, "Bandwidth for kernal function (default): ");
			SawBandwidth = TRUE;
		} else if (LexEqual(token, "-NX", 3)) {
			npt_minimum = LexGetInt(500, "Minimum number of points in final function (500): ");
		} else {
			ERRprintf("ERROR: %s is not a recognized option\n", token);
			return(NOMORE);
		}
	}

/* Calculate the range (for use in options) and set up default values */
	if (npt <= 0) {
		ERRprintf("WARNING: No data in curve for estimating probability distribution\n");
		return OKAY;
	}
	ArrayStats(x,npt, &xmin, &xmax, &xave, &sdev);
	if (bandwidth <= 0) {											/* Still need a bandwidth? */
		bandwidth  = (REAL) (1.06*sdev/pow(npt,0.2));		/* From theory for kernals (Dekking) */
		bandwidth = (REAL) round_me(bandwidth, 2);			/* Rounded off so not rediculous */
		if (! SawBandwidth) {
			if (! LexChkToken(token, sizeof(token))) {
				TTYprintf("Data stats: min=%g max=%g  <ave>=%g  <sigma>=%g\n", xmin, xmax, xave, sdev);
				TTYprintf("  Recommended bandwidth: %g\n", bandwidth);
			}
			bandwidth = LexGetReal((REAL) bandwidth, "Bandwidth for kernal function (recommended value): ");
		}
	}

/* Now, start calculating real ranges */
	if (bandwidth <= 0) {
		ERRprintf("ERROR: Bandwidth must be positive\n");
		return NOMORE;
	}
	dx = (2*bandwidth) / 10.0;										/* 11 intervals across the kernal function */
	ndx = (int) ((xmax-xmin) / dx + 11);						/* Estimate for number of points needed */
	if (ndx < npt_minimum) {										/* Keep enough points to look good */
		dx = (xmax-xmin+2*bandwidth)/(npt_minimum-1);		/* And at least specified number across range */
	}
	dx = round_me(dx, 1);											/* Finally, round to a nice value */

/* Calculate number of intervals and where they will be */
	ilow  = (long) floor((xmin-bandwidth)/dx) - 1;			/* Bin of low to be 1		*/
	ihigh = (long) floor((xmax+bandwidth)/dx) + 1;			/* Bin of top to npt-2		*/

	nwin  = (INT) (ihigh - ilow + 1);							/* One below, one above	*/
	if ( nwin > GVI_MAX_LENGTH) {									/* Too many? */
		ERRprintf("ERROR: The kernal width would require too many points across the data span\n");
		return NOMORE ;
	} else if ( (bptr = calloc(nwin, sizeof(*bptr))) == NULL) {
		ERRprintf("ERROR: Unable to allocate space to create the kernal curve (nwin=%d)\n", nwin);
		return NOMORE;
	} 

/* Now ... just go through all points and calculate kernal at the x values */
	for (i=0; i <npt; i++) {										/* And loop data	*/
		j = (int) (floor((x[i]-bandwidth)/dx) - ilow);		/* Point to start */
		if (j < 0) j = 0;
		zval = -1.0;
		while (j < nwin && zval < 1-dx/bandwidth) {
			zval = ((ilow+j)*dx-x[i])/bandwidth;
			if (fabs(zval) < 1) switch (kernal_type) {
				case TRIANGULAR:
					bptr[j] += 1-fabs(zval);						/* Most simple */
					break;
				case EPANECHNIKOV:
					bptr[j] += 0.75*(1-zval*zval);
					break;
				case BIWEIGHT:
					bptr[j] += 15.0/16.0*pow(1-zval*zval,2);
					break;
				case TRIWEIGHT:
					bptr[j] += 35.0/32.0*pow(1-zval*zval,3);
					break;
				case COSINE:
					bptr[j] += 0.78539816*cos(1.5707963*zval);
					break;
			}
			j++;
		}
	}

/* Normalize by number of data points and the bandwidth */
	for (i=0; i<nwin; i++) bptr[i] /= (npt*bandwidth);

/* Increase size if necessary */
	if (nwin > GptCurve->nptmax) {
		if (! GVResize(GptUseCurve, nwin)) {
			ERRprintf("ERROR: Unable to resize curve to hold result of kernal estimation\n");
			return NOMORE;
		}
		GptLinkXYZ(GptUseCurve);
	}

/* Save result */
	for (i=0; i<nwin; i++) {
		GptCurve->x[i] = (REAL) ((ilow+i)*dx);
		GptCurve->y[i] = (REAL) bptr[i];
	}
	GptCurve->npt = nwin;
	free(bptr);

	return(OKAY);
}


/* ===========================================================================
-- Cummulative probability distribution routine.  Takes x,y pointers and creates
-- a curve of the cummulative probability.  Effectives sorts the data, sets
-- X to the values, and sets Y to a uniform probability growing from 0 to 1
--
-- Usage:  int cdf_me(REAL *x, REAL *y, int npt);
--
-- Inputs: x,y - required pointers to an array of points
--         npt - number of elements in the array
--
-- Output: Replaces x,y with the cummulative probability result
--
-- Return: OKAY if everytyhing succeeds, NOMORE on errors
=========================================================================== */
static char CDF_Help[] = 
"\n"
"Usage: CDF [-options]\n"
"\n"
"Converts the data to a cummulative probability distribution function\n"
"giving probability of F(a) = P(x<=a).  This is the empirical estimate\n"
"and is just the sorted data with an assigned F(a) value assuming equal\n"
"intervals.\n"
"\n"
"By default, it is assumed that the dataset has seen neither the highest\n"
"nor the smallest value, so the limiting points are assigned values of\n"
"1/2n and 1-1/2n respectively.  The -FULL option cases the limiting values\n"
"to be assigned values of 0 and 1.  Thus the returned range of Y is either\n"
"[1/2n, 1-1/2n] or [0,1] depending on the -FULL option\n"
"\n"
"Options:\n"
"    -full    -> return CDF on range [0,1] covering all values\n";

PRIVATE int cdf_me(REAL *x, REAL *y, int npt) {

	long i;
	char token[DFLT_STR_SIZE];
	LOGICAL full=FALSE;

/* First, check for help query */
	if (LexCheckHelp("CDF", CDF_Help, NULL)) return(OKAY);

/* Scan options */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-FULL", 2)) {
			full = TRUE;
		} else {
			ERRprintf("ERROR: %s is not a recognized option\n", token);
			return(NOMORE);
		}
	}

/* If there isn't any data at this point, resut is trivial :-) */
	if (npt == 0) {
		return OKAY;
	} else if (npt == 1) {
		x[0] = y[0];
		y[0] = 0.5;
		return OKAY;
	}

/* Simple - sort the data, move to X, assign Y */
	heap_sort(y, NULL, NULL, npt, SORT_ON_X);
	for (i=0; i<npt; i++) {
		x[i] = y[i];
		y[i] = (REAL) (full ? (i/(npt-1.0)) : ((i+0.5)/npt));
	}
	return OKAY;
}


/* ===========================================================================
-- Routines to support histogram functions
--
-- ArrayStats (double precision)
-- round_me
=========================================================================== */
static void ArrayStats(REAL *x, int npt, double *pxmin, double *pxmax, double *pxave, double *pxsdev) {

	int i;
	double xmin=0.0, xmax=0.0, xave=0.0, xsdev=0.0, x1=0.0, x2=0.0;

	if (npt > 0) {
		xmin = xmax = x[0];
		for (i=0; i<npt; i++) {
			if (x[i] < xmin) xmin = x[i];
			if (x[i] > xmax) xmax = x[i];
			x1 += x[i];
			x2 += x[i]*x[i];
		}
		xave = x1/npt;
		xsdev = sqrt(x2/npt-xave*xave);
	}

	if (pxmin  != NULL) *pxmin  = xmin;
	if (pxmax  != NULL) *pxmax  = xmax;
	if (pxave  != NULL) *pxave  = xave;
	if (pxsdev != NULL) *pxsdev = xsdev;
	return;
}

static double round_me(double x, int n) {
	BOOL sign;
	int iexp;

	if (x == 0) return 0;
	sign = (x<0); if (sign) x = -x;
	iexp = (int) floor(log10(x)); if (iexp != 0) x = x/pow(10,iexp);
	x = ((int) (x*pow(10,n)+0.5)) / pow(10,n);
	if (iexp != 0) x = x*pow(10,iexp);
	if (sign) x = -x;
	return x;
}
