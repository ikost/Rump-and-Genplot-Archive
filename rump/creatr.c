/*  creatr.c */

/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ----------------- */
/*  ---------                                              ----------------- */
/*  ---------    The source code to RUMP may be freely     ----------------- */
/*  ---------  modified as long as this copyright notice   ----------------- */
/*  ---------          is included and unchanged.          ----------------- */
/*  ------------------------------------------------------------------------ */

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
#if (defined LINUX && ! defined __USE_XOPEN)	/* Need prototype for erfc()			*/
	#define	__USE_XOPEN								/* Must be placed just before math.h */
#endif
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	GV_MATH_EXTENSIONS				/* make erf, erfc, ndtr, ndtri visible */
#include "rump.h"
#include "tplot.h"							/* Need ArrayMinMax */
#include "stopping.h"
#include "sample.h"
#include "creatr.h"
#include "xsect.h"
#include "sigma.h"							/* For cross sections */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define CUTOFF (samm->cutoff)			/*  minimum keV for tolerable epsilon	*/
#define DBUG(x)

#define	RPI		3.14159265						/* pi				*/

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void CalcCpuUsage(int key, char *buffer);
static void CalcAverageStop(STOPPING_TABLE *table, BOOL inflag);

static BOOL FillSimHeader(SAMPLE *sample);
static void FillSimStructure(int iteration, SAMPLE *sample);

static void ConvoluteDetector(SPECTRUM *buf);
static void AddNoise(SPECTRUM *buf);

static void SimCideal(int elno, int from, int to, REAL misot, REAL fisot);
static void SimMakeit(SAMPLE *sample, int elx, int layx);
static void SimFlyout(int lay, REAL *ee, REAL *ratde);
static void SimBacklay(int z_detect, REAL m_detect, int lay, REAL efront, REAL hfront, REAL ratde, REAL km2);

static void SimPrecal(SAMPLE *sample);
static REAL RbsEfact(int layer, REAL ee, REAL km2);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
SAMM *samm=NULL;
BOOL SimSilent=FALSE;

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static int     simflops = 0;
static clock_t cputime  = 0;
static int InFullSimMode = FALSE;		/* Used to limit messages */

/* ============================================================================
--  Usage Guide:
--
--      SUBROUTINE CHKSIM
--     Subroutine CHKSIM determines if the simulation buffer (0) is out
--     of date, and if so calls CREATE to generate a fresh one.
--  Quick: Updates simulation if necessary
--
--     INPUTS:   Compares values of buffers 1 and 0 in RUMP common block
--
--     OUTPUTS:  Call/No call to CREATE
--
--     Returns:  TRUE if simulation was performed, FALSE otherwise
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       BMANIP, BFPROC
--     CALLS:             NAMEIT, CREATE
--
=========================================================================== */
BOOL SimCheck(void *Sample) {

	clock_t cp;
	SAMPLE *sample;

	sample = (Sample == NULL) ? SimDefaultSample : (SAMPLE *) Sample;

	if (sample == NULL || Rmp->autsim == 0) {	/* No simulations */
		return(FALSE);

	} else if (Rmp->autsim < 0 ||
       (ibuf->theta   != ALTBUF->theta)   ||	/*  Angle changed */
       (ibuf->phi     != ALTBUF->phi)     ||	/*  Angle changed */
       (ibuf->psi     != ALTBUF->psi)     ||	/*  Angle changed */
       (ibuf->e0      != ALTBUF->e0)      ||	/*  Energy changed */
       (ibuf->fwhm    != ALTBUF->fwhm)    ||	/*  Det. res. changed */
       (ibuf->tau     != ALTBUF->tau)     ||	/*  Shaping time changed */
       (ibuf->current != ALTBUF->current) ||	/*  Pileup different */
       (ibuf->geom    != ALTBUF->geom)    ||	/*  Different geometry */
       (ibuf->type    != ALTBUF->type)    ||	/*  Different type spectrum */
       (ibuf->kevch   != ALTBUF->kevch)   ||	/*  Different MCA's */
       (ibuf->kev0    != ALTBUF->kev0)    ||	/*  Different MCA's */
       (ibuf->zbeam   != ALTBUF->zbeam)   ||
       (ibuf->mbeam   != ALTBUF->mbeam) ) {	/*  Different beam */

/*  ... Something rendered the simulation obsolete: we must redo it. */
		 InFullSimMode = TRUE;
		 if (! SimSilent) TTYputs("Automatic simulation .");
		 cp = clock();														/* Start timing	 */
		 SimCreateDetails(sample,ELEM_INVALID,LAYER_INVALID);	/* Go do it			 */
		 SimSetIdent();													/* Reset the names */
		 Rmp->autsim = 1;													/* And we are done */
		 if (! SimSilent) {
			 TTYprintf(" all(%.3f) .", ((REAL) (clock()-cp)) / CLOCKS_PER_SEC);
			 TTYputs(" performed.\n");
		 }
		 InFullSimMode = FALSE;
		 return(TRUE);

	} else {
		return(FALSE);
	}
}


/* ===========================================================================
-- Routine to print out stopping power and info based on current simulation.
=========================================================================== */
void SimCalcInfo(ATOMS *atomp) {
	int i;
	REAL x1;
	
	i = atomp->z;
	x1 = atomic_density(i) / 6.022e23f * atomic_mass(i); /*  g/cc density */

	TTYputs  ("\n-----------------------------------------------------\n");
	TTYprintf("%2s  Z: %2d   Mass: %6.2f  Density: %11.4e at/cc (%5.2f g/cc)\n\n",
				 atomic_symbol(i), i, atomic_mass(i), atomic_density(i), x1);
	TTYprintf("  Beam Energy: %8seV  Theta%6.2f     Phi%7.2f\n",
				 ToEngFormat(ibuf->e0*1.00e6f), ibuf->theta, ibuf->phi);
	TTYputs("\n");
	return;
}

/* ===========================================================================
-- Routine to output the current profile as a tab-delimited file containing
-- the depth and elemental compositions.  Suitable for insertion into a
-- spreadsheet or for GENPLOT graphing.
=========================================================================== */
BOOL SimWriteProfile(void *Sample, FILE *funit) {

	SAMPLE *sample;
	SAMM *sm;
	SIMLAYER *layer;
	int i,j;
	double thick_cm2, depth_cm2, depth_nm, thick_nm;

	sample = (Sample == NULL) ? SimDefaultSample : (SAMPLE *) Sample;
	if (sample == NULL) return(FALSE);				/* Nothing done */

	if ( (sm = SimCreateDetails(sample, ELEM_INVALID, LAYER_INVALID)) == NULL) return(FALSE);

	if (funit == NULL) funit = stdout;

/* Find the layer information */
	layer = sm->layer;									/* Layer information */
	if (layer == NULL) return(FALSE);

/*	Loop through the headers */
	fprintf(funit, "Layer\t  x(nm)\t (/cm2)\t dx(nm)\t (/cm2)");
	for (i=0; i<sm->num_elements; i++) fprintf(funit, "\t   %s", sample->elem_names[i]);
	fputc('\n', funit);

/* Loop through all of the sub-layers and elements printing */
	depth_cm2 = depth_nm = 0;
	for (i=0; i<sm->num_layers; i++) {
		for (thick_cm2=0,j=0; j<sm->num_elements; j++) thick_cm2 += layer[i].strct[j];
		thick_nm = thick_cm2 / layer[i].density;	/* 1E15/cm2 / 1E23/cm3 = 10E-8 cm = A */
		thick_nm /= 10.0f;								/* And then to nm */
		fprintf(funit, "%d\t%7.1f\t%7.1f\t%7.1f\t%7.1f", layer[i].layer, depth_nm, depth_cm2, thick_nm, thick_cm2);
		for (j=0; j<sm->num_elements; j++) fprintf(funit, "\t%7.5f", layer[i].strct[j]/thick_cm2);
		fputc('\n', funit);
		depth_cm2 += thick_cm2;
		depth_nm  += thick_nm;
	}

/* And terminating line giving final positions */
	fprintf(funit, "%d\t%7.1f\t%7.1f\t%7.1f\t%7.1f", layer[i-1].layer, depth_nm, depth_cm2, 0.0, 0.0);
	for (j=0; j<sm->num_elements; j++) fprintf(funit, "\t%7.5f", layer[i-1].strct[j]/thick_cm2);
	fputc('\n', funit);

	return(TRUE);
}



/* ===========================================================================
--  Usage Guide:
--
--     Subroutine CREATE starts from the common block /sample.h/ and
--     finishes with a spectrum for that sample in the alternate
--     buffer.  On its own, it zeros the spectrum (so that subprograms
--     can cumulate into it), and scans through the isotope table for
--     each element, calling CIDEAL for each isotope of each element.
--     CIDEAL will then overlay each of the isotopes into the spectrum.
--     CREATE has to call FILSAM beforehand, to turn /sample.h/ into /SAMPLE/,
--     and calls DETRES and PILEUP afterwards to take the ideal spectrum
--     into something similar to that actually measured.
--     
--  Quick: Computes a simulated spectrum
--
--     INPUTS:   RUMP's /PMS1/ common block has the necessary info:
--               E0    incident engery (MeV)
--               THETA    Angle of incident beam to sample normal (deg)
--               PHI      Angle of exit beam to incident beam (deg)
--               Each of these has two values - we copy the ones in
--                  SHAD into the other (ALTBUF), so there is no confusion.
--
--     We also copy the SHAD into ALT of other parameters, used
--     by subroutines directly or indirectly called from here:
--     specifically:
--               kevch    Energy of channel number zero (keV)
--               kev0     Energy per channel (keV)
--
--     And for this idealized computation, normalization factors are
--     set to standard values in the ALT half:
--               Q        Charge accumulation, set to 1 uCoulomb
--               OMEGA    Detector solid angle, set to 1 mSteradian
--               CORR     Correction factor, set to unity
--
-- Note: SimCreateDetails and FillSimStructure must understand the 
--       sample.h sample structure, as well as the creatr.h structure.
=========================================================================== */
void *SimCreateDetails(void *Sample, int elx, int layx) {

	int i, j, npt;
	double tmp, sum;
	SAMPLE *sample;

	sample = (Sample == NULL) ? SimDefaultSample : (SAMPLE *) Sample;
	
/* ... Check on samm first */
	if (samm == NULL) {
		samm = calloc(1,sizeof(SAMM));						/* Make all NULL */
		samm->num_layer_allocated = 50;
		samm->layer = calloc(samm->num_layer_allocated, sizeof(SIMLAYER));
		samm->glofirst = NULL;
	}

/* ... Copy everything from ibuf to alt, then modify as necessary */
	RbsCopySpectrum(ALTBUF, ibuf);			/* Copy current buf to altbuf		*/
	if (ALTBUF->nptmax < CMAX) RbsResizeSpectrum(ALTBUF, CMAX);
	ALTBUF->npt   =  0;							/* No points in case of errors	*/
	ALTBUF->q     =  (REAL) ALTBUF->cbeam;	/* These will be reset later		*/
	ALTBUF->omega =  1.0f;
	ALTBUF->corr  =  1.0f;
	strcpy(ALTBUF->ltct, "Not implemented");

/* ... Early sanity checks */
	if (sample->first == NULL) {
		ERRprintf("failed\nERROR: No valid layers exist in sample description\n");
		return(NULL);
	} else if (! FillSimHeader(sample)) {
		return(NULL);
	}

/* ... Okay, empty out the resulting buffer and get started */
	ALTBUF->npt = CMAX;							/* Empty the buffer */
	for (i=0; i<CMAX; i++) ALTBUF->counts[i] = 0.0f;

	if (SimInitFillSpectrum != NULL)			/* Initialize spectrum fill routine */
		(*SimInitFillSpectrum)(ALTBUF);

/* ...  Loop through all the iterations */
	for (i=0; i<samm->maxit; i++)	{
		FillSimStructure(i, sample);
		SimMakeit(sample, elx, layx);
	}

	if (SimTermFillSpectrum != NULL)			/* Terminate spectrum fill routine */
		(*SimTermFillSpectrum)(ALTBUF);

/* ... Convolute detector resolution with ideal spectrum */
/* if (sample->straggle == 0.0) --- REMOVED 7/9/94 MOT */
	ConvoluteDetector(ALTBUF);

/*  ... Normalize back to real counts.  Set q,omega etc as valid */
	tmp = RbsNormK(ALTBUF)/RbsNormK(ibuf);		/* Put back q,omega,corr	*/
	ALTBUF->q      = ibuf->q;						/*  Now, set these equal	*/
	ALTBUF->omega  = ibuf->omega;
	ALTBUF->corr   = ibuf->corr;
	for (i=0; i<ALTBUF->npt; i++) ALTBUF->counts[i] *= (float) tmp;

/* ... And with counts, do the pileup correction */
	if (SimPileup != NULL) (*SimPileup)(ALTBUF);
	
/* ... Set npt to reasonable number, either where data is, or original buf */
/* ... Has real "counts", so I can be a little more lenient than exactly 0 */
	j = ALTBUF->nptmax;							/* Maximum it can be */
	while (j>0 && ALTBUF->counts[j-1] <= 0.5) j--;
	ALTBUF->npt = min( max(ibuf->npt,j+2), ALTBUF->nptmax);

/* If wanted, add ad-hoc multiple scattering */
	if (sample->multiple != 0.0) {
		for (sum=0,i=0; i<ALTBUF->npt; i++) sum += ALTBUF->counts[i];
		sum /= ALTBUF->q*ALTBUF->omega;					/* "Strength" of multiple scatter */
		sum *= 1.5E-9 * sample->multiple;				/* Ad-hoc scaling */
		for (tmp=0,i=ALTBUF->npt-1; i>=0; i--) {
			tmp += ALTBUF->counts[i];
			ALTBUF->counts[i] += (float) (tmp*sum);
		}
	}

/* If configured, add background spectrum */
	if (sample->background != NULL) {
		npt = min(sample->background->npt, ALTBUF->npt);
		for (i=0; i<npt; i++) ALTBUF->counts[i] += sample->background->counts[i];
	}

/* If desired, add noise to the spectrum */
	if (sample->noise != 0.0) AddNoise(ALTBUF);
	
/* And flag dirty, return */
	ALTBUF->dirty = FALSE;						/* Flag the fresh spectrum */
	return((void *) samm);
}


