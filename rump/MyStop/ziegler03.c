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
#include <stdio.h>
#include <string.h>
#include <windows.h>

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
void msg(char *text) {

	char log_path[]  = "c:/Program Files/SRIM 2008/SR Module/log.txt";

	FILE *funit;
	funit = fopen(log_path, "a");
	fprintf(funit, "%s\n", text);
	fclose(funit);
	return;
}

int ZStop(int z1, double m1, int z2, double m2, REAL kev[], REAL stop[], int npt) {

	double p[10], a1,a2,a3;
	int i,j, rcode;
	FILE *funit;
	char current_path[256];
	char srim_path[] = "c:/Program Files/SRIM 2008/SR Module";
	char data_path[] = "c:/Program Files/SRIM 2008/SR Module/rump.data";
	char aline[256], *aptr;

	sprintf(aline, "Request for Z1,M1: %d %f  Z2,M2: %d %f", z1,m1,z2,m2);
	msg(aline);

/* Will have to change to SRIM directory - save current path for return */
	getcwd(current_path, sizeof(current_path));

/* Change to the SRModule directory */
	if (srim_path[1] == ':') {
		if (_chdrive(srim_path[0] & 0x1F) != 0) return(-1);
		if (_chdir(srim_path+2) != 0) return(-1);					/* Set path */
	} else {
		if (_chdir(srim_path) != 0) return(-1);
	}

/* Delete the existing data file so we know when SRModule is done */
	_unlink(data_path);

/* Create the data file */
	if ( (funit = fopen("SR.IN", "w")) == NULL) {
		fprintf(stderr, "Doesn't appear that SRIM is installed\n");
		return(-1);
	}
	fprintf(funit, "---Stopping/Range Input Data (Number-format: Period = Decimal Point)\n");
	fprintf(funit, "---Output File Name\n");
	fprintf(funit, "\"%s\"\n", data_path);
	fprintf(funit, "---Ion(Z), Ion Mass(u)\n");
	fprintf(funit, "%d   %.3f\n", z1, m1);
	fprintf(funit, "---Target Data: (Solid=0,Gas=1), Density(g/cm3), Compound Corr.\n");
	fprintf(funit, "0    1    0\n");
	fprintf(funit, "---Number of Target Elements\n");
	fprintf(funit, "1 \n");
	fprintf(funit, "---Target Elements: (Z), Target name, Stoich, Target Mass(u)\n");
	fprintf(funit, "%d   \"unknown\" 100  %.3f\n", z2, m2);
	fprintf(funit, "---Output Stopping Units (1-8)\n");
	fprintf(funit, "7\n");
	fprintf(funit, "---Ion Energy : E-Min(keV), E-Max(keV)\n");
	fprintf(funit, "0 0\n");
	for (i=0; i<npt; i++) fprintf(funit, "%.3f\n", kev[i]);
	fprintf(funit, "0\n");
	fclose(funit);
	system("SRModule");

/* Return to the original directory */
	if (current_path[1] == ':') {
		_chdrive(current_path[0] & 0x1F);				/* Drive number */
		chdir(current_path+2);
	} else {
		chdir(current_path);
	}

/* Try to open the file - wait for SRModule to complete */
	for (i=0; i<10; i++) {
		if ( (funit = fopen(data_path, "r")) != NULL) break;
		Sleep(50);
	}
	if (funit == NULL) { msg(" open of data file failed"); return(-1); }

/* Try to read the file created by SRModule */
	rcode = 0;
	for (i=0; i<4; i++) fgets(aline, sizeof(aline), funit);	/* Skip headers */
	for (i=0; i<npt; i++) {												/* Read the energies */
		if (fgets(aline, sizeof(aline), funit) == NULL) { rcode = -1; break; }
		a1 = strtod(aline, &aptr);
		a2 = strtod(aptr, &aptr);
		a3 = strtod(aptr, &aptr);
		if (fabs(kev[i]-a1)/kev[i] > 0.002) {
			sprintf(aline, " kev match failed %f entry versus %f found", kev[i], a1);
			msg(aline);	rcode = -1;	break;
		}
		stop[i] = a2+a3;
	}
	fclose(funit);
	return(rcode);
}
