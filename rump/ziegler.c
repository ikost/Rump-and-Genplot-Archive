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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#ifdef GENPLOT_ENTRY
	#include "mytypes.h"
	#include "extends.h"
	#include "lexp.h"
	typedef struct _ATOMS {
		int    zmm1;              			/* Most common isotope mass number   */
		REAL   zm1;               			/* Most common isotope weight (amu)  */
		REAL   zm2;               			/* Average atomic weight (amu)       */
		REAL   zrho;              			/* Target density (g/cm3)            */
		REAL   zatrho;            			/* Target density (atoms/cm3)        */
		REAL   zvferm;            			/* Fermi velocity of solid / v0      */
		REAL   zlfctr;            			/* Lambda screening factor for Ions  */
		REAL   zpcoef[8];         			/* Stopping coefficients for protons */
	} ATOMS;
	static ATOMS atom[92];
#else
	#include "rump.h"
#endif

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static double hestop(int z1, double m1, int z2, double m2, double e, double pcoef[8]);
static double pstop( int z1, double m1, int z2, double m2, double e, double pcoef[8]);
static double histop(int z1, double m1, int z2, double m2, double e, double ee,
   double vfermi, double lfctr, double pcoef[8]);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */

/* ===========================================================================
-- Usage:       zread1(char *filename);
--
-- Description: Reads the Ziegler coefficients from table
--
-- Inputs:      filename - full pathname to appropriate data file
--
-- Outputs:     fills in internal data structures (atom)
--
-- Returns:     success (TRUE or FALSE)
--
-- Notes:       Hopefully doesn't use too much so easily modified
=========================================================================== */
int zread1(char *filename) {

	char inbuf[DFLT_STR_SIZE];
	FILE *lun;

	int i, a, ierr;
	ATOMS *zp;

	if ( (lun = fopen(filename, "r")) == NULL) {
		ERRprintf("ERROR: %s failed to open\n", filename);
		return(FALSE);
	}

/* In both blocks, treat # and C comment formats as comments */
/* Start scanning for first block of info */
	i = 0;
	zp = atom;
	while (i<92 && fgets(inbuf, sizeof(inbuf), lun) != NULL) {
		if (*inbuf == '#' || strncmp(inbuf, "/*", 2) == 0) continue;
		sscanf(inbuf,"%d %d %f %f %f %f %f %f\n", &a,
					&(zp->zmm1), &(zp->zm1), &(zp->zm2), &(zp->zrho),
					&(zp->zatrho), &(zp->zvferm), &(zp->zlfctr) );
		zp->zatrho = zp->zatrho*1e22f;
		if (a != i+1) {ierr=2; goto err;}
		i++; zp++;
	}
	if (i < 92) {ierr=1; goto err;}

/* Scan second block for information */
	i = 0;
	zp = atom;
	while (i<92 && fgets(inbuf, sizeof(inbuf), lun) != NULL) {
		if (*inbuf == '#' || stricmp(inbuf, "/*") == 0) continue;
		sscanf(inbuf,"%d %f %f %f %f %f %f %f %f\n", &a,
				&(zp->zpcoef[0]),&(zp->zpcoef[1]),&(zp->zpcoef[2]),&(zp->zpcoef[3]),
				&(zp->zpcoef[4]),&(zp->zpcoef[5]),&(zp->zpcoef[6]),&(zp->zpcoef[7]) );
		if (a != i+1) {ierr=3; goto err;}
		i++; zp++;
	}
	if (i != 92) {ierr=4; goto err;}

	fclose(lun);
	return (TRUE);

err:
	ERRprintf("ERROR: %s not in proper format for Ziegler data (%d).\n", filename, ierr);
	fclose(lun);
	return(FALSE);
}


