/* gptmatrx.c */

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
#include <string.h>
#include <signal.h>
#include <math.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#define	LEXP_EXTENSIONS
#include "lexp.h"
#include "tplot.h"
#include "gptxtrn.h"
#include "helper.h"

#include "fft2c.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */
void Smooth_3D_Surface(SURFACE *s1);
void Rotate_3D_Matrix(SURFACE *s1, char *sname, double angle, BOOL FixedSize);
void Rotate_3D_Surface(SURFACE *s1, char *sname, double angle, BOOL FixedSize);
int  Contour_3D_Surface(SURFACE *s1, double zt, int flags);
int  Centroid_3D_Contour_Surface(SURFACE *s1, double zt, int flags);

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static CURVE *FindCurve(char *name, int need3d);
static int  FindIndex(double x, REAL *ar, int npt);
static int  Histogram(SURFACE *source);
static int  Mark(SURFACE *source);
static void Denoise(SURFACE *src, REAL mark, REAL zero);
static int polyfit(REAL *x, REAL *y, int npt, int order, REAL *cf);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Function to "window" surface for the power spectrum determination
-- Multiplies the data buffer by either a square, Parzen or Welch window in
-- both directions to estimate power spectra.  Required normalization is
-- returned by the function.
--
-- Usage:  REAL*4 = window_p(type,z, nrow,ncol)
--
-- Inputs: type  - type of window.  1 => square window
--		                              2 => Parzen window
--				                        3 => Welch window
--         z         - data array
--         nrow,ncol - size of the array.  Data stored in column format
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
static double window_p(int type, REAL *z, int nrow, int ncol) {

	int i,j;
	double sumx, sumy, tmp;

	if (type == 2 || type == 3) {
		sumx = sumy = 0;
		for (i=0; i<nrow; i++) {									/* First do the columns - row by row */
			tmp = (2*i-(nrow-1))/(nrow+1.0);						/* [-1,1] fraction along the rows */
			tmp = (type == 2) ? 1-fabs(tmp) : 1-tmp*tmp ;
			for (j=0; j<ncol; j++) z[j*nrow+i] *= (REAL) tmp;
			sumx += tmp*tmp;
		}
		for (i=0; i<ncol; i++) {									/* And now the columns, row by row */
			tmp = (2*i-(ncol-1))/(ncol+1.0);			
			tmp = (type == 2) ? 1-fabs(tmp) : 1-tmp*tmp ;
			for (j=0; j<nrow; j++) z[j+i*nrow] *= (REAL) tmp;
			sumy += tmp*tmp;
		}
	} else {
		sumx = nrow; sumy = ncol;
	}
	return( 1/(nrow*ncol*sumx*sumy) );
}

/* ============================================================================
-- Routine to handle specific transformations of the data.  May be modified
-- by user if desired.
--
-- Usage: gptmatrx(void)
--
-- Inputs: none (all passed as globals)
--
-- Output: possible modification/creation of function evaluator objects
--
-- Notes: This routine implements matrix transforms needed.
============================================================================ */
typedef enum _OPS1 {
	MA_LIST,									/* List the available commands */
	MA_ABORT,								/* Abort from the process */
	MA_TRANSPOSE,							/* Transpose the matrix   */
	MA_DUPLICATE,							/* Duplicate a matrix */
	MA_SUBMATRIX_GET,						/* Extract a sub-matrix */
	MA_XYSUBMATRIX,						/* Extract a sub-matrix based on X/Y range */
	MA_SUBMATRIX_PUT,						/* Put a sub-matrix into existing matrix */
	MA_ROW_EXTRACT,						/* Extract a row (or average of rows) */
	MA_COL_EXTRACT,						/* Extract a column (or average of cols) */
	MA_ROW_PUT,								/* Put a row back into the matrix (single) */
	MA_COL_PUT,								/* Put a column back into the matrix (single) */
	MA_INSERT_POINT,						/* Insert a point at given row/column */
	MA_INSERT_DATA,						/* Insert data as row/column starting at arbitrary point */
	MA_EXTRACT_DATA,						/* Extract data as row/column starting at arbitrary point */
	MA_ROW_PEAK,							/* Find the peak value of each row in matrix */
	MA_COL_PEAK,							/* Find the peak value of each col in matrix */
	MA_TO_CURVE,							/* Convert a surface to a curve */
	MA_TO_SURFACE,							/* Convert a curve to a surface */
	MA_HISTOGRAM,							/* Histogram the Z values of the surface */
	MA_SMOOTH,								/* Biaxially smooth a surface */
	MA_ROTATE,								/* Rotate surface by specified angle */
	MA_SPIN,									/* Spin surface by specified angle */
	MA_CONTOUR,								/* Return contour points from surface */
	MA_CENTROID,							/* Return centroid of area above value */
	MA_PEAK_DETECT,						/* Locate peaks within the data set - return as curve */
	MA_MARK,									/* General thresholding routine */
	MA_THRESHOLD,							/* Threshold the data */
	MA_WINDOW,								/* Threshold in a window */
	MA_ISOLATE,								/* Isolate regions */						
	MA_FLATTEN,								/* Flatten a surface by removing all but constant offset */
	MA_UNCURL,								/* "Uncurl" a surface in X or Y using polynomial fit */
	MA_FFT,									/* Return the 2D FFT power spectrum */
	MA_S_OF_Q,								/* Return the S(Q) averaged from 2D FFT */
	MA_RDF,									/* Radial distribution function */
} OPS1;
#define	NEED_SOURCE			0x01

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPS1 rcode;
	int  options;
} CMTYPE;

PRIVATE const CMTYPE cmlist[] = {
			{"LIST",				 2,	MA_LIST,				0},
			{"/",					 1,	MA_LIST,				0},
			{"ABORT",			 5,	MA_ABORT,			0},
			{"?",					-1,	MA_LIST,				0},
			{"TRANSPOSE",		 4,	MA_TRANSPOSE,		NEED_SOURCE},
			{"DUPLICATE",		 3,	MA_DUPLICATE,		NEED_SOURCE},
			{"COPY",				-4,	MA_DUPLICATE,		NEED_SOURCE},
			{"EXTRACT", 		 7,   MA_SUBMATRIX_GET,	NEED_SOURCE},
			{"GET_SUBMATRIX",  6,	MA_SUBMATRIX_GET,	NEED_SOURCE},
			{"SUBMATRIX",		-4,   MA_SUBMATRIX_GET,	NEED_SOURCE},
			{"SUBMATRIX_GET",	-10,	MA_SUBMATRIX_GET,	NEED_SOURCE},
			{"PUT_SUBMATRIX",  6,	MA_SUBMATRIX_PUT,	NEED_SOURCE},
			{"SUBMATRIX_PUT",	-10,	MA_SUBMATRIX_PUT,	NEED_SOURCE},
			{"XY_EXTRACT",		 5,	MA_XYSUBMATRIX,	NEED_SOURCE},
			{"ROW_GET",			 3,   MA_ROW_EXTRACT,	NEED_SOURCE},
			{"GET_ROW",			-5,   MA_ROW_EXTRACT,	NEED_SOURCE},
			{"COL_GET",			 3,	MA_COL_EXTRACT,	NEED_SOURCE},
			{"GET_COL",			-5,	MA_COL_EXTRACT,	NEED_SOURCE},
			{"ROW_PUT",			 5,	MA_ROW_PUT,			NEED_SOURCE},
			{"PUT_ROW",			 5,	MA_ROW_PUT,			NEED_SOURCE},
			{"COL_PUT",			 5,	MA_COL_PUT,			NEED_SOURCE},
			{"PUT_COL",			 5,	MA_COL_PUT,			NEED_SOURCE},
			{"COL_PEAK",		 5,	MA_COL_PEAK,		NEED_SOURCE},
			{"ROW_PEAK",		 5,	MA_ROW_PEAK,		NEED_SOURCE},
			{"SET_POINT",		 3,	MA_INSERT_POINT,	NEED_SOURCE},
			{"INSERT_DATA",	 5,	MA_INSERT_DATA,	NEED_SOURCE},
			{"EXTRACT_DATA",	 6,	MA_EXTRACT_DATA,	NEED_SOURCE},

			{"X_SCAN",			-3,   MA_ROW_EXTRACT,	NEED_SOURCE},
			{"Y_SCAN",			-3,   MA_COL_EXTRACT,	NEED_SOURCE},
			{"X_PUT",			-5,	MA_ROW_PUT,			NEED_SOURCE},
			{"Y_PUT",			-5,	MA_COL_PUT,			NEED_SOURCE},
			{"ROW_EXTRACT",	-5,   MA_ROW_EXTRACT,	NEED_SOURCE},
			{"COL_EXTRACT",	-5,	MA_COL_EXTRACT,	NEED_SOURCE},
			{"COLUMN_EXTRACT",-6,	MA_COL_EXTRACT,	NEED_SOURCE},
			{"COLUMN_PUT",	   -8,	MA_COL_PUT,			NEED_SOURCE},
			{"COLUMN_PEAK",	-8,	MA_COL_PEAK,		NEED_SOURCE},

			{"TO_CURVE",		 4,	MA_TO_CURVE,		NEED_SOURCE},
			{"TO_SURFACE",		 4,	MA_TO_SURFACE,		0},

			{"HISTOGRAM",		 4,	MA_HISTOGRAM,		0},		/* Gets it itself */
			{"MARK",				 4,	MA_MARK,				0},		/* Gets it itself */

			{"ROTATE",			 3,	MA_ROTATE,			NEED_SOURCE},
			{"SPIN",				 3,	MA_SPIN,				NEED_SOURCE},
			{"SMOOTH",			 3,	MA_SMOOTH,			NEED_SOURCE},
			{"CONTOUR",			 3,	MA_CONTOUR,			NEED_SOURCE},
			{"CENTROID",		 3,	MA_CENTROID,		NEED_SOURCE},
			{"PEAK_DETECT",	 4,	MA_PEAK_DETECT,	NEED_SOURCE},
			{"FIND_PEAKS",		 6,	MA_PEAK_DETECT,	NEED_SOURCE},
			{"THRESHOLD",		 6,   MA_THRESHOLD,		NEED_SOURCE},
			{"WINDOW",			 6,   MA_WINDOW,			NEED_SOURCE},
			{"ISOLATE",			 7,	MA_ISOLATE,			NEED_SOURCE},
			{"FLATTEN",			 4,	MA_FLATTEN,			NEED_SOURCE},
			{"UNCURL",			 6,	MA_UNCURL,			NEED_SOURCE},
			{"FFT",				 3,	MA_FFT,				NEED_SOURCE},
			{"S(Q)",				 4,	MA_S_OF_Q,			NEED_SOURCE},
			{"RDF",				 3,	MA_RDF,				NEED_SOURCE},

			{NULL,				 0,	MA_LIST,				0} };

			static char MatrixHelp[] = 
"\n"
" Commands to manipulate matrices (surfaces) efficiently.\n"
"\n"
"   MATRIX <command> <args> [-opts]\n"
"\n"
" Some of the commands are duplicated as TRANSFORM commands also.  They are here\n"
" as the primary location - MATRIX is essentially TRANSFORM on surfaces.\n"
"\n"
"Column and row indicies begin counting at 1 (unlike arrays which start at zero).\n"
"In commands taking a range, the range must be specified as comma delimited pair\n"
"with no spaces in between values.\n"
"\n"
" <commands>:\n"
"   LIst | ?                    - Lists all of the available commands (internal)\n"
"   ABORT                       - Cancel without doing anything\n"
"   DUPlicate <old> <new>       - Duplicates the surface <old> to identical <new> copy\n"
"   COPY <old> <new>            - Synonymous with duplicate\n"
"   EXTRACT <old> <new> <r1,r2> <c1,c2> - Create a duplicate containing only rows [r1,r2 and columns [c1,c2]\n"
"     SUBMatrix                 - synonym\n"
"     SUBMATRIX_Get             - synonym\n"
"     GET_SUBmatrix             - synonym\n"
"   XY_EXTRACT <old><new> <xl,xh> <yl,yh> - Extract submatrix within specified x,y bands\n"
"   PUT_SUBmatrix <src> <dest> <row> <col> - Insert submatrix at specified point in dest\n"
"     SUBMATRIX_Put             - synonym\n"
"   TRANspose <surf>            - Transposes the matrix (in place)\n"
"\n"
"   ROW_get <surf> <irow>       - extracts single row into main curve\n"
"   ROW_get <surf> <r1,r2>      - extracts average of rows r1-r2 (inclusive)\n"
"     X_Scan                    - synonym\n"
"     GET_Row                   - synonym\n"
"   ROW_Put <surf> <irow>       - Put current curve into row <irow>\n"
"     X_PUT                     - synonym\n"
"\n"
"   COL_get <surf> <icol>       - extracts single column into main curve\n"
"   COL_get <surf> <c1,c2>      - extracts average of columns c1-c2 (inclusive)\n"
"     Y_Scan                    - synonym\n"
"     GET_Col                   - synonym\n"
"   COL_Put <surf> <icol>       - Put current curve into column <icol>\n"
"     Y_PUT                     - synonym\n"
"\n"
"   SET_POINT <surf> <irow> <icol> <zval> - sets matrix element to specified value\n"
"   INSERT_DATA <surf> <irow> <icol> [ROW | COLUMN | X | Y] - Inserts data at arbitrary point\n"
"                               - Inserts at specified point with no range checking.\n"
"   EXTRACT_DATA <surf> <irow> <icol> [ROW | COLUMN | X | Y] - Extract data from arbitrary point\n"
"                               - Starts at given point, extracts NPT points to X,Y\n"
"\n"
"   COL_Peak <surf>             - Returns the maximum value in each column of the matrix\n"
"   ROW_Peak <surf>             - Returns the maximum value in each row of the matrix\n"
"\n"
"   TO_Curve <curf>             - Converts 2D surface into a 3D curve as individual\n"
"                                 points (nrow*ncol total), column by column.  X and Y\n"
"                                 are properly set for each value.  Must be in 3D mode.\n"
"   TO_Surface <cv> <surf>      - Attempts to convert points in a 3D curve into an\n"
"                                 equivalent surface.  Column or row orientation, and\n"
"                                 size of matrix (nrow,ncol) are determined by looking\n"
"                                 for identical X and Y values at beginning.  Will not\n"
"                                 proceed if values are inconsistent (ie. npt=ncol*nrow).\n"
"\n"
"   ROTATE <surf> <deg> [-all]  - rotates specified surface by clockwise angle\n"
"                                 the -all/-full option increases size to keep all data\n"
"                                 Different X,Y range properly handled\n"
"   SPIN <surf> <deg> [-all]    - spin (rotate) specified surface by clockwise angle\n"
"                                 the -all/-full option increases size to keep all data\n"
"                                 In contrast to ROTATE, rows/columns assumed equally spaced\n"
"\n"
"   SMOOTH <surf>               - biaxial smoothing applied to specified surface\n"
"   CONTOUR <surf> <z> [-sort]  - Returns curve corresponding to contour points (crossings)\n"
"                                 specified value.  If the -sort option is not given, returns\n"
"                                 just unsorted list of points on grid where surface would\n"
"                                 have the given value.  Sorting attempts to arrange points\n"
"                                 as curves forming closed contours.  Sorting can take\n"
"                                 considerable time if many (>100,000) crossings occur.\n"
"   CENTROID <surf> <z> [-silent] - Returns an estimate of the centroid position above given\n"
"                     [-CM | -CP] - contour.  The default is based on area.  -CM weights each\n"
"                                   pixel by the function value estimating the center of mass.\n"
"                                   -CP finds the X/Y with equal integrals above/below and\n"
"                                   left/right.  Equivalent to a 4-quadrant detector\n"
"   HISTOGRAM <surf>             - Histogram of surface.  Use \"MATRIX HIST -?\" for more help\n"
"   RDF <surf> [-options]        - Radial distribution function of surface (based on x,y values)\n"
"         [-CENter <x0> <y0>]             - Origin of the image (default = 0,0)\n"
"         [-BIN <dx>]                     - Size of bins for RDF (default based on image)\n"
"   MARK <surf> <options>        - General thresholding.  Use \"MATRIX MARK <s1> -?\" for help\n"
"\n"
"   PEAK_DETECT <surf>           - Returns curve with all points which are > than 8 neighbors\n"
"   FIND_PEAKS <surf>            - synonymous with PEAK_DETECT\n"
"   THRESHOLD <surf> <zv>        - Threshold data.  Return 1 if z>=zt, 0 otherwise\n"
"   WINDOW <surf> <z_l> <z_h>    - Threshold data.  Return 1 if zl<=z<=zh, 0 otherwise\n"
"   ISOLATE <surf> <zv> <cnt>    - Threshold with requirement of >=cnt along each row/col\n"
"   FLATTEN <surf>               - Fit plane and remove slopes (leave average)\n"
"   UNCURL <surf> <X|Y> <order>  - Fit X or Y to nth order polynomial for uncurling surface\n"
"   FFT <surf> <dest> [-options] - 2D FFT on the matrix (must be 2^n format)\n"
"            [-PSD]                                   - Output in spectral density\n"
"            [-power | -magn | -real | -imag | -dB]   - Which output returned\n"
"            [-square | -welch | -parzen]             - Windowing options\n"
"   S(Q) <surf>                   - Converts a 2D FFT into main curve S(Q)\n"
"                                   Averages for radial symmetry of pattern\n"
"\n"
" Examples: matrix rotate s1 20\n"
"           matrix col_get\n"
"           matrix fft s_data s_power -power\n"
"           matrix S(Q) s_power\n"
;

