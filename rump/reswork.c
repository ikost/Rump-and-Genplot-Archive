/*  ------------------------------------------------------------------------- */
/*  ---------                                              ------------------ */
/*  --------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------ */
/*  ---------                                              ------------------ */
/*  ---------    The source code to RUMP may be freely     ------------------ */
/*  ---------  modified as long as this copyright notice   ------------------ */
/*  ---------          is included and unchanged.          ------------------ */
/*  ------------------------------------------------------------------------- */

/*  reswork.f77 */

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
#include "xsect.h"

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
#ifdef RESONANCE
	RES_TABLE *reschk[MXNEL+1];	/* Resonance entry indexed by Z2 (target) */
#endif

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */


/* ===========================================================================
--  Nuclear resonance support - also requires resonance compatible creatr.f77
--
--  Information:
--     The information on resonance is maintained in a simple piecewise linear
--     approximations to the resonance curve.  Multiply indirect pointers
--     direct the CREATR program to find each entry.
--
--    There are NTAB entries for resonance data.  Each entry corresponds to a
--    given incident particle and target particles at a given scattering angle.
--    The piecewise approximation is stored as a list of (ENERGY/VALUE) pairs.
--    The first entry for the given resonance is pointed to by RPNTR.
--
-- 	->Z1 / ->M1   ==> projectile
-- 	->Z2 / ->M2   ==> target
-- 	->phi         ==> Scattering angle valid for
--
--	   fit[i].kev   ==> Energy of resonance point
-- 	fit[i].sigma ==> Cross-section at given energy
--                     End of set occurs when kev = 0.0
--
--   The table entries are maintained sorted by element number.  Each element
--   can determine where the table starts for him via RESCHK(Z) where Z is the 
--   element number.
--
--  Routine to read in a resonance data file.
--
--  Usage:  BOOL = resred(lun)
--
--  Inputs: lun  - logical unit currently opened for reading
--
--  Output: XSECT.INS common block
--          resred - success of the read
--
--  Called by: LOAD (ATOMIO.F77)
--
--  Calls to:  none
--
-- Only way to discard data is SIM RESET -ALL
--
-- Data should be downloaded from http://www-nds.iaea.org/ibandl/
=========================================================================== */
#ifdef RESONANCE

struct {
	char *token;
	enum {Q_VERSION, Q_COMMENT, Q_REACTION, Q_DISTRIBUTION, Q_QVALUE, Q_ERROR,
		   Q_THETA, Q_UNITS, Q_SIGFACTORS, Q_ENFACTORS, 
			Q_NVALUES, Q_DATA, Q_IGNORE} action;
} headers[] = {	{"COMMENT",					Q_COMMENT},

						{"VERSION",					Q_VERSION},
						{"SOURCE",					Q_IGNORE},
						{"NAME",						Q_IGNORE},
						{"ADDRESS1",				Q_IGNORE},
						{"ADDRESS2",				Q_IGNORE},
						{"ADDRESS3",				Q_IGNORE},
						{"ADDRESS4",				Q_IGNORE},
						{"ADDRESS5",				Q_IGNORE},
						{"ADDRESS6",				Q_IGNORE},
						{"SERIAL NUMBER",			Q_IGNORE},
						{"SUBFILE",					Q_IGNORE},
						{"X4NUMBER",				Q_IGNORE},

						{"REACTION",				Q_REACTION},
						{"DISTRIBUTION",			Q_DISTRIBUTION},
						{"COMPOSITION",			Q_IGNORE},
						{"MASSES",					Q_IGNORE},
						{"ZEDS",						Q_IGNORE},
						{"QVALUE",					Q_QVALUE},
						{"THETA",					Q_THETA},

						{"SIGMA FACTORS",			Q_SIGFACTORS},
						{"SIGFACTORS",				Q_SIGFACTORS},
						{"ENERGY FACTORS",		Q_ENFACTORS},
						{"ENFACTORS",				Q_ENFACTORS},
						{"UNITS",					Q_UNITS},

						{"NVALUES",					Q_NVALUES},
						{"DATA",						Q_DATA},
						{"ENDDATA:",				Q_IGNORE},

						{NULL,						Q_ERROR}
};

