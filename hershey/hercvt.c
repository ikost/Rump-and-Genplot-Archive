/* <routine name> */

/* ===========================================================================
-- Modification history
--
-- MOT 12/17/94 - Added full page support.  Just assume that each character
--                set index conforms to code page 850.  Could later change
--                that to other code pages, but for now that will simplify
--                international applications.
=========================================================================== */

/* ---------------------------------------------------------------------------
--  Conversion from single character in UNIX net version to NIST version
-        -40 *  -32 2  -24 :  -16 B   -8 J   0 R    8 Z  16 b  24 j  32 r  40 z
-        -39 +  -31 3  -23 ;  -15 C   -7 K   1 S    9 [  17 c  25 k  33 s  41 {
-        -38 ,  -30 4  -22 <  -14 D   -6 L   2 T   10 \  18 d  26 l  34 t  42 |
-        -37 -  -29 5  -21 =  -13 E   -5 M   3 U   11 ]  19 e  27 m  35 u  43 }
-        -36 .  -28 6  -20 >  -12 F   -4 N   4 V   12 ^  20 f  28 n  36 v  44 ~
-        -35 /  -27 7  -19 ?  -11 G   -3 O   5 W   13 _  21 g  29 o  37 w  
-        -34 0  -26 8  -18 @  -10 H   -2 P   6 X   14 `  22 h  30 p  38 x
- -41 )  -33 1  -25 9  -17 A  -09 I   -1 Q   7 Y   15 a  23 i  31 q  39 y
-                      
--------------------------------------------------------------------------- */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard		*/
#include "preload.h"

/* #define NIST_FORMAT  */					/* Undefine to use HERSHEY1/2.DAT	*/
#undef DEBUG									/* Leave defined to get hercvt.log file	*/

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)
#define	TRUE	1
#define	FALSE	0
#ifndef min
	#define	min(a,b)		(((a) < (b)) ? (a) : (b))
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
void ReadSetIndexes(char *InputFile);
void MakeNeedsTable(void);
void ReadStrokeData(void);
void SetIndexes(void);
void WriteOutputFile(char *OutputFile);
int  ReadOneSymbol(FILE *unt, int *chr, short *xsizeval, short *xstartval, 
               int *nptval, short x[], short y[], short pen[]);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
#define	DEFAULTOUTPUTFILE	"hdata.chr"
#define	MAXJOT		32750					/* max total number of jots (strokes)	*/
#define	MAXSET		30						/* max number of character sets			*/
#define	NCHARS		256					/* Number of characters/set				*/
#define	MXSTRK		500					/* max strokes per character				*/
#define	MXCODE		20000					/* max valid Hershey character code		*/
#define	YOFFS			21						/* Offset in Y on characters				*/

/* ... Names of the character sets and stroke info */
char  *SetName[MAXSET];					/* Name of the set (ASCII)				*/
short *SetIndex[MAXSET];				/* Index for each into JOT array		*/
short *Jots=NULL;							/* Actual jot information				*/
short	*JotIndex=NULL;					/* Pointer into jots for each char	*/
unsigned short NumSets;					/* Actual # of sets read				*/
unsigned short NumUnique;				/* Number of unique symbols used		*/
unsigned short NumJots;					/* Number of jots in jot buffer		*/

/* ---------------------------------------------------------------------------
-- Routine to read Hershey character set data and write the .chr files
--
-- Original coding: FORTRAN 31-may-89
--
--------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {

	char *InputFile  = NULL;					/* Where to get list of files from	*/
	char *OutputFile = DEFAULTOUTPUTFILE;	/* Where to write output data			*/

	while (--argc > 0) {
		argv++;
		if (strcmp(*argv, "-o") == 0) {
			++argv; --argc;
			OutputFile = *argv;
		} else if (strcmp(*argv, "-i") == 0) {
			++argv; --argc;
			InputFile = *argv;
		} else {
			fprintf(stderr, "%s: I will ignore this command line token\n", *argv);
		}
	}

	ReadSetIndexes(InputFile);					/* Which sets wanted?	*/
	MakeNeedsTable();								/* Sort the index set	*/
	ReadStrokeData();								/* Read the stroke data */
	SetIndexes();									/* Set pointers			*/
	WriteOutputFile(OutputFile);				/* Write the data			*/
	return 0;
}