/* ===========================================================================
--    BOOL FillSimHeader(SAMPLE *sample)
--
--    This used to be part of Precal.  It fills in the constant portions
--    of the SIM structure samm.  Handles scattering angles, determines
--    size of structures needed, number of iterations.
--
--  Quick: Munches /sample.h/ and SPECTRUM into /creatr.h/ headers
--
--  Output: Prints <failed> followed by message if something is wrong.
--
--  Returns: TRUE ==> everything is set up.
--
--  Note: This routine requires knowledge of  the sample structure from SIM.
--
-- MODIFICATION: Changed so that samm->cosph is TRUE scattering angle.
--               This eliminates the sign ambiguity in code throughout.
=========================================================================== */
#define DEGREES_TO_RADS  (3.141592654/180.0)
#define SIND(x)			sin(DEGREES_TO_RADS*(x))
#define COSD(x)			cos(DEGREES_TO_RADS*(x))

static BOOL FillSimHeader(SAMPLE *sample) {

	int i;
	LAYER *tmp;

	GLOBAL_LAYER *gtmp;							/* sample.h's version */
	GLOLAYER *glotmp;								/* creatr.h's version */
	REAL sum, density, ymin, ymax, *spl;
	int itype, opts;
	CURVE *cv, **aptr;

/* ... Fill in stopping power table based on incident beam */
	samm->zproj = ALTBUF->zbeam;				/* Z of beam			*/
	samm->mproj = ALTBUF->mbeam;				/* Mass of beam		*/
	if (samm->zproj == 0) {
		ERRprintf("failed\nERROR: Incident particle is bad - no simulation possible\n");
		return(FALSE);
	} else if ( (samm->pi = RbsStpfind(samm->zproj, samm->mproj,
			&e1_scale, ALTBUF->e0)) == NULL) {
		ERRprintf("failed\nERROR: No stopping power available for beam - simulation aborted");
		return(FALSE);
	}

/* ... Fill in the scattering geometry path corrections */
	samm->phi   = (REAL) ALTBUF->phi;
	samm->sinph = (REAL) SIND(ALTBUF->phi);
	samm->cosph = (REAL) -COSD(ALTBUF->phi);									/* NOW REAL SCATTERING ANGLE PHI */
	samm->secin = (REAL) COSD(ALTBUF->theta);
	if (samm->secin != 0.0) samm->secin = 1.0f / samm->secin;
	samm->secout = 0.0;
	switch (ALTBUF->geom) {
		case CORNELL:
			if (samm->cosph != 0) samm->secout = - samm->secin / samm->cosph;
			break;
		case IBM:
			samm->secout = (REAL) (1.0f / COSD(ALTBUF->theta + ALTBUF->phi));
			break;
		default:
			samm->secout = (REAL) (1.0f / COSD(ALTBUF->psi));
			break;
	}
	if ( samm->secin <= 0 || samm->secout <= 0) {
		ERRprintf("failed\nERROR: Bad scattering geometry - check angles\n");
		return(FALSE);
	}

/* ---------------------------------------------------------
-- Determine number of iterations for the FUZZ feature
--------------------------------------------------------- */
	samm->maxit = 1;
	for (tmp=sample->first; tmp!=NULL; tmp=tmp->next) {
		if (tmp->fuzzs > 0) samm->maxit *= tmp->fuzzs;
	}

/* ---------------------------------------------------------
--  Copy over the Element table Z and M, converting from
-- integer mass number to actual nuclide mass.
--
-- BEWARE: Order correspondence of elements between *samm 
--         and *sample is assumed in SimWriteProfile
--------------------------------------------------------- */
	samm->num_elements = sample->nel;
	for (i=0; i<samm->num_elements; i++) {
		samm->z[i]  = sample->z2[i];				/* Copy over Z					*/
		samm->m2[i] = 0.0f;							/* Use natural abundance	*/
		if (sample->nukem[i] != 0)					/* Or specified value		*/
			samm->m2[i] = RbsGetRealMass(samm->z[i], sample->nukem[i]);
	}

/* ----------------------------------------------------------
-- If we have a global spline feature, free any already 
-- allocated and do the spline initialization.
---------------------------------------------------------- */
	while (samm->glofirst != NULL) {
		free(samm->glofirst->spl);
		glotmp = samm->glofirst->next;
		free(samm->glofirst);
		samm->glofirst = glotmp;
	}

	for (gtmp=sample->g_first; gtmp!=NULL; gtmp=gtmp->next) {
		if (*gtmp->curve == '\0') continue;

		/* First, sum the species values and inverse density */
		sum = density = 0.0f;
		for (i=0; i<sample->nel; i++) {
			sum     += gtmp->species[i];
			density += gtmp->species[i] / atomic_density(samm->z[i]);
		}
		if (sum <= 0) continue;									/* Nothing here */
		density = density / sum;								/* Normalize atomic fractions */
		density = (density <= 0.0f) ? 0.4997f : (1.0f/density) / 1E23f;

		if (! GVGetInfo(gtmp->curve, &itype, (void **) &aptr) || (itype != GV_2DCURVE && itype != GV_3DCURVE)) {
			ERRprintf("ERROR: Global diffusant profile (%s) does not resolve to a 2D/3D curve.\n", gtmp->curve);
			return(FALSE);
		}
		cv = *aptr;												/* Get the curve itself */

		ArrayMinMax(cv->y, cv->npt, &ymin, &ymax);
		if (ymin < 0.00 || ymax > 1.0) {
			ERRprintf("WARNING: Atomic fraction in global curve (%s) extend %f to %f.\n"
						 "         Values will be truncated to stay < 1 and may not give desired results.\n", 
						 gtmp->curve, ymin,ymax);
		}

		opts = 0;
		if (gtmp->mode & 0x01) opts |= 0x01;		/* Piecewise linear */
		if ( (spl = GVFitSpline(NULL, cv->x, cv->y, cv->npt, opts)) == NULL) {
			ERRprintf("ERROR: Unable to fit global spline - data invalid? (FillSimStructure)\n");
			return(FALSE);
		}

/* Okay - believe enough to insert it into the SAMM structure block */
		glotmp = calloc(1,sizeof(GLOLAYER));		/* New layer */
		glotmp->next = samm->glofirst;
		samm->glofirst = glotmp;

		glotmp->species = gtmp->species;				/* Duplicate this pointer */
		glotmp->start   = gtmp->start;				/* And this */
		glotmp->sum     = sum;
		glotmp->density = density;
		glotmp->spl     = spl;
		ArrayMinMax(cv->x, cv->npt, &glotmp->gxmin, &glotmp->gxmax);
/*		TTYprintf("Have global diffusant: %f %f %f %f\n", glotmp->gxmin, glotmp->gxmax, ymin, ymax); */
	}

	return(TRUE);
}


