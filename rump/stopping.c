/*  stopping.c */
/*  --------------------------------------------------------------------------
-- Routine contains most/all subroutines related to the stopping power
-- lookup and handling.
--------------------------------------------------------------------------- */

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
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "stopping.h"
#include "../genplot/genplot.h"			/* For FitPolynomial */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#define DBUG(x)

/* Stopping power generation exceptions */
typedef struct _STOPEXCEPTION {
	int z1;				/* Incident particle Z						*/
	int m1;				/* AMU (closest) of incident particle	*/
	int z2;				/* Target Z										*/
	char *expr;			/* Mathematical expr for eps(kev)		*/
} STOPEXCEPTION;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static BOOL RbsCalcKalbitzerStop(int z1, REAL m1, int z2, REAL *kev, REAL *spower, int npt);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
int ZieglerDataValid = FALSE;
STOPPING_TABLE	*stop_tables=NULL;			/* Stopping power tables	*/
STOPPING_TYPE   stop_type=STOP_SQRT;		/* Type of stop fit to use	*/

int (*UserZStop)(int z1, double m1, int z2, double m2, REAL *keV, REAL *stop, int npt) = NULL;

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static STOPEXCEPTION *StopExceptionTable=NULL;
static int StopExceptionCount=0, StopExceptionAlloc=0;


/* ===========================================================================
-- RbsLoadStopTable(char *filename);
--
-- Returns:  TRUE  -> successfully loaded
--           FALSE -> some error
=========================================================================== */
BOOL RbsLoadStopTable(char *fname) {

	int i, j, nread, n2, z1,m1, z2, lastchr;
	fpos_t pos;
	FILE *lun;
	char filename[PATH_MAX], inbuf[LONG_STR_SIZE], expr[LONG_STR_SIZE], *aptr, *bptr;
	STOPPING_TABLE *tab;

	SysFindFile(filename, fname, RbsConfigPath, NULL, R_OK);

/* ------------------------------------------------------------------------
-- Check the file header.  Must correspond in number and polynomial order
-- to this code.
------------------------------------------------------------------------ */
	if ( (lun = fopen(filename, "r")) == NULL) {
		ERRprintf("ERROR: %s failed to open\n", filename);
		return(FALSE);
	}

/* ... Scan comments.  Check if an exception file instead of normal stop */
	while (TRUE) {
		fgetpos(lun, &pos);
		if (fgets(inbuf, sizeof(inbuf), lun) == NULL) break;
		aptr = inbuf; while (isspace(*aptr)) aptr++;
		if (*aptr == '#' || *aptr == '\0') continue;
		if (strnicmp(aptr, "EXCEPTION", 9) == 0) goto Stop_Exception_Load;
		fsetpos(lun, &pos);
		break;
	}

/* Handle as a normal file */
	TTYprintf("  Loading stopping powers: %s\n", filename);
	tab = malloc(sizeof(STOPPING_TABLE) + NumElements*sizeof(STOPPING_POWER));
	if (tab == NULL) {
		ERRprintf("ERROR: malloc(stop_table) failed.  This is serious\n");
		return(FALSE);
	}
	tab->nelem = NumElements;				/* Actual # of entries (okay?) */
	for (i=0; i<NumElements; i++) tab->stop[i].p[0] = STOP_INVALID;

/* Read Z, mass, emin,emax and cutoff for this table */
	if (fscanf(lun, "%d %f",    &tab->z, &tab->mass) != 2 ||
		 fscanf(lun, "%f %f %f", &tab->emin, &tab->emax, &tab->cutoff) != 3 || 
		 fscanf(lun, "%d %d",    &nread, &n2) != 2 || n2 != NDEG) {
		ERRprintf("ERROR: Bad headers in stopping power table\n");
		fclose(lun);
		free(tab);
		return(FALSE);
	}
	tab->type = STOP_LINEAR;

	for (i=0; i<nread; i++) {
		for (j=0; j<NDEG; j++) fscanf(lun, "%f ", &tab->stop[i].p[j]);
	}
	fclose(lun);

/* And, now insert the table into the structure */
	tab->next   = stop_tables;		/* Link to front of existing tables	*/
	stop_tables = tab;				/* And place myself as start			*/
	return(TRUE);

/* ===========================================================================
-- Here for the stopping power exception table loading
=========================================================================== */
Stop_Exception_Load:
	
	TTYprintf("  Loading stopping power exceptions: %s\n", filename);

/* Scan the files */
	while (fgets(inbuf, sizeof(inbuf), lun) != NULL) {
		aptr = inbuf; while (isspace(*aptr)) aptr++;
		if (*aptr == '#' || *aptr == '\0') continue;
		z1 = strtol(aptr, &bptr, 10);
		m1 = strtol(bptr, &aptr, 10);
		z2 = strtol(aptr, &bptr, 10);
		if (aptr == bptr) {
			ERRprintf("ERROR: Ill formed stopping power exception: %s\n", inbuf);
			continue;
		}

		aptr = expr;							/* Start copying expression */
		lastchr = '\0';
		while (TRUE) {
			for (; *bptr!='\0'; bptr++) {
				if (!isspace(*bptr)) lastchr = *aptr++ = *bptr;
			}
			if (lastchr!='\\') break;
			aptr--;											/* Read next line */
			if (fgets(inbuf, sizeof(inbuf), lun) == NULL) break;
			bptr = inbuf;
		}
		*aptr = '\0';

/*		TTYprintf("Z1=%d M1=%d Z2=%d EXPR=%s\n", z1,m1,z2,expr); */
		
/* Scan the table as a replacement first */
		for (i=0; i<StopExceptionCount; i++) {
			if (StopExceptionTable[i].z1==z1 && StopExceptionTable[i].m1==m1 && 
				 StopExceptionTable[i].z2==z2) break;
		}
		if (i >= StopExceptionCount) {						/* New Entry */
			i = StopExceptionCount++;
			if (StopExceptionCount >= StopExceptionAlloc) {
				StopExceptionAlloc += 20;
				StopExceptionTable  = realloc(StopExceptionTable, StopExceptionAlloc*sizeof(*StopExceptionTable));
			}
		} else {
			free(StopExceptionTable[i].expr);
		}

		StopExceptionTable[i].z1   = z1;
		StopExceptionTable[i].m1   = m1;
		StopExceptionTable[i].z2   = z2;
		StopExceptionTable[i].expr = strdup(expr);
	}
	TTYprintf("INFO: Total number of exceptions: %d\n", StopExceptionCount);
	fclose(lun);

	return(TRUE);
}