int gpt_do_matrix(void) {

	CMTYPE *citem;
	LOGICAL	AlreadyGivenList, ColumnMajor;
	SURFACE *source, *dest;
	CURVE   *cv;
	void **varptr;
	int i,j, type,ilow,ihigh,imax, idx, npt, ierr, nrow,ncol, icol,irow, flags;

	int r1,r2,c1,c2;
	char list[DFLT_STR_SIZE], token[DFLT_STR_SIZE], *tokptr, *prompt;
	char SourceName[DFLT_STR_SIZE], DestName[DFLT_STR_SIZE];			/* Actual name of source/dest matrix vars */
	double angle, zl,zh, zt, omega, correct, psd_scale;
	REAL xtmp, *x, *y, *z, *ztmp;

	REAL *rmat, *imat, *rdata, *idata;										/* For the FFT */
	BOOL output_PSD;
	enum {DO_SQUARE=1, DO_PARZEN=2, DO_WELCH=3} filter_mode;			/* Must match p_window */
	enum {DO_POWER, DO_MAGN, DO_DB, DO_REAL, DO_IMAG} output_mode;

	int ipos, ilen;																/* For S(Q) */
	double posn;

	BOOL mode;
	int rcode;

/* First, look for a help request */
	if (LexCheckHelp("Matrix", MatrixHelp, NULL)) return(OKAY);

/* Look for a command to execute - give it two tries only */
	AlreadyGivenList = FALSE;
	prompt = "MATRIX command: (list): ";
	while (TRUE) {										/* Loop forever */
		if (! LexGetTokenP(token, sizeof(token), prompt)) {
			if (AlreadyGivenList) return(OKAY);
			strcpy(token, "list");
		}
		if (LexEscape(TRUE)) return(OKAY);

		if ( (citem = LexCmdl(token, cmlist, sizeof(CMTYPE))) == NULL) {
			ERRprintf("ERROR: %s in not a recognized MATRIX command\n", token);
			return(NOMORE);
		}

		if (citem->rcode == MA_LIST) {
			LexCmdlPrint(cmlist, sizeof(CMTYPE), "\nRecognized MATRIX keywords:");
			TTYputc('\n');
			AlreadyGivenList = TRUE;					/* Change action */
			prompt = "MATRIX command: (ABORT): ";	/* On default	  */
		} else if (citem->rcode == MA_ABORT) {
			return(OKAY);
		} else {
			break;
		}
	}

/* If a source SURFACE is required, ensure it is available at this point */
	if (citem->options & NEED_SOURCE) {
		if ( (source = GptSurface) != NULL) {				/* Don't think this will ever be true */
			strcpy(SourceName, "<unknown>");
		} else {
			if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface (abort): ")) return(OKAY);
			if (LexEscape(TRUE) || *SourceName == '/') return(OKAY);
			if (! GVGetInfo(SourceName, &type, (void **) &varptr)) {
				ERRprintf("ERROR: Source surface must exist.  %s was not found\n", SourceName);
				return(NOMORE);
			} else if (type != GV_SURFACE) {
				ERRprintf("ERROR: %s is not a surface variable.\n", SourceName);
				return(NOMORE);
			}
			source = (SURFACE *) *varptr;
		}
	}

/* Okay, now start to process */
	rcode  = UNIMPLEMENTED;							/* Assume failure */
	switch (citem->rcode) {

		case MA_LIST:
		case MA_ABORT:
			ERRprintf("ERROR in gpt_do_matrix: Not expecting this switch (%d)\n", citem->rcode);
			break;
		case MA_TRANSPOSE:
			npt = source->nrow*source->ncol*sizeof(*ztmp);
			ztmp = z = malloc(npt);
			for (i=0; i<source->nrow; i++) {
				for (j=0; j<source->ncol; j++) *ztmp++ = source->z[i+j*source->nrow];
			}
			memcpy(source->z, z, npt);
			free(z);

			/* Finally have to relink the X and Y arrays */
			z = source->x; source->x = source->y; source->y = z;
			i = source->nrow; source->nrow = source->ncol; source->ncol = i;
			i = source->nrowmax; source->nrowmax = source->ncolmax; source->ncolmax = i;
			GVRelinkSurface(SourceName, source);
			rcode = OKAY; break;

		case MA_FFT:
			if (source->nrow != fft_power_2(source->nrow, source->nrow) || source->ncol != fft_power_2(source->ncol, source->ncol)) {
				ERRprintf("ERROR: FFT is only possible for data that has row/col = 2^n\n");
				return(NOMORE);
			}
			if (! LexGetTokenP(DestName, sizeof(DestName), "Surface for FFT (abort): ")) return(OKAY);
			if (LexEscape(TRUE) || *DestName == '/') return(OKAY);
			if (! GVAllocSurface(DestName, GVF_USER, source->nrow, source->ncol)) {
				ERRprintf("ERROR: Unable to allocate new surface as %s\n", DestName);
				return(NOMORE);
			} else if (! GVGetInfo(DestName, &type, (void **) &varptr) || (type != GV_SURFACE)) {
				ERRprintf("ERROR: Surface variable %s was not allocated\n", DestName);
				return(NOMORE);
			}
			dest = (SURFACE *) *varptr;

			/* Scan for options */
			output_mode = DO_POWER;
			filter_mode = DO_SQUARE;
			output_PSD  = FALSE;
			while (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-dB", 3)) {
					output_mode = DO_DB;
				} else if (LexEqual(token, "-PSD", 4)) {
					output_PSD = TRUE;
				} else if (LexEqual(token, "-power", 4)) {
					output_mode = DO_POWER;
				} else if (LexEqual(token, "-magnitude", 5) || LexEqual(token, "-amplitude", 5)) {
					output_mode = DO_MAGN;
				} else if (LexEqual(token, "-real", 5)) {
					output_mode = DO_REAL;
				} else if (LexEqual(token, "-complex", 5) || LexEqual(token, "-imaginary", 5)) {
					output_mode = DO_IMAG;
				} else if (LexEqual(token, "-parzen", 4)) {
					filter_mode = DO_PARZEN;
				} else if (LexEqual(token, "-welch", 4)) {
					filter_mode = DO_WELCH;
				} else if (LexEqual(token, "-square", 4)) {
					filter_mode = DO_SQUARE;
				} else {
					ERRprintf("ERROR: Unrecognized option ignored (%s)\n", token);
				}
			}

			/* Initial setups */
			ncol = source->ncol;																/* Make local copy */
			nrow = source->nrow;
			memset(dest->z, 0, source->nrow*source->ncol*sizeof(*source->z));
			memcpy(dest->ids, source->ids, sizeof(source->ids));
			memcpy(dest->options, source->options, sizeof(source->options));

			/* Set the spatial frequency immediately - easy */
			omega = 1.0/(source->x[ncol-1]-source->x[0]);						/* Frequency spacing */
			for (i=0; i<ncol; i++) dest->x[i] = (float) ((i-ncol/2.0)*omega);
			omega = 1.0/(source->y[nrow-1]-source->y[0]);						/* Frequency spacing */
			for (i=0; i<nrow; i++) dest->y[i] = (float) ((i-nrow/2.0)*omega);
			psd_scale = (source->y[nrow-1]-source->y[0])*(source->x[ncol-1]-source->x[0]);

			/* Start the 2D FFT - use fully symmetric formalism */
			/* Z order is columns, each of nrow elements, with ncol columns. */
			/* Do the ncol FFT's first, filling in the data */
			rmat = calloc(ncol*nrow, sizeof(REAL));									/* Buffers for full complex FFT data */
			imat = calloc(ncol*nrow, sizeof(REAL));
			for (i=0; i<nrow*ncol; i++) rmat[i] = source->z[i];

			/* Window the data to clean up edge and normalize.  Must do both directions */
			correct = window_p(filter_mode, rmat, nrow, ncol);

			/* Do the first FFTs on columns - in place */
			for (i=0; i<ncol; i++) {
				fft2c(rmat+i*nrow, imat+i*nrow, nrow, 1);	/* Forward FFT of each */
			}

			/* On the second FFT, also roll so that zero frequency is at center+: y[512] in a 1024 array */
			rdata = malloc(sizeof(REAL)*ncol);											/* For second direction FFT */
			idata = malloc(sizeof(REAL)*ncol);
			if (output_PSD) correct *= psd_scale;										/* Scale by cell area */
			for (i=0; i<nrow; i++) {
				for (j=0; j<ncol; j++) { rdata[j] = rmat[((i+ncol/2)%ncol)+j*nrow]; idata[j] = imat[((i+ncol/2)%ncol)+j*nrow]; }
				fft2c(rdata, idata, ncol, +1);											/* Invert the other direction */
				for (j=0; j<ncol; j++) {													/* And put into the results */
					zt = correct*(pow(rdata[j],2)+pow(idata[j],2));					/* Default power */
					switch (output_mode) {
						case DO_MAGN:
							zt = sqrt(zt); break;
						case DO_DB:
							zt = 10*log(zt)/log(10.0); break;
						case DO_REAL:
							zt = sqrt(correct)*rdata[j]; break;
						case DO_IMAG:
							zt = sqrt(correct)*idata[j]; break;
						case DO_POWER:
						default:
							break;
					}
					dest->z[i+((j+ncol/2)%ncol)*nrow] = (REAL) zt;
				}
			}

			free(rdata); free(idata); free(rmat); free(imat);
			rcode = OKAY; break;

		case MA_S_OF_Q:								/* Just assume it is from an FFT - but don't force */
			nrow = source->nrow; ncol = source->ncol;
			z = source->z;
			ilen = min(nrow/2, ncol/2);

			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {		/* Where to save data */
				return(NOMORE);
			} else if (cv->nptmax < ilen) {
				GVResize(GptMainCurve, ilen);
				cv = FindCurve(GptMainCurve, FALSE);
			}
			cv->npt = ilen;
			for (i=0; i<ilen; i++) { cv->x[i] = source->x[ncol/2+i]; cv->y[i] = 0; }

			for (icol=ncol/2-ilen; icol<ncol/2+ilen; icol++) {
				for (irow=nrow/2-ilen; irow<nrow/2+ilen; irow++) {
					posn = sqrt(pow(icol-ncol/2,2) + pow(irow-nrow/2,2));
					if (posn >= ilen-1) continue;
					ipos = (int) posn;
					cv->y[ipos]   += (REAL) (source->z[icol*nrow+irow]*(1.0-(posn-ipos)));
					cv->y[ipos+1] += (REAL) (source->z[icol*nrow+irow]*(posn-ipos));
				}
			}
			for (i=1; i<ilen; i++) cv->y[i] /= (REAL) (2*3.141592654*i);
			rcode = OKAY; break;

		case MA_RDF:									/* Radial distribution function */
		{	int *tmp;
			double rmax;
			double x0=0, y0=0, dx=0.0;

			while (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-center", 4)) {
					x0 = LexGetReal(0.0f, "X center of image (0.0): ");
					y0 = LexGetReal(0.0f, "Y center of image (0.0): ");
				} else if (LexEqual(token, "-bin", 4) || LexEqual(token, "-dx", 3)) {
					dx = LexGetReal(1, "Bin spacing (1.0): ");
				} else {
					ERRprintf("ERROR: Unrecognized option ignored (%s)\n", token);
				}
			}

			nrow = source->nrow; ncol = source->ncol;
			x = source->x; y = source->y; z = source->z;
			if (nrow < 2 || ncol < 2) {
				ERRprintf("ERROR: RDF on an image this small makes no sense - sorry to refuse your request\n");
				rcode = NOMORE; break;
			}

			rmax = sqrt((x[0]-x0)*(x[0]-x0)+(y[0]-y0)*(y[0]-y0));
			rmax = max(rmax, sqrt((x[ncol-1]-x0)*(x[ncol-1]-x0)+(y[0]-y0)*(y[0]-y0)));
			rmax = max(rmax, sqrt((x[0]-x0)*(x[0]-x0)          +(y[nrow-1]-y0)*(y[nrow-1]-y0)));
			rmax = max(rmax, sqrt((x[ncol-1]-x0)*(x[ncol-1]-x0)+(y[nrow-1]-y0)*(y[nrow-1]-y0)));
			if (dx <= 0) dx = max( fabs((x[ncol-1]-x[0])/(ncol-1)), fabs((y[nrow-1]-y[0])/(nrow-1)));
			ilen = (int) (rmax/dx+1);
			if (ilen > 1048576) {									/* Million points is already way too many! */
				ERRprintf("WARNING: Specified histogram spacing is ludicrous.  Reduced to only rediculous.\n");
				ilen = 1048576;
				dx = rmax/ilen;
			}

			/* Make sure we have a curve large enough to use */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {		/* Where to save data */
				return(NOMORE);
			} else if (cv->nptmax < ilen) {
				GVResize(GptMainCurve, ilen);
				cv = FindCurve(GptMainCurve, FALSE);
			}
			cv->npt = ilen;
			for (i=0; i<ilen; i++) { cv->x[i] = (REAL) (dx*(i+0.5)); cv->y[i] = 0.0;}
			tmp = calloc(ilen, sizeof(*tmp));

			/* Go through all of the data points */
			for (icol=0; icol<ncol; icol++) {
				for (irow=0; irow<nrow; irow++) {
					i = (int) (sqrt((x[icol]-x0)*(x[icol]-x0)+(y[irow]-y0)*(y[irow]-y0))/dx);
					if (i >= ilen) i = ilen-1;
					cv->y[i] += z[icol*nrow+irow];
					tmp[i]++;
				}
			}
			for (i=0; i<ilen; i++) if (tmp[i] != 0) cv->y[i] /= tmp[i];
			free(tmp);
			rcode = OKAY; break;
		}

		case MA_DUPLICATE:
			if (! LexGetTokenP(DestName, sizeof(DestName), "New surface name (abort): ")) return(OKAY);
			if (LexEscape(TRUE) || *DestName == '/') return(OKAY);
			if (! GVAllocSurface(DestName, GVF_USER, source->nrow, source->ncol)) {
				ERRprintf("ERROR: Unable to allocate new surface as %s\n", DestName);
				return(NOMORE);
			} else if (! GVGetInfo(DestName, &type, (void **) &varptr) || (type != GV_SURFACE)) {
				ERRprintf("ERROR: Surface variable %s was not allocated\n", DestName);
				return(NOMORE);
			}
			dest = (SURFACE *) *varptr;
			memcpy(dest->z, source->z, source->nrow*source->ncol*sizeof(*source->z));
			memcpy(dest->x, source->x, source->ncol*sizeof(*source->x));
			memcpy(dest->y, source->y, source->nrow*sizeof(*source->y));
			memcpy(dest->ids, source->ids, sizeof(source->ids));
			memcpy(dest->options, source->options, sizeof(source->options));
			rcode = OKAY; break;

		case MA_SUBMATRIX_GET:
		case MA_XYSUBMATRIX:
			if (! LexGetTokenP(DestName, sizeof(DestName), "New surface name (abort): ")) return(OKAY);
			if (LexEscape(TRUE) || *DestName == '/') return(OKAY);
			if (citem->rcode == MA_SUBMATRIX_GET) {
				r1 = LexGetInt(1, "First row to be included (1): ");
				r2 = LexGetInt(source->nrow, "Last row to be included (all): ");
				c1 = LexGetInt(1, "First column to be included (1): ");
				c2 = LexGetInt(source->ncol, "Last column to be included (all): ");
			} else {
				REAL x1,x2,y1,y2;
				x1 = LexGetReal(source->x[0], "Lower x limit (first column): ");
				x2 = LexGetReal(source->x[source->ncol-1], "Upper x limit (last column): ");
				y1 = LexGetReal(source->y[0], "Lower y limit (first row): ");
				y2 = LexGetReal(source->y[source->nrow-1], "Upper y limit (last row): ");
				c1 = FindIndex(x1, source->x, source->ncol);		c2 = FindIndex(x2, source->x, source->ncol);
				r1 = FindIndex(y1, source->y, source->nrow);		r2 = FindIndex(y2, source->y, source->nrow);
				if (r1 > r2) { 
					i = r1; r1 = r2; r2 = i;
					if (source->x[r2] != x1) r2++;
				} else {
					if (source->x[r2] != x2) r2++;
				}
				if (c1 > c2) { 
					i = c1; c1 = c2; c2 = i;
					if (source->y[c2] != y1) c2++;
				} else {
					if (source->y[c2] != y2) c2++;
				}
				r1++; r2++; c1++; c2++;										/* Change from index to row/col numbers */
			}
			r1 = max(1,r1); r2 = min(r2, source->nrow);
			c1 = max(1,c1); c2 = min(c2, source->ncol);
			if (r1 > r2 || c1 > c2) {
				ERRprintf("ERROR: Invalid range for rows or columns.  Nothing to be copied.\n");
				return(NOMORE);
			} else if (! GVAllocSurface(DestName, GVF_USER, r2-r1+1, c2-c1+1)) {
				ERRprintf("ERROR: Unable to allocate new surface as %s\n", DestName);
				return(NOMORE);
			} else if (! GVGetInfo(DestName, &type, (void **) &varptr) || (type != GV_SURFACE)) {
				ERRprintf("ERROR: Surface variable %s was not allocated\n", DestName);
				return(NOMORE);
			}
			c1--; c2--; r1--; r2--;						/* Convert from 1 based indexing to 0 based indexing */
			dest = (SURFACE *) *varptr;
			for (i=c1; i<=c2; i++) dest->x[i-c1] = source->x[i];
			for (i=r1; i<=r2; i++) dest->y[i-r1] = source->y[i];
			z = dest->z;
			for (i=c1; i<=c2; i++) {
				for (j=r1; j<=r2; j++) *(z++) = source->z[i*source->nrow+j];
			}
			memcpy(dest->ids, source->ids, sizeof(source->ids));
			memcpy(dest->options, source->options, sizeof(source->options));
			rcode = OKAY; break;

		case MA_SUBMATRIX_PUT:
			if (! LexGetTokenP(DestName, sizeof(DestName), "Destination matrix (abort): ")) return(OKAY);
			if (LexEscape(TRUE) || *DestName == '/') return(OKAY);
			if (! GVGetInfo(DestName, &type, (void **) &varptr) || (type != GV_SURFACE)) {
				ERRprintf("ERROR: Surface variable %s was not allocated\n", DestName);
				return(NOMORE);
			}
			dest = (SURFACE *) *varptr;

			r1 = LexGetInt(1, "Row to insert at (1): ");
			c1 = LexGetInt(1, "Column to insert at (1): ");
			if (r1 < 1 || r1 > dest->nrow || c1 < 1 || c1 > dest->ncol) {
				ERRprintf("ERROR: Specified location is outside bound of the destination matrix\n");
				return(NOMORE);
			}
			for (i=0; i<source->ncol; i++) {
				icol = c1+i-1;												/* Dest column (index not number) */
				if (icol >= dest->ncol) break;						/* We are done */
				for (j=0; j<source->nrow; j++) {
					irow = r1+j-1;											/* Dest row (index not number) */
					if (irow >= dest->nrow) break;
					dest->z[icol*dest->nrow+irow] = source->z[i*source->nrow+j];
				}
			}
			rcode = OKAY; break;

