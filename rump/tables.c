/*  ------------------------------------------------------------------------ */
/*  --------                                              ------------------ */
/*  -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  --------                                              ------------------ */
/*  --------    The source code to RUMP may be freely     ------------------ */
/*  --------  modified as long as this copyright notice   ------------------ */
/*  --------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------ */

/*  tables.c */
/* ===========================================================================
=========================================================================== */

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
#include "rump.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* -------------------------------- */
/* My share of the global variables */
/* -------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ============================================================================
-- Subroutine to read the density tables from data directory
--
-- Usage: int SimLoadDensityTable(char *filename)
--
-- Inputs: filename - file to open and read (assumed to exist)
--
-- Output: Creates table SimDensityTable[] containing all known units.
--
-- Returns: -1 => failure.  Reason will be printed
--           0 => success
--
-- Notes: Extra space will be allocated in the SimDensityTable for up to
--        MAXDEN new values at run time.
============================================================================ */
UNITS *SimDensityTable=NULL;
int    SimDensityTableSize=0;

int SimLoadDensityTable(char *filename) {

	FILE *funit;
	char inbuf[DFLT_STR_SIZE], *aptr, *bptr;	/* Input buffer */
	int  icnt;

	static UNITS StaticTable[] = {
		{"A",			ANGSTROMS,		1.0},			/* Base unit (default)			*/
		{"nm",		ANGSTROMS,		10.0},		/* Wish it were base unit		*/
		{"um",		ANGSTROMS,		10000.0},	/* Multiply by 1E4 to A			*/
		{"/CM2",		ATOMIC,			1.0},			/* x10^15 atoms/cm^3				*/
		{"M/CM2",	MOLECULAR,		1.0}			/* x10^15 molecules/cm^3		*/
	};
	#define	DENSITY_LIST	(sizeof(StaticTable)/sizeof(StaticTable[0]))

/* Scan file first to determine number of entries */
	icnt = 0;
	funit = NULL;
	if (filename != NULL) {
		if ( (funit = fopen(filename, "r")) == NULL) {
			ERRprintf("ERROR: %s density table file could not be opened for reading\n", filename);
		} else {
			while (fgets(inbuf, sizeof(inbuf), funit) != NULL) {
				aptr = inbuf;
				while (isspace(*aptr)) aptr++;
				if (*aptr == '#' || *aptr == '\0') continue;
				icnt++;
			}
			fseek(funit, 0L, SEEK_SET);					/* Rewind the file */
		}
	}

/* Allocate the density table and preload with permanent values */
	SimDensityTableSize = icnt+DENSITY_LIST+MAXDEN;
	SimDensityTable = calloc(SimDensitTableSize, sizeof(*SimDensityTable));
	for (icnt=0; i<DENSITY_LIST; icnt++) SimDensityTable[icnt]=StaticTable[icnt];

/* And now rescan the file, interpreting the data this time */
	if (funit != NULL) {
		while (fgets(inbuf, sizeof(inbuf), funit) != NULL) {
			aptr = inbuf;												/* Skip leading <sp>	*/
			while (isspace(*aptr)) aptr++;						/* Any space char		*/
			if (*aptr == '#' || *aptr == '\0') continue;		/* Check if comment	*/
			bptr = aptr;												/* Scan to name end	*/
			while (!isspace(*bptr) && *bptr!='\0') bptr++;	/* Walking forward	*/
			if (*bptr == '\0') continue;							/* Something left?	*/
			*bptr++ = '\0';											/* Terminate string	*/
			strscpy(SimDensityTable[icnt].ident, aptr, sizeof(SimDensityTable->ident));
			while (isspace(*bptr)) btpr++;						/* Skip space again	*/
			if (*bptr == '\0') continue;							/* Nothing there		*/
			SimDensityTable[icnt].type    = ABSOLUTE;
			SimDensityTable[icnt].density = atof(bptr);
			icnt++;
		}
		fclose(funit);
	}

	TTYprintf("Density tables loaded: %d permanent, %d from config files\n", DENSITY_LIST, icnt-DENSITY_LIST);
	return(0);
}