/* ===========================================================================
-- RbsLoadZieglerData(char *filename);
--
-- Returns:  0 -> successfully loaded
--          -1 -> file failed to open
--          -2 -> error in headers or polynomial order
--         -99 -> internal error (malloc or otherwise)
=========================================================================== */
BOOL RbsLoadZieglerData(char *fname) {

	char filename[PATH_MAX];

	SysFindFile(filename, fname, RbsConfigPath, NULL, R_OK);
	TTYprintf("Loading Ziegler data: %s\n", filename);

	if (NumElements < 92) {
		ERRprintf("ERROR: Internal configuration of RUMP unable to handle modified Ziegler\n");
		return(FALSE);
	}

	ZieglerDataValid = zread1(filename);
	return(ZieglerDataValid);
}


/* ===========================================================================
-- Routine to find a STOPPING_TABLE for a given particle.  This table includes
-- all information necessary to get the stopping power of an element.
-- Internal format is hidden however, and routines are encouraged to use
-- only routines to access the table information.
--
-- If no appropriate table exists, one will be automatically started for the
-- particle.  Positions are filled in by calling Ziegler routines as needed.
--
-- Usage: STOPPING_TABLE *RbsStpfind(zb, mb, *e_scale, e_beam)
--
-- Inputs: zb, mb - Z and mass of incident particle
--         e_beam - Energy of the beam (MeV)
--
-- Output: e_scale - scaling needed to multiply stopping powers to get dE/dx
--
-- Returns: Either a pointer to a STOPPING_TABLE, or NULL if none can be
-           found or created.
=========================================================================== */
STOPPING_TABLE *RbsStpfind(int zb, REAL mb, REAL *e_scale, REAL e_beam) {

	STOPPING_TABLE *table, *z_match, *zm_match;
	REAL z_ebest, zm_ebest;
	int j;

/* ---------------------------------------------------------------------------
-- Search algorithm:
--   (1) Reject a table if Z does not match
--   (2) If no table yet, or if better energy match,
--       switch z_match to this table.
--   (3) If mass match also, repeat for zm_match
--
-- Define better energy match as table fitting to lower energy, with maximum
-- E within e_max of table.
--------------------------------------------------------------------------- */
	z_match = zm_match = NULL;
	z_ebest = zm_ebest = REAL_MAX;
	for (table=stop_tables; table!=NULL; table=table->next) {
		if (table->z != zb) continue;							/* Basic validity tests */
		if (table->type != stop_type) continue;			/* Must be correct */
		if (e_beam >   table->emax) continue;				/* Highest must be valid */
		if (e_beam < 2*table->emin) continue;				/* And must have some room */
		if (z_match==NULL || table->emin < z_ebest) {	/* Set Z match	*/
			z_match = table;
			z_ebest = table->emin;
		}

		if (fabs(table->mass-mb) > 0.2) continue;					/* M okay too?	*/
		if (zm_match == NULL || table->emin < zm_ebest) {		/* Set M match	*/
			zm_match = table;
			zm_ebest = table->emin;
		}
	}

	if (zm_match != NULL) {							/* Best - exact match */
		*e_scale = 1.0;
		return(zm_match);
	} else if (z_match != NULL) {					/* OK - simple scaling needed */
		*e_scale = z_match->mass / mb;
		return(z_match);
	} else if (ZieglerDataValid) {
      table = malloc(sizeof(STOPPING_TABLE) + NumElements*sizeof(STOPPING_POWER));
		if (table == NULL) {
			ERRprintf("ERROR: Ouch! - out of memory.  Give me a break\n");
			return(NULL);
		}
		table->next   = stop_tables;	/* Insert new one in link-list */
		stop_tables   = table;
		table->z      = zb;
		table->mass   = mb;
		table->nelem  = NumElements;
/* Need some experience in setting these limits */
		if (stop_type == STOP_LINEAR) {
			table->type   = STOP_LINEAR;
			table->emin   = 0.08f*e_beam;
			table->emax   = 1.15f*e_beam;
			table->cutoff = 0.03f*e_beam;
		} else {
			table->type   = STOP_SQRT;
			table->emin   = 0.04f*e_beam;					/* Can be almost 0 */
			table->emax   = 1.15f*e_beam;					/* Needs to be higher */
			table->cutoff = 0.03f*e_beam;					/* May go negative! */
		}			
		for (j=0; j<NumElements; j++) table->stop[j].p[0] = STOP_INVALID;
		*e_scale = 1.0;              /* by definition */
		TTYprintf("Created new stopping power table: %d %.2f %.3f %.3f\n",
			table->z, table->mass, table->emin, table->emax);
		return(table);
	}

	return(NULL);
}


