/*  sigma.c */

/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1999 (c) Computer Graphics Service ----------------- */
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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "sigma.h"							/* For cross sections */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define DBUG(x)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static double rutherford(double kev, SP *sp);
static double rutherford_offset(double kev, SP *sp);
static double rutherford_screen(double kev, SP *sp);
/* static double rutherford_modified(double kev, SP *sp); */

static double ziegler_hscatt(double kev, SP *sp);
static double quillet_dscatt(double kev, SP *sp);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
int (*UserCrossSection)(int type, int z1, double m1, int z2, double m2, double energy,
								double cosph, double *s0, double *sm2) = NULL;

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */

/* ===========================================================================
-- Routine to determine the cross section for Scattering Geometry
--
-- Rutherford cross section for recoils in lab coordinates
--            / z1 z2 e^2 \2     4         [sqrt{1 - [(m1/m2) sin theta]^2} + cos theta]^2
--    sigma = | --------- |  -----------   -----------------------------------------------
--            \   4 E     /  sin^4 theta            sqrt{1 - [(m1/m2) sin theta]^2}
--   extract constants (e^2/4)^2 and E to obtain:
--   sigma = 1/E^2 * [ (e^2/4)^2 * mess ]
--   e = 4.803E-10 stat C ==> e^2 = 14.418 ev-A
--      14.418 eV-A = 0.014418 keV-A = 144.18 keV-sqrt(barn)
--      (e^2/4)^2 = 1299.242 keV^2-barn
-- Values updated 2/2007 on recommendation of Nuno Barradas
--
-- Usage: BOOL SetupSigmaScatter(SP *sp)
--  #define	Q							(1.60217653E-19)							Coulombs
--  #define	E0							(8.854187817E-12)							F/m
--  #define	PI							(3.1415926536)								pi
--  #define	E2_OVER_4_SQUARED		(pow(Q/(4*PI*E0)/4,2)*(1E28/1E6))	KeV^2-barn
--                             = 1295.9358
-- Only the final value is used below as E2_OVER_4_SQUARED
--
-- Inputs: sp - filled in with scattering geometry constants and parameters
--
-- Output: sp - components required for determining the cross section,
--              including routine to be called on each calculation.
--
-- Returns: TRUE if the cross section is properly configured, FALSE otherwise.
=========================================================================== */
#define	E2_OVER_4_SQUARED		(1295.9358)						/* (pow(Q/(4*PI*E0)/4,2)*(1E28/1E6)) */
#define	E2_OVER_2_SQUARED		(5183.7432)						/* (pow(Q/(4*PI*E0)/2,2)*(1E28/1E6)) */

BOOL SetupSigmaScatter(SP *sp) {

	double x,detail,sqirt;

/* Do the Rutherford component first - then any other formalism */
	x = sp->m1 / sp->m2;									/* Mass ratio - projectile/target	*/
	sqirt = sqrt(1-pow((x*sp->sinph),2));			/* Expression in do/dW					*/
	if (sp->sinph == 0) {								/* Explicitly check for 0				*/
		detail = pow((1-x*x),2);						/* For sin(ph) = 0						*/
	} else {
		detail = 4.0/pow(sp->sinph,4) * pow((sqirt + sp->cosph),2)/sqirt;
	}
	sp->csigma = E2_OVER_4_SQUARED * detail * pow(sp->z1*sp->z2,2);
	sp->csig_0 = 0.0;
	sp->csig_f = (0.049 * sp->z1 * pow(sp->z2,1.3333));		/* F&M 2.21 */

	if (GVGetInfo("NO_SIGMA_SCREEN", NULL, NULL)) sp->csig_f = 0;

	sp->calc = rutherford_screen;
	return(TRUE);
}