/* ===========================================================================
--      UNIVERSAL STOPPING POWER SUBROUTINE
--
--      VALID FOR SOLIDS
--      VALID FOR ENERGIES BELOW 100,000 keV/AMU
--      MEAN ACCURACY IS 9 %
--
--      INPUT ZEROS FOR DEFAULT VALUES : M1, UNITS.
--
--      NEED DATA FILE : SCOEF DATA (184,80)
--      SCOEF(*,1) = ATOMIC NUMBER OF ELEMENT
--      SCOEF(*,2) = ATOMIC MASS OF MOST ABUNDANT ISOPOPE
--      SCOEF(*,3) = ATOMIC WEIGHT OF M.A.I.
--      SCOEF(*,4) = TARGET MASS (AMU)
--      SCOEF(*,5) = G/CM3 DENSITY (NORNAL ABUNDANCE)
--      SCOEF(*,6) = ATOMS/CM3*1E22 (NORMAL ABUNDANCE)
--      SCOEF(*,7) = (FERMI VELOCITY OF SOLID) / V0
--      SCOEF(*,8) = LAMBDA SCREENING FACTOR FOR IONS
--      SCOEF(92,*) TO SCOEF(184,*) = COEF. OF PROTON STOPPING
--
--      Z1     = ION ATOMIC NUMBER
--      MM1    = ION ATOMIC MASS
--      M1     = ION ATOMIC WEIGHT (AMU)
--      Z2     = TARGET ATOMIC NUMBER
--      M2     = TARGET ATOMIC WEIGHT (AMU)
--      EE     = ION ENERGY (keV)
--      RHO    = TARGET DENSITY (G/CM3)
--      ATRHO  = TARGET DENSITY (ATOMS/CM3)
--      VFERMI = (FERMI VELOCITY OF SOLID) / V0
--      LFCTR  = LAMBDA SCREENING FACTOR FOR IONS
--      PCOEF  = STOPPING COEFFICIENT FOR PROTONS
--      SE     = CALCULATED ELECTRONIC STOPPING (eV-CM2/1E15)
--      SN     = CALCULATED NUCLEAR STOPPING (eV-CM2/1E15)
--      UNITS : 1 = eV-CM2/1E15    (DEFAULT VALUE)
--              2 = MeV-CM2/MG
--              3 = eV/ANGSTROM
--              4 = L.S.S. REDUCED UNITS (DELTA EPSILON/DELTA RHO)
--      TIME  : 1   if this is the first time the program is
--                  called.
--            : 2   if the program has already been called with
--                  the same input parameters, except EE which is
--                  allowed to change.
--
-- Returns:  0 -> all okay
--          -1 -> can't fit those Z,M values (not in structure)
--
--  Modified 4/1/87 LRD - Error message via ERTYPE instead of write (6,) etc.
--  Modified 11/18/93 LRD - conversion to c                      
--  Modified 12/19/93 MOT - conversion to real c
--  Modified 3/24/96  MOT - added zcheck routine to determine if there is a
--                          possibility of using Ziegler calculation
=========================================================================== */
int zcheck(int z1, REAL m1, int z2) {
	if (z1>92 || z1<1 || z2>92 || z2<1) return(-1);
	return(0);
}

