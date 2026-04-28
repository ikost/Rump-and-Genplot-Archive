/* Extended Attribute manipulation code */
/* ===========================================================================
-- Code for dealing with extended attributes
--
-- int EA_Set(char *Filename, char *szAttrib, void *EA, int cbEA);
-- int EA_Query(char *Filename, char *szAttrib, void **EA, int *cbEA);
-- int EA_SetAscii(char *Filename, char *szAttrib, char *szValue);
-- int EA_QueryAscii(char *Filename, char *szAttrib, char *szValue, int maxlen);
-- int EA_SetAsciiList(char *Filename, char *szAttrib, char *szValue[]);
-- int EA_QueryAsciiList(char *Filename, char *szAttrib, char **list[]);
-- int EA_Print(void *EA, int cbEA);
--
-- Warning: There is evidence that the documentation on Multiple-Value /
--          Single-Type is wrong.  Setting multiple ASCII values according
--          to the documentation appears not to be successful.  It seems
--          that each data item in the multiple set still requires the
--          TYPE identifier, despite documentation.  The example code can
--          be made to work (EAS from toolkit) if MVST is assumed to be
--          identical to MVMT.  However, the settings notebook still will
--          not recognize MVST.  Use MVMT only is the recommendation at this
--          time.  (7/29/95).
--
-- History: 7/29/95 - MOT - initial coding
=========================================================================== */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#define INCL_DOSFILEMGR   /* File Manager values */
#define INCL_DOSERRORS    /* DOS error values    */
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include <ea.h>

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */

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
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

#ifdef MAIN
/* ===========================================================================
-- Test routine.  All files specified on command line will have their .TYPE
-- attribute set to "OS/2 Command File" and "Plain Text".  The attributes for
-- .TYPE will be printed also.
=========================================================================== */
int main(int argc, char *argv[]) {

	int rc;
	char *list[] = {"OS/2 Command File", "Plain Text", NULL};
	char **mylist;
	char buffer[1024];
	void *EA;
	int i, cbEA;

	argc--; argv++;						/* Skip over first name */
	while (argc--) {

/*		rc = EA_SetAsciiList(*argv, ".TYPE", list);
		rc = EA_SetAscii(*argv, ".SUBJECT", "My Files");
		rc = EA_Query(*argv, ".TYPE", &EA, &cbEA);
		if (rc == 0) EA_Print(EA, cbEA);
*/
		rc = EA_QueryAscii(*argv, ".LONGNAME", buffer, sizeof(buffer));
		printf(".LONGNAME returns: %d   %s\n", rc, buffer);
		rc = EA_QueryAscii(*argv, ".SUBJECT", buffer, sizeof(buffer));
		printf(".SUBJECT returns:  %d   %s\n", rc, buffer);
		rc = EA_QueryAscii(*argv, ".HISTORY", buffer, sizeof(buffer));
		printf(".HISTORY returns:  %d   %s\n", rc, buffer);
		rc = EA_QueryAsciiList(*argv, ".TYPE", &mylist);
		printf(".TYPE(s) returns count of: %d\n", rc);
		for (i=0; i<rc; i++) printf("%5d: %s\n", i, mylist[i]);
		free(mylist);
		argv++;
	}
	return(EXIT_SUCCESS);
}
#endif

/* ===========================================================================
-- Routine to set a single extended attribute to a single simple ASCII string.
--
-- Usage:  EA_SetAscii(char *Filename, char *szAttrib, char *szValue);
--
-- Inputs: Filename - file to be modified
--         szAttrib - attribute to be set (".TYPE")
--         szValue  - value to set in the attribute
--
-- Output: Modifies extended attributes of the files
--
-- Return: 0 if successful, error code from offending call if not
=========================================================================== */
int EA_SetAscii(char *Filename, char *szAttrib, char *szValue) {

/* Extended attribute structures */
	char *base;
	USHORT *EA;								/* Actual attribute encoded format */
	int ineed, rc;

/* If request is to eliminate, do so now */
	if (szValue == NULL || *szValue == '\0') {
		return EA_Set(Filename, szAttrib, NULL, 0);
	}
		
/* Create the extended attribute itself with data types, etc.	*/
	ineed = 2*sizeof(USHORT) + strlen(szValue);
	base = malloc(ineed);
	EA = (USHORT *) base;
	*EA++ = EAT_ASCII;
	*EA++ = strlen(szValue);
	memcpy(EA, szValue, strlen(szValue));
	EA = (USHORT *) ( ((CHAR *) EA) + strlen(szValue));

	rc = EA_Set(Filename, szAttrib, base, ((char *) EA) - base);
	free(base);
	return(rc);
}


