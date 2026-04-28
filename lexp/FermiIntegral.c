/* ===========================================================================
Algorithm 779: Fermi-Dirac Functions of
Order –1/2, 1/2, 3/2, 5/2
ALLAN J. MACLEOD
University of Paisley

ACM Transactions on Mathematical Software, Vol. 24, No. 1, March 1998, Pages 1–12
=========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "FermiIntegral.h"

#define	TWOE		(5.4365636569180905)				/* 2e         */
#define	GAM1P5	(0.8862269254527580)				/* Gamma(1.5) */
#define	GAM2P5	(1.329340388179137)				/* Gamma(2.5) */
#define	GAM3P5	(3.323350970447843)				/* Gamma(3.5) */
#define	GAM4P5	(11.63172839656745)				/* Gamma(4.5) */

double CHEVAL(int n, double *a, double t);

/** ==========================================================================
-- DESCRIPTION:
--
--      This function computes the Fermi-Dirac function of
--      order -1/2, defined as
--
--                     Int{0 to inf} t**(-1/2) / (1+exp(t-x)) dt
--         FDM0P5(x) = -----------------------------------------
--                                 Gamma(1/2)
--
--      The function uses Chebyshev expansions which are given to
--      16 decimal places for x <= 2, but only 10 decimal places 
--      for x > 2.
--
--
--   ERROR RETURNS:
--   
--      None.
--
--
--   MACHINE-DEPENDENT CONSTANTS:
--
--      NTERMS1 - INTEGER - The number of terms used from the array
--                          ARRFD1. The recommended value is such that
--                               ABS(ARRFD1(NTERMS1)) < EPS/10
--                          subject to 1 <= NTERMS1 <= 14.
--
--      NTERMS2 - INTEGER - The number of terms used from the array
--                          ARRFD2. The recommended value is such that
--                               ABS(ARRFD2(NTERMS2)) < EPS/10
--                          subject to 1 <= NTERMS1 <= 23.
--
--      NTERMS3 - INTEGER - The number of terms used from the array
--                          ARRFD3. The recommended value is such that
--                               ABS(ARRFD3(NTERMS3)) < EPS/10
--                          subject to 1 <= NTERMS3 <= 28.
--
--      XMIN1 - REAL - The value of x below which
--                         FDM0P5(x) = exp(x)
--                     to machine precision. The recommended value
--                     is    LN ( SQRT(2) * EPSNEG )
--
--      XMIN2 - REAL - The value of x below which
--                         FDM0P5(x) = 0.0 
--                     to machine precision. The recommended value
--                     is    LN ( XMIN )
--
--      XHIGH - REAL - The value of x above which
--                         FDM0P5(x) = 2 sqrt (x/pi) 
--                     to machine precision. The recommended value
--                     is    1 / sqrt( 2 * EPSNEG )
--
--      For values of EPS, EPSNEG, and XMIN the user should refer to the
--      paper by Cody in ACM. Trans. Math. Soft. Vol. 14 (1988) p303-311.
--   
--      This code is provided with single and double precision values
--      of the machine-dependent parameters, suitable for machines
--      which satisfy the IEEE floating-point standard.
--
--
--   AUTHOR:
--          DR. ALLAN MACLEOD,
--          DEPT. OF MATHEMATICS AND STATISTICS,
--          UNIVERSITY OF PAISLEY,
--          HIGH ST.,
--          PAISLEY,
--          SCOTLAND
--          PA1 2BE
--
--          (e-mail: macl-ms0@paisley.ac.uk )
--
--
--   LATEST UPDATE:
--                 20 NOVEMBER, 1996
========================================================================== **/
/*  Machine-dependent constants (suitable for IEEE machines) */
#define	NTERM1	(15)
#define	NTERM2	(24)
#define	NTERM3	(49)
#define	XMIN1		(-36.39023)
#define	XMIN2		(-708.39641)
#define	XHIGH		(67108864.0)