/* ===========================================================================
-- Routine to create a STOPPING_TABLE from user specified particle and energy
-- range.  This table includes all information necessary to get the stopping
-- power of an element.  Simply adds self into the list, but never does any
-- real calculations.
--
-- Usage: BOOL RbsStpCreate(void)
--
-- Inputs: none
--
-- Output: Creates internal stopping power table
--
-- Returns: TRUE if successful, FALSE if some sort of an abort
=========================================================================== */
BOOL RbsStpCreate(int zb, REAL mb, REAL emin, REAL emax, REAL cutoff) {

	STOPPING_TABLE *table;
	int j;

	if (! ZieglerDataValid) {
		ERRprintf("ERROR: Ziegler data is not loaded - can't dynamically allocate tables\n");
		return(FALSE);
	}

	table = malloc(sizeof(STOPPING_TABLE) + NumElements*sizeof(STOPPING_POWER));
	if (table == NULL) {
		ERRprintf("ERROR: Ouch! - out of memory.  Give me a break\n");
		return(FALSE);
	}

	table->next   = stop_tables;	/* Insert new one in link-list */
	stop_tables   = table;
	table->nelem  = NumElements;
	for (j=0; j<NumElements; j++) table->stop[j].p[0] = STOP_INVALID;

	table->z      = zb;								/* Copy passed parms */
	table->mass   = mb;
	table->emin   = emin;
	table->emax   = emax;
	table->type   = stop_type;
	table->cutoff = cutoff;

	return(TRUE);
}