/* ===========================================================================
-- Routine to set an extended attribute to a multiple simple ASCII strings.
--
-- Usage:  int EA_SetAsciiList(char *file, char *szAttrib, char *list[]);
--
-- Inputs: file     - file to be modified
--         szAttrib - attribute to be set (".TYPE")
--         szValue  - array of pointers to character values.  Last pointer
--                    must be NULL.
--
-- Output: Modifies extended attributes of the files
--
-- Return: 0 if successful, error code from offending call if not
=========================================================================== */
int EA_SetAsciiList(char *Filename, char *szAttrib, char *list[]) {

	char *base;
	USHORT *EA;								/* Actual attribute encoded format */
	int i, ineed, icnt, rc;

/* If request is to eliminate, do so now */
	if (list == NULL || *list == NULL) {
		return EA_Set(Filename, szAttrib, NULL, 0);
	}

/* Create the extended attribute itself with data types, etc.	*/
/* And at the end save the length of the EA itself in pFEAItem	*/
	ineed = 3*sizeof(USHORT);						/* MVMT header space */
	for (icnt=0; list[icnt]!=NULL; icnt++) {
		ineed += 2*sizeof(USHORT) + strlen(list[icnt]);
	}
	ineed += 2;											/* Leave space for nulls   */
	base = malloc(ineed);							/* Base of the extended value */
	EA = (USHORT *) base;							/* Extended attribute values	*/
	*EA++ = EAT_MVMT;									/* Multiple value and type	*/
	*EA++ = 0;											/* Code page 0					*/
	*EA++ = icnt;										/* Have multiple values		*/
	for (i=0; i<icnt; i++) {
		*EA++ = EAT_ASCII;
		*EA++ = strlen(list[i]);
		memcpy(EA, list[i], strlen(list[i]));
		EA = (USHORT *) ( ((CHAR *) EA) + strlen(list[i]));
	}

	rc = EA_Set(Filename, szAttrib, base, ((char *) EA) - base);
	free(base);
	return(rc);
}

/* ===========================================================================
-- Routine to set an extended attribute for a file.
--
-- Usage: int EA_Set(char *Filename, char *szAttrib, void *EA, int cbEA);
--
-- Inputs:  Filename - pathname for which EA is to be set
--          szAttrib - ASCII representation of the attribute (".TYPE")
--          EA       - pointer to the EA structure
--          cbEA     - total number of bytes in EA
--
-- Output: Sets the attribute in the extended attributes of the file
--
-- Returns: 0 if successful, an error code otherwise based on failure
--
-- Notes: EA is a complex linear structure.  First word is the type of
--        attribute, which may be a marker to multiple value, multiple types.
--        After type is usually a length, followed by the value, again
--        normally not terminated with the null character.
--
--        See code for SetAsciiEA for example of usage.
=========================================================================== */
int EA_Set(char *Filename, char *szAttrib, void *EA, int cbEA) {

/* Extended attribute structures */
	EAOP2 eaopWrite;
	FEA2LIST *pFEAList;
	int ineed, rc;

/* Check validity of arguments */
	if (EA == NULL) cbEA = 0;								/* If 0, must be an erase */

/* Create a FEA object for the extended attribute */
	ineed = sizeof(FEA2LIST)+strlen(szAttrib)+cbEA;	/* Space needed (minimum) */
	pFEAList = (FEA2LIST *) malloc(ineed);				/* Make lots of space	  */
	pFEAList->cbList = ineed;
	pFEAList->list[0].oNextEntryOffset = 0;			/* Only a single entry	  */
	pFEAList->list[0].fEA = 0;								/* Not critical attribute */
	pFEAList->list[0].cbName  = strlen(szAttrib);	/* Length of attrib name  */
	pFEAList->list[0].cbValue = cbEA;
	strcpy(pFEAList->list[0].szName, szAttrib);		/* Copy the name over	  */
	if (EA != NULL && cbEA != 0) 
		memcpy(pFEAList->list[0].szName+strlen(szAttrib)+1, EA, cbEA);

/* And finally set the file with the attribute */
	eaopWrite.fpGEA2List = NULL;
	eaopWrite.fpFEA2List = (FEA2LIST FAR *) pFEAList;
	rc = DosSetPathInfo(Filename,					/* Path and name of file	*/
							  FIL_QUERYEASIZE,		/* Request to set EA			*/
							  &eaopWrite,				/* Buffer for info			*/
							  sizeof(eaopWrite),		/* Size of buffer				*/
							  0);

/* Free allocated space and return */
	free(pFEAList);
	return(rc);
}