/* ===========================================================================
-- Routine to determine the cross section for Recoil Geometry
--
-- Rutherford cross section for recoils in lab coordinates
--            /  z1 z2 e^2 (M1 + M2)  \2       1
--    sigma = | --------------------- |   -----------
--            \       2 M2 E          /   cos^3 theta
--   extract constants (e^2/2)^2 and E to obtain:
--   sigma = 1/E^2 * [ (e^2/2)^2 * ( (z1*z2*(m1+m2))/m2 )^2 * (cos(phi))^-3 ]
--   e = 4.803E-10 stat C ==> e^2 = 14.418 ev-A
--      14.418 eV-A = 0.014418 keV-A = 144.18 keV-sqrt(barn)
--      (e^2/2)^2 = 5196.9681 keV^2-barn
--
-- Usage: BOOL SetupSigmaRecoil(SP *sp)
--
-- Inputs: sp - filled in with scattering geometry constants and parameters
--
-- Output: sp - components required for determining the cross section,
--              including routine to be called on each calculation.
--
-- Returns: TRUE if the cross section is properly configured, FALSE otherwise.
=========================================================================== */
BOOL SetupSigmaRecoil(SP *sp) {

	enum {IS_H=0, IS_D=1} itype;
	static BOOL H_Angle_Warned=FALSE, H_Energy_Warned=FALSE;
	static BOOL D_Angle_Warned=FALSE, D_Energy_Warned=FALSE;

/* Do the Rutherford component first - then any other formalism */
	sp->csigma = E2_OVER_2_SQUARED * pow(sp->z1*sp->z2*(1+sp->m1/sp->m2),2) / pow(sp->cosph,3);
	sp->csig_0 = 0.0;
	sp->csig_f = 0.0;
	sp->calc   = rutherford;

/* ----------------------------------------
-- For Helium incident and Hydrogen target
---------------------------------------- */
	if ( sp->z1 == 2 && sp->z2 == 1) {			/* 1H(4He,1H)4He possible reaction	*/
		itype = IS_H;											/* Assume hydrogen target */
		if (nint(sp->m2) == 2) itype = IS_D;
		if (GVGetInfo("RUTHERFORD_FORWARD", NULL, NULL)) {
			sp->calc   = rutherford;
		} else if (sigtab[itype] >= 0) {					/* Manual override of cross sections values */
			sp->csig_0 = sigtab[itype]*0.1602;			/* Counts/uC/msr/(1E15atom/cm2)	*/
			sp->csigma = coffe2[itype]*0.1602;			/* Coeff for 1/E^2 of the Xsect	*/
			sp->calc   = rutherford_offset;				/* Use constant plus the 1/E^2 term */
																	/* 0.1602 is old compatibility where sigma had to be given in internal units. */
/*			TTYprintf("Values are: %f %i %f %f\n", sp->m2, i, sp->csig_0, sp->csigma); */
		} else {
			if (itype == IS_H) {								/* Hydrogen again */
				if (sp->phi <= 40) {							/* Angle is within Ziegler routine capabilities */
					sp->calc = ziegler_hscatt;				/* Use Ziegler cross section w/ possible scaling */
					sp->csigma = fabs(sigtab[itype]);	/* Scaling factor from value determined by ziergler_hscatt */
					if (sp->kev_max > 4000) {
						if (! H_Energy_Warned) ERRprintf(
							"Ziegler descriptions of H scattering cross section are valid only for true\n"
							"scattering below 40 degrees and energies below 4.0 MeV.  Energy is out of range.\n"
							"Using Rutherford energy dependence (1/E^2) extrapolation above 4.0 MeV.n"
							"   Values were: phi = %.1f  kev_max = %.1f\n", sp->phi, sp->kev_max);
						H_Energy_Warned = TRUE;
					}
				} else {
					if (! H_Angle_Warned) ERRprintf(
							"Ziegler descriptions of H scattering cross section are valid only for true\n"
							"scattering below 40 degrees and energies below 4.0 MeV.  Angle out of bounds\n"
							"Since I have no idea how to extrapolate on angle, using Rutherford now.\n"
							"   Values were: phi = %.1f  kev_max = %.1f\n", sp->phi, sp->kev_max);
					H_Angle_Warned = TRUE;
				}

			} else {												/* Deuterium scattering */
				if (sp->phi > 10 && sp->phi < 32) {

					sp->calc = quillet_dscatt;				/* Use Quillet routines */
					sp->csigma = fabs(sigtab[itype]);	/* Allow user scaling of results */

					/* Do the constant calculations once only */
					sp->pf[0] =   -2.6e3*sp->phi*sp->phi    -1.76e5*sp->phi + 8.79e6;		/* A term */
					sp->pf[1] =     0.18*sp->phi*sp->phi        -10*sp->phi + 1422;		/* B term */
					sp->pf[2] =    -2.59*sp->phi*sp->phi     +111.6*sp->phi - 72;			/* C term */
					sp->pf[3] =  3.09e-3*sp->phi*sp->phi  -1.278e-1*sp->phi + 5.83e-1;	/* D term */
					sp->pf[4] = -9.05e-7*sp->phi*sp->phi  +3.645e-5*sp->phi - 1.71e-4;	/* K term */
					
					if (sp->kev_max > 2700) {
						if (! D_Energy_Warned) ERRprintf(
							"Quillet descriptions of D scattering cross section are valid only for true\n"
							"scattering angles 10-32 degrees and energies below 2.7 MeV.  Energy out of bounds.\n"
							"Using Rutherford energy dependence (1/E^2) extrapolation above 2.7 MeV.n"
							"   Values were: phi = %.1f  kev_max = %.1f\n", sp->phi, sp->kev_max);
						D_Energy_Warned = TRUE;
					}

				} else {
					if (! D_Angle_Warned) ERRprintf(
							"Quillet descriptions of D scattering cross section are valid only for true\n"
							"scattering angles 10-32 degrees and energies below 2.7 MeV.  Angle out of bounds.\n"
							"Since I have no idea how to extrapolate on angle, using Rutherford now.\n"
							"   Values were: phi = %.1f  kev_max = %.1f\n", sp->phi, sp->kev_max);
					D_Angle_Warned = TRUE;
				}
			}
		}
	}

	return(TRUE);
}

