/* ===========================================================================
-- Example file of user defined stopping power calculations.
--
-- This program traps the Z=93 element (defined as Mylar) and calculates
-- the stopping power values for H, D and He from a simple polynomial
--
-- Requirements:
--               (1) ZStop must be an exported entry point to this DLL
--                   (see the .def file)
--               (2) ZStop must be prototyped as in this file
--               (3) Zstop returns an integer
--                     0 => This incident/target combination is handled
--                    !0 => Don't use this routine for that pair
--               (4) Must return stopping power in ev/1E15 at/cm^2 units ONLY
--
-- Under NT, this must be compiled with at least /LD option.  Typical is
--		cl /nologo /LD /G5 /W3 /O2 mystop.c mystop.def
-- where mystop.def includes at least
--    LIBRARY mystop
--    EXPORTS
--      ZStop
--
-- To verify that the code is being used, check the reports after first sim:
--   Fitting particle 2H   for Z = 93 ... usf ... maximum deviation:  0.00%
-- The "u" in "usf" indicates user routine.  z is ziegler, e is exception
-- and "m" is mylar kludge.
--
-- Stopping powers are only generated once unless conditions are changed
-- dramatically.  Unloading this routine, or loading a new one, will 
-- automatically release all of the modules and recalculate stopping powers.
=========================================================================== */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#define TRUE  1
#define FALSE 0

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>


/* ===========================================================================
-- Usage:       int ZStop(int z1, double m1, int z2, double kev, double *stop);
--
-- Description: Calculates the stopping power in eV/1E15 for pair
--
-- Inputs:      z1 - incident particle Z
--              m1 - incident particle mass
--              z2 - target Z
--              keV - particle energy in KeV
--
-- Outputs:     stop - returned stopping power in eV/1E15 at/cm^2
--
-- Returns:     0 ==> routine handles this pair
--             !0 ==> routine does not handle this pair
--
-- Notes:   (1) The "handles this pair" is only checked once, with keV set
--              to the maximum it will ever be.  If the routine handles the
--              incident/target pair, it must do so for all energies below
--              the given keV.
--          (2) No distinction is made between nuclear and electronic stopping.
--          (3) If this routine chooses not to handle the pair, the default
--              calculation will be automatically used, possible modified by
--              the exception tables.
--          (4) This routine will be called many (currently 201) times for each
--              particle pair.  The values are fit to the internal RUMP
--              polynomial before use.
=========================================================================== */
int ZStop(int z1, double m1, int z2, double kev, double *stop) {

	double *p;

	static double h_in_my[6] =
		{13.37, -1.969E-2, +1.551E-5, -6.592E-9, +1.426E-12, -1.229E-16};
	static double d_in_my[6] =
		{10.37, -4.430E-4, -6.729E-6, +4.674E-9, -1.239E-12, +1.172E-16};
	static double he_in_my[6] =
		{12.71, +5.538E-2, -5.557E-5, +2.440E-8, -5.175E-12, +4.274E-16};

/* Check for everything we don't handle */
	if (z2 != 93 || z1 > 2)  return(-1);			/* We don't handle */
	if (z1 == 1 && m1 > 2.1) return(-1);			/* really bad anyway */
	if (z1 == 2 && m1 != 4)  return(-1);

/* Select one of the polynomial sets */
	if (z1 == 1 && m1 < 1.1) {			/* Hydrogen table */
		p = d_in_my;
	} else if (z1 == 1) {				/* Deuterium table */
		p = d_in_my;
	} else {									/* Helium table */
		p = he_in_my;
	}

/* Calculate the value */
	*stop = ((((p[5]*kev + p[4])*kev + p[3])*kev + p[2])*kev + p[1])*kev + p[0];

/* Return that we did indeed calculate */
	return 0;
}
