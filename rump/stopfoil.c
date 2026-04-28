/* StopFoil */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE                  /* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>

#ifdef TESTING
	#define	TTYprintf printf
	#define	ERRprintf printf
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "sample.h"									/* Need SimStopperFoilProc() */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define  panic    SysPanic(__FILE__, __LINE__)

typedef struct _ENTRY {
	int z;
	REAL mass;
} ENTRY;
	
typedef struct _DATA {
	REAL energy,stragg;
} DATA;

typedef struct _TABLE {
	char filename[PATH_MAX];		/* Source of the data					*/
	char description[DFLT_STR_SIZE];	/* Nominal description				*/
	REAL emin, emax, einc;			/* Lower limit, upper & increments	*/
	int num_points;					/* Number of energy points for each */
	int num_entries;					/* Number of Z/M pairs in list		*/
	DATA *data;							/* Actual data points (all)			*/
	ENTRY entry[1];					/* List of array points					*/
} TABLE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
void (*SimStopperFoilProc)(int z, REAL mass, REAL *energy, REAL *fwhm) = NULL;
void *SimStopperFoilDataTable = NULL;

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/* ===========================================================================
-- Usage:       SimCalcStopFoil(int z, REAL mass, REAL *energy, REAL *straggle);
--
-- Description: Routine to calculate the energy of a particle after passing
--              through a stopper foil.  Allows for careful calibration of a
--              stopping foil beyond simple simulation.
--
-- Inputs:      z       - Z of particle hitting the foil
--              mass    - mass of particle hitting foil
--              *energy - energy incident on the foil
--
-- Output:      *energy   - energy leaving the foil
--              *straggle - exit energy leaving the foil
--
-- Returns:     void
--
-- Notes:       A return energy negative will be interpreted as the particle
--              not penetrating.  This will return negative for any energy
--              below some minimum.  But, do better to have it actually put
--              out "valid" negative energies so last foils can be handled
--              a bit more cleanly.
=========================================================================== */
void SimCalcStopFoil(int z, REAL mass, REAL *energy, REAL *straggle) {

	DATA  *data;
	int i;
	REAL ediff, einc, eout, stragg;
	TABLE *table=SimStopperFoilDataTable;				/* Local usage */


/* Early checks for table and limits.  Also set defaults */
	*straggle = 0.0;											/* Common use		*/
	if (table == NULL) {										/* Need I bother? */
		return;
	} else if (*energy < table->emin) {
		*energy = -1.0;
		return;
	}
	
/* Find the particle in the list of entries */
	for (i=0; i<table->num_entries; i++) {
		if (table->entry[i].z == z && fabs(table->entry[i].mass-mass) < 0.5) break;
	}
	if (i >= table->num_entries) {				/* No entry -> can't penetrate */
		*energy = -1;
		return;
	}

/* Interpolate between points */
	einc = *energy;
	data = table->data + i*table->num_points;		/* Where data starts */
	if (einc > table->emax) {
		eout   = einc - (table->emax - data[table->num_points-1].energy);
		stragg = data[table->num_points-1].stragg;
	} else {
		i      = (int) ((einc-table->emin) / table->einc);
		ediff  = (einc - (table->emin+i*table->einc)) / table->einc;
		eout   = (1.0f-ediff) * data[i].energy + ediff * data[i+1].energy;
		stragg = (1.0f-ediff) * data[i].stragg + ediff * data[i+1].stragg;
	}
	*energy   = eout;
	*straggle = stragg;
	return;
}


/* ===========================================================================
-- Usage:       int SimReadStopFoilData(char *filename);
--
-- Description: Routine to read a file and setup the tables to handle the
--              correction for a stopper foil.  Format of the foil stopping
--              data is incorporated in the file itself.  
--
-- Inputs:      filename - source filename for the stoping foil inputs
--
-- Output:      fills in and initializes internal data structures
--
-- Returns:      0 => success
--              -1 => failure
=========================================================================== */
static int getline(FILE *funit, char *line, size_t length) {

	char *aptr;

	while (fgets(line, (int) length, funit) != NULL) {
		if (*line == '\0') continue;					/* Ignore non-lines */
		aptr = line + strlen(line) - 1;
		if (*aptr == '\n') *aptr = '\0';				/* Strip <nl> at end */
		aptr = line;
		while (isspace(*aptr)) aptr++;
		if (*aptr == '\0') continue;
		if (strncmp(aptr, "/*", 2) == 0) continue;
		if (tolower(*aptr) == 'c' && isspace(aptr[1])) continue;
		return(0);
	}
	return(-1);
}