/* ===========================================================================
-- Routine to return a list of all consecutive ASCII types in an extended
-- attribute.  Will not handle different types interspersed.
--
-- Usage: int EA_QueryAsciiList(char *Filename, char *szAttrib, char **list[]);
--
-- Inputs: Filename - name of file to scan
--         szAttrib - attribute to read
--         list     - pointer to array of pointers
--
-- Output: list will be set to a malloc()'d area containing a list of pointers
--         to all ASCII strings in the EA.  This is a single malloc()'d area
--         which it is the responsibility of the caller to release when done.
--         Actual strings are packed in this array after all pointers.
--
-- Returns: rc has number of strings read, 0 if none, and -1 on error.
=========================================================================== */
int EA_QueryAsciiList(char *Filename, char *szAttrib, char **list[]) {

	void *EA;
	char **mylist, *aptr;
	USHORT *aValue;
	int rc, rcmax, ineed, cbEA;
	
/* Default value */
	*list = NULL;

/* And the list */
	if ( (rc = EA_Query(Filename, szAttrib, &EA, &cbEA)) != 0) return(-1);
	if (EA == NULL || cbEA <= 0) return(-1);

/* Now, determine whether a multiple value item, or single value */
	aValue = (USHORT *) EA;
	if (*aValue == EAT_ASCII) {				/* Trivial result */
		ineed = aValue[1] + 1 + 2*sizeof(char *);
		*list = mylist = malloc(ineed);
		aptr = (char *) &mylist[2];
		mylist[0] = aptr;
		memcpy(aptr, &aValue[2], aValue[1]);
		aptr[aValue[1]] = '\0';
		mylist[1] = NULL;
		rc = 1;
	} else if (*aValue == EAT_MVMT) {
		rcmax = aValue[2];
		ineed = cbEA + rcmax;					/* Add some space for EOS chars	*/
		*list = mylist = malloc(ineed);
		aptr = (char *) &mylist[rcmax+1];	/* Last possible pointer needed	*/
		aValue += 3;								/* Skip over MVMT header			*/
		for (rc=0; rc<rcmax; rc++) {
			if (*aValue != EAT_ASCII) break;
			mylist[rc] = aptr;
			memcpy(aptr, &aValue[2], aValue[1]);
			aptr[aValue[1]] = '\0';
			aptr += aValue[1]+1;
			aValue = (USHORT *) ( ((char *) (aValue+2)) + aValue[1] );
		}
		mylist[rc] = NULL;
		if (rc == 0) {free(mylist); *list = NULL;}
	} else {
		rc = -1;
	}

	free(EA);
	return(rc);
}


