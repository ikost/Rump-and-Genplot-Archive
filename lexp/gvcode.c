/* gvcode.c - Routines for controlling function evaluator components */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define	_POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	GVCODE_C_SOURCE
#include "mytypes.h"
#include "extends.h"
#define	LEXP_EXTENSIONS					/* Need MATH for LexParseLineEx */
#include "lexp.h"
#include "gvdefs.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* Combination flag of all "variable usage" type GVF flags */
#define	USER_FLAG_MASK		(GVF_INTERNAL | GVF_USER | GVF_ALIAS)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
PRIVATE GV_ENTRY *gv_make_entry(char *name, int type, int flags);
PRIVATE GV_ENTRY *gv_find_entry(char *name);
PRIVATE int			gv_del_entry_by_name(char *name);
PRIVATE int			gv_del_entry(GV_ENTRY *entry, BOOL do_all);
PRIVATE GV_ENTRY *gv_find_adr(char *name, int *type, void **adr, int *length);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
EXPORT INT	GVMathMode = MATHFATAL | MATHERROR | MATHWARN | MATHINFO | MATH_SAFETY_ON ;
INT	   GVLocalIndex= 0;
INT		GVMaxIndex  = 0;
REAL     GVLnZero    = 0.0f;

int  (*RGB_Color)(char *)=NULL;				/* Special link from RST_COMP.C */

PRIVATE  GV_ENTRY _LnZero, _MaxIndex;
PRIVATE	GV_ENTRY _MaxIndex={MY_GV_ID, "$I", -1, GV_INT_LINK, 
									 GVF_NODELETE | GVF_HIDDEN | GVF_INTERNAL,
									0, {&GVMaxIndex}, 0,0, NULL, &_LnZero, NULL, ""};
PRIVATE	GV_ENTRY _LnZero={MY_GV_ID, "$LNZERO", -1, GV_REAL_LINK,
									 GVF_NODELETE | GVF_HIDDEN | GVF_INTERNAL,
									0, {(void *) &GVLnZero}, 0,0, NULL, NULL, &_MaxIndex, ""};

GV_ENTRY *GVFirstEntry=&_MaxIndex;
	
/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

PRIVATE int gv_local_level = 0;				/* Variable level for set/end local */

/* ===========================================================================
-- Routine to tell whether a given name is validly constructed.  Returns
-- simple Boolean value.
--
-- Usage:  BOOL = GVIsValidName(char *varname);
--
-- Inputs: varname - string to be examined
--
-- Output: none
--
-- Return: TRUE  - if given string could be a valid variable name.
--         FALSE - string has format errors
--
-- Notes: Currently, the following conditions must be met for names.
--          (1) Must begin with [a-z,A-Z,$,_]
--          (2) May contain only alphanumerics and the $ or _ character
--          (3) Must be less than 32 characters in length.
=========================================================================== */
BOOL GVIsNameValid(char *varname) {

	BOOL rcode;
	char *aptr;

	if ( (rcode = ( (varname != NULL)) && (strlen(varname) < 32) )) {
		rcode = isalpha(*varname) || (strchr("$_", *varname) != NULL);
		for (aptr=varname+1; *aptr && rcode; aptr++) {
			rcode = isalnum(*aptr) || (strchr(":$_", *aptr) != NULL);
		}
	}
	return(rcode);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
int GVSetLocal(void) {
	gv_local_level++;
	return(gv_local_level);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
int GVEndLocal(void) {

	GV_ENTRY *entry, *enext;

	if (gv_local_level == 0) return(0);		/* Can't release global vars */

	for (entry=GVFirstEntry; entry!=NULL; entry=enext) {
		enext = entry->next;						/* In case this one gets dumped */
		if (entry->flags & GVF_GLOBAL) {		/* Allocated as a global - change now */
			entry->flags &= ~GVF_GLOBAL;
			entry->level = 0;
		}
		if (entry->level == gv_local_level) gv_del_entry(entry, FALSE);
		entry = enext;
	}

	gv_local_level--;
	return(gv_local_level);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVMakeGlobal(char *name) {

	GV_ENTRY *entry;
	char modname[VARNAME_STR_SIZE+10];

	if ( (entry = gv_find_entry(name)) == NULL)		/* Can I find it?		*/
		return(FALSE);
	entry->level = 0;											/* Make it global */

	switch (entry->type) {
		case GV_3DCURVE:
		case GV_2DCURVE:
			if (entry->type == GV_3DCURVE)
				GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":z") );
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":y") );
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":x") );
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_SURFACE:
			GVMakeGlobal( strcat( strcpy(modname, entry->varname), ":z") );
			GVMakeGlobal( strcat( strcpy(modname, entry->varname), ":y") );
			GVMakeGlobal( strcat( strcpy(modname, entry->varname), ":x") );
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":ncol") );
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":nrow") );
			break;
		case GV_ARRAY:
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_COMPLEX_ARRAY:
			GVMakeGlobal( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
	}
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkReal(char *name, int flags, REAL *value) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_REAL_LINK, flags)) == NULL) return(FALSE);
	entry->var.floatadr = value;
	return(TRUE);
}

