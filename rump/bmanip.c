/*  bmanip.c */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef enum _OPS1 {
	B1_BUFFERS,		B1_ACTIVE,		B1_IDENT,		B1_DATE,			B1_MEV, 
	B1_CHARGE,		B1_CONVERSI,	B1_KEVCH,		B1_KEV0,			B1_CORRECTI,
	B1_THETA,		B1_PHI,			B1_PSI,			B1_OMEGA,		B1_CHOFF,
	B1_CURRENT,		B1_FWHM,			B1_TAU,			B1_GEOMETRY,	B1_BEAM,
	B1_FILE,			B1_SWALLOW,		B1_COMPRESS,	B1_REWRITE,		B1_WRASCII,
	B1_WRITE,		B1_RECALC,     B1_SPECTYPE,
/* ... Internal use only keys ... */
	B1_RESET,		B1_CLEAR,		B1_DEFAULT,
/* ... Multiple buffer processing ... */
	B_POINTAT,		B_NEWALL,		B_EMPTY,			B_RELEASE,		B_MOVE,
	B_COPY,			B_XCOPY,			B_ADD,			B_XADD,
	B_SUBTRACT,		B_XSUBTRACT,	B_DIVIDE
} OPS1;

typedef struct _CMTYPE {
	char *name;
	int	minlen;
	OPS1	rcode;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void RbsBuffList( void );
static int RbsBuffOne( SPECTRUM *ibf, OPS1 key);
static int RbsBuffProc(int key);
static int RbsBuffTwo(SPECTRUM *isrc, SPECTRUM *idest, int key);
	#define BF_CPY  1			/*  Buffer copy		*/
	#define BF_ADD  2			/*  Buffer add			*/
	#define BF_SUB  3			/*  Buffer subtract	*/
	#define BF_DIV  4			/*  Buffer divide		*/

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Global Variables 					  */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* Note the painful handling of "identifier" to avoid conflict with
   and "identify" in plsystem.  What a pain in the compatibility butt! */
static CMTYPE cmlist1[] = {
	{"buffers",			2,	B1_BUFFERS },  {"listbuffers",  -4, B1_BUFFERS },
	{"active",			2,	B1_ACTIVE  },  {"status",       -4, B1_ACTIVE  },
	{"description",   4, B1_IDENT   },
	{"identifier",	  -8,	B1_IDENT   },	{"iden",			  -3, B1_IDENT   },
	{"date",				4,	B1_DATE    },
	{"mev",				3,	B1_MEV     },  {"energy",		  -4, B1_MEV     },
	{"charge",			2,	B1_CHARGE  },  {"q",				  -1, B1_CHARGE  },
	{"conversion",		4,	B1_CONVERSI},	{"scales",		  -5, B1_CONVERSI},
													{"econvert",	  -5, B1_CONVERSI},
	{"kev/ch",			4, B1_KEVCH   },
	{"kev(0)",			4, B1_KEV0    },
	{"correction",		3,	B1_CORRECTI},
	{"theta",			3,	B1_THETA   },
	{"phi",				3,	B1_PHI     },
	{"psi",				3,	B1_PSI     },
	{"omega",			5,	B1_OMEGA   },
	{"choff",			5,	B1_CHOFF   },
	{"current",			4,	B1_CURRENT },  {"curent",		  -6,	B1_CURRENT },
	{"fwhm",				4,	B1_FWHM    },	{"resolution",	  -5, B1_FWHM    },
	{"tau",				3, B1_TAU	  },	{"shaping",		  -4, B1_TAU     },
	{"geometry",		4,	B1_GEOMETRY},
	{"beam",				4,	B1_BEAM    },	{"particle",	  -4, B1_BEAM    },
	{"spectrum",      4, B1_SPECTYPE},
	{"filename",		4,	B1_FILE    },
	{"swallow",			7,	B1_SWALLOW },
	{"compress",		8,	B1_COMPRESS},
	{"rewrite",			3,	B1_REWRITE },	{"save",			  -4, B1_REWRITE },
	{"write",			5, B1_WRITE   },
	{"saveas",			6, B1_WRITE   },
	{"wrascii",			5,	B1_WRASCII },
	{"recalculate",	5, B1_RECALC  },
	{ NULL,				0,	B1_RECALC  }
};

CMTYPE cmlist2[] = {
	{"pointat",	  -2,	B_POINTAT },
	{"newall",		3,	B_NEWALL  },
	{"empty",		3,	B_EMPTY   },
	{"release",		3,	B_RELEASE },
	{"move",			4,	B_MOVE    }, 	{"mv",		  -2,	B_MOVE    },
	{"copy",			4,	B_COPY    },   {"cp",		  -2,	B_COPY    },
												{"xcopy",	  -5, B_XCOPY   },
	{"add",			3,	B_ADD     },	{"xadd",		  -4,	B_XADD    },
	{"subtract",	5,	B_SUBTRACT},	{"xsubtract", -4,	B_XSUBTRACT},
	{"divide",		3,	B_DIVIDE  },
	{ NULL,			0, B_DIVIDE  }
};
 
/* ===========================================================================
--      BOOL FUNCTION BMANIP(KEY, TOKE)
--  Usage Guide:
--
--      BOOL FUNCTION BMANIP(KEY, TOKE)
--
--  BMANIP does all the buffer manipultion commands, including
--  all modification of spectral parameters.
--  It is one of the four command processors called by the main routine
--  RUMP.
--  Quick: Processor for user's buffer manipulation commands
--  Called by     LOG = BMANIP(KEY, TOKEN)
--        where:
--          KEY    Chooses the operation:  0 = Process command
--                                        -1 = Initialize
--                                        -2 = Reset
--                                        -3 = List Commands
--                                        -4 = Display Parameters
--                                        -5 = Turn Off
--          TOKEN  is the command (Character string) for the case KEY = 0
--          LOG    (Function value) True only for Key = 0 and the command
--                 was found and action was attempted.
--
--  ... May 18, 1986 - MOT
--      Added COMPRESS command to compress channels of a file.
=========================================================================== */
int RbsBmanip(int key, char *token) {

	int i, j, rcode;
	SPECTRUM *tmp_ibf;
	CMTYPE *cmd;

/*  -- Code begin -- */
	if (key > 0) {
		gen_err("Unknown key to BMANIP");
		return(FALSE);
	}

	switch (key) {
		case U_INIT:								/* Our part of the reset operation */
		case U_RESET:
			Rmp->autsim = 0;						/*  No active simulation */
			return(FALSE);

		case U_HELP:								/* Our part of command listing */
			LexCmdlPrint(cmlist1,sizeof(CMTYPE),"Buffer operations:");
			LexCmdlPrint(cmlist2,sizeof(CMTYPE),NULL);
			return(FALSE);

		case U_PARM:								/* Our part of parameters */
			return(FALSE);

		case U_QUIT:								/* Our part of quiting */
			return(FALSE);

		case 0:
			break;

		default:
			return(FALSE);
	}

	if ( (cmd = LexCmdl(token, cmlist1, sizeof(CMTYPE))) != NULL)  {
		RbsBuffOne(ibuf, cmd->rcode);				/* Responsible for errors outself */
		return(TRUE);
	}

	if ( (cmd = LexCmdl(token, cmlist2, sizeof(CMTYPE))) == NULL)
		return(FALSE);								/* Command is not ours */

	rcode = TRUE;
	switch (cmd->rcode) {

/* -------------------------------------------------------------------------
----------------------  BUFFER MANIPULATION COMMANDS -----------------------
--------------------------------------------------------------------------- */

/* .... pointat - Point at a specific buffer as active */
		case B_POINTAT:
			if ( (ibuf = RbsGetBuf("Buffer: ",ibuf)) == NULL) {
				ibuf = MAINBUF;
				rcode = FALSE;
			}
			break;

/* .... newall - Reset and empty all buffers */
		case B_NEWALL:
			for (i=0; i<=RbsNumBuf; i++) {
				RbsFreeSpectrum(RbsBuffers[i]);
				RbsBuffers[i] = RbsAllocateSpectrum(NULL, CMAX);
			}
			RbsFreeSpectrum(RbsTempBuf);
			RbsTempBuf   = RbsAllocateSpectrum(NULL, CMAX);
			RbsActiveBuf = MAINBUF;
			Rmp->autsim = 0;
			break;

/* .... empty - Creates a blank array and points to it */
		case B_EMPTY:
			if ( (tmp_ibf = RbsAllocateSpectrum(NULL, CMAX)) == NULL) {
				ERRprintf("Unable to allocate a clear spectrum\n");
				rcode = FALSE;
				break;
			}
			RbsBufferScroll(tmp_ibf);
			ibuf = MAINBUF;
			RbsBuffOne(ibuf, B1_RESET);			/*  Clear the preliminaries */
			RbsBuffOne(ibuf, B1_CLEAR);			/*  Fill buffer with zeros */
			break;

/* .... move - Interchanges any two buffers */
		case B_MOVE:
			i = LexGetInt(1,"Buffer numbers to interchange? (1) ");
			j = LexGetInt(2,"Other buffer? (2) ");
			if (i == 0 || j == 0)  SimCheck(NULL);	/*  Perhaps redo simulation */
			tmp_ibf = buffers[i];
			buffers[i] = buffers[j];
			buffers[j] = tmp_ibf;
			break;

/* .... copy - Copy one buffer to another */
		case B_COPY:
		case B_XCOPY:
			rcode = RbsBuffProc( (cmd->rcode==B_COPY) ? BF_CPY : -BF_CPY);
			break;

/* .... add - destination = destination + source */
		case B_ADD:
		case B_XADD:
			rcode = RbsBuffProc( (cmd->rcode==B_ADD) ? BF_ADD : -BF_ADD);
			break;

/* .... subtract - destination = destination - source */
		case B_SUBTRACT:
		case B_XSUBTRACT:
			rcode = RbsBuffProc( (cmd->rcode==B_SUBTRACT) ? BF_SUB : -BF_SUB);
			break;

/* --------------------------------------------------------------------------
--  divide - DESTINATION = DESTINATION / SOURCE  (FOR Xmin determinations)
--       For the moment, let the user beware of keeping reasonable parameters.
--       Will only take care of offsets in data start and nothing else.
--------------------------------------------------------------------------- */
		case B_DIVIDE:
			rcode = RbsBuffProc(BF_DIV);
			break;

/* .... release - Releases current buffer and mark as unused */
		case B_RELEASE:
			i = RbsGetBufNum(ibuf);
			if (i == BUFF_UNKNOWN) break;				/* Unlikely (I hope)! */
			RbsBuffOne(ibuf, B1_RESET);				/*  Clear the usual stuff */
			for (j=i; j<=RbsNumBuf-1; j++) RbsBuffers[j] = RbsBuffers[j+1];
			RbsBuffers[RbsNumBuf] = ibuf;
			ibuf = RbsBuffers[i];
			break;

		default:
			gen_err("Internal bmanip error");
			rcode = FALSE;
	}

	if (! rcode) LexFlush();					/*  Here on errors! */
	return(TRUE);									/* Command was handled */
}


/* ===========================================================================
-- Here for changing attributes of the spectrum file
-- To make change permanent, a rewrite must be executed
--
--  Modified 5/14/89 LRD:
--   Changed all occurences of ibuf to ibf, as they should have been all along.
--   Didn't matter much because the only caller used ibuf as actual parameter.
--   Change is for form, clarity, and future use.
--   Also added new reset functions, collected from EMPTY, RELEASE, and RSTBUF.
=========================================================================== */
static int RbsBuffOne(SPECTRUM *ibf, OPS1 key) {

	int i, j, k, i1, force, rcode;
	FILE *lun;
	char token[DFLT_STR_SIZE], filename[PATH_MAX];
	char *aptr, *bptr;
	BOOL btmp, IsConsole, twocol;

/*  -- Code begin -- */
	if (ibf == NULL) {
		gen_err("Null spectrum pointer passed to RbsBuffOne!\n");
		return (FALSE);
	}

	rcode = TRUE;
	switch (key) {

/* .... buffers - List out all current buffers with files specified */
		case B1_BUFFERS:
			RbsBuffList();
			break;

/* .... Prints out all the available information about the current buffer */
		case B1_ACTIVE:
			RbsActive(ibf);
			break;

/* .... New descriptive text for file */
		case B1_IDENT:
			if (! LexGetToken(ibf->id, sizeof(ibf->id)) )
				LexPromptStr(ibf->id, sizeof(ibf->id), "Description: ");
			ibf->modify = TRUE;
			break;

/* .... New date for file */
		case B1_DATE:
			if (! LexGetToken(ibf->date, sizeof(ibf->date)) )
				LexPromptStr(ibf->date, sizeof(ibf->date), "Date: ");
			ibf->modify = TRUE;
			break;

/* .... New incident beam energy */
		case B1_MEV:
			ibf->e0 = LexGetReal(ibf->e0, "Incident beam energy (same): ");
			ibf->modify = TRUE;
			break;

/* .... New integrated charge */
		case B1_CHARGE:
			ibf->q = LexGetReal(ibf->q, "Total integrated charge (same): ");
			ibf->modify = TRUE;
			break;

/* .... New factors for energy to channel conversion */
		case B1_CONVERSI:
			ibf->kevch = LexGetReal(ibf->kevch, "KeV/channel (same): ");
			ibf->kev0 = LexGetReal(ibf->kev0, "Energy of channel 0 (same): ");
			ibf->modify = TRUE;
			break;

		case B1_KEVCH:
			ibf->kevch = LexGetReal(ibf->kevch, "KeV/channel (same): ");
			ibf->modify = TRUE;
			break;
			
		case B1_KEV0:
			ibf->kev0 = LexGetReal(ibf->kev0, "Energy of channel 0 (same): ");
			ibf->modify = TRUE;
			break;

/* .... New multiplicative factor for all data */
		case B1_CORRECTI:
			ibf->corr = LexGetReal(ibf->corr, "Correction factor (same): ");
			ibf->modify = TRUE;
			break;

/* .... New beam-sample normal angle */
		case B1_THETA:
			ibf->theta = LexGetReal(ibf->theta, "Beam-sample normal angle (same): ");
			ibf->modify = TRUE;
			break;

/* .... New detector to theta plane angle */
		case B1_PHI: 
			ibf->phi = LexGetReal(ibf->phi, "Detector-theta plane angle (same): ");
			ibf->modify = TRUE;
			break;

/* .... New detector to sample normal angle */
		case B1_PSI:
			if (ibf->geom != GENERAL)
				gen_warn("Psi only meaningful in GEOM GENERAL mode");
			ibf->psi = LexGetReal(ibf->psi, "Detector-theta plane angle (same): ");
			ibf->modify = TRUE;
			break;

/* .... New detector solid angle */
		case B1_OMEGA:
			ibf->omega = LexGetReal(ibf->omega, "Detector solid angle (same): ");
			ibf->modify = TRUE;
			break;

/* .... channel - Channel number of first data point (effectively offset) */
		case B1_CHOFF:
			ibf->first = LexGetReal(ibf->first, "Channel of first data point (same): ");
			ibf->modify = TRUE;
			break;

/* .... current - Average beam current */
		case B1_CURRENT:
			ibf->current = LexGetReal(0.0, "Average beam current (0 nA): ");
			ibf->modify = TRUE;
			break;

/* .... FWHM - Full width at half maximum of detector resolution */
		case B1_FWHM:
			ibf->fwhm = LexGetReal(20.0, "Detector resolution - (20 keV): ");
			ibf->modify = TRUE;
			break;

		case B1_TAU:
			ibf->tau = LexGetReal(5.0, "MCA detector shaping time constant (5 uS): ");
			ibf->modify = TRUE;
			break;

/* .... geometry - Change geometry of scattering */
		case B1_GEOMETRY:
			if (LexGetTokenP(token, sizeof(token),
								  "Scattering geometry (CORNELL ibm general): ")) {
				i = LexSelect(token, "CORNELL IBM GENERAL");
				if (i == 1)
					ibf->geom = CORNELL;
				else if (i == 2)
					ibf->geom = IBM;
				else if (i == 3)
					ibf->geom = GENERAL;
				else {
					ERRprintf("ERROR: Unrecognized geometry (%s) - using CORNELL\n", token);
					ibf->geom = CORNELL;
				}
				ibf->modify = TRUE;
			}
			break;

/* .... beam - Change scattered particle */
		case B1_BEAM:
			if (LexGetTokenP(token, sizeof(token),
								  "Scattered particle - He++,H,3He,... (abort): ")) {
		   /* Compatibility with previous WRASCII's */
				if (LexEqual(token, "D",   1)) strcpy(token, "2H" );
 			   if (LexEqual(token, "HE3", 3)) strcpy(token, "3HE");
				if (! RbsIdentp(token, &i, &i1, &j)) {
					gen_err("Scattering particle unrecognized");
					return(FALSE);
				}
				ibf->zbeam  = i;
				ibf->cbeam  = j;
				ibf->mbeam  = RbsGetRealMass(ibf->zbeam, i1);
				ibf->modify = TRUE;
			}
			break;

/* .... beam - Change spectrum type */
		case B1_SPECTYPE:
			if (LexGetTokenP(token, sizeof(token),
								  "Spectrum type (RBS Fres Pixe Nuclear): ")) {
				i = LexSelect(token, "RBS FRES PIXE NUCLEAR");
				if (i == 1)
					ibf->type = RBS;
				else if (i == 2)
					ibf->type = FRES;
				else if (i == 3)
					ibf->type = PIXE;
				else if (i == 3)
					ibf->type = NUCLEAR;
				else {
					ERRprintf("ERROR: Unrecognized spectrum type (%s) - using RBS\n", token);
					ibf->type = RBS;
				}
				ibf->modify = TRUE;
			}
			break;
			
/* .... filename - Modifies the file which data will be rewritten to */
		case B1_FILE:
			if (LexGetFileP(filename, sizeof(filename), "New filename (no change): ")) {
				SysAddExt(filename, ".rbs");
				strscpy(ibf->filename, filename, sizeof(ibf->filename));
				ibf->RdwrProc = NULL;				/* Name changed */
			}
			break;

/* .... swallow - Incorporate new data as the spectrum */
		case B1_SWALLOW:
			twocol = FALSE;
			if (LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-twocolumn", 4) || LexEqual(token, "-2column", 5)) {
					twocol = TRUE;
				} else if (LexEqual(token, "-onecolumn", 4) || LexEqual(token, "-1column", 5)) {
					twocol = FALSE;
				} else {
					ERRprintf("ERROR: Illegal option for ASCII read command (%s)\n", token);
					rcode = FALSE;
					break;
				}
			}

			IsConsole = LexQueryActiveInputID(NULL) == 0;
			if (IsConsole) TTYprintf("Enter channel data, ending with a blank line\n");
			if (! IsConsole) btmp = LexSetLocalNoEcho(TRUE);

			for (i=0; i<CMAX; i++) ibf->counts[i] = 0.0;
			ibf->npt = 0;

			for (i=0; i<CMAX; i++) {										/* Now start reaeding */
				if (! twocol) {
					j = i;
				} else {
					j = LexGetInt(-999, "Channel: ");
					if (j == -999) break;
					if (j < 0) j = 0;
					if (j >= CMAX) j = CMAX-1;
				}
				ibf->counts[j] = LexGetReal(-999.0, "Channel data: ");
				if (ibf->counts[j] == -999.0) break;
				ibf->npt = max(ibf->npt, j+1);
			}
			if (! IsConsole) LexSetLocalNoEcho(btmp);
			ibf->dirty = TRUE;
			i = RbsGetBufNum(ibf);
			TTYprintf("%d points entered into buffer %d\n", ibf->npt, i);
			break;

/* .... compress - Adds adjacent channels together to improve S/N ratio */
		case B1_COMPRESS:
			i1 = LexGetInt(2,"Channels per compression (2): ");
			if (i1 < 2 || i1 > ibf->npt) return(TRUE);	/* Ignore stupid ones */
			ibf->counts[0] /= 2;									/* Stupid correction  */
			k = 0;
			for (i=0; i<ibf->npt; i+=i1) {	/*  Go through all the data in i1 steps */
				for (j=i; j<min(i+i1,ibf->npt); j++) {		/*  Add I1 channels  */
					ibf->counts[k] += ibf->counts[j];
				}
				k++;
				ibf->counts[k] = 0;
			}
			ibf->npt     = k;
			ibf->dirty   = TRUE;
			ibf->kev0 += (REAL) (ibf->kevch * (i1-1)/2.0);
			ibf->kevch *= i1;
			ibf->first  /= i1;
			ibf->modify  = TRUE;
			break;

/* .... rewrite - Rewrites the data to the disk */
		case B1_REWRITE:
			if (strlen(ibf->filename) == 0) {
				ERRprintf("ERROR: There is no filename for the buffer\n");
				rcode = FALSE;
			} else if (! RbsWriteFile(ibf, TRUE)) {
				rcode = FALSE;
			}
			break;

/* .... write - Writes the data to the disk under a new filename */
/* .... wrascii - ASCII write to file                            */
		case B1_WRITE:
			if (LexCheckHelp("Write", NULL, NULL)) {		/* Dummy check to pass on help request */
				rcode = RbsWriteFile(ibf, FALSE);
			} else if (LexGetTokenP(filename, sizeof(filename), "Filename to write (abort): ")) {
				SysAddExt(filename, ".rbs");
				SysQualifyPath(filename, filename, sizeof(filename));

				strscpy(ibf->filename, filename, sizeof(ibf->filename));
				ibf->RdwrProc = NULL;					/* Name changed */
				rcode = RbsWriteFile(ibf, FALSE);
			}
			break;

/* .... wrascii - ASCII write to file                            */
		case B1_WRASCII:
			if (! LexGetTokenP(filename, sizeof(filename), "File for ASCII output of spectrum: (abort) ")) break;
			force = FALSE;
			while (rcode && LexGetOption(token, sizeof(token))) {
				if (LexEqual(token, "-force", 2) || LexEqual(token, "-yes", 2)) {
					force = TRUE;
				} else {
					ERRprintf("ERROR: Option %s not recognized by this command\n", token);
					rcode = FALSE;
				}
			}
			if (! rcode) break;

			SysQualifyPath(filename, filename, sizeof(filename));
			if (! force && access(filename, R_OK) == 0) {
				sprintf(token, "WARNING: %s exists.  Overwrite? (NO) ", filename);
				rcode = LexYesNo(FALSE, token);		/* Shall we proceed? */
				if (! rcode) break;
			}

			if ( (lun = fopen(filename, "w")) == NULL) {
				ERRprintf("ERROR: %s failed to open - WRASCII aborts\n", filename);
				rcode = FALSE;
			} else {
				RbsBeamCode(ibf, token);
				aptr = "??";
				if (ibf->geom == IBM )    aptr = "IBM";
				if (ibf->geom == CORNELL) aptr = "Cornell";
				if (ibf->geom == GENERAL) aptr = "General";
				                          bptr = "RBS";
				if (ibf->type == FRES)    bptr = "FRES";
				if (ibf->type == PIXE)    bptr = "PIXE";
				if (fprintf(lun,	"Empty File '%s'\n"
									   "Spectrum    %s\n"
										"Ident      '%s'\n"
										"Date       '%s'\n"					
										"Charge      %f     MeV  %f\n"
										"Conversion %f %f\n"			
										"Theta       %f     Phi  %f\n" 
										"Omega       %f     Corr %f\n"		
										"Choff       %f     FWHM %f\n" 
										"Current %f\n"					
										"Geometry %s        Beam %s\n"
										"Swallow\n",
										ibf->filename, bptr, ibf->id, 
										ibf->date, ibf->q, ibf->e0,
										ibf->kevch, ibf->kev0,
										ibf->theta, ibf->phi, ibf->omega, ibf->corr,
										ibf->first, ibf->fwhm, ibf->current,
										aptr, token) < 0) rcode = FALSE;
				for (i=0; rcode && i<ibf->npt; i++) 
					rcode = (fprintf(lun, "%f\n", ibf->counts[i]) >= 0);
				fprintf(lun, "\n");
				fclose (lun);
				if (! rcode) 
					ERRprintf("ERROR: Failure during file write - WRASCII aborts\n");
			}
			ibf->dirty  = FALSE;
			ibf->modify = FALSE;
			break;

/* ... recalc - mark the altbuffer as invalid so will be recalculated */
		case B1_RECALC:
			ALTBUF->e0 = 0.0;
			break;

/* ....  reset - reset buffer components - INTERNAL USE ONLY */
		case B1_RESET:
			 ibf->npt  = 0;						/*  This guy now empty! */
			*ibf->filename = '\0';
			*ibf->id   = '\0';
			*ibf->date = '\0';
			*ibf->ltct = '\0';
			 ibf->dirty  = FALSE;
			 ibf->modify = FALSE;
			 ibf->iddone = FALSE;
			 break;

/* .... CLEAR - Set all counts to zero  */
		case B1_CLEAR:
			for (i=0; i<CMAX; i++) ibf->counts[i] = 0;
			ibf->npt = CMAX;
			break;

/* .... Default - Set default conditions */
		case B1_DEFAULT: 
			ibf->e0     = 3.0;
			ibf->q      = 1.0;
			ibf->kevch = 4.95f;
			ibf->kev0 = 1.60f;
			ibf->phi    = 9.00;
			ibf->theta  = 7.00;
			ibf->omega  = 3.40f;
			ibf->corr   = 1.00;
			ibf->first  = 0.0;
			ibf->geom   = CORNELL;
			ibf->mbeam  = 4.00150586f;	/*  These three */
			ibf->zbeam  = 2;				/*  lines replace */
			ibf->cbeam  = 2;				/*  old "partcl" */
			ibf->fwhm   = 20.0;
			ibf->current = 0.0;
			break;

		default:
			TTYprintf("Unknown key to RbsBuffOne: %d\n",key);
			rcode = FALSE;
			break;
	}

	if (! rcode) LexFlush();
	return(rcode);
}


