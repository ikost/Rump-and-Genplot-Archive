/* STOPP - calculates stopping powers given Ziegler coefficients */

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
#include "stopping.h"
#include "../genplot/genplot.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

#define ORDERME(a,b) if	(a>b)	{REAL tmp; tmp=a; a=b; b=tmp;}
#define DBUG(x)
#define NPLOT 201    /* Length of plot arrays */

typedef enum _OPS1 {
/* ... for base level commands ... */
		C_EFIT,		C_RRUMP,		C_WRUMP,		C_RZIEGLER,	C_WZIEGLER,	C_FIT,	
		C_TABLE,		C_LIST,		C_ELEMENT,	C_HELP,		C_QUIT,
} OPS1;

typedef enum _OPS2 {
/* ... for element commands ... */
		CE_HELP,		CE_EFIT,		CE_EPLOT,	CE_SPLOT,	CE_ASYMBOL,	CE_Z,
		CE_ISOTOP,	CE_DENSITY,	CE_FVELOCIT,CE_ZPARAM,	CE_RPOLY,	CE_FIT,
		CE_COMPUTE,	CE_PLOT,		CE_RETURN
} OPS2;

typedef struct _CMTYPE1 {
	char *name;
	int  minlen;
	OPS1 rcode;
} CMTYPE1;

typedef struct _CMTYPE2 {
	char *name;
	int  minlen;
	OPS2 rcode;
} CMTYPE2;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void elem( int elno);
static void fitone( int elno);
static void newFitRange( REAL fmin, REAL fmax );

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of the global vars     */
/* ------------------------------- */

/* ------------------------------- */
/* Private global vars             */
/* ------------------------------- */
static  int   isfit;                       /* Rump fit to Ziegler?     */
static  int   first_time=TRUE;
static  REAL efmin=0.35f, efmax=3.5f;      /* Energy range for fit     */
static  REAL epmin=0.35f, epmax=3.5f,      /* Energy range for plot    */
              spmin, spmax,                /* Stopping power for plot  */
              ener[NPLOT],                 /* Energy array             */
              spow[NPLOT];                 /* Stopping power array     */


static int EvalZieg( int itype, TMPREAL *result, TMPREAL *args) {
	REAL se, sn;
	if (itype != 0) return(-1);	/* Don't handle complex */
	/* zstop(z1, m1, z2, ee, &se, &sn, 1);  */
	zstop((int) *args, (REAL) *(args+1), (int) *(args+2), (REAL) *(args+3), &se, &sn, 1);
	*result = se+sn;
	return(0);
}