/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION FillSimStructure(ITER)
--
--     Function FILSAM is responsible for translating the /sample.h/
--     common block into the /creatr.h/ common block. This involves
--     making unit conversions, dividing up the sublayers, and
--     computing the average values of the polynomial dE/dX fit for
--     each layer.
--     This routine must be called multiple times for proper
--     implementation of the FUZZ option.  A number of layer
--     structures will be computed, simulating the effects of
--     lateral nonuniformity.  The resultant spectra are to
--     be averaged.  See the code in CREATE
--  Quick: Munches /sample.h/ into /creatr.h/
--
--     INPUTS:   ITER  Iteration count: start at 1, it will be modified
--                     for the next call.
--
--               /sample.h/ common block
--               /ATOMS/ common block
--
--     OUTPUTS:  FILSAM   Will be .TRUE. as long as there are more iterations
--                        left to do; .FALSE. on return from the last one.
--
--               /SAMPLE/ common block
--
--  Note: This is the only routine in creatr that requires knowledge of 
--        the sample structure from SIM.  Must be kept in sync.
--
-- IF THIS ROUTINE CHANGES, MODIFY ALSO EQUIVALENT ROUTINES IN SIM2.C
=========================================================================== */
static void FillSimStructure(int iteration, SAMPLE *sample) {

/*  -- Local Variables -- */
	int i;
	int iel,isublayer,ilayer;
	int jf,kf,mf,nf;
	int runner, numerrs;
	REAL size;

	REAL	matrix_sum,						/* Sum of matrix composition coeff's	*/
			matrix_density,				/* Weighted matrix density (1E23/cm3)	*/
			species_sum,					/* Sum of species composition coeff's	*/
			species_density,				/* Weighted matrix density (1E23/cm3)	*/
			thick_to_cm2,					/* Conversion user thick to atoms/cm2	*/
			sthick_to_cm2,					/* Species thickness to atoms/cm2		*/
			maxpath,							/* Maximum thickness (1E15 atoms/cm2)	*/
			user_thick,						/* Thickness in user (given) units		*/
			cm2_thick,						/* Thickness in 1E15/cm2 units			*/
			cm_thick,						/* Thickness in centimeters				*/
			frac_matrix,					/* Atomic fraction for matrix comp		*/
			frac_species,					/* Atomic fraction for species diff		*/
			frac_global,					/* Total of all global diffusants		*/
			inv_density_global,			/* Sum of weighted inverse global density */
			front_depth_cm2,				/* Depth (from surface) of layer in 10^15 at/cm^2 */
			back_depth_cm2;				/* Depth (from surface) of back of sub-layer */
	int	num_sublayers;					/* Number of sublayers to use				*/
	void *spl=NULL;						/* For spline equation						*/

	enum {FORWARD, REVERSE} dir=FORWARD;
	REAL aden,y,k, x,xx;
	REAL c0,c1, x0,dose, sigma, fourDt, Dt, D_over_v, integ_frac, t1,t2;
	REAL *xtmparray=NULL;				/* For user equations						*/

	LAYER *layer, *tmp;
	GLOLAYER *gtmp;

/*  -- Code begin -- */
	samm->ampl  = 1.0f;					/* Fully sum this layer into result		*/
	samm->num_layers = 0;				/* No layers when we first start			*/
	maxpath = (REAL) (sample->maxpth / max( fabs(samm->secin), fabs(samm->secout) ));

/* ---------------------------------------------------------------------------
-- Sum coefficients from composition and species, and determine a weighted
-- atomic density.  For density, we use the idea of hard ball packing with
-- weighted sum of "cm^3/atom" instead of "atoms/cm^3".  Thus, matrix_density
-- below is really weighted sum of inverse density, which is fixed below 
--
-- Prior to 1/97, the sum was done on atomic density rather than inverse
-- density.  This "less correct" mode can be forced if desired.
--------------------------------------------------------------------------- */
/* Haven't started any of the global diffusants yet */
	for (gtmp=samm->glofirst; gtmp!=NULL; gtmp=gtmp->next) gtmp->start_cm2 = -1.0;

	front_depth_cm2 = back_depth_cm2 = 0;
	for (ilayer=1,layer=sample->first; layer!=NULL; ilayer++,layer=layer->next) {

		matrix_sum  = matrix_density  = 0.0f;					/* Sum compositions	*/
		species_sum = species_density = 0.0f;

		if (RbsDensityCalc == IMPROVED) {
			for (iel=0; iel<sample->nel; iel++) {
				matrix_sum      += layer->matrix[iel];			/* Matrix sum			*/
				species_sum     += layer->species[iel];		/* Species sum			*/
				matrix_density  += layer->matrix[iel] / atomic_density(samm->z[iel]);
				species_density += layer->species[iel] / atomic_density(samm->z[iel]);
			}
			if (matrix_sum <= 0.0 || matrix_density <= 0.0) {
				matrix_density = 0.4997f;							/* Just choose Si    */
			} else {
				matrix_density = matrix_density / matrix_sum;	/* Average inverse density */
				matrix_density = (1.0f/matrix_density) / 1E23f;	/* Now 1E23 at/cm^3 */
			}
			if (species_sum > 0) {
				species_density = species_density / species_sum;	/* Average inverse density */
				species_density = (1.0f/species_density) / 1E23f;	/* Now 1E23 at/cm^3 */
			} else {
				species_sum = species_density = 1.0;
			}

		} else {
			for (iel=0; iel<sample->nel; iel++) {
				matrix_sum      += layer->matrix[iel];			/* Matrix sum			*/
				species_sum     += layer->species[iel];		/* Species sum			*/
				matrix_density  += layer->matrix[iel]*atomic_density(samm->z[iel]);
				species_density += layer->species[iel]*atomic_density(samm->z[iel]);
			}

			if (matrix_sum <= 0.0 || matrix_density <= 0.0) {
				matrix_density = 0.4997f;							/* Just choose Si    */
			} else {
				matrix_density  =  matrix_density/matrix_sum/1E23f;	 /* 1E23 at/cm^3 */
			}
			if (species_sum > 0) {
				species_density = species_density/species_sum/1E23f;/* 1E23 at/cm^3 */
			} else {
				species_sum = species_density = 1.0f;
			}
		}

/* Determine conversion from user thickness to atoms/cm^2 (thick_to_cm2). */
		thick_to_cm2 = 
			SimThickConvert(layer->thick.units, matrix_density, matrix_sum, &matrix_density);

		if (layer->eqn != NULL && layer->eqn_units != NULL) {
			sthick_to_cm2 =
			SimThickConvert(layer->eqn_units, species_density, species_sum, &species_density);
		} else {
			sthick_to_cm2 = 1;
		}
			
/* ---------------------------------------------------------------------------
-- Troll out the "FUZZ" feature - modify Thickness per iteration
--
-- This involves using a different thickness each time through (iteration
-- count above) and then summing with a weighting factor.  Implemented as a
-- gaussian weighting of thicknesses.
--------------------------------------------------------------------------- */
		user_thick = layer->thick.magn;
		runner = iteration;
		for (tmp=sample->first; tmp!=NULL; tmp=tmp->next) {
			if (tmp->fuzzs == 0) continue;						/* Nothing here */
			if (layer == tmp) {										/* Handling this layer? */
				nf = layer->fuzzs;
				kf = runner%nf;
				jf = min(kf+1,nf-kf);
				mf = nf/2;
				if (2*mf == nf)   {
					size = .5f/(mf*mf);
				} else {
					size = 1.f/(2*mf*mf+nf);
				}
				y = (jf*(jf-1)+.5f) * size;
				x = (REAL) ndtri( y );
				if (kf >= mf) x = -x;
				samm->ampl = (2*jf-1) * size * samm->ampl;
				user_thick = user_thick + layer->fuzzd * x * .7071067f;
			}
			runner = runner / tmp->fuzzs;
		}

/* Decide if anything more to do on this layer */
		if (user_thick <= 0) continue;					/* Go to next layer */
  
/* Troll out the number of sublayers */
		cm2_thick = user_thick*thick_to_cm2;			/* Convert to 1E15/cm2	*/
		cm_thick  = cm2_thick/matrix_density/1E8f;	/* Into cm units */
		if (layer->num_sublayers != 0) {					/* Explicit # sublayers	*/
			num_sublayers = layer->num_sublayers;
		} else if (layer->thisub.magn != 0)   {		/* Explicit # maxpath	*/
			num_sublayers = (int) (1 +
				cm2_thick / (layer->thisub.magn*SimThickConvert(layer->thisub.units, matrix_density, matrix_sum, NULL)));
		} else if (layer->eqn != NULL) {
			num_sublayers = layer->eqn->rcmd_sublayers;
		} else {												/* Default maxpath		*/
			num_sublayers = (int) (1 + cm2_thick/maxpath);
		}
  
/* ... Set up constants for dealing with diffusion equations */
		if (layer->eqn != NULL) {
			switch(layer->eqn->type) {
				case EQ_CONST:					/* Constant Equation */
					c0 = layer->par[0];
					c1 = 0;
					break;
				case EQ_LINEAR:				/* LINEAR    Linear Distribution */
					c0 = layer->par[0];	
					c1 = layer->par[1]-layer->par[0];
					break;

				case EQ_ERFC:					/* ERFC - constant source diffusion */
					c0 = layer->par[0];
					x0 = 0;
					fourDt = (REAL) sqrt(4*layer->par[1]*layer->par[2]);	/* sqrt(4Dt) (cm) */
					if (c0 < 0) {c0=-c0; dir=REVERSE;}
					break;
				case EQ_SEMI_INF:				/* SEMI-INF  Semi-Infinite Solids */
					c0 = layer->par[0]/2.0f;										/* Normalization */
					x0 = layer->par[3]*1E-8f;										/* Becomes cm */
					if (x0 < 1E-10) x0 = x0*1E8f;									/* Was given in cm */
					fourDt = (REAL) sqrt(4*layer->par[1]*layer->par[2]);	/* sqrt(4Dt) (cm) */
					if (c0 < 0) {c0=-c0; dir=REVERSE;}
					break;
				case EQ_EXP:					/* EXPONENT  Exponential Function */
					c0 = layer->par[0];
					D_over_v = (layer->par[1]/layer->par[2]);	/* D/v (cm)	*/
					if (c0 < 0) {c0=-c0; dir=REVERSE;}
					break;

				case EQ_THINFILM:				/* THINFILM  Thin Film Diffusion */
					x0    = 0;											/* Center (cm)	*/
					Dt    = layer->par[1]*layer->par[2];		/* Dt (cm^2)	*/
					sigma = (REAL) sqrt(2*Dt);						/* Sigma (cm)	*/
					dose = layer->par[0]*sthick_to_cm2;			/* in 1E15/cm2	*/
					dose = 2*dose/(cm2_thick/num_sublayers);	/* correct to layer fraction and 2 for half-gaussian */
					integ_frac = 0.5;									/* Starting fraction */
					break;
				case EQ_BURIED:				/* BURIEDFILM Buried thin film	*/
					x0    = layer->par[3]*1E-8f;					/* Becomes cm */
					if (x0 < 1E-10) x0 = x0*1E8f;					/* Was given in cm */
					Dt    = layer->par[1]*layer->par[2];		/* Dt (cm^2)	*/
					sigma = (REAL) sqrt(2*Dt);						/* Sigma (cm)	*/
					dose = layer->par[0]*sthick_to_cm2;			/* in 1E15/cm2	*/
					dose = dose/(cm2_thick/num_sublayers);		/* correct to layer fraction and 2 for half-gaussian */
					integ_frac = (REAL) ndtr(-x0/sigma);		/* Frac before layer starts */
					break;
				case EQ_GAUSS:					/* GAUSSIAN  Gaussian Distribution	*/
					x0    = layer->par[1]*1E-8f;					/* Center (cm)		*/
					sigma = (REAL) (layer->par[2]/sqrt(8*log(2))*1E-8); /* stdv from fwhm */
					dose  = layer->par[0]*sthick_to_cm2;		/* In 1E15/cm2		*/
					dose  = dose/(cm2_thick/num_sublayers);	/* Correct to layer fraction */
					integ_frac = (REAL) ndtr(-x0/sigma);		/* Frac before layer starts */
					break;

				case EQ_EDGEWORTH:
					dose  = layer->par[0]*sthick_to_cm2;		/* In 1E15/cm2		*/
					c0    = (REAL) (dose / layer->par[2] / sqrt(2*RPI));
					c0    = c0 / matrix_density;					/* True fraction	*/
					x0    = layer->par[1]*1E-8f;					/* To cm				*/
					sigma = layer->par[2]*1E-8f;					/* Really std dev	*/
					break;

				case EQ_THICFILM:				/* THICFILM  Thick film equation */
					Dt    = layer->par[2]*layer->par[3];		/* Dt (cm^2)	*/
					sigma = (REAL) sqrt(2*Dt);						/* Sigma (cm)	*/
					c0    = layer->par[0];							/* Initial conc */
					c1    = layer->par[1]-layer->par[0];		/* final conc	*/
					x0    = layer->par[4]*1E-8f;					/* Becomes cm */
					if (x0 < 1E-10) x0 = x0*1E8f;					/* Was given in cm */
					break;

				case EQ_SPLINE:
				{
					REAL x[5]={0.0f, 0.25f, 0.50f, 0.75f, 1.00f};
					spl = GVFitSpline(NULL, x, layer->par, 5, 0);
				}
				break;

/* Don't understand this one but keep for those that do */
				case EQ_TIMEDEPE:				/* TIMEDEPE  Time Dependent Equation */
					c0 = layer->par[0];
					x0 = layer->par[3]*layer->par[3] * layer->par[2] / layer->par[1];
					sigma = (REAL) sqrt(2*x0);
					D_over_v = layer->par[1] / layer->par[3];
					break;

				case EQ_USER:					/* USER - User defined function */
					xtmparray = malloc(sizeof(*xtmparray)*num_sublayers);
					for (i=0; i<num_sublayers; i++) {
						xtmparray[i] = (i+0.5f)/num_sublayers * cm_thick * 1E8f;
					}
					GVLinkArray("P",  0, layer->par, 5, NULL);
					GVLinkArray("X",  0, xtmparray, num_sublayers, NULL);
					GVAllocReal("X0", 0, cm_thick*1E8f);
					if (! GVEvalArrayExpr(xtmparray, num_sublayers, layer->diff_eqn)) {
						ERRprintf("ERROR: Expression %s failed to parse.  Assume 0.\n", layer->diff_eqn);
						for (i=0; i<num_sublayers; i++) xtmparray[i] = 0.0;
					}
					break;
					
				default:
					ERRprintf("ERROR: Mike & Larry were out to lunch: Equation type not recognized\n");
					break;
			}  /* end of switch statement */
		}
			
/* Now we can make the entries in STRUCT */
/* Watch for first real layer but also deal with FRES only mode */
		if (ilayer == sample->absorber_layers+1) samm->fsurf = samm->num_layers;
		if (ilayer <= sample->absorber_layers && sample->fres_only_absorber &&
			ALTBUF->type != FRES) continue;

/* ... Increase number we use, and allocate more space if necessary */
		samm->num_layers += num_sublayers;
		while (samm->num_layers > samm->num_layer_allocated-2) {
			samm->num_layer_allocated += 100;
			samm->layer = realloc(samm->layer, samm->num_layer_allocated * sizeof(SIMLAYER));
			TTYprintf("MSG: Increased SIM sublayer limit to %d\n", samm->num_layer_allocated);
		}

/* Check if this layer starts counting for any of the global diffusants */
		if (ilayer > sample->absorber_layers) {		/* Track sample only		*/
			for (gtmp=samm->glofirst; gtmp!=NULL; gtmp=gtmp->next) {
				if (gtmp->start_cm2 < 0 && ilayer >= gtmp->start)	gtmp->start_cm2 = back_depth_cm2;
			}
		}

/* ... Loop over the sublayers creating entries in final structure */
		numerrs = 0;										/* Errors on a given layer		*/
		for (isublayer=0; isublayer<num_sublayers; isublayer++) {

			if (ilayer > sample->absorber_layers) {		/* Track sample only		*/
				front_depth_cm2 = back_depth_cm2;			/* Keep track of depth	*/
				back_depth_cm2 += cm2_thick/num_sublayers; /* Will be in /cm2		*/
			}

			if (layer->eqn == NULL) {
				frac_species = 0.0;							/* No species fraction */
			} else {
				x = (isublayer+0.5f)/num_sublayers;		/* Fractional posn	*/
				if (dir == REVERSE) x = 1.0f-x;			/* Reverse sense		*/
				xx = x*cm_thick;								/* Distance in cm		*/

				switch(layer->eqn->type) {
	
					case EQ_NONE:					/* (lint) clean */
						break;
					case EQ_CONST:					/* CONSTANT  Constant Equation */
					case EQ_LINEAR:				/* LINEAR    Linear Distribution */
						frac_species = c0 + c1*x;
						break;
	
					case EQ_ERFC:					/* ERFC - constant source diffusion */
					case EQ_SEMI_INF:				/* SEMI-INF  Semi-Infinite Solids */
						frac_species = (REAL) (c0*erfc((xx-x0)/fourDt));	/* C0*erfc(x/4Dt) */
						break;
	
					case EQ_EXP:					/* EXPONENT  Exponential Function */
						frac_species = (REAL) (c0*exp(-xx/D_over_v));			/* C0*exp[-x/(D/v)] */
						break;
	
					case EQ_THICFILM:				/* THICFILM  Thick film equation */
						frac_species = (REAL) (c0 + c1*(2-ndtr((x0-xx)/sigma)-ndtr((x0+xx)/sigma)));
						break;
	
/* Thin film and gaussian are equivalent and use the integral form ndtr(x) */
					case EQ_THINFILM:				/* THINFILM  Thin Film Diffusion */
					case EQ_BURIED:				/* BURIEDFILM Buried thin film	*/
					case EQ_GAUSS:					/* GAUSSIAN  Gaussian Distribution	*/
						xx = (isublayer+1.0f)/num_sublayers*cm_thick;		/* Back edge */
						k = (xx-x0)/sigma;
						if (k<-5 || (k>5 && integ_frac>0.999)) {frac_species=0.0; break;}
						frac_species = (REAL) (dose*(ndtr(k)-integ_frac));
						integ_frac = (REAL) ndtr(k);				/* Keep for next time */
						break;
	
/* Edgeworth has no integral expression, so must use many sublayers */
					case EQ_EDGEWORTH:
						k  = (xx-x0) / sigma;
						frac_species = (fabs(k) > 5) ? 0.0f :
							 (REAL) (c0 * exp(-k*k/2) * (1 + layer->par[3]/6*(k*(k*k-3)) +
							 layer->par[4]/24*((k*k-6)*k*k+3) +
							 layer->par[3]*layer->par[3]/72*(((k*k-15)*k*k+45)*k*k-15) ));
						if (frac_species < 0) frac_species = 0.0;
						break;

/* Spline fit (has integral form) */
					case EQ_SPLINE:
						frac_species = num_sublayers*GVEvalSplineIntegral(spl, ((REAL) isublayer)/num_sublayers,
															 (isublayer+1.0f)/num_sublayers);
						if (frac_species < 0.0f) frac_species = 0.0f;
						break;
						
/* Don't understand this one but keep for those that do */
					case EQ_TIMEDEPE:				/* TIMEDEPE  Time Dependent Equation */
						k = xx / D_over_v ;
						frac_species  = (REAL) (c0*(exp(-k)*ndtr(-(k-x0)/sigma) + ndtr(-(k+x0)/sigma)));
						break;

					case EQ_USER:					/* USER - User defined function */
						frac_species = xtmparray[isublayer];
						break;
					
				}  /* end of switch statement */
 
					/* But do allow negative for the time being - trap below */
			}

/* Handle the global diffusant */
			inv_density_global = frac_global = 0.0;								/* Assume none */
			for (gtmp=samm->glofirst; gtmp!=NULL; gtmp=gtmp->next) {
				if (gtmp->start_cm2 < 0 || 
					 front_depth_cm2==back_depth_cm2 ||
					 (front_depth_cm2-gtmp->start_cm2) >= gtmp->gxmax ||
					 (back_depth_cm2-gtmp->start_cm2)  <= gtmp->gxmin) {
					gtmp->frac = 0.0;
				} else {
					t1 = max(front_depth_cm2-gtmp->start_cm2, gtmp->gxmin);	/* t1 at least gxmin */
					t2 = min(back_depth_cm2-gtmp->start_cm2,  gtmp->gxmax);	/* t2 not above gxmax */
					gtmp->frac = GVEvalSplineIntegral(gtmp->spl, t1, t2) / (back_depth_cm2-front_depth_cm2);
					if (gtmp->frac < 0.0f) gtmp->frac = 0.0f;
					inv_density_global += gtmp->frac/gtmp->density;
					frac_global += gtmp->frac;
				}
			}

/* And now all of the fractions (matrix, species, global) */
			frac_matrix = 1.0f-frac_species;
			if (frac_species > 1.0f) {numerrs++; frac_species = 1.0f;}		/* Don't allow over 1.0 */
			if (frac_global  > 1.0f) {numerrs++; frac_global  = 1.0f;}		/* But do allow negative */
			frac_matrix  *= (1.0f-frac_global);
			frac_species *= (1.0f-frac_global);
			DBUG(printf("ilayer=%d, isublayer=%d, i=%d, matrix,species,global = %f %f %f\n",ilayer, isublayer, i, frac_matrix, frac_species, frac_global);)

/* Copy information useful for analysis of the layers later */
			i = samm->num_layers+isublayer-num_sublayers;   /* Real, zero-based sub-layer number */
			samm->layer[i].layer = ilayer;						/* Identify user layer # */
			if (layer->thick.units->type == ABSOLUTE) {
				samm->layer[i].density = layer->thick.units->density;
			} else {
				samm->layer[i].density = 1.0f / ( frac_matrix/matrix_density + frac_species/species_density + inv_density_global);
			}

/* And fill in the tables */
			for (iel=0; iel<sample->nel; iel++) {
				aden = frac_matrix  * layer->matrix[iel]/matrix_sum +
						 frac_species * layer->species[iel]/species_sum;
				for (gtmp=samm->glofirst; gtmp!=NULL; gtmp=gtmp->next) {
					aden += gtmp->frac * gtmp->species[iel]/gtmp->sum;
				}
				if (aden<0) {numerrs++; aden=0.0;}				/* Common if frac < 0 */
				samm->layer[i].strct[iel] = aden*cm2_thick/num_sublayers;
				DBUG(if(samm->layer[i].strct[iel]!=0){printf("struct(%d,%d)=%f\n",iel,i,samm->layer[i].strct[iel]);})
			} /*  end of iel loop   */

		}	  /*  end of isublayer loop */
		if (numerrs!=0) ERRprintf("WARNING: Diffusants fractions were clipped %d times in layer %d\n", numerrs, ilayer);
		if (xtmparray != NULL) {
			free(xtmparray); 
			xtmparray = NULL;
			GVDeallocate("X");
		}

	}	  /*  end of ilayer loop */

	if (spl != NULL) free(spl);							/* Free this if created */
	return;
}