static BOOL InterpretRxn(char *rxn, int *zinc, int *minc, int *ztarget, int *mtarget) {
	
	int i, z[4], m[4];
	char endlist[4] = {'(', ',', ')', '\0'};
	char token[DFLT_STR_SIZE], *aptr;
	
	for (i=0; i<4; i++) {
		aptr = token;
		while (*rxn && *rxn != endlist[i]) *(aptr++) = *(rxn++);
		if (aptr == token) return(FALSE);
		*aptr = '\0';
		if (*rxn && *rxn == endlist[i]) rxn++;

		if (islower(*token) && token[1] == '\0') {		/* Lower case nomenclature */
			switch (*token) {
				case 'p':
					z[i] = 1; m[i] = 1; break;
				case 'd':
					z[i] = 1; m[i] = 2; break;
				case 't':
					z[i] = 1; m[i] = 3; break;
				case 'a':
					z[i] = 2; m[i] = 4; break;
				case 'n':									/* Neutrons */
				case 'g':									/* Gamma rays */
				case 'x':									/* X rays */
				default:
					return(FALSE);
			}
		} else if (stricmp(token, "p0") == 0) {
			z[i] = 1; m[i] = 1;
		} else {
			if (! RbsIdentp(token, &z[i], &m[i], NULL)) return(FALSE);
		}
	}

/* Some sanity checks */
	if (z[1] != z[2] || m[1] != m[2] || z[0] != z[3] || m[0] != m[3]) return(FALSE);

/* And copy over - order does not matter */
	*zinc    = z[2]; *minc    = m[2];
	*ztarget = z[3]; *mtarget = m[3];
	return(TRUE);
}
				