/* ---------------------------------------------------------------------------
-- Will take the current surface and create a X,Y,Z curve in the main space
-- consisting of connected points on the surface.  Scan will go down one
-- column before starting the next row, as stored in memory.
--------------------------------------------------------------------------- */
		case MA_TO_CURVE:
			npt = source->ncol*source->nrow;
			if ( (cv = FindCurve(GptMainCurve, TRUE)) == NULL) {
				return(NOMORE);
			} else if (cv->nptmax < npt) {
				GVResize(GptMainCurve, npt);
				cv = FindCurve(GptMainCurve, TRUE);
			}
			x = cv->x; y = cv->y; z = cv->z;
			if (z == NULL) {
				ERRprintf("ERROR: Must be in 3D mode to execute a TO_Curve command\n");
				rcode = NOMORE;
			} else {
				cv->npt = npt;
				for (i=0; i<source->ncol; i++) {
					for (j=0; j<source->nrow; j++) {
						*x++ = source->x[i];
						*y++ = source->y[j];
						*z++ = source->z[i*source->nrow+j];
					}
				}
				rcode = OKAY;
			}
			break;

/* ---------------------------------------------------------------------------
-- Will take the current surface and create a X,Y,Z curve in the main space
-- consisting of connected points on the surface.  Scan will go down one
-- column before starting the next row, as stored in memory.
--------------------------------------------------------------------------- */
		case MA_TO_SURFACE:
			if ( (dest = GptSurface) != NULL) {
				if (! LexGetTokenP(DestName, sizeof(DestName), "Curve to convert (use / for main): ")) return(OKAY);
				if (LexEscape(TRUE)) return(OKAY);
				if (*DestName == '/') strcpy(DestName, GptMainCurve);
				if ( (cv = FindCurve(DestName, TRUE)) == NULL) return(NOMORE);
			} else {
				cv = GptCurve;
			}
			x = cv->x; y = cv->y; z = cv->z;	npt = cv->npt;
			if (z == NULL) {
				ERRprintf("ERROR: Must be in 3D mode to execute a TO_Surface command\n");
				rcode = NOMORE;
				break;
			}

			if (npt > 1 && x[1] == x[0]) {
				ColumnMajor = TRUE;
				for (nrow=2; nrow<npt; nrow++) {if (x[nrow] != x[0]) break;}
				ncol = npt/nrow;
			} else if (npt > 1 && y[1] == y[0]) {
				ColumnMajor = FALSE;
				for (ncol=2; ncol<npt; ncol++) {if (y[ncol] != y[0]) break;}
				nrow = npt/ncol;
			} else {
				ERRprintf("ERROR: X/Y values do not repeat at beginning.  Is it really a matrix?\n");
				return(NOMORE);
			}
			if (npt != ncol*nrow) {
				ERRprintf("ERROR: Looks like a %d x %d matrix, but npt=%d != row*col\n", nrow, ncol, npt);
				return(NOMORE);
			}

			if (dest == NULL) {
				if (! LexGetTokenP(DestName, sizeof(DestName), "New surface name (abort): ")) return(OKAY);
				if (LexEscape(TRUE) || *DestName == '/') return(OKAY);
				if (! GVAllocSurface(DestName, GVF_USER, nrow, ncol)) {
					ERRprintf("ERROR: Unable to allocate new surface as %s\n", DestName);
					return(NOMORE);
				} else if (! GVGetInfo(DestName, &type, (void **) &varptr) || (type != GV_SURFACE)) {
					ERRprintf("ERROR: Surface variable %s was not allocated\n", DestName);
					return(NOMORE);
				}
				dest = (SURFACE *) *varptr;
			}
			
			if (ColumnMajor) {
				for (i=0; i<ncol; i++) dest->x[i] = x[i*nrow];
				for (i=0; i<nrow; i++) dest->y[i] = y[i];
				for (i=0; i<ncol; i++) {
					for (j=0; j<nrow; j++) dest->z[i*nrow+j] = z[i*nrow+j];
				}
			} else {
				for (i=0; i<ncol; i++) dest->x[i] = x[i];
				for (i=0; i<nrow; i++) dest->y[i] = y[i*ncol];
				for (i=0; i<ncol; i++) {
					for (j=0; j<nrow; j++) dest->z[i*nrow+j] = z[i+j*ncol];
				}
			}
			memcpy(dest->ids, cv->ids, sizeof(dest->ids));
			rcode = OKAY; break;

