/*  atomio.c */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"

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
static void empty_atom(ATOMS *atomp);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */
int   RumpDataValid = FALSE;				/* Assume both invalid		*/
int   NumElements = 0;						/* Number of elements in atom	*/
ATOMS *atom=NULL;								/* Atomic data tables		*/

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/* ============================================================================
-- Usage Guide:
--
-- Usage:   BOOL RbsLoad1(char *fname)
--
-- Quick:   Reads ASCII atomic data
--
-- Inputs:  fname - Name of the ASCII file to be read as data
--
-- Output:  fills in internal data structures
--
-- Return:  TRUE  -> everything okay
--          FALSE -> something failed (file does not exist?)
--
-- Global Vars:  atom, NumElements
--
-- Called By:    Only meant to be used for system setup
--
-- Calls:        None
--
-- Description:
--     Function LOAD1 reads in an ASCII file containing atomic data:
--     Atomic Symbols, Weights, Densities, Isotope data, Stopping powers
--     This can be used on new computers until the binary read works.
--
-- Notes:
============================================================================ */
int RbsLoad1(char *fname) {
	ERRprintf("ERROR: This routine will probably disappear.\n"
				 "       To load resonance files, use RESREAD <file>\n");
	return(FALSE);
}

BOOL RbsLoadAtomicData(char *fname) {

	FILE *lun;
	fpos_t pos;
	int  i, j, n1, n2;
	char *aptr, inbuf[LONG_STR_SIZE];
	char filename[PATH_MAX];

/* Make first sanity checks before deleting the previous ATOMS data */
	if (fname == NULL || *fname == '\0') {
		return(FALSE);
	} else if (! SysFindFile(filename, fname, RbsConfigPath, NULL, R_OK)) {
		ERRprintf("ERROR: Atomic data file %s not in path %s\n", fname, RbsConfigPath);
		return(FALSE);
	} else if ( (lun = fopen(filename,"r")) == NULL ) {
		ERRprintf("ERROR: Atomic data file %s failed to open\n", filename);
		return(FALSE);
	}

/* Okay, we have a file - delete existing information and continue */
	if (atom != NULL) {free(atom); atom = NULL;}
	NumElements = 0;

	TTYprintf("Loading atomic data: %s\n", filename);
	while (TRUE) {
		fgetpos(lun, &pos);
		if (fgets(inbuf, sizeof(inbuf), lun) == NULL) break;
		if (*inbuf != '#') {
			fsetpos(lun, &pos);
			break;
		}
	}
	if (fgets(inbuf, sizeof(inbuf), lun) == NULL ||
		sscanf(inbuf, "%d", &NumElements) != 1 || NumElements <= 0 || NumElements > 115) {
		ERRprintf("ERROR: Error reading # of elements in atomic data file\n");
		goto BadFile;
	}

/* Allocate space for the data and set defaults */
	atom = malloc(sizeof(*atom) * NumElements);
	for (i=0; i<NumElements; i++) empty_atom(atom+i);
	   
/* Scan the next lines for the element names */
	aptr = inbuf; *inbuf = '\n';
	for (i=0; i<NumElements; i++) {
		if (*aptr == '\n' || *aptr == '\0') {
			aptr = inbuf;
			if (fgets(inbuf, sizeof(inbuf), lun) == NULL) {
				ERRprintf("ERROR: Bad line of element symbols in atomic data file\n");
				goto BadFile;
			}
		}
		if (*aptr == ' ') {
			aptr++;
			atom[i].name[0] = *aptr++;
			atom[i].name[1] = atom[i].name[2] = '\0';
		} else {
			atom[i].name[0] = *aptr++;
			atom[i].name[1] = tolower(*aptr++);
			atom[i].name[2] = '\0';
		}
/*   printf("Element %d is '%2s'\n",i,(atom[i].name)); */
	}

/* ... Assume the data file is okay for next two blocks of info */
	for (i=0; i<NumElements; i++) fscanf(lun, "%f ", &atom[i].mass);
	for (i=0; i<NumElements; i++) fscanf(lun, "%f ", &atom[i].dense);

/* And the isotope table - some isotope tables may be empty */
	if (fscanf(lun, "%d %d", &n1, &n2) != 2 || NISOT != n2) {
		ERRprintf("ERROR: Isotope table mismatch\n");
		goto BadFile;
   }

	for (i=0; i<n1; i++) {					/* Default values already here */
		for (j=0; j<NISOT; j++) {
			fscanf(lun,"%f %f ",
					 &atom[i].isotop[j].fraction, &atom[i].isotop[j].mass);
		}
	}

	fclose (lun);

/* ... Fill in Z for each element, and index into other tables */
	for (i=0; i < NumElements ; i++) {
		atom[i].z     = i+1;
		atom[i].index = i;
	}

/* Missing feature: resetting scale factors back to unity.  Something like: */
#if 0
	found = FALSE;	/*  Reset rescale parameters */
	for (i=0; i<NumElements ; i++ ) {
      if (atom[i].scale != 1.0)   {
			found = TRUE;
			atom[i].scale = 1.0;
      }
	}
	if (found)  TTYputsnl("Rescale parameters all reset to unity");
#endif

/* For now, reset without comment - this applies first time, too */
   for (i=0; i<NumElements; i++) atom[i].scale = 1.0;

	RumpDataValid = TRUE;
	return(TRUE);

/* *********************/
/* *** Errors here *** */
/* *********************/
BadFile:
	if (lun != NULL) fclose(lun);
	ERRprintf("ERROR: Atomic data file %s not loaded\n", filename);
	RumpDataValid = FALSE;
	return(FALSE);

}

/* ===========================================================================
-- routine to prefill an atom structure will default values
=========================================================================== */
static void empty_atom(ATOMS *atomp) {
	
	int i;

	atomp->z = -1;							/* Nothing there		*/
	strcpy(atomp->name, "Si");
	atomp->mass  = 28;
	atomp->dense = 5.0E22f;
	atomp->scale = 1.0;
	for (i=0; i<NISOT; i++) atomp->isotop[i].mass = atomp->isotop[i].fraction = 0;
	return;
}