static double rutherford(double kev, SP *sp) {
	return (sp->csigma / kev / kev);
}

static double rutherford_offset(double kev, SP *sp) {
	return (sp->csig_0 + sp->csigma / kev / kev);
}

static double rutherford_screen(double kev, SP *sp) {
	return (sp->csigma / kev / kev) * (1.0 - sp->csig_f / kev);
}

#if 0
static double rutherford_modified(double kev, SP *sp) {
	return (sp->csig_0 + sp->csigma / kev / kev) * (1.0 - sp->csig_f / kev) ;
}
#endif


/* ===========================================================================
--  Function to return the cross section for forward recoil scattering
--
--  Usage:  double ziegler_hscatt(double kev, SP *sp)
--
--  Inputs: kev - incident beam energy
--          sp->csigma ==> scaling factor above calculation
--                           |sp->sigma| = 1  ==> theoretical value
--                           |sp->sigma| = 2  ==> doulbe theoretical value
--          sp-><blah> ==> all other scattering parameters
--
--  Output: none
--
--  Returns: cross section at this energy in barns/sr
--
-- Source: Basic equations from
--           J.F. Ziegler, "RBS/ERD simulation problems: Stopping powers,
--			        nuclear reactions and detector resolution.", Nucl. Instr.
--               and Meth. B 136-138, (1998), p. 141-146.
--         but the coefficients have been modified to more closely follow
--         the values from 
--           J. Tirira, P. Trocellier, J.P. Frontier, P. Trouslard,
--               "Theoretical and Experimental Study of Low Energy 4He 
--               Induced 1H Elastic Recoil ...", Nucl. Instr. and Meth. B
--               45, (1990), p. 203-207.
--         New coefficients are uniform fit to Tirira values over the range
--         5-40 degrees and 800-3000 keV.  This form has a cleaner fit to
--         low energy tail, etc.
--
--         sigma/sigma_rutherford = 1 + C1*MeV^C2 + C3*MeV^C4 * cos(phi)^C5
--
--                  C1          C2          C3          C4         C5
--             ----------------------------------------------------------
--    Ziegler  | 0.0655155   2.49984     0.137687     2.8191     6.98127
--    RUMP     | 0.09720717  1.359809    0.1429851    3.06073    5.406188
--             ----------------------------------------------------------
--
--         In practice, the deviations are small.  RUMP has an average 4%
--         higher cross section above 1 MeV, rising to 10% at small angles
--         and high energies (3 MeV).  At large angles (35-40 degrees),
--         RUMP is actually lower than Ziegler by up to 4%, though there is
--         a dearth of data in this regime.
--
-- CONCLUSION: Will use the modified values specified as "RUMP" above.
--             Uses the Ziegler polynomial and formalism, but with values
--             that are slightly different.
--
-- Notes: This routine LIMITS the energy to 4.0 MeV - constant extension
--        of cross section above this limit.
=========================================================================== */
static double ziegler_hscatt(double kev, SP *sp) {

	static double c[5] = {0.09720717, 1.359809, 0.1429851, 3.06073, 5.406188};

	double mev, mev4,									/* Local energy (MeV)		*/
			 sigma;										/* Local cross section		*/

	mev = max(kev/1000.0, 0.001);					/* Energy in MeV for calcs */
	mev4 = min(mev, 4.0);							/* mev4 is never > 4.0 MeV */

/* Ziegler forms gives value relative to rutherford, so later add rutherford */
	sigma = 1.0 + c[0] * pow(mev4,c[1]) + c[2] * pow(mev4,c[3]) * pow(sp->cosph,c[4]);
	sigma = sigma / mev / mev ;

	return(sigma*sp->csigma);						/* Add scaling factor */
}