/* ---------------------------------------------------------------------------
-- Calls to external routines for smoothing, rotating and contours
--------------------------------------------------------------------------- */
		case MA_SMOOTH:								/* Biaxially smooth a surface */
			Smooth_3D_Surface(source);
			rcode = OKAY; break;

		case MA_ROTATE:								/* Rotate surface by specified angle */
		case MA_SPIN:
			angle = LexGetReal(0.0f, "Clockwise Rotation angle (degrees): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			mode = TRUE;
			if (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-all", 4) || LexEqual(token, "-full", 5)) {
					mode = FALSE;
				} else {
					ERRprintf("ERROR: Unrecognized option(%s)\n", token);
				}
			}
			if (citem->rcode == MA_ROTATE) {
				Rotate_3D_Surface(source, SourceName, angle, mode);
			} else {
				Rotate_3D_Matrix(source, SourceName, angle, mode);
			}
			rcode = OKAY; break;

		case MA_CONTOUR:								/* Return contour points from surface */
			zt = LexGetReal(0.0f, "Value to search (0.0): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}

			flags = 0;
			if (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-nosort", 4)) {
					flags = 0;
				} else if (LexEqual(token, "-sort", 2)) {
					flags |= 0x01;
				} else {
					ERRprintf("ERROR: Unrecognized option (%s)\n", token);
				}
			}
			rcode = (Contour_3D_Surface(source, zt, flags) < 0) ? NOMORE : OKAY;
			break;

		case MA_PEAK_DETECT:
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {
				return(NOMORE);
			} 
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			npt = 0;
			for (irow=1; irow<nrow-1; irow++) {
				for (icol=1; icol<ncol-1; icol++) {
					idx = irow+icol*nrow;									/* This point						*/
					if (z[idx] > z[idx     -1] &&                         z[idx] > z[idx     +1] &&
						 z[idx] > z[idx-nrow-1] && z[idx] > z[idx-nrow] && z[idx] > z[idx-nrow+1] &&
						 z[idx] > z[idx+nrow-1] && z[idx] > z[idx+nrow] && z[idx] > z[idx+nrow+1]) {
						double ax,bx,ay,by;
						if (npt >= cv->nptmax) {							/* Do I need more space?		*/
							GVResize(GptMainCurve, npt+2048);
							cv = FindCurve(GptMainCurve, FALSE);
						}
						bx = (z[idx+nrow]-z[idx-nrow]) / 2;				/* Fit z-z0 = ax^2+bx (x normalized -1 to 1)		  */
						ax = (z[idx-nrow]+z[idx+nrow]-2*z[idx]) / 2;	/* x = -b/2a												  */
						by = (z[idx+1   ]-z[idx-1   ]) / 2;				/* z(diff) = a(b^2/4a^2)-b^2/2a = b^2/4a^2-b^2/2a */
						ay = (z[idx-1   ]+z[idx+1   ]-2*z[idx]) / 2;	/*         = -b^2/4a  (must add both corrections) */
						cv->y[npt] = (REAL) (source->y[irow] - by/(2*ay) * (source->y[irow+1]-source->y[irow-1]) / 2);
						cv->x[npt] = (REAL) (source->x[icol] - bx/(2*ax) * (source->x[icol+1]-source->x[icol-1]) / 2);
						if (cv->z != NULL) cv->z[npt] = (REAL) (z[idx] - bx*bx/4/ax - by*by/4/ay);
						npt++;
					}
				}
			}
			cv->npt = npt;
			rcode = OKAY; break;

		case MA_THRESHOLD:
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			for (zt=0,i=0; i<ncol*nrow; i++) zt += z[i];
			zt /= ncol*nrow;
			zt = LexGetReal((REAL) zt, "Thresholding value (average): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			for (i=0; i<ncol*nrow; i++) z[i] = (REAL) ((z[i] >= zt) ? 1 : 0);
			rcode = OKAY; break;

		case MA_WINDOW:
			zl = LexGetReal(0.0f, "Lower threshold (0.0): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			zh = LexGetReal(1.0f, "Upper threshold (1.0): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			for (i=0; i<source->ncol*source->nrow; i++) source->z[i] = (REAL) ((z[i] >= zl && z[i] <= zh) ? 1 : 0);
			rcode = OKAY; break;

		case MA_FLATTEN:
		{
			REAL x0,y0,xmin,xmax,ymin,ymax;
			double s=0,sx=0,sy=0,sxx=0,syy=0,sxy=0;	/* Matrix elements */
			double b[3]={0,0,0};								/* Vector sum */
			double ar[3*4/2];									/* Real symmetric storage array */

			if (source->ncol*source->nrow < 4) {
				ERRprintf("ERROR: Need a surface of at least 4 points\n");
				rcode = NOMORE; break;
			}

			ncol = source->ncol;								/* Copy the variables over */
			nrow = source->nrow;
			npt  = ncol*nrow;
			x = source->x;										/* [0,ncol-1] */
			y = source->y;										/* [0,nrow-1] */
			z = source->z;										/* [0,nrow*ncol-1] by column then row */

				/* Flatten based on middle of range for x and y */
			ArrayMinMax(x, ncol, &xmin, &xmax);	x0 = (xmax+xmin)/2;
			ArrayMinMax(y, nrow, &ymin, &ymax); y0 = (ymax+ymin)/2;

/* ---------------------------------------------------------------------------
-- Sum the power elements for fitting
---------------------------------------------------------------------------- */
			for (i=0; i<ncol; i++) {
				for (j=0; j<nrow; j++) {
					s   += 1.0;
					sx  += (x[i]-x0);
					sy  += (y[j]-y0);
					sxx += (x[i]-x0)*(x[i]-x0);
					syy += (y[j]-y0)*(y[j]-y0);
					sxy += (x[i]-x0)*(y[j]-y0);

					b[0] += z[i*nrow+j]*(x[i]-x0);			/* Result vector */
					b[1] += z[i*nrow+j]*(y[j]-y0);
					b[2] += z[i*nrow+j];
				}
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
				rcode = NOMORE; break;
			}
			g_sppsl(ar, 3, b);								/* Result to b vector		*/
			TTYprintf(" Surface: z = %g (x-%g) + %g (y-%g) + %g\n", b[0],x0,b[1],y0,b[2]);

			for (i=0; i<ncol; i++) {
				for (j=0; j<nrow; j++) {
					z[i*nrow+j] -= (REAL) (b[0]*(x[i]-x0) + b[1]*(y[j]-y0));
				}
			}
		}
		rcode = OKAY; break;

		case MA_UNCURL:
		{
			enum {UNCURL_X, UNCURL_Y} mode = UNCURL_X;
			int nrow, ncol, npt, iorder;
			REAL *x, *y, *z, *f, tmp, cf[12];

			mode = LexChoice(TRUE, "X;ROW", "Y;COLUMN", "Uncurl in which direction (X|y): ") ? UNCURL_X : UNCURL_Y;
			if (LexEscape(TRUE)) { rcode = NOMORE; break; }
			iorder = LexGetInt(2, "Order of polynomial fit (2): ");
			if (LexEscape(TRUE)) { rcode = NOMORE; break; }
			if (iorder < 0 || iorder > 12) {
				ERRprintf("Order of fit for uncurl is limited to 0 <= order <= 12\n");
				rcode = NOMORE; break;
			}

			ncol = source->ncol;								/* Copy the variables over */
			nrow = source->nrow;
			npt  = ncol*nrow;
			x = source->x;										/* [0,ncol-1] */
			y = source->y;										/* [0,nrow-1] */
			z = source->z;										/* [0,nrow*ncol-1] by column then row */

			switch (mode) {
				case UNCURL_X:
					if (ncol < iorder+1) {
						ERRprintf("ERROR: Need a surface with at least iorder+1 rows\n");
						rcode = NOMORE; break;
					}
					f = calloc(ncol,sizeof(*f));
					for (i=0; i<ncol; i++) {
						for (j=0; j<nrow; j++) f[i] += z[i*nrow+j];
						f[i] /= nrow;													/* So average value */
					}
					if (polyfit(x, f, ncol, iorder, cf) == 0) {				/* Subtract off the fit */
						for (i=0; i<ncol; i++) {
							tmp = cf[iorder];
							for (j=iorder-1; j>=0; j--) tmp = tmp*x[i]+cf[j];
							for (j=0; j<nrow; j++) z[i*nrow+j] -= tmp;
						}
					}
					break;
				case UNCURL_Y:
					if (ncol < iorder+1) {
						ERRprintf("ERROR: Need a surface with at least iorder+1 columns\n");
						rcode = NOMORE; break;
					}
					f = calloc(ncol,sizeof(*f));
					for (i=0; i<nrow; i++) {
						for (j=0; j<ncol; j++) f[i] += z[j*nrow+i];
						f[i] /= ncol;													/* So average value */
					}
					if (polyfit(y, f, nrow, iorder, cf) == 0) {				/* Subtract off the fit */
						for (i=0; i<nrow; i++) {
							tmp = cf[iorder];
							for (j=iorder-1; j>=0; j--) tmp = tmp*y[i]+cf[j];
							for (j=0; j<ncol; j++) z[j*nrow+i] -= tmp;
						}
					}
					break;
			}
			free(f);
		}
		rcode = OKAY; break;

		case MA_ISOLATE:
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			for (zt=0,i=0; i<ncol*nrow; i++) zt += z[i];
			zt /= ncol*nrow;
			zt = LexGetReal((REAL) zt, "Thresholding value (average): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			npt = LexGetInt(10, "Mininum count along row/column (10): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}

			/* Simple threshold first */
			ncol = source->ncol; nrow = source->nrow; z = source->z;
			for (i=0; i<ncol*nrow; i++) z[i] = (REAL) ((z[i]>=zt) ? 1 : 0);
			/* Now track along rows */
			for (irow=0; irow<nrow; irow++) {
				ihigh = 0;													/* Number of +1's seen */
				for (icol=0; icol<ncol; icol++) {
					idx = irow+icol*nrow;								/* Index in array		*/
					if (z[idx] == 1) {									/* Another positive	*/
						ihigh++;												/* Just keep track of count */
					} else {
						if (ihigh < npt) {								/* Have to zero out the previous ihigh points */
							for (i=icol-ihigh; i<icol; i++) z[irow+i*nrow] = 0;
						}
						ihigh = 0;											/* Restart the count */
					}
				}
			}
			/* And repeat along the columns */
			for (icol=0; icol<ncol; icol++) {
				ihigh = 0;													/* Number of +1's seen */
				for (irow=0; irow<nrow; irow++) {
					idx = irow+icol*nrow;								/* Index in array		*/
					if (z[idx] == 1) {									/* Another positive	*/
						ihigh++;												/* Just keep track of count */
					} else {
						if (ihigh < npt) {								/* Have to zero out the previous ihigh points */
							for (i=irow-ihigh; i<irow; i++) z[i+icol*nrow] = 0;
						}
						ihigh = 0;											/* Restart the count */
					}
				}
			}
			rcode = OKAY; break;
						
		case MA_CENTROID:
			zt = LexGetReal(0.0f, "Value to search (0.0): ");
			if (LexEscape(TRUE)) {rcode = OKAY; break;}
			flags = 0;
			while (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-silent", 4)) {
					flags |= 0x01;
				} else if (LexEqual(token, "-AREA", 3)) {
					flags &= ~(0xC0);
				} else if (LexEqual(token, "-CM", 3)) {
					flags |= 0x40;
				} else if (LexEqual(token, "-CP", 3)) {
					flags |= 0x80;
				} else {
					ERRprintf("ERROR: Unrecognized option ignored (%s)\n", token);
				}
			}
			if (flags & 0x80 && flags & 0x40) {
				ERRprintf("ERROR: -CM and -CP are mutually exclusive.  -CM has priority\n");
			}
			rcode = (Centroid_3D_Contour_Surface(source, zt, flags) < 0) ? NOMORE : OKAY;
			break;

/* ---------------------------------------------------------------------------
-- The row/col numbers specified are based on 1 index.  1 = first row, etc.
--------------------------------------------------------------------------- */
		case MA_COL_PUT:
			icol = LexGetInt(1, "Column to insert as (1 = first): ");
			icol = max(1,min(icol, source->ncol)) - 1;		/* Limit, and now 0-based index */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) return(NOMORE);
			for (i=0; i<min(cv->npt, source->nrow); i++) source->z[i+icol*source->nrow] = cv->y[i];
			rcode = OKAY; break;

		case MA_ROW_PUT:
			irow = LexGetInt(1, "Row to insert as (1 = first): ");
			irow = max(1,min(irow, source->nrow)) - 1;		/* Limit, and now 0-based index */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) return(NOMORE);
			for (i=0; i<min(cv->npt, source->ncol); i++) source->z[irow+i*source->nrow] = cv->y[i];
			rcode = OKAY; break;

		case MA_EXTRACT_DATA:
			irow = LexGetInt(1, "Row to insert at (1 = first): ");
			irow = max(1,min(irow, source->nrow)) - 1;		/* Limit, and now 0-based index */
			icol = LexGetInt(1, "Column to insert at (1 = first): ");
			icol = max(1,min(icol, source->ncol)) - 1;		/* Limit, and now 0-based index */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) return(NOMORE);
			mode = LexChoice(FALSE, "COLUMN;Y", "ROW;X", "Extract ROW or COLumn data (ROW): ");
			if (LexEscape(TRUE)) { rcode = OKAY; break; }
			if (mode) {														/* True is COLUMN mode */
				for (i=0; i<cv->npt; i++) {
					cv->y[i] = source->z[(irow+i)+icol*source->nrow];
					cv->x[i] = source->y[irow+i];
				}
			} else {															/* False is ROW mode   */
				for (i=0; i<cv->npt; i++) {
					cv->y[i] = source->z[irow+(icol+i)*source->nrow];
					cv->x[i] = source->x[icol+i];
				}
			}
			rcode = OKAY; break;

		case MA_INSERT_POINT:
			irow = LexGetInt(1, "Row to insert at (1 = first): ");
			irow = max(1,min(irow, source->nrow)) - 1;		/* Limit, and now 0-based index */
			icol = LexGetInt(1, "Column to insert at (1 = first): ");
			icol = max(1,min(icol, source->ncol)) - 1;		/* Limit, and now 0-based index */
			zt = LexGetReal(0.0f, "Value to insert (0.0): ");
			source->z[irow+icol*source->nrow] = (REAL) zt;
			rcode = OKAY; break;

		case MA_INSERT_DATA:
			irow = LexGetInt(1, "Row to insert at (1 = first): ");
			irow = max(1,min(irow, source->nrow)) - 1;		/* Limit, and now 0-based index */
			icol = LexGetInt(1, "Column to insert at (1 = first): ");
			icol = max(1,min(icol, source->ncol)) - 1;		/* Limit, and now 0-based index */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) return(NOMORE);
			mode = LexChoice(FALSE, "COLUMN;Y", "ROW;X", "Insert as ROW or COLumn data (ROW): ");
			if (LexEscape(TRUE)) { rcode = OKAY; break; }
			if (mode) {														/* True is COLUMN mode */
				for (i=0; i<cv->npt; i++) source->z[(irow+i)+icol*source->nrow] = cv->y[i];
			} else {															/* False is ROW mode   */
				for (i=0; i<cv->npt; i++) source->z[irow+(icol+i)*source->nrow] = cv->y[i];
			}
			rcode = OKAY; break;

/* ---------------------------------------------------------------------------
-- The row/col numbers specified are based on 1 index.  1 = first row, etc.
--------------------------------------------------------------------------- */
		case MA_COL_PEAK:
			ncol = source->ncol;								/* The source information */
			nrow = source->nrow;
			z    = source->z;

			npt = ncol;
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {
				return(NOMORE);
			} else if (cv->nptmax < npt) {
				GVResize(GptMainCurve, npt);
				cv = FindCurve(GptMainCurve, FALSE);
			}
			cv->npt = npt;
			x = cv->x;
			y = cv->y;
			for (i=0; i<npt; i++) x[i] = source->x[i];

			for (i=0; i<ncol; i++) {
				xtmp = z[i*nrow];								/* Starting value */
				for (j=0; j<nrow; j++) xtmp = max(xtmp, z[j+i*nrow]);
				y[i] = xtmp;
			}
			rcode = OKAY; break;

		case MA_ROW_PEAK:
			ncol = source->ncol;								/* The source information */
			nrow = source->nrow;
			z    = source->z;

			npt = nrow;
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {
				return(NOMORE);
			} else if (cv->nptmax < npt) {
				GVResize(GptMainCurve, npt);
				cv = FindCurve(GptMainCurve, FALSE);
			}
			cv->npt = npt;
			x = cv->x;
			y = cv->y;
			for (i=0; i<npt; i++) x[i] = source->y[i];

			for (i=0; i<nrow; i++) {
				xtmp = z[i];								/* Starting value */
				for (j=0; j<ncol; j++) xtmp = max(xtmp, z[i+j*nrow]);
				y[i] = xtmp;
			}
			rcode = OKAY; break;

/* ---------------------------------------------------------------------------
-- Takes the specified surface and histograms the Z values into the main curve
--------------------------------------------------------------------------- */
		case MA_HISTOGRAM:
			rcode = Histogram(GptSurface); break;

/* ---------------------------------------------------------------------------
-- Takes the specified surface and histograms the Z values into the main curve
--------------------------------------------------------------------------- */
		case MA_MARK:
			rcode = Mark(GptSurface); break;

/* ---------------------------------------------------------------------------
-- The row/col numbers specified are based on 1 index.  1 = first row, etc.
-- If a series of rows/columns are desired to be averaged, specify as a comma
-- separated list with no spaces.
--     matrix s1 row 1,8
-- will extract the average of rows 1-8 into the main curve
--------------------------------------------------------------------------- */
		case MA_ROW_EXTRACT:
		case MA_COL_EXTRACT:
			imax = (citem->rcode == MA_ROW_EXTRACT) ? source->nrow : source->ncol ;

			if (citem->rcode == MA_ROW_EXTRACT) {
				npt  = source->ncol;						/* Number of points needed */
				imax = source->nrow;						/* Max can request			*/
				x    = source->x;
			} else {
				npt  = source->nrow;
				imax = source->ncol;
				x    = source->y;
			}

			LexConvertMode = MATH;						/* Get list in math mode */
			if (! LexGetListP(list, sizeof(list), "Row or column [or range specified as comma separated pair n1,n2] (abort): "))
				return(OKAY);
			if (LexEscape(TRUE)) return(OKAY);

			tokptr = list;
			for (i=0; i<2; i++) {						/* Get ilow and ihigh */
				LexConvertMode = MATH;
				if (LexParseLine(token, sizeof(token), tokptr, &tokptr)) {
					xtmp = GVTrimToReal(GVEvalExpr(token, &ierr));
					if (ierr != 0 || xtmp < 1 || xtmp > imax) {
						ERRprintf("ERROR: Illegal value - may be out of bounds (%s)\n", token);
						return(NOMORE);
					}
					if (i == 0) ilow = nint(xtmp); else ihigh = nint(xtmp);
				} else {
					if (i == 0) return(OKAY);
					ihigh = ilow;
				}
			}
			if (ihigh < ilow) {ilow = imax; ilow = ihigh; ihigh = imax;}

/* Get the curve to sum things into */
			if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) {
				return(NOMORE);
			} else if (cv->nptmax < npt) {
				GVResize(GptMainCurve, npt);
				cv = FindCurve(GptMainCurve, FALSE);
			}

			cv->npt = npt;
			for (i=0; i<npt; i++) {
				cv->x[i] = x[i]; 
				xtmp = 0;
				for (j=ilow-1; j<ihigh; j++) {			/* Turn 1 base into 0 based */
					idx = (citem->rcode==MA_ROW_EXTRACT) ? i*source->nrow+j : j*source->nrow+i ;
					xtmp += source->z[idx];
				}
				cv->y[i] = xtmp / (ihigh-ilow+1);
			} 
			rcode = OKAY; break;
	}
	return(rcode);
}


/* ===========================================================================
-- Routine to return a pointer to a named curve, either 2 or 3D.
=========================================================================== */
static CURVE *FindCurve(char *name, int need3d) {

	CURVE *cv;
	int type;
	void **varptr;
	
	cv = NULL;
	if (! GVGetInfo(name, &type, (void **) &varptr)) {
		ERRprintf("ERROR: Unable to find the required curve %s\n", name);
	} else if (need3d && type != GV_3DCURVE) {
		ERRprintf("ERROR: Curve %s is not 3 dimension.  Switch to 3D mode first.\n", name);
	} else if (type != GV_3DCURVE && type != GV_2DCURVE) {
		ERRprintf("ERROR: Curve %s is apparently not a simple curve - Ouch\n", name);
	} else {
		cv = (CURVE *) *varptr;
	}
	return(cv);
}