/* ----------------------------------------------------------------------------
--  Routine to read the data sets
---------------------------------------------------------------------------- */
void ReadSetIndexes(char *InputFile) {

	int i, isym, iend;
	FILE *unt, *input;
	char filename[64], name[129], linebuf[132], *line, *aptr;

	if (InputFile == NULL) {
		printf("Enter names of character set files, one per line.  End with blank or EOF.\n\n");
		input = stdin;
	} else if ( (input = fopen(InputFile, "r")) == NULL) {
		perror(InputFile);
		fprintf(stderr,"%s: Failed to open\n", InputFile);
		abort();
	}

	for (NumSets=0; NumSets<MAXSET; NumSets++) {

		if (fgets(filename, sizeof(filename), input) == NULL) break;
		if ( (i = (int) strlen(filename)) == 0) break;
		if (filename[i-1] == '\n') filename[i-1] = '\0';
		aptr = filename; while (isspace(*aptr)) aptr++;	/* Skip whitespace */
		if (*aptr == '\0') break;								/* Exit if blank */

		if ( (unt = fopen(filename, "r"))  == NULL) {
			perror(filename); 
			fprintf(stderr,"%s: Unable to open (FATAL)\n", filename);
			abort();
		}

		do {
			if (fgets(name, sizeof(name), unt) == NULL) {
				fprintf(stderr, "%s: Failed to read character set descriptor\n", name);
				abort();
			}
		} while (*name == '#');										/* Skip comments */

		if ( (aptr = strchr(name, '\n')) != NULL) *aptr = '\0';
		printf(" Defining %i: %s\n", NumSets, name);
		SetName[NumSets] = malloc(strlen(name)+1);			/* Get space */
		strcpy(SetName[NumSets], name);							/* And copy name */
		SetIndex[NumSets] = malloc(sizeof(short)*NCHARS);	/* Get room for data	*/

		*linebuf = '\0';												/* Start w/ blank line */
		for (line=linebuf,i=0; i<NCHARS; ) {
			while (isspace(*line)) line++;
			if (*line == '#') *line = '\0';						/* Comment clears */
			if (*line == '\0' || *line == '\n') {
				if (fgets(linebuf, sizeof(linebuf), unt) == NULL) {
					printf(" Characters from %d to %d are set to empty\n", i, NCHARS);
					while (i<NCHARS) SetIndex[NumSets][i++] = 0;
				}
				line = linebuf;
				continue;
			}
			isym = (int) strtol(line, &line, 10);
			if (isym < 0 || isym > MXCODE) {
				fprintf(stderr, "%s: Invalid character code specified %i\n", filename, isym);
				abort();
			}
			if (*line != '-') {								/* Just single char */
				SetIndex[NumSets][i] = isym;
				i++;
				continue;
			} else {
				line++;											/* Bypass the - sign */
				iend = (int) strtol(line, &line, 10);
				if (iend <= 0 || iend > MXCODE) {
					fprintf(stderr, "%s: Invalid character code specified %i\n", filename, iend);
					abort();
				}
				while (isym <= iend && i < NCHARS) {
					SetIndex[NumSets][i] = isym;
					i++; isym++;
				}
			}
		}
		fclose(unt);
	}
	if (input != stdin) fclose(input);
	return;
}

/* ----------------------------------------------------------------------------
-- Make a master list of all the needed elements.  Just place into array JotIndex
-- a value of -1 for each needed character, and 0 if unneeded.
--
-- Inputs: /COMMON/INDEX - list of required characters
--
-- Output: /COMMON/SORT  - strictly sorted list of required characters
---------------------------------------------------------------------------- */
void MakeNeedsTable(void) {

	unsigned set,ichr,chr;

	JotIndex = (short *) calloc(MXCODE+1, sizeof(short));

	for (set=0; set<NumSets; set++) {
		for (ichr=0; ichr<NCHARS; ichr++) {
			chr = SetIndex[set][ichr];
			if (chr > MXCODE) 
				printf("Invalid character in set %d, position %d.  Request %d ignored.\n", set, ichr, chr);
			else if (chr != 0) 
				JotIndex[chr] = -1;
		}
	}
	return;
}