BOOL ResRead(char *filename) {

	REAL m1r,m2r,phi;									/* Resonance info */
	int  m1,m2,z1,z2;
	REAL d1, d2;										/* Dummy variables */

	int i, npt, npt_space, npt_max;
	REAL *kev, *sigma, Fs, F0E, F1E;
	char inbuf[LONG_STR_SIZE], path[PATH_MAX], token[DFLT_STR_SIZE], *aptr;
	FILE *lun;
	RES_TABLE *table, *last;
	enum {PRESCAN, HEADERS, COMMENT, DATA, OLD_DATA, EXIT, RECYCLE} status;
	enum {U_BARNS, U_MILLIBARNS, U_RELATIVE} units;
	enum {EARLY_RUMP, R33, DSIR_33a} version;
	BOOL rcode, comment;


	if (! SysFindFile(path, filename, RbsConfigPath, ".adt", R_OK) ||
		( (lun = fopen(path, "r")) == NULL) ) {
		ERRprintf("ERROR: Resonance file %s (%s) failed to open\n", filename, path);
		return(FALSE);
	}

/* Create the defaults */
	RbsIdentp("4He", &z1, &m1, NULL);			/* Assume simple RBS			*/
	RbsIdentp("16O", &z2, &m2, NULL);			/* And oxygen					*/
	phi = 180-168;										/* Default angle				*/
	units = U_MILLIBARNS;							/* Default for dSigma/dOmega */
	npt = npt_space = 0;								/* No data yet					*/
	npt_max = 65536;									/* Rediculous limit			*/
	kev = sigma = NULL;								/* No data yet					*/
	Fs  = 1.0f;											/* Scaling factor for dSigma/dOmega */
	F0E = 1.0f;											/* Linear scaling on energy */
	F1E = 0.0f;											/* Offset scaling on energy */
	version = EARLY_RUMP;							/* Assume compatibility mode */
	rcode = FALSE;										/* Assume we will fail		*/

/* Read the file, processing as we go */
	status = PRESCAN;									/* Start in PRESCAN - don't know format */
LoadNext:
	while (fgets(inbuf, sizeof(inbuf), lun) != NULL) {
		if ( (aptr=strchr(inbuf, '\n')) != NULL) *aptr = '\0';

		if (*inbuf == '#') {
			aptr = inbuf+1; 
			comment = TRUE;
		} else if (strncmp(inbuf, "/*", 2) == 0) {
			aptr = inbuf+2;
			comment = TRUE;
		} else {
			aptr = inbuf;
			comment = FALSE;
		}
		while (isspace(*aptr)) aptr++;			/* And jump over whitespace */

		switch (status) {
			case PRESCAN:								/* Either switch modes, or use old */
				if (strnicmp(aptr, "VERSION:", 8) == 0) {
					aptr += 8;							/* Skip over the VERSION: text */
					while (isspace(*aptr)) aptr++;
					if (strnicmp(aptr, "DSIR 33a", 8) == 0) {
						version = DSIR_33a;
					} else if (strnicmp(aptr, "R33", 3) == 0) {
						version = R33;
					} else {
						ERRprintf("ERROR: I do not recognize that version of nuclear reaction data files\n"
									 "       %s\n", inbuf);
						goto AllExit;
					}
					status = HEADERS;

				/* Also allow COMMENT: to be the first header item */
				} else if (strnicmp(aptr, "COMMENT:", 8) == 0) {
					status = COMMENT;
					
				/* For compatibility, try via old version */
				} else if (! comment) {
					if (sscanf(inbuf,"%d %f %d %f %f %d", &z1,&m1r,&z2,&m2r,&phi,&npt_max) != 6
						 || z2 <= 0 || z2 >= MXNEL || z1 <= 0 || z1 >= MXNEL) {
						ERRprintf("ERROR: Compatibility mode.  Header here should contain Z1 M1 Z2 M2 Phi NPT.\n"
									 "       Got instead: %s\n", inbuf);
						goto AllExit;
					}
					m1 = (int) (m1r+0.5); m2 = (int) (m2r+0.5);
					units  = U_BARNS;								/* Always in barns */
					status = OLD_DATA;
					version = EARLY_RUMP;
				}
				break;

			case COMMENT:											/* In the comment section */
				if (*aptr == '\0') status = HEADERS;		/* End on blank line */
				break;


			case EXIT:												/* When finished with data */
				break;

			case RECYCLE:											/* RECYCLE becomes headers immediately */
				status = HEADERS;									/* Absence of break is intentional here */
			case HEADERS:
				if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, ":")) break;
				for (i=0; headers[i].token!=NULL; i++) {
					if (stricmp(token, headers[i].token) == 0) break;
				}
				switch (headers[i].action) {
					case Q_VERSION:
						if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, "\t\n") || *token == '\0') {
							ERRprintf("ERROR: Resonance file contains VERSION: tag but no version text\n");
							goto AllExit;
						}
						aptr = token+strlen(token)-1;
						while (isspace(*aptr) && aptr != token) aptr--;
						if (stricmp(token, "R33") == 0) {
							version = R33;
						} else if (stricmp(token, "DSIR 33a") == 0) {
							version = DSIR_33a;
						} else {
							ERRprintf("ERROR: Invalid version (%s) specified.  Must be R33 or DSIR 33a.\n", token);
							goto AllExit;
						}
						break;
						
					case Q_COMMENT:
						status = COMMENT;
						break;

					case Q_UNITS:
						if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, " \t\n") || *token == '\0') {
							ERRprintf("ERROR: Resonance file contains UNITS: tag but no units specification\n");
							goto AllExit;
						} else if (stricmp(token, "b/sr") == 0) {
							units = U_BARNS;
						} else if (stricmp(token, "mb/sr") == 0) {
							units = U_MILLIBARNS;
						} else if (stricmp(token, "rtr") == 0 || stricmp(token, "rr") == 0 || stricmp(token, "relative") == 0) {
							units = U_RELATIVE;
						} else {
							ERRprintf("ERROR: Invalid UNITS: specified (%s).  Must be mb/sr, b/sr, rtr, relative or rr.\n", token);
							goto AllExit;
						}
						break;

					case Q_REACTION:
						if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, " \t\n")) {
							ERRprintf("ERROR: Must specify the reaction in a nuclear cross section files\n");
							goto AllExit;
						} else if (! InterpretRxn(token, &z1, &m1, &z2, &m2)) {
							ERRprintf("ERROR: RUMP cannot understand/use reaction: %s\n", token);
							goto AllExit;
						}
						break;

					case Q_DISTRIBUTION:
						if (! LexParseLineEx(token, sizeof(token), aptr, &aptr, 0, " \t\n")) break;
						if (stricmp(token, "ENERGY") != 0) {
							ERRprintf("ERROR: Only format DISTRIBUTION: ENERGY files are useful\n");
							goto AllExit;
						}
						break;

					case Q_QVALUE:
						if (atof(aptr) != 0.0) {
							ERRprintf("ERROR: At moment, RUMP does not accept non-zero Q reactions\n");
							goto AllExit;
						}
						break;

					case Q_THETA:
						phi = (REAL) (180.0-atof(aptr));
						break;

					case Q_ENFACTORS:
						F0E = (REAL) strtod(aptr, &aptr);	/* Hope linear scaling	*/
						while (*aptr == ',' || isspace(*aptr)) aptr++;
						F1E = (REAL) strtod(aptr, &aptr);	/* And offset scaling	*/
						break;

					case Q_SIGFACTORS:
						Fs  = (REAL) strtod(aptr, &aptr);	/* Sigma conversion		*/
						break;

					case Q_DATA:
						npt_max = 65536;							/* No reasonable limit */
						status = DATA;
						break;

					case Q_NVALUES:
						npt_max = atoi(aptr);
						if (npt_max <= 0) npt_max = 65536;
						status = DATA;
						break;

					case Q_IGNORE:
						break;

					case Q_ERROR:
						ERRprintf("ERROR: Invalid header item (%s)\n", token);
						goto AllExit;
				}
				break;
				
			case OLD_DATA:
			case DATA:
				if (strnicmp(aptr, "EndData:", 8) == 0 ||			/* Correct format */
					 strnicmp(aptr, "End_Data:", 9) == 0 ||		/* Prior allowed ending terms */
					 strnicmp(aptr, "Data_End:", 9) == 0) {		/* And another historical alternative */
					status = RECYCLE;
					goto SaveData;
				}
				if (comment || *aptr == '\0') continue;
				if (npt >= npt_space) {
					npt_space += 100;
					kev   = realloc(kev,   npt_space*sizeof(*kev));
					sigma = realloc(sigma, npt_space*sizeof(*sigma));
				}
				switch (version) {
					case EARLY_RUMP:
					case DSIR_33a:
						if (sscanf(inbuf, "%f %f", &kev[npt], &sigma[npt]) != 2) {
							ERRprintf("ERROR: Invalid data line in resonance file.\n%s", inbuf);
							goto AllExit;
						}
						break;
					case R33:
						if (sscanf(inbuf, "%f %f %f %f", &kev[npt], &d1, &sigma[npt], &d2) != 4) {
							ERRprintf("ERROR: Invalid line of data in resonance file.\n  Offending line: %s\n", inbuf);
							goto AllExit;
						}
						break;
				}
				kev[npt]   = F0E * kev[npt] + F1E;						/* E and sigma	*/
				sigma[npt] = Fs * sigma[npt];								/* Scale data	*/
				if (units == U_MILLIBARNS) sigma[npt] *= 0.001f;
				if (npt > 0 && kev[npt] <= kev[npt-1]) {
					ERRprintf("ERROR: Point (%f,%f) deleted to maintain strict data sort.\n", kev[npt], sigma[npt]);
					npt_max--;													/* But was read */
				} else {
					npt++;
				}
				if (npt >= npt_max) {
					status = EXIT;
					goto SaveData;
				}
				break;
		}
	}

	if (status != DATA && status != RECYCLE) {
		if (status == COMMENT) {
			ERRprintf("ERROR: Resonance file ended while still in COMMENT mode.\n"
						 "       According to DSIR 33 and R33, blank line must terminate a comment\n");
		} else {
			ERRprintf("ERROR: Resonance file ended unexpectedly - check format.\n");
		}
		goto AllExit;
	} else if (status == RECYCLE && npt == 0) {		/* No data definitely is done also */
		goto AllExit;
	}

