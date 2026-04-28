/* gpt_3d.c */

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
#include <limits.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"
#include "tplot.h"					/* Needed for ArrayMinMax() */
#include "gptxtrn.h"
#include "gptdef.h"

#include "helper.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef struct _POINT {
		REAL dst;									/* Square of normalized distance */
		REAL x,y,z;									/* X,Y,Z values						*/
		int quad;									/* Which quadrant is this point	*/
} POINT;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static double estimate_z(POINT *list, int npt, double xt, double yt, double Smooth, double WeightExp);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Routine to create a surface from discrete x,y,z points
--
-- Usage:  int = gpt_do_3Dgrid(void);
--
-- Inputs: 
--
-- Output: 
--
-- Note: 
--
-- Algorithm: Weighted fit from the nearest 3 points, or all points within
--            a specific distance of the actual point.  We start by being
--            very stupid and handle as a linear search through the data set
--            for each point.  Later, we can become more clever.
--
-- 3/28/2007 - Added nearest point algorithm
============================================================================ */
static char Grid_3D_Help[] =
   "\n"
   "Routine to create a surface from a curve of X,Y,Z points.  This isn't\n"
   "really very good at the moment, but it has uses.  Various algorithms\n"
   "are implemented.\n"
   "\n"
   "3D_Grid <surface_name> [-options]\n"
   "\n"
   "Options:\n"
   "   -mesh <ncol> <nrow>  => Set number of rows/columns in surface\n"
   "   -range <xmin> <xmax> <ymin> <ymax> => Range of final surface\n"
   "   -nearest             => Assign value from nearest x,y,z triplet\n"
   "   -radius <fraction>   => Set radius searched for points to fit\n"
   "   -power <exponent>    => Power on weighting of points for value\n"
   "   -minpoints <n>       => Minimum number of points used in determing Z\n"
   "   -nofit               => Don't use a smoothing algorithm on values\n"
   "   -smooth <fraction>   => Smoothing radius\n"
   "\n"
   "Notes:\n"
   "   (1) For radius and smoothing, the X/Y range are normalized to [0,1]\n"
   "       and values are fractions of theh total range.  Specifying a\n"
   "       value of 0.1 indicates 10% of the total range.\n"
   "   (2) Nearest disables most other options.\n"
   "   (3) For estimation, the nearest points to each surface grid point\n"
   "       are weighted and averaged.  At least 4 points, including one\n"
   "       in each of the quadrants, are required.\n"
   "   (4) If there are at least 7 points within the defined radius and\n"
   "       -nofit has not been set, then the points are fit to a reasonable\n"
   "       surface which then is interpolated at the grid point.\n"
   "   (5) Otherwise, a weighted average with smoothing is used.\n"
   "\n"
   "Examples:\n"
   "   3d_grid s1      => Simplest usage, creates based on current display mesh\n"
   "   3d_grid s1 -mesh 101 101 -range -10 10 -5 8\n"
   "   3d_grid s1 -mesh 60 40 -nearest\n";