/* ===========================================================================
-- Routine to return first ASCII entry of an extended attribute.
--
-- Usage: int EA_QueryAscii(char *Filename, char *szAttrib, char *szValue, int maxlen);
--
-- Inputs: Filename - name of file to scan
--         szAttrib - attribute to read
--         szValue  - pointer to array of pointers
--         maxlen   - maximum # of characters to be put in szValue
--
-- Output: szValue will get the first value returned by EA_QueryAsciiList.
-
-- Returns: rc is 0 if successful, -1 on errors.
=========================================================================== */
int EA_QueryAscii(char *Filename, char *szAttrib, char *szValue, int maxlen) {

	char **mylist=NULL;
	int rc;

	*szValue = '\0';

	rc = EA_QueryAsciiList(Filename, szAttrib, &mylist);
	if (rc < 0) return(rc);

	if (mylist != NULL && mylist[0] != NULL) {
		strncpy(szValue, mylist[0], maxlen-1);
		szValue[maxlen-1] = '\0';
	}

	free(mylist);
	return(0);
}

/* ===========================================================================
-- Routine to query an extended attribute for a file.
--
-- Usage: int EA_Query(char *Filename, char *szAttrib, void **EA, int *cbEA);
--
-- Inputs:  Filename - pathname for which EA is to be set
--          szAttrib - ASCII representation of the attribute (".TYPE")
--          pEA      - pointer to char *pointer to receive EA structure
--          pcbEA    - pointer to int which gets total number of bytes
--
-- Output: EA        - Set to NULL on error.  Otherwise malloc() space
--                     containing the EA structure (type, values)
--         cbEA      - Total number of bytes in the EA
--
-- Returns: 0 if successful, an error code otherwise based on failure
--
-- Notes: EA is left in its complex linear structure.  See notes on
--        SetEA and documentation of attributes
=========================================================================== */
int EA_Query(char *Filename, char *szAttrib, void **pEA, int *pcbEA) {

	FILESTATUS4 fStat;
	GEA2LIST *pGEAList;
	FEA2LIST *pFEAList;
	EAOP2	eaopGet;

	int ineed, rc;

/* Default values */
	if (pEA   != NULL) *pEA = NULL;
	if (pcbEA != NULL) *pcbEA = 0;

/* Determine the size of buffer needed to hold EAs */
	rc = DosQueryPathInfo(Filename,				/* Path and name of file	*/
								 FIL_QUERYEASIZE,		/* Request EA size			*/
								 &fStat,					/* Buffer for info			*/
								 sizeof(fStat));		/* Size of buffer				*/
	if (rc != 0) return(rc);
	if (fStat.cbList == 0) return(0);			/* No attribute found		*/

/* Create the GEA table */
	ineed = sizeof(GEA2LIST) + strlen(szAttrib);
	pGEAList = (GEA2LIST *) malloc(ineed);
	pGEAList->cbList = ineed;
	pGEAList->list[0].oNextEntryOffset = 0;
	pGEAList->list[0].cbName = strlen(szAttrib);
	strcpy(pGEAList->list[0].szName, szAttrib);

	ineed = sizeof(FEA2LIST) + 2*fStat.cbList;	/* Ensures adequate space */
	pFEAList = (FEA2LIST *) malloc(ineed);
	pFEAList->cbList = ineed;

	eaopGet.fpGEA2List = (GEA2LIST FAR *) pGEAList;
	eaopGet.fpFEA2List = (FEA2LIST FAR *) pFEAList;
	rc = DosQueryPathInfo(Filename,					/* Path and name of file	*/
								 FIL_QUERYEASFROMLIST,	/* Request EA values			*/
								 &eaopGet,					/* Buffer for info			*/
								 sizeof(eaopGet));		/* Size of buffer				*/

/* If not an error, then copy information to the user space */
	if (rc == 0) {
		ineed = pFEAList->list[0].cbValue;			/* Size of the EA structure */
		if (pcbEA != NULL) *pcbEA = pFEAList->list[0].cbValue;
		if (pEA   != NULL) {
			*pEA = malloc(pFEAList->list[0].cbValue);
			memcpy(*pEA, 
					 pFEAList->list[0].szName+pFEAList->list[0].cbName+1, 
					 pFEAList->list[0].cbValue);
		}
	}

/* Free my locally allocated space and return with no error */
	free(pGEAList); free(pFEAList);
	return(rc);
}