/* ===========================================================================
-- SUBROUTINE MAKEIT
--     Subroutine MAKEIT first does the pre-calculations, then makes
--     sure CIDEAL is called appropriately for the isotopes of each
--     element.  High Z is done by using the average
--     atomic weight, Low Z elements get the sum of individual isotopes.
--  Quick: Computes each element's contribution to simulation
--
-- Usage: SimMakeit(SAMPLE *sample, int specific_elem, int specific_layer);
--
-- Inputs: sample         - pointer to sample structure
--         specific_elem  - a specific element to plot or ELEM_INVALID for all
--         specific_layer - a specific layer to plot or LAYER_INVALID for all
--                          (first layer is 1 based on ordering from create)
--     INPUTS:   None
--
--     OUTPUTS:  Parameters in call to CIDEAL
--
--     COMMON BLOCKS:     SAMPLE, ATOMS
--     CALLED FROM:       CREATE
--     CALLS:             PRECAL, CIDEAL
--
=========================================================================== */
static void SimMakeit(SAMPLE *sample, int specific_elem, int specific_layer) {

	int	i, j;
	int	first_layer, last_layer;			/* Which layers to do */
	int	first_elem,  last_elem;				/* Which elements to do */
	ISOTOPE *isoto;

	CalcCpuUsage(0, NULL);						/* Start accumulating CPU time */

	SimPrecal(sample);							/* Calculate entry e-hit values	*/

	if (specific_elem != ELEM_INVALID) {	/* SPLOT of specific element?		*/
		first_elem = specific_elem;
		last_elem  = specific_elem;
	} else {
		first_elem = 0;
		last_elem  = samm->num_elements-1;
	}

/* Check if layer valid - remember user calls 1 first layer, we call it 0 */
	if (specific_layer != LAYER_INVALID) {		/*  SPLOT of specific layer?		*/
		first_layer = last_layer = -1;
		for (i=0; i<samm->num_layers; i++) {
			if (samm->layer[i].layer == specific_layer) {
				if (first_layer == -1) first_layer = i;
				last_layer = i;
			}
		}
		if (first_layer == -1 || first_layer < samm->fsurf) {
			TTYprintf("Layer %d doesn't exist.  SPLOT aborts.\n", specific_layer+1);
			return;
		}
	} else {
		first_layer = samm->fsurf;
		last_layer  = samm->num_layers-1;
	}

  	for (i=first_elem; i<=last_elem; i++) {	/* Loop through elements		*/
		isoto = atomic_data(samm->z[i])->isotop;
		if (samm->m2[i] != 0)   {					/* Explicit isotope -- EASY!	*/
			SimCideal(i, first_layer, last_layer, samm->m2[i], samm->ampl);
		} else if (isoto->mass <= 0) {			/* High Z case -- use average */
			SimCideal(i, first_layer, last_layer, atomic_mass(samm->z[i]), samm->ampl);
		} else {											/* Low Z -- individual masses */
			for (j=0; j<NISOT && isoto->mass>0.0; j++,isoto++) {
				SimCideal(i,first_layer,last_layer,isoto->mass,isoto->fraction*samm->ampl);
			}
		}
	}

	CalcCpuUsage(1, ALTBUF->ltct);
	return;
}

/* ===========================================================================
--  Usage Guide:
--
--  Usage:  void CalcCpuUsage(int key, char *buffer);
--
--  Inputs: key - 0 ==> reset and initialize for timing
--                1 ==> determine timing and write info to buffer
--
--  Output: buffer - filled with text describing timing if key == 1.
--
--  Returns: none
--
--  Description:
--     This routine is used for timing and reporting of the simulation
--     algorithm.  It uses the POSIX clock() routine to determine the total
--     CPU utilization.  The precise meaning may change between systems, but
--     then again, this is just for fun.  Could be deleted with no loss.
--
--  Quick: Real time monitor for simulation process
=========================================================================== */
static void CalcCpuUsage(int key, char *buffer) {

	REAL sec_used;

	if (key == 0) {
		cputime  = clock();
		simflops = 0;
	} else if (buffer != NULL) {
		sec_used = ((REAL) (clock()-cputime)) / CLOCKS_PER_SEC;
		sprintf(buffer, "%d operations done in %.3f seconds", simflops, sec_used);
	}
	return;
}

/* ===========================================================================
--  Quick: Sets outward path stopping powers for given ion
--
-- If inflag is TRUE, coffi will be set for the layers, otherwise coffo.
--
-- Modification: If inflag is TRUE, it will also scale by the channeling
-- factor for this layer.  Thanks to Nastasi.
=========================================================================== */
static void CalcAverageStop(STOPPING_TABLE *table, BOOL inflag) {

	int i, j, iel;
	STOPPING_POWER sum, *stop;

	DBUG(printf("Switching to stopping power table Z=%d m=%f\n",
                                           table->z, table->mass);)

	for (i=0; i<samm->num_layers; i++) {
		for (j=0; j<NDEG; j++) sum.p[j] = 0.0;
		for (iel=0; iel<samm->num_elements; iel++) {
			if (samm->layer[i].strct[iel] != 0) {
				stop = RbsLookupStop(table, samm->z[iel]);
				for (j=0; j<NDEG; j++) 
					sum.p[j] += stop->p[j] * samm->layer[i].strct[iel];
			}
		}
		if (inflag) {
/*			for (j=0; j<NDEG; j++) sum.p[j] *= samm->layer[i].dedx_scale; */
			samm->layer[i].coffi = sum;
		} else {
			samm->layer[i].coffo = sum;
		}
	}

	return;
}