/* ===========================================================================
-- Routine to smooth a surface
--
-- Usage:  void Smooth_3D_Surface(SURFACE *s1);
--
-- Inputs: s1    - pointer to existing surface structure
--
-- Output: *s1   - filled with smoothed Z values
--
-- Return: none
--
-- Note: Smoothing is based on uniform spacing X,Y grid.  No correction
--       for changing or distorted spacing
--
-- Algorithm: Performs a similar 5 pt smooth through surface data using 
--            an analog to the Savitsky-Goulay algorithm.  Edges of the
--            surface are constant extended.
--
--            distance     weighting        #        total
--               0            17            1         17
--               1            12            4         48
--             sqrt(2)         5            4         20
--               2            -3            4        -12
--                                                   ====
--                                                    73
=========================================================================== */
void Smooth_3D_Surface(SURFACE *s1) {

	REAL *z,*nz;
	int i,j, im1,im2,ip1,ip2, jm1,jm2,jp1,jp2, nrow, ncol;
	
	z = s1->z;											/* Extract working values */
	nrow = s1->nrow;
	ncol = s1->ncol;

/* Create new space to hold smoothed values, copy back at end */
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

/* ===========================================================================
-- Routine to rotate a surface by a specified angle (degrees).  Interpolates
-- the z values based on the nearest 3-points in the overlaid grid.
--
-- Usage:  void Rotate_3D_Matrix(SURFACE *s1, char *sname, double angle, BOOL FixedSize)
--
-- Inputs: s1    - pointer to existing surface structure
--         sname - name of the surface to be rotated (need only if !FixedSize)
--         angle - desired clockwise rotation angle (degrees)
--         FixedSize - if true, final matrix will be same size as original
--                     if false, will be increased to fit "rotated" image.
--
-- Output: *s1   - filled with rotated/interpolated values
--         If FixedSize is FALSE, the size of the matrix is changed and
--         the surface is redefined (entirely new one).
--
-- Return: none
=========================================================================== */
void Rotate_3D_Matrix(SURFACE *s1, char *sname, double angle, BOOL FixedSize) {

	int i,j, irow,icol,nrow,ncol;
	REAL *z,*znew,*zptr;
	double row_eff, col_eff, zval, z00,z10,z01,z11, cosd, sind;
	
	if (angle == 0.0) return;									/* Save time if nothing to do */
	
/* Copy over the surface parameters */
	nrow = s1->nrow;
	ncol = s1->ncol;
	z = s1->z;

/* And defined values */
	cosd = cos(angle * 0.017453292519943);
	sind = sin(angle * 0.017453292519943);

	if (FixedSize) {
		nrow = s1->nrow;
		ncol = s1->ncol;
	} else {
		nrow = (int) (s1->nrow*fabs(cosd) + s1->ncol*fabs(sind) + 0.5);
		ncol = (int) (s1->nrow*fabs(sind) + s1->ncol*fabs(cosd) + 0.5);
	}
	z = s1->z;
	zptr = znew = calloc(nrow*ncol,sizeof(*znew));

	for (i=0; i<ncol; i++) {									/* Order of Z */
		for (j=0; j<nrow; j++) {
			row_eff =  (j-nrow/2.0)*cosd + (i-ncol/2.0)*sind + s1->nrow/2.0;
			col_eff = -(j-nrow/2.0)*sind + (i-ncol/2.0)*cosd + s1->ncol/2.0;
			if (row_eff <  0) row_eff = 0;
			if (row_eff >= s1->nrow) row_eff = s1->nrow-1;
			if (col_eff <  0) col_eff = 0;
			if (col_eff >= s1->ncol) col_eff = s1->ncol-1;
			irow = (int) row_eff; row_eff -= irow;			/* Integer and fractional part */
			icol = (int) col_eff; col_eff -= icol;			/* Integer and fractional part */
			if (irow == s1->nrow-1) { irow--; row_eff = 1; }	/* Make it legal always to go to irow+1 */
			if (icol == s1->ncol-1) { icol--; col_eff = 1; }
			
			z00 = z[irow+icol*s1->nrow];						/* Lower left	*/
			z10 = z[irow+(icol+1)*s1->nrow];					/* Lower right */
			z01 = z[(irow+1)+icol*s1->nrow];					/* Upper left	*/
			z11 = z[(irow+1)+(icol+1)*s1->nrow];			/* Upper right */

			if (row_eff <= 0.5 && col_eff <= 0.5) {										/*   Z01   *      |      *   Z11		*/
				zval = z00*(1-row_eff-col_eff) + z10*col_eff + z01*row_eff;			/*          r>0.5 |	c>0.5				*/
			} else if (row_eff <= 0.5 && col_eff  > 0.5) {								/*          c<0.5 |	r>0.5				*/
				col_eff = 1-col_eff;																/*         -------|-------				*/
				zval = z10*(1-row_eff-col_eff) + z00*col_eff + z11*row_eff;			/*          c<0.5 |	c>0.5				*/
			} else if (row_eff  > 0.5 && col_eff <= 0.5) {								/*          r<0.5 |	r<0.5				*/
				row_eff = 1-row_eff;																/*   Z00   *      |      *   Z10		*/
				zval = z01*(1-row_eff-col_eff) + z11*col_eff + z00*row_eff;		
			} else if (row_eff  > 0.5 && col_eff  > 0.5) {
				col_eff = 1-col_eff;
				row_eff = 1-row_eff;
				zval = z11*(1-row_eff-col_eff) + z01*col_eff + z10*row_eff;
			}
			*(zptr++) = (REAL) zval;
		}
	}

/* Decide how to save the new Z matrix now */
	if (FixedSize || (nrow == s1->nrow && ncol == s1->ncol)) {		/* Easy, just copy over */
		memcpy(z, znew, nrow*ncol*sizeof(*z));
		free(znew);
	} else {
		double dx,dx0,dy,dy0;
		dx = (s1->x[s1->ncol-1]-s1->x[0])/(s1->ncol-1);
		dx0 = s1->x[s1->ncol/2];
		dy = (s1->y[s1->nrow-1]-s1->y[0])/(s1->nrow-1);
		dy0 = s1->y[s1->nrow/2];
		if (ncol > s1->ncolmax) { s1->x = realloc(s1->x, ncol*sizeof(*s1->x)); s1->ncolmax = ncol; }
		if (nrow > s1->nrowmax) { s1->y = realloc(s1->y, nrow*sizeof(*s1->y)); s1->nrowmax = nrow; }
		for (i=0; i<ncol; i++) s1->x[i] = (REAL) (dx0 + dx*(i-ncol/2.0));
		for (i=0; i<nrow; i++) s1->y[i] = (REAL) (dy0 + dy*(i-nrow/2.0));

		if (nrow*ncol > s1->nptmax) {
			free(s1->z);
			s1->z = znew; s1->nptmax = nrow*ncol;
		} else {
			memcpy(z, znew, nrow*ncol*sizeof(*z));
			free(znew);
		}
		s1->ncol = ncol;							/* New array sizes */
		s1->nrow = nrow;
		s1->npt  = nrow*ncol;
		GVRelinkSurface(sname, s1);
	}
	return;
}

/* ===========================================================================
-- Routine to rotate a surface by a specified angle (degrees).  Interpolates
-- the z values based on the nearest 3-points in the overlaid grid.
--
-- Usage:  void Rotate_3D_Surface(SURFACE *s1, char *sname, double angle, BOOL FixedSize)
--
-- Inputs: s1    - pointer to existing surface structure
--         sname - name of the surface to be rotated (need only if !FixedSize)
--         angle - desired clockwise rotation angle (degrees)
--         FixedSize - if true, final matrix will be same size as original
--                     if false, will be increased to fit "rotated" image.
--
-- Output: *s1   - filled with rotated/interpolated values
--         If FixedSize is FALSE, the size of the matrix is changed and
--         the surface is redefined (entirely new one).
--
-- Return: none
--
-- Notes: This is a revised version of the prior code.  Major problem
--        when the grid is non-rectangular.  So, easy fix is to
--        interpolate the less dense axis so that we have an equal
--        spacing in X and Y.  Note, we assume that the spacing and 
--        are uniform in X and Y ... no non-linear interpolation.  DX
--        and DY are set by the full extent of the matrix data.
=========================================================================== */
void Rotate_3D_Surface(SURFACE *s1, char *sname, double angle, BOOL FixedSize) {

	int i,j, irow,icol,nrow,ncol;
	REAL *x,*y,*z,*znew,*zptr;
	double row_eff, col_eff, zval, z00,z10,z01,z11, cosd, sind;
	double dx,dx0,dy,dy0;

	if (angle == 0.0) return;									/* Save time if nothing to do */

/* Copy over the surface parameters */
	nrow = s1->nrow;
	ncol = s1->ncol;
	x = s1->x; y = s1->y; z = s1->z;
	dx = (x[ncol-1]-x[0])/(ncol-1);
	dy = (y[nrow-1]-y[0])/(nrow-1);

/* And defined values */
	cosd = cos(angle * 0.017453292519943);
	sind = sin(angle * 0.017453292519943);

	if (FixedSize) {
		nrow = s1->nrow;
		ncol = s1->ncol;
	} else {
		nrow = (int) (s1->nrow*fabs(cosd) + dx/dy*s1->ncol*fabs(sind) + 0.5);
		ncol = (int) (s1->ncol*fabs(cosd) + dy/dx*s1->nrow*fabs(sind) + 0.5);
	}
	z = s1->z;
	zptr = znew = calloc(nrow*ncol,sizeof(*znew));

	for (i=0; i<ncol; i++) {									/* Order of Z */
		for (j=0; j<nrow; j++) {
			row_eff =  (j-nrow/2.0)*cosd + dx/dy*(i-ncol/2.0)*sind + s1->nrow/2.0;
			col_eff =  (i-ncol/2.0)*cosd - dy/dx*(j-nrow/2.0)*sind + s1->ncol/2.0;
			if (row_eff <  0) row_eff = 0;
			if (row_eff >= s1->nrow) row_eff = s1->nrow-1;
			if (col_eff <  0) col_eff = 0;
			if (col_eff >= s1->ncol) col_eff = s1->ncol-1;
			irow = (int) row_eff; row_eff -= irow;			/* Integer and fractional part */
			icol = (int) col_eff; col_eff -= icol;			/* Integer and fractional part */
			if (irow == s1->nrow-1) { irow--; row_eff = 1; }	/* Make it legal always to go to irow+1 */
			if (icol == s1->ncol-1) { icol--; col_eff = 1; }

			z00 = z[irow+icol*s1->nrow];						/* Lower left	*/
			z10 = z[irow+(icol+1)*s1->nrow];					/* Lower right */
			z01 = z[(irow+1)+icol*s1->nrow];					/* Upper left	*/
			z11 = z[(irow+1)+(icol+1)*s1->nrow];			/* Upper right */

			if (row_eff <= 0.5 && col_eff <= 0.5) {										/*   Z01   *      |      *   Z11		*/
				zval = z00*(1-row_eff-col_eff) + z10*col_eff + z01*row_eff;			/*          r>0.5 |	c>0.5				*/
			} else if (row_eff <= 0.5 && col_eff  > 0.5) {								/*          c<0.5 |	r>0.5				*/
				col_eff = 1-col_eff;																/*         -------|-------				*/
				zval = z10*(1-row_eff-col_eff) + z00*col_eff + z11*row_eff;			/*          c<0.5 |	c>0.5				*/
			} else if (row_eff  > 0.5 && col_eff <= 0.5) {								/*          r<0.5 |	r<0.5				*/
				row_eff = 1-row_eff;																/*   Z00   *      |      *   Z10		*/
				zval = z01*(1-row_eff-col_eff) + z11*col_eff + z00*row_eff;		
			} else if (row_eff  > 0.5 && col_eff  > 0.5) {
				col_eff = 1-col_eff;
				row_eff = 1-row_eff;
				zval = z11*(1-row_eff-col_eff) + z01*col_eff + z10*row_eff;
			}
			*(zptr++) = (REAL) zval;
		}
	}

/* Decide how to save the new Z matrix now */
	if (FixedSize || (nrow == s1->nrow && ncol == s1->ncol)) {		/* Easy, just copy over */
		memcpy(z, znew, nrow*ncol*sizeof(*z));
		free(znew);
	} else {
		dx = (s1->x[s1->ncol-1]-s1->x[0])/(s1->ncol-1);
		dx0 = s1->x[s1->ncol/2];
		dy = (s1->y[s1->nrow-1]-s1->y[0])/(s1->nrow-1);
		dy0 = s1->y[s1->nrow/2];
		if (ncol > s1->ncolmax) { s1->x = realloc(s1->x, ncol*sizeof(*s1->x)); s1->ncolmax = ncol; }
		if (nrow > s1->nrowmax) { s1->y = realloc(s1->y, nrow*sizeof(*s1->y)); s1->nrowmax = nrow; }
		for (i=0; i<ncol; i++) s1->x[i] = (REAL) (dx0 + dx*(i-ncol/2.0));
		for (i=0; i<nrow; i++) s1->y[i] = (REAL) (dy0 + dy*(i-nrow/2.0));

		if (nrow*ncol > s1->nptmax) {
			free(s1->z);
			s1->z = znew; s1->nptmax = nrow*ncol;
		} else {
			memcpy(z, znew, nrow*ncol*sizeof(*z));
			free(znew);
		}
		s1->ncol = ncol;							/* New array sizes */
		s1->nrow = nrow;
		s1->npt  = nrow*ncol;
		GVRelinkSurface(sname, s1);
	}
	return;
}

/* ===========================================================================
-- Routine to determine contour points from a surface.
--
-- Usage:  void Contour_3D_Surface(SURFACE *s1, double zt, int flags);
--
-- Inputs: s1    - pointer to existing surface structure
--         zt    - target value for finding contours (crossings)
--         flags - bit-wise operation modifiers
--                 0x01 ==> sort into closed curves
--
-- Output: GptCurve will be filled with the values, expanded as necessary
--
-- Return: number of points (unsorted) or chains (sorted).  If <0, then
--         indicates an error.
=========================================================================== */
typedef struct _CONTOUR_PT {
	REAL x,y;
	INT cell;
	SHORT orient, polarity ;
} CONTOUR_PT;
#define	IS_HORZ	(0x01)					/* Orient definitions */
#define	IS_VERT	(0x02)
#define	IS_USED	(0x00)
#define	IS_LEFT	(-1)						/* On horizontal, right < left  */
#define	IS_RIGHT	(+1)						/* On horizontal, right > left  */
#define	IS_DOWN	(-1)						/* On vertical,   above < below */
#define	IS_UP		(+1)						/* On vertical,   above > below */
#define	ONE_ROW	(1<<16)
#define	CELL_FROM_ROWCOL(irow,icol)	((irow)<<16 | (icol))
#define	COL_FROM_CELL(cell)				((cell) & 0xFFFF)
#define	ROW_FROM_CELL(cell)				((cell) >> 16)

/* Local routines to test whether a vetical/horz transition will pass to next point */
static BOOL HTEST(CONTOUR_PT *start, CONTOUR_PT *end) {
	int idiff,rc=FALSE;

	idiff = end->cell - start->cell;								/* Number of cells apart */
	if (idiff == -ONE_ROW) {										/* Cell below */
		if ( (end->orient == IS_VERT && end->polarity != start->polarity) ||
			  (end->orient == IS_HORZ && end->polarity == start->polarity)) rc = TRUE;
	} else if (idiff == -ONE_ROW+1) {							/* Cell below to right */
		if (end->orient == IS_VERT && end->polarity == start->polarity) rc = TRUE;
	} else if (idiff == 0) {										/* Same cell - exit to left */
		if (end->polarity == start->polarity) rc = TRUE;
	} else if (idiff == 1) {										/* Next cell, exit on vertical */
		if (end->orient == IS_VERT && end->polarity != start->polarity) rc = TRUE;
	} else if (idiff == ONE_ROW) {								/* Above */
		if (end->orient == IS_HORZ && end->polarity == start->polarity) rc = TRUE;
	}
	return rc;
}

static BOOL VTEST(CONTOUR_PT *start, CONTOUR_PT *end) {
	int idiff,rc=FALSE;

	idiff = end->cell - start->cell;								/* Number of cells apart */
	if (idiff == -1) {												/* Previous cell */
		if ( (end->orient == IS_VERT && end->polarity == start->polarity) ||
			  (end->orient == IS_HORZ && end->polarity != start->polarity)) rc = TRUE;
	} else if (idiff == 0) {										/* Same cell - allow exit below */
		if (end->polarity == start->polarity) rc = TRUE;
	} else if (idiff == 1) {										/* Cell to right, exit across */
		if (end->orient == IS_VERT && end->polarity == start->polarity) rc = TRUE;
	} else if (idiff == ONE_ROW-1) {								/* Above to left */
		if (end->orient == IS_HORZ && end->polarity == start->polarity) rc = TRUE;
	} else if (idiff == ONE_ROW) {								/* Above to right */
		if (end->orient == IS_HORZ && end->polarity != start->polarity) rc = TRUE;
	}
	return rc;
}


int Contour_3D_Surface(SURFACE *s1, double zt, int flags) {

	CONTOUR_PT *pt, pt_start;
	REAL *x, *y, *z, z00,z10,z01;
	int	i,j,k, npt, chain_count, chain_length, orient;
	REAL  xlast, ylast, t1, t2, dx;
	int *rnx;

	BOOL do_fix, do_sort;
	int nx,nxmax, nrow,ncol,irow,icol, chain_start;
	enum {FIND_EDGE, FIND_ANY} imode;

#define	PZ(row,col)					(z[(row)+(col)*nrow])
#define	INRANGE(x,left,right)	(((left)<=(x) && (right)>(x)) || ((left)>=(x) && (right)<(x)))

	do_sort = (flags & 0x01);
	nrow = s1->nrow;
	ncol = s1->ncol;

/* First, eliminate "exact" values.  Change randomly. */
	z  = s1->z;													/* Z points to the surface array */
	t1 = t2 = z[0];
	do_fix = FALSE;
	for (i=1; i<nrow*ncol; i++) {
		if (z[i] == zt) do_fix = TRUE;
		if (z[i] < t1)  t1 = z[i];
		if (z[i] > t2)  t2 = z[i];
	}
	if (t1 == t2) {											/* If no range, no contour */
		GptCurve->npt = 0;
		return(0);
	} else if (do_fix) {
		for (i=0; i<ncol*nrow; i++) {
			if (z[i] == zt) z[i] += (REAL) (((rand() < RAND_MAX/2) ? -1 : +1) * (t2-t1) * 1E-4);
		}
	}

	nxmax = nrow*ncol/32;									/* Should seldom be that many, but power of 2 */
	pt = calloc(nxmax, sizeof(*pt));						/* Crossing points */
	rnx = calloc(nrow, sizeof(*rnx));					/* Index nx for each row */
	nx = 0;														/* # of pts found */

/* Find every crossing, sorted by rows */
	for (irow=0; irow<nrow; irow++) {					/* Go across in row-fashion			*/
		rnx[irow] = nx;										/* Save the starting index point		*/
		z10 = PZ(irow,0);										/* First point (becomes z00 below)	*/
		for (icol=0; icol<ncol; icol++) {				/* March across col-fashion			*/
			if (nx+2 > nxmax) { 
				nxmax *= 2; 
				pt = realloc(pt,nxmax*sizeof(*pt));	
			}
			z00 = z10;											/* Current = previous loop's next	*/
			if (icol+1 < ncol) {								/* Look at segment to right			*/
				z10 = PZ(irow,icol+1);						/* Z value									*/
				if (INRANGE(zt,z00,z10)) {					/* Crosses this segment?				*/
					pt[nx].cell = CELL_FROM_ROWCOL(irow,icol);	/* Unique marker							*/
					pt[nx].x = (REAL) (icol + (zt-z00)/(z10-z00));
					pt[nx].y = (REAL) irow;
					pt[nx].orient = IS_HORZ;
					pt[nx].polarity = z10>z00 ? IS_RIGHT : IS_LEFT;
					nx++;
				}
			}
			if (irow+1 < nrow) {								/* Look at upgoing segment				*/
				z01 = PZ(irow+1,icol);
				if (INRANGE(zt,z00,z01)) {
					pt[nx].cell = CELL_FROM_ROWCOL(irow,icol);	/* Unique marker							*/
					pt[nx].x = (REAL) icol;
					pt[nx].y = (REAL) (irow + (zt-z00)/(z01-z00));
					pt[nx].orient = IS_VERT;
					pt[nx].polarity = z01>z00 ? IS_UP : IS_DOWN;
					nx++;
				}
			}
		}
	}

/* Okay - get read to copy into the curve.  We need *at least* nx points  */
	if (GptCurve->nptmax < nx+100) {					/* Assume possibly 100 chains */
		if (! GVResize(GptUseCurve, nx+100)) {
			ERRprintf("ERROR: Unable to resize main curve to hold contour points (%d).", nx);
			free(pt); free(rnx);
			return(-1);
		}
		GptLinkXYZ(GptUseCurve);
	}
	x = GptCurve->x;										/* X,Y,Z now point to the return curve */
	y = GptCurve->y;
	z = GptCurve->z;

/* If not sorting, easy -  only have now to copy and finish */
	if (! do_sort) {
		for (i=0; i<nx; i++) { x[i] = pt[i].x; y[i] = pt[i].y; }
		npt = nx;
		goto ExitContour;
	}

/*	Otherwise, go back and start linking, beginning with edges */
	npt = 0;
	imode = FIND_EDGE;										/* Start by finding elements on edge, then any */
	chain_count = 0;
	while (TRUE) {
		for (i=0; i<nx; i++) {
			if (pt[i].orient == IS_USED) continue;
			if (imode == FIND_EDGE) {
				if (pt[i].x == 0 || pt[i].x == ncol-1 || pt[i].y == 0 || pt[i].y == nrow-1) break;
			} else {
				break;
			}
		}
		if (i >= nx) {											/* Did we fail to find anything? */
			if (imode == FIND_ANY) break;					/* All right - we are done!		*/
			imode = FIND_ANY;									/* Otherwise, go fill in internal loops */
			continue;
		}

		chain_start = npt;									/* First element */
		pt_start = pt[i];										/* For termination tests */
		xlast = x[npt] = pt[i].x;							/* Use this point - and keep as value */
		ylast = y[npt] = pt[i].y;	npt++;
		orient = pt[i].orient;
		pt[i].orient = IS_USED;								/* Mark as used now */
		chain_count++;
		chain_length = 1;										/* Number of points in chain */

/* Find all remaining elements */
		while (TRUE) {													
			if (npt+5 > GptCurve->nptmax) {				/* Make sure room is available */
				if (! GVResize(GptUseCurve, GptCurve->nptmax+1000)) {
					ERRprintf("ERROR: Unable to resize curve to hold more of the results (nptmax was %d)", GptCurve->nptmax);
					goto ExitContour;
				}
				GptLinkXYZ(GptUseCurve);
				x = GptCurve->x;										/* X,Y,Z now point to the return curve */
				y = GptCurve->y;
				z = GptCurve->z;
			}

			/* See if we are ready to close - only on internal segs and more than 3 chain elements already */
			if (imode == FIND_ANY && chain_length >= 4) {
				if (orient == IS_VERT && VTEST(pt+i, &pt_start)) break;
				if (orient == IS_HORZ && HTEST(pt+i, &pt_start)) break;
			}

			/* Separate behavior into vertical and horizontal segments */
			if (orient == IS_VERT) {													/* On a vertical segment? */
/*				printf("V cross: %d %f %f\n", i, xlast, ylast); */

				for (j=-1,k=max(0,i-3); k<nx; k++) {							/* Local cells */
					if (pt[k].orient == IS_USED) continue;
					if (VTEST(pt+i, pt+k)) { j = k; break; }
					if (pt[k].cell - pt[i].cell > ONE_ROW) break;			/* +1 row, +1 columns ==> too far, nothing possible */
				}

			} else {																		/* On a horizontal segment */
/*				printf("H cross: %d %f %f\n", i, xlast, ylast);	*/

				k = ROW_FROM_CELL(pt[i].cell);									/* Row of this cell */
				k = rnx[max(0,k-1)];													/* Index of previous row */
				for (j=-1; k<nx; k++) {
					if (pt[k].orient == IS_USED) continue;
					if (HTEST(pt+i, pt+k)) {j = k; break; }
					if (pt[k].cell - pt[i].cell > ONE_ROW) break;			/* +1 row, +1 columns ==> too far, nothing possible */
				}
			}
			if (j < 0) break;										/* No more to be found */
			i = j;
			xlast = x[npt] = pt[i].x;							/* Use this point - and keep as value */
			ylast = y[npt] = pt[i].y;	npt++;
			orient = pt[i].orient;
			pt[i].orient = IS_USED;								/* Mark as used now */
			chain_length++;										/* And chain has one more element to it */
		}

		if (imode == FIND_ANY) {								/* Should close */
			if ((pow(xlast-x[chain_start],2)+pow(ylast-y[chain_start],2)) < 2) {
				x[npt] = x[chain_start];
				y[npt] = y[chain_start]; npt++;
#ifdef DEBUG_CONTOUR
			} else {
				TTYprintf("Chain %d starting at (%f,%f) failed to close\n", chain_count, x[chain_start], y[chain_start]);
			}
		} else {
			if (x[npt-1] != 0 && x[npt-1] != ncol-1 && y[npt-1] != 0 && y[npt-1] != nrow-1) {
				TTYprintf("Chain %d starting on edge (%f,%f) didn't terminate on an edge (%f,%f)\n", chain_count, x[chain_start], y[chain_start], x[npt-1],y[npt-1]);
#endif
			}
		}
	}

ExitContour:
	free(pt); free(rnx);

/* Finally, replace index points with the actual x,y values.  Possibly set z value also */
	for (i=0; i<npt; i++) {
		j = (int) x[i];  dx = x[i]-j;
		if (j >= ncol-1) { j = ncol-2; dx = 1.0; }
		x[i] = s1->x[j]*(1-dx) + s1->x[j+1]*dx;

		j = (int) y[i];  dx = y[i]-j;
		if (j >= nrow-1) { j = nrow-2; dx = 1.0; }
		y[i] = s1->y[j]*(1-dx) + s1->y[j+1]*dx;

		if (z != NULL) z[i] = (REAL) zt;		/* Allows overlaying on a 3D surface */
	}
	GptCurve->npt = npt;
/*	TTYprintf("Total of %d chains\n", chain_count); */
	return(do_sort ? chain_count : npt);
}

/* ===========================================================================
-- Routine to link with the plotting routine to do contours
=========================================================================== */
#define MAX_CHAINS	(2000)

CONTOUR_DATA Enum_Contour_3D_Surface(SURFACE *s1, double zt, int maxpts) {

	CONTOUR_PT *pt, pt_start;
	REAL *x, *y, *z, z00,z10,z01;
	int	i,j,k, npt, chain_count, chain_length, orient;
	int *chain_list;
	REAL  xlast, ylast, t1, t2, dx;
	int *rnx;
	CONTOUR_DATA rc;

	BOOL do_fix;
	int nx,nxmax, nrow,ncol,irow,icol, chain_start;
	enum {FIND_EDGE, FIND_ANY} imode;

	nrow = s1->nrow;
	ncol = s1->ncol;

/* First, eliminate "exact" values.  Change randomly. */
	z  = s1->z;													/* Z points to the surface array */
	t1 = t2 = z[0];
	do_fix = FALSE;
	for (i=1; i<nrow*ncol; i++) {
		if (z[i] == zt) do_fix = TRUE;
		if (z[i] < t1)  t1 = z[i];
		if (z[i] > t2)  t2 = z[i];
	}
	if (t1 == t2) {											/* If no range, no contour */
		rc.x = rc.y = NULL;
		rc.chain_list = NULL;
		rc.chain_count = rc.npt = 0;
		return rc;
	} else if (do_fix) {
		for (i=0; i<ncol*nrow; i++) {
			if (z[i] == zt) z[i] += (REAL) (((rand() < RAND_MAX/2) ? -1 : +1) * (t2-t1) * 1E-4);
		}
	}

	if (maxpts <= 0) maxpts = 2*nrow*ncol+1;			/* Basically no limit					*/
	nxmax = min(maxpts,nrow*ncol/32);					/* Should seldom be that many, but power of 2 */
	pt = calloc(nxmax, sizeof(*pt));						/* Crossing points						*/
	rnx = calloc(nrow, sizeof(*rnx));					/* Index nx for each row				*/
	nx = 0;														/* # of pts found							*/

/* Find every crossing, sorted by rows */
	for (irow=0; irow<nrow; irow++) {					/* Go across in row-fashion			*/
		rnx[irow] = nx;										/* Save the starting index point		*/
		z10 = PZ(irow,0);										/* First point (becomes z00 below)	*/
		for (icol=0; icol<ncol; icol++) {				/* March across col-fashion			*/
			if (nx+2 > nxmax) { 
				if (nxmax >= maxpts) break;				/* No more additions, but set rnx properly */
				nxmax = min(nxmax*2,maxpts); 
				pt = realloc(pt,nxmax*sizeof(*pt));	
			}
			z00 = z10;											/* Current = previous loop's next	*/
			if (icol+1 < ncol) {								/* Look at segment to right			*/
				z10 = PZ(irow,icol+1);						/* Z value									*/
				if (INRANGE(zt,z00,z10)) {					/* Crosses this segment?				*/
					pt[nx].cell = CELL_FROM_ROWCOL(irow,icol);	/* Unique marker							*/
					pt[nx].x = (REAL) (icol + (zt-z00)/(z10-z00));
					pt[nx].y = (REAL) irow;
					pt[nx].orient = IS_HORZ;
					pt[nx].polarity = z10>z00 ? IS_RIGHT : IS_LEFT;
					nx++;
				}
			}
			if (irow+1 < nrow) {								/* Look at upgoing segment				*/
				z01 = PZ(irow+1,icol);
				if (INRANGE(zt,z00,z01)) {
					pt[nx].cell = CELL_FROM_ROWCOL(irow,icol);	/* Unique marker							*/
					pt[nx].x = (REAL) icol;
					pt[nx].y = (REAL) (irow + (zt-z00)/(z01-z00));
					pt[nx].orient = IS_VERT;
					pt[nx].polarity = z01>z00 ? IS_UP : IS_DOWN;
					nx++;
				}
			}
		}
	}
	
/*	Create space for the data, up to some maximum number of chains */
	x = calloc(nx+MAX_CHAINS, sizeof(*x));
	y = calloc(nx+MAX_CHAINS, sizeof(*x));
	chain_list = calloc(MAX_CHAINS+1, sizeof(*chain_list));
	
/*	Otherwise, go back and start linking, beginning with edges */
	npt = 0;
	imode = FIND_EDGE;										/* Start by finding elements on edge, then any */
	chain_count = 0;
	while (chain_count < MAX_CHAINS) {
		for (i=0; i<nx; i++) {
			if (pt[i].orient == IS_USED) continue;
			if (imode == FIND_EDGE) {
				if (pt[i].x == 0 || pt[i].x == ncol-1 || pt[i].y == 0 || pt[i].y == nrow-1) break;
			} else {
				break;
			}
		}
		if (i >= nx) {											/* Did we fail to find anything? */
			if (imode == FIND_ANY) break;					/* All right - we are done!		*/
			imode = FIND_ANY;									/* Otherwise, go fill in internal loops */
			continue;
		}

		chain_start = npt;									/* First element */
		pt_start = pt[i];										/* For termination tests */
		xlast = x[npt] = pt[i].x;							/* Use this point - and keep as value */
		ylast = y[npt] = pt[i].y;	npt++;
		orient = pt[i].orient;
		pt[i].orient = IS_USED;								/* Mark as used now */

		chain_list[chain_count] = chain_start;			/* Starting point of this chain */
		chain_count++;
		chain_length = 1;										/* Number of points in chain */

/* Find all remaining elements */
		while (TRUE) {													

			/* See if we are ready to close - only on internal segs and more than 3 chain elements already */
			if (imode == FIND_ANY && chain_length >= 4) {
				if (orient == IS_VERT && VTEST(pt+i, &pt_start)) break;
				if (orient == IS_HORZ && HTEST(pt+i, &pt_start)) break;
			}

			/* Separate behavior into vertical and horizontal segments */
			if (orient == IS_VERT) {													/* On a vertical segment? */
				for (j=-1,k=max(0,i-3); k<nx; k++) {							/* Local cells */
					if (pt[k].orient == IS_USED) continue;
					if (VTEST(pt+i, pt+k)) { j = k; break; }
					if (pt[k].cell - pt[i].cell > ONE_ROW) break;			/* +1 row, +1 columns ==> too far, nothing possible */
				}
			} else {																		/* On a horizontal segment */
				k = ROW_FROM_CELL(pt[i].cell);									/* Row of this cell */
				k = rnx[max(0,k-1)];													/* Index of previous row */
				for (j=-1; k<nx; k++) {
					if (pt[k].orient == IS_USED) continue;
					if (HTEST(pt+i, pt+k)) {j = k; break; }
					if (pt[k].cell - pt[i].cell > ONE_ROW) break;			/* +1 row, +1 columns ==> too far, nothing possible */
				}
			}
			if (j < 0) break;										/* No more to be found */
			i = j;
			xlast = x[npt] = pt[i].x;							/* Use this point - and keep as value */
			ylast = y[npt] = pt[i].y;	npt++;
			orient = pt[i].orient;
			pt[i].orient = IS_USED;								/* Mark as used now */
			chain_length++;										/* And chain has one more element to it */
		}

		if (imode == FIND_ANY) {								/* Should close */
			if ((pow(xlast-x[chain_start],2)+pow(ylast-y[chain_start],2)) < 2) {
				x[npt] = x[chain_start];
				y[npt] = y[chain_start]; npt++;
#ifdef DEBUG_CONTOUR
			} else {
				TTYprintf("Chain %d starting at (%f,%f) failed to close\n", chain_count, x[chain_start], y[chain_start]);
			}
		} else {
			if (x[npt-1] != 0 && x[npt-1] != ncol-1 && y[npt-1] != 0 && y[npt-1] != nrow-1) {
				TTYprintf("Chain %d starting on edge (%f,%f) didn't terminate on an edge (%f,%f)\n", chain_count, x[chain_start], y[chain_start], x[npt-1],y[npt-1]);
#endif
			}
		}
	}

	chain_list[chain_count] = npt;					/* Ending point of last chain */
	free(pt); free(rnx);

/* Finally, replace index points with the actual x,y values.  Possibly set z value also */
	for (i=0; i<npt; i++) {
		j = (int) x[i];  dx = x[i]-j;
		if (j >= ncol-1) { j = ncol-2; dx = 1.0; }
		x[i] = s1->x[j]*(1-dx) + s1->x[j+1]*dx;

		j = (int) y[i];  dx = y[i]-j;
		if (j >= nrow-1) { j = nrow-2; dx = 1.0; }
		y[i] = s1->y[j]*(1-dx) + s1->y[j+1]*dx;

	}

/* Copy over the return codes */
	rc.x = x; rc.y = y; rc.npt = npt; 
	rc.chain_count = chain_count;
	rc.chain_list = chain_list;
	return rc;
}


/* ===========================================================================
-- Routine to estimate the centroid of an area above a given contour.
--
-- Usage:  void Centroid_3D_Contour_Surface(SURFACE *s1, double zt, int flags);
--
-- Inputs: s1    - pointer to existing surface structure
--         zt    - target value for finding contours (crossings)
--         flags - bit-wise operation modifiers
--                 0x01 - make no report
--                 0x40 - Do center of mass weighting
--                 0x80 - Do center of integral weighting
--
-- Output: none
--
-- Return: Always returns 0.
=========================================================================== */
int Centroid_3D_Contour_Surface(SURFACE *s1, double zt, int flags) {

	REAL *x, *y, *z;
	double sum_f, sum_fx, sum_fy, weight, sum_0, sum_i;
	int ix,iy, ix0,iy0, nrow,ncol;
	enum {CM, CP, AREA} mode=AREA;

	static BOOL First = TRUE;
	static double x0=0.0, y0=0.0, xp=0.0, yp=0.0, area=0.0;

#define	PZ(row,col)					(z[(row)+(col)*nrow])

/* Decide on the way we want to calculate the centroid */
	mode = AREA;
	if (flags & 0x80) mode = CP;
	if (flags & 0x40) mode = CM;
	
	nrow = s1->nrow;				/* Make local pointers */
	ncol = s1->ncol;
	x = s1->x;						
	y = s1->y;
	z = s1->z;

	xp = yp = x0 = y0 = 0.0;									/* Initial guesses */
	sum_f = sum_fx = sum_fy = 0.0;
	
	switch (mode) {
		case AREA:
		case CM:
			ix0 = ncol/2;					/* Bias the sum to extend potential range with +/- */
			iy0 = nrow/2;

			sum_f = sum_fx = sum_fy = 0.0;
			for (ix=0; ix<ncol; ix++) {
				for (iy=0; iy<nrow; iy++) {								/* Calculate X first */
					if (PZ(iy,ix) < zt) continue;
					weight = (mode == AREA) ? 1.0 : PZ(iy,ix);
					sum_f +=  weight;											/* Another square in the block */
					sum_fx += weight*(ix-ix0);
					sum_fy += weight*(iy-iy0);
				}
			}

			if ( sum_f != 0) {												/* Report area back to GENPLOT */
				x0 = sum_fx/sum_f + ix0;
				y0 = sum_fy/sum_f + iy0;
			}
			break;

		case CP:
			sum_f = 0.0;
			for (ix=0; ix<ncol; ix++) {
				for (iy=0; iy<nrow; iy++) {
					if (PZ(iy,ix) < zt) continue;
					sum_f += PZ(iy,ix);
				}
			}

			/* Find the X value where the integral is 1/2 of the total */
			sum_i = 0.0;											/* Value of last and value of current point */
			for (ix=0; ix<ncol; ix++) {
				sum_0 = sum_i;										/* Save current value */
				for (iy=0; iy<ncol; iy++) {
					if (PZ(iy,ix) < zt) continue;
					sum_i += PZ(iy,ix);
				}
				if (sum_i > sum_f/2.0) break;
			}
			x0 = (sum_i == sum_0) ? ix : ix - (sum_i-sum_f/2.0)/(sum_i-sum_0);

			/* And equivalently the Y value */
			sum_i = 0.0;											/* Value of last and value of current point */
			for (iy=0; iy<ncol; iy++) {
				sum_0 = sum_i;										/* Save current value */
				for (ix=0; ix<ncol; ix++) {
					if (PZ(iy,ix) < zt) continue;
					sum_i += PZ(iy,ix);
				}
				if (sum_i > sum_f/2.0) break;
			}
			y0 = (sum_i == sum_0) ? iy : iy - (sum_i-sum_f/2.0)/(sum_i-sum_0);
			break;
	}

	area = sum_f;
	ix0 = nint(x0);										/* Interpolate x/y from s1->x,s1->y */
	iy0 = nint(y0);
	if (ix0 < 0) ix0 = 0; if (ix0 >= ncol-1) ix0 = ncol-2;
	if (iy0 < 0) iy0 = 0; if (iy0 >= nrow-1) iy0 = nrow-2;
	xp = s1->x[ix0]+(s1->x[ix0+1]-s1->x[ix0])*(x0-ix0);
	yp = s1->y[iy0]+(s1->y[iy0+1]-s1->y[iy0])*(y0-iy0);

	if (First) {
		GVLinkDouble("X_C",    GVF_USER | GVF_CONSTANT | GVF_NODELETE, &xp);
		GVLinkDouble("Y_C",    GVF_USER | GVF_CONSTANT | GVF_NODELETE, &yp);
		GVLinkDouble("Xi_C",   GVF_USER | GVF_CONSTANT | GVF_NODELETE, &x0);
		GVLinkDouble("Yi_C",   GVF_USER | GVF_CONSTANT | GVF_NODELETE, &y0);
		GVLinkDouble("Area_C", GVF_USER | GVF_CONSTANT | GVF_NODELETE, &area);
		First = FALSE;
	}

	if (! (flags & 0x01) ) {
		TTYprintf("Centroid estimate at: x=%g (%g)  y=%g (%g) based on an area of %g\n", xp, x0, yp, y0, area);
	}
	return(0);
}



/* ===========================================================================
-- Simple program to find the index where a point lies between two
-- values in an array
--
-- Usage:  i   - FindIndex(double x, REAL *ar, int npt);
--
-- Inputs: x   - value to search for
--         ar  - array of values
--         npt - number of elements in ar
--
-- Output: none
--
-- Return: index of point where i would lie in sorted order.  
--            ar[i] <= x < ar[i+1]   == or ==
--            ar[i] >= x > ar[i+1]   == or ==
--         always returns a valid index into ar.
=========================================================================== */
static int FindIndex(double x, REAL *ar, int npt) {
	int i;
	if (x == ar[0]) return(0);									/* Exact value cases */
	for (i=0; i<npt-1; i++) {
		if (x == ar[i+1]) return(i+1);
		if ( (x > ar[i]) != (x > ar[i+1]) ) return(i);
	}
	return(npt-1);
}


/* ===========================================================================
-- Histogramming routine.  Takes an array and generates a new array
-- corresponding to the probability (or number) of times a value is
-- in particular range (possibly weighted by a WEIGHT field)
--
-- Usage: int Histogram(SURFACE *source);
--
-- Inputs: source - pointer (possibly NULL) to an existing surface.
--                  if NULL, surface will be requested from command line
--
-- Output: creates main curve 
--
-- Return: OKAY - success
--         NOMORE - some error
--
-- Note: Has extensive set of its own command options which are parsed.
=========================================================================== */
static char Hist_Help[] = 
 "\n"
 "Usage: MATRIX HISTogram [-options] <surf>\n"
 "       MATRIX <surf> HISTogram [-options]\n"
 "       MATRIX HISTogram <surf> [-options]\n"
 "\n"
 "Generates a curve corresponding to a histogram of the surface.\n"
 "\n"
 "Options:\n"
 "    -center      -> use centered bins (-dx/2,dx/2) rather than (0,dx)\n"
 "    -normalized  -> normalize so sum(y) = 1\n"
 "    -density     -> per unit dx*dy interval\n"
 "    -dx <val>    -> set the bin width\n"
 "    -width <val> -> synonymous with -dx\n"
 "    -nx <ival>   -> set # of bins in X\n"
 "    -span <low> <high> -> range of values for bins\n"
 "    -surface <s1> -> Specify the surface with options\n"
 "\n"
 "Defaults: nx=100 with span existing over full range of data\n";

static int Histogram(SURFACE *source) {

	INT  nwin, ndx;
	long i,j, ilow, ihigh, npt;
	REAL a, dx, sum, xmin, xmax, *bptr, *x;
	char token[DFLT_STR_SIZE];
	LOGICAL center, normalize, densify;

	char SourceName[DFLT_STR_SIZE];
	CURVE *cv;
	void **varptr;
	int type;

/* First, check for help query */
	if (LexCheckHelp("Histogram", Hist_Help, NULL)) return OKAY;

/* Default values */
	*SourceName = '\0';								/* No surface yet specified */
	dx  = 0;												/* Work with ndx,ndy instead */
	ndx = 100;											/* Suggested # of intervals */
	xmin = xmax = 0.0;								/* No range (will cause autorange below) */
	center = normalize = densify = FALSE;		/* No options true yet */

/* Check for possible SURFACE identifier first */
	if (LexChkToken(token, sizeof(token)) && *token != '-' && *token != '/') {
		if (GVGetInfo(token, &type, (void **) &varptr) && type == GV_SURFACE) LexGetToken(SourceName, sizeof(SourceName));
	}

/* Now Scan options */
	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-CENTER", 2)) {
			center = TRUE;
		} else if (LexEqual(token, "-NORMALIZED", 2)) {
			normalize = TRUE;
		} else if (LexEqual(token, "-DENSITY", 2)) {
			densify = TRUE;
		} else if (LexEqual(token, "-DX", 3) || LexEqual(token, "-width", 4)) {
			dx = LexGetReal(-1, "Width of intervals in histogram (span/100): ");
			ndx = (dx <= 0) ? 100 : 0;
		} else if (LexEqual(token, "-NX", 3)) {
			ndx = LexGetInt(100, "Number of histogram bins (100): ");
		} else if (LexEqual(token, "-SPAN", 4)) {
			xmin = LexGetReal(xmin, "Lower band of binning (data): ");
			xmax = LexGetReal(xmax, "Upper band of binning (data): ");
		} else if (LexEqual(token, "-SURFACE", 5)) {
			if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface (abort): ")) *SourceName = '\0';
		} else {
			ERRprintf("ERROR: %s is not a recognized option\n", token);
			return(NOMORE);
		}
	}