double FDM0P5(double x) {

	double rc, chv, expx, t, xsq;
	static double ARRFD1[15] = {
		 1.7863596385102264E0,	-0.999372007632333E-1,	 0.64144652216054E-2,
		-0.4356415371345E-3,		 0.305216700310E-4,		-0.21810648110E-5,
		 0.1580050781E-6,			-0.115620570E-7,			 0.8525860E-9,
		-0.632529E-10,				 0.47159E-11,				-0.3530E-12,
		 0.265E-13,					-0.20E-14,					 0.2E-15
	};
	static double ARRFD2[24]={
		 1.6877111526052352E0,	 0.5978360226336983E0,	 0.357226004541669E-1,
		-0.132144786506426E-1,	-0.4040134207447E-3,		 0.5330011846887E-3,
		-0.148923504863E-4,		-0.218863822916E-4,		 0.19652084277E-5,
		 0.8565830466E-6,			-0.1407723133E-6,			-0.305175803E-7,
		 0.83524532E-8,			 0.9025750E-9,				-0.4455471E-9,
		-0.148342E-10,				 0.219266E-10,				-0.6579E-12,
		-0.10009E-11,				 0.936E-13,					 0.420E-13,
		-0.71E-14,					-0.16E-14,					 0.4E-15
	};
	static double ARRFD3[59]={
		 0.8707195029590563E0,	 0.59833110231733E-2,	-0.432670470895746E-1,
		-0.393083681608590E-1,	-0.191482688045932E-1,	-0.65582880980158E-2,
		-0.22276691516312E-2,	-0.8466786936178E-3,		-0.2807459489219E-3,
		-0.955575024348E-4,		-0.362367662803E-4,		-0.109158468869E-4,
		-0.39356701000E-5,		-0.13108192725E-5,		-0.2468816388E-6,
		-0.1048380311E-6,			 0.236181487E-7,			 0.227145359E-7,
		 0.145775174E-7,			 0.153926767E-7,			 0.56924772E-8,
		 0.50623068E-8,			 0.23426075E-8,			 0.12652275E-8,
		 0.8927773E-9,				 0.2994501E-9,				 0.2822785E-9,
		 0.910685E-10,				 0.696285E-10,				 0.366225E-10,
		 0.124351E-10,				 0.145019E-10,				 0.16645E-11,
		 0.45856E-11,				 0.6092E-12,				 0.9331E-12,
		 0.5238E-12,				-0.56E-14,					 0.3170E-12,
		-0.926E-13,					 0.1265E-12,				-0.327E-13,
		 0.276E-13,					 0.33E-14,					-0.42E-14,
		 0.101E-13,					-0.73E-14,					 0.64E-14,
		-0.37E-14,					 0.23E-14,					-0.9E-15,
		 0.2E-15,					 0.2E-15,					-0.3E-15,
		 0.4E-15,					-0.3E-15,					 0.2E-15,
		-0.1E-15,					 0.1E-15
	};

/* Code for x < -1 */
	if (x < XMIN2) {										/* Below cutoff for exp(x) */
		rc = 0.0;
	} else if (x < XMIN1) {								/* Pure Boltzmann approximation */
		rc = exp(x);
	} else if (x < -1.0) {								/* x < -1 */
		expx = exp(x);
		t = TWOE * expx - 1;
		rc = expx * CHEVAL(NTERM1, ARRFD1, t);
	} else if (x <= 2.0) {								/* -1 < x < 2 */
		t = (2*x - 1.0) / 3.0;
		rc = CHEVAL(NTERM2, ARRFD2, t);
	} else {													/* 2 < x */
		rc = sqrt(x) / GAM1P5;
		if (x <= XHIGH) {
			xsq = x*x;
			t = (50.0-xsq) / (42.0+xsq);
			chv = CHEVAL(NTERM3, ARRFD3, t);
			rc *= 1.0 - chv/xsq;
		}
	}
	return rc;
}
#undef	NTERM1
#undef	NTERM2
#undef	NTERM3
#undef	XMIN1
#undef	XMIN2
#undef	XHIGH