/* ===========================================================================
-- Routine to print out the value of an EA structure, specifying the type
-- and interpreting where possible.  Will call itself recursively on
-- multiple type structures.
--
-- Usage: int EA_Print(void *EA, int cbEA);
--
-- Inputs: EA   - pointer to memory containing the EA structures (from QueryEA)
--         cbEA - number of characters in the EA
--
-- Output: prints the top entry in the EA, and enumerates if a multiple type,
--         and then returns.
--
-- Return: >0 Number of bytes used for the item (should equal cbEA)
--         -1 Some sort of error, usually unrecognized EAT type
=========================================================================== */
static int printval(int type, USHORT *aValue, int cbEA);

int EA_Print(void *EA, int cbEA) {
	
	USHORT *aValue=EA;
	int rc;

/* First entry is the type, and thus first 2 bytes used already */
	rc = (EA==NULL||cbEA<=0) ? -1 : printval(*aValue,aValue+1,cbEA-sizeof(USHORT));
	if (rc > 0) rc += sizeof(USHORT);
	return(rc);
}

static int printval(int type, USHORT *aValue, int cbEA) {

	char   szBuf[1024];
	int i, itype, icnt, codepage, irc, rc;

	rc = 0;
	icnt = *aValue++; rc += sizeof(USHORT);	/* Count normally in pos 2	*/

	switch (type) {						/* First parameter type				*/
		case EAT_BINARY:
			printf("EAT_BINARY:   %d\n", icnt);
			rc += icnt;	break;
		case EAT_ASCII:
			memcpy(szBuf, aValue, icnt); szBuf[icnt] = '\0';
			printf("EAT_ASCII:    %d %s\n", icnt, szBuf);
			rc += icnt;	break;
		case EAT_BITMAP:
			printf("EAT_BITMAP:   %d\n", icnt);
			rc += icnt;	break;
		case EAT_METAFILE:
			printf("EAT_METAFILE: %d\n", icnt);
			rc += icnt;	break;
		case EAT_ICON:
			printf("EAT_ICON:     %d\n", icnt);
			rc += icnt;	break;
		case EAT_EA:
			memcpy(szBuf, aValue, icnt); szBuf[icnt] = '\0';
			printf("EAT_EA:       %d %s\n", icnt, szBuf);
			rc += icnt;	break;
		case EAT_MVMT:
			codepage = icnt;
			icnt = *aValue++;	rc += sizeof(USHORT);
			printf("EAT_MVMT:  codepage=%d  number=%d\n", codepage, icnt);
			for (i=0; i<icnt; i++) {
				itype = *aValue++; rc += sizeof(USHORT);
				irc = printval(itype, aValue, cbEA-rc);
				if (irc > 0) {
					aValue = (USHORT *) ( ((char *) aValue) + irc);
					rc += irc;
				} else {
					rc = -1;
					break;
				}
			}
			break;
		case EAT_MVST:
			codepage = icnt;
			icnt = *aValue++; rc += sizeof(USHORT);
			printf("EAT_MVST:  codepage=%d  number=%d\n", codepage, icnt);
			itype = *aValue++; rc += sizeof(USHORT);
			for (i=0; i<icnt; i++) {
				irc = printval(itype, aValue, cbEA-rc);
				if (irc > 0) {
					aValue = (USHORT *) ( ((char *) aValue) + irc);
					rc += irc;
				} else {
					rc = -1;
					break;
				}
			}
			break;
		case EAT_ASN1:
			printf("EAT_ASN1: don't know how to handle.\n");
			rc = -1;
			break;
		default:
			printf("Unknown EAT type (0x%4.4x).  Aborting.\n", type);
			rc = -1;
	}
	return(rc);
}