#ifdef STANDALONE
void main( int argc, char *argv[] ) {
#else
void RbsStopp(void) {
#endif

/*  -- Local variables -- */
	char token[DFLT_STR_SIZE];
	int i, j, vcount, tabn;
	STOPPING_TABLE *table, *last_table;
	CMTYPE1 *cmd;

	CMTYPE1 cmlist[] = {
		{"?",				1,		C_HELP},
		{"efit",			2,		C_EFIT},
		{"rrump",		2,		C_RRUMP},
		{"wrump",		2,		C_WRUMP},
		{"rziegler",	2,		C_RZIEGLER},
		{"wziegler",	2,		C_WZIEGLER},
		{"fit",			3,		C_FIT},
		{"table",		2,		C_TABLE},
		{"element",		2,		C_ELEMENT},
		{"list",			2,		C_LIST},
		{"status",		4,		C_HELP},
#ifdef STANDALONE
		{"quit",			1,		C_QUIT},
#else
		{"return",		2,		C_QUIT},
#endif
		{NULL,			0,		C_QUIT}
	};

/*  -- Code begin -- */

#ifdef STANDALONE
	CONInitialize();
	LexInitialize();
	PlotInitialize();
	TTYputs("[STOPP version 1.0]\nUse ? for help.\n");
	LexSystem (U_INIT, NULL);			/* Initialize everyone */
	PlotSystem(U_INIT, NULL, NULL);
	/* LexInsText(inpstr); */
#endif

	if (first_time) {
		GVLinkFnc("ZIEG", 0, 4, &EvalZieg);
	/* GVLinkFnc("RSTOP", 0, 1, &EvalRump); */
		first_time=FALSE;
	}

	while (TRUE) {
		while (! LexGetTokenP(token, sizeof(token), "Stopp Command: ")) continue;
		if ( (cmd = LexCmdl(token, cmlist, sizeof(CMTYPE1))) == NULL) {
			if (! LexSystem(0, token)) {
				ERRprintf("ERROR: %s - unknown command.  Use ? for help.\n", token);
				LexFlush();
			}
			continue;
		}
		switch (cmd->rcode) {

/*  (EF) Energy range for fit:    xxxx.x MeV to xxxx.x MeV */
			case C_EFIT:
				efmin = LexGetReal(efmin,"MeV range to be fit? (Unchanged) ");
				efmax = LexGetReal(efmax,"Upper MeV limit for fit? (Unchanged) ");
				ORDERME(efmin, efmax);
				newFitRange( efmin, efmax);
				isfit = FALSE;
				break;

/*  (RR) Read RUMP style atomic data */
			case C_RRUMP:
				if (LexGetTokenP(token, sizeof(token), "[ASCII|Binary] (abort) ")) {
					i = LexSelect(token,"ASCII BINARY");
					if (i == 1) {
						if ( LexGetTokenP(token, sizeof(token), "Filename for ASCII read of RUMP data: ")) {
							RumpDataValid = RbsLoad1(token);
							isfit = FALSE;
						}
					} else if (i == 2)   {
#if 0
						if ( LexGetTokenP(token, sizeof(token),
							"Filename for Binary read of RUMP data: ") )   {
							RumpDataValid   = RbsLoad(token);
							isfit = FALSE;
							}
#else
						ERRputs("ERROR: BINARY read of RUMP data not implemented.\n");
#endif
					}
				}
				break;

/*  (WR) Write RUMP style atomic data */
			case C_WRUMP:
				if (! RumpDataValid) {
					ERRputs("ERROR: No RUMP data available to write.\n");
					if (ZieglerDataValid) {
						TTYputs("Use the FIT command to generate it.\n");
					}
					LexFlush();
					break;
				}

				if (ZieglerDataValid &&  ! isfit) {
					ERRputs("WARNING: The data you are about to write is stale.\n");
				}

				if ( LexGetTokenP(token, sizeof(token), "[ASCII|Binary] (abort) ") ) {
					i = LexSelect(token,"ASCII BINARY");
					if (i == 1)   {
						ERRputs("ASCII write of RUMP data not implemented.\n");
						LexFlush();
					} else if (i == 2)   {
#if 0
						if ( LexGetTokenP(token, sizeof(token), "Filename for Binary write of RUMP data: ")) {
							RbsStash(token);
						}
#else
						ERRputs("BINARY write of RUMP data not implemented.\n");
#endif
					}
				}
				break;

/*  (RZ) Read Ziegler style atomic data */
			case C_RZIEGLER:
				if (LexGetTokenP(token, sizeof(token), "Filename for read of Ziegler data: ")) {
					ZieglerDataValid   = zread1(token);
					isfit = FALSE;
				}
				break;

/*  (WZ) Write Ziegler style atomic data */
			case C_WZIEGLER:
				ERRputs("Write of Ziegler style data not implemented.\n");
				LexFlush();
				break;

/*  (FIT) Compute RUMP stopping powers from Ziegler's (all elements) */
			case C_FIT:
				if (! ZieglerDataValid) {
					ERRputs("No Ziegler data available.  Use RZ command.\n");
					LexFlush();
				} else {
					for (i=1; i<=92; i++) fitone(i);
					RumpDataValid = TRUE;
					isfit = TRUE;
				}
				break;

/*  (TABLE) Edit Stopping Power Table */
			case C_TABLE:
				i = LexGetInt(0, "Stopping Power Table #: (abort) ");
				if (i <= 0) {
					ERRputs("Aborting Stopping Power Table edit.\n");
					LexFlush();
					break;
				}
				last_table=NULL;

				table = stop_tables; tabn=1;						/* Find the end */
				while (tabn<i && table != NULL) {table = table->next; tabn++;}

				if (table == NULL) {
					TTYprintf("Creating new table, entry #%d.\n",tabn);
					if (! LexYesNo(TRUE,"OK? [yes] ")) {
						ERRputs("Aborting Stopping Power Table edit.\n");
						LexFlush();
						break;
					}
					table = malloc(sizeof(STOPPING_TABLE) + NumElements*sizeof(STOPPING_POWER));
					if (table == NULL) {
						TTYputs("Ouch!\n");
						break;
					}
					if (last_table == NULL) {
						stop_tables = table;
					}
					else {
						last_table->next = table;  /* This table goes at end of list */
					}
					table->next  = NULL;
					table->z     = 2;
					table->mass  = 4.00150586f;
					table->nelem = NumElements;
					table->emin  = .35f;
					table->emax  = 3.5f;
					for (j=0; j<NumElements; j++) {    /* new values get computed as needed */
						table->stop[j].p[0] = STOP_INVALID;
					}
				}
tagain:
				TTYprintf("Stopping power table %d: Z = %d, m = %9.5f, E = (%9.4f,%9.4f)\n",
					tabn, table->z, table->mass, table->emin, table->emax);
				if (! LexGetTokenP(token,sizeof(token),"[Delete|Beam|Erange|Quit] "))
					break;
				i = LexSelect(token,"DELETE BEAM ERANGE QUIT");
				switch (i) {
					case 1:   /* Delete */
						if (last_table == NULL) {
							stop_tables = table->next;
						} else {
							last_table->next = table->next;
						}
						free (table);
						break;
					case 2:   /* Beam */
						table->z    =  LexGetInt(table->z,   "Nuclear Z: (unchanged) ");
						table->mass = LexGetReal(table->mass,"Mass: (unchanged) ");
						for (j=0; j<NumElements; j++) {    /* new values get computed as needed */
							table->stop[j].p[0] = STOP_INVALID;
						}
						goto tagain;
					case 3:   /* Erange */
						table->emin = LexGetReal(table->emin,"Emin: (unchanged) ");
						table->emax = LexGetReal(table->emax,"Emax: (unchanged) ");
						ORDERME(table->emin,table->emax);
						for (j=0; j<NumElements; j++) {    /* new values get computed as needed */
							table->stop[j].p[0] = STOP_INVALID;
						}
						goto tagain;
					case 4:   /* Quit */
					default:  /* none of the above */
						break;
				}
				break;

/*  (EL) Examine or modify individual element data */
			case C_ELEMENT:
				if (ZieglerDataValid || RumpDataValid)   {
					i = LexGetInt(0,"Element #? ");
					if (i >= 0) {
						elem(i);
					}
				} else {
					ERRputs("No data available.  Use RZ or RR command.\n");
					LexFlush();
				}
				break;

/*  (LI) Print out detailed stopping power table info */
			case C_LIST:
				TTYprintf("  #  Zbeam   Mbeam      emin      emax  nelem  #valid_elems\n");
				for (i=1,table=stop_tables; table!=NULL; table=table->next,i++) {
					vcount = 0;
					for (j=0;j<table->nelem;j++) {
						if (table->stop[j].p[0] != STOP_INVALID) vcount++;
					}
					TTYprintf(" %2d  %3d  %9.5f %9.4f %9.4f  %3d  %3d\n",
						i, table->z, table->mass, table->emin, table->emax, table->nelem, vcount);
				}
				break;

/*  (?) Print out the long list */
			case C_HELP:
				TTYprintf(" (EF) Energy range for fit:    %8.3f MeV to %8.3f MeV\n",
					efmin, efmax);
				for (i=1,table = stop_tables;table!=NULL;table=table->next,i++) {
					TTYprintf(" Table #%2d:   Z =%2d   Mass =%9.5f  emin,emax = %f,%f  type = %s\n",
						i, table->z, table->mass, table->emin, table->emax, (table->type==STOP_LINEAR)?"LINEAR":"SQRT");
				}
				TTYputs(
					" Commands: (RR/WR) Read/Write RUMP style atomic data\n"
					"           (RZ/WZ) Read/Write Ziegler style atomic data\n"
					"           (FIT)   Compute RUMP stopping powers from Ziegler's (all elements)\n"
					"           (LI)    Detailed list of current Stopping Power tables\n"
					"           (TABLE) Edit Stopping Power Table\n"
					"           (EL)    Examine or modify individual element data\n"
#ifdef STANDALONE
					"           (Q)     Quit this program\n"
#else
					"           (RET)   Return to RUMP\n"
#endif
					);

				TTYprintf("    RUMP atomic data currently %savailable.\n",RumpDataValid?"":"NOT ");
				TTYprintf(" Ziegler atomic data currently %savailable.\n",ZieglerDataValid?"":"NOT ");
				break;

/*  (Q) Quit this program */
			case C_QUIT:
				/* genoff(); */
				return;
		}
	}

	return;					/* Unexecuted command */
}

/* ===========================================================================
-- Routine to enter GENPLOT with a specified set of X,Y variables
--
-- For simplicity, this routine simply links the passed variables to a
-- temporary name and calls GENPLOT.  Curve temporary name is $GENPLT$.
=========================================================================== */
static void genplt(REAL *x, REAL *y, int npt, int nptmax) {

	CURVE *curve;
	int rcode;

	DBUG(TTYputs("Trying to get into GENPLOT!\n");)

	if ( (curve=GVLink2DCurve("$GENPLOT$", GVF_NORESIZE, x,y,nptmax)) == NULL) {
		ERRprintf("ERROR: Unable to link curve (genplt)\n");
		return;
	}
	curve->npt = npt;

/* PlotSetRange(Rmp->emin,Rmp->emax,Rmp->ymin,Rmp->ymax); */     /* just to start */ 
	DBUG(TTYputs("Here we go ..\n");)
	rcode = Genplot("$GENPLOT$");
	if (rcode & 0x0100) {									/* A "QUIT" executed in GENPLOT */
		ERRputs("WARNING: Normally, one does not quit from STOPP, but we will allow it\n");
		exit(rcode & 0xFF);
	}
	DBUG(TTYputs(" .. whew!\n");)
	return;
}

/* ===========================================================================
=========================================================================== */
static void elem( int elno) {

/*  -- Local variables -- */
	int i, j, j_1, j_2, np;
	char token[SHORT_STR_SIZE];
	REAL mas, ab, tot, twt, kev, mev, sn, se, rump_power;
	ATOMS *zp;
	STOPPING_TABLE *table;
	STOPPING_POWER *stop;

	CMTYPE2 *cmd;

	CMTYPE2 cmlist[] = {
		{"?",				1, CE_HELP},
		{"status",		4, CE_HELP},
		{"efit",			2, CE_EFIT},
		{"eplot",		2, CE_EPLOT},
		{"splot",		2, CE_SPLOT},
		{"asymbol",		2, CE_ASYMBOL},
		{"z",				1, CE_Z},
		{"isotop",		2, CE_ISOTOP},
		{"density",		2, CE_DENSITY},
		{"fvelocity",	2, CE_FVELOCIT},
		{"zparam",		2, CE_ZPARAM},
		{"rpoly",		2, CE_RPOLY},
		{"fit",			3, CE_FIT},
		{"compute",		2, CE_COMPUTE},
		{"plot",			2, CE_PLOT},
		{"return",		3, CE_RETURN},
		{NULL,			0,	CE_RETURN}
	};

/*  -- Code begin -- */
	zp = &(atom[elno-1]);

	while (TRUE) {
		if ( ! LexGetTokenP(token, sizeof(token), "Element Command: ")) continue;
		if (( cmd = LexCmdl(token, cmlist, sizeof(CMTYPE2))) == NULL ) {
			ERRprintf("ERROR: %s -- unknown command.  Use ? for help.\n", token);
			LexFlush();
			continue;
		}

/*  (?) Write the long list */

		switch (cmd->rcode) {

			case CE_HELP:
				TTYprintf(" (EF) Energy range for fit:    %8.3f MeV to %8.3f MeV\n"
							 " (EP) Energy range for plot:   %8.3f MeV to %8.3f MeV\n",
								efmin, efmax, epmin, epmax);

/* (LT) Line type: xx */
/* (SY) Symbol:    xx */
/* (AS) Atomic Symbol:   Si */
/* (Z)  Atomic charge:   14 */

				if (elno <= 83) {
					char head[15];
					tot  = 0.0;
					twt  = 0.0;
					strcpy(head,"(IS) Isotopes: ");
					for (i=0; i < NISOT ; i++ ) {
						if (zp->isotop[i].mass != 0)   {
							tot = tot + zp->isotop[i].fraction;
							twt = twt + zp->isotop[i].fraction*zp->isotop[i].mass;
							TTYprintf(" %s Mass =%9.4f  Abundance =%9.4f%%\n",
								head, zp->isotop[i].mass, zp->isotop[i].fraction*100.);
							strcpy (head,"               ");
						}
					}
					TTYprintf("        Atomic Weight: %9.4f  Total     =%9.4f%%\n",
						twt, tot*100.);
				} else {
					TTYprintf("        Atomic Weight:  %9.4f\n", zp->mass);
				}

				TTYprintf(" (DE) Density:        %9.3f  E23 atoms/cm^3  (E15 atoms/cm^2/A)\n",
					atom[elno-1].dense*1.e-23);

				if ( ! ZieglerDataValid)   {
					TTYputs(" Ziegler atomic data currently NOT present.\n");
				} else {
					TTYprintf(	"  Most common isotope:    %3d   %9.3f amu\n"
									"  Average atomic weight:        %9.3f amu\n"
									"  Density:              %12.5e g/cm3%12.5e atoms/cm3\n"
									"  Lambda screening factor:      %8.4f  Vfermi:  %9.3f v0\n",
						zp->zmm1, zp->zm1, zp->zm2, zp->zrho, zp->zatrho, zp->zlfctr, zp->zvferm);
					TTYprintf(	"  Ziegler Proton Coefficients: %10.5f %10.5f %10.5f\n"
									"  %10.5f %10.5f %10.5f %10.5f %10.5f\n",
						zp->zpcoef[0], zp->zpcoef[1], zp->zpcoef[2], zp->zpcoef[3], 
						zp->zpcoef[4], zp->zpcoef[5], zp->zpcoef[6], zp->zpcoef[7]);
				}

				if (! RumpDataValid) {
					TTYputs(" RUMP atomic data currently NOT present.\n");
				} else {
					TTYputs(" (RP) RUMP stopping power polynomial coefficients:\n");
#if 1
					for (table=stop_tables,np=1;table!=NULL;table=table->next,np++) {
						stop = &(table->stop[elno-1]);
						TTYprintf("  Particle %2d: %17.6e%17.6e%17.6e\n"
									 "               %17.6e%17.6e%17.6e\n",
							np,stop->p[0],stop->p[1],stop->p[2],
							stop->p[3],stop->p[4],stop->p[5]);
					}
#endif
				}

				TTYputs(" (FIT)  Fit RUMP stopping powers from Ziegler's\n"
						  " (COMP) Compute stopping powers at energy\n"
						  " (PLOT) Plot stopping powers vs. energy\n");

				break;

/*  (EF) Energy range for fit:    xxxx.x MeV to xxxx.x MeV */
			case CE_EFIT:
				efmin = LexGetReal(efmin,"MeV range to be fit? (Unchanged) ");
				efmax = LexGetReal(efmax,"Upper MeV limit for fit? (Unchanged) ");
				ORDERME(efmin, efmax);
				newFitRange( efmin, efmax);
				isfit = FALSE;
				break;

/*  (EP) Energy range for plot:   xxxx.x MeV to xxxx.x MeV */
			case CE_EPLOT:
				epmin = LexGetReal(epmin,"MeV range for plot? (Unchanged) ");
				epmax = LexGetReal(epmax,"Upper MeV limit for plot? (Unchanged) ");
				ORDERME(epmin, epmax);
				break;

/*  (SP) Stopping power for plot: xxxx.xE-15 to xxxx.xE-15 eV-cm^2 */
			case CE_SPLOT:
				spmin = LexGetReal(spmin,"SP range for plot? (Unchanged) ");
				spmax = LexGetReal(spmax,"Upper SP limit for plot? (Unchanged) ");
				ORDERME(spmin, spmax);
				break;

/*  (AS) Atomic Symbol:   Si */
			case CE_ASYMBOL:
				if (LexGetTokenP(token, sizeof(token), "New atomic symbol: ")) 
					strscpy(zp->name, token, sizeof(zp->name));
				break;

/*  (Z)  Atomic charge:   14 */
			case CE_Z:
				ERRputs("Not implemented.\n");
				LexFlush();
				break;

/*  (IS) Isotopes: Mass = xxxx.xxx  Abundance = xxx.xxxx% */
			case CE_ISOTOP:
				mas = LexGetReal(0.,"Atomic mass: (abort) ");
				if (mas <= 0)   {
					LexFlush();
				} else {
					j_1 = 0;
					j_2 = 0;
					for (i=6; i <= 1 ; i++ ) {
						if (zp->isotop[i].mass - mas <= 0.3 &&
							zp->isotop[i].fraction != 0) j_1 = i;
						if (zp->isotop[i].fraction == 0) j_2 = i;
					}
					if (j_1 == 0 && j_2 == 0)   {
						ERRputs("No room for new isotope.\n");
						TTYputs("Delete one (by setting its abundance to zero) and retry.\n");
						LexFlush();
						break;
					} else if (j_1 == 0)   {
						TTYprintf(" Creating new isotope with mass%9.3f\n",mas);
						j_1 = j_2;
					} else {
						TTYprintf(" Replacing isotope mass%9.3f with%9.3f\n",zp->isotop[j_1].mass,mas);
					}

/*  should be able to process A for abort, R for "sum to 100", 0 for zero */
					if ( ! LexGetTokenP(token, sizeof(token),
						"[ Frac Abundance | Abort | Rest ]: (A) ") ) {
						strcpy(token,"A");
						}
					if (LexEqual(token,"R",1))   {
						tot = 0.0;
						for (i=0; i < NISOT ; i++ ) {
							tot = tot + zp->isotop[i].fraction;
						}
						zp->isotop[j_1].mass = mas;
						zp->isotop[j_1].fraction = zp->isotop[j_1].fraction + ( 1.0f - tot );
					} else if (! LexEqual(token,"A",1)) {
						ab = (REAL) atoi(token);
						zp->isotop[j_1].mass = mas;
						zp->isotop[j_1].fraction = ab;
					}
				}
				break;

/*  (DE) Density:         xxxx.xxx E23 atoms/cm^3  (E15 atoms/cm^2/A) */
			case CE_DENSITY:
				zp->dense = LexGetReal(zp->dense, "Density of element? (Unchanged) ");
				break;

/*  (FV) Fermi velocity:  xxxx.xxx v0 */
			case CE_FVELOCIT:
				ERRputs("Not yet implemented.\n");
				LexFlush();
/*       IF (ZIEGLERDATAVALID) THEN */
/*          ZVFERM(ELNO) = RDARG(ZVFERM(ELNO), */
/*      +                 'Fermi Velocity? (Unchanged) ') */
/*       ELSE */
/*          CALL ERTYPE('Ziegler data not loaded.') */
/*          CALL CFLUSH */
/*       ENDIF */
				break;

/*  (ZP) Ziegler Parameters:  something x 6 */
			case CE_ZPARAM:
				ERRputs("Not yet implemented.\n");
				break;

/*  (RP) RUMP Polynomial:     something x 18 */
			case CE_RPOLY:
				ERRputs ("Not yet implemented.\n");
				break;

/*  (FIT)  Fit (this element) RUMP stopping powers from Ziegler's */
			case CE_FIT:
				if (ZieglerDataValid)   {
					fitone(elno);
				} else {
					ERRputs("Ziegler data not loaded.\n");
					LexFlush();
				}
				break;

/*  (COMP) Compute stopping powers at energy */
			case CE_COMPUTE:
				mev = LexGetReal(0.,"Energy (MeV) for evaluation: (abort) ");
				if (mev > 0.0)   {
					TTYprintf("               RUMP          Ziegler\n");
					kev = mev * 1000.0f;
					for (table=stop_tables,np=1; table!=NULL; table=table->next,np++ ) {
						TTYprintf(" Particle%2d: ");
						if (RumpDataValid) {
							stop = &(table->stop[elno-1]);
							if (stop->p[0] == STOP_INVALID) RbsGenStopp(elno, table);
							if (table->type == STOP_LINEAR) {
								rump_power = LINE_S_POWER(stop, LINE_S_XFORM(kev));
							} else {
								rump_power = (REAL) SQRT_S_POWER(stop, SQRT_S_XFORM(kev));
							}
							TTYprintf("  %15.4e ", rump_power);
						} else {
							TTYprintf("    unavailable   ");
						}
						if (ZieglerDataValid) {
							zstop(table->z, table->mass, elno, kev, &se, &sn, 1);
							TTYprintf("  %15.4e ", sn+se);
						} else {
							TTYprintf("    unavailable   ");
						}
						TTYprintf(" eV-cm^2\n", np, rump_power, sn+se);
					}
				}
				break;

/*  (PLOT) Plot stopping powers vs. energy */
			case CE_PLOT:
				np = LexGetInt(0,"What particle are you interested in? (abort) ");
				table = stop_tables; j = 1;
				while (j<np && table != NULL) {table = table->next; j++;}
				if ( (table==NULL) || (np<=0)) {
					ERRputs("Aborting plot. Use ? to list particles.\n");
					LexFlush();
					break;
				}

				if (RumpDataValid)   {
					DBUG(TTYprintf(" Energy (MeV)   Rump Stopping Power\n");)
					for (j=0; j < NPLOT ; j++ ) {
						mev     = epmin + (epmax-epmin)*(j)/((REAL)(NPLOT-1));
						ener[j] = mev;
						kev     = mev * 1000.0f;
						stop = &(table->stop[elno-1]);
						if (stop->p[0] == STOP_INVALID) RbsGenStopp(elno, table);
						if (table->type == STOP_LINEAR) {
							rump_power = LINE_S_POWER(stop, LINE_S_XFORM(kev));
						} else {
							rump_power = (REAL) SQRT_S_POWER(stop, SQRT_S_XFORM(kev));
						}
						spow[j] = rump_power;
						DBUG(TTYprintf(" %f     %f\n",mev,rump_power);)
					}
					TTYputs("Entering GENPLT with RUMP curve.\n");
					genplt(ener,spow,NPLOT,NPLOT);
				} else {
					TTYputs("No RUMP curve available to plot.\n");
				}

				if (ZieglerDataValid)   {
					DBUG(TTYprintf(" Energy (MeV)   Ziegler Stopping Power\n");)
					for (j=0; j < NPLOT ; j++ ) {
						mev = epmin + (epmax-epmin)*(j)/((REAL)(NPLOT-1));
						ener[j] = mev;
						kev = mev * 1000.0f;
						zstop(table->z, table->mass, elno, kev, &se, &sn, 1);
						spow[j] = sn + se;
						DBUG(TTYprintf(" %f     %f\n",mev,spow[j]);)
					}
					TTYputs("Entering GENPLT with Ziegler curve.\n");
					genplt(ener,spow,NPLOT,NPLOT);
				} else {
					TTYputs("No Ziegler data available to plot.\n");
				}
				break;
 
/*  (RET) Return to calling program */
			case CE_RETURN:
				return;

		}  /* end of huge "switch" statement */
	}

	return;										/* Never reached */
}


/* ===========================================================================
=========================================================================== */
static void newFitRange( REAL fmin, REAL fmax ) {
	STOPPING_TABLE *table;
	int i;

	if (ZieglerDataValid) {					/* Don't mess people up unnecessarily */
		for (table=stop_tables; table!=NULL ; table=table->next) {
			table->emin = fmin;
			table->emax = fmax;
			for (i=0; i<table->nelem; i++) table->stop[i].p[0] = STOP_INVALID;
		}
	}
	return;
}

/* ===========================================================================
=========================================================================== */
static void fitone( int elno) {
	STOPPING_TABLE *table;

	for (table=stop_tables; table!=NULL; table=table->next) RbsGenStopp(elno, table);
	return;
}