int SimReadStopFoilData(char *filename) {

	FILE *funit;
	char inbuf[DFLT_STR_SIZE], desc[DFLT_STR_SIZE], *ermsg, *aptr, *bptr;
	TABLE *mytable=NULL;
	REAL emin, einc, etmp;
	int i,j,k, ierr, num_points, num_entries;

	if ( (funit = fopen(filename, "r")) == NULL) {
		ERRprintf("ERROR: %s failed to open\n", filename);
		return(-1);
	}

/* Deallocate previous table, if present */
	if (SimStopperFoilDataTable != NULL) {
		free(((TABLE *) SimStopperFoilDataTable)->data);
		free(SimStopperFoilDataTable);
		SimStopperFoilDataTable = NULL;
	}

/* Get the description line */
	ermsg = "DESCRIPTION_LINE";
	if (getline(funit, desc, sizeof(desc)) == -1) goto BadFile;
	
/* Get the line with starting energy, energy increment, and # of points */
	ermsg = "EMIN,EINC,NPT";
	if (getline(funit, inbuf, sizeof(inbuf)) == -1) goto BadFile;
	if (sscanf(inbuf, "%f %f %d", &emin, &einc, &num_points) != 3) goto BadLine;

/* Get the line with number of particles listed */
	ermsg = "NUM_PARTICLES";
	if (getline(funit, inbuf, sizeof(inbuf)) == -1) goto BadFile;
	if (sscanf(inbuf, "%d", &num_entries) != 1) goto BadLine;
	if (num_entries <= 0 || num_entries > 20) goto BadLine;

/* Allocate the "table" for this information */
	mytable = malloc(sizeof(TABLE) + num_entries*sizeof(sizeof(ENTRY)) );
	if (mytable == NULL) {
		fclose(funit);
		ERRprintf("ERROR: Unable to allocate memory for the stop foil table\n");
		return(-1);
	}

	strcpy(mytable->filename, filename);
	strcpy(mytable->description, desc);
	mytable->emin = emin;
	mytable->emax = emin + (num_points-1)*einc;
	mytable->einc = einc;
	mytable->num_points  = num_points;
	mytable->num_entries = num_entries;
	mytable->data = NULL;

/* Read the Z/mass lists */
	ermsg = "PARTICLE_LIST";
	for (i=0; i<num_entries; i++) {
		if (getline(funit, inbuf, sizeof(inbuf)) == -1) goto BadFile;
		if (sscanf(inbuf, "%d %f", &mytable->entry[i].z, &mytable->entry[i].mass) != 2)
			goto BadLine;
	}

/* And finally, allocate and read the data */
	ermsg = "ENERGY_LIST";
	mytable->data = malloc(sizeof(*mytable->data) * num_entries * num_points);

	ierr = 0;
	for (i=0; i<num_points; i++) {
		if (getline(funit, inbuf, sizeof(inbuf)) == -1) goto BadFile;
		etmp = (REAL) strtod(inbuf, &aptr);
		if (ierr < 10 && (fabs(emin+einc*i - etmp) > 0.5) ) {
			TTYprintf("WARNING: Expected energy (%f) mismatch with given energy (%f)\n",
				emin+einc*i, etmp);
			if (++ierr >= 10) ERRprintf("ERROR: No more warning messages printed\n");
		}
		for (j=0; j<num_entries; j++) {
			k = j*num_points + i;
			mytable->data[k].energy = (REAL) strtod(aptr, &bptr);
			if (bptr == aptr) goto BadLine;
			mytable->data[k].stragg = (REAL) strtod(bptr, &aptr);
			if (bptr == aptr) goto BadLine;
		}
	}

/* Put us in place and continue */
	TTYprintf("Loaded %s: %s\n", mytable->filename, mytable->description);
	SimStopperFoilDataTable = mytable;				/* And put us in place	*/
	fclose(funit);
	return(0);
	

BadFile:
	ERRprintf("ERROR: Could not find expected line of %s in StopFoil data file\n", ermsg);
	goto AllExit;
BadLine:
	ERRprintf("ERROR: Something wrong with StopFoil data line looking for %s\n"
		       "       text: %s", ermsg, inbuf);
	goto AllExit;
AllExit:
	if (mytable != NULL) {free(mytable->data); free(mytable);}
	fclose(funit);
	return(-1);
}

#ifdef TESTING