/* ===========================================================================
--  Usage Guide:
--
--      STOPPING_POWER *RbsLookupStop(STOPPING_TABLE *table, int z)
--
--     Returns pointer to the STOPPING_POWER structure for element z
--     from table.  Will generate the stopping power if it is not already
--     present in the table.
--
--  Quick: Lookup stopping power structure for element z
--
--  Inputs:  table - pointer to a stopping power table structure such as
--                   returned by RbsStpfind()
--           z     - Z of element to lookup within the table
--
--  Returns: Pointer to STOPPING_POWER structure properly initialized
=========================================================================== */
STOPPING_POWER *RbsLookupStop(STOPPING_TABLE *table, int z) {

	STOPPING_POWER *stop;

	stop = &table->stop[z-1];
	if (stop->p[0] == STOP_INVALID) RbsGenStopp(z, table);
	return(stop);
}


/* ===========================================================================
-- Routine to fit a generate the coefficients corresponding to a single
-- element and place them in a STOPPING_TABLE structure.
--
-- Usage: void RbsGenStopp(int elno, STOPPING_TABLE *table);
--
-- Inputs: elno  - z of element to create
--         table - pointer to existing stopping power table structure
--
-- Output: table->stop[i] - filled in with fit corresponding to the
--                          energy range.
--
-- Notes: Input of Ziegler's stopping powers comes via calls to ZSTOP.
=========================================================================== */
void RbsGenStopp (int elno, STOPPING_TABLE *table) {

#define NPLOT 201								/* Number of points in fit */

/*  -- Local variables -- */
	ATOMS *zp;
	int z1, z2, i;
	REAL m1, m2, se, sn, fxmin, fxmax, x, curerr, maxerr;
/*	double sval; */
	REAL d[6];
	REAL kev[NPLOT],							/* Actual kev of stopping power fit */
		  spower[NPLOT],						/* Actual stopping cross section		*/
		  scaled[NPLOT],						/* Scaled energy (linear or sqrt)	*/
		  sigma[NPLOT];						/* for weighting of the fit			*/
	STOPEXCEPTION *UseStopException;

/* Look up the parameters */
	zp = &(atom[elno-1]);
	z1 = table->z;
	m1 = table->mass;
	z2 = zp->z;
	m2 = zp->mass;
	if (z1 == 0) return;

/* Load in a default stopping power as constant in event of errors */
	for (i=0; i<NDEG; i++) table->stop[(zp->z)-1].p[i] = 0;
	table->stop[(zp->z)-1].p[0] = 40.0;

/* Make some quick checks */
	if (! ZieglerDataValid ) {
		ERRputs(" Ooops - can't generate stopping power without Ziegler data loaded.\n");
		return;
	}
	TTYprintf(" Fitting particle %d%-2s  for Z = %2d ... ",	(int) m1, atomic_symbol(z1), z2);

/* Scan the exception table to see if must handle differently */
	UseStopException = NULL;
	for (i=0; i<StopExceptionCount; i++) {
		if (StopExceptionTable[i].z1==z1 &&
			 StopExceptionTable[i].m1==((int) m1) && 
			 StopExceptionTable[i].z2==z2) {
			UseStopException = &StopExceptionTable[i];
			break;
		}
	}
		
/* WARNING: This code must handle the different forms of stopping fits */
	fxmin = table->emin*1000.0f;					/* Work in KeV */
	fxmax = table->emax*1000.0f;
	if (table->type == STOP_SQRT) {				/* Convert to sqrt(E) range */
		fxmin = (REAL) sqrt(fxmin);
		fxmax = (REAL) sqrt(fxmax);
	}
	for (i=0; i<NPLOT; i++) {
		scaled[i] = fxmin + i*(fxmax-fxmin)/(NPLOT-1.0f) ;
		kev[i]    = scaled[i];
		if (table->type == STOP_SQRT) kev[i] = kev[i]*kev[i];
		spower[i] = 40;													/* Default value */
	}

/* Calculate the Ziegler fit to the data range (if possible) */
	if (UserZStop != NULL && (*UserZStop)(z1, m1, z2, m2, kev, spower, NPLOT) == 0) {
		TTYprintf("u");
	} else if (RbsCalcKalbitzerStop(z1, m1, z2, kev, spower, NPLOT)) {
		TTYprintf("k");
	} else if (zcheck(z1, m1, z2) == 0) {							/* Can do! */
		for (i=0; i<NPLOT; i++) {
			zstop(z1, m1, z2, kev[i], &se, &sn, 1);
			spower[i] = se + sn;
		}
		TTYprintf("z");
/* ... Kludge to support ancient Mylar hardcoded into DOS RUMP */
	} else if (z2 == 93 && ((z1 == 1 && nint(m1) <= 2) || (z1 == 2 && nint(m1) == 4)) ) {
		static REAL h_in_my[6]  = {13.37f, -1.969E-2f, +1.551E-5f, -6.592E-9f, +1.426E-12f, -1.229E-16f};
		static REAL d_in_my[6]  = {10.37f, -4.430E-4f, -6.729E-6f, +4.674E-9f, -1.239E-12f, +1.172E-16f};
		static REAL he_in_my[6] = {12.71f, +5.538E-2f, -5.557E-5f, +2.440E-8f, -5.175E-12f, +4.274E-16f};
		REAL *p;
		p = (z1 == 1) ? ((nint(m1) == 1) ? h_in_my : d_in_my) : he_in_my;
		for (i=0; i<NPLOT; i++) {
			spower[i] = ((((p[5] *kev[i] + p[4])*kev[i] + p[3])*kev[i] +
							    p[2])*kev[i] + p[1])*kev[i] + p[0];
		}
		TTYprintf("m");
/* ... If no exception, then real problem */
	} else if (UseStopException == NULL) {
		ERRprintf("\nERROR: Ziegler calc can't handle and no exception stopping power registered\n");
		return;
	}

/* If stopping power exception was located, modify the values */
	if (UseStopException != NULL) {
		GVLinkArray("kev", GVF_USER, kev,    NPLOT, NULL);
		GVLinkArray("zgl", GVF_USER, spower, NPLOT, NULL);
		GVEvalArrayExpr(spower, NPLOT, UseStopException->expr);
		TTYprintf("x");
		GVDeallocate("kev");
		GVDeallocate("zgl");
	}

/* Create a "sigma" on data points to weight as 1/Y */
	for (i=0; i<NPLOT; i++) {
		sigma[i] = (spower[i]!=0.0f) ? (REAL) sqrt(fabs(spower[i])) : 1.0f ;
	}
	TTYprintf("s");

/* Now do the actual fit to get polynomial coefficients d */
/*	legcof(scaled, spower, NPLOT, NDEG, d); */
#ifdef DEBUG_STOPPING
	{
		FILE *lun;
		char file[PATH_MAX];
		sprintf(file, "%d_%d_%d.raw", z1, (int) m1, z2);
		lun = fopen(file, "w");
		for (i=0; i<NPLOT; i++)
			fprintf(lun, "%g %g %g %g\n", scaled[i], kev[i], spower[i], sigma[i]);
		fclose(lun);
	}
#endif
	FitPolynomial(scaled,spower, sigma, NPLOT, NDEG-1, NULL, d);
	TTYprintf("f");

/* Output the data to a file */
#ifdef DEBUG_STOPPING
	{
		FILE *lun;
		char file[PATH_MAX];
		sprintf(file, "%d_%d_%d.fit", z1, (int) m1, z2);
		lun = fopen(file, "w");
		for (i=0; i<NPLOT; i++) {
			x = scaled[i];
			x = ((((((d[5])*x+d[4])*x+d[3])*x+d[2])*x+d[1])*x+d[0]);
			fprintf(lun, "%g %g %g %g\n", scaled[i], kev[i], spower[i], x);
		}
		fclose(lun);
	}
#endif

/* Calculate maximum deviation on all but first/last 5 points */
	maxerr = 0;
	for (i=5; i<NPLOT-5; i++) {
		x = scaled[i];
		x = ((((((d[5])*x+d[4])*x+d[3])*x+d[2])*x+d[1])*x+d[0]);
		curerr = (REAL) fabs((spower[i]-x)/spower[i]);
		if (curerr > maxerr) maxerr = curerr;
	}
	TTYprintf(" ... max error: %5.2f%%\n", maxerr*100.0);

/* Put coefficients into place */
	for (i=0; i<NDEG; i++) table->stop[(zp->z)-1].p[i] = d[i];
	return;
}