/* ===========================================================================
This function computes the Fermi-Dirac function of order 1/2, defined as

                Int{0 to inf} t**(1/2) / (1+exp(t-x)) dt
   FDP0P5(x) = -----------------------------------------
                            Gamma(3/2)

The function uses Chebyshev expansions which are given to 16 decimal places
for x <= 2, but only 10 decimal places for x > 2.

ERROR RETURNS:
   
  If XVALUE too large and positive, the function value will overflow. An
  error message is printed and the function returns DBL_MAX

MACHINE-DEPENDENT CONSTANTS:

  NTERMS1 - INTEGER - The number of terms used from the array
                       ARRFD1. The recommended value is such that
                            ABS(ARRFD1(NTERMS1)) < EPS/10
                       subject to 1 <= NTERMS1 <= 13.

  NTERMS2 - INTEGER - The number of terms used from the array
                      ARRFD2. The recommended value is such that
                           ABS(ARRFD2(NTERMS2)) < EPS/10
                      subject to 1 <= NTERMS1 <= 23.

  NTERMS3 - INTEGER - The number of terms used from the array
                      ARRFD3. The recommended value is such that
                          ABS(ARRFD3(NTERMS3)) < EPS/10
                      subject to 1 <= NTERMS3 <= 32.

  XMIN1 - REAL - The value of x below which
                      FDP0P5(x) = exp(x)
                 to machine precision. The recommended value
                 is 1.5*LN(2) + LN(EPSNEG)

      XMIN2 - REAL - The value of x below which
                         FDP0P5(x) = 0.0 
                     to machine precision. The recommended value
                     is LN(XMIN)

      XHIGH1 - REAL - The value of x above which
                         FDP0P5(x) = x**(3/2)/GAMMA(5/2)
                     to machine precision. The recommended value
                     is pi/SQRT(8*EPS)

      XHIGH2 - REAL - The value of x above which FDP0P5 would 
                      overflow. The reommended value is
                              (1.329*XMAX)**(2/3)

      For values of EPS, EPSNEG, and XMIN the user should refer to the
      paper by Cody in ACM. Trans. Math. Soft. Vol. 14 (1988) p303-311.
   
      This code is provided with single and double precision values
      of the machine-dependent parameters, suitable for machines
      which satisfy the IEEE floating-point standard.

   AUTHOR:
          DR. ALLAN MACLEOD,
          DEPT. OF MATHEMATICS AND STATISTICS,
          UNIVERSITY OF PAISLEY,
          HIGH ST.,
          PAISLEY,
          SCOTLAND
          PA1 2BE
          (e-mail: macl-ms0@paisley.ac.uk )

   LATEST UPDATE:   20 NOVEMBER, 1996
=========================================================================== */
/*  Machine-dependent constants (suitable for IEEE machines) */
#define	NTERM1	(14)
#define	NTERM2	(24)
#define	NTERM3	(54)
#define	XMIN1		(-37.5)
#define	XMIN2		(-708.394)
#define	XHIGH1	(7.45467E7)
#define	XHIGH2	(3.8392996E205)