/* ===========================================================================
-- Routine to add statistical noise to the spectrum based on the exact
-- values specified.  Ooh boy - is this fun
--
--
=========================================================================== */
static void AddNoise(SPECTRUM *buf) {

	int i,ix,ix_max;
	double t,lambda,p;

	for (i=0; i<buf->npt; i++) {
		t = (REAL) rand()/RAND_MAX;
		if (buf->counts[i] > 10) {
			p = buf->counts[i] + ndtri(t)*sqrt(buf->counts[i]);
			buf->counts[i] = (REAL) nint(p);
		} else if (buf->counts[i] != 0) {
			lambda = buf->counts[i]+0.5;				/* Expectation value (shifted up so symmetric) */
			ix_max = (int) (4*lambda);
			if (ix_max < 2) ix_max = 2;
			p      = exp(-lambda);						/* probability of zero counts */
			for (ix=0; p<t && ix<ix_max; ix++) p += p*lambda/(ix+1);
			buf->counts[i] = (REAL) ix;
		}
	}
	return;
}

/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE DETRES(FWHM1)
--
--      DETRES convolves the spectrum with a Gaussian.
--     Several approximations are made:
--         The Gaussian is only extended to three sigma, after that
--     it is approximated by zero.
--
--  Quick: Convolutes spectrum by a Gaussian
--
--     INPUTS:   FWHM1    The Full Width at Half Maximum of the detector
--                        resolution simulated (keV)
--               kevch    Energy per channel (keV) (in RUMP common)
--
--     OUTPUTS:  COUNTS   (in RUMP common) modified by the smoothing
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       CREATE
--     CALLS:             NDTR (Normal distribution function from SSP)
--                         (Changed 1/14/85 from using IMSL routine ERF)
--
=========================================================================== */
#define LMAX 100
static void ConvoluteDetector(SPECTRUM *buf) {

	REAL temp[LMAX],gauss[LMAX];
	int lgauss,i,j,k,kmax,imax;
	int warn;
	REAL omeg,gnew,gold,scale,x,term;
	clock_t cp;

/*  COMPUTE THE CONSTANTS IN THE CALCULATION */
	cp  = clock();										/* Track time here */

	if (buf->fwhm <= 0.0) return;
	omeg = buf->fwhm / 2.355f;
	lgauss = (int) ((3.0 * omeg / buf->kevch) + 2);
	scale = buf->kevch / omeg * 0.707107f;

	warn = lgauss > LMAX;
	if (warn) lgauss = LMAX;

/*  NOW COMPUTE THE GAUSSIAN */

	gold = (REAL) ndtr(-0.5*1.41421356*scale);
	gauss[0] = 2 * (0.5f - gold);
	for (j=1; j < lgauss; j++) {
		gnew = (REAL) ndtr(-(j+0.5)*1.41421356*scale);
		gauss[j] = gold - gnew;
		gold = gnew;
	}

	x = gauss[lgauss-1] * 100.0f;
	if (warn)
		TTYprintf("WARNING: Gaussian truncated at %5.2f\n",x);

/*  FINISHED COMPUTING GAUSSIAN, NOW ON TO THE DATA */
	for (k=0; k<lgauss; k++) {	
		temp[k] = buf->counts[k];
		buf->counts[k] = 0;
	}

/*  We have now created just enough space in which to work */
	j = 0;
	for (i=0; i<lgauss-1; i++) {
		x = temp[j];
		temp[j] = buf->counts[i+lgauss];
		buf->counts[i+lgauss] = 0.0;
		if (x != 0.0) {

/*  Enough of the busy work: next add the X weighted by the Gaussian */
/*  into whatever is supposed to recieve it. */

			buf->counts[i] += x * gauss[0];
			if (i != 0) {
				for (k=1; k<=i; k++) {
					term = x * gauss[k];
					buf->counts[i+k] += term;
					buf->counts[i-k] += term;
				}
			}
			for (k=i+1; k<lgauss; k++) 
				buf->counts[i+k] += x * gauss[k];
		}

/*  Advance our ring buffer to match the do loop index */
		j = j + 1;
		if (j >= lgauss) j = 0;
	}

/*  Step two: the middle bulk of the array */
	imax = buf->npt - lgauss;
	for (i=lgauss-1; i<imax; i++) {
		x = temp[j];
		temp[j] = buf->counts[i+lgauss];
		buf->counts[i+lgauss] = 0.0;
		if (x != 0.0) {

/*  Enough of the busy work: next add the X weighted by the Gaussian */
/*  into whatever is supposed to receive it. */

			buf->counts[i] += x * gauss[0];
			for (k=1; k<lgauss; k++) {
				term = x * gauss[k];
				buf->counts[i+k] += term;
				buf->counts[i-k] += term;
			}

/*  Advance our ring buffer to match the do loop index */
		}
		j = j + 1;
		if (j >= lgauss) j = 0;
	}

/*  Step three: the end LGAUSS of the array */
	for (i=imax; i<buf->npt; i++) {
		x = temp[j];
		if (x != 0.0) {

/*  Enough of the busy work: next add the X weighted by the Gaussian */
/*  into whatever is supposed to recieve it. */

      buf->counts[i] += x * gauss[0];
      kmax = buf->npt - i;
		if (kmax >= 2) {
			for (k=1; k<kmax; k++) {
				term = x * gauss[k];
				buf->counts[i+k] += term;
				buf->counts[i-k] += term;
			}
		}
		for (k=kmax; k<lgauss; k++)
			buf->counts[i-k] += x * gauss[k];

/*  Advance our ring buffer to match the do loop index */
		}
		j++;
		if (j >= lgauss) j = 0;
	}

	if (! SimSilent && InFullSimMode) 
		TTYprintf(" fwhm(%.3f) .", ((REAL) (clock()-cp)) / CLOCKS_PER_SEC);
	return;							/* All done!!! */
}


