/* fft_me.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE                                           /* Always require POSIX standard */
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
#include "lexp.h"
#include "gptxtrn.h"

#include "fft2c.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define	BUFREAL		"CBUF$R"
#define	BUFIMAG		"CBUF$I"
#define	BUFCOMPLEX	"CBUF$Z"

typedef enum _enumtype_ {DUMMY} ENUMDUMMY ;

typedef struct _CMTYPE {
	const char *command;
	int length;
	ENUMDUMMY *element;
	ENUMDUMMY value;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int cv_power_2(CURVE *cv, char *cvname, BOOL ReLink);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
PRIVATE enum {W_SQUARE=1, W_PARZEN=2, W_WELCH=3, WIN_UNKNOWN}
		  window = W_WELCH;
PRIVATE enum {D_INVERSE=-1, D_POWER=0, D_FORWARD=+1, DIR_UNKNOWN}
		  dir    = D_FORWARD;
PRIVATE enum {O_NOMODIFY=0, O_COMPLEX, O_REAL, O_IMAG, O_MAGN, O_PHASE, O_DEPHASE, OUT_UNKNOWN} 
        output = O_COMPLEX;
PRIVATE enum {F_SYMMETRIC, F_FULL, FRM_UNKNOWN}
		  form   = F_SYMMETRIC;
PRIVATE enum {D_NODEFINE, D_DEFINE, DEF_UNKNOWN}
		  define = D_NODEFINE;
PRIVATE enum {S_DATA, S_BUFS, S_BUFZ, S_ZBUF, SRC_UNKNOWN}
		  source = S_DATA;
PRIVATE enum {P_COPYPAD, P_ZEROPAD, P_TRUNCATE, PAD_UNKNOWN}
        padding = P_COPYPAD;

/*---------------------------------------------------------------------------
            -1      0       1       2       3       4       5      6
           --------------------------------------------------------------
 Dir       invr    power   forw
 Window                    square  parzen  welch
 Source                    data    cbuf$r  cbuf$z  zbuf
 Output            none    cmplx   real    imag    magn    phase  dephase
 Format                    half    full
 Define            no      yes
 Padding           pad     zeropad truncate
---------------------------------------------------------------------------- */
PRIVATE const CMTYPE cmlist[] = {
		{"-FORWARD",	4,	(ENUMDUMMY *) &dir,	  (ENUMDUMMY) D_FORWARD},	/* +1     ==> direction */
		{"-POWER",     4, (ENUMDUMMY *) &dir,    (ENUMDUMMY) D_POWER},		/* POWER  ==> direction */
		{"-INVERSE",	4, (ENUMDUMMY *) &dir,    (ENUMDUMMY) D_INVERSE},	/* -1     ==> direction */

		{"-SQUARE",		7, (ENUMDUMMY *) &window, (ENUMDUMMY) W_SQUARE},	/* Square window */
		{"-PARZEN",		5, (ENUMDUMMY *) &window, (ENUMDUMMY) W_PARZEN},	/* Parzen window */
		{"-WELCH",		6, (ENUMDUMMY *) &window, (ENUMDUMMY) W_WELCH},		/* Welch  window */
			
		{"-CBUF",		5, (ENUMDUMMY *) &source, (ENUMDUMMY) S_BUFS},		/* CBUF source */
		{"-CBUF$R",		7, (ENUMDUMMY *) &source, (ENUMDUMMY) S_BUFS},		/* CBUF source */
		{"-CBUFZ",		6, (ENUMDUMMY *) &source, (ENUMDUMMY) S_BUFZ},		/* CBUF source is complex array */
		{"-CBUF$Z",		7, (ENUMDUMMY *) &source, (ENUMDUMMY) S_BUFZ},		/* CBUF source is complex array */
		{"-ZBUF",		5, (ENUMDUMMY *) &source, (ENUMDUMMY) S_ZBUF},		/* CBUF source is complex array */
		{"-DATA",		5, (ENUMDUMMY *) &source, (ENUMDUMMY) S_DATA},		/* Data source */
			
		{"-COMPLEX",	4, (ENUMDUMMY *) &output, (ENUMDUMMY) O_COMPLEX},	/* Complex output */
		{"-REAL",		5,	(ENUMDUMMY *) &output, (ENUMDUMMY) O_REAL},		/* Real output */
		{"-IMAGINARY",	3,	(ENUMDUMMY *) &output, (ENUMDUMMY) O_IMAG},		/* Imaginary output */
		{"-MAGNITUDE",	5, (ENUMDUMMY *) &output, (ENUMDUMMY) O_MAGN},		/* Magnitude output */
		{"-PHASE",		6, (ENUMDUMMY *) &output, (ENUMDUMMY) O_PHASE},		/* Phase output */
		{"-DEPHASE",	6, (ENUMDUMMY *) &output, (ENUMDUMMY) O_DEPHASE},	/* Dephase output */
		{"-NOMODIFY",	4, (ENUMDUMMY *) &output, (ENUMDUMMY) O_NOMODIFY},	/* No modification of anything */

		{"-FULL",		5,	(ENUMDUMMY *) &form,	(ENUMDUMMY) F_FULL},			/* Full format */
		{"-HALF",		5, (ENUMDUMMY *) &form,	(ENUMDUMMY) F_SYMMETRIC},	/* Half format */

		{"-PAD",			4, (ENUMDUMMY *) &padding,(ENUMDUMMY) P_COPYPAD},	/* copy padding */
		{"-EXTEND",		4, (ENUMDUMMY *) &padding,(ENUMDUMMY) P_COPYPAD},	/* copy padding */
		{"-ZEROPAD",	6, (ENUMDUMMY *) &padding,(ENUMDUMMY) P_ZEROPAD},	/* zero padding */
		{"-TRUNCATE",	6, (ENUMDUMMY *) &padding,(ENUMDUMMY) P_TRUNCATE},	/* truncate padding */

		{"-DEFINE",		4,	(ENUMDUMMY *) &define, (ENUMDUMMY) D_DEFINE},	/* Define CBUFR/I */
		{NULL,			0, NULL, (ENUMDUMMY) 0} };

/* ============================================================================
--  Usage:  int fft_me(CURVE *cv, char *cvname);
--
--  Routine to handle all the strange FFT's of curves
-- 
--  Inputs: cv     - pointer to curve structure containing the values
--          cvname - name of the curve pointed to by cv (for resizing)
--
--  Output: may modify x,y, npt and nptmax in curve
============================================================================ */
static char FFTHelp[] = 
"\n"
" Transformation performs complex or real FFT conversions.  By default, the\n"
" data in the main curve is converted, but complex or real buffers may also\n"
" also be specified.  The operation is complex, and it is critical to\n"
" understand the subleties of Fourier transforms to fully utilize.  Only\n"
" minimal explanations of the options are given here.\n"
"\n"
"   TRANSF FFT [-options]\n"
"\n"
" Options within a class are generally exclusive -- ie. only one may be set.\n"
" The default is indicated by a *\n"
"\n"
"   Transform type:     * -FORward    -INVerse     -POWer\n"
"   Windowing (power):    -SQUARE     -PARZen      -WELCH\n"
"   Source:             * -DATA       -CBUF        -CBUF$Z      -ZBUF <z_array>\n"
"   Output:             * -COMplex    -REAL        -IMaginary   -MAGNitude\n"
"                         -PHASE      -DEPHAse     -NOModify\n"
"   Format:             * -FULL       -HALF\n"
"   Padding:            * -PAD (or -EXTEND)        -ZEROPad     -TRUNCate\n"
"   Create cbuf$r,i,z:    -DEFINE\n"
"   Resolution:           -RESolution <n>  - or -  -SKIP <n>\n"
"\n"
"Notes:\n"
"   1. FFT operates only on data sets with an even power of 2 number of points.\n"
"      Data must be extended or truncated to produce such data sets by padding.\n"
"      The options -PAD and -EXTEND are synonymous and duplicate data by copying\n"
"      the beginning of the data set.  -ZEROpad pads with zeros, and -TRUNCATE\n"
"      simply reduces the number of points to the next lower power of 2.\n"
"   2. The Welch window is initially used for power spectrum estimation.\n"
"      Changing the window function is sticky and retained across calls.\n"
"   3. Normalization is handled on the inverse transform only.\n"
"   4. The output options apply to the X,Y curve only.  The array call\n"
"      versions are always modified\n"
"   5. The arrays for -CBUF must be named CBUF$R and CBUF$I.  For -CBUFZ, it\n"
"      must be a CBUF$Z complex array.  ZBUF allows arbitrary names\n"
"   6. Because this is an FFT, the number of data points must be an exact\n"
"      power of 2.  If from data, padding or truncation may be used.\n"
"   7. Because of the expected use, -ZBUF defaults to -NOMOD and -FULL\n"
"   8. Not all option combinations are valid.  Inappropriate combinations\n"
"      will use reasonable guesses instead\n"
"\n"
"  Typical use:\n"
"     transf fft -power -welch\n"
"     transf fft -magn -half\n"
"     alloc u1 complex array 1024 transf fft -zbuf u1\n";

int fft_me(CURVE *cv, char *cvname) {

	static REAL omega, xlast=0, lastphase=0;

/* Data x,y information */
	REAL *x,*y;									/* X,Y buffer */
	INT  npt, nptuse;							/* Number of points */
	REAL *rdata, *idata;						/* Real and Imag pointers	*/

	char token[OPTION_STR_SIZE];
	char zbuffer[DFLT_STR_SIZE];
	INT i,j,resolution,ineed;				/* Interpolation resolution */
	int  itype;
	ARRAY **aaptr=NULL;
	COMPLEX_ARRAY **captr=NULL;
	COMPLEX *cptr;

	CMTYPE *citem;

/* Set default values */
	dir        = D_FORWARD;					/* Forward operation		*/
	source     = S_DATA;						/* Source is DATA			*/
	output     = O_COMPLEX;					/* Complex output			*/
	define     = D_NODEFINE;				/* No define initially	*/
	padding    = P_COPYPAD;					/* Copy padding			*/
	resolution = 1;							/* Pixel resolution		*/

/* ... Check for a help entry */
	if (LexCheckHelp("FFT", FFTHelp, NULL)) return(OKAY);

/* ... Parse all options */
	dir        = DIR_UNKNOWN;				/* Forward operation		*/
	source     = SRC_UNKNOWN;				/* Source is DATA			*/
	output     = OUT_UNKNOWN;				/* Complex output			*/
	define     = DEF_UNKNOWN;				/* No define initially	*/
	padding    = PAD_UNKNOWN;				/* Copy padding			*/
	form       = FRM_UNKNOWN;				/* Form of transform		*/
	resolution = 1;							/* Pixel resolution		*/

	while (LexGetOption(token, sizeof(token))) {
		if ( (citem=LexCmdl(token, cmlist, sizeof(CMTYPE))) != NULL) {
			*citem->element = citem->value;
			if (citem->element == (ENUMDUMMY *) &source && citem->value == S_ZBUF) {		/* Special case */
				LexGetStrExprP(zbuffer, sizeof(zbuffer), "Z array: ");
				if (output == OUT_UNKNOWN) output = O_NOMODIFY;
				if (form   == FRM_UNKNOWN) form   = F_FULL;
			}
		} else if (LexEqual(token, "-resolution", 4) || LexEqual(token, "-SKIP", 5)) {
			resolution = LexGetInt(1, "Resolution enhancement (1): ");
		} else {
			gen_err2("Unrecognized FFT option", token);
			return(NOMORE);
		}
	}

	if (dir     == DIR_UNKNOWN) dir     = D_FORWARD;		/* Forward operation		*/
	if (source  == SRC_UNKNOWN) source  = S_DATA;			/* Source is DATA			*/
	if (output  == OUT_UNKNOWN) output  = O_COMPLEX;		/* Complex output			*/
	if (define  == DEF_UNKNOWN) define  = D_NODEFINE;		/* No define initially	*/
	if (padding == PAD_UNKNOWN) padding = P_COPYPAD;		/* Copy padding			*/
	if (form    == FRM_UNKNOWN) form    = F_SYMMETRIC;		/* Form of transform		*/
	if (resolution <= 0)        resolution = 1;				/* Pixel resolution		*/

	if (output == O_DEPHASE) {						/* Okay, handle dephase this way */
		LexInsText("transform y dephase");
		output = O_PHASE;
	}

/* ===========================================================================
-- ... Here to handle requested FFT of main data curve
============================================================================ */
	if (source == S_DATA) {							/* Want to work from data */

/* ... Check the data validity and determine best NPT to use */
		npt = cv->npt;
		if (npt < 8) {							/* Invalid # of points				*/
			ERRprintf("FOOLISH ERROR: You are ON DRUGS! Too few points! (FFT)\n");
			return(NOMORE);
		}

		if (padding == P_TRUNCATE) {					/* Truncate data set? */
			nptuse = fft_power_2(npt, npt);
		} else {
			nptuse = cv_power_2(cv, cvname, TRUE);	/* Nearest valid power of 2?	*/
			for (i=npt; i<nptuse; i++)
				cv->y[i] = (padding == P_COPYPAD) ? cv->y[i-npt] : 0.0f;
		}

/* ... Get addresses of X, Y, determine frequency interval, and set new npt */
		x = cv->x;	y = cv->y;							/* X,Y buffers */
		omega = (npt/(x[npt-1]-x[0])) / nptuse;	/* Frequency interval				*/
		npt = nptuse;

/* ... Alias the data through resolution request */
		if (resolution > 1) {					/* Enhanced resolution?? */
			j = npt/resolution;
			for (i=0; i<j; i++)   y[i] = y[i*resolution]*resolution;	/* Equal power */
			for (i=j; i<npt; i++) y[i] = 0;
			i = (INT) ((resolution-1.0)/resolution * (npt/2));
			fft_y_roll(y,npt,i);								/* Center the data */
			omega = omega/resolution;						/* Smaller intervals */
		}

/* ... Do the requested FFT operation */
		if (dir == D_POWER) {								/* Power estimation		*/
			fft_pow_est(window, y, &npt);					/* Power (changes NPT)	*/
			for (i=0; i<npt; i++) x[i] = i*omega;		/* Set FREQ spectrum		*/
			define = D_NODEFINE; output = O_NOMODIFY;	/* Don't change further	*/
		} else if (dir == D_FORWARD) {
			fft_real(y,npt,1);								/* Take the FFT			*/
			lastphase = (xlast=x[0])*omega;				/* Get phase and xlast	*/
			fft_shft(y,y+(npt/2),npt/2,lastphase);		/* Correct for Y value	*/
			for (i=0; i<npt; i++) x[i] = i*omega;		/* And set X values		*/
		} else if (dir == D_INVERSE) {
			fft_shft(y,y+npt/2,npt/2,-lastphase);		/* Undo phase shift		*/
			fft_real(y,npt,-1);								/* Inverse FFT				*/
			for (i=0; i<npt; i++) x[i] = xlast+i*omega;	/* And attempt x		*/
		}

/* Allocate and fill in CBUF$R/I if requested */
		if (define == D_DEFINE) {							/* Allocate buffer space */
			ineed = (form==F_FULL) ? npt : npt/2 ;		/* Symmetric needs NPT/2 */
			if (!GVGetInfo(BUFREAL, &itype, (void **) &aaptr) || (itype!=GV_ARRAY && itype!=GV_ARRAY_LINK) || ((*aaptr)->maxsize < ineed))
				GVAllocArray(BUFREAL, GVF_USER, ineed);
			if (!GVGetInfo(BUFIMAG, &itype, (void **) &aaptr) || (itype!=GV_ARRAY && itype!=GV_ARRAY_LINK) || ((*aaptr)->maxsize < ineed))
				GVAllocArray(BUFIMAG, GVF_USER, ineed);
			if (!GVGetInfo(BUFCOMPLEX, &itype, (void **) &captr) || (itype!=GV_COMPLEX_ARRAY) || ((*captr)->maxsize < ineed))
				GVAllocComplexArray(BUFCOMPLEX, GVF_USER, ineed);
			rdata = idata = NULL; cptr=NULL;
			if (GVGetInfo(BUFREAL,    NULL, (void **) &aaptr)) {rdata = (*aaptr)->x; *(*aaptr)->size = ineed;}
			if (GVGetInfo(BUFIMAG,    NULL, (void **) &aaptr)) {idata = (*aaptr)->x; *(*aaptr)->size = ineed;}
			if (GVGetInfo(BUFCOMPLEX, NULL, (void **) &captr)) {cptr  = (*captr)->z; *(*captr)->size = ineed;}
			if (rdata == NULL || idata == NULL || cptr == NULL) {
				gen_err("Unable to allocate CBUF$I/R/Z - arrays have not been properly initialized\n");
				return(NOMORE);
			}
			for (i=0; i<npt/2; i++) {rdata[i] = y[i]; idata[i] = y[i+npt/2];}		/* Full in unique part */
			if (form == F_FULL) fft_full(rdata, idata, npt);							/* Make symmetric if want */
			for (i=0; i<ineed; i++) {cptr[i].x=rdata[i]; cptr[i].y=idata[i];}		/* Fill in complex array */
		}

		if (output == O_REAL) {
			npt = npt/2;
		} else if (output == O_IMAG) {				/* Return only IMAG		*/
			npt = npt/2;
			for (i=0; i<npt; i++) y[i] = y[i+npt];
		} else if (output == O_MAGN) {				/* Magnitude only?		*/
			npt = npt/2;
			for (i=0; i<npt; i++) y[i] = (REAL) (sqrt(y[i]*y[i]+y[i+npt]*y[i+npt]));
		} else if (output == O_PHASE) {
			npt = npt/2;
			for (i=0; i<npt; i++) y[i] = (REAL) (atan2(y[i+npt], y[i]));
		}
		cv->npt = npt;
		return(OKAY);

/* ===========================================================================
-- ... Here to handle requested FFT of buffers CBUF$R/I
============================================================================ */
	} else {											/* CBUF$R/CBUF$I or CBUF$Z arrays */

		if (dir == D_POWER) {
			gen_err("Can't do power estimate on CBUF$R/I pair (FFT)");
			return(NOMORE);
		}

/* ... Determine addresses and size of CBUF$R/I */
		rdata = idata = NULL;

		if (source == S_BUFZ || source == S_ZBUF) {					/* Single complex array */
			if (source == S_BUFZ) strcpy(zbuffer, BUFCOMPLEX);
			if (GVGetInfo(zbuffer, &itype, (void **) &captr) && (itype==GV_COMPLEX_ARRAY) ) {
				npt   = *(*captr)->size;
				cptr  =  (*captr)->z;
				rdata = malloc(npt*sizeof(*rdata)); idata = malloc(npt*sizeof(*idata));
				for (i=0; i<npt; i++) { rdata[i] = cptr[i].x; idata[i] = cptr[i].y; }
			} else {
				ERRprintf("ERROR: %s did not parse as a complex array.  Must exist\n", zbuffer);
				return(NOMORE);
			}

		} else {
			if (GVGetInfo(BUFREAL, &itype, (void **) &aaptr) && (itype==GV_ARRAY || itype==GV_ARRAY_LINK)) {
				rdata =  (*aaptr)->x; 
				npt   = *(*aaptr)->size;
			}
			if (GVGetInfo(BUFIMAG, &itype, (void **) &aaptr) && (itype==GV_ARRAY || itype==GV_ARRAY_LINK)) {
				idata =  (*aaptr)->x; 
				if (npt != *(*aaptr)->size) idata = NULL;
			}
			if ( (rdata == NULL) || (idata == NULL) || (npt != fft_power_2(npt, npt)) ) {
				gen_err("CBUF$R/I pair or CBUF$Z are invalid or non-existent (FFT)");
				return(NOMORE);
			}
		}

		if (form == F_FULL) {						/* Do the full transform */
			fft2c(rdata, idata, npt, dir);
		} else if (form == F_SYMMETRIC) {		/* Do the symmetric transform */
			npt = 2*npt;								/* Real size is twice value	*/
			fft_r2(rdata, idata, npt, dir);		/* Special symmetric code		*/
			if (dir == D_FORWARD) { 				/* Only half actually there	*/
				npt = npt/2;
			} else if (output == O_COMPLEX) {	/* Complex, only real valid	*/
				output = O_REAL;
			} else if (output == O_MAGN) {		/* Packed data ----	hard!		*/
				output = O_REAL;						/* magn/phase undefined so		*/
			} else if (output == O_PHASE) {		/* translate to valid types	*/
				output = O_IMAG;
			}
		}

		if (output != O_NOMODIFY) {					/* Need any output values	*/
			ineed = (output != O_COMPLEX) ? npt : 2*npt ;
			if (cv->nptmax < ineed) {					/* Outputting anything? */
				gen_err("Too many points. Y modified only to NPTMAX (FFT)");
				npt = (output != O_COMPLEX) ? cv->nptmax : cv->nptmax/2 ;
			}
			cv->npt = ineed;								/* Tell GENPLOT how many valid */
			y = cv->y;
			for (i=0; i<npt; i++) {
				if (output == O_COMPLEX) {
					y[i] = rdata[i]; y[i+npt] = idata[i];
				} else if (output == O_REAL) {
					if (form==F_FULL || dir==D_FORWARD)		/* rdata is really valid */
						y[i] = rdata[i];
					else												/* values are packed */
						y[i] = ((i & 0x01) == 0) ? rdata[i/2] : idata[(i+1)/2] ;
				} else if (output == O_IMAG) {
					if (form==F_FULL || dir==D_FORWARD)		/* idata is really valid */
						y[i] = idata[i];
					else												/* All imag must be zero */
						y[i] = 0;
				} else if (output == O_MAGN) {
					y[i] = (REAL) (sqrt(rdata[i]*rdata[i]+idata[i]*idata[i]));
				} else if (output == O_PHASE) {
					y[i] = (REAL) (atan2(idata[i],rdata[i]));
				}
			}
		}

/* If complex buffer, put back into place and destroy temporary space */
		if (source == S_BUFZ || source == S_ZBUF) {		/* Single complex array		*/
			npt   = *(*captr)->size;							/* Reset in case changed	*/
			for (i=0; i<npt; i++) { cptr[i].x = rdata[i]; cptr[i].y = idata[i]; }
			free(rdata); free(idata);
		}
		return(OKAY);
	}
}

/*  ============================================================================
-- Usage: BOOL autocorr(CURVE *cv, char *cvname);
--
-- Returns the autocorrelation of a function with itself.
--
-- Inputs: cv     - pointer to curve containing data of interest
--         cvname - pointer to name of curve for possible resizing (NULL ok)
--
-- Output: modified X,Y, possibly npt,nptmax in curve
--           Y   - autocorrelation values          (center packed)
--           X   - lag value                       (0 at NPT/2)
--           NPT - possibly modified # of points
--
-- Note: NPT must be an even power of 2.  If it is not currently, the data will
--       be blank padded to the higher number, or data will be truncated.
============================================================================ */
BOOL autocorr(CURVE *cv, char *cvname) {

	REAL *x, *y;
	INT  i, npt;
	REAL tmp;

/* ... Determine closest power of 2 and zero fill Y if necessary */
	i = cv_power_2(cv, cvname, TRUE);			/* Nearest valid power of 2?	*/
	npt = cv->npt;
	x   = cv->x;
	y   = cv->y;
	while (npt<i) y[npt++] = 0;					/* Zero pad if necessary */
	npt = cv->npt = i;								/* In case truncated instead */
	tmp = x[1]-x[0];									/* Timing info */
	fft_auto(y, npt);									/* Do the autocorrelation */
	for (i=0; i<npt; i++) x[i] = (i+1-npt/2)*tmp;	/* Zero at NPT/2-1 */
	return(TRUE);
}

/* ***********************************************************************
-- Smoothing with FFT (Fast Fourier Transformation) based on routine in
--       Numerical Recipes
--       The Art of Scientific Computing                 
--       Cambridge University Press, Cambridge (1986)
--
-- Usage:  call FFTSmooth(y,npt,pst)
--
-- Inputs: y(*) - Array of evenly spaced points                  REAL ARRAY
--         npt  - Number of points in array                      INT
--         pst  - Smoothing range (points)                       REAL
--
-- Output: y(*) - Smoothed array                                 REAL ARRAY
--
-- Notes:  Smooths an array Y of length NPT, with a window whose full width is
--         of order of PST neighboring points, a user supplied value.
*********************************************************************** */
BOOL FFTSmooth(REAL *yp, INT npt, INT nptmax, REAL pts) {

	INT i,j,m;
	REAL a,b,var1,fac;
	REAL *y;

/* ... Determine size of FFT to use.  Must be data size + 2*window but < max */
	m = fft_power_2(npt+2*nint(pts), GVI_MAX_LENGTH);		/* Needed size */
	if (m <= nptmax) {
		y = yp;
	} else {
		y = malloc(m*sizeof(*y));
		memcpy(y, yp, npt*sizeof(*y));
	}

/* ... Remove the linear trend from the data and zero pad to fill buffer */
	a = y[0];
	b = (y[npt-1]-y[0])/(npt-1);
	for (i=0; i<npt; i++) y[i] = y[i]-(a+b*i);	/* Remove linear trend */
	while (i<m) y[i++] = 0;								/* And zero pad the rest */

/* ... Perform the real FFT */
	fft_real(y,m,+1);										/* Forward FFT */

	var1 = (pts/m)*(pts/m);								/* Scaling distance */
	fac = 1;													/* Start with factor = 1 */
	for (i=1; i<m/2; i++) {								/* Filter the transform */
		j = i+m/2;											/* Posn of imaginary part */
		if (fac > 0) {
			fac = 1-var1*i*i;								/* Inverted bell */
			if (fac < 0) fac = 0;
			y[i] *= fac;									/* Real part */
			y[j] *= fac;									/* Imaginary part */
		} else {
			y[i] = 0;										/* Real part */
			y[j] = 0;										/* Imaginary part */
		}
	}
	y[m/2] = 0;												/* Highest aliased point */
	fft_real(y,m,-1);										/* Inverse the FFT */

	for (i=0; i<npt; i++) y[i] = y[i]+(a+b*i);	/* Replace linear trend */

/* If we had to allocate work space, clean up now */
	if (yp != y) {
		memcpy(yp, y, npt*sizeof(*y));
		free(y);
	}

	return(TRUE);
}


/* ***********************************************************************
-- Smoothing with Gaussian (Fast Fourier Transformation) 
--
-- Usage:  call FFTGaussSmooth(y,npt,pst)
--
-- Inputs: y(*) - Array of evenly spaced points                  REAL ARRAY
--         npt  - Number of points in array                      INT
--         pst  - Smoothing range (points)                       REAL
--                Effective gaussian 2*sigma measured in points
--
-- Output: y(*) - Smoothed array                                 REAL ARRAY
--
-- Notes:  Smooths an array Y of length NPT, with a window whose full width is
--         of order of PST neighboring points, a user supplied value.
*********************************************************************** */
BOOL FFTGaussSmooth(REAL *yp, INT npt, INT nptmax, REAL pts) {

	INT i,j,m;
	REAL a, b, fac, sigma;
	REAL *y;

	/* Verify the parameters */
	if (pts <= 0) return TRUE;								/* Well, that was easy */
	pts /= 1.732f;												/* Gives sames smoothing as the FFT_SMOOTH above for same pts */
	
/* ... Determine size of FFT to use.  Must be data size + n*window but < max, 
   ... n chosen so end does not influence the beginning */
	m = fft_power_2(npt+10*nint(pts), GVI_MAX_LENGTH);		/* Needed size */
	if (m <= nptmax) {
		y = yp;
	} else {
		y = malloc(m*sizeof(*y));
		memcpy(y, yp, npt*sizeof(*y));
	}

/* ... Remove the linear trend from the data and zero pad to fill buffer */
	a = y[0];
	b = (y[npt-1]-y[0])/(npt-1);
	for (i=0; i<npt; i++) y[i] = y[i]-(a+b*i);	/* Remove linear trend */
	while (i<m) y[i++] = 0;								/* And zero pad the rest */

/* ... Perform the real FFT */
	fft_real(y,m,+1);										/* Forward FFT */

	sigma = (REAL) (1.0*npt)/pts;						/* Sigma of the gaussian */
	fac = 1;													/* Start with factor = 1 */
	for (i=1; i<m/2; i++) {								/* Filter the transform */
		j = i+m/2;											/* Posn of imaginary part */
		if (fac != 0) {
			fac = (REAL) pow(i/sigma,2);
			fac = (REAL) ((fac<69) ? exp(-fac) : 0); /* When hits zero, just use zero on out */
			y[i] *= fac;									/* Multiply by a Gaussian */
			y[j] *= fac;
		} else {
			y[i] *= fac;									/* Zero out high frequencies */
			y[j] *= fac;
		}
	}
	y[m/2] = 0;												/* Highest aliased point */
	fft_real(y,m,-1);										/* Inverse the FFT */

	for (i=0; i<npt; i++) y[i] = y[i]+(a+b*i);	/* Replace linear trend */

/* If we had to allocate work space, clean up now */
	if (yp != y) {
		memcpy(yp, y, npt*sizeof(*y));
		free(y);
	}

	return(TRUE);
}


/* ============================================================================
-- Subroutine to filter a function in the frequency domain through an
-- expression or set of filter constants.
--
-- Usage: call filter_(x,y,npt,nptmax,exp)
--
-- Inputs: x      - X array (for frequency)              REAL*4 (0:npt-1)
--         y      - Array of Y values (filtered)         REAL*4 (0:npt-1)
--         npt    - Number of points                     INTEGER
--         nptmax - Maximum valid # of points            INTEGER
--         exp    - Arbitrary filter expression          CHARACTER*(*)
--                  Must resolve to real # at each frequency f$.
--
-- Output: Y   - filtered time domain spectra.  Each point in the transform is
--               multiplied by the corresponding real filter value at that 
--               frequency and the function re-inverted.
============================================================================ */
BOOL filter(CURVE *cv, char *cvname, char *expr) {

	REAL *x=cv->x, *y=cv->y;

	int errcnt, err;
	INT i, npt;
	REAL omega,freq,tmp;
	GVCMDS *cmds;

	if (! GVLinkReal("f$", GVF_USER, &freq)) {
		ERRprintf("ERROR: Unable to link frequency variable F$ (huh?)\n");
		return(FALSE);
	} else if ( (cmds=GVParse(expr, NULL)) == NULL) {
		ERRprintf("ERROR: Filter expression \"%s\" does not parse\n",expr);
		return(FALSE);
	}

	npt = cv_power_2(cv, cvname, TRUE);			/* How many powers of 2? */
	x = cv->x; y = cv->y;							/* Get valid pointers */

	if (cv->npt < npt) {								/* Expansion - have to pad */
		for (i=cv->npt; i<npt; i++) y[i] = y[cv->npt-1];
	} else {												/* Possible truncation */
		cv->npt = npt;
	}

	omega = 1/(npt*(x[1]-x[0]));					/* Frequency spacing */
	fft_real(y,npt,+1);								/* Forward transform */

	errcnt = 0;
	freq = 0;
	y[npt/2] = 0;										/* Arbitrarily toss NYQUIST */
	for (i=0; i<npt/2; i++) {
		freq = i*omega;
		tmp = GVTrimToReal(GVEvalCmdsI(cmds, i, &err));
		if (err != 0) {
			if (errcnt++ == 0) gen_warn("Illegal expressions are set to 0");
			tmp = 0;
		}
		y[i] *= tmp;
		y[i+npt/2] *= tmp;
	}

	fft_real(y, npt, -1);							/* Reverse the transform */
	GVDeallocate("f$");
	return(TRUE);
}


/* ============================================================================
-- Function to return the nearest power of 2 greater than or equal to npt but
-- less than nptmax for a curve.  If possible, the size of the curve will be
-- increased so that the returned value is greater than NPT rather than less.
--
-- Usage:  INTEGER = cv_power_2(CURVE *cv, char *cvname, BOOL ReLink)
--
-- Inputs: cv     - pointer to curve structure
--         cvname - name of the curve (or NULL if not known)
--         relink - if true, GPTLinkXYZ will be called with curve if modified
--
-- Output: May change defined size of the curve cv
--
-- Return: power_2 - 2**M where M is an integer.  power_2 will be such that
--                      cv->npt <= power_2 <= cv->nptmax.
--
-- Notes: Since designed for FFT, POWER_2 will try to return at least 8.  Do
--        not call with NPTMAX < 4 if you know what's good for you.
============================================================================ */
static int cv_power_2(CURVE *cv, char *cvname, BOOL ReLink) {

	int rcode=8;										/* Initial guess */

	while (rcode < cv->npt) rcode = 2*rcode;	/* But less than NPTMAX */

/* Try to resize if I have a name as well as curve pointer */
	if (rcode > cv->nptmax) {						/* Try to resize */
		if (cvname != NULL && GVResize(cvname, rcode)) {
			if (ReLink) GptLinkXYZ(cvname);
		} else {
			rcode /= 2;
		}
	}

	return(rcode);
}