/* Okay, get sourcename if not already specified, and pointer to surface */
	if (source == NULL || *SourceName != '\0') {
		if (*SourceName == '\0') {
			if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface (abort): ")) return OKAY;
			if (LexEscape(TRUE) || *SourceName == '/') return OKAY;
		}
		if (! GVGetInfo(SourceName, &type, (void **) &varptr)) {
			ERRprintf("ERROR: Source surface must exist.  %s was not found\n", SourceName);
			return NOMORE;
		} else if (type != GV_SURFACE) {
			ERRprintf("ERROR: %s is not a surface variable.\n", SourceName);
			return NOMORE;
		}
		source = (SURFACE *) *varptr;
	}
	x = source->z;
	npt = source->nrow*source->ncol;
	if (xmin == xmax) ArrayMinMax(x, npt, &xmin, &xmax);		/* Establish a recommended range */

/* And find the curve where result will go */
	if ( (cv = FindCurve(GptMainCurve, FALSE)) == NULL) return(NOMORE);

/* If there isn't any data at this point, result is trivial :-) */
	if (npt == 0) {
		cv->npt = 2; cv->x[0] = cv->y[0] = cv->y[1] = 0.0; cv->x[1] = 1.0;
		return OKAY;
	}

/* Now, determine number of intervals and spacing */
	a = center ? 0.5f : 0.0f;									/* Offset for centering bins */
	if (ndx > 0) dx = (REAL) (fabs(xmax-xmin)/ndx);		/* If specifying ndx */
	if (dx == 0) {
		cv->npt = 3; cv->x[0] = xmin-a-1; cv->x[1] = xmin-a; cv->x[2] = xmin-a+1;
		cv->y[0] = cv->y[2] = 0.0; cv->y[1] = (REAL) npt;
		return OKAY;
	}