/* If the same table already exists, delete it */
SaveData:
	if (npt > 0) {										/* Do we have data */
		for (last=NULL,table=reschk[z2]; table!=NULL; last=table,table=table->next) {
			if (table->z1==z1 && table->m1==m1 && table->z2==z2 && table->m2==m2 && table->phi==phi) {
				TTYprintf("INFO: Overwriting old resonance table from %s\n", table->pathname);
				if (last != NULL) {
					last->next = table->next;
				} else {
					reschk[z2] = table->next;
				}
				free(table);
				break;
			}
		}

/* Allocate space, because of the [1] in RES_TABLE, get 1 extra I need */
		table = malloc(sizeof(RES_TABLE) + npt*sizeof(RES_ENTRY));
		table->next = NULL;
		SysQualifyPath(table->pathname, filename, sizeof(table->pathname));
		table->checked = FALSE;		/* Need to check the validity still */
		table->overrun = FALSE;		/* Has table been overrun				*/
		table->mode = (units == U_RELATIVE) ? M_RELATIVE : M_BARNS;
		table->z1   = z1;		table->m1   = m1;
		table->z2   = z2;		table->m2   = m2;
		table->phi  = phi;
		table->npt  = npt;
		for (i=0; i<npt; i++) {
			table->fit[i].kev   = kev[i];
			table->fit[i].sigma = sigma[i];
			if (i+1 < npt) {
				table->fit[i].slope = (sigma[i+1]-sigma[i])/(kev[i+1]-kev[i]);
			} else {
				table->fit[i].slope = 0;
			}
		}
		table->fit[npt].kev	 = 0;		/* Flag end-of-data */
		table->fit[npt].slope = 0;

/* Insert table into the structure */
		if (reschk[z2] != NULL) table->next = reschk[z2];
		reschk[z2] = table;

/* Print out some info that we are loading */
		TTYprintf("Resonance data:  z1=%d  m1=%d   z2=%d  m2=%d  phi=%6.2f  npt=%d\n",
					 z1,m1, z2,m2, phi, npt);
	}