double FDP0P5(double x) {

	double rc, chv, expx, t, xsq;
	double ARRFD1[14] = {
		 1.8862968392734597E0,		-0.543580817644053E-1,		 0.23644975439720E-2,
		-0.1216929365880E-3,			 0.68695130622E-5,			-0.4112076172E-6,
		 0.256351628E-7,				-0.16465008E-8,				 0.1081948E-9,
		-0.72392E-11,					 0.4915E-12,					-0.338E-13,
		0.23E-14,						-0.2E-15
	};

	double ARRFD2[24] = {
		 2.6982492788170612E0,		 1.2389914141133012E0,		 0.2291439379816278E0,
		 0.90316534687279E-2,		-0.25776524691246E-2,		-0.583681605388E-4,
		 0.693609458725E-4,			-0.18061670265E-5,			-0.21321530005E-5,
		 0.1754983951E-6,				 0.665325470E-7,				-0.101675977E-7,
		-0.19637597E-8,				 0.5075769E-9,					 0.491469E-10,
		-0.233737E-10,					-0.6645E-12,					 0.10115E-11,
		-0.313E-13,						-0.412E-13,						 0.38E-14,
		 0.16E-14,						-0.3E-15,						-0.1E-15
	};

	double ARRFD3[54] = {
		 2.5484384198009122E0,		 0.510439408960652E-1,		 0.77493527628294E-2,
		 -0.75041656584953E-2,		-0.77540826320296E-2,		-0.45810844539977E-2,
		 -0.23431641587363E-2,		-0.11788049513591E-2,		-0.5802739359702E-3,
		 -0.2825350700537E-3,		-0.1388136651799E-3,			-0.680695084875E-4,
		 -0.335356350608E-4,			-0.166533018734E-4,			-0.82714908266E-5,
		 -0.41425714409E-5,			-0.20805255294E-5,			-0.10479767478E-5,
		 -0.5315273802E-6,			-0.2694061178E-6,				-0.1374878749E-6,
		 -0.702308887E-7,				-0.359543942E-7,				-0.185106126E-7,
		 -0.95023937E-8,				-0.49184811E-8,				-0.25371950E-8,
		 -0.13151532E-8,				-0.6835168E-9,					-0.3538244E-9,
		 -0.1853182E-9,				-0.958983E-10,					-0.504083E-10,
		 -0.262238E-10,				-0.137255E-10,					-0.72340E-11,
		 -0.37429E-11,					-0.20059E-11,					-0.10269E-11,
		 -0.5551E-12,					-0.2857E-12,					-0.1520E-12,
		 -0.811E-13,					-0.410E-13,						-0.234E-13,
		 -0.110E-13,					-0.67E-14,						-0.30E-14,
		 -0.19E-14,						-0.9E-15,						-0.5E-15,
		 -0.3E-15,						-0.1E-15,						-0.1E-15
	};

/* Test for error condition */
	if (x > XHIGH2) {
		printf("ERROR: X too large for FDP0P5\n");
		return DBL_MAX;
	}

/*  Code for x < -1 */
	if (x < XMIN2) {										/* Below cutoff for exp(x) */
		rc = 0.0;
	} else if (x < XMIN1) {								/* Pure Boltzmann approximation */
		rc = exp(x);
	} else if (x < -1.0) {								/* x < -1 */
		expx = exp(x);
		t = TWOE * expx - 1;
		rc = expx * CHEVAL(NTERM1, ARRFD1, t);
	} else if (x <= 2.0) {								/* -1 < x < 2 */
		t = (2*x - 1.0) / 3.0;
		rc = CHEVAL(NTERM2, ARRFD2, t);
	} else {													/* 2 < x */
		rc = x*sqrt(x) / GAM2P5;
		if (x <= XHIGH1) {
			xsq = x*x;
			t = (50.0-xsq) / (42.0+xsq);
			chv = CHEVAL(NTERM3, ARRFD3, t);
			rc *= 1.0 + chv/xsq;
		}
	}
	return rc;
}
#undef	NTERM1
#undef	NTERM2
#undef	NTERM3
#undef	XMIN1
#undef	XMIN2
#undef	XHIGH1
#undef	XHIGH2