/* ----------------------------------------------------------------------------
-- Read the data from the data set based on the SORTED INDEX set
--
-- Inputs: /COMMON/SORT - sorted list of characters needed
--
-- Output: /COMMON/Jots   - vector strokes for characters
--         /COMMON/JotIndex - pointer into Jots for each character
---------------------------------------------------------------------------- */
void ReadStrokeData(void) {

#ifdef NIST_FORMAT
	char *filename[] = {"hershey1.dat", "hershey2.dat", "local.dat", NULL};
#else
	char *filename[] = {"hersh.oc", "local.oc", NULL};
#endif
	char **files = filename;

	short xsize, xstart, *x, *y, *pen;
	int i, chr, npt;
	FILE *unt, *ount=NULL;

	Jots = (short *) malloc(MAXJOT*sizeof(short));
	x    = (short *) malloc(MXSTRK * sizeof(short));
	y    = (short *) malloc(MXSTRK * sizeof(short));
	pen  = (short *) malloc(MXSTRK * sizeof(short));

	NumUnique = NumJots = 0;

#ifdef DEBUG
	if ( (ount = fopen("hercvt.log", "w")) == NULL) perror("hercvt.log");
#endif

	do {
		if ( (unt = fopen(*files, "r")) == NULL) {
			perror(*files); 
			fprintf(stderr, "%s: Cannot be opened for scanning\n", *files);
			continue;
		}
		printf(" Reading symbols: %s\n", *files);
		while (ReadOneSymbol(unt, &chr, &xsize, &xstart, &npt, x, y, pen)) {
			if (ount != NULL) 
				fprintf(ount,"Character %d with %d strokes, %d %d\n", chr, npt, xstart, xsize);
			if (chr < 0 || chr > MXCODE) continue;			/* Invalid chars */
			if (JotIndex[chr] > 0) printf(" *** Redefinition of symbol %d ignored ***\n", chr);
			if (JotIndex[chr] >= 0) continue;					/* Not needed */
			if (NumJots+2*npt+2 > MAXJOT) {
				fprintf(stderr, "Toilet has overflowed (too many complex characters!)\n");
				abort();
			}
			NumUnique++;
			JotIndex[chr] = NumJots + 1;							/* FORTRAN Index */
			Jots[NumJots++] = xsize | (xstart << 6);		/* Size and start */
			for (i=0; i<npt; i++) {								/* Encode the data */
				if (x[i] < 0 || y[i] < 0) printf(" *** Bogus coordinate %i: %i %i ***\n", chr, x[i], y[i]);
				if (i != 0 && pen[i] == 3) Jots[NumJots++] = 63;	/* Raise pen */
				Jots[NumJots++] = x[i] | (y[i] << 6);		/* Encode coordinate */
			}
			Jots[NumJots++] = 4095;								/* EOF stroke */
		}
		fclose(unt);
	} while (*(++files) != NULL);

	if (ount != NULL) fclose(ount);
	free(x); free(y); free(pen);
	return;
}


/* ============================================================================
-- Routine to read in a given RECORD from the open file
--
-- Usage:  logical = ReadOneSymbol(chr, xsize, xstart, npt, x, y, pen)
--
-- Inputs: chr - character we are looking for
--
-- Output: xsize   - size of the character
--         xstart  - starting point of character
--         npt     - number of strokes
--         x,y,pen - stroke information
--         ReadOneSymbol  - Success of finding the character
============================================================================ */
int ReadOneSymbol(FILE *unt, int *chr, short *xsizeval, short *xstartval, 
               int *nptval, short x[], short y[], short pen[]) {

	int xoff, xsize, xstart, npt;
	int i, iptr, xx,yy, ipen;
	char achr, *aptr, tok132[2*MXSTRK+15];				/* Space for single line! */

#ifdef NIST_FORMAT

/* ... Read through file till desired character found */
	while (TRUE) {
		if (fgets(tok132, sizeof(tok132), unt) == NULL) return(FALSE);
		if ( (*chr = atoi(tok132)) != 0) break;				/* Successful */
		if (strcmp(tok132,"       :\n") == 0) continue;		/* Blank line */
		printf(" *** Surprise in file -- expecting character start but saw: %s ***", tok132);
	}
	iptr = 8;

/* ... First two have x negative extent, x positive extent */
	xoff  = -atoi(tok132+iptr);					/* Offset needed for left start */
	xsize = atoi(tok132+iptr+4) + xoff;			/* And full size in X			  */

	xstart = 0;											/* Use to track actual extent */
	ipen = 3;											/* Pen initially up				*/
	npt = 0;												/* No points in array			*/

	while (TRUE) {
		if ( (iptr+=8) >= 128) {						/* Do I need another line? */
			fgets(tok132, sizeof(tok132), unt);
			iptr = 8;
		}
		xx = atoi(tok132+iptr); yy = atoi(tok132+iptr+4);
		if (xx == -64 && yy == -64) {					/* That's all folks		*/
			break;
		} else if (xx == -64) {							/* Pen lift command		*/
			ipen = 3;
		} else if (npt >= MXSTRK) {					/* Room still?				*/
			fprintf(stderr, "Too many strokes involved in drawing symbol %d\n", *chr);
			abort();
		} else {
			x[npt] = xx+xoff;								/* X position				*/
			xstart = min(xstart,x[npt]);				/* Keep track of left	*/
			y[npt] = -yy+YOFFS;							/* Hershey upside-down	*/
			pen[npt] = ipen;								/* And the pen wanted	*/
			ipen = 2;										/* Next is pen down		*/
			npt++;
		}
	}

#else

/* ... This is the UNIX net format distributed same time */
/* ... Find next character in the file (if it exists)    */
	while (TRUE) {
		if (fgets(tok132, sizeof(tok132), unt) == NULL) return(FALSE);
		if (*tok132 == '\n' || *tok132 == '\0') continue;		/* Skip line */
		achr = tok132[5]; tok132[5]='\0'; *chr = atoi(tok132);   tok132[5] = achr;
		achr = tok132[8]; tok132[8]='\0'; iptr = atoi(tok132+5); tok132[8] = achr;
		if (*chr != 0 && iptr != 0) break;
		printf(" *** Surprise in file -- expecting character start but saw: %s\n ***", tok132);
	}
	aptr = tok132+8;					/* Where characters start */

/* ... First two have x negative extent, x positive extent */
	xoff = -(*(aptr++)-'R');						/* Offset needed for left start */
	xsize = *(aptr++)-'R' + xoff;					/* And full size in X			  */

	xstart = 0;											/* Use to track actual extent */
	ipen = 3;											/* Pen initially up				*/
	npt = 0;												/* No points in array			*/

	while (--iptr != 0) {							/* I Know how many to do		*/
		if (*aptr == '\0' || *aptr == '\n') {	/* Are we at end of the line	*/
			fgets(tok132, sizeof(tok132), unt);	/* Read another */
			aptr = tok132;
		}
		xx = *(aptr++)-'R'; yy = *(aptr++)-'R';	/* Values					*/
		if (xx == (' '-'R')) {							/* Pen lift command		*/
			ipen = 3;
		} else if (npt >= MXSTRK) {					/* Room still?				*/
			fprintf(stderr, "Too many strokes involved in drawing symbol %d\n", *chr);
			abort();
		} else {
			x[npt] = xx+xoff;								/* X position				*/
			xstart = min(xstart,x[npt]);				/* Keep track of left	*/
			y[npt] = -yy+YOFFS;							/* Hershey upside-down	*/
			pen[npt] = ipen;								/* And the pen wanted	*/
			ipen = 2;										/* Next is pen down		*/
			npt++;
		}
	}

#endif

	if (xstart < 0) {									/* Did they go to left of 0? */
		xstart = -xstart;								/* Correct for when drawing */
		for (i=0;i<npt;i++)							/* Correct the data */
			x[i] += xstart;							/* Correct the data */
	}
	*nptval    = npt;
	*xstartval = xstart;
	*xsizeval  = xsize;
	return(TRUE);
}