void zstop(int z1, REAL m1, int z2, REAL ee, REAL *r_se, REAL *r_sn, int units) {

	double pcoef[8];
	double a,b,lfctr,m2,atrho,vfermi;		/* rho - not needed */

	double sn, se;
	int i;
	double e, epsil;

/* Default return values */
	if (z1>92 || z1<1 || z2>92 || z2<1) {
		TTYprintf("WARNING: Input z1/z2 (%d,%d) not in range for Ziegler calculation\n", z1,z2);
		if (r_se != NULL) *r_se = 40;				/* Make finite so no blowup */
		if (r_sn != NULL) *r_sn = 5;
		return;
	} else if (ee<1e-20) {
		if (r_se != NULL) *r_se = 0;
		if (r_sn != NULL) *r_sn = 0;
		return;
	}

/* -----------------------------------------------------------------------
-- Lookup the Ziegler parameters for this pair.  There is an inefficiency
-- doing the lookups each time, but not enough to warrent special code or
-- loss or re-entrancy.
------------------------------------------------------------------------ */
	a      = atom[z1-1].zm1;
	b      = atom[z1-1].zm2;
	lfctr  = atom[z1-1].zlfctr;
	m2     = atom[z2-1].zm2;
/*	rho    = atom[z2-1].zrho; */				/* Apparently not needed */
	atrho  = atom[z2-1].zatrho;
	vfermi = atom[z2-1].zvferm;
	for (i=0;i<8;i++) pcoef[i]  = atom[z2-1].zpcoef[i];

	if (m1 == 0.0) m1 = (REAL) a;
	if (m1 == 0.0) m1 = (REAL) b;

	e=ee/m1;
	if (e>100000.) {
		TTYprintf("Reduced energy out of range: %f keV/amu (STOP)\n", e);
		return;
	}

	if (z1==1) {
		se = pstop(z1,m1,z2,m2,e,pcoef);
	} else if (z1==2) {
		se = hestop(z1,m1,z2,m2,e,pcoef);
	} else if (z1<=92) {
		se = histop(z1,m1,z2,m2,e,ee,vfermi,lfctr,pcoef);
	} else {
		TTYprintf("Element number out of range: %d\n",z1);
		return;
	}

/*   CALCULATE UNIVERSAL NUCLEAR STOPPING POWERS */

	epsil= 32.53*m2*ee/(z1*z2*(m1+m2)*(pow(z1,0.23)+pow(z2,0.23)));
	if (epsil<30.) {
		a=(.01321*pow(epsil,0.21226))+(.19593*sqrt(epsil));
		sn=.5*log(1+1.1383*epsil)/(epsil+a);
	} else {
		sn=log(epsil)/(2*epsil);
	}

/*   NOW CONVERT FROM REDUCED UNITS TO eV-CM2/1E15 */

	sn=sn*z1*z2*m1*8.462/((m1+m2)*(pow(z1,0.23)+pow(z2,0.23)));

/*   NOW CONVERT TO DESIRED STOPPING UNITS */
	switch (units) {
		case 1:
			break;
		case 2:                 /*   CONVERT TO MeV-CM2/MG */
			se=se*.60222/m2;
			sn=se*.60222/m2;
			break;
		case 3:                /*   CONVERT TO eV/ANGSTROM */
			se=se*atrho*1e-23;
			sn=sn*atrho*1e-23;
			break;
		case 4:                /*   CONVERT TO L.S.S. REDUCED UNITS :
											(DELTA EPSILON/DELTA RHO) */
			a=((m1+m2)*sqrt(pow(z1,0.6667)+pow(z2,0.6667)))/(z1*z2*m1*8.462);
			se=se*a;
			sn=sn*a;
			break;
		default:
			TTYprintf("Unknown unit index: using eV-CM2/1E15");
			break;
	}

	if (r_se != NULL) *r_se = (REAL) se;
	if (r_sn != NULL) *r_sn = (REAL) sn;
	return;
}

/* ===========================================================================
--
--          HELIUM ELECTRONIC STOPPING POWERS.
--
=========================================================================== */
static double hestop(int z1, double m1, int z2, double m2, double e, double pcoef[8]) {

	double heo, heh, he, a, b, sp, se;

/*   VELOCITY PROPORTIONAL STOPPING BELOW keV/AMU  ** HEO **. */

	heo = 1.0;
	he = max(heo,e);
	b = log(he);
	a = .744647+.142913*b+.0156235*b*b-.0026665*pow(b,3)+1.32512e-6*pow(b,8);
	heh = 1.0-exp(-min(30.0,a));

/*   ADD Z1**3 EFFECT TO HE/H STOPPING POWER RATIO  ** HEH ** */

	a = pow(7.6-max(0.,log(he)),2);
	heh = heh*(1.+(.007+.00005*z2)*exp(-a));
	sp = pstop(z1,m1,z2,m2,he,pcoef);
	se = sp*pow((z1*heh),2);
	if (e<=heo)
		se=se*sqrt(e/heo);   /*   CALCULUS HE VELOCITY PROPORTIONAL STOPPING */
	return(se);
}