/* ===========================================================================
--  Usage Guide:
--      BOOL FUNCTION BFPROC(KEY)
--     Function BFPROC does copy, add, subtract, and divide when the
--     key is 1, 2, 3, 4.  Does all of the prompting and returns
--     with the operation complete.
--  Quick: Sub-processor for copy, add, subtract, divide
--
--     INPUTS:   KEY      to operation: 1  Copy
--                                      2  Add
--                                      3  Subtract
--                                      4  Divide
--
--     OUTPUTS:  Changes made to RUMP common block.
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       BMANIP
--     CALLS:             GETBUF, CHKSIM
--
=========================================================================== */
static int RbsBuffProc(int key) {

	SPECTRUM *isrc, *idest;

	if (key != BF_DIV)   {						/* Most simple */
		if ( (isrc = RbsGetBuf("Source buffer? (MAIN)  ", MAINBUF)) == NULL ||
			 (idest = RbsGetBuf("Destination (ABORT)  ",   NULL))    == NULL)
			return(FALSE);

	} else {											/* Case of divide more difficult */
		if ( (isrc = RbsGetBuf("Random? (ALT) ",       ALTBUF))  == NULL ||
			 (idest = RbsGetBuf("Channeled? (MAIN)  ", MAINBUF)) == NULL)
			return(FALSE);
	}

/* Make sure any buffers we are using are up to date: */
/* If key is negative, means we are to bypass this check */
	if ( key>0 && (isrc==ALTBUF || (idest==ALTBUF && key!=BF_CPY)) )
		SimCheck(NULL);

	return(RbsBuffTwo(isrc, idest, abs(key)));	/* Pass the work to sub-process */
}