/* ===========================================================================
This function computes the Fermi-Dirac function of order 3/2, defined as

                  Int{0 to inf} t**(3/2) / (1+exp(t-x)) dt
      FDP1P5(x) = -----------------------------------------
                              Gamma(5/2)

   The function uses Chebyshev expansions which are given to
   16 decimal places for x <= 2, but only 10 decimal places 
   for x > 2.

ERROR RETURNS:
   
   If XVALUE too large and positive, the function value
   will overflow. An error message is printed and the function
   returns the value 0.0.

MACHINE-DEPENDENT CONSTANTS:

   NTERMS1 - INTEGER - The number of terms used from the array
                       ARRFD1. The recommended value is such that
                            ABS(ARRFD1(NTERMS1)) < EPS/10
                       subject to 1 <= NTERMS1 <= 12.

   NTERMS2 - INTEGER - The number of terms used from the array
                       ARRFD2. The recommended value is such that
                            ABS(ARRFD2(NTERMS2)) < EPS/10
                       subject to 1 <= NTERMS1 <= 22.

   NTERMS3 - INTEGER - The number of terms used from the array
                       ARRFD3. The recommended value is such that
                            ABS(ARRFD3(NTERMS3)) < EPS/10
                       subject to 1 <= NTERMS3 <= 33.

   XMIN1 - REAL - The value of x below which
                      FDP1P5(x) = exp(x)
                  to machine precision. The recommended value
                  is   2.5*LN(2) + LN(EPSNEG)

   XMIN2 - REAL - The value of x below which
                      FDP1P5(x) = 0.0 
                  to machine precision. The recommended value
                  is    LN ( XMIN )

   XHIGH1 - REAL - The value of x above which
                      FDP1P5(x) = x**(5/2)/GAMMA(7/2) 
                  to machine precision. The recommended value
                  is   pi * SQRT(1.6/EPS)

   XHIGH2 - REAL - The value of x above which FDP1P5 would 
                   overflow. The reommended value is
                           (3.233509*XMAX)**(2/5)

   For values of EPS, EPSNEG, and XMIN the user should refer to the
   paper by Cody in ACM. Trans. Math. Soft. Vol. 14 (1988) p303-311.

   This code is provided with single and double precision values
   of the machine-dependent parameters, suitable for machines
   which satisfy the IEEE floating-point standard.

AUTHOR:
       DR. ALLAN MACLEOD,
       DEPT. OF MATHEMATICS AND STATISTICS,
       UNIVERSITY OF PAISLEY,
       HIGH ST.,
       PAISLEY,
       SCOTLAND
       PA1 2BE
       (e-mail: macl_ms0@paisley.ac.uk )

LATEST UPDATE: 21 NOVEMBER, 1996
=========================================================================== */
/*  Machine-dependent constants (suitable for IEEE machines) */
#define	NTERM1	(13)
#define	NTERM2	(23)
#define	NTERM3	(56)
#define	XMIN1		(-35.004)
#define	XMIN2		(-708.396418)
#define	XHIGH1	(166674733.2)
#define	XHIGH2	(3.204467E123)