/* And now, either go back and load more, or exit */
	if (status == RECYCLE) {								/* Do some more! */
		npt = 0;
		goto LoadNext;
	}
	rcode = TRUE;										/* And everything is okay */
	goto AllExit;

AllExit:
	if (lun != NULL) fclose(lun);
	free(kev);										/* Free these if used */
	free(sigma);									/* Free these if used */
	return(rcode);
}

#else /* RESONANCE */

BOOL ResRead(char *filename) {
	return(FALSE);
}

#endif /* RESONANCE */

/* ===========================================================================
--  Routine to print the status of the RESONANCE data
--
--  Usage:  CALL RESTAT
--
--  Inputs: none
--
--  Output: terminal I/O only
--
--  Format:
--
--             ---  Resonance Data Tables  ---
--   Z1   M1        Z2   M2        phi      Emin    Emax  #points
-- ___2___4.8________4___4.8______10.0______1.00____1.00_____73
--   928 points free
=========================================================================== */
static char *EncodeRxn(unsigned int z1, unsigned int m1, unsigned int z2, unsigned int m2) {
	static char rxn[30];											/* 197Au(195Pt,195Pt)197Au */
	char incident[7], target[7];
	char plist[] = "npdta";

	incident[0] = incident[1] = '\0';						/* Set up for a single character validity */
	if (z1 == 0 && m1 == 1) *incident = plist[0];
	if (z1 == 1 && m1 <= 3) *incident = plist[m1];
	if (z1 == 2 && m1 == 4) *incident = plist[4];
	if (*incident == 0) sprintf(incident, "%d%s", m1, atom[z1-1].name);
	sprintf(target, "%d%s", m2, atom[z2-1].name);
	sprintf(rxn, "%s(%s,%s)%s", target, incident, incident, target);
	return rxn;
}

void ResStatus(int level) {

#ifdef RESONANCE

	int i,j, header = FALSE;
	RES_TABLE *table;

	for (i=1; i<=MXNEL; i++) {
		if ( (table=reschk[i]) == NULL) continue;
		if (! header) {
			TTYputs("            ---  Resonance Data Tables  ---\n"
					  " Reaction        Z1  M1   Z2  M2      phi      Emin    Emax   Mode   #points\n");
			header = TRUE;
		}
		while (table != NULL) {
			TTYprintf(" %-14s %3d %3d  %3d %3d%10.1f %8.2f%8.2f  %s  %4d\n",
				EncodeRxn(table->z1, table->m1, table->z2, table->m2),
				table->z1, table->m1, table->z2, table->m2, table->phi,
				0.001*table->fit[0].kev, 0.001*table->fit[table->npt-1].kev,
				(table->mode==M_BARNS) ? "Absolute" : "Relative", table->npt);
			if (level >= 1) {
				for (j=0; j<table->npt; j++) {
					if (table->mode == M_BARNS) {
						TTYprintf(" %7.1f %6.4f", table->fit[j].kev, table->fit[j].sigma);
					} else {
						TTYprintf(" %7.1f %6.2f", table->fit[j].kev, table->fit[j].sigma);
					}
					if (j % 5 == 4) TTYputs("\n");
				}
				if (j %5 != 0) TTYputs("\n");
			}
			table = table->next;
		}
	}
	return;

#else		/* RESONANCE */
	return;
#endif	/* RESONANCE */
	
}


/* ===========================================================================
--  Resonance reset code
--
--  Usage:  call resres
--
--  Inputs: none
--
--  Output: none
=========================================================================== */
void ResReset(int level) {

#ifdef RESONANCE

	static int first=TRUE;
	int i;
	RES_TABLE *table, *next;

	if (level != U_INIT) return;			/* Only deal with full INIT */

	if (! first) {
		for (i=0; i<=MXNEL; i++) {
			table = reschk[i];
			while (table != NULL) {
				next = table->next;
				free(table);
				table = next;
			}
		}
	}

	for (i=0; i<=MXNEL; i++) reschk[i] = NULL;
	first = FALSE;
	return;

#else		/* RESONANCE */
	return;
#endif	/* RESONANCE */
	
}