/* Calculate number of intervals - reset DX if necessary - and where they will be */
	while (TRUE) {
		ilow  = (long) floor(xmin/dx+a) - 1;		/* Bin of low to be 1		*/
		ihigh = (long) floor(xmax/dx+a) + 1;		/* Bin of top to npt-2		*/
		nwin  = (INT) (ihigh - ilow + 1);			/* One below, one above		*/
		if (nwin <= GVI_MAX_LENGTH) break;			/* Small enough number to use */
		dx *= 2;
	}
	if ( (bptr = calloc(nwin, sizeof(*bptr))) == NULL) {
		ERRprintf("ERROR: Unable to allocate space to handle binning\n");
		return 0;
	} 

/* Now, do the actual binning - really easy */
	for (i=0; i <npt; i++) {								/* And loop data	*/
		j = (int) (floor(x[i]/dx+a) - ilow);			/* Bin to place	*/
		bptr[j]++;
	}

/* Normalize and/or densify as requested */
	if (normalize) {														/* Probability instead */
		for (sum=0,i=0; i<nwin; i++) sum += bptr[i];
		if (sum != 0) for (i=0;i<nwin;i++) bptr[i] /= sum;
	}
	if (densify) for (i=0; i<nwin; i++) bptr[i] /= dx;			/* Probability density */

/* Increase size if necessary */
	if (cv->nptmax < nwin) {
		GVResize(GptMainCurve, nwin);
		cv = FindCurve(GptMainCurve, FALSE);
	}

/* Store results */
	cv->npt = nwin;
	for (i=0; i<nwin; i++) {
		cv->x[i] = (ilow+0.5f-a+i)*dx;
		cv->y[i] = bptr[i];
	}
	free(bptr);
	return OKAY;
}