/* ===========================================================================
--  Function to return the cross section for forward recoil scattering
--
--  Usage:  call dscatt(energy,cosphi,c0,cm2)
--
--  Inputs: c0     - Scaling factor for cross-section		REAL*4
--                     -1 ==> unity scaling from theoretical value
-- 		              -2 ==> double the theoretical value
--          energy - Incident He energy (keV)               REAL*4
--          cosphi - cosine of true scattering angle			REAL*4
--
--  Output: c0     - constant term of the forward scattering cross-section
--          cm2    - 1/E2 term of the forward scattering cross-section
--
-- Mods: 12/26/99 - MOT/PR
--    For energies below 2.7 MeV and angles between 10 and 30 degrees (real),
--    now uses cross sections from:
--       V.Quillet, F.Able, M.Schott.  NIM B 83 (1993) 47.
--    This analytical energy-angular dependence is supposed to work within 
--    range E<2700 keV and 10=<phi<=30 (phi as a real scattering angle).
=========================================================================== */
static double quillet_dscatt(double kev, SP *sp) {

#define	E_MATCH	2700				/* Energy where switch from Quillet to scaled rutherford */
#define	USE_CONSTANT_CROSS_SECTION_EXTRAPOLOATION

   double sigma;
	
#ifdef USE_CONSTANT_FACTOR_OVER_RUTHERFORD_EXTRAPOLATION
	static BOOL first=TRUE;
	static double sigma_r_const, quillet_to_rutherford_factor=1.0;
	double sigma_r, sigma_q;

	if (kev < E_MATCH) {
		sigma = 0.001 * (sp->pf[0]/(pow(kev-2128.0,2) + sp->pf[1]) + sp->pf[2] + kev*(sp->pf[3] + kev*sp->pf[4]));
	} else {
		if (first) {
			first = FALSE;
			sigma_q = 0.001 * (sp->pf[0]/(pow(E_MATCH-2128.0,2) + sp->pf[1]) + sp->pf[2] + E_MATCH*(sp->pf[3] + E_MATCH*sp->pf[4]));
			sigma_r_const = 5196.9724 * pow(sp->z1*sp->z2*(1+sp->m1/sp->m2),2)/pow(sp->cosph,3);
			sigma_r = sigma_r_const / E_MATCH / E_MATCH;
			quillet_to_rutherford_factor = sigma_q / sigma_r;
		}
		sigma = quillet_to_rutherford_factor * sigma_r_const / kev / kev;		/* Scaled rutherford */
	}
#elif defined USE_CONSTANT_CROSS_SECTION_EXTRAPOLOATION
	if (kev > E_MATCH) kev = E_MATCH;
	sigma = 0.001 * (sp->pf[0]/(pow(kev-2128.0,2) + sp->pf[1]) + sp->pf[2] + kev*(sp->pf[3] + kev*sp->pf[4]));
#else
	#error No extrapolation method defined
#endif		

	return(sigma * sp->csigma);
}


#if 0 

static void recoil( int z1, REAL m1, int z2, REAL m2,
             REAL energy, REAL cosph,  double *c0, double *cm2);
static void hscatt( double energy, double cosphi, double *c0, double *cm2);
static void dscatt( double energy, double cosphi, double *c0, double *cm2);