/* ===========================================================================
--           HEAVY ION ELECTRONIC STOPPING POWERS.
=========================================================================== */
static double histop(int z1, double m1, int z2, double m2, double e, double ee,
				double vfermi, double lfctr, double pcoef[8]) {

	double yrmin, vrmin, v, vr, yr, a, b, q, se;
	double l0, l1, q1, q2, l, zeta, sp, vmin, eee;

/*   USE VELOCITY STOPPING FOR (YRMIN=VR* / Z1**.67) .LE. 0.13 OR */
/*   FOR VR .LE. 1.0 */

	yrmin = 0.13;
	vrmin = 1.0;
	v = sqrt(e/25)/vfermi;
	if (v<1.0) {
		vr=(3*vfermi/4)*(1+(2*v*v/3)-pow(v,4)/15);
	} else {
		vr=v*vfermi*(1+1/(5*v*v));
	}

/*   SET YR = MAXIMUM OF (VR/Z1**.67),(VRMIN/Z1**.67) OR YRMIN. */

	yr = max(yrmin,vr/pow(z1,0.6667));
	yr = max(yr,vrmin/pow(z1,0.6667));
	a = -.003845/yr-.09876+1.0406*yr-.08483*yr*yr+.01294*pow(yr,3);
	q = min(1.,max(0.,1.-exp(-min(a,50.))));

/*   Q = IONIZATION LEVEL OF THE ION AT VELOCITY * YR * . */
/*   NOW WE CONVERT IONIZATION LEVEL TO EFFECTIVE CHARGE. */

	b = 0.26-.004*z1;
	l0 = 2*.24*(pow((1.-q),0.6667))/(pow(z1,0.3333)*(1.-.143*(1.-q)));

	if        (q<(max(0.,.6-.015*z1))) {
		l1=l0;
	} else if (q<(max(0.,.8-.02*z1))) {
		q1 = max(0.,.6-.015*z1);
		q2 = max(0.,.8-.02*z1);
		l1 = 2.*.24*(pow((1.-q2),0.6667))/(pow(z1,0.3333)*(1.-.143*(1.-q2)));
		l1 = l1+(b-l1)*(q-q1)/(q2-q1);
	} else if (q<(max(0.,1.-.02*z1))) {
		l1 = b;
	} else {
		l1 = b*(1.-q)/(.02*z1);
	}
	l = max(l1,l0*lfctr);
	zeta = q+(1./(2.*vfermi*vfermi))*(1.-q)*log(1+pow((4*l*vfermi/1.919),2));

/*   ADD Z1**3 EFFECT AS SHOWN IN REF. 779. */

	a = -pow((7.6-max(0.,log(e))),2);
	zeta = zeta*(1.+(1./(z1*z1))*(.18+.0015*z2)*exp(a));
	if (yr > max(yrmin,vrmin/pow(z1,0.6667))) {
		sp = pstop(z1,m1,z2,m2,e,pcoef);
		se = sp*pow((zeta*z1),2);
		return(se);
	}

/*   CALCULATE VELOCITY STOPPING FOR YR LESS THAN YRMIN. */

	vrmin = max(vrmin,yrmin*pow(z1,0.6667));
	vmin = .5*(vrmin+sqrt(max(0.,vrmin*vrmin-0.8*vfermi*vfermi)));
	eee = 25*vmin*vmin;
	sp = pstop(z1,m1,z2,m2,eee,pcoef);
	se = (sp*pow((zeta*z1),2))*sqrt(e/eee);

	if (z2 == 6)		/* SPECIAL CORRECTION FOR LOW ENERGY ION IN CARBON */
		se=se*(pow((e/eee),(.75*.5)))/sqrt(e/eee);

	return(se);
}