/* ---------------------------------------------------------------------------
-- Routine to set the INDEX pointers for each character into JOT matrix
--
-- Inputs: /COMMON/JotIndex - list of characters and where data starts
--         /COMMON/INDEX  - index of which characters are wanted
--
-- Output: /COMMON/INDEX  - revised index into the JOT matrix for characters
---------------------------------------------------------------------------- */
void SetIndexes(void) {

	unsigned int set, ichr, chr;

	for (set=0; set<NumSets; set++) {
		for (ichr=0; ichr<NCHARS; ichr++) {
			chr = SetIndex[set][ichr];
			if (chr != 0) {
				if (JotIndex[chr] == -1) {
					printf(" *** Character not found: %d ***\n", chr);
					JotIndex[chr] = 0;
				}
				SetIndex[set][ichr] = JotIndex[chr];
			}
		}
	}
	return;
}

/* ----------------------------------------------------------------------------
-- Subroutine to actually create the data file from HERSHEY data files
---------------------------------------------------------------------------- */
void WriteOutputFile(char *OutputFile) {

	unsigned int i;
	short sets[MAXSET];
	FILE *unt;

/* -------------------------------------------
-- ... Create the unformatted HDATA.CHR file
------------------------------------------- */
	if ( (unt=fopen(OutputFile, "wb")) == NULL) {
		perror(OutputFile); 
		fprintf(stderr, "%s: unable to open output file for writing\n", OutputFile);
		abort();
	}

/* ... All of these had better be shorts or major fuckup */
	if (fwrite(&NumSets, sizeof(short), 1, unt) != 1)    goto error;
	for (i=0; i<NumSets; i++) sets[i] = '0'+i;		/* Now, just linear */
	if (fwrite(sets, sizeof(short), NumSets, unt) != NumSets) goto error;
	for (i=0; i<NumSets; i++) {
		if (fwrite(SetIndex[i], sizeof(short), NCHARS, unt) != NCHARS) goto error;
	}
	if (fwrite(&NumJots, sizeof(short), 1,       unt) != 1)       goto error;
	if (fwrite(Jots,     sizeof(short), NumJots, unt) != NumJots) goto error;
	fclose(unt);

	printf("\nOutput file %s successfully written\n"
	       "  Number of character sets - %d\n"
			 "  Number of unique symbols - %d\n"
		    "  Total number of strokes  - %d\n",
			 OutputFile, NumSets, NumUnique, NumJots);
	return;

error:
	fprintf(stderr, "Error in fwrite() to hdata.chr data file\n");
	abort();
}