#ifdef UNUSED_CODE

/* ===========================================================================
-- The legendre coefficient code was previously used to fit the stopping
-- power data and determine the appropriate coefficients for the polynomial
-- form.  On closer examination, this fit does very poorly at the high energy
-- end of the data -- not exactly sure why -- and fails miserably for some
-- values.  Rather than try to find the problem just switched to using the
-- polynomial fit code from GENPLOT which is known to be stable and work
-- well over a wide range of conditions.
--
-- MOT 3/24/96
=========================================================================== */

static void legcof( REAL *xx, REAL *yy, int nbre, int order, REAL *coeff);
static REAL comb( int kk, int n);

/* ===========================================================================
=========================================================================== */
static void legcof( REAL *xx, REAL *yy, int nbre, int order, REAL *coeff){

#define ORDER  NDEG
#define MAXLEN NPLOT

/*     Programme calculant le polynome de Legendre associe a une courbe */
/*     representee par les tableaux (XX,YY). */

/*     Argument(s) : */
/*      .XX      < E >  :  Tableau des abscisses de la courbe. */
/*      .YY      < E >  :  Tableau des ordonnees de la courbe. */
/*      .NBRE    < E >  :  Nombre de points de la courbe, <=500. */
/*      .COEFF   < S >  :  Tableau des coefficients du polynome. */

/*  Mangled by Patrick Serrafero on or about 09.05.85 */
/*                                   (probably 5/9/85, given crazy Europe) */
/*  Reformed by Larry Doolittle  10/9/86 */

/*  -- Local variables -- */
	int      i, j;
	REAL    e_min, e_max;
	REAL    s[ORDER], b[ORDER];
	REAL    x, m, v;
	double  sum;        /* EXTEND MY PRECISION! */
	static  int oldnum=0;
	static  REAL p[ORDER][MAXLEN];
	static  REAL norm;

/*  -- Code begin -- */

/*  Start by creating Legendre polynomials in arrays */

/*  Recurrence relation for Legendre polynomials is */

/*   (n+1)Pn+1 = (2n+1)xPn + nPn-1 */

/*  Since the first Legendre polynomial has index 0, our */
/*  numbering system is off by 1. */

	if (nbre > MAXLEN)   {
		ERRputs("Data too long for Legendre fit. CRASH!\n");
		return;
	}

/*  If this is the first time through with this size array, */
/*  need to generate the P array ( Legendre Polynomials evaluated across */
/*  the span from -1 to 1).  Otherwise, leave well enough alone. */

	if (oldnum != nbre)   {
		DBUG(TTYprintf("Computing %dth order Legendre polynomials for %d entries.\n",ORDER,nbre);)
		norm = 1./((REAL)(nbre-1));
		for (i=0; i < nbre ; i++ ) {
			x = 2*((REAL)(i))*norm - 1.;
			p[0][i] = 1.;
			p[1][i] = x;
			for (j=2; j < ORDER; j++) {
				p[j][i] = ((2*j-1)*x*p[j-1][i] - (j-1)*p[j-2][i])/(j);
			}
		}
		oldnum = nbre;
	}

/*  Integrate (F,FN) to get coefficients for Legendre */

	for (j=0; j < ORDER ; j++ ) {
		sum = 0.5 * (yy[1]*p[j][0] + yy[nbre]*p[j][nbre-1]);
		for (i=1; i < nbre-1 ; i++ ) {
			sum = sum + yy[i]*p[j][i];
		}
		s[j] = sum * (2*(j)+1) * norm;
	}

/*  Map Legendre coefficients onto simple polynomial coeff. */

	b[0] = s[0] -  .5*s[2] +  .375*s[4];
	b[1] = s[1] - 1.5*s[3] + 1.875*s[5];
	b[2] =        1.5*s[2] - 3.75 *s[4];
	b[3] =        2.5*s[3] - 8.75 *s[5];
	b[4] =                   4.375*s[4];
	b[5] =                   7.875*s[5];
	
/*  Convert B (defined relative to -1<x<1) */
/*       to D (defined relative to EMIN<E<EMAX) */

	e_min = xx[0];
	e_max = xx[nbre-1];
	m = 2./(e_max-e_min);
	v = -(e_max+e_min)/(e_max-e_min);
	for (i=0; i < ORDER ; i++ ) {
		coeff[i] = 0.;
		for (j=i; j < ORDER ; j++ ) {
/*      DBUG(TTYprintf("comb(%d,%d) = %f\n",i,j,comb(i,j));) */
			coeff[i] = coeff[i] + b[j]*comb(i,j)*pow(m,i)*pow(v,(j-i));
		}
		DBUG(TTYprintf("s[%d] = %f b[%d] = %f   coeff[%d] = %f\n",
			i,  s[i],  i,  b[i],        i, coeff[i]);)
	}
	return;
}