/* ===========================================================================
-- Local thresholding routine.  Takes a surface and thresholds it based
--- on values compared to average in the neighborhood (rectangular region).
--
-- Usage: int Mark(SURFACE *s);
--
-- Inputs: s     - pointer to the surface to be thresholded
--
-- Output: *s    - z array modified to threshold
--
-- Return: 0 - success
--         <>0 - some error
--
-- Note: Internal command options which are parsed.
=========================================================================== */
static char Mark_Help[] = 
 "\n"
 "Usage: MATRIX MARK [-options] <surf>\n"
 "       MATRIX <surf> MARK [-options]\n"
 "       MATRIX MARK <surf> [-options]\n"
 "\n"
 "Generates a curve corresponding to a histogram of the surface.\n"
 "\n"
 "Options:\n"
 "    -above <val>       -> mark pixels above value\n"
 "    -below <val>       -> mark pixels below value\n"
 "    -between <v1> <v2> -> mark pixels between values\n"
 "    -window  <v1> <v2> -> mark pixels between values\n"
 "    -outside <v1> <v2> -> mark pixels outside values\n"
"\n"
 "    -absolute          -> values are absolute data values\n"
 "    -peak              -> values are fraction of data peak\n"
 "    -average           -> values are fraction of data average\n"
 "    -range             -> values are fraction of data range (span)\n"
"\n"
 "    -full              -> use full set as data for comparison\n"
 "    -span <pixels>     -> use data in +/- pixel around point\n"
"\n"
 "    -denoise           -> remove isolated mark/blank pixels\n"
 "    -mark <value>      -> set value for marked pixels\n"
 "    -zero <value>      -> set value for unmarked pixels\n"
"\n"
 "    -surface <s1>      -> specify the surface\n"
"\n"
 "Notes:\n"
 " (1) -span with either -peak or -range options will be quite slow.\n"
 "     Speed goes as 1/npixel^2, and values >25 are painful on 1Kx1K images.\n"
 " (2) -span <pixels> -average has been optimized for reasonable speed.\n"
 "     Speed goes roughly as 1/npixel and 100 is fine on 1Kx1K images.\n"
 "Defaults: -above 100 -absolute -full -mark 1 -zero 0\n";

static int Mark(SURFACE *src) {

	int i,j,npt, nrow,ncol,irow,icol,ir,ic,ir1,ic1;
	REAL avg,sum, zmin, zmax, tmp, *z, ztest;
	char token[DFLT_STR_SIZE];
	REAL *ztmp;

	char SourceName[DFLT_STR_SIZE];
	void **varptr;
	int type;

	enum {ABOVE, BELOW, WINDOW, OUTSIDE} window=ABOVE;
	enum {ABSOLUTE, AVERAGE, PEAK, RANGE} mode=ABSOLUTE;
	enum {FULL, SPAN} area=FULL;
	LOGICAL denoise = FALSE;

	REAL mark=1.0,zero=0.0;							/* Values for a mark or a zero */
	REAL parm[2] = {100.0, 0.0};					/* Windowing parameters */
	struct { int row, col; } span = {10, 10};	/* Spanning parameters */

/* First, check for help query */
	if (LexCheckHelp("Mark", Mark_Help, NULL)) return OKAY;

/* Default values */
	*SourceName = '\0';								/* No surface yet specified */

/* Check for possible SURFACE identifier first */
	if (LexChkToken(token, sizeof(token)) && *token != '-' && *token != '/') {
		if (GVGetInfo(token, &type, (void **) &varptr) && type == GV_SURFACE) LexGetToken(SourceName, sizeof(SourceName));
	}

/* Now Scan options */
	while (LexGetOption(token, sizeof(token))) {
		/* What criteria for marking pixels */
		if (LexEqual(token, "-ABOVE", 3)) {
			window = ABOVE;
			parm[0] = LexGetReal(100, "Threshold level (100): ");
		} else if (LexEqual(token, "-BELOW", 3)) {
			window = BELOW;
			parm[0] = LexGetReal(100, "Threshold level (100): ");
		} else if (LexEqual(token, "-BETWEEN", 5) || LexEqual(token, "-WINDOW", 4)) {
			window = WINDOW;
			parm[0] = LexGetReal(0,   "Lower threshold (0): ");
			parm[1] = LexGetReal(100, "Upper threshold (100): ");
		} else if (LexEqual(token, "-OUTSIDE", 4)) {
			window = OUTSIDE;
			parm[0] = LexGetReal(0,   "Lower threshold (0): ");
			parm[1] = LexGetReal(100, "Upper threshold (100): ");

		/* How are the values to be interpreted */
		} else if (LexEqual(token, "-ABSOLUTE", 4)) {
			mode = ABSOLUTE;
		} else if (LexEqual(token, "-AVERAGE", 5) || LexEqual(token, "-AVG", 4)) {
			mode = AVERAGE;
		} else if (LexEqual(token, "-PEAK", 3)) {
			mode = PEAK;
		} else if (LexEqual(token, "-RANGE", 3)) {
			mode = RANGE;

		/* What is the "comparison" set */
		} else if (LexEqual(token, "-FULL", 5)) {
			area = FULL;
		} else if (LexEqual(token, "-SPAN", 5)) {
			area = SPAN;
			span.row = span.col = LexGetInt(10, "Window area (+/- pixels) (10): ");

		/* Values for the true and false */
		} else if (LexEqual(token, "-MARK", 5) || LexEqual(token, "-TRUE", 5)) {
			mark = LexGetReal(1.0, "Value for pixels to be marked (1.0): ");
		} else if (LexEqual(token, "-ZERO", 5) || LexEqual(token, "-FALSE", 6)) {
			zero = LexGetReal(0.0, "Value for pixels to be cleared (0.0): ");

		/* Random other options */
		} else if (LexEqual(token, "-DENOISE", 4)) {
			denoise = TRUE;
		} else if (LexEqual(token, "-SURFACE", 5)) {
			if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface (abort): ")) *SourceName = '\0';

		} else {
			ERRprintf("ERROR: %s is not a recognized option\n", token);
			return(NOMORE);
		}
	}

/* Okay, get sourcename if not already specified, and pointer to surface */
	if (src == NULL || *SourceName != '\0') {
		if (*SourceName == '\0') {
			if (! LexGetTokenP(SourceName, sizeof(SourceName), "Surface (abort): ")) return OKAY;
			if (LexEscape(TRUE) || *SourceName == '/') return OKAY;
		}
		if (! GVGetInfo(SourceName, &type, (void **) &varptr)) {
			ERRprintf("ERROR: Source surface must exist.  %s was not found\n", SourceName);
			return NOMORE;
		} else if (type != GV_SURFACE) {
			ERRprintf("ERROR: %s is not a surface variable.\n", SourceName);
			return NOMORE;
		}
		src = (SURFACE *) *varptr;
	}
	z = src->z; nrow = src->nrow; ncol = src->ncol;
	npt = src->nrow*src->ncol;
	if (npt == 0) return OKAY;
	
/* Get the span and average of all the data */
	sum = 0.0;										/* Determine average of the total data */
	zmin = zmax = z[0];
	for (i=0; i<npt; i++) {
		sum += z[i];
		if (z[i] < zmin) zmin = z[i];
		if (z[i] > zmax) zmax = z[i];
	}
	avg = sum/npt;

/* Simply my life -- window/outside and above/below can be changed linked with mark/zero */
	if (window == OUTSIDE) {
		window = WINDOW; tmp = mark; mark = zero; zero = tmp;
	} else if (window == BELOW) {
		window = ABOVE; tmp = mark; mark = zero; zero = tmp;
	}

/* Full mode, or ABSOLUTE where AREA has no meaning -- clean */
	if (area == FULL || mode == ABSOLUTE) {
		for (i=0; i<npt; i++) {
			switch (mode) {
				case ABSOLUTE:	ztest = z[i];							break;
				case AVERAGE:	ztest = z[i]/avg;						break;
				case PEAK:		ztest = z[i]/zmax;					break;
				case RANGE:		ztest = (z[i]-zmin)/(zmax-zmin); break;
			}
			if (window == ABOVE)  z[i] = (ztest > parm[0]) ? mark : zero ;
			if (window == WINDOW) z[i] = ((ztest-parm[0])*(ztest-parm[1]) <= 0) ? mark : zero ;
		}
/* Case of -SPAN but not -AVERAGE -- do full pain recalculatin zmin,zmax,avg */
	} else if (mode != AVERAGE) {
		ztmp = malloc(sizeof(*ztmp)*nrow*ncol);				/* Can't do in place however */
		for (irow=0; irow<nrow; irow++) {
			for (icol=0; icol<ncol; icol++) {
				i = irow+icol*nrow;
				zmin = zmax = z[i]; sum = 0;
				for (ir=-span.row; ir<=span.row; ir++) {			/* Scan rows */
					ir1 = (irow+ir+nrow)%nrow;
					for (ic=-span.col; ic<=span.col; ic++) {
						ic1 = (icol+ic+ncol)%ncol;
						j = ir1+ic1*nrow;
						sum += z[j];
						if (z[j] < zmin) zmin = z[j];
						if (z[j] > zmax) zmax = z[j];
					}
				}
				avg = sum / ((2*span.col+1)*(2*span.row+1));
				switch (mode) {
					case AVERAGE:	ztest = z[i]/avg;						break;
					case PEAK:		ztest = z[i]/zmax;					break;
					case RANGE:		ztest = (z[i]-zmin)/(zmax-zmin); break;
					case ABSOLUTE:												break;
				}
				if (window == ABOVE)  ztmp[i] = (ztest > parm[0]) ? mark : zero ;
				if (window == WINDOW) ztmp[i] = ((ztest-parm[0])*(ztest-parm[1]) <= 0) ? mark : zero ;
			}
		}
		for (i=0; i<nrow*ncol; i++) z[i] = ztmp[i];			/* Copy into real location now */
		free(ztmp);

	/* Case of -SPAN -AVERAGE -- do efficiently */
	} else {
		ztmp = malloc(sizeof(*ztmp)*nrow*ncol);				/* Can't do in place however */
		for (irow=0; irow<nrow; irow++) {
			sum = 0;
			for (ir=-span.row; ir<=span.row; ir++) {			/* Sum the box */
				ir1 = (irow+ir+nrow)%nrow;
				for (ic=-span.col; ic<=span.col; ic++) sum += z[ir1 + nrow*((ic+ncol)%ncol)];
			}
			for (icol=0; icol<ncol; icol++) {
				i = irow+icol*nrow;
				avg = sum/(2*span.col+1)/(2*span.row+1);
				ztest = z[i] / avg;
				if (window == ABOVE)  ztmp[i] = (ztest > parm[0]) ? mark : zero ;
				if (window == WINDOW) ztmp[i] = ((ztest-parm[0])*(ztest-parm[1]) <= 0) ? mark : zero ;

				/* Now remove column span.col backwards, and add the column span.col+1 forward */
				for (ir=-span.row; ir<=span.row; ir++) {			/* Modify the sum */
					ir1 = (irow+ir+nrow)%nrow;
					sum -= z[ir1 + nrow*((icol-span.col  +ncol)%ncol)];
					sum += z[ir1 + nrow*((icol+span.col+1+ncol)%ncol)];
				}
			}
		}
		for (i=0; i<nrow*ncol; i++) z[i] = ztmp[i];			/* Copy into real location now */
		free(ztmp);
	}

/* Do we want to denoise? */
/* Remove the "minority" first before removing the "majority" mark */
	if (denoise) {
		sum = 0; for (i=0; i<nrow*ncol; i++) { sum += z[i]; } avg = sum/(nrow*ncol);
		if (fabs(avg-mark) < fabs(avg-zero)) {
			Denoise(src, zero, mark); Denoise(src, mark, zero);
		} else {
			Denoise(src, mark, zero); Denoise(src, zero, mark);
		}
	}

	return OKAY;
}

/* ---------------------------------------------------------------------------
-- Routine to remove isolated pixels of the specified value.
-- 
--
-- Usage: Denoise(SURFACE *src, REAL mark, REAL zero);
--
-- Inputs: src - pointer to a surface
--         mark - value to be scanned for within the image
--         zero - value to replace any isolated "marks"
--
-- Output: Modified surface
--
-- Return: None
--
-- Isolated means no marked pixel in any of the 8 neighboring positions
--------------------------------------------------------------------------- */
#define	Z(irow,icol)		(z[(irow)+(icol)*nrow])

static void Denoise(SURFACE *src, REAL mark, REAL zero) {

	int irow,icol,nrow,ncol;
	REAL *z;

/* Copy from the surface structure */
	nrow = src->nrow;
	ncol = src->ncol;
	z    = src->z;

/* Scan through all the pixels */
/* If any around a mark is a mark, then we can't erase it */
	for (irow=0; irow<nrow; irow++) {
		for (icol=0; icol<ncol; icol++) {
			if (Z(irow,icol) != mark) continue;					/* Not what we are looking for */
			if (irow > 0                       && Z(irow-1,icol  ) == mark) continue;
			if (irow < nrow-1                  && Z(irow+1,icol  ) == mark) continue;
			if (                 icol > 0      && Z(irow  ,icol-1) == mark) continue;
			if (                 icol < ncol-1 && Z(irow  ,icol+1) == mark) continue;
			if (irow > 0      && icol > 0      && Z(irow-1,icol-1) == mark) continue;
			if (irow < nrow-1 && icol > 0      && Z(irow+1,icol-1) == mark) continue;
			if (irow > 0      && icol < ncol-1 && Z(irow-1,icol+1) == mark) continue;
			if (irow < nrow-1 && icol < ncol-1 && Z(irow+1,icol+1) == mark) continue;
			Z(irow,icol) = zero;
		}
	}
	return;
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

static int polyfit(REAL *x, REAL *y, int npt, int order, REAL *cf) {

/* -- Local Variables -- */
	double sumx[2*MAXFIT+1],					/* Sum X**N coefficients */
	sumy[2*MAXFIT+1],								/* Sum Y*X**N coefficients */
	ar[(MAXFIT+1)*(MAXFIT+2)/2],				/* Symmetric storage coeff's */
	temp;												/* And temporary sum values */
	REAL xmin, xmax, ymin, ymax, ynorm;
	double ax,bx,xtmp,syy,det;
	int i,j,k,nfit,nmax,num_valid,ier;

/* -- Code begin -- */
	order = min(MAXFIT, max(0, order));			/* Constrain and limit */
	nfit = order+1;									/* My order (effective) */
	for (i=0; i<nfit; i++) cf[i] = 0.0;			/* So on return, just get -1 */

	/* Validy check ... enough points to fit? */
	if (npt < nfit) {
		ERRprintf("ERROR: Please check skull for grey matter - Order of fit > # points!\n");
		return(-1);
	}
	nmax = 2*nfit-1;									/* Number of terms to collect */

	for (i=0; i<nmax; i++) sumx[i] = 0.0;
	for (i=0; i<nfit; i++) sumy[i]  = 0.0;
	syy = 0.0;											/* Sum of Y*Y */

	ArrayMinMax(x, npt, &xmin, &xmax);			/* Determine min/max values	*/
	if (xmin == xmax) {								/* ERROR!!!							*/
		ERRprintf("ERROR: Check skull for grey matter.  Same X for all data - no fit possible\n");
		return(-1);
	}

	if (order > 1) {									/* Rescale if above linear		*/
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

/* ... Create the necessary sums of x**n, x**n*y, etc. */
	for (i=0; i<npt; i++) {
		xtmp = x[i]*ax+bx;								/* Scaled coordinates */
		temp = 1.0;
		for (j=0; j<nmax; j++) {						/* Generate sums (x**n) */
			sumx[j] += temp;
			temp    *= xtmp;
		}
		temp = (y[i]-ymin)/ynorm;						/* Scaled Y							*/
		syy  += temp*temp;								/* Special sum of Y*Y			*/
		for (j=0; j<nfit; j++) {						/* Generate the sums y*(x**n) */
			sumy[j] += temp;
			temp *= xtmp;
		}
	}
	num_valid = npt;

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
	return(0);

SingularMatrix:
	ERRprintf("ERROR: Unable to fit data - matrix was singular\n");
	return(-1);
}