/* ===========================================================================
--  Usage Guide:
--      BOOL FUNCTION BF$TWO(KEY)
--     Function BF$TWO does copy, add, subtract, and divide when the
--     key is 1, 2, 3, 4.  Lower level slave for BFPROC.
--     Keys are the same as BFPROC.
--  Quick: Slave processor for copy, add, subtract, divide
--
--     INPUTS:   KEY      to operation (see BFPROC)
--
--     OUTPUTS:  Changes made to RUMP common block.
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       BFPROC
--     CALLS:
=========================================================================== */
static int RbsBuffTwo( SPECTRUM *isrc, SPECTRUM *idest, int key) {

	int i, j, k, l, ist, i_end;
	REAL a, b, x, x1, x2, x3, ratio, du, deltac, scal;

/*  ... Copy Processing */
	switch (key) {
		case BF_CPY:
			RbsCopySpectrum(idest, isrc);
			break;

/*  ...  Add/Subtract Processing */
		case BF_ADD:
		case BF_SUB:
			x1 = 1.0;									/*  Sign of add direction	*/
			if (key == BF_SUB) x1 = -1.0;			/*  Subtract operation		*/
			idest->dirty = TRUE;						/*  Dest buffer is dirty	*/

			if (Rmp->raw) {							/*  Raw mode					*/
				a = 1.0;
				b = x1;
				x = idest->q/idest->corr + isrc->q/isrc->corr;
				idest->corr = 1.0;
				if (x != 0) idest->corr = (idest->q + isrc->q)/x;
				idest->q = idest->q + x1*isrc->q;
			} else {										/*  Normalized mode */
				a = 1.0;
				b = x1 * RbsNormK(isrc) / RbsNormK(idest);
			}

/*  The following code reduces to a simple */
/*      COUNTS(I,IDEST) = A * COUNTS(I,IDEST) + B * COUNTS(I,ISRC) */
/*  in the case when they have the same  kevch,  kev0, and FIRST. */

			i_end = idest->npt;
			for (i=0; i<idest->npt; i++)
				idest->counts[i] = a * idest->counts[i];

			ratio = idest->kevch/isrc->kevch;		/*  Ratio of window sizes */
			du = (idest->kev0-isrc->kev0+idest->kevch*(idest->first-1))
				  /isrc->kevch - isrc->first + 1;			/*  Offset between indices */
			ist = max(1, (int) ((1.0-du)/ratio) );
			if (i_end <= ist)   {						/*  Some kind of kinky ERROR */
				gen_err("BIZARRE indeed? (ADD)");
				return (FALSE);
			}

			x2 = ist*ratio + du;							/*  Alt Index at begin of IST */
			x = (REAL) isrc->npt;						/*  Ending point for loop */
			for (i=ist; i<=i_end; i++) {
				x3 = (i+1)*ratio + du;	/*  Alt index at end */
				if (x3 >= x) x3 = x;	/*  Don't run out of data */
				k = (int) x3;
				deltac = 0.0;
				j = (int) x2;

				while (j < k) {					/*  Loop if more than 1 fit */
					deltac = deltac + (j + 1 - x2) * isrc->counts[j];
					j = j + 1;
					x2 = (REAL) j;
				}
				idest->counts[i] = idest->counts[i] + b*(deltac +
					(x3-x2) * isrc->counts[j]);
				x2 = x3;
				if (x2 >= x) break;				/*  Out of alt data points */
			}
			break;
 
/*  Divide processing - start by fixing up the scaling */
		case BF_DIV:
			scal = Rmp->raw ? 1.0f : RbsNormK(idest) / RbsNormK(isrc) ;

			idest->dirty = TRUE;
			idest->corr  = 1.0;
			idest->omega = 1.0;
			idest->q     = 1.0;

			j = (int) (idest->first - isrc->first);	/*  Data point offset */
			k = idest->npt;
			l = idest->npt;
			for (i=0; i<k; i++) {					/*  Loop through DEST points	*/
				if ( (i+j < 0) ||						/*  Before start of ISRC?		*/
					  (i+j > l) ||						/*  More points than ISRC?		*/
					  (isrc->counts[i+j] == 0)) {	/*  Avoid zero divide			*/
					idest->counts[i] = 0.0;
				} else {
					idest->counts[i] = scal*idest->counts[i] / isrc->counts[i+j];
				}
			}
			break;
	}

	return(TRUE);
}