/* ===========================================================================
--  COMB  -- COMBINATIONS CALCULATOR: COMPUTES (K,N),
--           THE NUMBER OF WAYS TO CHOOSE K OBJECTS
--           OUT OF A SET OF N OBJECTS.
--
--           COMB IS A REAL FUNCTION
--           K AND N ARE INTEGER UNMODIFIED
--               CALLER DEFINED PARAMETERS
--
--           MATHEMATICAL FORMULA :  COMB(K,N) = N!/K!(N-K)!
--
--           CALCULATION DONE ITERATIVELY, WITHOUT THE OVERFLOW
--           PROBLEMS OF DOING THE FACTORIALS INDIVIDUALLY
--
--           12/7/82  LARRY DOOLITTLE
--           11/24/94 rewritten MOT
--
--  formula of interest:
--     comb(k,n) = n . n-1 . n-2 . . . n-k+1 / 1 . 2 . 3 . . . k
--     comb(k,n) = comb(n-k,n)
=========================================================================== */
static REAL comb(int k, int n) {

	REAL rval;

	if (2*k > n)  k=n-k;								/* Use symmetry of fnc */
	if (k < 0) return(0);							/* Invalid function */

	for (rval=1.0; k>0; n--,k--) rval = (rval * n) / k;
	return(rval);
}