BOOL GVLinkDouble(char *name, int flags, DOUBLE *value) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name,GV_DOUBLE_LINK, flags)) == NULL) return(FALSE);
	entry->var.doubleadr = value;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkComplex(char *name, int flags, COMPLEX *value) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_COMPLEX_LINK, flags)) == NULL) return(FALSE);
	entry->var.complexadr = value;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkInt(char *name, int flags, INT *value) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_INT_LINK, flags)) == NULL) return(FALSE);
	entry->var.intadr   = value;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocReal(char *name, int flags, REAL inival) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_REAL, flags)) == NULL) return(FALSE);
	entry->var.floatvalue = inival;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocDouble(char *name, int flags, DOUBLE inival) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_DOUBLE, flags)) == NULL) return(FALSE);
	entry->var.doublevalue = inival;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocComplex(char *name, int flags, COMPLEX inival) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_COMPLEX, flags)) == NULL) return(FALSE);
	entry->var.complexvalue = inival;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocInt(char *name, int flags, INT inival) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_INT, flags)) == NULL) return(FALSE);
	entry->var.intvalue = inival;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocArray(char *name, int flags, INT length) {

	GV_ENTRY *entry;
	ARRAY    *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if (length > GVI_MAX_LENGTH) return(FALSE);				/* Limit length */

	if ( (arrayptr = (ARRAY *) malloc(sizeof(ARRAY))) != NULL) {
		arrayptr->maxsize = length;
		if ( (arrayptr->x = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
			if ( (entry = gv_make_entry(name, GV_ARRAY, flags)) != NULL) {
				entry->extra     = length;
				arrayptr->size   = &entry->extra;
				entry->var.array = arrayptr;
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
				return(TRUE);
			}
			free(arrayptr->x);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocComplexArray(char *name, int flags, INT length) {

	GV_ENTRY *entry;
	COMPLEX_ARRAY *c_arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if (length > GVI_MAX_LENGTH) return(FALSE);				/* Limit length */

	if ( (c_arrayptr = (COMPLEX_ARRAY *) malloc(sizeof(COMPLEX_ARRAY))) != NULL) {
		c_arrayptr->maxsize = length;
		if ( (c_arrayptr->z = (COMPLEX *) calloc(length, sizeof(COMPLEX))) != NULL) {
			if ( (entry = gv_make_entry(name, GV_COMPLEX_ARRAY, flags)) != NULL) {
				entry->extra       = length;
				c_arrayptr->size   = &entry->extra;
				entry->var.c_array = c_arrayptr;
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, c_arrayptr->size);
				return(TRUE);
			}
			free(c_arrayptr->z);
		}
		free(c_arrayptr);
	}
	return(FALSE);
}

/* ===========================================================================
-- Helper routine linked with :NPT of 2D and 3D curves to allow automatic
-- size increase when NPT set to more than original allocated size.
--
-- This routine is called indirectly from GVSetValue()
=========================================================================== */
static int gv_verify_curve_size(GV_ENTRY *entry) {
	char cname[VARNAME_STR_SIZE+4];						/* Enough room for the name */
	int ilen;
	CURVE *curve;

	strcpy(cname, entry->namechars);					/* What is this curve */
	if (strlen(cname) <= 4) return 0;				/* Can't be */
	if (stricmp(cname+strlen(cname)-4, ":npt") != 0) return 0;
	cname[strlen(cname)-4] = '\0';					/* Truncate to underlying name */
	if ( (entry = gv_find_entry(cname)) == NULL) return 0;
	curve = entry->var.curve;

/* Now validate the information */
	if (curve->npt <= curve->nptmax) return 0;

/* If bad, and we can resize, do so.  Otherwise refuse to set */
	if (curve->npt > GVI_MAX_LENGTH) {
		ERRprintf("ERROR: Attempt to set NPT (%d) to more than GVI_MAX_LENGTH (%d).\n       Value set to nptmax (%d)\n", curve->npt, GVI_MAX_LENGTH, curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	} else if (entry->flags & GVF_NORESIZE) {
		ERRprintf("ERROR: Attempt to set NPT beyond NPTMAX.  Curve cannot be resized.\n       Value set to nptmax (%d)\n", curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	}

/* Resize - expensive operation so add 10% overhead to size */
	ilen = curve->npt + curve->npt/10;
	if (ilen > GVI_MAX_LENGTH) ilen = GVI_MAX_LENGTH;
	if (! GVResize(cname, ilen)) {
		ERRprintf("ERROR: Attempt to increase size of curve to match NPT unsuccessful.\n       Value set to nptmax (%d)\n", curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	}
	TTYprintf("INFO: NPT (%d) requested is larger than NPTMAX.  Increased curve size to %d\n", curve->npt, curve->nptmax);
	return 0;
}

/* ===========================================================================
-- Similar help specifically designed to deal with NPT in GENPLOT.
--
-- This routine is called indirectly from GVSetValue()
=========================================================================== */
static int gv_verify_genplot_npt(GV_ENTRY *entry) {

	char cname[VARNAME_STR_SIZE];						/* Enough room for the name */
	int ilen;
	CURVE *curve;

	memcpy(&entry, &entry->extra, sizeof(entry));
	strcpy(cname, entry->namechars);					/* What is this curve */
	curve = entry->var.curve;

/* Now validate the information */
	if (curve->npt <= curve->nptmax) return 0;

/* If bad, and we can resize, do so.  Otherwise refuse to set */
	if (curve->npt > GVI_MAX_LENGTH) {
		ERRprintf("ERROR: Attempt to set NPT (%d) to more than GVI_MAX_LENGTH (%d).\n       Value set to nptmax (%d)\n", curve->npt, GVI_MAX_LENGTH, curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	} else if (entry->flags & GVF_NORESIZE) {
		ERRprintf("ERROR: Attempt to set NPT beyond NPTMAX.  Curve cannot be resized.\n       Value set to nptmax (%d)\n", curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	}

/* Resize - expensive operation so add 10% overhead to size */
	ilen = curve->npt + curve->npt/10;
	if (ilen > GVI_MAX_LENGTH) ilen = GVI_MAX_LENGTH;
	if (! GVResize(cname, ilen)) {
		ERRprintf("ERROR: Attempt to increase size of curve to match NPT unsuccessful.\n       Value set to nptmax (%d)\n", curve->nptmax);
		curve->npt = curve->nptmax;
		return 1;
	}
	TTYprintf("INFO: NPT (%d) requested is larger than NPTMAX.  Increased curve size to %d\n", curve->npt, curve->nptmax);
	return 0;
}

/* ===========================================================================
-- Calling program that sets up call to gv_verify_genplot_npt
--
-- This routine is called indirectly from GVSetValue()
=========================================================================== */
int GVValidateGenplotVars(char *type, char *var, char *cname) {

	GV_ENTRY *entry1, *entry2;

	if (stricmp(type, "curve") != 0) return 1;					/* Only deal with curves */
	if (stricmp(var,  "npt")   != 0) return 1;					/* And only npt for now */
	if ( (entry1 = gv_find_entry(cname)) == NULL) return 1;	/* Nothing to do either */
	if ( (entry2 = gv_find_entry("npt")) == NULL) return 1;	/* Must have both */
	if (entry2->var.intadr != &entry1->var.curve->npt) {
		ERRprintf("GENPLOT tried to link a bad NPT value (GVValidateGenplotVars)\n");
		return 1;
	} 
	entry2->validate = &gv_verify_genplot_npt;
	memcpy(&entry2->extra, &entry1, sizeof(entry1));			/* Push the pointer into memory.  Might occupy both */
	return 0;																/* extra and tmpuse ... be very careful in 64 bit */
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAlloc2DCurve(char *name, int flags, INT length) {

	GV_ENTRY *entry;
	CURVE    *curveptr;
	REAL *x, *y;
	char modname[VARNAME_STR_SIZE+10];

	if (length > GVI_MAX_LENGTH) return(FALSE);				/* Limit length */

	if ( (curveptr = (CURVE *) malloc(sizeof(CURVE))) != NULL) {
		strcpy(curveptr->ids, "New Curve");
		*curveptr->options = '\0';
		curveptr->nptmax  = length;
		curveptr->npt     = length;
		if ( (x = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
			if ( (y = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
				curveptr->x = x;
				curveptr->y = y;
				curveptr->z = NULL;
				if ( (entry = gv_make_entry(name, GV_2DCURVE, flags)) != NULL) {
					entry->flags |= GVF_ARRAY_ALLOCATED;
					entry->var.curve = curveptr;
					flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
					GVLinkArray( strcat( strcpy(modname, name), ":x"),		flags, x, length, &curveptr->npt);
					GVLinkArray( strcat( strcpy(modname, name), ":y"),		flags, y, length, &curveptr->npt);
					GVLinkInt  ( strcat( strcpy(modname, name), ":npt"),	flags, &curveptr->npt);
					GVLinkString(strcat( strcpy(modname, name), ":ids"),	flags,  curveptr->ids, sizeof(curveptr->ids)); 
					if ( (entry = gv_find_entry(strcat( strcpy(modname, name), ":npt"))) != NULL) entry->validate = &gv_verify_curve_size;
					return(TRUE);
				}
				free(y);
			}
			free(x);
		}
		free(curveptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAlloc3DCurve(char *name, int flags, INT length) {

	GV_ENTRY *entry;
	CURVE    *curveptr;
	REAL *x, *y, *z;
	char modname[VARNAME_STR_SIZE+10];

	if (length > GVI_MAX_LENGTH) return(FALSE);				/* Limit length */

	if ( (curveptr = (CURVE *) malloc(sizeof(CURVE))) != NULL) {
		strcpy(curveptr->ids, "New Curve");
		*curveptr->options = '\0';
		curveptr->nptmax  = length;
		curveptr->npt     = length;
		if ( (x = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
			if ( (y = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
				if ( (z = (REAL *) calloc(length, sizeof(REAL))) != NULL) {
					curveptr->x = x;
					curveptr->y = y;
					curveptr->z = z;
					if ( (entry = gv_make_entry(name, GV_3DCURVE, flags)) != NULL) {
						entry->flags |= GVF_ARRAY_ALLOCATED;
						entry->var.curve = curveptr;
						flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
						GVLinkArray( strcat( strcpy(modname, name), ":x"),		flags, x, length, &curveptr->npt);
						GVLinkArray( strcat( strcpy(modname, name), ":y"),		flags, y, length, &curveptr->npt);
						GVLinkArray( strcat( strcpy(modname, name), ":z"),		flags, z, length, &curveptr->npt);
						GVLinkInt  ( strcat( strcpy(modname, name), ":npt"),	flags, &curveptr->npt);
						GVLinkString(strcat( strcpy(modname, name), ":ids"),	flags,  curveptr->ids, sizeof(curveptr->ids)); 
						if ( (entry = gv_find_entry(strcat( strcpy(modname, name), ":npt"))) != NULL) entry->validate = &gv_verify_curve_size;
						return(TRUE);
					}
					free(z);
				}
				free(y);
			}
			free(x);
		}
		free(curveptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
CURVE *GVLink2DCurve(char *name, int flags, REAL *x, REAL*y, int nptmax) {

	GV_ENTRY *entry;
	CURVE    *curveptr;
	char modname[VARNAME_STR_SIZE+10];

	if (nptmax < 0 || x == NULL || y == NULL) return(NULL);	/* Force valid */

	if ( (curveptr = (CURVE *) malloc(sizeof(CURVE))) != NULL) {
		strcpy(curveptr->ids, "New Curve");
		*curveptr->options = '\0';
		curveptr->nptmax  = nptmax;
		curveptr->npt     = nptmax;
		curveptr->x = x;
		curveptr->y = y;
		curveptr->z = NULL;
		if ( (entry = gv_make_entry(name, GV_2DCURVE, flags)) != NULL) {
			entry->flags |= GVF_NORESIZE;
			entry->var.curve = curveptr;
			flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
			GVLinkArray( strcat( strcpy(modname, name), ":x"),		flags, x, nptmax, &curveptr->npt);
			GVLinkArray( strcat( strcpy(modname, name), ":y"),		flags, y, nptmax, &curveptr->npt);
			GVLinkInt  ( strcat( strcpy(modname, name), ":npt"),	flags, &curveptr->npt);
			GVLinkString(strcat( strcpy(modname, name), ":ids"),	flags,  curveptr->ids, sizeof(curveptr->ids)); 
			return(curveptr);
		}
		free(curveptr);
	}
	return(NULL);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
CURVE *GVLink3DCurve(char *name, int flags, REAL *x, REAL*y, REAL *z, int nptmax) {

	GV_ENTRY *entry;
	CURVE    *curveptr;
	char modname[VARNAME_STR_SIZE+10];

	if (nptmax < 0 || x == NULL || y == NULL || z == NULL) return(NULL);

	if ( (curveptr = (CURVE *) malloc(sizeof(CURVE))) != NULL) {
		strcpy(curveptr->ids, "New Curve");
		*curveptr->options = '\0';
		curveptr->nptmax  = nptmax;
		curveptr->npt     = nptmax;
		curveptr->x = x;
		curveptr->y = y;
		curveptr->z = z;
		if ( (entry = gv_make_entry(name, GV_3DCURVE, flags)) != NULL) {
			entry->flags |= GVF_NORESIZE;
			entry->var.curve = curveptr;
			flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
			GVLinkArray( strcat( strcpy(modname, name), ":x"),		flags, x, nptmax, &curveptr->npt);
			GVLinkArray( strcat( strcpy(modname, name), ":y"),		flags, y, nptmax, &curveptr->npt);
			GVLinkArray( strcat( strcpy(modname, name), ":z"),		flags, z, nptmax, &curveptr->npt);
			GVLinkInt  ( strcat( strcpy(modname, name), ":npt"),	flags, &curveptr->npt);
			GVLinkString(strcat( strcpy(modname, name), ":ids"),	flags,  curveptr->ids, sizeof(curveptr->ids)); 
			return(curveptr);
		}
		free(curveptr);
	}
	return(NULL);
}

/* ---------------------------------------------------------------------------
-- Assumes that npt, nptmax, x, y, z, nrow, ncol have been properly set! 
-- Basically only relinks the internal variable definitions to possibly
-- changed pointers (x,y,z primarily).
--------------------------------------------------------------------------- */
BOOL GVRelinkSurface(char *name, SURFACE *surface) {
	char modname[VARNAME_STR_SIZE+10];
	int flags;

	flags = GVF_HIDDEN | GVF_INTERNAL;
	GVLinkArray( strcat( strcpy(modname, name), ":x"), flags, surface->x, surface->ncol, &surface->ncol);
	GVLinkArray( strcat( strcpy(modname, name), ":y"), flags, surface->y, surface->nrow, &surface->nrow);
	GVLinkArray( strcat( strcpy(modname, name), ":z"), flags, surface->z, surface->nrow*surface->ncol, &surface->npt);
	GVLinkInt  ( strcat( strcpy(modname, name), ":ncol"), flags, &surface->ncol);
	GVLinkInt  ( strcat( strcpy(modname, name), ":nrow"), flags, &surface->nrow);
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocSurface(char *name, int flags, INT nrow, INT ncol) {

	GV_ENTRY *entry;
	SURFACE  *surfaceptr;
	REAL *x, *y, *z;
	int i;
	char modname[VARNAME_STR_SIZE+10];

	if (nrow*ncol > GVI_MAX_LENGTH) return(FALSE);				/* Limit length */

	if ( (surfaceptr = (SURFACE *) malloc(sizeof(SURFACE))) != NULL) {
		strcpy(surfaceptr->ids, "New Surface");
		*surfaceptr->options = '\0';
		surfaceptr->nrow = surfaceptr->nrowmax    = nrow;
		surfaceptr->ncol = surfaceptr->ncolmax    = ncol;
		surfaceptr->npt  = surfaceptr->nptmax     = ncol*nrow;
		if ( (x = (REAL *) calloc(ncol, sizeof(REAL))) != NULL) {
			if ( (y = (REAL *) calloc(nrow, sizeof(REAL))) != NULL) {
				if ( (z = (REAL *) calloc(ncol*nrow, sizeof(REAL))) != NULL) {
					for (i=0; i<ncol; i++) x[i] = (REAL) i;
					for (i=0; i<nrow; i++) y[i] = (REAL) i;
					surfaceptr->x = x;
					surfaceptr->y = y;
					surfaceptr->z = z;
					if ( (entry = gv_make_entry(name, GV_SURFACE, flags)) != NULL) {
						entry->flags |= GVF_ARRAY_ALLOCATED;
						entry->var.surface = surfaceptr;
						flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
						GVLinkArray( strcat( strcpy(modname, name), ":x"),		flags, x, ncol, &surfaceptr->ncol);
						GVLinkArray( strcat( strcpy(modname, name), ":y"),		flags, y, nrow, &surfaceptr->nrow);
						GVLinkArray( strcat( strcpy(modname, name), ":z"),		flags, z, nrow*ncol, &surfaceptr->npt);
						GVLinkInt  ( strcat( strcpy(modname, name), ":ncol"),	flags, &surfaceptr->ncol);
						GVLinkInt  ( strcat( strcpy(modname, name), ":nrow"),	flags, &surfaceptr->nrow);
						GVLinkString(strcat( strcpy(modname, name), ":ids"),	flags,  surfaceptr->ids, sizeof(surfaceptr->ids)); 
						return(TRUE);
					}
					free(z);
				}
				free(y);
			}
			free(x);
		}
		free(surfaceptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkArray(char *name, int flags, REAL *x, INT maxsize, INT *size) {

	GV_ENTRY *entry;
	ARRAY    *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (ARRAY *) malloc(sizeof(ARRAY))) != NULL) {
		arrayptr->maxsize = maxsize;
		arrayptr->size = (size != NULL) ? size : &arrayptr->maxsize;
		arrayptr->x = x;
		if ( (entry = gv_make_entry(name, GV_ARRAY_LINK, flags)) != NULL) {
			entry->flags |= GVF_NORESIZE;
			entry->var.array = arrayptr;
			if (strchr(name, ':') == NULL && stricmp(name, "x") != 0 && stricmp(name, "y") != 0 && stricmp(name, "z") != 0) {
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				if (size == NULL) flags |= GVF_CONSTANT;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
			}
			return(TRUE);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkDoubleArray(char *name, int flags, DOUBLE *x, INT maxsize, INT *size) {

	GV_ENTRY *entry;
	DOUBLE_ARRAY *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (DOUBLE_ARRAY *) malloc(sizeof(DOUBLE_ARRAY))) != NULL) {
		arrayptr->maxsize = maxsize;
		arrayptr->size = (size != NULL) ? size : &arrayptr->maxsize;
		arrayptr->x = x;
		if ( (entry = gv_make_entry(name, GV_DOUBLE_ARRAY_LINK, flags)) != NULL) {
			entry->flags |= GVF_NORESIZE;
			entry->var.d_array = arrayptr;
			if (strchr(name, ':') == NULL) {
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				if (size == NULL) flags |= GVF_CONSTANT;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
			}
			return(TRUE);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkIntArray(char *name, int flags, INT *x, INT maxsize, INT *size) {

	GV_ENTRY *entry;
	INT_ARRAY *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (INT_ARRAY *) malloc(sizeof(INT_ARRAY))) != NULL) {
		arrayptr->maxsize = maxsize;
		arrayptr->size = (size != NULL) ? size : &arrayptr->maxsize;
		arrayptr->ival = x;
		if ( (entry = gv_make_entry(name, GV_INT_ARRAY_LINK, flags)) != NULL) {
			entry->flags |= GVF_NORESIZE;
			entry->var.i_array = arrayptr;
			if (strchr(name, ':') == NULL) {
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				if (size == NULL) flags |= GVF_CONSTANT;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
			}
			return(TRUE);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkComplexArray(char *name, int flags, COMPLEX *z, INT maxsize, INT *size) {

	GV_ENTRY *entry;
	COMPLEX_ARRAY  *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (COMPLEX_ARRAY *) malloc(sizeof(COMPLEX_ARRAY))) != NULL) {
		arrayptr->maxsize = maxsize;
		arrayptr->size = (size != NULL) ? size : &arrayptr->maxsize;
		arrayptr->z = z;
		if ( (entry = gv_make_entry(name, GV_COMPLEX_ARRAY_LINK, flags)) != NULL) {
			entry->var.c_array = arrayptr;
			if (strchr(name, ':') == NULL) {
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				if (size == NULL) flags |= GVF_CONSTANT;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
			}
			return(TRUE);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocFilePtr(char *name, int flags) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_FILEPTR, flags)) == NULL) return(FALSE);
	entry->var.fileptr    = NULL;
	entry->extra          = -1;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Allocate it as an empty string so can be tested.
--
-- If length is non-zero, then will be a blank filled string of specified
--     length.  Not normally used.
-- If length is zero, then no string will be allocated but the variable will 
--     be defined with a NULL pointer.  Gets replaced on first set command.
--------------------------------------------------------------------------- */
BOOL GVAllocString(char *name, int flags, int length) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_STRING, flags)) == NULL) return(FALSE);
	if (length > 0) {
		entry->var.stradr = malloc(length+1);
		memset(entry->var.stradr, ' ', length);
		entry->var.stradr[length] = '\0';
	} else {
		entry->var.stradr = NULL;
	}
	entry->extra          = 0;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVLinkString(char *name, int flags, char *string, INT length) {

	GV_ENTRY *entry;

	if ( (entry=gv_make_entry(name, GV_STRING_LINK, flags)) == NULL) return(FALSE);
	entry->var.stradr     = string;
	entry->extra          = length;
	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVAllocStrArray(char *name, int flags, INT length) {

	GV_ENTRY *entry;
	STRING_ARRAY *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (STRING_ARRAY *) malloc(sizeof(STRING_ARRAY))) != NULL) {
		arrayptr->maxsize = length;
		if ( (arrayptr->sval = calloc(length, sizeof(char *))) != NULL) {
			if ( (entry = gv_make_entry(name, GV_STRING_ARRAY, flags)) != NULL) {
				entry->extra       = length;
				arrayptr->size     = &entry->extra;
				entry->var.s_array = arrayptr;
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
				return(TRUE);
			}
			free(arrayptr->sval);
		}
		free(arrayptr);
	}
	return(FALSE);
}


/* ---------------------------------------------------------------------------
-- The linked array is an array of pointers.  The order of elements within 
-- that array may be changed by the program, but will always remain. There
-- is no checking on lengths of strings written to the linked array elements.
--------------------------------------------------------------------------- */
BOOL GVLinkStrArray(char *name, int flags, CHAR *s[], INT maxsize, INT *length) {

	GV_ENTRY *entry;
	STRING_ARRAY *arrayptr;
	char modname[VARNAME_STR_SIZE+10];

	if ( (arrayptr = (STRING_ARRAY *) malloc(sizeof(STRING_ARRAY))) != NULL) {
		arrayptr->sval = s;
		arrayptr->maxsize = maxsize;
		if ( (entry = gv_make_entry(name, GV_STRING_ARRAY_LINK, flags)) != NULL) {
			entry->extra   = maxsize;
			arrayptr->size = (length != NULL) ? length : &entry->extra ;
			entry->var.s_array = arrayptr;
			if (strchr(name, ':') == NULL) {
				flags = (flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
				if (length == NULL) flags |= GVF_CONSTANT;
				GVLinkInt ( strcat( strcpy(modname, name), ":npt"), flags, arrayptr->size);
			}
			return(TRUE);
		}
		free(arrayptr);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
-- Define a function expression
--
-- Usage: BOOL GVAllocFnc(char *name, int flags, char *def)
--
-- Inputs: name  - name of the function (including dummy variables)
--         flags - options
--         def   - definition of function (in terms of dummy vars)
--
-- Output: Adds an internal function definition to the function evaluator
--
-- Return: TRUE if successful
--
-- Notes:  The form is typically f(x,y) = a+b*x+c*y
--         String arguments must be identified in the function name with a
--         # character to identify string format.
--             f(x,y,#str) = a+b*x+c*y+strlen(str)
--------------------------------------------------------------------------- */
BOOL GVAllocFnc(char *name, int flags, char *def) {

	FUNCTION *function;
	GV_ENTRY *entry;
	int  malloc_size;
	char tmpname[DFLT_STR_SIZE], tmpdef[LONG_STR_SIZE];
	char *startptr, *endptr, *aptr, *bptr, *variable;
	int  iarg=0, sarg=0, aarg=0, narg=0, paren_count=0;
	ARGTYPE argtypes[MAX_FNC_ARGS];
	FNCTYPE fnctype = FNC_REAL;					/* Default is numeric */
	
/* Handle function type right off the start */
	if (*name == '#') {fnctype = FNC_STRING; name++;}
	
/* Local copy the rest */
	strlwr(strcpy(tmpname, name));
	strcpy(tmpdef, def);										/* Must maintain case for function def's */
	
/* ... Check validity of argument list -- must be (name, name, name) */
	if ( (startptr=strchr(tmpname, '(')) != NULL) {	/* No list */
		endptr = tmpname + strlen(tmpname) - 1;		/* Start at end				*/
		while (isspace(*endptr)) --endptr;				/* Strip blanks at end		*/
		if (*endptr != ')') return(FALSE);				/* Better be parenthesis	*/
		*(startptr++) = '\0';								/* Truncated function name */
		*endptr = '\0';										/* Mark end of vars			*/
	
		while (*startptr != '\0') {
			while (isspace(*startptr)) startptr++;
			if (narg >= MAX_FNC_ARGS) {
				ERRprintf("ERROR: Too many arguments in the function definition.  Limited to %d by configuration\n", MAX_FNC_ARGS);
				goto ParseError;
			}
			if (*startptr == '#') {
				argtypes[narg] = ARG_STRING;		startptr++;
			} else if (*startptr == '*') {
				argtypes[narg] = ARG_ARRAY;		startptr++;
			} else {
				argtypes[narg] = ARG_REAL;
			}
			if (! isalpha(*(variable=startptr))) goto ParseError;
			if ( (startptr=strchr(startptr,',')) != NULL) {
				aptr = startptr - 1;							/* aptr = end */
				if (*(++startptr) == '\0') goto ParseError;
			} else {
				aptr = (startptr=endptr) - 1;
			}
			while (isspace(*aptr)) aptr--;
			*(++aptr) = '\0';									/* Finishes *variable */
			for (aptr = variable; *aptr != '\0'; aptr++) {
				if ( (! isalnum(*aptr)) && (strchr(":$_", *aptr) == NULL)) 
				  	goto ParseError;
			}
			gv_replace_args(tmpdef, variable, argtypes[narg], iarg, sarg, aarg);

/* Keep track of how many of each type */
			if (argtypes[narg] == ARG_STRING) {
				sarg++;
			} else if (argtypes[narg] == ARG_REAL) {
				iarg++;
			} else if (argtypes[narg] == ARG_ARRAY) {
				aarg++;
			}
			narg++;
		}
	}

	aptr = bptr= tmpdef;

/* Last step, lowercase everything except the string functions */
	while (*aptr) {								/* Go through the full list */
		if (*aptr == '(' || *aptr == '{' || *aptr == '[') {
			paren_count++;
			*(bptr++) = '(';
		} else if (*aptr == ')' || *aptr == '}' || *aptr == ']') {
			paren_count--;
			*(bptr++) = ')';
		} else if (*aptr == '"') {						/* String variable - skip	*/
			*(bptr++) = *(aptr++);						/* Okay, first done		*/
			while (*aptr) {								/* As long as possible	*/
				if ( (*(bptr++) = *(aptr++)) == '"' ) {
					if (*aptr != '"') break;
					*(bptr++) = *(aptr++);				/* Double quotes			*/
				}
			}
			continue;										/* Don't need the final aptr++ */
		} else if (! isspace(*aptr)) {
			*(bptr++) = tolower(*aptr);
		}
		aptr++;
	}
	*bptr = '\0';											/* Truncate now */

	if (paren_count != 0) goto ParenError;

/* ... Now, allocate space and make the definitions */
	malloc_size = (int) (strlen(tmpdef) + strlen(name) + strlen(def) + sizeof(FUNCTION) + 3);
	if ( (function = (FUNCTION *) malloc(malloc_size)) != NULL) {
		aptr = function->vars;
		function->def       = strcpy(aptr, tmpdef); aptr = aptr+1+strlen(tmpdef);
		function->givenname = strcpy(aptr, name);   aptr = aptr+1+strlen(name);
		function->givendef  = strcpy(aptr, def);    aptr = aptr+1+strlen(def);

		function->fnctype   = fnctype;
		function->nargs     = narg;
		memcpy(function->argtypes, argtypes, MAX_FNC_ARGS*sizeof(*argtypes));
		if ( (entry=gv_make_entry(tmpname, GV_FUNCTION, flags)) != NULL) {
			entry->var.function = function;
			return(TRUE);
		}
		free(function);
	}
	return(FALSE);

ParseError:
	gen_err2("Illegal structure of function definition", name);
	return(FALSE);
ParenError:
	gen_err("Illegal function - parenthesis mismatch");
	return(FALSE);
}


/* ===========================================================================
-- Routine to replace all instances of a variable (say xvar) with the stack
-- reference &nn where nn is the argument number (0-99).  Used for parsing
-- functions both in GVAllocFnc and as part of functions like @solve.
--
-- Usage: int gv_replace_args(char *str, char *var, int iarg, BOOL string_var);
--
-- Inputs: str  - pointer to the function declaration
--         var  - pointer to the variable name
--         iarg - current argument position (0 based, separate # and string)
--         string_var - TRUE  ==> string reference, use &snn
--                      FALSE ==> float  reference, use &nn
--
-- Output: str - revised with elements replaces.  Assumes space exists.
--
-- Return: icnt - number of arguments that were replaced.
--
-- Note: Maximum string length is LONG_STR_SIZE.  Fixed!
--
-- Fix: The code 1E-7 would have the E replaced if an argument.  Ouch!
=========================================================================== */
int gv_replace_args(char *str, char *var, ARGTYPE type, int iarg, int sarg, int aarg) {
	char *look, tmp[LONG_STR_SIZE];
	int icnt = 0;
	size_t slen;

	slen = strlen(var);
	look = str;
	while (*look) {								/* Go through the full definition */
		if (*look == '"') {						/* String variable - skip	*/
			look++;
			while (*look) {						/* Skip over the string		*/
				if (*look == '"') {
					look++;
					if (*look != '"') break;	/* Okay, we've finished */
				}
				look++;
			}
		} else if (isalpha(*look)) {			/* Is this a variable position? */
#if 0	/* old version of the test below */
			if (strnicmp(look, var, slen) == 0 && strchr("+-*/!^()}] ,.=<>|&?:", *(look+slen)) != NULL) {
#endif
/** -------------------------------------------
 Painful problem.  define f(e) = 1.07E-7*E-3
 * The E is not a variable if
 * (1) var has length 1
 * (2) var is 'e'
 * (3) next char is a - sign
 * (4) previous char is a digit or a .
**/
			if (   (strnicmp(look, var, slen) == 0)		/* Matches name */
             && (! isalnum(*(look+slen)))					/* Not more of a variable name */
				 && (*(look+slen) != '_')						/* Also indicates a variable */
				 && (slen!=1 || tolower(*var)!='e' || *(look+1)!='-' || look==str || (*(look-1)!='.' && ! isdigit(*(look-1)))) ) {	/* Wierd 1.0E-7 */
				icnt++;
				*look = '\0';								/* Truncate here */
				if (type == ARG_REAL) {
					sprintf(tmp, "%s%c%2.2d%s",  str, STACK_REF_CHAR, iarg, look+slen);
					look += 3;
				} else if (type == ARG_STRING) {
					sprintf(tmp, "%s%cs%2.2d%s", str, STACK_REF_CHAR, sarg, look+slen);
					look += 4;
				} else if (type == ARG_ARRAY) {
					sprintf(tmp, "%s%ca%2.2d%s", str, STACK_REF_CHAR, aarg, look+slen);
					look += 4;
				} else {
					ERRprintf("ERROR: Programmers missing a case in gv_replace_args - tell them\n");
				}
				strcpy(str, tmp);
				continue;
			}
			while (isalnum(*look) || *look == '_') look++;	/* Not my variable, so copy to something non-variable */
		}
		while (*look && *look != '"' && ! isalpha(*look)) look++;
	}
	return icnt;
}


/* ===========================================================================
-- Routine to link in an external function to return a real value from
-- only real (or complex) arguments.  Use GVLinkFncA to get new version
-- allowing both string and numeric arguments.
--
-- Usage: BOOL GVLinkFnc(char *name, int flags, int nargs, EXT_FNC_LINK *fnc);
--
-- Inputs: name    - internal name for function evaluator
--         flags   - option flags for linked function (GVF_HIDDEN, etc.) - typically 0
--         nargs   - number of numeric arguments for the function
--         fnc     - pointer to the external function which will be called
--        
-- Output: none - creates internal structures for the function linkage
--
-- Return: TRUE if successful, FALSE on any type of error
--
-- Notes: The function is called as
--          int (*fnc)(int itype, TMPREAL *result, TMPREAL *args);
--             itype = 0 for a real evalution, 1 for a complex evaluation
--             *result - returned value
--             args[] = the real (or complex) arguments
--                 if itype == 0, these are TMPREAL
--                 if itype == 1, these are cast from the actual TMPCOMPLEX *
--                                and must be reassigned by fnc
--                 if a function does not want to handle complex arguments, 
--                 return -1 on a call where itype == 1, and the function will 
--                 will be recalled with real arguments (real part of each
--                 argument only)
--          The function returns 0 if successful, non-zero if a problem.     
--          If called from the complex value evaluator, a non-zero
--            return will cause the function to be called again with
--            only the real part of each numeric argument.
=========================================================================== */
BOOL GVLinkFnc	(char *name, int flags, int nargs, EXT_FNC_LINK *fnc) {

	GV_ENTRY *entry;
	FNC_LINK *ext_fnc;

/* First, deal with a couple of internal kludges */
	if (stricmp(name, "RGB_Color") == 0) {
		RGB_Color = (int (*)(char *)) fnc;
		return(TRUE);
	}

	ext_fnc = calloc(1, sizeof(*ext_fnc));						/* Allocate space for ext_fnc */
	ext_fnc->fnc = fnc;
	ext_fnc->nargs = nargs;
	memset(ext_fnc->argtypes, 0, sizeof(*ext_fnc->argtypes)*MAX_FNC_ARGS);	/* All are real arguments */

	if ( (entry=gv_make_entry(name, GV_FUNCTION_LINK, flags)) == NULL) {
		free(ext_fnc);
		return(FALSE);
	}
	entry->var.ext_fnc = ext_fnc;
	return(TRUE);
}


/* ===========================================================================
-- Routine to link in an external function to return a real value to
-- the function evaluator.  The function can depend on both string and 
-- numeric arguments.
--
-- Usage: BOOL GVLinkFncA(char *name, int flags, char *arglist, EXT_FNCA_LINK *fnc);
--
-- Inputs: name    - internal name for function evaluator
--         flags   - option flags for linked function (GVF_HIDDEN, etc.) - typically 0
--         arglist - string indicating number and type/order of arguments
--                   The number of arguments is the length of this string, and each
--                   position indicates the type of argument.  Currently "s" indicates
--                   a string variable, "r" indicates a number.  Any unrecognized
--                   character is assumed to be "r".
--                   Example:  "srr" is fnc(sval,rval,rval)
--                             "rsrrs" is fnc(rval,sval,rval,rval,sval)
--                   Although other letters will not cause an error, their use is
--                   discouraged for potential extensions to allow other types of 
--                   arguments (such as file pointers, etc.).
--         fnc     - pointer to the external function which will be called
--        
-- Output: none - creates internal structures for the function linkage
--
-- Return: TRUE if successful, FALSE on any type of error
--
-- Notes: The function is called as
--          int (*fnc)(int itype, TMPREAL *result, TMPREAL *args, char *sargs[]);
--             itype = 0 for a real evalution, 1 for a complex evaluation
--             *result gets the answer
--             args[] = the real (or complex) arguments
--                 if itype == 0, these are TMPREAL
--                 if itype == 1, these are cast from the actual TMPCOMPLEX *
--                                and must be reassigned by fnc
--                 if a function does not want to handle complex arguments, 
--                 return -1 on a call where itype == 1, and the function will 
--                 will be recalled with real arguments (real part of each
--                 argument only)
--             sargs[] - pointers to the string arguments.  Will be NULL
--                 if there are no string arguments
--          The function returns 0 if successful, non-zero if a problem.     
--          If called from the complex value evaluator, a non-zero
--            return will cause the function to be called again with
--            only the real part of each numeric argument.
=========================================================================== */
BOOL GVLinkFncA(char *name, int flags, char *arglist, EXT_FNCA_LINK *fnc) {

	GV_ENTRY *entry;
	FNCA_LINK *ext_fnc;
	int i, nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];

/* Figure out the argument type and order - convert to the flag version
 * of the structure.  A 1 bit indicates a string argment.  0x01 is first
 * argument, 0x02 is second, etc. */
	nargs = (int) strlen(arglist);
	if (nargs > MAX_FNC_ARGS) {
		ERRprintf("ERROR: Maximum number of arguments in an external function call is %d (%d)\n", MAX_FNC_ARGS, nargs);
		return FALSE;
	}
	for (i=0; *arglist != '\0'; i++,arglist++) {
		argtypes[i]  = (tolower(*arglist) == 's') ? ARG_STRING : ARG_REAL ;
	}

/* Create the data structure, and make an entry in the function list */
	ext_fnc = calloc(1, sizeof(*ext_fnc));				/* Allocate space for ext_fnc */
	ext_fnc->fnc = fnc;
	ext_fnc->nargs = nargs;
	memcpy(ext_fnc->argtypes, argtypes, sizeof(*argtypes)*MAX_FNC_ARGS);

	if ( (entry=gv_make_entry(name, GV_FUNCTION_A_LINK, flags)) == NULL) {
		free(ext_fnc);
		return(FALSE);
	}
	entry->var.ext_fnca = ext_fnc;

	return(TRUE);
}


/* ===========================================================================
-- Routine to link in an external function to return a string value to
-- the function evaluator.  The function can depend on both string and 
-- numeric arguments.
--
-- Usage: BOOL GVLinkStrFnc(char *name, int flags, char *arglist, EXT_STR_FNC_LINK *fnc);
--
-- Inputs: name    - internal name for function evaluator
--         flags   - option flags for linked function (GVF_HIDDEN, etc.) - typically 0
--         arglist - string indicating number and type/order of arguments
--                   The number of arguments is the length of this string, and each
--                   position indicates the type of argument.  Currently "s" indicates
--                   a string variable, "r" indicates a number.  Any unrecognized
--                   character is assumed to be "r".
--                   Example:  "srr" is fnc(sval,rval,rval)
--                             "rsrrs" is fnc(rval,sval,rval,rval,sval)
--                   Although other letters will not cause an error, their use is
--                   discouraged for potential extensions to allow other types of 
--                   arguments (such as file pointers, etc.).
--         fnc     - pointer to the external function which will be called
--        
-- Output: none - creates internal structures for the function linkage
--
-- Return: TRUE if successful, FALSE on any type of error
--
-- Notes: The function is called as
--          int (*fnc)(int itype, char **result, TMPREAL *args, char *sargs[]);
--             itype = 0 for a real evalution, 1 for a complex evaluation
--             **result get a malloc'd string answer (must be malloc'd)
--             args[] = the real (or complex) arguments
--                 if itype == 0, these are TMPREAL
--                 if itype == 1, these are cast from the actual TMPCOMPLEX *
--                                and must be reassigned by fnc
--                 if a function does not want to handle complex arguments, 
--                 return -1 on a call where itype == 1, and the function will 
--                 will be recalled with real arguments (real part of each
--                 argument only)
--             sargs[] - pointers to the string arguments.  Will be NULL
--                 if there are no string arguments
--          The function returns 0 if successful, non-zero if a problem.     
--          If called from the complex value evaluator, a non-zero
--            return will cause the function to be called again with
--            only the real part of each numeric argument.
=========================================================================== */
BOOL GVLinkStrFnc	(char *name, int flags, char *arglist, EXT_STR_FNC_LINK *fnc) {

	GV_ENTRY *entry;
	STR_FNC_LINK *str_ext_fnc;
	int i, nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];

/* Figure out the argument type and order - convert to the flag version
 * of the structure.  A 1 bit indicates a string argment.  0x01 is first
 * argument, 0x02 is second, etc. */
	nargs = (int) strlen(arglist);
	if (nargs > MAX_FNC_ARGS) {
		ERRprintf("ERROR: Maximum number of arguments in an external function call is %d (%d)\n", MAX_FNC_ARGS, nargs);
		return FALSE;
	}
	for (i=0; *arglist != '\0'; i++,arglist++) {
		argtypes[i]  = (tolower(*arglist) == 's') ? ARG_STRING : ARG_REAL ;
	}

/* Create the data structure, and make an entry in the function list */
	str_ext_fnc = calloc(1, sizeof(*str_ext_fnc));				/* Allocate space for str_ext_fnc */
	str_ext_fnc->fnc = fnc;
	str_ext_fnc->nargs = nargs;
	memcpy(str_ext_fnc->argtypes, argtypes, sizeof(*argtypes)*MAX_FNC_ARGS);

	if ( (entry=gv_make_entry(name, GV_STR_FUNCTION_LINK, flags)) == NULL) {
		free(str_ext_fnc);
		return(FALSE);
	}
	entry->var.str_ext_fnc = str_ext_fnc;

	return(TRUE);
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
char *GVFindString(char *varname) {
	
	GV_ENTRY *entry;
	if ( (entry = gv_find_entry(varname)) == NULL) return(NULL);
	if (entry->type == GV_STRING || entry->type == GV_STRING_LINK) {
		return(entry->var.stradr);
	} else if (entry->type == GV_FUNCTION) {
		return(entry->var.function->givendef);
	} else if (entry->type == GV_2DCURVE || entry->type == GV_3DCURVE
		|| entry->type == GV_SURFACE) {
		return(entry->var.curve->ids);
	} else {
		return(NULL);
	}
}


/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVGetInfo(char *varname, INT *type, void **ptr) {

	GV_ENTRY *entry;
	
	if ( (entry = gv_find_entry(varname)) == NULL) return(FALSE);
	if (type != NULL) *type = entry->type;
	if (ptr != NULL)  *ptr  = &(entry->var);
	return(TRUE);
}
	
/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
BOOL GVGetAdrInfo(char *varname, INT *type, void **ptr, INT *length) {

	void *myptr;
	int  mylength, mytype;

	if (gv_find_adr(varname, &mytype, &myptr, &mylength) == NULL) return(FALSE);
	if (type   != NULL) *type   = mytype;
	if (ptr    != NULL) *ptr    = myptr;
	if (length != NULL) *length = abs(mylength);
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Usage:  GVListVars(int masktypes, int maskflags, int options);
--
-- Inputs: types -- bit pattern corresponding to GV_INT, etc which will be 
--                  output.  Use 0xFFFF for all.
--         flags -- Do not display vars with this flag.
--         options -- options for information
--              0x01 ==> Print debug level detail about many variables
--
-- Output: prints values for
--         ((var->type & masktypes) != 0) && ((var->flags & maskflags) == 0) 
--------------------------------------------------------------------------- */
void GVListVars(int masktypes, int maskflags, int options) {
	
	static struct _NameType {
		int  type;
		char *name;
	} TypeNames[] = {	{GV_REAL,					"REAL"},
							{GV_REAL_LINK,				"REAL LINK"},
							{GV_DOUBLE,					"DOUBLE"},
							{GV_DOUBLE_LINK,			"DOUBLE LINK"},
							{GV_COMPLEX,				"COMPLEX"},
							{GV_COMPLEX_LINK,			"COMPLEX LINK"},
							{GV_INT,						"INTEGER"},
							{GV_INT_LINK,				"INTEGER LINK"},
							{GV_ARRAY,					"ARRAY"},
							{GV_ARRAY_LINK,			"ARRAY LINK"},
							{GV_DOUBLE_ARRAY,			"DOUBLE ARRAY"},
							{GV_DOUBLE_ARRAY_LINK,	"DOUBLE ARRAY LINK"},
							{GV_COMPLEX_ARRAY,		"COMPLEX ARRAY"},
							{GV_COMPLEX_ARRAY_LINK,	"COMPLEX ARRAY LINK"},
							{GV_INT_ARRAY,				"INTEGER ARRAY"},
							{GV_INT_ARRAY_LINK,		"INTEGER ARRAY LINK"},
							{GV_STRING,					"STRING"},
							{GV_STRING_LINK,			"STRING LINK"},
							{GV_STRING_ARRAY,			"STRING ARRAY"},
							{GV_STRING_ARRAY_LINK,	"STRING ARRAY LINK"},
							{GV_FUNCTION,				"FUNCTION"},
							{GV_FUNCTION_LINK,		"FUNCTION LINK"},
							{GV_FUNCTION_A_LINK,		"ALT FNC LINK"},
							{GV_STR_FUNCTION_LINK,	"STR FNC LINK"},
							{GV_2DCURVE,				"2D CURVE"},
							{GV_3DCURVE,				"3D CURVE"},
							{GV_SURFACE,				"3D SURFACE"},
							{GV_FILEPTR,				"FILE HANDLE"},
							{0xFFFF,						NULL} };

	struct _NameType *TypePtr;
	GV_ENTRY *entry;
	char		*varname, *str;
	int		type, length;
	double	rval, ival;
	BOOL debug;

	debug = (options & 0x01);

	for (entry=GVFirstEntry; entry!=NULL; entry=entry->next) {
		if ( (entry->type  & masktypes) == 0) continue;
		if ( (entry->flags & maskflags) != 0) continue;
		varname = entry->varname;
		type    = entry->type;
		TypePtr = TypeNames;
		while (TypePtr->type != 0xFFFF) {
			if (TypePtr->type == type) break;
			TypePtr++;
		}
		switch (type) {
			case GV_INT:
				TTYprintf("    1:%-15s  %d  %-15s: %d\n", TypePtr->name, entry->level, varname, entry->var.intvalue);
				break;
			case GV_INT_LINK:
				TTYprintf("    1:%-15s  %d  %-15s: %d\n", TypePtr->name, entry->level, varname, *(entry->var.intadr));
				break;
			case GV_REAL:
				TTYprintf("    1:%-15s  %d  %-15s: %g\n", TypePtr->name, entry->level, varname, entry->var.floatvalue);
				break;
			case GV_REAL_LINK:
				TTYprintf("    1:%-15s  %d  %-15s: %g\n", TypePtr->name, entry->level, varname, *(entry->var.floatadr));
				break;
			case GV_DOUBLE:
				TTYprintf("    1:%-15s  %d  %-15s: %g\n", TypePtr->name, entry->level, varname, entry->var.doublevalue);
				break;
			case GV_DOUBLE_LINK:
				TTYprintf("    1:%-15s  %d  %-15s: %g\n", TypePtr->name, entry->level, varname, *(entry->var.doubleadr));
				break;
			case GV_COMPLEX:
				TTYprintf("    1:%-15s  %d  %-15s: %g%+gj\n", TypePtr->name, entry->level, varname, entry->var.complexvalue.x, entry->var.complexvalue.y);
				break;
			case GV_COMPLEX_LINK:
				TTYprintf("    1:%-15s  %d  %-15s: %g%+gj\n", TypePtr->name, entry->level, varname, (entry->var.complexadr)->x, (entry->var.complexadr)->y);
				break;
			case GV_ARRAY:
			case GV_ARRAY_LINK:
				rval   = *(entry->var.array->x);
				length =  (entry->var.array->maxsize);
				TTYprintf("%5d:%-15s  %d  %-15s: %g\n", length, TypePtr->name, entry->level, varname, rval);
				break;
			case GV_DOUBLE_ARRAY:
			case GV_DOUBLE_ARRAY_LINK:
				rval   = *(entry->var.d_array->x);
				length =  (entry->var.d_array->maxsize);
				TTYprintf("%5d:%-15s  %d  %-15s: %g\n", length, TypePtr->name, entry->level, varname, rval);
				break;
			case GV_INT_ARRAY:
			case GV_INT_ARRAY_LINK:
				ival   = *(entry->var.i_array->ival);
				length =  (entry->var.i_array->maxsize);
				TTYprintf("%5d:%-15s  %d  %-15s: %d\n", length, TypePtr->name, entry->level, varname, ival);
				break;
			case GV_COMPLEX_ARRAY:
			case GV_COMPLEX_ARRAY_LINK:
				rval   = (entry->var.c_array->z)->x;
				ival   = (entry->var.c_array->z)->y;
				length =  (entry->var.c_array->maxsize);
				TTYprintf("%5d:%-15s  %d  %-15s: %g%+gj\n", length, TypePtr->name, entry->level, varname, rval, ival);
				break;
			case GV_STRING_ARRAY:
			case GV_STRING_ARRAY_LINK:
				length =  (entry->var.s_array->maxsize);
				TTYprintf("%5d:%-15s  %d  %-15s: %s\n", length, TypePtr->name, entry->level, varname, entry->var.s_array->sval[0]);
				break;
			case GV_2DCURVE:
			case GV_3DCURVE:
				str    = entry->var.curve->ids;
				length = entry->var.curve->nptmax;
				TTYprintf("%5d:%-15s  %d  %-15s: %s\n", length, TypePtr->name, entry->level, varname, str);
				break;
			case GV_SURFACE:
				str    = entry->var.surface->ids;
				length = entry->var.surface->npt;
				TTYprintf("%5d:%-15s  %d  %-15s: %s\n", length, TypePtr->name, entry->level, varname, str);
				break;
			case GV_STRING:
			case GV_STRING_LINK:
				length = entry->extra;
				TTYprintf("%5d:%-15s  %d  %-15s: %s\n", length, TypePtr->name, entry->level, varname, entry->var.stradr);
				break;
			case GV_FUNCTION:
				varname = entry->var.function->givenname;
				str     = entry->var.function->givendef;
				TTYprintf("    1:%-15s  %d  %-15s: %s\n", TypePtr->name, entry->level, varname, str);
				if (debug) {
					int i;
					switch (entry->var.function->fnctype) {
						case FNC_REAL:
							TTYprintf("\t\ttype:  REAL\n"); break;
						case FNC_COMPLEX: 
							TTYprintf("\t\ttype:  COMPLEX\n"); break;
						case FNC_STRING: 
							TTYprintf("\t\ttype:  STRING\n"); break;
						default:
							TTYprintf("\t\ttype:  <unknown>\n"); break;
					}
					TTYprintf("\t\tdef:   %s\n", entry->var.function->def);
					TTYprintf("\t\tnargs: %d\n", entry->var.function->nargs);
					for (i=0; i<entry->var.function->nargs; i++) {
						switch (entry->var.function->fnctype) {
							case ARG_REAL:
								TTYprintf("\t\t\t[%d]: REAL\n"); break;
							case ARG_STRING: 
								TTYprintf("\t\t\t[%d]: STRING\n"); break;
							case ARG_ARRAY: 
								TTYprintf("\t\t\t[%d]: ARRAY\n"); break;
							default:
								TTYprintf("\t\t\t[%d]: <unknown>\n"); break;
						}
					}
				}
				break;
			case GV_FUNCTION_LINK:
				str = entry->var.stradr;	/* Actually fnc, but assuming equivalence */
				TTYprintf("    1:%-15s  %d  %-15s: %p\n", TypePtr->name, entry->level, varname, str);
				break;
			case GV_FUNCTION_A_LINK:
				str = entry->var.stradr;	/* Actually fnc, but assuming equivalence */
				TTYprintf("    1:%-15s  %d  %-15s: %p\n", TypePtr->name, entry->level, varname, str);
				break;
			case GV_STR_FUNCTION_LINK:
				str = entry->var.stradr;	/* Actually fnc, but assuming equivalence */
				TTYprintf("    1:%-15s  %d  %-15s: %p\n", TypePtr->name, entry->level, varname, str);
				break;
			case GV_FILEPTR:
				TTYprintf("    1:%-15s  %d  %-15s: %p\n", TypePtr->name, entry->level, varname, entry->var.fileptr);
				break;
			default:
				TTYprintf("    1:%-15s  %d  %-15s:\n", "UNKNOWN", entry->level, varname);
		}
	}
	return;
}

/* ---------------------------------------------------------------------------
--------------------------------------------------------------------------- */
void GVWriteVars(FILE *funit, int masktypes, int maskflags) {
	
	GV_ENTRY *entry;
	char	 *varname;
	BOOL ComplexSet = FALSE;
	
	for (entry=GVFirstEntry; entry!=NULL; entry=entry->next) {
		if (entry->flags & GVF_INTERNAL) continue;
		if ( (entry->type  & masktypes) == 0) continue;
		if ( (entry->flags & maskflags) != 0) continue;
		varname = entry->varname;								/* Useful variable */
		switch (entry->type) {
			case GV_INT:
				fprintf(funit, "alloc %s integer let %s = %d\n", varname, varname, entry->var.intvalue);
				break;
			case GV_REAL:
				fprintf(funit, "alloc %s real let %s = %g\n", varname, varname, entry->var.floatvalue);
				break;
			case GV_DOUBLE:
				fprintf(funit, "alloc %s double let %s = %g\n", varname, varname, entry->var.doublevalue);
				break;
			case GV_COMPLEX:
				if (! ComplexSet) {fprintf(funit, "Mathmode complex\n"); ComplexSet = TRUE;}
				fprintf(funit, "alloc %s complex let %s = (%g%+gj)\n", varname, varname, entry->var.complexvalue.x, entry->var.complexvalue.y);
				break;
			case GV_ARRAY:
				fprintf(funit, "alloc %s array %5i\n", varname, entry->var.array->maxsize);
				break;
			case GV_COMPLEX_ARRAY:
				fprintf(funit, "alloc %s c_array %5i\n", varname, entry->var.c_array->maxsize);
				break;
			case GV_2DCURVE:
				fprintf(funit, "alloc %s 2D %5i\n", varname, entry->var.curve->nptmax);
				break;
			case GV_3DCURVE:
				fprintf(funit, "alloc %s 3D %5i\n", varname, entry->var.curve->nptmax);
				break;
			case GV_SURFACE:
				fprintf(funit, "alloc %s SURFACE %5i %5i\n", varname, entry->var.surface->nrow, entry->var.surface->ncol);
				break;
			case GV_STRING:
				fprintf(funit, "declare %s = \"%s\"\n", varname, entry->var.stradr);
				break;
			case GV_FUNCTION:
				fprintf(funit, "define %s = %s\n", entry->var.function->givenname, entry->var.function->givendef);
				break;
			case GV_REAL_LINK:
				fprintf(funit, "let %s = %g\n", varname, *(entry->var.floatadr));
				break;
			case GV_DOUBLE_LINK:
				fprintf(funit, "let %s = %g\n", varname, *(entry->var.doubleadr));
				break;
			case GV_COMPLEX_LINK:
				if (! ComplexSet) {fprintf(funit, "Mathmode complex\n"); ComplexSet = TRUE;}
				fprintf(funit, "let %s = (%g%+gj)\n", varname, (entry->var.complexadr)->x, (entry->var.complexadr)->y);
				break;
			case GV_INT_LINK:
				fprintf(funit, "let %s = %d\n", varname, *(entry->var.intadr));
				break;
			case GV_STRING_LINK:
				fprintf(funit, "let %s = \"%s\"\n", varname, entry->var.stradr);
				break;
		}
	}

	return;
}


/* ---------------------------------------------------------------------------
-- Usage:  void *GVEnumVars(void *start, int masktypes, int maskflags, 
--                    char *name[], void *adr[], int *type[], int *nmax);
--
-- Inputs: start -- pointer returned from previous call to GVEnumVars
--                  indicating point to continue reading, or NULL to restart
--         types -- bit pattern corresponding to GV_INT, etc which will be 
--                  output.  Use 0xFFFF for all.
--         flags -- Do not display vars with this flag.
--         name  -- pointer to array which will receive address of the "name"
--         adr   -- pointer to array to receive address of "value"
--         type  -- pointer to array to receive variable type
--         nmax  -- pointer to integer containing number of elements that can
--                  be stored in name,adr and number actually stored on return.
--
-- Notes: In general, the name is the internal one.  For functions, it is the
--        given function name (ie. f(x,y)) and the adr points to the given
--        definition (ie. sin(x*y)).
--            GV_INT      GV_INT_LINK          pointer to INTEGER
--            GV_REAL     GV_REAL_LINK         pointer to REAL
--            GV_COMPLEX  GV_COMPLEX_LINK      pointer to COMPLEX
--            GV_ARRAY    GV_ARRAY_LINK        pointer to REAL
--            GV_COMPLEX_ARRAY ...             pointer to COMPLEX
--            GV_2DCURVE  GV_3DCURVE           pointer to string IDS
--            GV_STRING   GV_STRING_LINK       pointer to string
--            GV_FUNCTION                      pointer to given definition
--            GV_FUNCTION_LINK                 pointer to function
--            GV_FUNCTION_A_LINK               pointer to function
--            GV_STR_FUNCTION_LINK             pointer to function
--
--        Because the address of variables are passed, calling routines may
--        be expected to change values.  Consequently, GVF_CONSTANT variables
--        are not enumerated by this list.
--
-- Returns: NULL -- Successfully scanned entire list
--          !NULL -- pointer to return as start next time to continue scan.
--------------------------------------------------------------------------- */
void *GVEnumVars(void *start, int masktypes, int maskflags,
					 char *name[], void *adr[], int type[], int *nmax) {
	
	GV_ENTRY *entry;
	int nhave=0, itype;

	maskflags |= GVF_CONSTANT;					/* Never return constants */

	entry = (start == NULL) ? GVFirstEntry : (GV_ENTRY *) start;
	
	while ( (entry != NULL) && (nhave < *nmax) ) {
		if ( (entry->type & masktypes) && !(entry->flags & maskflags) ) {
			*name++ = entry->varname;
			*type++ = itype = entry->type;
			nhave++;
			switch (itype) {
				case GV_INT:
					*adr++  = &entry->var.intvalue;
					break;
				case GV_INT_LINK:
					*adr++ = entry->var.intadr;
					break;
				case GV_REAL:
					*adr++ = &entry->var.floatvalue;
					break;
				case GV_REAL_LINK:
					*adr++ = entry->var.floatadr;
					break;
				case GV_DOUBLE:
					*adr++ = &entry->var.doublevalue;
					break;
				case GV_DOUBLE_LINK:
					*adr++ = entry->var.doubleadr;
					break;
				case GV_COMPLEX:
					*adr++ = &entry->var.complexvalue;
					break;
				case GV_COMPLEX_LINK:
					*adr++ = entry->var.complexadr;
					break;
				case GV_ARRAY:
				case GV_ARRAY_LINK:
					*adr++ = entry->var.array->x;
					break;
				case GV_COMPLEX_ARRAY:
				case GV_COMPLEX_ARRAY_LINK:
					*adr++ = entry->var.c_array->z;
					break;
				case GV_2DCURVE:
				case GV_3DCURVE:
					*adr++ = entry->var.curve->ids;
					break;
				case GV_SURFACE:
					*adr++ = entry->var.surface->ids;
					break;
				case GV_STRING:
				case GV_STRING_LINK:
					*adr++ = entry->var.stradr;
					break;
				case GV_STRING_ARRAY:
				case GV_STRING_ARRAY_LINK:
					*adr++ = entry->var.s_array->sval;
					break;
				case GV_FUNCTION:
					name--;
					*name++ = entry->var.function->givenname;
					*adr++  = entry->var.function->givendef;
					break;
				case GV_FUNCTION_LINK:
					*adr++ = (void *) entry->var.ext_fnc->fnc;
					break;
				case GV_FUNCTION_A_LINK:
					*adr++ = (void *) entry->var.ext_fnca->fnc;
					break;
				case GV_STR_FUNCTION_LINK:
					*adr++ = (void *) entry->var.str_ext_fnc->fnc;
					break;
				default:									/* Undo the get		*/
					(*nmax)++;
					nhave--;
			}
		}
		entry = entry->next;
	}
	
	*nmax = nhave;										/* Number we read		*/
	return(entry);										/* And return values */
}

/* ---------------------------------------------------------------------------
Usage: 
	void *entry=NULL, *adr;
	char *varname;
	int type;

	while (GVGetNextEntry(&entry, &varname, &type, &adr)) {
	}
--------------------------------------------------------------------------- */
BOOL GVGetNextEntry(void **entry, char **name, INT *type, INT *flags, void **adr) {

	if (*entry == NULL) {
		*entry = (void *) GVFirstEntry;
	} else if ( ((GV_ENTRY *) *entry)->ID != MY_GV_ID) {
		*entry = NULL;
	} else {
		*entry = (void *) ((GV_ENTRY *) *entry)->next;
	}

	if (*entry != NULL) {
		if (name  != NULL) *name  =  ((GV_ENTRY *) *entry)->varname;
		if (type  != NULL) *type  =  ((GV_ENTRY *) *entry)->type;
		if (flags != NULL) *flags =  ((GV_ENTRY *) *entry)->flags;
		if (adr   != NULL) *adr   = &((GV_ENTRY *) *entry)->var;
	}
	return(*entry != NULL);
}

/* ---------------------------------------------------------------------------
-- Routine to delete an entry from the table.  Release all resources used
--
-- Syntax:  BOOL GVDeallocate(char *name);
--
-- Notes: Will accept deletion of functions when specified with their
--        arguments.  GVDeallocate("f(x)") is same as GVDeallocate("f");
--------------------------------------------------------------------------- */
BOOL GVDeallocate (char *name) {

	int rc;
	char localname[VARNAME_STR_SIZE+1], *aptr;

	rc = gv_del_entry_by_name(name);
	if (rc != +1) return(rc == 0);			/* +1 ==> not found */

/* ... Check if of function call form */
	if ( (aptr = strchr(name, '(')) == NULL) return(FALSE);
	rc = (int) (aptr-name);
	if (rc >= sizeof(localname)) return(FALSE);
	strncpy(localname, name, rc); localname[rc] = '\0';
	rc = gv_del_entry_by_name(localname);
	return(rc == 0);
}

/* ---------------------------------------------------------------------------
-- Routine to resize a specified curve or array.  Will reallocate the memory 
-- space and copy as much of existing arrays as possible.
--
-- Usage:   BOOL GVResize(char *name, int length);
--
-- Inputs:  name   - name of a curve or allocated array to be resized
--          length - new size of the curve/array
--
-- Output:  none
--
-- Returns: TRUE  -- able to resize as desired
--          FALSE -- unable because not existing, not curve or invalid size
--------------------------------------------------------------------------- */
BOOL GVResize(char *name, int length) {

	GV_ENTRY *entry;
	CURVE    *curve;
	REAL	   *x, *y, *z;
	COMPLEX	*cz;
	char		modname[VARNAME_STR_SIZE+10];
	int		myflags;
	INT		*pNpt;
	
	if ( (length > GVI_MAX_LENGTH) || 						/* Is request valid?	*/
		  ( (entry = gv_find_entry(name)) == NULL) ||	/* Does it exist at all */
		  (entry->flags & GVF_NORESIZE))			 			/* Operation allowed	*/
		return(FALSE);

	switch (entry->type) {
		case GV_ARRAY:
			if ( (x = (REAL *) realloc(entry->var.array->x,length*sizeof(REAL))) == NULL) break;
			entry->var.array->x        = x;
			*entry->var.array->size    = min(length, *entry->var.array->size);
			entry->var.array->maxsize  = length;
			return(TRUE);

		case GV_COMPLEX_ARRAY:
			if ( (cz = (COMPLEX *) realloc(entry->var.c_array->z,length*sizeof(COMPLEX))) == NULL) break;
			entry->var.c_array->z       = cz;
			*entry->var.c_array->size   = min(length, *entry->var.c_array->size);
			entry->var.c_array->maxsize = length;
			return(TRUE);

		case GV_2DCURVE:
		case GV_3DCURVE:
			if (! (entry->flags & GVF_ARRAY_ALLOCATED) ) return(FALSE);
			curve = entry->var.curve;
			pNpt = &curve->npt;										/* Pointer to NPT	*/
			myflags = (entry->flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL;
			curve->nptmax = min(length, curve->nptmax);		/* In case we fail */
			curve->npt    = min(length, curve->npt);			/* And for always	 */
			if ( (x = (REAL *) realloc(curve->x,length*sizeof(REAL))) == NULL) break;
			curve->x = x;
			GVLinkArray(strcat(strcpy(modname,name),":x"), myflags, x, length, pNpt);
			if ( (y = (REAL *) realloc(curve->y,length*sizeof(REAL))) == NULL) break;
			curve->y = y;
			GVLinkArray(strcat(strcpy(modname,name),":y"), myflags, y, length, pNpt);
			if (entry->type == GV_3DCURVE) {
				if ( (z = (REAL *) realloc(curve->z,length*sizeof(REAL))) == NULL) break;
				curve->z = z;
				GVLinkArray(strcat(strcpy(modname,name),":z"), myflags, z, length, pNpt);
			}
			curve->nptmax = length;									/* Now real length */
			return(TRUE);
	}
	return(FALSE);
}

/* ---------------------------------------------------------------------------
-- Routine to modify a curve between 2D/3D.  Going to 2D, Z lost.  Going to
-- 3D, Z initially set to zero.
--
-- Usage:   BOOL GVModifyCurve(char *name, int type);
--
-- Inputs:  name   - name of a curve or allocated array to be resized
--
-- Output:  none
--
-- Returns: TRUE  -- able to modify as desired
--          FALSE -- unable because not exist, not curve, or not resizable
--------------------------------------------------------------------------- */
BOOL GVModifyCurve(char *name, int type) {

	GV_ENTRY *entry;
	CURVE    *curve;
	char		zname[VARNAME_STR_SIZE+10];
	INT      i;
	
	if ( (type != GV_2DCURVE && type != GV_3DCURVE) ||
		  ((entry = gv_find_entry(name)) == NULL)    ||
		  (entry->type != GV_2DCURVE && entry->type != GV_3DCURVE) )
		return(FALSE);
	if (entry->type == type) return(TRUE);			/* Nothing to do */
	if ( (entry->flags & GVF_NORESIZE) || !(entry->flags & GVF_ARRAY_ALLOCATED) )
		return(FALSE);
	curve = entry->var.curve;
	strcat(strcpy(zname,name),":z");

	if (entry->type == GV_3DCURVE) {			/* Want it to become 2D */
		entry->type = GV_2DCURVE;				/* Now 2D curve */
		free(curve->z);							/* Release the Z memory */
		curve->z = NULL;							/* And mark it gone		*/
		gv_del_entry_by_name(zname);
	} else {
		entry->type = GV_3DCURVE;
		curve->z = (REAL *) calloc(curve->nptmax, sizeof(REAL));
		for (i=0; i<curve->npt; i++) curve->z[i] = 0.0f;
		GVLinkArray(zname, (entry->flags & ~USER_FLAG_MASK) | GVF_HIDDEN | GVF_INTERNAL,
			curve->z, curve->nptmax, &curve->npt);
	}
	return(TRUE);
}

/* ---------------------------------------------------------------------------
-- Routine to set value of an internal variable.  Handle CURVE/ARRAY etc.
--
-- Setting a curve only sets the value of the IDS expression -- ie. equivalent
-- to a string expression.
--
-- Setting a member of an array is valid here as well.  Arrays outside of valid
-- range, error message will be printed.
--
--   Numeric types: Evaluate the expression and assign the value
--   String type:   If GVGuessExprType returns GVP_STRING, then evaluate
--                         otherwise just assign
--------------------------------------------------------------------------- */
BOOL GVSetValue(char *name, char *expression) {

	GV_ENTRY *entry;
	int i, err, type, length, errcnt=0;
	void *adr;													/* Random address */
	char *aptr;
	FILE *funit;
	char **sptr;												/* String pointers */
	GVCMDS *cmds;
	INT *ip;
	REAL *x;
	COMPLEX *z;
	
	if ( (entry = gv_find_adr(name, &type, &adr, &length)) == NULL) {
		ERRprintf("ERROR (GVSetValue): Could not find name (%s)\n", name);
		return(FALSE);
	}

	if (entry->flags & GVF_CONSTANT) {
		ERRprintf("ERROR: Variable %s is marked as constant -- cannot be modified\n", name);
		return(FALSE);
	} else if (length < 0 && (GVMathMode & MATH_SAFETY_ON)) {
		ERRprintf("ERROR: Array element %s beyond array limits.  Operation aborted.\n", name);
		return(FALSE);
	} else if (length < 0 && (GVMathMode & MATHWARN)) {
		TTYprintf("WARNING: Array element %s beyond array bounds.\n", name);
	}

	switch (type) {											/* Many handled here! */
		case GV_POINTER:
			ERRprintf("ERROR: Cannot set the value of a pointer at this time\n");
			return(FALSE);

		case GV_FILEPTR:										/* These are difficult */
			funit = *((FILE **) adr);
			if (funit != NULL && funit != stdin && funit != stdout && funit != stderr) fclose(funit);
			funit = (FILE *) GVEvalPtrExpr(expression, &err, GVP_FILEPTR);
			*((FILE **) adr) = (err == 0) ? funit : NULL;
			return(err==0);
			
		case GV_REAL:
			*((REAL *) adr) = GVTrimToReal(GVEvalExpr(expression, &err));
			return(err==0);

		case GV_DOUBLE:
			*((DOUBLE *) adr) = GVTrimToDouble(GVEvalExpr(expression, &err));
			return(err==0);

		case GV_COMPLEX:
			*((COMPLEX *) adr) = GVTrimToComplex(GVEvalComplexExpr(expression, &err));
			return(err==0);

		case GV_INT:
			*((INT *) adr) = (INT) GVTrimToNint(GVEvalExpr(expression, &err));
			if (entry->validate != NULL) (*entry->validate)(entry);
			return(err==0);

		case GV_ARRAY:
			if ( (cmds = GVParse(expression, NULL)) == NULL) return(FALSE);
			if (GVMathMode & MATH_INPLACE) {
				x = (REAL *) adr;
			} else {
				x = malloc(sizeof(*x) * length);
			}
			GVLocalIndex = 0;
			for (i=0; i<length; i++) {
				x[i] = GVTrimToReal(gv_eval_cmds(cmds, &err).x);
				if (err != 0 && ++errcnt > 10) break;
				if (SysChkBreak(FALSE)) break;
				GVLocalIndex++;
			}
			if (! (GVMathMode & MATH_INPLACE)) {
				memcpy(adr, x, sizeof(*x)*i);			/* Copy number done */
				free(x);
			}
			return( (errcnt < 10) && ! SysChkBreak(FALSE) );

		case GV_INT_ARRAY:
			if ( (cmds = GVParse(expression, NULL)) == NULL) return(FALSE);
			if (GVMathMode & MATH_INPLACE) {
				ip = (INT *) adr;
			} else {
				ip = malloc(sizeof(*ip) * length);
			}
			GVLocalIndex = 0;
			for (i=0; i<length; i++) {
				ip[i] = GVTrimToNint(gv_eval_cmds(cmds, &err).x);
				if (err != 0 && ++errcnt > 10) break;
				GVLocalIndex++;
			}
			if (! (GVMathMode & MATH_INPLACE)) {
				memcpy(adr, ip, sizeof(*ip)*i);
				free(ip);
			}
			return( (errcnt < 10) );
			
		case GV_COMPLEX_ARRAY:
			if ( (cmds = GVParse(expression, NULL)) == NULL) return(FALSE);
			gv_force_complex(cmds);
			if (GVMathMode & MATH_INPLACE) {
				z = (COMPLEX *) adr;
			} else {
				z = malloc(sizeof(*z) * length);
			}
			GVLocalIndex = 0;
			for (i=0; i<length; i++) {
				z[i] = GVTrimToComplex(gv_eval_cmds(cmds, &err));
				if (err != 0 && ++errcnt > 10) break;
				if (SysChkBreak(FALSE)) break;
				GVLocalIndex++;
			}
			if (!(GVMathMode & MATH_INPLACE)) {
				memcpy(adr, z, sizeof(*z)*i);			/* Copy number done */
				free(z);
			}
			return( (errcnt < 10) && ! SysChkBreak(FALSE) );

		case GV_STRING_ARRAY:
		case GV_STRING_ARRAY_LINK:
			if ( (cmds = GVParseEx(expression, NULL, GVP_STRING)) == NULL) return(FALSE);
			GVLocalIndex = 0;
			sptr = (char **) adr;
			for (i=0; i<length; i++) {
				if (type == GV_STRING_ARRAY && sptr[i] != NULL) free(sptr[i]);
				aptr = gv_eval_str_cmds(cmds, &err);
				if (type == GV_STRING_ARRAY) {
					sptr[i] = aptr;
				} else {
					strcpy(sptr[i], aptr);
					free(aptr);
				}
				if (err != 0 && ++errcnt > 10) break;
				GVLocalIndex++;
			}
			return( (errcnt < 10) );
	}

/* Others are string type, go back to original entry table to handle properly */
/* First, check if we can evaluate it as a string expression */
	if (type != GV_STRING) {
		ERRprintf("ERROR: Variable type 0x%x not handled by GVSetValue()\n", type);
		return(FALSE);
	}

	aptr = NULL;
	if (GVGuessExprType(expression) == GVP_STRING) aptr = GVEvalStrExpr(expression, &err);
	if (aptr != NULL && err == 0) expression = aptr;
	
	switch (entry->type) {										/* Look at real base type */
		case GV_STRING:
			if ( (entry->var.stradr == NULL) || (int) strlen(expression) > entry->extra) {
				entry->extra = (int) strlen(expression);
				if (entry->var.stradr != NULL) free(entry->var.stradr);
				entry->var.stradr = strdup(expression);
			} else {
				strcpy(entry->var.stradr, expression);
			}
			break;
		case GV_STRING_LINK:
			strscpy(entry->var.stradr, expression, entry->extra);
			break;
		case GV_STRING_ARRAY:
			if (length < 0) break;								/* Don't set if outside bounds */
			i = entry->tmpuse;									/* Kludge usage					*/
			free(entry->var.s_array->sval[i]);
			entry->var.s_array->sval[i] = strdup(expression);
			break;
		case GV_STRING_ARRAY_LINK:
			if (length < 0) break;								/* Don't set if outside bounds */
			strcpy((char *) adr, expression);
			break;

		case GV_2DCURVE:
		case GV_3DCURVE:
			strscpy(entry->var.curve->ids, expression, sizeof(entry->var.curve->ids));
			break;
		case GV_SURFACE:
			strscpy(entry->var.surface->ids, expression, sizeof(entry->var.surface->ids));
			break;
		default:
			return(FALSE);
	}

	if (aptr != NULL) free(aptr);
	return(TRUE);
}


/* ---------------------------------------------------------------------------
-- Routine to fill a passed array with values from an expression.
--------------------------------------------------------------------------- */
#if 0
#include <time.h>
#define	TIME_DEBUG(point)	TTYprintf("%s time: %d\n", point, clock()-itime), itime=clock(), TTYflush();
	clock_t itime;
	itime = clock();
	TIME_DEBUG("Eval");
#endif

BOOL GVEvalArrayExpr(REAL *adr, INT length, char *expression) {

	int i, err, errcnt=0;
	GVCMDS *cmds;
	REAL *x;

	if ( (cmds = GVParse(expression, NULL)) == NULL) return(FALSE);

	if (GVMathMode & MATH_INPLACE) {
		x = adr;
	} else {
		x = malloc(sizeof(*x) * length);
	}
	GVLocalIndex = 0;

	for (i=0; i<length; i++) {
		x[i] = GVTrimToReal(gv_eval_cmds(cmds, &err).x);
		if (err != 0 && ++errcnt > 10) break;
		GVLocalIndex++;
	}
	if (! (GVMathMode & MATH_INPLACE)) {
		memcpy(adr, x, sizeof(*x)*i);			/* Copy number done */
		free(x);
	}

	return( (errcnt < 10) );
}

/* ---------------------------------------------------------------------------
-- Routine to delete an entry from the table.  Release all resources used
--
-- Returns: 0 or positive is successful.  Negative indicates failure.
--          0 ==> Entry exists and was deleted
--         +1 ==> Entry requested was NULL
--         -1 ==> Delete not allowed (via GVF_NODELETE bit)
--         -2 ==> Delete unsuccessful for strange reason
--------------------------------------------------------------------------- */
PRIVATE int gv_del_entry_by_name(char *name) {
	
	GV_ENTRY *entry;

	if ((entry = gv_find_entry(name)) == NULL) return(+1);	/* Not there */
	return gv_del_entry(entry, TRUE);
}
	

PRIVATE int gv_del_entry(GV_ENTRY *entry, BOOL do_all) {

	char modname[VARNAME_STR_SIZE+10];
	int i;

	if  (entry->flags & GVF_NODELETE) return(-1);	/* Not allowed */

/* Remake the linked list -- deleting this entry */
	if (entry->last != NULL) {					/* First in chain? */
		(entry->last)->next = entry->next;
	} else {
		GVFirstEntry = entry->next;
	}
	if (entry->next != NULL) {					/* Last in chain? */
		(entry->next)->last = entry->last;
	}

/* Delete any allocated memory blocks in the structures */
	switch (entry->type) {
		case GV_3DCURVE:
		case GV_2DCURVE:
			if (do_all) {
				if (entry->type == GV_3DCURVE)
					gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":z") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":y") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":x") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":ids") );
			}
			if (entry->flags & GVF_ARRAY_ALLOCATED) {
				if (entry->type == GV_3DCURVE) free(entry->var.curve->z);
				free(entry->var.curve->y);
				free(entry->var.curve->x);
			}
			if (do_all) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			free(entry->var.curve);
			break;
		case GV_SURFACE:
			if (do_all) {
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":z") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":y") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":x") );
				gv_del_entry_by_name( strcat( strcpy(modname, entry->varname), ":ids") );
			}
			if (entry->flags & GVF_ARRAY_ALLOCATED) {
				free(entry->var.surface->z);
				free(entry->var.surface->y);
				free(entry->var.surface->x);
			}
			if (do_all) {
				gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":ncol") );
				gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":nrow") );
			}
			free(entry->var.surface);
			break;
		case GV_ARRAY:
			free(entry->var.array->x);
			free(entry->var.array);
			if (do_all) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_ARRAY_LINK:
			free(entry->var.array);
			if (do_all && strchr(entry->varname,':') == NULL) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_DOUBLE_ARRAY_LINK:
			free(entry->var.d_array);
			if (do_all && strchr(entry->varname,':') == NULL) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_INT_ARRAY_LINK:
			free(entry->var.i_array);
			if (do_all && strchr(entry->varname,':') == NULL) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_COMPLEX_ARRAY:
			free(entry->var.c_array->z);
			free(entry->var.c_array);
			if (do_all) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_COMPLEX_ARRAY_LINK:
			free(entry->var.c_array);
			if (do_all && strchr(entry->varname,':') == NULL) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
		case GV_FUNCTION:
			free(entry->var.function);
			break;
		case GV_STRING:
			free(entry->var.stradr);
			break;
		case GV_STRING_ARRAY:
			for (i=0; i<entry->var.s_array->maxsize; i++) free(entry->var.s_array->sval[i]);
			free(entry->var.s_array->sval);
			free(entry->var.s_array);
			break;
		case GV_STRING_ARRAY_LINK:
			free(entry->var.s_array);
			if (do_all && strchr(entry->varname,':') == NULL) gv_del_entry_by_name( strcat( strcpy(modname,entry->varname), ":npt") );
			break;
	}

/* And free the entry */
	free(entry);
	return(0);
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
char *GVEvalStrCmdsI(GVCMDS *cmds, int i, int *err) {
	GVLocalIndex = i;
	return(gv_eval_str_cmds(cmds, err));
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
char *GVEvalStrCmds(GVCMDS *cmds, int *err) {
	GVLocalIndex = 0;
	return( gv_eval_str_cmds(cmds, err));
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate pointer expressions
---------------------------------------------------------------------------- */
void *GVEvalPtrExpr(char *expr, int *err, GVP_PARSEMODE mode) {

	GVCMDS *cmds;
	int i;

	GVLocalIndex = 0;

	if (err == NULL) err = &i;
	if ( (cmds = GVParseEx(expr, NULL, mode)) == NULL) {
		*err = 1;
		return(NULL);
	}
	
	return( gv_eval_ptr_cmds(cmds, err, mode));
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
char *GVEvalStrExpr(char *expr, int *err) {

	GVCMDS *cmds;
	int i;
	
	GVLocalIndex = 0;

	if (err == NULL) err = &i;								/* Just so err valid */
	if ( (cmds = GVParseEx(expr, NULL, GVP_STRING)) == NULL) {		/* Can we parse?		*/
		*err = 1;
		return(NULL);
	}
	return( gv_eval_str_cmds(cmds, err));
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
TMPREAL GVEvalCmdsI(GVCMDS *cmds, int i, int *err) {

	GVLocalIndex = i;
	return( gv_eval_cmds(cmds, err).x);

}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
TMPREAL GVEvalCmds(GVCMDS *cmds, int *err) {

	GVLocalIndex = 0;
	return( gv_eval_cmds(cmds, err).x);

}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
TMPREAL GVEvalExpr(char *expr, int *err) {

	GVCMDS *cmds;
	int i;
	
	GVLocalIndex = 0;

	if (err == NULL) err = &i;								/* Just so err valid */
	if ( (cmds = GVParse(expr, NULL)) == NULL) {		/* Can we parse?		*/
		*err = 1;
		return(0.0);
	}
	return( gv_eval_cmds(cmds, err).x);
}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
TMPCOMPLEX GVEvalComplexCmdsI(GVCMDS *cmds, int i, int *err) {

	GVLocalIndex = i;
	gv_force_complex(cmds);
	return( gv_eval_cmds(cmds, err));

}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full expression
---------------------------------------------------------------------------- */
TMPCOMPLEX GVEvalComplexCmds(GVCMDS *cmds, int *err) {

	GVLocalIndex = 0;
	gv_force_complex(cmds);
	return( gv_eval_cmds(cmds, err));

}

/* ---------------------------------------------------------------------------
-- Routine to evaluate a full complex expression
---------------------------------------------------------------------------- */
TMPCOMPLEX GVEvalComplexExpr(char *expr, int *err) {

	GVCMDS *cmds;
	int i;
	
	GVLocalIndex = 0;

	if (err == NULL) err = &i;								/* Just so err valid */
	if ( (cmds = GVParse(expr, NULL)) == NULL) {		/* Can we parse?		*/
		TMPCOMPLEX zero={0,0};
		*err = 1;
		return(zero);
	}
	gv_force_complex(cmds);
	return( gv_eval_cmds(cmds, err));
}


/* ---------------------------------------------------------------------------
-- Routine to make a new entry in the table and initialize appropriate values.
--------------------------------------------------------------------------- */
PRIVATE GV_ENTRY *gv_make_entry(char *name, int type, int flags) {

	GV_ENTRY	*entry;

	if (gv_del_entry_by_name(name) == -1) return(NULL);	/* Delete illegal? */
	if ((entry=(GV_ENTRY *) calloc(sizeof(GV_ENTRY)+strlen(name), 1)) == NULL) return(NULL);

/* ... Create a default list */
	entry->ID           = MY_GV_ID;						/* Put an ID on it	*/
	entry->varname      = entry->namechars;			/* Multiple ways		*/
	entry->next         = GVFirstEntry;					/* This is the end	*/
	entry->last         = NULL;							/* Multiply linked	*/
	entry->type         = type;							/* Set as int value	*/
	entry->var.intvalue = 0;								/* And assume value	*/
	entry->magic        = gv_make_magic(name);		/* Make the magic #	*/
	entry->level        = gv_local_level;				/* Local level			*/
	entry->flags        = flags;							/* Copy in the flags	*/
	strcpy(entry->namechars, name);						/* Copy the	name		*/

	if (GVFirstEntry != NULL) GVFirstEntry->last = entry;
	GVFirstEntry = entry;
	
	return(entry);												/* Return pointer		*/
}

/* ---------------------------------------------------------------------------
-- Returns: 0 ==> Simple number or array (no evaluation necessary)
--          1 ==> First element is simple address, though more required
-- If a specified array element is beyond size of array, length returned -1
--------------------------------------------------------------------------- */
PRIVATE GV_ENTRY *gv_find_adr(char *name, int *type, void **adr, int *length) {
	
	char varname[LONG_STR_SIZE], *member, brace;
	int i;
	GV_ENTRY *entry;
	
	*length = 1;									/* Default length */

	strcpy(varname, name);
	brace = 0;										/* No element reference */
	if ( (member = strpbrk(varname,"([{")) != NULL) {
		brace = *member;
		*member = '\0';
	} else {
		brace = 0;									/* Needed for items (surface) that can be a name or an element */
	}
	if ( (entry = gv_find_entry(varname)) == NULL) return(NULL);

	switch (entry->type) {
		case GV_INT:
			*type = GV_INT;
			*adr = (void *) &(entry->var.intvalue);
			break;
		case GV_INT_LINK:
			*type = GV_INT;
			*adr = (void *) (entry->var.intadr);
			break;
		case GV_REAL:
			*type = GV_REAL;
			*adr = (void *) &(entry->var.floatvalue);
			break;
		case GV_REAL_LINK:
			*type = GV_REAL;
			*adr = (void *) (entry->var.floatadr);
			break;
		case GV_DOUBLE:
			*type = GV_DOUBLE;
			*adr = (void *) &(entry->var.doublevalue);
			break;
		case GV_DOUBLE_LINK:
			*type = GV_DOUBLE;
			*adr = (void *) (entry->var.doubleadr);
			break;
		case GV_COMPLEX:
			*type = GV_COMPLEX;
			*adr = (void *) &(entry->var.complexvalue);
			break;
		case GV_COMPLEX_LINK:
			*type = GV_COMPLEX;
			*adr = (void *) (entry->var.complexadr);
			break;
		case GV_STRING:
		case GV_STRING_LINK:
			*type   = GV_STRING;
			*adr    = (void *) entry->var.stradr;
			break;
		case GV_STRING_ARRAY:
			*type   = GV_STRING_ARRAY;
			*length = *(entry->var.s_array->size);
			*adr    = (void *) entry->var.s_array->sval;
			break;
		case GV_STRING_ARRAY_LINK:
			*type   = GV_STRING_ARRAY_LINK;
			*length = *(entry->var.s_array->size);
			*adr    = (void *) entry->var.s_array->sval;
			break;
		case GV_ARRAY:
		case GV_ARRAY_LINK:
			*type   = GV_ARRAY;
			*length = *(entry->var.array->size);
			*adr    = (void *) entry->var.array->x;
			break;
		case GV_INT_ARRAY:
		case GV_INT_ARRAY_LINK:
			*type   = GV_INT_ARRAY;
			*length = *(entry->var.i_array->size);
			*adr    = (void *) entry->var.i_array->ival;
			break;
		case GV_COMPLEX_ARRAY:
		case GV_COMPLEX_ARRAY_LINK:
			*type   = GV_COMPLEX_ARRAY;
			*length = *(entry->var.c_array->size);
			*adr    = (void *) entry->var.c_array->z;
			break;
		case GV_2DCURVE:
		case GV_3DCURVE:
			*type = GV_STRING;
			*adr = (void *) entry->var.curve->ids;
			break;
		case GV_SURFACE:
			if (brace == 0) {
				*type = GV_STRING;
				*adr = (void *) entry->var.surface->ids;
			} else {
				*type = GV_SURFACE;
				*adr = (void *) entry->var.surface;
			}
			break;
		case GV_FUNCTION:
			*type = GV_FUNCTION;
			*adr  = (void *) entry->var.stradr;
			break;
		case GV_FUNCTION_LINK:
			*type = GV_FUNCTION_LINK;
			*adr  = (void *) entry->var.ext_fnc->fnc;
			break;
		case GV_FUNCTION_A_LINK:
			*type = GV_FUNCTION_A_LINK;
			*adr  = (void *) entry->var.ext_fnca->fnc;
			break;
		case GV_STR_FUNCTION_LINK:
			*type = GV_STR_FUNCTION_LINK;
			*adr  = (void *) entry->var.str_ext_fnc->fnc;
			break;

		case GV_POINTER:										/* Now getting dangerous */
			*type = GV_FILEPTR;
			*adr = (void *) &(entry->var.pointer);
			break;

		case GV_FILEPTR:
			*type = GV_FILEPTR;
			*adr = (void *) &(entry->var.fileptr);
			break;

		default:
			return(NULL);									/* Unknown type */
	}

/* Member of construction */
	if (member != NULL) {								/* Not a simple name? */
		if (*type != GV_ARRAY         && *type != GV_ARRAY_LINK         &&
			 *type != GV_INT_ARRAY     && *type != GV_INT_ARRAY_LINK     &&
			 *type != GV_COMPLEX_ARRAY && *type != GV_COMPLEX_ARRAY_LINK &&
			 *type != GV_STRING_ARRAY  && *type != GV_STRING_ARRAY_LINK  &&
			 *type != GV_SURFACE )
			return(NULL);
		if (*type != GV_SURFACE) {
			*member = brace;									/* Replace the character */
			i = (int) GVTrimToNint(GVEvalExpr(member, NULL));
			if (*type == GV_ARRAY || *type == GV_ARRAY_LINK) {
				*type = GV_REAL;
				*adr = (void *) (entry->var.array->x + i);
				*length = (i >= 0 && i < *entry->var.array->size) ? +1 : -1;	/* Out of bounds? */
			} else if (*type == GV_INT_ARRAY || *type == GV_INT_ARRAY_LINK) {
				*type = GV_INT;
				*adr = (void *) (entry->var.i_array->ival + i);
				*length = (i >= 0 && i < *entry->var.i_array->size) ? +1 : -1;	/* Out of bounds? */
			} else if (*type == GV_STRING_ARRAY || *type == GV_STRING_ARRAY_LINK) {
				entry->tmpuse = i;											/* Kludge for possible use */
				*type = GV_STRING;
				*adr = (void *) (entry->var.s_array->sval[i]);
				*length = (i >=0 && i < *entry->var.s_array->size) ? +1 : -1;	/* Out of bounds? */
			} else {
				*type = GV_COMPLEX;
				*adr = (void *) (entry->var.c_array->z + i);
				*length = (i >= 0 && i < *entry->var.c_array->size) ? +1 : -1;	/* Out of bounds? */
			}
		} else {												/* Form is surf[row,col] - need two expressions */
			char *aptr, token[LONG_STR_SIZE];
			REAL xp,yp, *x,*y;
			int ix,iy,  nx,ny;

			LexParseLineEx(token, sizeof(token), member+1, &aptr, MATH, ",");
			if (*token == '\0') return NULL;
			xp = GVTrimToReal(GVEvalExpr(token, NULL));
			LexParseLineEx(token, sizeof(token), aptr, &aptr, MATH, ")]}");
			if (*token == '\0' || *aptr != '\0') return NULL;
			yp = GVTrimToReal(GVEvalExpr(token, NULL));

			/* Get easy variables */
			x = entry->var.surface->x; nx = entry->var.surface->ncol; 
			y = entry->var.surface->y;	ny = entry->var.surface->nrow;

			/* Find the index where x[i] and x[i+1] bound value safely */
			if ( (xp<x[0]) == (xp<x[nx-1]) ) {						/* Outside one or the other */
				if (fabs(xp-x[0]) < fabs(xp-x[nx-1])) {
					ix = 0;
				} else {
					ix = nx-1;
				}
			} else {
				for (ix=0; ix<nx-2; ix++) {
					if ( (xp<x[ix]) != (xp<x[ix+1]) ) break;
				}
				if (fabs(xp-x[ix]) > fabs(xp-x[ix+1])) ix++;
			}

			/* Find the index where x[i] and x[i+1] bound value safely */
			if ( (yp<y[0]) == (yp<y[ny-1]) ) {						/* Outside one or the other */
				if (fabs(yp-y[0]) < fabs(yp-y[ny-1])) {
					iy = 0;
				} else {
					iy = ny-1;
				}
			} else {
				for (iy=0; iy<ny-2; iy++) {
					if ( (yp<y[iy]) != (yp<y[iy+1]) ) break;
				}
				if (fabs(yp-y[iy]) > fabs(yp-y[iy+1])) iy++;
			}
/*			TTYprintf("Returning xp,yp: %f %f   ix,iy: %d %d   x[ix],y[iy]	%g %g\n", xp,yp,ix,iy,x[ix],y[iy]); */
			*type   = GV_REAL;
			*adr    = entry->var.surface->z + iy + ix*ny;
			*length = +1;													/* By definition, will be valid */
		}
	}
	return(entry);
}

/* ---------------------------------------------------------------------------
-- Routine to return with the address of the entry corresponding to the
-- requested name.
--------------------------------------------------------------------------- */
PRIVATE GV_ENTRY *gv_find_entry(char *name) {

	int magic;
	GV_ENTRY	*entry=GVFirstEntry;

	magic = gv_make_magic(name);								/* Determine magic # */
	while (entry != NULL) {
		if (entry->magic == -1) entry->magic = gv_make_magic(entry->varname);
		if (entry->magic == magic) {
			if (stricmp(name,entry->varname)==0) break;	/* Have a match! */
		}
		entry = entry->next;
	}
	return(entry);
}

/* ---------------------------------------------------------------------------
-- Routine to convert a string into a magic number for quick search comparison
-- The list of names is unsorted so need method for fast checking.
--
-- WARNING: There are internal usages of the "magic" expression in GVPARSE
--------------------------------------------------------------------------- */
EXTERN int gv_make_magic(char *name) {
	int magic=0;
	while (*name != '\0') magic = MAGIC(magic, tolower(*(name++)));
	return(magic);
}