/* ===========================================================================
-- Usage:       SimWriteStopFoilData(char *filename);
--
-- Description: Routine to write out the internal stop foil information.
--              Currently only useful for testing.  May be incorporated into
--              later code to simulate and rewrite this information.
--
-- Inputs:      filename - name of file to be created (or overwitten)
--
-- Output:      creates or truncates filename and outputs the internal data
--
-- Returns:     0 => success
--             -1 => some failure
=========================================================================== */
int SimWriteStopFoilData(char *filename) {

	FILE *funit;
	int i,j,k;
	TABLE *table=SimStopperFoilDataTable;				/* Local usage */

	if (table == NULL) return(-1);						/* Nothing to write */
	
	if ( (funit = fopen(filename, "w")) == NULL) {
		ERRprintf("Unable to open file %s\n", filename);
		return(-1);
	}

	fprintf(funit, 
			"/* Stopper Foil data file\n"
			"/*\n"
			"/* All lines beginning with a /* or blank will be ignored as comments.\n"
			"/* However, the rest of the file has very little flexibility and RUMP will\n"
			"/* not be very tolerant of deviations from the structure specified here.\n"
			"/* Creation of this file may require some fairly painful hand effort.\n"
			"/*\n"
			"/* Lines are interpreted stupidly.  Each line *MUST* be as specified.\n"
			"/*\n"
			);
			
	fprintf(funit,
			"/* ---------------------------------------------------------------------------\n"
			"/* Id of this stopper foil - just some sort of name and description\n"
			"/* ---------------------------------------------------------------------------\n"
			"%s\n\n"
			, table->description);

	fprintf(funit,
			"/* ---------------------------------------------------------------------------\n"
			"/* keV of 1st point; keV between points; # of data points for each particle\n"
			"/* Any particle below 1st energy will not penetrate the foil\n"
			"/* ---------------------------------------------------------------------------\n"
			"%f %f %d      /* begin (keV), spacing (keV), number\n\n"
			, table->emin, table->einc, table->num_points);


	fprintf(funit,
			"/* ---------------------------------------------------------------------------\n"
			"/* Number of particles which are considered - others won't penetrate\n"
			"/* ---------------------------------------------------------------------------\n"
			"%d        /* Number of particles in this file\n\n"
			, table->num_entries);

	fprintf(funit, 
			"/* ---------------------------------------------------------------------------\n"
			"/* List of particles - Z / Mass pairs (mass must be within 0.5 of true)\n"
			"/* ---------------------------------------------------------------------------\n"
			);
	for (i=0; i<table->num_entries; i++) {
		fprintf(funit, "  %d  %f      /* Particle #%d\n",
			table->entry[i].z, table->entry[i].mass, i);
	}

	fputs("\n", funit);
	fputs(
			"/* ---------------------------------------------------------------------------\n"
			"/* Actual data.  On each line, \n"
			"/*      incident energy (keV)\n"
			"/*      exit energy (keV) and exit straggle of first particle\n"
			"/*      exit energy (keV) and exit straggle of second particle\n"
			"/*      ....\n"
			"/*      exit energy and exit straggle of last particle\n"
			"/* Incident energy given are mainly for convenience but are checked.\n"
			"/* Spacing between entries must correspond to early information.\n"
			"/* ---------------------------------------------------------------------------\n"
			, funit);
	for (i=0; i<table->num_points; i++) {
		fprintf(funit, " %7.1f ", table->emin + i*table->einc);
		for (j=0; j<table->num_entries; j++) {
			k = j*table->num_points + i;
			fprintf(funit, "  %7.1f %5.1f ", table->data[k].energy, table->data[k].stragg);
		}
		fputs("\n",funit);
	}

	fclose(funit);
	return(0);
}



int main(int argc, char *argv[]) {

	int i;
	TABLE *table=SimStopperFoilDataTable;				/* Local usage */

	if (argc < 2) {
		printf("Usage: <exe_name> <inputfile> [output_data_file]\n");
		return(0);
	}

	if (SimReadStopFoilData(argv[1]) != 0) {
		printf("Failed to read properly file %s\n", argv[1]);
		return(0);
	}

	if (argc > 2) SimWriteStopFoilData(argv[2]);
	
	table=SimStopperFoilDataTable;

	printf("table->filename:    %s\n"
			 "table->description: %s\n"
		    "     ->emin:        %f\n"
		    "     ->emax:        %f\n"
		    "     ->einc:        %f\n"
		    "     ->npt:         %d\n"
		    "     ->entries:     %d\n",
		table->filename, table->description,
		table->emin, table->emax, table->einc, 
		table->num_points, table->num_entries);
	for (i=0; i<table->num_entries; i++) {
		printf("     ->entry[%d].z:    %d\n"
			    "     ->entry[%d].mass: %f\n",
			i, table->entry[i].z, i, table->entry[i].mass);
	}
	printf("\n");

#ifdef JUNK
	for (i=0; i<table->num_points; i++) {
		int j,k;
		printf(" %8f ", table->emin + i*table->einc);
		for (j=0; j<table->num_entries; j++) {
			k = j*table->num_points + i;
			printf("  %8f %8f ", table->data[k].energy, table->data[k].stragg);
		}
		printf("\n");
	}
#endif

	return(0);
}
#endif  /* TESTING */