#endif


/* ===========================================================================
-- Routines to implement the Kalbitzer stopping power formalism
=========================================================================== */
typedef struct _KONAC {
	int z1,m1,z2;									/* Which one does it apply to? */
	double scaling;
	double s, a0,a1,a2,a3,a4,a5, beta;		/* Konac coefficients */
} KONAC;

static int KalTableAlloc=0, KalTableSize=0;
static KONAC *KalTable=NULL;
static char KalFilename[PATH_MAX]="";

/* ===========================================================================
-- RbsLoadKalbitzerData(char *filename);
--
-- Returns:  TRUE -> successfully loaded
=========================================================================== */
BOOL RbsLoadKalbitzerData(char *fname) {

	char aline[256], *aptr;
	FILE *funit;
	KONAC *ent;

/* Delete existing table already allocated */
	if (KalTable != NULL) free(KalTable);
	KalTable = NULL;
	KalTableAlloc = KalTableSize = 0;					/* Entries in table */
	*KalFilename = '\0';

	if (fname == NULL) return(TRUE);						/* Is this just request to unload? */

	SysFindFile(KalFilename, fname, RbsConfigPath, NULL, R_OK);
	if ( (funit = fopen(KalFilename, "r")) == NULL) {
		ERRprintf("WARNING: No Kalbitzer format stopping power data file found (%s)\n", KalFilename);
		*KalFilename = '\0';
		return(FALSE);
	}

/* Read entries */
	while (fgets(aline, sizeof(aline), funit) != NULL) {
		while ( (aptr = strchr(aline, '\n')) != NULL) *aptr = '\0';
		aptr = aline; while (isspace(*aptr)) aptr++;
		if (*aptr == '#' || *aptr == '\0' || strnicmp(aptr, "/*", 2) == 0) continue;
		if (KalTableAlloc <= KalTableSize) {
			KalTableAlloc += 50;
			KalTable = realloc(KalTable, KalTableAlloc*sizeof(*KalTable));
		}
		ent = KalTable+KalTableSize;				/* Next entry */
		if ( (sscanf(aptr, "%d %d %d %lg %lg %lg %lg %lg %lg %lg %lg %lg", &ent->z1, &ent->m1, &ent->z2,
						 &ent->scaling, 
						 &ent->s, &ent->a0, &ent->a1, &ent->a2, &ent->a3, &ent->a4, &ent->a5, 
						 &ent->beta)) != 12) {
			ERRprintf("ERROR: Invalid format line ignored\n  : %s\n", aptr);
			continue;
		}
		KalTableSize++;
	}
	fclose(funit);

	TTYprintf("Loading Kalbitzer data: %d dE/dx entries: %s\n", KalTableSize, KalFilename);
	return(TRUE);
}