double FDP1P5(double x) {

	double rc, chv, expx, t, xsq;
	double ARRFD1[13] = {
		 1.9406549210378650E0,	-0.287867475518043E-1,	 0.8509157952313E-3,
		-0.332784525669E-4,		 0.15171202058E-5,		-0.762200874E-7,
		 0.40955489E-8,			-0.2311964E-9,				 0.135537E-10,
		-0.8187E-12,				 0.507E-13,					-0.32E-14,
		 0.2E-15
	};
	double ARRFD2[23] = {
		 3.5862251615634306E0,	 1.8518290056265751E0,	 0.4612349102417150E0,
		 0.579303976126881E-1,	 0.17043790554875E-2,	-0.3970520122496E-3,
		-0.70702491890E-5,		 0.76599748792E-5,		-0.1857811333E-6,
		-0.1832237956E-6,			 0.139249495E-7,			 0.46702027E-8,
		-0.6671984E-9,				-0.1161292E-9,				 0.284438E-10,
		0.24906E-11,				-0.11431E-11,				-0.279E-13,
		0.439E-13,					-0.14E-14,					-0.16E-14,
		0.1E-15,						 0.1E-15
	};
	double ARRFD3[56] = {
		12.1307581736884627E0,	-0.1547501111287255E0,	-0.739007388850999E-1,
		-0.307235377959258E-1,	-0.114548579330328E-1,	-0.40567636809539E-2,
		-0.13980158373227E-2,	-0.4454901810153E-3,		-0.1173946112704E-3,
		-0.148408980093E-4,		 0.118895154223E-4,		 0.146476958178E-4,
		 0.113228741730E-4,		 0.75762292948E-5,		 0.47120400466E-5,
		 0.28132628202E-5,		 0.16370517341E-5,		 0.9351076272E-6,
		 0.5278689210E-6,			 0.2951079870E-6,			 0.1638600190E-6,
		 0.905205409E-7,			 0.497756975E-7,			 0.272955863E-7,
		 0.149214585E-7,			 0.81420359E-8,			 0.44349200E-8,
		 0.24116032E-8,			 0.13105018E-8,			 0.7109736E-9,
		 0.3856721E-9,				 0.2089529E-9,				 0.1131735E-9,
		 0.612785E-10,				 0.331448E-10,				 0.179419E-10,
		 0.96953E-11,				 0.52463E-11,				 0.28343E-11,
		 0.15323E-11,				 0.8284E-12,				 0.4472E-12,
		 0.2421E-12,				 0.1304E-12,				 0.707E-13,
		 0.381E-13,					 0.206E-13,					 0.111E-13,
		 0.60E-14,					 0.33E-14,					 0.17E-14,
		 0.11E-14,					 0.5E-15,					 0.3E-15,
		 0.1E-15,					 0.1E-15
	};

/* GAM3P5,T,XSQ,XVALUE,ZERO */

	if (x > XHIGH2) {
		printf("ERROR: X TOO LARGE FOR FDP1P5\n");
		rc = DBL_MAX;
	} else if (x < XMIN2) {
		rc = 0.0;
	} else if (x < XMIN1) {
		rc = exp(x);
	} else if (x < -1.0) {
		expx = exp(x);
		t = TWOE * expx - 1;
		rc = expx * CHEVAL(NTERM1, ARRFD1, t);
	} else if (x <= 2.0) {
		t = (2*x-1.0)/3.0;
		rc = CHEVAL(NTERM2, ARRFD2, t);
	} else {
		rc = x*x*sqrt(x) / GAM3P5;
		if (x <= XHIGH1) {
			xsq = x*x;
			t = (50.0-xsq) / (42.0+xsq);
			chv = CHEVAL(NTERM3, ARRFD3, t);
			rc *= 1.0 + chv/xsq;
		}
	}
	return rc;
}
#undef	NTERM1
#undef	NTERM2
#undef	NTERM3
#undef	XMIN1
#undef	XMIN2
#undef	XHIGH1
#undef	XHIGH2


/* ===========================================================================
      This function computes the Fermi-Dirac function of
      order 5/2, defined as

                     Int{0 to inf} t**(5/2) / (1+exp(t-x)) dt
         FDP2P5(x) = -----------------------------------------
                                 Gamma(7/2)

      The function uses Chebyshev expansions which are given to
      16 decimal places for x <= 2, but only 10 decimal places 
      for x > 2.


   ERROR RETURNS:
   
      If XVALUE too large and positive, the function value
      will overflow. An error message is printed and the function
      returns the value 0.0.


   MACHINE-DEPENDENT CONSTANTS:

      NTERMS1 - INTEGER - The number of terms used from the array
                          ARRFD1. The recommended value is such that
                               ABS(ARRFD1(NTERMS1)) < EPS/10
                          subject to 1 <= NTERMS1 <= 11.

      NTERMS2 - INTEGER - The number of terms used from the array
                          ARRFD2. The recommended value is such that
                               ABS(ARRFD2(NTERMS2)) < EPS/10
                          subject to 1 <= NTERMS1 <= 21.

      NTERMS3 - INTEGER - The number of terms used from the array
                          ARRFD3. The recommended value is such that
                               ABS(ARRFD3(NTERMS3)) < EPS/10
                          subject to 1 <= NTERMS3 <= 39.

      XMIN1 - REAL - The value of x below which
                         FDP2P5(x) = exp(x)
                     to machine precision. The recommended value
                     is   3.5*LN(2) + LN(EPSNEG)

      XMIN2 - REAL - The value of x below which
                         FDP2P5(x) = 0.0 
                     to machine precision. The recommended value
                     is    LN ( XMIN )

      XHIGH1 - REAL - The value of x above which
                         FDP2P5(x) = x**(7/2)/GAMMA(9/2) 
                     to machine precision. The recommended value
                     is   pi * SQRT(35/(12*EPS))

      XHIGH2 - REAL - The value of x above which FDP2P5 would 
                      overflow. The reommended value is
                              (11.6317*XMAX)**(2/7)

      For values of EPS, EPSNEG, and XMIN the user should refer to the
      paper by Cody in ACM. Trans. Math. Soft. Vol. 14 (1988) p303-311.
   
      This code is provided with single and double precision values
      of the machine-dependent parameters, suitable for machines
      which satisfy the IEEE floating-point standard.


   AUTHOR:
          DR. ALLAN MACLEOD,
          DEPT. OF MATHEMATICS AND STATISTICS,
          UNIVERSITY OF PAISLEY,
          HIGH ST.,
          PAISLEY,
          SCOTLAND
          PA1 2BE

          (e-mail: macl-ms0@paisley.ac.uk )


   LATEST UPDATE:
                 21 NOVEMBER, 1996
=========================================================================== */
/*  Machine-dependent constants (suitable for IEEE machines) */
#define	NTERM1	(12)
#define	NTERM2	(22)
#define	NTERM3	(62)
#define	XMIN1		(-34.3107854)
#define	XMIN2		(-708.396418)
#define	XHIGH1	(254599860.5)
#define	XHIGH2	(2.383665E88)