/* ===========================================================================
--
--           PROTON ELECTRONIC STOPPING POWERS
--
=========================================================================== */
static double pstop( int z1, double m1, int z2, double m2, double e, double pcoef[8]) {

	double peo, pe, sl, sh, velpwr, se;

/*   VELOCITY PROPORTIONAL STOPPING BELOW VELOCITY ** PEO **. */

	peo = 25.;
	pe = max(peo,e);
	sl = (pcoef[0]*pow(pe,pcoef[1]))+pcoef[2]*pow(pe,pcoef[3]);
	sh = pcoef[4]/pow(pe,pcoef[5])*log((pcoef[6]/pe)+pcoef[7]*pe);
	se = sl*sh/(sl+sh);

	if (e<=peo) {		/* VELPWR IS THE POWER OF VELOCITY STOPPING BELOW PEO. */
		velpwr = (z2>6) ? 0.45 : 0.25 ;
		se=se*pow((e/peo),velpwr);
	}
	return(se);
}



#ifdef GENPLOT_ENTRY

/* ============================================================================
-- External function to be linked into the function evaluator
--
-- Usage: int <fnc>(int itype, TMPREAL *result, TMPREAL *args);
--
-- Inputs: itype -  0 ==> Real call,    result & args are    TMPREAL *
--                  1 ==> Complex call, result & args really TMPCOMPLEX *
--         result - pointer to where result should be stored
--         args   - pointer to array of arguments from call
--
-- Returns: 0 ==> everything is okay
--         !0 ==> function is not implemented (only valid for type=1)
--
-- Notes: The same call is used for both REAL format and COMPLEX function
--        evaluations.  The pointers result and args are typecast to TMPREAL *
--        but are really TMPCOMPLEX * if itype = 1.  
--
--        If function does not want to deal with complex arguments, return
--        -1 and function will be recalled with the real part of each arg
--        only, and will set the real part of a complex as the result.
--
-- Call Sequence: GVLinkFnc("name", int flags, int nargs, &fnc));
--                GVLinkFnc("square", 0, 1, &square);
=========================================================================== */
int myzstop(int type, TMPREAL *result, TMPREAL *args) {

	int z1,z2;
	REAL ee, m1, r_se, r_sn;

	if (type != 0) return(-1);							/* Don't deal w/ complex	*/

	ee = args[0];											/* Energy in keV	*/
	z1 = args[1]+0.5;										/* Incident z		*/
	m1 = args[2];											/* Incident m (AMU)*/
	z2 = args[3]+0.5;										/* Target z			*/

	zstop(z1, m1, z2, ee, &r_se, &r_sn, 3);		/* Get answer in eV/A */
	*result = r_se + r_sn;
	return(0);
}


/*============================================================================
-- User function initialization procedure.  This routine is called as the
-- module is loaded.  Run time per-instance initialization should be
-- performed by this routine.  At minimum, the routine should print a
-- message indicating successful load and version.  May also set a global
-- parameter to prevent reloading.
--
-- Usage:  Initialize the dynamic link module (local control)
--
-- Syntax: int Init(void);
--
-- Inputs: none
--
-- Output: local control only
--
-- Returns:  0 ==> all is okay
--          !0 ==> error, (but GENPLOT will ignore it!)
============================================================================ */
int Init(void) {

	if (! zread1("pscoef.dat")) {
		ERRprintf("ERROR: Failed to read pscoef.dat\n");
		return(-1);
	}

	if (! GVLinkFnc("STOPP", 0, 4, &myzstop)) {			/* 4 arguments */
		ERRprintf("Failed to link routine - GVLinkFnc() returned error");
		return(-1);
	} else {
		TTYprintf("Dynamic user module for STOPP loaded successfully\n");
		TTYputs("\n"
			"   stopp(KeV, Z1, M1, Z2)\n"
			"        keV   - energy in keV\n"
			"        Z1,M1 - incident particle\n"
			"        Z2    - target Z (mass taken as average)\n"
			"   Returns eV/A\n\n"
			);
	}
	return(0);
}

#endif