/* ===========================================================================
--  Usage Guide:
--      REAL FUNCTION NORMK(IBUF)
--
--     INPUTS:   IBUF     Buffer number
--
--     OUTPUTS:  NORMK    Function value is the normalization constant for
--                        the buffer.  Normalized Yield = NORMK * Raw Counts
--                        Normalized yield has units #/uC/msr/keV
--                        Divided out are beam dose, detector solid angle,
--                        and MCA channel width.
--  Quick: Returns spectrum normalization constant
--
--     CALLS:             None
--
=========================================================================== */
REAL RbsNormK(SPECTRUM *ibf) {

	REAL x;

	x = ibf->omega * ibf->q * ibf->kevch;
	if (ibf->cbeam != 0 && x != 0.0 && ibf->corr != 0.0) {
		x = ibf->cbeam * ibf->corr / x;
	} else {
		gen_warn("Couldn't normalize buffer");
		x = 1.0;
	}
	return(x);
}

/* ===========================================================================
=========================================================================== */
static void RbsBuffList(void) {

	int i, j;
	char wrkbuf[10], flags[3], *fname;

	TTYputs("* => Main    ? => Dirty    @ => Parameters changed\n");

	for (i=0; i<=RbsNumBuf; i++) {							/*  Include SIM buffer */
		if (RbsBuffers[i] == NULL) continue;
		if (RbsBuffers[i]->npt==0 && (i!=0||Rmp->autsim==0)) continue;	/*  Skip blanks */
		strcpy(flags, "  ");
		strcpy(wrkbuf, (i==1) ? " Main   " : ((i==0) ? " Theory " : " Buffer ") );
		if (RbsBuffers[i] == ibuf) wrkbuf[0] = '*';
		if (RbsBuffers[i]->dirty)  flags[0] = '?';
		if (RbsBuffers[i]->modify) flags[1] = '@';
		j = (int) strlen(RbsBuffers[i]->filename);
		fname = RbsBuffers[i]->filename + max(0,j-20);
		TTYprintf(" %s%2d %s %20s  %s\n",wrkbuf,i,flags,fname,RbsBuffers[i]->id);
	}
	return;
}

/* ===========================================================================
Performs a lookup of the SPECTRUM * argument, returning the index
of buffers which corresponds.  Returns BUFF_UNKNOWN if none found.
Useful for telling the user which buffer he/she is working with,
or figuring out how to scroll things around.
=========================================================================== */
int RbsGetBufNum(SPECTRUM *buff) {
	int i;
	for (i=0; i<=RbsNumBuf; i++) {
		if (RbsBuffers[i] == buff) return(i);
	}
	return(BUFF_UNKNOWN);
}