double FDP2P5(double x) {

	double rc, chv, expx, t, xsq;
	double ARRFD1[12] = {
		 1.9694416685896693E0,	-0.149691794643492E-1,	 0.3006955816627E-3,
		-0.89462485950E-5,		 0.3298072025E-6,			-0.139239298E-7,
		 0.6455885E-9,				-0.320623E-10,				 0.16783E-11,
		-0.916E-13,					 0.52E-14,					-0.3E-15
	};
	double ARRFD2[22] = {
		 4.2642838398655301E0,	 2.3437426884912867E0,	 0.6727119780052076E0,
		 0.1148826327965569E0,	 0.109363968046758E-1,	 0.2567173957015E-3,
		-0.505889983911E-4,		-0.7376215774E-6,			 0.7352998758E-6,
		-0.166421736E-7,			-0.140920499E-7,			 0.9949192E-9,
		 0.2991457E-9,				-0.401332E-10,				-0.63546E-11,
		 0.14793E-11,				 0.1181E-12,				-0.524E-13,
		-0.11E-14,					 0.18E-14,					-0.1E-15,
		-0.1E-15
	};
	double ARRFD3[62] = {
		 30.2895676859802579E0,	 1.1678976642060562E0,	 0.6420591800821472E0,
		 0.3461723868407417E0,	 0.1840816790781889E0,	 0.973092435354509E-1,
		 0.513973292675393E-1,	 0.271709801041757E-1,	 0.143833271401165E-1,
		 0.76264863952155E-2,	 0.40503695767202E-2,	 0.21543961464149E-2,
		 0.11475689901777E-2,	 0.6120622369282E-3,		 0.3268340337859E-3,
		 0.1747145522742E-3,		 0.934878457860E-4,		 0.500692212553E-4,
		 0.268373821846E-4,		 0.143957191251E-4,		 0.77272440700E-5,
		 0.41503820336E-5,		 0.22305118261E-5,		 0.11993697093E-5,
		 0.6452344369E-6,			 0.3472822881E-6,			 0.1869964215E-6,
		 0.1007300272E-6,			 0.542807561E-7,			 0.292607829E-7,
		 0.157785918E-7,			 0.85110768E-8,			 0.45922760E-8,
		 0.24785001E-8,			 0.13380255E-8,			 0.7225103E-9,
		 0.3902350E-9,				 0.2108157E-9,				 0.1139122E-9,
		 0.615638E-10,				 0.332781E-10,				 0.179919E-10,
		 0.97288E-11,				 0.52617E-11,				 0.28461E-11,
		 0.15397E-11,				 0.8331E-12,				 0.4508E-12,
		 0.2440E-12,				 0.1321E-12,				 0.715E-13,
		 0.387E-13,					 0.210E-13,					 0.114E-13,
		 0.61E-14,					 0.33E-14,					 0.18E-14,
		 0.11E-14,					 0.5E-15,					 0.3E-15,
		 0.2E-15,					 0.1E-15
	};

	if (x > XHIGH2) {
		printf("ERROR: X TOO LARGE FOR FDP2P5\n");
		rc = DBL_MAX;
	} else if (x < XMIN2) {
		rc = 0.0;
	} else if (x < XMIN1) {
		rc = exp(x);
	} else if (x < -1.0) {
		expx = exp(x);
		t = TWOE * expx - 1.0;
		rc = expx * CHEVAL(NTERM1, ARRFD1, t);
	} else if (x < 2.0) {
		t = (2*x-1.0) / 3.0;
		rc = CHEVAL(NTERM2, ARRFD2, t);
	} else {
		rc = x*x*x*sqrt(x) / GAM4P5;
		if (x < XHIGH1) {
			xsq = x*x;
			t = (50.0-xsq) / (42.0+xsq);
			chv = CHEVAL(NTERM3, ARRFD3, t);
			rc *= 1.0 + chv/xsq;
		}
	}
	return rc;
}
#undef	NTERM1
#undef	NTERM2
#undef	NTERM3
#undef	XMIN1
#undef	XMIN2
#undef	XHIGH1
#undef	XHIGH2

