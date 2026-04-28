/*  ------------------------------------------------------------------------ */
/*  ---------                                              ----------------- */
/*  --------- COPYRIGHT 1993 (c) Computer Graphics Service ----------------- */
/*  ---------                                              ----------------- */
/*  ---------    The source code to RUMP may be freely     ----------------- */
/*  ---------  modified as long as this copyright notice   ----------------- */
/*  ---------          is included and unchanged.          ----------------- */
/*  ------------------------------------------------------------------------ */

/* atomdo.c */

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
-- RbsIdentp  interprets input strings that are of the from "4He++"
--            returns given mass number, Z, and charge state
--            This routine only handles the charge state, and calls RbsIdent
--            to do the rest of the work.
--            Unlike RbsIdent, this routine delivers Z, not a pointer to
--            an element structure.
=========================================================================== */
int RbsIdentp(char *token, int *Z, int *isotmp, int *charge) {

/*  -- Local Variables -- */
   int i;
   char *aptr;
   ATOMS *atomp;
	int ch, iso;

/*  -- Code begin -- */
	ch = 0;														/* Determine the charge */
   aptr = token + strlen(token)-1;
   while (*aptr == '+') {ch++; *aptr-- = '\0';}

   if (ch == 0) ch = 1;
   i = RbsIdent(token, &atomp, &iso);

	if (Z      != NULL) *Z      = atomp->z;
	if (charge != NULL) *charge = ch;
	if (isotmp != NULL) *isotmp = iso;
   return(i);
}

/* ===========================================================================
--  Usage Guide:
--
--     FUNCTION IDENT(SYMBOL)
--     IDENTIFY FUNCTION.  DETERMINES IF SYMBOL PASSED IS ANY ELEMENT.
--     Allows special setup by passing symbol folled by a + to
--     indicate change of mass.  For looking at specific isotopes.
--     Reads a number after the + to get that mass number.
--     For instance, SI+28 gets only Silicon 28 data.
--
--  Quick: Parses input stream for element identifier
--
--     INPUTS:   SYMBOL   Element name (ASCII string)
--
--     OUTPUTS:  IDENT    .TRUE.   If symbol identified
--                        .FALSE.  If no recognized symbol
--               T        pointer to *ATOM structure
--               ISOTMP   Mass number of isotope, or 0 for natural abund.
--
--     COMMON BLOCKS:     ATOMS
--     CALLED FROM:       PREPAN
--     CALLS:             None
=========================================================================== */
int RbsIdent(char *token, ATOMS **atomp, int *isotmp) {

	char *aptr, *name;
	int    isotmp_invalid;
	ATOMS *atomp_invalid;

/* Allow NULL arguments */
	if (isotmp == NULL) isotmp = &isotmp_invalid;
	if (atomp  == NULL) atomp  = &atomp_invalid;

/* Default invalid returns */
	*atomp  = NULL;									/*  Default invalid returns	*/
	*isotmp = 0;

/* Look for specific isotope specifications */
	if ((aptr=strstr(token,"+")) != NULL) {	/* Is it of form Si+28?			*/
		aptr++;											/* First digit						*/
		if (! isdigit(*aptr)) {						/* Hope it is okay				*/
			ERRprintf("ERROR: If you specify +, specify an element (%s)\n", token);
			return(FALSE);
		}
		*isotmp = atoi(aptr);						/* And convert						*/
		name = token;									/* Name starts first char		*/
	} else if (isdigit(*token)) {					/* Look for form 28Si			*/
		*isotmp = strtol(token, &name, 10);		/* Convert valid chars			*/
	} else {												/* Last possibility Si29		*/
		name = token;
		while (isalpha(*name)) name++;			/* Skip all alphabetic chars	*/
		*isotmp = atoi(name);
		name = token;									/* But name really starts 1st	*/
	}

/* And finally look up the element name */
	if ((*atomp = RbsIdentRaw(name)) == NULL) {
		ERRprintf("ERROR: Illegal element/isotope specified: %s\n", token);
		return(FALSE);
	}

	return (TRUE);
}