/* ===========================================================================
--  Usage Guide:
--
-- Pileup calculation.  This approximates the pileup as second "peak" occuring
-- on the falling edge of a shaped signal resulting in a jump detected as
-- a new event.  The falling shape is triangular, so each real event has equal
-- probability of adding any energy up to it's value to all higher energy
-- events.
--
-- Algorithm:  sum[j] = weighted sum of all events with at least [j] but below
--                current energy.  Each of these is a potential tail_event[j].
--                Weighting is 1/i so event at i evenly influences all below.
--             pileup[i] = SUM ( real_event[j] + tail_events[i-j] )
--
-- Scaling:  Ideally, we should be able to do this quantitatively knowing only
--           the effective time tau for decay of each event.  Should be 
--           proportional to product of 
--             (1) count rate = (total events) / (charge / current)
--             (2) time-constant for shaping
--           reduced by some factor related to efficiency of the pileup
--           rejection.  Since rejection violates the entire hypothesis, hard
--           to handle, so just ignore.
--
-- Original concept & F77: J.S. Custer, Ph.D Thesis, Cornell University
-- Current implementation: M.O. Thompson, Cornell
--
-- There are possibilities of changing the algorithm to more accurately
-- reflect an exponential shaping with finite peak time.  But there are
-- already too many approximations to make that really significant.
=========================================================================== */
void SimNewPileup(SPECTRUM *buf) {

	REAL *cnts, *tails, *pileup, *ptr;			/* Temporary buffers			*/
	REAL sum, factor, secs, pileup_total, counts_total;
	int i, j, nptvalid, imax, npt, nptmax;
	clock_t cp;

/* If time-constant is zero or negative, don't bother */
	if (buf->tau <= 0 || buf->current <= 0) return;
	
/* Determine # points with valid data (npt), and # after pileup (nptmax) */
	cp  = clock();										/* Track time here */
	cnts   = buf->counts;
	npt    = buf->npt;
	nptmax = max(npt, min(2*npt, buf->nptmax));

	tails  = calloc(npt, sizeof(*tails));		/* Assume calloc() cheap	*/
	pileup = calloc(nptmax, sizeof(*pileup));	/* and also free()		 	*/

/* ===========================================================================
-- Sum to find # of events contributing to tail_events at energy [i]
--
-- There is a cnts[i]/i term.  The 1/i correction accounts for the
-- fact that an event at energy 2E spends only 1/2 as much time at
-- each incremental energy dE contributing to potential pileup
=========================================================================== */
	sum = counts_total = 0;							/* Count for total # events	*/
	for (i=npt-1; i>0; i--) {
		if (sum == 0) nptvalid = i;				/* Highest still zero			*/
		tails[i] = sum;								/* This channel's net tail		*/
		sum += ((REAL) cnts[i])/i;					/* And how must this adds		*/
		counts_total += cnts[i];					/* Need total counts later		*/
	}
	if (sum == 0) {free(tails); free(pileup); return;}

/* ---------------------------------------------------------------------------
-- Now, sum all real_events[j]*tail_events[k] where j+k = energy [i]
--
-- This is the one time-consuming loop since a double loop over the whole
-- beast.  The simplest and straight forward loop definition is
--    for (j=0; j<i,j+i<nptmax; j++) pileup[i+j] += cnts[i]*tails[j];
-- Addition of imax dramatically reduces time on AIX - loop identification
-- apparently.  Although pileup[i+j] should be rolled out of the loop, it
-- must be missed since setting it as a pointer makes a 15% difference also.
--------------------------------------------------------------------------- */
	for (i=0; i<nptvalid; i++) {
		if (cnts[i] == 0) continue;
		imax = min(nptvalid, nptmax-i);          /* Error fixed 5/21/2007 */
		ptr = pileup+i;
		for (j=0; j<imax; j++) *(ptr++) += cnts[i]*tails[j];
	}

/* ---------------------------------------------------------------------------
-- Determine scaling constants.  Need to normalize tails[i] to probability
-- by 1/sum, and then scale pileup by the probability of pileup's.  Do as
-- a single step, and separate loops so can be optimized easily.
--
-- For efficiency, two terms in normalization cancel out.  pileup should be
-- divided by (total # of counts) when tails[] is turned into a probability.
-- However, rate is proportional to total # of counts.  So, total cancels
-- and I can avoid having to sum true total # of counts.
--
-- There is a 2X ambiguity here.  If the shaping is really exponential,
-- then the factor should be multipled by 2 to account for the total
-- area under an exponential versus a triangle of the same timing.  
--------------------------------------------------------------------------- */
	secs   = (buf->q / buf->current) * 1E9f;	/* How many uS for acquire			*/
	factor = (1/secs) * buf->tau;					/* rate*tau = probability			*/
	for (i=0, pileup_total=0; i<nptmax; i++) {
		pileup[i] *= factor;							/* Turn into true counts */
		pileup_total += pileup[i];					/* And total the pileup counts */
	}

/* ---------------------------------------------------------------------------
-- And finally, either add to the existing data, or set to this data.
-- Since pileup counts are really 2 real counts lost, we must scale also
-- the general counts down by that factor.
--------------------------------------------------------------------------- */
	factor = (counts_total-2*pileup_total)/counts_total;		/* Correction factor */
	for (i=0;   i<npt;    i++) cnts[i] = cnts[i]*factor + pileup[i];
	for (i=npt; i<nptmax; i++) cnts[i] = pileup[i];

	buf->npt = nptmax;										/* New number of points			*/
	free(pileup); free(tails);								/* Free temporary memory		*/

	if (! SimSilent && InFullSimMode) 
		TTYprintf(" pileup(%.3f) .", ((REAL) (clock()-cp)) / CLOCKS_PER_SEC);
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--  PILEUP implements a very primitive routine for reproducing the
--  qualitative aspects of pulse pileup.  The background falls
--  to zero at twice the maximum scattered energy, and has an
--  amplitude which increases like the square of the count rate.
--  The value 3.E-11 used in the program scales the result to
--  fit usually within a factor of 2 of the experimentally observed
--  pileup on the Cornell machine.  CURRENT is the beam current in
--  nanoamps.
--
--  Quick: Computes estimate of pulse pileup
--
=========================================================================== */
void SimOldPileup(SPECTRUM *buf) {

/*  ... Normalize back to real counts.  Set q,omega etc as valid */
	int i, npt;
	REAL sum, scaling, *cnts;
	REAL correction;								/* Inverse correction to get back */
	clock_t cp;

	if (buf->current <= 0.0) return;

	cp = clock();									/* Start timing */

	correction = buf->q / buf->cbeam *		/* q initially set to cbeam only	*/
					 buf->omega / buf->corr;	/* Omega had been 1, corr 1		*/

	npt  = buf->npt;
	cnts = buf->counts;

/* Determine the total count rate for pileup */
	sum = 0.0;
	for (i=0; i<npt; i++) sum += cnts[i];

/* Determine a "roughing constant for scaling */
	scaling = sum * buf->current * 3E-11f / correction;

/* Now, sum back across */
	sum = 0.0;
	for (i=npt-1; i>=0; i--) {
		sum += scaling * cnts[i];
		if (2*i+1 >= buf->nptmax) continue;
		if (2*i   < npt) cnts[2*i+0] += sum;
		if (2*i+1 < npt) cnts[2*i+1] += sum;
	}

	buf->npt = min(2*npt-1, buf->nptmax);

	if (! SimSilent && InFullSimMode) 
		TTYprintf(" pileup(%.3f) .", ((REAL) (clock()-cp)) / CLOCKS_PER_SEC);
	return;
}


/* ===========================================================================
--     BOOL FUNCTION PRECAL()
--
--     PRECAL does the first pass on the sample structure.
--     It computes the incident energies for each of the sublayers,
--     stored in the array EHIT.  Finally, it figures out the
--     "Rutherford Integral", integral over the thickness of each
--     sublayer of E**-2.  This is common to the calculation of the
--     area of spectrum for each of the elements in that layer.
--
--  Quick: Pre-calculation phase, before each isotope
--
--     INPUT:    none
--
--     OUTPUTS:  Function value used to indicate possible error.
--               Summation into array COUNTS of the yield of this curve.
--
--     COMMON BLOCKS:     RUMP, SAMPLE
--     CALLED FROM:       MAKEIT
--     CALLS:             None
--
--  ... July 1988 - MOT
--      Added code from George Amsel to deal w/ simulations of He3 and Deteurium
--      Now has a "scaling" code to allow energy to be scaled by a constant in
--      determining the stopping power.  This should work for He3 which behaves
--      as if the energy were 4/3 higher.
=========================================================================== */
static void SimPrecal(SAMPLE *sample) {

	STOPPING_POWER *stop;
	REAL ee, p0, p1, p2, de, secin, e1, scon, stot, tem;
	int lay, iel;

/*  fill in coffo and coffi */
	CalcAverageStop(samm->pi, FALSE);			/* Second time, outgoing path	*/
	CalcAverageStop(samm->pi, TRUE);				/* Second time, ingoing path	*/
   samm->cutoff = 1000 * samm->pi->cutoff;	/* Good first guess (unit=keV) */

/* ... SCON = 4 * pi * (ZPROJ * e**2)**2  Straggling constant */
	scon = (REAL) (sample->straggle * 12.56637e15 * pow(samm->zproj * 1.4398e-10,2));
	stot = 0.0;
	samm->layer[0].strag = 0.0;

/* --------------------------------------------------------------------------
-- Start with energy in RUMP common block.  Have to remember to deal with the
-- ev,keV,MeV differences which introduce a bunch of scalings.  Internally, 
-- SIM uses keV.  However, stopping polynomial returns result in eV.  The
-- correction from eV to keV in stopping power handled by secin scaling
-------------------------------------------------------------------------- */

	ee   = 1000.0f * ALTBUF->e0;				/* Access original E -> keV!		*/
	secin = 1.0E-3f * samm->secin;				/* Hide eV->keV scaling in secin	*/
	samm->layer[samm->fsurf].ehit = ee;		/* First active layer				*/
	DBUG(printf("Ehit(%d) = %f\n",samm->fsurf,samm->layer[samm->fsurf].ehit);)


/* ---------------------------------------------------------------------------
-- Delta E for each layer is computed from parabolic expansion of the stopping
-- power.  COFFI has the coefficients of the power series expansion, multiplied
-- by the layer thickness, so really is just units of eV through layer.  Must
-- be scaled by the geometric layer expansion (secin), which also handles the
-- units conversion.
--------------------------------------------------------------------------- */
	for (lay=samm->fsurf; lay<samm->num_layers; lay++) {
		DBUG(printf("Ehit(%d) = %f\n", lay, samm->layer[lay].ehit);)
		if (ee < CUTOFF) break;					/* Below cutoff, don't continue	*/
		tem = (REAL) S_XFORM(ee*e1_scale);	/* Transform/scale energy here	*/
		stop = &samm->layer[lay].coffi;		/* Stopping power in this layer	*/
		p0 = S_POWER(stop,tem);
		p1 = (REAL) (DS_POWER(stop,tem)  * e1_scale);
		p2 = (REAL) (DDS_POWER(stop,tem) * e1_scale*e1_scale);

		de = secin*p0*(1 - secin*(0.5f*p1 - 0.1666667f*secin*(p1*p1+p0*p2)));
		e1 = ee - de;								/* Determine exit energy			*/

		samm->layer[lay].qq =								/* Rutherford integral	*/
			(0.75f + 0.5f * p0 * secin / (ee*ee) *
			(ee + 0.5f * secin * (p0 - 0.6666667f*ee*p1)) ) / (ee*ee) +
			0.25f  / (e1*e1);

		for (iel=0; iel<samm->num_elements; iel++)	/* Estimate straggling	*/
			  stot += samm->layer[lay].strct[iel]*samm->z[iel];
		samm->layer[lay+1].strag =  scon * stot;

		ee = e1;										/* Hit energy of next layer		*/
		samm->layer[lay+1].ehit = ee;			/* Put into the data structure	*/
	}
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE CIDEAL(ELNO, FROM, TO, MISOT, FISOT)
--     Subroutine CIDEAL computes one isotope's contribution to
--     the backscattering spectrum.
--  Quick: Computes contribution from single isotope
--
--     INPUTS:   ELNO     Element number, index to arrays in SAMPLE
--               FROM     Layer number to start considering
--               TO       Final layer number of interest
--               MISOT    Mass of the isotope examined
--               FISOT    Fraction of the element with this mass
--
--     OUTPUTS:  A series of calls to ANLYZ, which actually makes
--               the entries in the spectrum array
--
--     COMMON BLOCKS:     SAMPLE
--     CALLED FROM:       CREATE
--     CALLS:             EFACT, ANLYZ, FLYOUT, BAKLAY
--
--  ... September 20, 1988 - MOT,LRD
--      Added code from LRD for non-Rutherford cross sections.
=========================================================================== */
static void SimCideal(int elno, int from, int to, REAL misot, REAL fisot) {

	REAL x,  					/*  Ratio projectile/target						*/
			sqirt,				/*  Temporary expressions							*/
			km2,					/*  Kinematic scattering							*/
		   ratde, oldrat,
		   hfront, hback,		/*  Height at front/back layer					*/
			ein, eout,			/*  Incident energy at front/back of layer	*/
		   efront, eback,		/*  Scattered energy at front/back layer		*/
		   area,					/*  Area under this region							*/
		   stragc;				/*  Straggling constant								*/
	REAL rfront, rback, ework, hwork, sigma;
	int part, ok, lay;

	int z_detect;				/*  Z of particle being detected	*/
	REAL m_detect;				/*  Mass of detected particle		*/

	SP sp;						/* Cross section calculation structure			*/

#define BSCALE  6.241507f		/*  Conversion barns/sr * 6.2415 = our funny units (basically 1/q) */
#define	SIGMA(kev) ( BSCALE * (*sp.calc)(kev, &sp) )

#ifdef RESONANCE
	RES_ENTRY *res_list;
	RES_TABLE *res_table;
	int ruther, isr;
	REAL x1, x2, hmid, emid, sigf, sigb;
#endif

	DBUG(printf("CIDEAL: Z = %d    M = %f   amp = %f\n", samm->z[elno], misot, fisot);)
	DBUG(printf("        computing layers %d to %d\n", from, to);)

	sp.z1    = samm->zproj;	  sp.m1 = samm->mproj;	/* Projectile					*/
	sp.z2    = samm->z[elno]; sp.m2 = misot;			/* Target						*/
	sp.phi   = 180-samm->phi;								/* True scattering angle	*/
	sp.cosph = samm->cosph; 
	sp.sinph = samm->sinph;
	sp.kev_max = samm->layer[from].ehit;				/* Maximum energy				*/
	sp.calc   = NULL;											/* No known routine			*/

	x = (REAL) (sp.m1/sp.m2);								/* Mass ratio - projectile/target */

	samm->po = NULL;											/* No specific exit stopping power table */
	for (part=1; part<=2; part++) {						/* Do scattering, recoil cases */

/* ---------------------------------------------------------------------
--   (e**2/4)**2 =  (1.4398e-10 keV-cm / 4)**2 * 1.e15 "/cm2"/cm2 *
--        1.e-3 msr/sr / 1.60206e-13 uC/event
--      = 8.088e3 keV**2*events/uC/msr/"/cm2"
--
--   csigma is the 1/E^2 constant in the cross section d\sigma/d\omega
--   csig_0  is the constant in the cross section
--   csig_f  is a low energy rolloff approximation:
--      sigma(E) = (csig_0 + csigma/E^2)*(1-csig_f/E)  where E is in keV
----------------------------------------------------------------------- */
		if (part == 1) {							/*  Do non-recoil case first */
			DBUG(printf("=====Scattering case===== x = %f %f %f\n", x, samm->cosph, samm->sinph);)
			if (x >= 1 && 							/* Target lighter than beam? */
				 ( (samm->cosph < 0) ||			/* Scattering totally impossible */
				   ((1 - pow((x*samm->sinph),2)) <= 0) ) ) goto NextParticle;

			z_detect = samm->zproj;				/* Detected is incident particle */
			m_detect = samm->mproj;

			/* Calculate the kinematic scattering factor */
			sqirt = (REAL) sqrt(1-pow((x*samm->sinph),2));			/* Expression in do/dW	*/
			km2 = (REAL) pow(((sqirt+x*samm->cosph)/(1+x)),2);		/* Kinematic factor		*/
			stragc = samm->secin*km2 + samm->secout;					/* Straggling constant?	*/

			e2_scale = e1_scale;									/* Backscatter case */

			/* Set up for calculation cross sections below */
			SetupSigmaScatter(&sp);								/* Do cross section setup */

		} else {
			DBUG(printf("=====Recoil case=====\n");)
			if (samm->cosph <= 0) goto NextParticle;
			if (samm->z[elno] > RbsRecoilZLimit) goto NextParticle;	/* Limit handle of forward */
			if ( (samm->po = RbsStpfind(samm->z[elno], misot, &e2_scale, ALTBUF->e0)) == NULL) goto NextParticle;

			z_detect = samm->z[elno];			/* Detected is target particle */
			m_detect = misot;						/* Mass is of the isotope		 */

			/* Calculate the kinematic scattering factor */
			km2 = 4 * samm->cosph * samm->cosph / (2+x+1/x);
			stragc = samm->secin*km2 + samm->secout;		/*  Straggling constant?	*/

			/* Set up for calculation cross sections below */
			SetupSigmaRecoil(&sp);								/* Do cross section setup */

			/* Calculate the stopping powers for each sub-layer on outgoing path */
			CalcAverageStop(samm->po, FALSE);
		}

		DBUG(printf("output beam %p, Km2=%f, sigma = (%f+%f*E^2)*(1-%f/E)\n",
						samm->po, km2, sp.csig_0, sp.csigma, sp.csig_f);)

/* ---------------------------------------------------------------------
--  Cross section is now defined as (CSIG_0+CSIGMA/EHIT**2)(1-CSIG_F/E)
--  where EHIT is in keV and SIGMA is in Counts/uCoulomb*mSteradian*(1.E15*Atoms/cm2)
--
--  KM2 is the Newtonian kinematic factor, unitless
--
--  SAMM->SECIN and SAMM->SECOUT are the secants of the incident and exit
--     beams to sample normal, respectively. (unitless)
--
--  EHIT(I) is the energy of the unscattered beam as it makes its way
--     through the sample, and as I goes from 1 to NUM_LAYERS (units are keV)
--
--  RATDE is the ratio of differential energies from where the beam
--     was scattered to where it is detected, i.e., at the surface.
--     important because of eqn. (3.50), suitably generalized for
--     multiple elements (eqn. (3.76)) and multiple layers.
----------------------------------------------------------------------- */

#ifdef RESONANCE
/* -------------------------------------------------------------------------
-- Figure out where the cross section table starts (if at all)
-- Track down and look at all linked entries
--  The following check needs updating: differentiate between
--  Recoil scattering and backscattering cases
--  Used to watch sin(scattering angle), but this is ambiguous between
--  forward and backscattering.  cos(scattering angle) is not.  6/17/89 LRD
--
-- Resonance data have sigma in units of barns, internal is a litle strange.
--     (events/uC/msr/"/cm2")/(barn/sr) 
--     1E15 "/cm2"/cm2 * 1E-3 msr/sr / 1.60206e-13 uC/event * 1.e-24] cm2/barn
---------------------------------------------------------------------------*/
		res_list = NULL;									/* No resonance entries */
		res_table = reschk[samm->z[elno]];			/* First linked table	*/
		ruther = (res_table == NULL);					/* Go with Rutherford? */
		DBUG(printf("cideal: res_table= %p  z2s= %d\n", res_table, (res_table!=NULL) ? res_table->z2 : 0);)

/* WARNING - table will match on phi either at angle or 180-angle - MOT bad choice as graduate student */
		if (! ruther) {
			while (res_table != NULL) {				/* Search all tables */
				if (	(res_table->z1 == samm->zproj) &&
						(fabs(res_table->m1-samm->mproj) < 0.2) &&
						(fabs(res_table->m2-misot) < 0.2) &&
						(fabs(COSD(res_table->phi)-fabs(samm->cosph)) < 0.01) ) break;
				res_table = res_table->next;
			}

			if (res_table != NULL) {					/* Still have a table? */
				res_list = res_table->fit;
				DBUG(TTYprintf("energ =%f  sigma=%f\n",res_list[0].kev,res_list[0].sigma);)
				if (! res_table->checked) {			/* Test Rutherford low limit */
					x1 = res_list[0].sigma;
					x2 = (REAL) (SIGMA(res_list[0].kev) / BSCALE);
					if (res_table->mode == M_RELATIVE) x1 *= x2;
					if (fabs(x1-x2) > .02*x2) { 
						ERRprintf("\n"
					"WARNING: Resonance data file: %s\n"
					"         Cross section at lower limit of table does not approach Rutherford.\n"
               "         Table at %6.1f keV: %.6f     Rutherford:  %.6f\n",
							res_table->pathname, res_list[0].kev, x1, x2);
					}
					res_table->checked = TRUE;
				}
			}
		}

/*  Figure out at what energy to start */
/*  isr is set so res_list[isr-1].kev < ehit <= res_list[isr].kev */
		if (res_list == NULL || (samm->layer[from].ehit < res_list[0].kev) ) {
			ruther = TRUE;
		} else {
			for (isr=1; isr<res_table->npt; isr++) {
				DBUG(printf("isr=%d, energy=%f, ehit(from)=%f \n",
								isr, res_list[isr].kev, samm->layer[from].ehit);)
				if (res_list[isr].kev >= samm->layer[from].ehit) break;
			}
			if (isr == res_table->npt && ! res_table->overrun) {	/* Warn if going over */
				x1 = res_list[isr-1].sigma;						/* Table value */
				x2 = (REAL) (SIGMA(res_list[isr-1].kev) / BSCALE);
				if (res_table->mode == M_RELATIVE) x1 *= x2;
				ERRprintf("\n"
					"WARNING: Resonance data file: %s\n"
					"         Beam energy beyond table.  Rutherford cross sections will be used.\n"
               "         Table at %6.1f keV: %.6f     Rutherford:  %.6f\n",
					res_table->pathname, res_list[isr-1].kev, x1, x2);
				res_table->overrun = TRUE;			/* Warning has been given */
			}
		}

#endif  /* RESONANCE */

		ok = FALSE;									/* EFRONT & RATDE are invalid	*/

/* Okay, start doing the layers now */
		for (lay=from; lay<=to; lay++) {

			if (samm->layer[lay].strct[elno] <= 0) {		/* None in this layer */
				ok = FALSE;											/* EFRONT & RATDE will be stale	*/
				continue;											/* so set flag to so indicate		*/
			}

			ein = samm->layer[lay].ehit;						/* Incident energy into layer	*/
			if (! ok) {												/* Recompute EFRONT & RATDE */
				efront = km2 * ein;
				SimFlyout(lay-1, &efront, &ratde);
				if (efront <= CUTOFF) goto NextParticle;	/* Past cutoff, we are done! */
			}

#ifdef RESONANCE

			if (! ruther) {										/* Keep isr valid so that		*/
				while (isr > 0) {									/* res_list[isr-1].kev < ein < res_list[isr].kev */
					if (ein > res_list[isr-1].kev) break;
					isr--;
				}
				ruther = (isr <= 0);							/* Have we gone off bottom? */
			}
#endif

			rfront = samm->layer[lay].strct[elno] * ratde *
					  (samm->secin/RbsEfact(lay,ein,km2)) * fisot;
			DBUG(printf("Relative height calc: ratde=%f, rfront=%f\n",ratde,rfront);)
#ifdef RESONANCE
			if (ruther || isr >= res_table->npt) {			/* Simple Rutherford?	*/
				sigma = (REAL) SIGMA(ein);						/* Rutherford				*/
			} else {
				sigma = res_list[isr-1].sigma + res_list[isr-1].slope * (ein-res_list[isr-1].kev);
				sigma *= (res_table->mode == M_BARNS) ? BSCALE : (REAL) SIGMA(ein);
			}
#else
			sigma = SIGMA(ein);
#endif /* RESONANCE */

			hfront = sigma * rfront;

/*  Now on to the back edge of the layer.  If the energy is too low to */
/*  be accurate, jump to a separate termination routine. */

			eout  = samm->layer[lay+1].ehit;
			eback = eout * km2;
			oldrat = ratde;
			SimFlyout(lay, &eback, &ratde);

			if (eback < CUTOFF) {					/*  Back edge time?? */
				SimBacklay(z_detect,m_detect, lay, efront, hfront, oldrat, km2);
				goto NextParticle;
			}

			rback = samm->layer[lay].strct[elno] * ratde *
			  (samm->secin/RbsEfact(lay,eout,km2)) * fisot;
			DBUG(printf("Relative height calc: ratde=%f,  rback=%f\n",ratde,rback);)
			ework = efront;
			hwork = hfront;
#ifndef RESONANCE
			sigma = SIGMA(eout);
			hback = sigma * rback;
			area  = (1 - csig_f/eout) * BSCALE * ( sp.csigma * samm->layer[lay].qq + sp.csig_0 ) *
			  			samm->layer[lay].strct[elno] * fisot * samm->secin;
#else
			if (ruther || isr >= res_table->npt)   {
				sigma = (REAL) SIGMA(eout);
				hback = sigma * rback;
				area  = (REAL) ( (1 - sp.csig_f/eout) * ( BSCALE * sp.csigma * samm->layer[lay].qq + BSCALE * sp.csig_0 )
				  * samm->layer[lay].strct[elno] * fisot * samm->secin );
			} else {
				sigf = samm->layer[lay].strag;
				while (TRUE) {										/* Do potential dynamic layers */
					if (eout > res_list[isr-1].kev)	{		/* Same layer?  Then finish now */
						sigma = res_list[isr-1].sigma + res_list[isr-1].slope * (eout-res_list[isr-1].kev);
						sigma *= (res_table->mode == M_BARNS) ? BSCALE : (REAL) SIGMA(eout);
						hback = sigma * rback;
						area  = 0.5f*(ework-eback)*(hwork+hback);
						break;
					}
					isr--;								/* Need to do dynamic layers */
					x1 = (res_list[isr].kev - eout) / (ein - eout);
					DBUG(printf("\nDynamic layer: El %.2f, El+1 %.2f, Ex(isr) %.2f, x1 %.4f\n",
						ein, eout, res_list[isr].kev, x1);)
					emid = eback + (efront-eback)*x1;		/* interpolate scattered energy */
					hmid =  res_list[isr].sigma * (rback+(rfront-rback)*x1);	/*  height */
					hmid *= (res_table->mode == M_BARNS) ? BSCALE : (REAL) SIGMA(res_list[isr].kev);
					area = 0.5f*(ework-emid)*(hwork+hmid);
					DBUG(printf("Dynamic brick: Ef=%8.2f, Eb=%8.2f, Hf=%f, Hb=%f A=%f\n",
						ework,emid,hwork,hmid,area);)
					if (samm->layer[lay+1].strag != 0) {	/*  Interpolation of straggling */
						sigb = samm->layer[lay+1].strag - x1 *
						      (samm->layer[lay+1].strag-samm->layer[lay].strag);
					} else {
						sigb = 0;
					}
					DBUG(printf("Anlyz: %.2f %2.f %f %f %f %f %f\n", ework,emid,hwork,hmid,area,sigf,sigb);)
					(*SimFillSpectrum)(z_detect,m_detect, ework,emid,hwork,hmid,area,
						sigf,sigb);
					ework = emid;
					hwork = hmid;
					sigf  = sigb;
					if (isr == 0) {						/* At end of table */
						sigma = (REAL) SIGMA(eout);
						hback = sigma * rback;
						area  = 0.5f*(ework-eback)*(hwork+hback);
						ruther = TRUE;
						break;
					}
				}
			}
#endif  /* RESONANCE */

			DBUG(printf("Normal  brick: Ef=%8.2f, Eb=%8.2f, Hf=%f, Hb=%f A=%f\n",
							ework,eback,hwork,hback,area);)

			(*SimFillSpectrum)(z_detect,m_detect, ework,eback,hwork,hback,area,
							stragc*samm->layer[lay].strag, stragc*samm->layer[lay+1].strag );

			DBUG(printf("Anlyz: %.2f %2.f %f %f %f %f %f\n", ework,eback,hwork,hback,area,
							stragc*samm->layer[lay].strag, stragc*samm->layer[lay+1].strag );)

/*  Set things up for the next pass: in this, the most efficient
--  case, the emergent energy and differential energy ratio for the
--  front of the next layer have already been computed as the
--  values for the back of this layer.  So transfer and flag the case.
*/
			efront = eback;
			ok = TRUE;
		}

NextParticle:
		if (samm->po != NULL)  CalcAverageStop(samm->pi, FALSE);
		samm->po = NULL;
	}

	return;
}


/* ===========================================================================
--  Usage Guide:
--
--     FLYOUT computes the energy lost by the beam on its way out
--     from the sample. Parameters include the angle of
--     attack and the energy involved. The algorithm involves the
--     computation of the first, second, and third derivatives of
--     energy as a function of depth, and then evaluating the Taylor
--     series to third order.  This is repeated for each layer
--     on the way out.
--
--  Quick: Exit path energy loss computations
--
--     INPUTS:   LAYER    Index to the layer involved
--               EE       Beam energy - modified to exit energy (keV)
--
--     USES:  (from common block SAMPLE)
--               COFF     Stopping power polynomial coefficients
--               SEC      Secant of the angle of traverse
--
--     OUTPUTS:  RATDE    Ratio of a differential energy spread on
--                        the output to the input - involved in
--                        spectrum height calculations.
--
--     COMMON BLOCKS:     SAMPLE
--     CALLED BY:         CIDEAL
--     CALLS:             None
--
=========================================================================== */
static void SimFlyout( int lay, REAL *x_ee, REAL *x_ratde) {

	REAL p0, p1, p2, de, secout, tem, ee, ratde;
	int layer;
	STOPPING_POWER *stop;

	DBUG(printf("Flyout from layer %d starting at %f keV\n",lay,*x_ee);)
	ratde = 1.0;
	ee = *x_ee;
	if (lay < 0) {
		*x_ratde = 1;
		return;
	}
	secout = 1.0E-3f * samm->secout;				/* Hide eV->keV scaling in secout */
	tem = (REAL) S_XFORM(ee*e2_scale);			/*  Copy energy w/ scaling */

	for (layer=lay; layer>=0; layer--) {
		if (ee < CUTOFF) break;
		if (layer < samm->fsurf) secout = 1E-3f;	/*  Absorber foil is not tilted */
		stop = &samm->layer[layer].coffo;
		p0 = S_POWER(stop,tem);
		p1 = (REAL) (DS_POWER(stop,tem) * e2_scale);
		p2 = (REAL) (DDS_POWER(stop,tem) * e2_scale*e2_scale);
		de = (REAL) (secout*p0*(1 - secout*(0.5*p1 - 0.1666667*secout*(p1*p1+p0*p2))));
                         
		DBUG(printf("Through layer %d at energy %7.2f keV: deltaE = %7.2f keV\n",
                 layer,ee,de*1.e-3);)
		ee -= de;
		if (ee < CUTOFF) break;				/*  ADDED CODE 8/2/84 */

/* Now at resulting energy, compute dE/dx for E'/E calculations */
		tem = (REAL) S_XFORM(ee*e2_scale);		/*  Copy energy w/ scaling */
		p1 = S_POWER(stop,tem);
		ratde = ratde * p0 / p1;
	}

	*x_ratde = ratde;
	*x_ee = (ee >= CUTOFF) ? ee : CUTOFF/2.0f;
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      REAL FUNCTION EFACT(LAYER, EE, KM2)
--     Function EFACT computes the "Surface Stopping Cross Section Factor"
--     which is defined in Chu et al., eqn. (3.10)
--  Quick: Computes stopping cross section factor [e]
--
--     INPUTS:   LAYER    Index to the layer of the sample of interest
--               EE       Energy (keV) at which to evaluate Factor
--               KM2      Kinematic factor of isotope
--
--     USES:  (from common block SAMPLE)
--               COFF     Stopping power polynomial coefficients
--               SAMM->SECIN    Secant of incident beam to sample normal
--               SAMM->SECOUT   Secant of exit beam to sample normal
--
--     OUTPUTS:  EFACT    Function result: "[epsilon]"
--
--     COMMON BLOCKS:     SAMPLE
--     CALLED FROM:       CIDEAL
--     CALLS:             None
--
=========================================================================== */
static REAL RbsEfact(int layer, REAL ee, REAL km2) {

	REAL epin, epout, tem, result;
	STOPPING_POWER *stop;

	tem   = (REAL) S_XFORM(ee*e1_scale);					/*  Copy energy w/ scaling */
	stop  = &samm->layer[layer].coffi;
	epin  = S_POWER(stop,tem);
	tem   = (REAL) S_XFORM(ee*e2_scale * km2);			/*  Scattered energy w/ scaling */
	stop  = &samm->layer[layer].coffo;
	epout = S_POWER(stop,tem);
	result = (km2*epin*samm->secin + epout*samm->secout) * 1.e-3f;
	DBUG(printf("Efact=%f for layer %d, E=%f, km2=%f\n",result,layer,ee,km2);)
	return (result);
}


/* ===========================================================================
--  Usage Guide:
--
-- Code for the bottom layer: the back edge has run off the screen, so we need
-- to approximate it down the cutoff energy.   Let NEWEN be the hit energy 
-- which gives CUTOFF for the exit energy.  NEWEN can be approximated (in the
-- limit of a thin final layer) by Ehit(I)-Thick*SAMM->SECIN*epsilon(E), where
-- Thickness we know by (Efront - Cutoff)/[e]    For now, use straight line fit
--
--      SUBROUTINE BAKLAY(LAY,EFRONT,HFRONT,RATDE,KM2)
--  Subroutine BAKLAY is a rather mediocre routine for figuring the
--  back end of the spectrum.  We resort to this program when the
--  exit energy of a brick is below the cutoff energy.  This
--  algorithm uses the front edge only to compute a crude estimate
--  of how fast the signal is rising with decreasing energy.
--  Quick: Back layer estimate
--
--     INPUTS:   EFRONT   Energy of front of brick (keV)
--
--     OUTPUTS:  Summation into array COUNTS of the yield of this curve.
--
--     COMMON BLOCKS:     SAMPLE
--     CALLED FROM:       CIDEAL
--     CALLS:             ANLYZ
--
=========================================================================== */
static void SimBacklay(int z_detect, REAL m_detect, int lay, REAL efront, REAL hfront, REAL ratde, REAL km2){

	REAL epin, efak, area, newrat, stragc, tem;
	REAL ee, eback, hback, eh, hmid, de, qthick;
	STOPPING_POWER *stop;

	eback = CUTOFF;
	de = .3f * (efront - CUTOFF) * ratde;
	efak = RbsEfact(lay,samm->layer[lay].ehit,km2);
	qthick = de / efak;	/*  this is the thickness of a layer */
/*  which is almost guaranteed not to have a back edge below the */
/*  cutoff energy.  Next compute the spectrum height at that point ... */

	ee   = samm->layer[lay].ehit;				/*  Added (believe it or not) 8/2/84 */
	tem  = (REAL) S_XFORM(ee*e1_scale);		/*  Copy energy w/ scaling */
	stop = &samm->layer[lay].coffi;
	epin = S_POWER(stop,tem);
	ee   = km2*samm->layer[lay].ehit - de;
	SimFlyout(lay-1,&ee,&newrat);
	if (ee <= CUTOFF) {	/*  Ignore if back edge of test layer runs off back */
		gen_warn("Extraneous back edge in simulation");
		return;
	}

	eh = samm->layer[lay].ehit - epin*samm->secin*qthick*1.e-3f;
	hmid = (REAL) (hfront * pow(samm->layer[lay].ehit,2) * efak * newrat /
			(ratde * RbsEfact(lay,eh,km2) * eh * eh));

/* ... and extrapolate back to the cutoff energy */
	hback = hfront + (efront - CUTOFF) * (hmid-hfront) / (efront-ee);
	area = .5f * (hback+hfront) * (efront-eback);
	stragc = samm->secin*km2 + samm->secout;
	(*SimFillSpectrum)(z_detect,m_detect, efront,eback,hfront,hback,area,
				stragc*samm->layer[lay].strag, stragc*samm->layer[lay].strag);
	return;
}


#if 0     /* This routine is not used, but is kept around in case  *
           * it is convenient for clarity in any rewrite.  This    *
           * functionality is embedded in precal.                  */
/* ===========================================================================
--  Usage Guide:
--
--      REAL FUNCTION INLOSS(LAY,EE)
--     Computes the energy lost going inward (at angle determined by
--     SAMM->SECIN) through layer LAY, incident energy EE.  The typical call
--     would be E = E - INLOSS(lay,E)
--  Quick: Inward energy loss calculation
--
--     INPUTS:   LAY      Layer number
--               EE       Incident energy (keV)
--
--     OUTPUTS:  INLOSS   Energy lost going through the layer
--
--     COMMON BLOCKS:     SAMPLE
--     CALLED FROM:       Nobody
--     CALLS:             None
--
=========================================================================== */
static REAL SimInloss(int lay, REAL ee) {

	REAL p0, p1, p2, secin, em;
	REAL sec, tem;
	STOPPING_POWER *stop;
	
	secin = 1.0E-3 * samm->secin;				/* Hide eV->keV scaling in secin	*/
	tem = S_XFORM(ee*e1_scale);				/* Transform/scale energy here	*/
	stop = &samm->layer[lay].coffi;			/* Stopping power in this layer	*/
	p0 = S_POWER(stop,tem);
	p1 = DS_POWER(stop,tem)  * e1_scale;
	p2 = DDS_POWER(stop,tem) * e1_scale*e1_scale;

	de = secin*p0*(1 - secin*(0.5*p1 - 0.1666667*secin*(p1*p1+p0*p2)));

	DBUG(printf("through layer %d at energy %6.1f: p0=%f p1=%f p2=%f de=%f\n",
					lay,ee,p0,p1,p2,de);)
	return(de);
}

#endif  /* Inloss */


/* ============================================================================
--  Usage Guide:
--
--      SUBROUTINE GenerateStopFoilTable(void);
--
=========================================================================== */
BOOL GenerateStopFoilTable(int z, int m, REAL e_min, REAL e_inc, int npt) {

	int i;
	STOPPING_POWER *stop;
	SAMPLE *sample;
	REAL ee, p0, p1, p2, de, scon, stot, tem;
	int lay, iel;

/* Check the sample */
	sample = SimDefaultSample;
	if (sample == NULL || sample->first == NULL) {
		ERRprintf("ERROR: No layers defined in sample description\n");
		return(FALSE);
	} 

/* ... Check on samm first */
	if (samm == NULL) {
		samm = calloc(1, sizeof(SAMM));
		samm->num_layer_allocated = 50;
		samm->layer = calloc(samm->num_layer_allocated, sizeof(SIMLAYER));
		samm->glofirst = NULL;
	}

/* ... Copy everything from ibuf to alt, then modify as necessary */
	RbsCopySpectrum(ALTBUF, ibuf);					/* Copy buf to altbuf		 */
	ALTBUF->e0    = (e_min+npt*e_inc) / 1000.0f;	/* Maximum MeV for incident */
	ALTBUF->geom  = CORNELL;
	ALTBUF->theta = 0;
	ALTBUF->phi   = 0;
	ALTBUF->psi   = 0;
	ALTBUF->zbeam = z;
	ALTBUF->mbeam = RbsGetRealMass(z, m);

	if (! FillSimHeader(sample)) return(FALSE);
	FillSimStructure(0, sample);				/* Fill in structure */

/* Part of FillSimHeader/Precal which must be repeated each time */
	samm->zproj = ALTBUF->zbeam;				/* Z of beam			*/
	samm->mproj = ALTBUF->mbeam;				/* Mass of beam		*/
	ee = ALTBUF->e0;								/* Initial energy		*/
	do {
		samm->pi = RbsStpfind(samm->zproj, samm->mproj, &e1_scale, ee);
		if (samm->pi == NULL) {
			ERRprintf("ERROR: Can't create new stopping power table\n");
			return(FALSE);
		}
		CalcAverageStop(samm->pi, FALSE);	/* Sets samm->layer[i].coffo */
		CalcAverageStop(samm->pi, TRUE);		/* Sets samm->layer[i].coffi */
		ee = 1.98f*samm->pi->emin;
	} while (ee > e_min/1000.0);

/* --------------------------------------------------------------------------
-- Start with energy in RUMP common block.  Have to remember to deal with the
-- ev,keV,MeV differences which introduce a bunch of scalings.  Internally, 
-- SIM uses keV.  However, stopping polynomial returns result in eV.  The
-- correction from eV to keV in stopping power handled by secin scaling
-------------------------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		ee = e_min + i*e_inc;					/* Energy in keV			*/

		samm->pi = RbsStpfind(samm->zproj, samm->mproj, &e1_scale, ee/1000.0f);
		if (samm->pi == NULL) {
			ERRprintf("ERROR: Can't get another stopping power table\n");
			return(FALSE);
		}
		CalcAverageStop(samm->pi, FALSE);			/* Sets samm->layer[i].coffo */
		CalcAverageStop(samm->pi, TRUE);				/* Sets samm->layer[i].coffi */
		samm->cutoff = 1000 * samm->pi->cutoff;	/* Good first guess (unit=keV) */

/* ... SCON = 4 * pi * (ZPROJ * e**2)**2  Straggling constant */
		scon = (REAL) (sample->straggle * 12.56637e15 * pow(samm->zproj * 1.4398e-10,2));
		stot = 0.0;									/* For summing straggle	*/

		for (lay=0; lay<samm->num_layers; lay++) {
			if (ee < CUTOFF) {							/* Below cutoff, use CUTOFF dE/dx */
				tem = (REAL) S_XFORM(CUTOFF*e1_scale);
			} else {
				tem = (REAL) S_XFORM(ee*e1_scale);	/* Transform/scale energy here	*/
			}
			stop = &samm->layer[lay].coffi;			/* Stopping power in this layer	*/
			p0 = S_POWER(stop,tem);
			p1 = (REAL) (DS_POWER(stop,tem)  * e1_scale);
			p2 = (REAL) (DDS_POWER(stop,tem) * e1_scale*e1_scale);

			de = 1.0E-3f*p0*(1.0f - 1.0E-3f*(0.5f*p1 - 0.1666667f*1.0E-3f*(p1*p1+p0*p2)));
			ee -= de;									/* Exit energy (keV)					*/

			for (iel=0; iel<samm->num_elements; iel++)	/* Estimate straggling	*/
				stot += samm->layer[lay].strct[iel]*samm->z[iel];
		}
		stot *= scon;									/* Scale up the straggling	 */
		stot = (REAL) sqrt(fabs(stot));			/* And return to real width */

		printf("%.1f  %.1f %.1f\n", e_min+i*e_inc, ee, stot);
	}
	return(TRUE);
}