static BOOL RbsCalcKalbitzerStop(int z1, REAL m1, int z2, REAL *kev, REAL *spower, int npt) {

	int i;
	double E,a,epsil, epsil0, se, sn, sn0, m2;
	KONAC *ent;

	if (KalTable == NULL) return(FALSE);

	for (i=0; i<KalTableSize; i++) {
		ent = KalTable + i;
		if (z1 == ent->z1 && z2 == ent->z2 && (int) (m1+0.5) == ent->m1) break;
	}
	if (i >= KalTableSize) return(FALSE);

	m2     = atom[z2-1].zm2;
	epsil0 = 32.53*m2/(z1*z2*(m1+m2)*(pow(z1,0.23)+pow(z2,0.23)));
	sn0    = z1*z2*m1*8.462/((m1+m2)*(pow(z1,0.23)+pow(z2,0.23)));	/* Convert to eV-cm2/1E15 */

	for (i=0; i<npt; i++) {
		E = kev[i]/1000.0/ent->m1;						/* MeV/amu */
		se = ent->scaling * pow(E,ent->s) * log(2.7182818+E*ent->beta) /
			  (ent->a0+ent->a1*pow(E,0.25)+ent->a2*pow(E,0.5)+ent->a3*pow(E,0.75)+ent->a4*E+ent->a5*pow(E,1+ent->s));

/* Add universal nuclear stopping power (from ziegler.c) */
		epsil= epsil0 * kev[i];
		if (epsil<30.0) {
			a=(.01321*pow(epsil,0.21226))+(.19593*sqrt(epsil));
			sn=.5*log(1+1.1383*epsil)/(epsil+a);
		} else {
			sn=log(epsil)/(2*epsil);
		}
		sn *= sn0;						/* Convert to eV-cm2/1E15 */
		spower[i] = (REAL) (se + sn);
	}

	return(TRUE);
}