/* ===========================================================================
-- RbsIdentRaw is the lowest level atomic symbol lookup
-- Other people need this resource, so break it out of code above
=========================================================================== */
ATOMS *RbsIdentRaw(char *select) {

	int i;
	char name[20];

/* First, copy over alphabetic component of the element name, up to 5 chars */
	for (i=0; i<sizeof(name)-1; i++) {
		if (! isalpha(*select)) break;			/* Will also happen on */
		name[i] = *select++;
	}
	name[i] = '\0';									/* Terminate the string */

/* And now find the element name */
	for (i=0; i<NumElements; i++) {
		if (stricmp(name, atom[i].name) == 0) return(&atom[i]);
	}
	return(NULL);
}

/* ===========================================================================
-- RbsGetRealMass finds the best (REAL) mass associated with an integer mass 
-- number.  Other people need this resource, so break it out of code above:
=========================================================================== */
REAL RbsGetRealMass(int iz, int iso) {

	int i;
	ISOTOPE *isotope;

	if (iso == 0)   {
		return(atom[iz-1].mass);
	} else if (iz <= NumElements)   {
		isotope = atom[iz-1].isotop;
	   for (i=0; i<NISOT; i++) {
			if (fabs(iso-isotope[i].mass) < 0.5) return(isotope[i].mass);
		}
	   gen_warn("Isotope not found.  Value used instead.");
	}
	return((REAL) iso);					/* Default standby value */
}

/* ===========================================================================
-- atomic_symbol returns a pointer to the name of an element, based on Z
-- used to centralize refrences to atomic data table
=========================================================================== */
char *atomic_symbol( int z ) {
  return (atom[z-1].name);
}

/* ===========================================================================
-- atomic_mass returns a pointer to the average mass of an element
-- used to centralize refrences to atomic data table
=========================================================================== */
REAL atomic_mass( int z ) {
  return (atom[z-1].mass);
}

/* ===========================================================================
-- atomic_mass returns a pointer to the average mass of an element
-- used to centralize refrences to atomic data table
=========================================================================== */
REAL atomic_density( int z ) {
  return (atom[z-1].dense);
}

/* ===========================================================================
-- atomic_isotopes returns a pointer to the isotope array for an element
-- used to centralize refrences to atomic data table
=========================================================================== */
REAL **atomic_isotopes( int z ) {
  return ((REAL **)atom[z-1].isotop);
}

/* ===========================================================================
-- atomic_isotopes returns a pointer to the isotope array for an element
-- used to centralize refrences to atomic data table
=========================================================================== */
ATOMS *atomic_data( int z ) {
  return (&atom[z-1]);
}

/* ===========================================================================
-- RbsScalePrint prints devations from unity stopping power scaling.
-- used in completing the SIM STATUS command
-- centralizes refrences to atomic data table
=========================================================================== */
void RbsScalePrint(void) {
  int j;
  for (j=0; j < NumElements ; j++ ) {
    if (atom[j].scale != 1.)   {
      TTYprintf("Stopping power of %s is scaled by %5.3f\n",
                  atom[j].name, atom[j].scale);
    }
  }
  return;
}

/* ===========================================================================
-- RbsSetSScale modifies the stopping power entry for an element
-- based on a new scale factor
--
-- Returns old scaling value (current if new_scale == 0)
=========================================================================== */
REAL RbsSetSScale (ATOMS *atomp, REAL new_scale) {

   int i, j;
	REAL old_scale;
	STOPPING_TABLE *table;

	old_scale = atomp->scale;
	if (new_scale > 0.0) {
		atomp->scale = new_scale;
		new_scale   /= old_scale;				/* Undo last one in place */
		i = atomp->index;
		for (table=stop_tables; table!=NULL; table=table->next) {
			for (j=0; j<NDEG; j++) table->stop[i].p[j] *= new_scale;
		}
	}
	return(old_scale);
}