int gpt_do_3Dgrid(void) {

	POINT item, *match, *closest, quad[4];
	int nmatch, nmatch_alloc;

	enum _mode {NORMAL, NEAREST} mode=NORMAL;	/* Operational mode */

	double	CritRadius=0.1;					/* Use points w/in 10% range	*/
	double	WeightExp=-2;						/* Weight by 1/r**2      		*/
	int		min_closest=10;					/* Minimum # of points to avg	*/
	double	Smooth=0.01;						/* Smoothing criteria			*/
	LOGICAL	UseSurfFit = TRUE;				/* Allow use of surface fit	*/

	int i,j;
	int icol,irow,nrow,ncol,npt,type, iquad;
	char token[OPTION_STR_SIZE];				/* random tokens */
	char name[VARNAME_STR_SIZE];				/* Surface name */
	double dst, xt,yt, wsum, zest, weight, CritDst;

	REAL xmin,xmax, ymin,ymax;
	REAL *x, *y, *z;
	SURFACE *surf, **psurf=NULL;

	if (LexCheckHelp("3D_Grid", Grid_3D_Help, NULL)) return(OKAY);

	npt = GptCurve->npt;							/* X & Y will be renormalized */
	z   = GptCurve->z;
	if (npt < 10) {								/* Can't deal with! */
		ERRprintf("HMM: How about collecting a few more data points first - say 10 or 11?");
		return(NOMORE);
	} else if (z == NULL) {
		ERRprintf("ERROR: I'd say Z=0 is a great surface fitting 2D data.  Give me 3D data, okay?\n");
		return(NOMORE);
	}

/* Get min/max and permit to be modified */
	ArrayMinMax(GptCurve->x, npt, &xmin, &xmax);
	ArrayMinMax(GptCurve->y, npt, &ymin, &ymax);

/* Query information from the user */
	if (! LexGetTokenP(name, sizeof(name), "Name of created surface: "))
		return(NOMORE);

	ncol = Gpt->mesh[0];							/* Use current screen resolution */
	nrow = Gpt->mesh[1];

	while (LexGetOption(token, sizeof(token))) {
		if (LexEqual(token, "-mesh", 2)) {
			ncol = LexGetInt(51, "Number of columns = lines of constant X (51): ");
			nrow = LexGetInt(51, "Number of rows    = lines of constant Y (51): ");
			if (ncol <= 0) ncol = Gpt->mesh[0];
			if (nrow <= 0) nrow = Gpt->mesh[1];
		} else if (LexEqual(token, "-nearest", 5)) {
			mode = NEAREST;
		} else if (LexEqual(token, "-radius", 4)) {
			CritRadius = LexGetReal(0.1f, "Test radius (0.1 => w/in 10% of range): ");
		} else if (LexEqual(token, "-power", 2)) {
			WeightExp = -LexGetReal(2.0f, "Weighting exponent (2 = normal): ");
		} else if (LexEqual(token, "-minpoints", 4)) {
			min_closest = LexGetInt(4, "Minimum # of points in averaging (4): ");
			if (min_closest < 4) min_closest = 4;
			if (min_closest > npt) {
				ERRprintf("ERROR: If you want %d minimum points, then get at least that much data\n", min_closest);
				return(NOMORE);
			}
		} else if (LexEqual(token, "-nofit", 4)) {
			UseSurfFit = FALSE;
		} else if (LexEqual(token, "-smooth", 3)) {
			Smooth = LexGetReal(0.01f, "Smoothing radius (0.01): ");
		} else if (LexEqual(token, "-range", 2)) {
			xmin = LexGetReal(xmin, "X extent (min/max): ");
			xmax = LexGetReal(xmax, "X maximum range: ");
			ymin = LexGetReal(ymin, "Y extent (min/max): ");
			ymax = LexGetReal(ymax, "Y maximum range: ");
		} else {
			ERRprintf("ERROR: %s is an unrecognized 3D grid option\n", token);
			return(NOMORE);
		}
	}

/* Allocate the new surface structure and fill in initial parameters */
	if (! GVAllocSurface(name, GVF_USER, nrow, ncol)) {
		ERRprintf("ERROR: Unable to allocate a surface %s\n", name);
		return(NOMORE);
	} else if (! GVGetInfo(name, &type, (void **) &psurf) || (type != GV_SURFACE) ) {
		ERRprintf("ERROR: Surface %s was not allocated for some reason\n", name);
		return(NOMORE);
	}

	surf = *psurf;									/* Get the structure itself!	*/
	surf->nrow = nrow;							/* Fill in simple components	*/
	surf->ncol = ncol;
	for (icol=0;icol<ncol;icol++) surf->x[icol] = xmin+(xmax-xmin)*icol/(ncol-1.0f);
	for (irow=0;irow<nrow;irow++) surf->y[irow] = ymin+(ymax-ymin)*irow/(nrow-1.0f);

/* Allocate new memory and renormalize X,Y in range [0,1] for simplicity */
	x = malloc(sizeof(*x)*npt);
	y = malloc(sizeof(*x)*npt);
	for (i=0; i<npt; i++) {
		x[i] = (GptCurve->x[i]-xmin)/(xmax-xmin);		/* Normalize distances */
		y[i] = (GptCurve->y[i]-ymin)/(ymax-ymin);
	}

/* Allocate space for points within the criteria radius (CritRadius) */
	nmatch_alloc = max(20,min_closest+4);				/* Space for min + quadrant */
	match   = malloc(nmatch_alloc*sizeof(*match));
	closest = malloc(min_closest*sizeof(*closest));

/* Now, just go through the list in the most obvious manner */
	CritDst  = CritRadius*CritRadius;			/* Work in square distances */

/* Now, go through the rows and columns and make the data */
	for (icol=0; icol<ncol; icol++) {
		TTYputc('.');									/* Let world know we are alive */
		if (icol%80 == 79) TTYputc('\n');			
		xt = icol/(ncol-1.0);

		for (irow=0; irow<nrow; irow++) {
			yt = irow/(nrow-1.0);

			nmatch = 0;										/* No points within range	*/
			for (i=0; i<min_closest; i++) closest[i].dst = 1000;
			for (i=0; i<4; i++) quad[i].dst = 1000;

			for (i=0; i<npt; i++) {						/* Go through all points	*/
				item.x    = x[i];
				item.y    = y[i];
				item.z    = z[i];

				dst = (xt-x[i])*(xt-x[i]) + (yt-y[i])*(yt-y[i]) ;
				if (x[i] > xt) {							/* Quadrant I or IV */ 
					iquad = (y[i] > yt) ? 0 : 3;
				} else {										/* Quadrant II or III */ 
					iquad = (y[i] > yt) ? 1 : 2;
				}
				item.dst  = (REAL) dst;
				item.quad = iquad;

				if (dst < CritDst) {						/* Save points within space */
					if (nmatch >= nmatch_alloc) {
						nmatch_alloc += 20;
						match = realloc(match, nmatch_alloc*sizeof(*match));
					}
					match[nmatch++] = item;
				} else if (nmatch<min_closest && dst<closest[min_closest-1].dst) {	/* Keep close ones */
					for (j=min_closest-1; j>0; j--) {
						if (closest[j-1].dst < dst) break;
						closest[j] = closest[j-1];
					}
					closest[j] = item;
				} else if (dst < quad[iquad].dst) {
					quad[iquad] = item;
				}
			}
			for (i=0; nmatch<min_closest; i++,nmatch++) match[nmatch]=closest[i];

/* Now, add points so there are some in all four quadrants */
			for (i=0; i<nmatch; i++)						/* Mark out quad's there */
				quad[match[i].quad].dst = 1000.0;
			for (i=0; i<4; i++)								/* Add those remaining */
				if (quad[i].dst < 1.0) match[nmatch++] = quad[i];

/* Okay, now we have nmatch points to interpolate from for Z value */
			if (mode == NEAREST) {
				dst = 1000;
				for (i=0; i<nmatch; i++) {
					if (match[i].dst < dst) { dst = match[i].dst; zest = match[i].z; }
				}
			} else if (nmatch >= 7 && UseSurfFit) {
				zest = estimate_z(match, nmatch, xt, yt, Smooth, WeightExp);
			} else {
				wsum = 0.0;
				zest = 0.0;
				for (i=0; i<nmatch; i++) {
					weight = 1E-10 + match[i].dst + Smooth*Smooth;
					weight = pow(weight, WeightExp/2);			/* dst is squared		*/
					zest   += match[i].z * weight;
					wsum   += weight;
				}
				zest /= wsum;
			}
			surf->z[icol*nrow+irow] = (REAL) zest;
		}
	}
	TTYputc('\n');								/* From the ..... going across */

	free(x); free(y);							/* My normalized copies */
	free(match); free(closest);			/* Matching point structures */
	
	return(OKAY);
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
static double estimate_z(POINT *list, int npt, double xt, double yt, double smooth, double ipow) {

	REAL x, y, z;
	int i,j,ierr;

	double s=0,y3x=0;							/* Deal with 1, y^3x */
	double xi[4]={0,0,0,0};					/* Deal with x^i, i=0,...,4	*/
	double yi[4]={0,0,0,0};					/* Deal with y^i, i=1,...,4	*/	
	double yxi[3]={0,0,0};					/* Deal with yx^i, i=1,2,3		*/
	double y2xi[2]={0,0};					/* Deal with y^2x^i, i=1,2		*/
	double b[6]={0,0,0,0,0,0};				/* Resultant vector				*/
	double ar[6*7/2];							/* Real symmetric storage array */
	double tmp,weight;						/* And temporary sum values */

/* -- Code begin -- */
	if (npt < 7) return(list[0].z);		/* Don't screw around */

/* ---------------------------------------------------------------------------
-- Note, my "weights" are the 1/sigma^2 terms in sums for analysis.        
---------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {

		x = list[i].x;
		y = list[i].y;
		z = list[i].z;

		weight = list[i].dst + smooth*smooth;		/* Smoothing weight	*/
		weight = pow(weight, ipow/2);					/* dst is squared		*/

		s += weight;
		for (tmp=weight,     j=0; j<=3; j++) {tmp *= x; xi[j]   += tmp;}
		for (tmp=weight,     j=0; j<=3; j++) {tmp *= y; yi[j]   += tmp;}
		for (tmp=weight*y,   j=0; j<=2; j++) {tmp *= x; yxi[j]  += tmp;}
		for (tmp=weight*y*y,	j=0; j<=1; j++) {tmp *= x; y2xi[j] += tmp;}
		y3x += weight*y*y*y*x;

		weight = z*weight;
		b[0] += weight*x*x;					/* Result vector */
		b[1] += weight*y*y;
		b[2] += weight*x*y;
		b[3] += weight*x;
		b[4] += weight*y;
		b[5] += weight;
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
	if (ierr != 0) return(list[0].z);
	g_sppsl(ar, 6, b);								/* Result to b vector		*/

	return(b[0]*xt*xt+b[1]*yt*yt+b[2]*xt*yt+b[3]*xt+b[4]*yt+b[5]);
}