/* ===========================================================================
--  Function to return the cross section for forward recoil scattering
--
--  Usage:  call dscatt(energy,cosphi,c0,cm2)
--
--  Inputs: c0     - Scaling factor for cross-section		REAL*4
--                     -1 ==> unity scaling from theoretical value
-- 		              -2 ==> double the theoretical value
--          energy - Incident He energy (keV)               REAL*4
--          cosphi - cosine of true scattering angle			REAL*4
--
--  Output: c0     - constant term of the forward scattering cross-section
--          cm2    - 1/E2 term of the forward scattering cross-section
--
-- Mods: 12/26/99 - MOT/PR
--    For energies below 2.7 MeV and angles between 10 and 30 degrees (real),
--    now uses cross sections from:
--       V.Quillet, F.Able, M.Schott.  NIM B 83 (1993) 47.
--    This analytical energy-angular dependence is supposed to work within 
--    range E<2700 keV and 10=<phi<=30 (phi as a real scattering angle).
=========================================================================== */
#define	COS10		0.98480775f			/* Cosine(10 degrees) */
#define	COS30		0.8660254f			/* Cosine(30 degrees) */
void dscatt(double energy, double cosphi, double *c0, double *cm2) {

   double phi, a, b, c, d, k, s;
	static int warned=FALSE;

/* Check if within the Quillet, et. al regime */
   if (energy < 2700.0f && cosphi >= COS30 && cosphi <= COS10 && 0 == 1) {
		phi = acos(cosphi)*57.29578;				  /* scattering angle in degrees */
      a =   -2.6e3*phi*phi    -1.76e5*phi + 8.79e6;
      b =     0.18*phi*phi        -10*phi + 1422;
      c =    -2.59*phi*phi     +111.6*phi - 72;
      d =  3.09e-3*phi*phi  -1.278e-1*phi + 5.83e-1;
      k = -9.05e-7*phi*phi  +3.645e-5*phi - 1.71e-4;
   
		s = a/(pow(energy-2128.0,2) + b) + c + energy*(d + energy*k);
      *c0 = (REAL) (.001*s);						/* from mb/sr to barns/sr	*/
      *cm2 = 0.0f;									/* No 1/E^2 coefficient		*/

   } else {
		if (! warned) {
			ERRprintf("WARNING: Deuterium cross-section not entered in this range of scattering parameters\n");
			warned = TRUE;
		}
		hscatt(energy,cosphi,c0,cm2);
	}
	return;
}

/* ===========================================================================
--  This routine must return the coefficients c0 and cm2 which approximate the
--  cross section (in internal units - not barns/sr) by sigma = c0 + cm2/E**2
--
--  Usage:  call recoil(z1,m1,z2,m2,energy,cosph,c0,cm2)
--
--  Inputs: z1,m1  - Z and mass of incident particle
--          z2,m2  - Z and mass of target particle
--          energy - Incident energy (keV)
--          cosph  - Cosine of the scattering angle (true angle!)
--
--  Output: c0     - constant term in the cross section  (internal units)
--          cm2    - term in E^-2 from data              (internal units)
--
--  Reference: A. Turos and O. Meyer, "Depth Profiling of Hydrogen
--             by Detection of Recoiled Protons", Nucl Instrum Methods
--             B4 (1984) 92-97
--
--  Move the calculation of H/D cross sections to separate routine.  They 
--  may at some point become part of the standard resonance file format.
=========================================================================== */
void recoil(int z1, REAL m1, int z2, REAL m2,
             REAL energy, REAL cosph, double *c0, double *cm2) {

	int i;
	if ((z1==2) && (z2==1) && sigtab[0] != 0) {
		i = 0;										/*  Assume H scatter					*/
		if (nint(m2) == 2) i = 1;				/*  If not, use D						*/
		*c0  = sigtab[i];							/*  Counts/uC/msr/(1E15atom/cm2)	*/
		*cm2 = coffe2[i];							/*  Coeff for 1/E^2 of the Xsect	*/
		if (*c0 <= 0.0 && *cm2 <= 0.0)   {
			if (i == 0)   {
				hscatt(energy,cosph,c0,cm2);	/*  Return b/sr */
			} else {
				dscatt(energy,cosph,c0,cm2);	/*  Return b/sr */
			}
			*c0  = *c0/.1602f;					/*  Convert to internal units */
			*cm2 = *cm2/.1602f;
		}
	} else {											/*  Use normal RUTHERFORD! */
		/*--------------------------------------------------------------*/
		/* rutherford cross section for recoils in lab coordinates      */
		/*          /  z1 z2 e^2 (m1 + m2)  \ ^2      1                 */
		/*  sigma = | --------------------- |     -----------           */
		/*          \       2 m2 e1         /     cos^3 theta           */
		/* extract constants (e^2/2)^2 and e1 to obtain:                */
		/* sigma = (e^2/2)^2 * ( (z1*z2*(m1+m2))/m2 )^2 * (cos(phi))^-3 */
		/* e^2 = 1.44 MeV fm                                            */
		/* 1.44 MeV fm = 1.440e3 keV fm = 1.440e-10 keV cm              */
		/* (e^2)^2 = 2.0736e-20 keV^2 cm^2                              */
		/* 2.0736e-20 keV^2 cm^2 = 2.0736e4 keV^2 barn                  */
		/* (e^2)^2 / 2^2 = 5184 [keV^2 barn]                            */
		/*--------------------------------------------------------------*/
		*cm2  = (REAL) (5184.0 * pow(z1*z2*(1+m1/m2),2)/pow(cosph,3));	/* do/dW set! */
		*c0   = 0.0f;															/*  No constant term */
	}

	return;
}