/* ===========================================================================
This function evaluates a Chebyshev series, using the
Clenshaw method with Reinsch modification, as analysed
in the paper by Oliver.

   INPUT PARAMETERS

       N - INTEGER - The no. of terms in the sequence

       A - REAL ARRAY, dimension 0 to N - The coefficients of
           the Chebyshev series

       T - REAL - The value at which the series is to be
           evaluated

   REFERENCES
        "An error analysis of the modified Clenshaw method for
         evaluating Chebyshev and Fourier series" J. Oliver,
         J.I.M.A., vol. 20, 1977, pp379-391

   MACHINE-DEPENDENT CONSTANTS: NONE

   INTRINSIC FUNCTIONS USED;
      ABS

    AUTHOR:  Dr. Allan J. MacLeod,
             Dept. of Mathematics and Statistics,
             University of Paisley ,
             High St.,
             PAISLEY,
             SCOTLAND
             ( e-mail:  macl-ms0@paisley.ac.uk )

   LATEST MODIFICATION:
                       21 September , 1995
=========================================================================== */
double CHEVAL(int n, double *a, double t) {
	int i;
	double rc, d1,d2,tt,u0,u1,u2;

/* if abs(t) < 0.6, use standard Clenshaw method */
	if (fabs(t) < 0.6) {
		tt = t+t;
		u2 = u1 = u0 = 0.0;
		for (i=0; i<n; i++) {
			u2 = u1;
			u1 = u0;
			u0 = tt*u1 + a[n-1-i] - u2;
		}
		rc = (u0-u2)/2;

/*  if abs(t) >= 0.6, use the Reinsch modification */
	} else if (t > 0) {			/*    T > =  0.6 code */
		tt = (t-0.5) - 0.5;
		tt = tt+tt;
		u1 = d1 = d2 = 0.0;
		for (i=0; i<n; i++) {
			d2 = d1;
			u2 = u1;
			d1 = tt*u2 + a[n-1-i] + d2;
			u1 = d1+u2;
		}
		rc = (d1+d2)/2;

	} else {							/*  T < =  -0.6 code */
		tt = (t+0.5) + 0.5;
		tt = tt+tt;
		u1 = d1 = d2 = 0.0;
		for (i=0; i<n; i++) {
			d2 = d1;
			u2 = u1;
			d1 = tt*u2 + a[n-1-i] - d2;
			u1 = d1-u2;
		}
		rc = (d1-d2)/2;
	}

	return rc;
}