/* ===========================================================================
--  Function to return the cross section for forward recoil scattering
--
--  Usage:  call hscatt(energy,cosphi,c0,cm2)
--
--  Inputs: c0     - Scaling factor for cross-section		REAL*4
--                     -1 ==> unity scaling from theoretical value
-- 		              -2 ==> double the theoretical value
--          energy - Incident He energy (keV)               REAL*4
--          cosphi - cosine of true scattering angle			REAL*4
--
--  Output: c0     - constant term of the forward scattering cross-section
--          cm2    - 1/E2 term of the forward scattering cross-section
--
--  Notes:  Theoretical and Experimental Study of Low Energy 4He Induced 1H
--          Elastic Recoil ..., J. Tirira, P. Trocellier, J.P. Frontier,
--          P. Trouslard, preprint from IBA 89.
--
--  Fit to coefficients done in cos(phi) instead of phi.  Basically, generates
--  much better fits with only quadratic dependence.  Need more of the data
--  though.
=========================================================================== */
void hscatt(double energy, double cosphi, double *c0, double *cm2) {

	double mev,					/*  Local energy (MeV)			*/
	       factor;				/*  User set scaling factor	*/

	static double c[4];			/*  Calculated energy coefficients */

	const double c_fit[4][3] =	/*  Fit coefficients (polynomial 2) */
		{	{  0.791092f, -2.45155f,  1.22479f },		/*  1/E**2 3.06561E-08 */
			{ -1.03606f,   4.99441f, -2.19183f },		/*  1/E    1.39467E-07 */
			{  2.46990f,  -5.72726f,  1.47478f },		/*  Const  4.89343E-08 */
			{ -0.541732f,  0.537255f, 0.255689f } };	/*  E      2.55180E-09 */

/* Cosine of scattering angle - (initialized to illegal value) */
	static double phi=2.0;

	factor = fabs(*c0);						/* User scaling */
	if (factor == 0) factor = 1;			/* zero is same as 1 */

	if (phi != cosphi) {
		phi  = cosphi;
		c[0] = c_fit[0][0] + phi*(c_fit[0][1] + phi*c_fit[0][2]);
		c[1] = c_fit[1][0] + phi*(c_fit[1][1] + phi*c_fit[1][2]);
		c[2] = c_fit[2][0] + phi*(c_fit[2][1] + phi*c_fit[2][2]);
		c[3] = c_fit[3][0] + phi*(c_fit[3][1] + phi*c_fit[3][2]);
		DBUG(printf("phi = %f C(n) = %f %f %f %f\n",
			57.29578*acos(phi),c[0], c[1], c[2], c[3]);)
	}

/*  ............................................................................ */
/*  ... The c(n) coeffient are in relative to MeV energies.  RUMP internally */
/*  ... uses keV so have to convert.  The cm2 coefficient returned must also be */
/*  ... corrected to properly work with keV. */
/*  ............................................................................ */
	mev  = max(energy/1000.0,.001);
	*c0  = (REAL) (pow(10.,(c[3]*mev + c[2] + (c[1]+c[0]/mev)/mev)));
	*cm2 = (REAL) (*c0*log(10.)*(-c[3]/2*pow(mev,3)+c[1]/2*mev+c[0]));
	*c0  = (REAL) ((*c0-*cm2/(pow(mev,2))) * factor);		/*  Correct C0			*/
	*cm2 = *cm2*1.0E6f * factor;									/*  And cm2 for keV	*/

	return;
}

#endif
