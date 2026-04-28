/*  ANLYTC.F77 */

/* ------------------------------------------------------------------------
   --------                                              ------------------
   -------- COPYRIGHT 1989 (c) Computer Graphics Service ------------------
   --------                                              ------------------
   --------    The source code to RUMP may be freely     ------------------
   --------  modified as long as this copyright notice   ------------------
   --------          is included and unchanged.          ------------------
   ------------------------------------------------------------------------ */

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
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "rump.h"
#include "tplot.h"
#include "stopping.h"
#include "../genplot/gptxtrn.h"			/* For FitPolynomial and FFTSmooth */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

typedef enum _OPCODE {
 AN_CURSOR, AN_ELEMENT, AN_MATRIX, AN_WHATISIT, AN_INFO, AN_INTEGRAL,
 AN_THICKNES, AN_BACKGROU, AN_SMOOTH, AN_WIDTH_TH, AN_PROFILE, AN_INTSET,
 AN_CALIBR, AN_DISPLAY, AN_FFT
} OPCODE;

typedef struct _CMTYPE {
	char *name;
	int  minlen;
	OPCODE rcode;
} CMTYPE;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int  RbsQueryElement( SPECTRUM *ibf, CHEMIC *target);

static int RbsSmooth_SV(SPECTRUM *ibf, int low, int high);
static int RbsSmooth_Conv(SPECTRUM *ibf, int low, int high);
static int RbsSmooth_FFT(SPECTRUM *ibf, int low, int high, REAL width);

static void RbsLocate( REAL einc, SPECTRUM *ibf);
static void RbsThickn( int ourkey, CHEMIC *target);
static void RbsIntegrate( REAL y0, REAL y1, REAL dx, REAL *y, REAL *gross);
static int  RbsBackground( SPECTRUM *ibf, int n, REAL x0, REAL x1, REAL x2,
								  REAL x3, int opts);
static int  RbsSetexp( SPECTRUM *ibf, CHEMIC *target);
static REAL RbsKappa( CHEMIC *target);
static REAL RbsSigma( REAL energy, CHEMIC *target);
static REAL RbsStoper( REAL energy, CHEMIC *target);
static REAL RbsEpsilon( REAL e0, CHEMIC *target);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Global Variables 					  */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static CMTYPE cmlist[] = {
	{"cursor",			3,	AN_CURSOR},
	{"element",			2,	AN_ELEMENT},
	{"matrix",			3,	AN_MATRIX},
	{"whatisit",		4,	AN_WHATISIT},
	{"info",				3,	AN_INFO},
	{"integral",		3,	AN_INTEGRAL},
	{"thickness",		4,	AN_THICKNES},
	{"background",		4,	AN_BACKGROU},
	{"smooth",			3,	AN_SMOOTH},
	{"width_thick",	3,	AN_WIDTH_TH},
	{"profile",			3,	AN_PROFILE},
	{"intset",			6,	AN_INTSET},
	{"calibrate",		3,	AN_CALIBR},
	{"display",			4,	AN_DISPLAY},
	{"fft",				3,	AN_FFT},
	{NULL,				0,	AN_FFT}
};


/*      BOOL FUNCTION ANLYTC(FKEY, TOKE) */
/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION ANLYTC(KEY, TOKEN)
--
--  Quick: Performs RBS specific analysis commands
--     ANLYTC performs all the analytic junk which is RBS specific.
--     It is one of the five command processors called by RUMP itself.
--
--     INPUTS:   KEY    Chooses the operation:  0 = Process command
--                                             -1 = Initialize
--                                             -2 = Reset
--                                             -3 = List Commands
--                                             -4 = Display Parameters
--                                             -5 = Turn Off
--               TOKEN  is the command (Character string) for the case KEY = 0
--
--     OUTPUTS:  LOG    (Function value) True only for Key = 0 and the command
--                      was found and action was attempted.
--
--     COMMON BLOCKS:     RUMP, CHEMIC
--     CALLED FROM:       RUMP
--     CALLS:             NEWPRF
--                        SMOOTH, BACKGR, LOCATE, THICKN,
--                        STOPER, KAPPA, SIGMA, EPSILO, IDENT, LOAD,
--                        PLDATA, RESET, MARK, GETMEV, FMTENG, PLSUPP
=========================================================================== */
int RbsAnlytc(int fkey, char *cmd_token) {

/*  -- Local Variables -- */
	int   curch;							/*  Cursor return character */
	char *enstr;							/*  Formatting routine result */
	int   i, key;
	BOOL SawOpt;
	REAL x, x1, x2, x3, x4, height,
			alpha, chanl, xcurs, ycurs, ener;
	char token[DFLT_STR_SIZE], elem_list[DFLT_STR_SIZE];
	char *tokptr;
	static CHEMIC target;

/*  Calibrate routine variables: */
	REAL ch1,ch2;							/*  Two channel numbers */
	REAL kh1,kh2;							/*  Two K- factors */
	REAL cmark, emark;					/*  Channel + Energy of known signal */
	CMTYPE *cmd;
	
	if (fkey < 0) {
		switch (fkey) {
			case U_RESET: 
			case U_INIT:
				break;

			case U_HELP:
				LexCmdlPrint (cmlist, sizeof(CMTYPE), "RBS analysis:");
				break;

			case U_PARM:
			case U_QUIT:
			default:
				break;
		}
		return(FALSE);

	} else if (fkey > 0) {
		gen_err("Unknown key to ANLYTC");
		return(FALSE);
	}

/* ... Not a special key, so process as a command - if possible */
	if ( (cmd = LexCmdl(cmd_token, cmlist, sizeof(CMTYPE))) == NULL)
		return(FALSE);

	switch (cmd->rcode) {

/* ... CURSOR - Display cursor on tektronix and return coordinates */
		case AN_CURSOR: 
			if (! PlotSystem(3," ",0)) {
				gen_err("Cursor not enabled or illegal device");
				return (TRUE);
			}
			TTYputsnl("Cursor on. Z (space) recycles. Others return.");

			do {
				RbsCursor(&xcurs, &ycurs, &curch);		/*  High level cursor */
				x1 = RBSCNNLE(xcurs, ibuf);
				enstr = ToEngFormat(xcurs*1.0e6f);
				if (Rmp->raw) {
					TTYprintf(" Channel: %6.1f    Energy: %seV    Counts:%12.3f\n",
						x1,enstr,ycurs);
				} else {
					TTYprintf(" Channel: %6.1f    Energy: %seV    Yield:%10.4f /uC/keV/msr\n",
						x1,enstr,ycurs);
				}
			} while ((curch == '0') || (curch == ' '));
			return (TRUE);

/* ... ELEMENT - Identifies channel and energy of elemental surface peak
                 must know incident energy and channel calibration to work */
		case AN_ELEMENT:
			height = 0.0f;							/* Default position */

			for (SawOpt=FALSE, i=0; ! SawOpt && i < 2; i++) {
				if (LexGetOption(token, sizeof(token))) {
/*					if (LexEqual(token, "-position", 4) || LexEqual(token, "-height", 2)
						|| LexEqual(token, "-posn", 5)) { */
					height = LexGetReal(-987.125f, "Height (counts) of marker (CURSOR): ");
					if (height == -987.125f) {
						RbsCursor(&x1, &height, &curch);
						if (curch == 0x1B) goto nomore;
						TTYprintf("height returned as: %f\n", height);
					}
					if (Rmp->raw) height=height*RbsNormK(ibuf);		/* Turn to real counts */
					SawOpt = TRUE;
				}

				if (i == 0) {											/* Only first time */
					if (! LexGetListP(elem_list, sizeof(elem_list), "Element(s)? (abort) ")) return(TRUE);
				}
			}

			tokptr = elem_list;
			while (LexParseLine(token, sizeof(token), tokptr, &tokptr)) {
				LexInsText(token);
				if (! RbsQueryElement(ibuf, &target) ) return(TRUE);
				x1 = RbsKappa(&target);   /*  Get scattering factor */
				if (x1 == 0)   {
					ERRprintf("ERROR: Scattering event cannot occur for this particle\n");
					continue;
				}
				ener = x1 * ibuf->e0;				/*  Real energy */
				chanl = RBSCNNLE(ener,ibuf);		/*  Channel number */

				if (target.isotmp == 0)   {
					strcpy (token, target.symbol);
				} else {
					sprintf(token, "^{%d}%s", target.isotmp, target.symbol);
				}
				enstr = ToEngFormat(ener*1.0e6f);
				TTYprintf(
							 " %2s  Z=%2d  Mass=%7.3f  K(ion)=%6.4f  Energy=%8seV  Channel=%8.3f\n",
							 target.symbol,target.z,target.weight,x1,enstr,chanl);
			
				if (PlotSystem(2," ",0)) RbsMark(MK_TKL, ener, height, token);
			}
			return(TRUE);

/* ... MATRIX - Calculates both the ener and height at which a matrix
                should exist.  Good for calibration of system. */
		case AN_MATRIX:
			if (! RbsQueryElement(ibuf, &target) ) goto nomore;

			x1 = RbsSigma(ibuf->e0, &target);
			if (x1 == 0)   {
				ERRprintf("ERROR: Scattering event cannot occur\n");
				return (TRUE);
			}
			height = x1 * 6.25e12f / target.cosin / RbsEpsilon(ibuf->e0, &target);
			ener  = ibuf->e0 * RbsKappa(&target);	/*  Get energy scattering	*/
			chanl = RBSCNNLE(ener,ibuf);				/*  Also channel number		*/
			
			enstr = ToEngFormat(ener*1.0e6f);
			TTYprintf(" %2s expected at %8seV (%6.1f) and height %8.3f\n",
				target.symbol,enstr,chanl,height);
			
			if (PlotSystem(2," ",0)) RbsMark(MK_XHR,ener,height,target.symbol);
			return (TRUE);

/* ...  WHATISIT - Identify the elements nearest specified channel */
		case AN_WHATISIT:
			key = 1;
			x = RbsGetMeV(&key, 0.0f, "Identify location with cursor.",
				"Search near what channel? (ABORT) ");
			if (x == RBSENERGY(0.0, ibuf))   {
				gen_warn("WHATISIT aborted by command");
			} else {
				RbsLocate(x, ibuf);
			}
			return (TRUE);

/* ...  INFO - Lists information about an element (density, number, isotop */
		case AN_INFO:
			if (! RbsQueryElement(ibuf, &target) ) goto nomore;
			
			x1 = target.densit / 6.022e23f * target.weight; /*  g/cc density */
			enstr = ToEngFormat(ibuf->e0*1.00e6f);
			TTYputs("\n-----------------------------------------------------\n");

			TTYprintf(
				"%2s  Z: %2d  Mass: %6.2f  Density: %11.4e at/cc (%5.2f g/cc)\n\n",
				target.symbol,target.z,target.weight,target.densit,x1);
			TTYprintf(
				"Parameters: Energy %8seV  Theta%6.2f     Phi%7.2f\n",
				enstr, ibuf->theta, ibuf->phi);
			TTYprintf(
				"            keV/channel%6.3f     keV(0)%8.3f\n\n",
				ibuf->kevch, ibuf->kev0);

			x1 = RbsKappa(&target);  /*  Scattering parameters */
			if (x1 == 0) {
				ERRprintf("ERROR: Scattering event cannot occur\n");
			} else {
				ener  = x1 * ibuf->e0;
				chanl = RBSCNNLE(ener, ibuf);
				enstr = ToEngFormat(ener*1.00e6f);
				TTYprintf(
					"Surface Scattering:          %6.4f at %8seV (Channel: %5.1f)\n",
					x1,enstr,chanl);
				x2 = RbsSigma(ibuf->e0, &target);
				x3 = RbsEpsilon(ibuf->e0, &target);
				height = x2 * 6.25e12f / target.cosin / x3;
				TTYprintf(
					"Matrix scattering height:    %6.2f Counts/uC/keV/msr\n",
					height);
				
				x2 = 1.0e24f * x2;
				TTYprintf(
					"Scattering cross section:    %7.3f (1E-24 cm2/ster)\n", 
					x2);
				
				x3 = 1.0e15f * x3;
				x1 = x3 * target.densit * 1.0e-23f;   /*  get eV/Angstrom */
				TTYprintf(
					"Stopping Factors:            [e] = %5.1f (1E-15 eV-cm2)   [S] = %5.1f eV/A\n",
					x3,x1);
			}
			TTYputs("\n");

			for (i=0; i < NISOT ; i++ ) {
				if (target.isoto[i].mass <= 0.0) break;
				TTYprintf("%s  Mass: %6.2f  Abundance: %7.5f\n",
					(i==0)?"Isotopes:":"         ",
					target.isoto[i].mass,target.isoto[i].fraction);
			}
			TTYputs("\n");

			return (TRUE);

/* ...  INTEGRAL - Cursor is used to define a region.  The total counts
                   in the region is returned to the user. */
		case AN_INTEGRAL:
			RbsThickn(TH_INT, &target);
			return (TRUE);

/* ...  THICKNESS - Integral plus conversion to physical units */
		case AN_THICKNES:
			RbsThickn(TH_THK, &target);
			return (TRUE);

/* ...  BACKGROUND - Fit and subtract off background */

		case AN_BACKGROU:
		{
			struct {
				char *token;
				int minlen;
				int key;
			} options[] = {{"-full", 3, 0x01},		{"-inplace", 4, 0x08},	{"-replace", 3, 0x08},
								{"-noplot", 4, 0x02},	{"-info",    4, 0x04},		{NULL, 0, 0}};
			int optkey;
			static char MyHelp[]=
				"\n"
				" Strip off a polynomial fit as approximation to background signal.\n"
				" The region of the polynomial fit is specified either using the cursor\n"
				" or entering the values directly.  Generally a 1st or 2nd order fit is\n"
				" all that is experimentally valid.  Several options control the spectra\n"
				" that result.  Options may be specified either before or after the\n"
				" channel values.\n"
				"\n"
				"   BACKGROUND [-options] { {/ (cursor) | 4 channel values} <order> } [-options]\n"
				"\n"
				" Options:\n"
				"   -help | -?   Prints this help message\n"
				"\n"
				"   -noplot      Don't bother plotting the stripped curve\n"
				"   -full        Leave portion of spectrum not stripped in final buffer also\n"
				"   -info        Print out the parameters of the fit\n"
				"   -inplace     Do not duplicate the buffer before stripping background\n"
				"\n"
				" The default is to create a new buffer containing only the segment of the\n"
				" spectrum that has been stripped, with the name .cut.  The fit curve, and\n"
				" all data points outside the fitting region, are put in the temp (-1) buffer.\n"
				"\n";

			if (LexCheckHelp("Background", MyHelp, NULL)) return(TRUE);
			
			if (! LexChkToken(token, sizeof(token))) {
				TTYputs("Background subtract requires two regions, one on either side of\n"
						  "peak to be studied.  Enter 4 points to identify those regions.\n");
			}

			optkey = 0;									/* Assume no options */
			while (LexGetOption(token, sizeof(token))) {
				for (i=0; options[i].token != NULL; i++) { if (LexEqual(token, options[i].token, options[i].minlen)) break; }
				if (options[i].token == NULL) {
					ERRprintf("ERROR: %s unrecognized option to background command.  Use -? for command format.\n", token);
					goto nomore;
				}
				optkey |= options[i].key;
			}

			x = LexGetReal(-987.125f, "Lower/upper points of regions (cursor): ");
			if (x == -987.125f) {
				key = 1;
				x  = RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL,
					"Lower and upper channels of first region? "), ibuf);
				x1 = RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL,
					"Upper channel of first region? "), ibuf);
				x2 = RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL,
					"Lower and upper channels of second region? "), ibuf);
				x3 = RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL,
					"Upper channel of second region? "), ibuf);
				TTYprintf("  Background fit range: %f %f and %f %f\n", x,x1,x2,x3);
			} else {
				x1 = LexGetReal(200.0f, "Upper channel of  first region (200): ");
				x2 = LexGetReal(400.0f, "Lower channel of second region (400): ");
				x3 = LexGetReal(600.0f, "Upper channel of second region (600): ");
			}

			do {
				OrderPair(&x, &x1);
			} while (OrderPair(&x1, &x2) || OrderPair(&x2, &x3));
			
			i = LexGetInt(2,"Order of the polynomial fit? (2) (0 ABORTS)  ");
			if (i <= 0)   {
				return (TRUE);
			} else if (i > 8)   {
				ERRprintf("ERROR: Fit order must be 8 or less\n");
				goto nomore;
			}

			/* And continue to scan for post-options */
			while (LexGetOption(token, sizeof(token))) {
				for (i=0; options[i].token != NULL; i++) { if (LexEqual(token, options[i].token, options[i].minlen)) break; }
				if (options[i].token == NULL) {
					ERRprintf("ERROR: %s unrecognized option to background command.  Use -? for command format.\n", token);
					goto nomore;
				}
				optkey |= options[i].key;
			}

			if (RbsBackground(ibuf,i, x,x1,x2,x3, optkey)) return(TRUE);
			goto nomore;
		}

/* ---------------------------------------------------------------------------
-- SMOOTH
--   -SV (or default): 5 pt Savitsky-Goulay smooth.  See tracor manuals
--                     xn = (-3x(n-2)+12x(n-1)+17x(n)+12x(n+1)-3x(n+2))/35
--   -CONVOLUTION:     Edge conserving convolution smooth
--   -FFT:             FFT convolution algorithm
--------------------------------------------------------------------------- */
		case AN_SMOOTH:
			{
			int mode, low, high, c1,c2;
			REAL width;
			low  = 0;							/* Starting channel		*/
			high = ibuf->npt-1;				/* Number of points		*/
			mode = 1;							/* Default is SV mode	*/

			while (LexGetOption(token, sizeof(token))) {
				i = LexSelect(token, "-SV -convolute -convolution -fft -range");
				if (i == 1) {							/* Savitsky-Goulay smooth */
					mode = 1;
				} else if (i == 2 || i == 3) {	/* Use convolution algorithm */
					mode = 2;
				} else if (i == 4) {					/* Use FFT algorithm */
					mode = 3;
				} else if (i == 5) {					/* Get limiting range */
					key = 1;
					c1 = nint(LexGetReal(-987.0f, "Channels of smoothing region (cursor): "));
					if (c1 == -987) {
						c1 = nint(RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL, "Lower channel? "), ibuf));
						c2 = nint(RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL, "Upper channel? "), ibuf));
					} else {
						c2 = nint(LexGetReal(600.0f, "Upper channel: "));
					}
					low  = min(c1,c2) - (int) ibuf->first;
					high = max(c1,c2) - (int) ibuf->first;
					low  = max(0, min(low,  ibuf->npt-1));
					high = max(0, min(high, ibuf->npt-1));
				} else {
					ERRprintf("ERROR: Unrecognized or illegal smoothing option (%s)\n", token);
					goto nomore;
				}
			}
			if (high - low < 4) {
				ERRprintf("ERROR: Too few points for smoothing - aborting\n");
				goto nomore;
			}
			if (mode == 1) {
				if (RbsSmooth_SV(ibuf, low, high) != 0) goto nomore;
			} else if (mode == 2) {
				if (RbsSmooth_Conv(ibuf, low, high) != 0) goto nomore;
			} else if (mode == 3) {
				width = LexGetReal(2.0f, "Smoothing width in channels (0 aborts): ");
				if (width <= 0.0) goto nomore;
				if (RbsSmooth_FFT(ibuf, low, high, width) != 0) goto nomore;
			}
			return(TRUE);
			}

/* ...  WIDTH_THICK - Calculate the thickness via width of a peak.  
                      Uses the mean-energy approximation. */
		case AN_WIDTH_TH:
			key = 1;
			x1 = RbsGetMeV(&key,100.0f,"Place cursor at half-height points.",
				"Enter channel numbers of half-height: ");
			x2 = RbsGetMeV(&key,600.0f,NULL,"Upper channel: ");
			x1 = (REAL) fabs(x2-x1);

			if (! RbsQueryElement(ibuf, &target)) goto nomore;

/*  ... Use the method of ENERGY LOSS RATIO (CHU et al. pg. 65) */
			x2 = RbsKappa(&target);   /*  Kinematic factor */
			if (x2 == 0)   {
				ERRprintf("ERROR: Scattering event cannot occur\n");
				return (TRUE);
			}
			alpha = RbsStoper(x2*ibuf->e0, &target)/RbsStoper(ibuf->e0,&target)*
				target.cosin/target.cosout;

/*  ... Alpha (3.21) */
/*  ... E1 = KAPPA*E0 - WIDTH */
/*  ... E(scat) = E0 - WIDTH/(KAPPA+ALPHA) */
/*  ... E(inc) = E0 - 0.5*WIDTH/(KAPPA+ALPHA) */
/*  ... E(out) = KAPPA*E0 - 0.5*WIDTH*[ALPHA/(KAPPA+ALPHA)] */
			x3 = ibuf->e0 - 0.5f*x1/(x2+alpha);     /*  Mean incident */
			x4 = x2*ibuf->e0 - 0.5f*x1*alpha/(x2+alpha);    /*  Mean exit */
			x4 = x2*RbsStoper(x3,&target)/target.cosin + RbsStoper(x4,&target)/target.cosout;
      /*  Mean stop. power */
			x4 = x4 * 1.0e15f;      /*  1E-15 eV-cm2 */
			x3 = x4 * target.densit * 1.0e-23f;     /*  eV/A */
        /*  Output stopping */
			TTYprintf("Scattering cross section:    %7.3f (1E-24 cm2/ster)\n",x4);
			TTYprintf("                             %7.3f (eV/A)\n",x3);

			x1 = x1 * 1000.0f;      /*  Go to keV */
			x2 = x1 / x4 * 1.0e18f; /*  Atoms/cm2 */
			x3 = x1 / x3 * 1.0e03f; /*  Angstroms */
			enstr = ToEngFormat(x1*1.00e3f);
			TTYprintf("Width: %seV   %11.4e Atoms/cm**2   %8.1f Angstroms\n",
						 enstr,x2,x3);
			return(TRUE);

		case AN_PROFILE:
			if (! RbsNewprf()) goto nomore;
			return(TRUE);

/* ... INTSET - Change settings of the integration routines */
		case AN_INTSET:
			RbsThickn(TH_SET, &target);
			return (TRUE);

/* ... CALIBR - Energy Calibration of the Spectrum */
		case AN_CALIBR:
			key = 1;

/*  .. First element */
			ch1 = RBSCNNLE(RbsGetMeV(&key, 0.0f,
				"Mark two surface element positions with the cursor:",
				"First element position (channel): "), ibuf);
			ch2 = RBSCNNLE(RbsGetMeV(&key, 0.0f, NULL,
				"Second element position (channel): "), ibuf);
			if (ch2 == 0) goto nomore;
			if (fabs(ch2-ch1) < 2) {
				ERRprintf("ERROR: Did you hear me -- I said two DIFFERENT positions ...\n");
				goto nomore;
			}

			if (  ! RbsQueryElement(ibuf, &target) ) goto nomore;
			kh1 = RbsKappa(&target);
			if (  ! RbsQueryElement(ibuf, &target) ) goto nomore;
			kh2 = RbsKappa(&target);
			if (fabs(kh1-kh2) < 0.001) {
				ERRprintf("ERROR: You don't understand - calibration needs 2 DIFFERENT elements.\n");
				goto nomore;
			}

			x3  = (kh1-kh2)/(ch1-ch2);      /*  Used a couple of places */
			
/*  .. Marker and Beam Energy calibration */
			emark = LexGetReal(4684.0f, "Marker energy in keV (4684 = 230Th): ");
			if (fabs(emark) > 10)   {        /*  Otherwise leave E0 unchanged */
				cmark = RBSCNNLE(RbsGetMeV(&key, 0.0f,
					"Identify marker position with cursor:",
					"Enter marker channel: "), ibuf);
				ibuf->e0 =  0.001f * emark / (kh2+x3*(cmark-ch2));
			} else {
				TTYputs("Marker not specified - cannot calibrate beam energy.\n");
			}

/*  .. ADC calibration */
			ibuf->kevch = 1000.0f *  x3*ibuf->e0;
			ibuf->kev0 = 1000.0f * kh1*ibuf->e0 - ch1*ibuf->kevch;
			
/*  .. Print the result */
/* nergy= 2.950 MeV    Conversion:1234.6789 keV/ch  12345.78 keV(0) */
			enstr = ToEngFormat(ibuf->e0*1.0e6f);
			TTYprintf(" Energy=%8seV    Conversion:%.4f keV/ch   %.4f keV(0)\n",
				enstr, ibuf->kevch, ibuf->kev0);

			ibuf->modify = TRUE;
			return (TRUE);


/* ... DISPLAY - Graphical equivalent to "SHOW" in SIM */
		case AN_DISPLAY:
			SimDrawSample();
			return (TRUE);

/* ... FFT Fast Fourier Transform */
		case AN_FFT:
			LexInsText("SMOOTH -FFT -RANGE");	/* Just shift modes */
			return(TRUE);

	}
	return(FALSE);									/* Unknown command */

/* ************************** */
/*  Problem returns come here */
/* ************************** */
nomore:
	LexFlush();
	return(TRUE);
}


/* ===========================================================================
-- Usage: void RbsSetCorrByMatchCounts()
--
-- Inputs: (1) Channel numbers for integration (via keyboard or cursor)
--         (2) Options
--                -Buffer <num/name>  <== Match to buffer instead of SIM
--                -NoSet              <== Report value but don't set in place
--
-- Output: Prints change in correction factor necessary to match
--
-- Returns: nothing
--
-- Routine default takes current buffer and matches counts setting CORR factor
-- of current buffer so the two match.  Comparison buffer is by default SIM
-- but can be reset.
=========================================================================== */
void RbsSetCorrByMatchCounts(void) {

	BOOL SetValue = TRUE, Verbose=TRUE;
	char token[DFLT_STR_SIZE];
	SPECTRUM *match=NULL;

	int i, gmkey;
	REAL gross_ibuf, gross_match, factr;
	REAL xst, xend;				/*  Real channel numbers */
	int  ist, iend;	       	/*  Integer indices to COUNTS array */

/* Scan for options and the channel range */
	for (i=0; i<2; i++) {					/* Allow options begin/end */
		while (LexGetOption(token, sizeof(token))) {
			if (LexEqual(token, "-?", 2) || LexEqual(token, "-help", 2)) {
				TTYprintf("Usage: MATCH [options] <low chan> <high chan>\n\n"
							 "       Command sets correction of current buffer so that it matches the integral\n"
							 "       of the simulation over the range.  Cursor is used if low channel is /.\n\n"
							 "       Options, which may preceed or follow channel numbers include:\n"
							 "           -?                 this message\n"
							 "           -Buffer <num>      match to a buffer other than simulation\n"
                      "           -NOSet             just report value, don't set as new CORR factor\n"
							 "           -Silent            quiet mode\n\n");
				return;
			} else if (LexEqual(token, "-NOSET", 4)) {
				SetValue = FALSE;
			} else if (LexEqual(token, "-SET", 3)) {
				SetValue = TRUE;
			} else if (LexEqual(token, "-SILENT", 2)) {
				Verbose = FALSE;
			} else if (LexEqual(token, "-BUFFER", 2)) {
				RbsGetBuf("Buffer to match to (SIM): ", ALTBUF);
			} else {
				ERRprintf("ERROR: Invalid option to command.  Use -? for list\n");
				LexFlush();
				return;
			}
		}
		if (i == 0) {							/* Only first time */
			ist = LexGetInt(-1, "Lower channel (cursor): ");
			if (ist == -1) {
				gmkey = 3;
				xst  = RbsGetMeV(&gmkey, Rmp->chmin, "Identify region with cursor.  Press any key.",
									  "Lower limit of integral? ");
				xend = RbsGetMeV(&gmkey, Rmp->chmax, NULL, "Upper limit of integral?  ");
				xst  = RBSCNNLE(xst , ibuf);			/* Remains a real number */
				xend = RBSCNNLE(xend, ibuf);			/* Remains a real number */
				OrderPair (&xst, &xend);
				ist  = RBSINDEX(xst , ibuf);
				iend = RBSINDEX(xend, ibuf);
			} else {
				iend = LexGetInt(ist+50, "Upper channel (start+50): ");
			}
		}
			
	}

/* A few sanity checks and updates */
	if (match == NULL) match = ALTBUF;
	if (match == ALTBUF) SimCheck(NULL);
	ist  = max(ist, 0);
	iend = min(iend, ibuf->npt-1);
	iend = min(iend, match->npt-1);
	if (ist >= iend) {
		ERRprintf("ERROR: Invalid channel range specified - no data there\n");
		LexFlush();
		return;
	}

/* Get the gross integral */
	gross_ibuf = gross_match = 0.0f;
	for (i=ist; i<=iend; i++) {
		gross_ibuf  += ibuf->counts[i];
		gross_match += match->counts[i];
	}

/* Correct for scaling paramters */
	gross_ibuf  *= RbsNormK(ibuf)  * ibuf->kevch;
	gross_match *= RbsNormK(match) * match->kevch;
	if (gross_ibuf == 0 || gross_match == 0) {
		ERRprintf("ERROR: Current buffer (%f) or match buffer (%f) had no counts\n", gross_ibuf, gross_match);
		LexFlush();
		return;
	}

/* And now determine the necessary correction factor */
	factr = gross_match / gross_ibuf;
	if (Verbose) TTYprintf(
			"CORR factor was (must be) changed from %.2f to %.2f (%.2f ratio)\n",
			ibuf->corr, ibuf->corr*factr, factr);
	if (SetValue) {
		ibuf->corr *= factr;
		ibuf->modify = TRUE;
	}

	return;
}

/* ===========================================================================
--  Usage Guide:
--
--  SMOOTH_SV performs a 5 point smooth through the data
--  uses the Savitsky-Goulay algorithm.  See TRACOR data manuals
--    xn = (-3x(n-2)+12x(n-1)+17x(n)+12x(n+1)-3x(n+2))/35
--
--  Usage:  int RbsSmooth_SV(SPECTRUM *ibf, int low, int high);
--
--  Inputs: ibf   - Buffer to be smoothed
--          low   - lower index in count array to be smoothed (inclusive)
--          upper - upper index in count array to be smoothed (inclusive)
--
--  Outputs:  Modifies count buffer in ibf
--
--  Returns: 0 => successfully completed
--
-- As per note by Gyorgy, there are multiple ways to handle the endpoints
-- of the range.  Current implementation uses data outside the window if it
-- exists for initialization and termination.  Smoothing at edges is thus
-- the same as if a larger window had been chosen.
--
-- Other alternatives would be to use a different smoothing formula (one
-- sided) at the edges.  I think I like the current solution.
=========================================================================== */
static int RbsSmooth_SV(SPECTRUM *ibf, int low, int high) {

/*  -- Local Variables -- */
	REAL temp[5], *buf;
	int i,j;

/*  -- Code begin -- */
	buf = ibf->counts;
	if ( (high-low < 5) || (high >= ibf->npt) || (low < 0) ) return(-1);

#if 1										/* Version using data outside window */
	temp[0] = buf[max(0,low-2)];
	temp[1] = buf[max(0,low-1)];
	temp[2] = buf[low];
	temp[3] = buf[low+1];
	temp[4] = buf[low+2];

	for (i=low; i<=high; i++) {
		buf[i] = (-3*(temp[0]+temp[4])+12*(temp[1]+temp[3])+17*temp[2])/35;
#ifdef CSET2						/* BUG - buf[i] negative if this not done */
		if (i<low) printf("i: %d  buf[i]: %f\n", i,buf[i]);
#endif
		for (j=0; j<4; j++) temp[j] = temp[j+1];
		temp[4] = buf[min(i+3,ibf->npt-1)];
	}
#else										/* Version using constant extension */
	#error I do not really expect this to be compiled
	temp[0] = buf[low];
	temp[1] = buf[low];
	temp[2] = buf[low];
	temp[3] = buf[low+1];
	temp[4] = buf[low+2];

	for (i=low; i<=high; i++) {
		buf[i]=(-3*(temp[0]+temp[4])+12*(temp[1]+temp[3])+17*temp[2])/35;
		for (j=0; j<4; j++) temp[j] = temp[j+1];
		temp[4] = buf[min(i+3,high)];
	}
#endif
	
	ibf->dirty = TRUE;
	return(0);
}

/* ----------------------------------------------------------------------------
-- Convolution routine (Part of SMOOTH_CONV below)
--
-- Usage:  CALL CONV(F,H)
--
-- Inputs: 
---------------------------------------------------------------------------- */
#define	NDIM	25
static void conv(REAL *y, REAL *ynew, int npt, REAL *gauss, int iwidth) {

	int i,j,istore,inow,itail;
	REAL temp[NDIM];

/* ... Start the convolution: y(i) <-- y(i) + conv(y-h,gauss) */
	istore = iwidth;
	inow   = -1;
	itail  = NDIM-1;						/* Next position to save? */

	for (i=iwidth; i<npt-iwidth; i++) {
		if (inow == itail) {				/* End of space? */
			itail = (itail+1)%NDIM;
			ynew[istore] += temp[itail];
			istore++;
		}

		inow = (inow+1)%NDIM;
		temp[inow] = 0.0f;
		for (j=-iwidth; j<=iwidth; j++)
			temp[inow] += (y[i+j]-ynew[i+j]) * gauss[j+iwidth];
	}

	for (i=istore; i<npt-iwidth; i++) {
		itail = (itail+1)%NDIM;
		ynew[i] += temp[itail];
	}

	return;
}

/* ===========================================================================
--  Usage Guide:
--
--  SMOOTH_CONV does an edge preserving smooth on the data.
--
--  Usage:  void RbsSmooth_Conv(SPECTRUM *ibf, int low, int high);
--
--  Inputs: ibf   - Buffer to be smoothed
--          low   - lower index in count array to be smoothed
--          upper - upper index in count array to be smoothed
--
--  Outputs:  Modifies count buffer in ibf
--
--  Returns: nothing
=========================================================================== */
static int RbsSmooth_Conv(SPECTRUM *ibf, int low, int high) {

	REAL *data, *work, *gauss;
	REAL lasterror=1E37f, error, sum, sigma;
	int iwidth, nsmooth, i, icnt, npt;

	nsmooth = LexGetInt(2, "Number of smoothing steps (2): ");
	nsmooth = max(2, nsmooth);

/* Determine the "characteristic width" of the natural detector smoothing */
	sigma = (REAL) (ibf->fwhm/2 / sqrt(log(2.0f)) / ibf->kevch);	/* Channel sigma */
	if (sigma < 1 || sigma > 50) {
		ERRprintf("ERROR: FWHM/SCALE parameters for smooth are rediculous\n");
		return(-1);
	} else if (ibf->npt < 20) {
		ERRprintf("ERROR: Too few points for a convolution smooth\n");
		return(-2);
	}

/* ---------------------------------------------------------------------------
-- This next section creates the instrument function. It takes sigma from the
-- FWHM and kevch parameters and makes a Gauss array extending 3 sigmas
-- to the left and right.  Important: Area normalized to assure conservation.
---------------------------------------------------------------------------- */
	npt    = ibf->npt;
	iwidth = nint(3*sigma);								/* Range of gaussian */
	gauss  = malloc((2*iwidth+1) * sizeof(*gauss));

	for (sum=0.0f,i=0; i<2*iwidth+1; i++) {
		gauss[i] = (REAL) exp(-pow( (i-iwidth)/sigma, 2));
		sum += gauss[i];
	}
	for (i=0; i<2*iwidth+1; i++) gauss[i] /= sum;

	work = malloc(ibf->npt * sizeof(*work));		/* Temporary buffer */
	data = malloc(ibf->npt * sizeof(*data));		/* Temporary result */
	for (i=0; i<npt; i++) {
		work[i] = ibf->counts[i];						/* Duplicate data */
		data[i] = 0.0f;										/* And zero result */
	}

	TTYputs(" RMS: ");
	for (icnt=1; icnt<=nsmooth; icnt++) {
		conv(work, data, npt, gauss, iwidth);
		for (error=0.0f,i=iwidth; i<npt-iwidth; i++)
			error += (REAL) pow(work[i]-data[i], 2);
		error = (REAL) sqrt(error) / (npt-2*iwidth);		/* Chi error */
		if (icnt != 1 && icnt%10 == 1) TTYputs("\n      ");
		TTYprintf("%6.4f ", error);
		if (lasterror <= error) break;
		lasterror = error;
	}
	TTYputs("\n");

	high = min(high, ibf->npt-1);
	for (i=low; i<=high; i++) ibf->counts[i] = data[i];
	ibf->dirty = TRUE;

	free(work);
	free(data);
	free(gauss);
	return(0);
}


/* ===========================================================================
-- Subroutine to smooth the contents of IBF by an FFT method between two
-- limiting points.
--
-- Usage:  call smofft(ibuf,n1,n2,val)
--
-- Inputs: ibuf - buffer to smooth
--         n1   - lower limit of range to smooth
--	        n2   - upper limit of range to smooth
--         val  - smoothing parameter (basically halfwidth)
--
-- Output: counts(ibuf) - modified
=========================================================================== */
static int RbsSmooth_FFT(SPECTRUM *ibf, int low, int high, REAL width) {

	REAL *buf;
	int i, npt;

	npt = high - low + 1;
	if ( (low < 0) || (high >= ibf->npt) || (high-low < 7) ) {
		ERRprintf("ERROR: Too few points for a FFT smooth\n");
		return(-1);
	}

	buf = malloc(2*npt*sizeof(*buf));		/* Leave space to expand */
	for (i=low; i<=high; i++) buf[i-low] = ibf->counts[i];

	FFTSmooth(buf, npt, 2*npt, width);		/* Use GENPLOT smooth algorithm */

	for (i=low; i<=high; i++) ibf->counts[i] = buf[i-low];
	ibf->dirty = TRUE;

	free(buf);
	return(0);

}

/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION BACKGR(IBF,N,X0,X1,X2,X3)
--
--     SUBROUTINE TO FIT AND SUBTRACT OUT A BACKGROUND CHARACTER.
--  Quick: Fits and subtracts out background
--
--     INPUTS:   IBF      BUFFER NUMBER TO WORK ON
--               NFIT     ORDER OF THE POLYNOMIAL FIT (<=5)
--               X0,X1    REGION TO LEFT OF IMPORTANT REGION
--               X2,X3    REGION TO RIGHT OR IMPORTANT REGION
--
--     OUTPUTS:  THE MAIN MEMORY ARRAY WILL BE MODIFIED WITH THE FITTED
--               BACKGROUND SUBTRACTED OUT.  IF A PLOTTER IS ENABLED, THE
--               FITTED CURVE AND THE RESIDUAL WILL BE PLOTTED.
--
--     COMMON BLOCKS:     RUMP
--     CALLED FROM:       ANLYTC
--     CALLS:             SPPFA, SPPSL (LINPACK linear algebra package)
--                        (Formerly used LEQ1S from IMSL applied math package,
--                         Revised 1/14/85)
--
--     NOTES: THE ROUTINE USES THE TWO REGIONS (ONE BELOW AND ONE ABOVE)
--            OUTSIDE OF THE "PEAK" TO FIT THE BACKGROUND.  THE REGIONS
--            SHOULD BE MADE AS LARGE AS POSSIBLE COMMENSURATE WITH A
--            SMOOTH CURVE HAVING NO PEAKS.  MAIN USE WILL BE IN SUBTRACTING
--            OFF A HEAVIER ELEMENT (SI) FROM LIGHTER ONES (O).
--
--     MODIFICATION: NEEDED TO NORMALIZE THE X COORDINATES (CHANNEL)
--                   TO [0,1] TO ELIMINATE INSTABILITY IN MATRIX
--                   INVERSION.
--
-- Allows enough space to handle up to eighth order fit, but rediculous there.
=========================================================================== */
static int RbsBackground(SPECTRUM *ibf, int order, 
                         REAL x0, REAL x1, REAL x2, REAL x3,
								 int opts) {

/* -- Local Variables -- */
	REAL *x, *y, *sigma;										/* Allocated space */
	REAL xs, ys, scaling[2], coeff[9];					/* Results */
	SPECTRUM *work;
	int i,j, mynpt, ierr, i0,i1,i2,i3;
	
	i0 = RBSINDEX(x0, ibf);				/* Convert to channel number */
	i1 = RBSINDEX(x1, ibf);
	i2 = RBSINDEX(x2, ibf);
	i3 = RBSINDEX(x3, ibf);

	if (i0 >= i1 || i1 >= i2 || i2 >= i3) {
		ERRprintf("ERROR: Null region: background subtract aborted.\n");
		return(FALSE);
	}

/* Create a new data set over the specified range */
	mynpt = (i1-i0+1) + (i3-i2+1);
	x     = malloc(sizeof(*x) * mynpt);
	y     = malloc(sizeof(*y) * mynpt);
	sigma = malloc(sizeof(*sigma) * mynpt);

/* Fill in x,y,sigma with selected data - take actual index as X */
	mynpt = 0;
	for (i=i0; i<=i1; i++) {
		x[mynpt] = (REAL) i; y[mynpt] = ibf->counts[i];
		mynpt++;
	}
	for (i=i2; i<=i3; i++) {
		x[mynpt] = (REAL) i; y[mynpt] = ibf->counts[i];
		mynpt++;
	}
	for (i=0; i<mynpt; i++) sigma[i] = (REAL) sqrt(max(1.0, y[i]));

/* Now, just do the fit */
	if ( (ierr = FitPolynomial(x, y, sigma, mynpt, order, scaling, coeff)) != 0) {
		ERRprintf("ERROR: Failure in fitting to the background (%d)\n", ierr);
	} else {
		if (opts & 0x04) {
			TTYprintf("  Fit: x = %f index %+f\n", scaling[1], scaling[0]);
			TTYprintf("       fit = ");
			for (i=order-1; i>=0; i--) TTYprintf("%+f*x^%d ", coeff[i], i);
			TTYprintf("\n");
		}
		RbsCopySpectrum(TMPBUF, ibf);						/* Make a copy into temporary buffer */
		work = (opts & 0x08) ? ibf : RbsCopySpectrum(NULL, ibf);		/* Is this to be done inplace? */

		for (i=i0; i<=i3; i++) {
			xs = i*scaling[1] + scaling[0];
			ys = coeff[order];
			for (j=order-1; j>=0; j--) ys = ys*xs + coeff[j];
			TMPBUF->counts[i] = ys;
			work->counts[i]  -= ys;
		}
		work->dirty = TRUE;
		SysReplaceExt(work->filename, ".cut");

		if (! (opts & 0x01)) {								/* If not option set		 */
			work->first += i0;								/* Now strip so only fit */
			work->npt = i3-i0+1;								/* is in the work buffer */
			for (i=0; i<work->npt; i++) work->counts[i] = work->counts[i+i0];
		}

		if (!(opts & 0x02) && PlotSystem(2,NULL,0)){	/* Overlay the result */
			PLOT_PARMS parms;
			parms.pen       = -1;							/* Default values */
			parms.linewidth = -1;
			parms.linetype  = -1;
			parms.symbol    = -1;
			parms.symsize   = -1.0f;
			parms.npoint    = -1;
			parms.shift     = parms.offset = 0.0f;		/* No shift of plot */
			parms.doids     = FALSE;
			parms.localids  = FALSE;
			parms.Line_Syms = FALSE;
			parms.xmin = x0;	parms.xmax = x3;
			RbsChgpen(TRUE); parms.linetype =  1; parms.ibf = TMPBUF; RbsPldata(&parms);
			RbsChgpen(TRUE); parms.linetype = -1; parms.ibf = work;   RbsPldata(&parms);
		}

		if (! (opts & 0x08)) {								/* If not inplace, then move	*/
			RbsBufferScroll(work);							/* and insert into list			*/
			RbsActiveBuf = work;
		}
	}

	free(x); free(y); free(sigma);
	return(TRUE);
}


/* ===========================================================================
-- Routine to prepare for processing of element by analytic subs.  Includes
-- input from the user as to the element of interest.  Takes the first isotope
-- within 0.5 amu of one specified.
--
-- Usage: static int RbsQueryElement(SPECTRUM *ibf, CHEMIC *targ);
--
-- Inputs: ibuf - pointer to a buffer which has experimental conditions
--
-- Output: targ - pointer to a CHEMIC structure which will be filled in
--                with the appropriate parameters for a specific element.
--
-- Returns: TRUE  -> successfully found the element
--          FALSE -> either element name is invalid, or internal failure
--
-- Errors: Invalid to call with either ibf or targ set to NULL.
=========================================================================== */
static int RbsQueryElement(SPECTRUM *ibf, CHEMIC *targ) {

/*  -- Local Variables -- */
	int i;
	char token[OPTION_STR_SIZE];

	static ATOMS *atomp=NULL;		/* Keep around for repeat requests */
	static int isotmp;				/* Need this also */

/*  -- Code begin -- */
	if (targ == NULL || ibf == NULL) return(FALSE);		/* Bad call */
	if (! RbsSetexp(ibf, targ)) return(FALSE);			/* Bad experiment */

	if (LexGetTokenP(token, sizeof(token), "Element? (no change) ")) {
		if (! RbsIdent(token, &atomp, &isotmp)) return(FALSE);
	}

	if (atomp == NULL) {								/* Somehow here with bad name */
		if (! RbsIdent("Si", &atomp, &isotmp)) return(FALSE);
	}
		
	targ->isotmp = isotmp;							/* Isotope mass */
	strcpy(targ->symbol, atomp->name);

	targ->z      = atomp->z;
	targ->densit = atomp->dense;
	targ->stopp  = RbsLookupStop(targ->stop_table, atomp->z);

	for (i=0; i<NISOT; i++) targ->isoto[i] = atomp->isotop[i];

/*  ... Process mass number information, resolve to actual mass */
	targ->weight = RbsGetRealMass(targ->z,targ->isotmp);
	return(TRUE);
}

/* ===========================================================================
--  Usage Guide:
--
--      BOOL FUNCTION SETEXP(IBF)
--
--  Quick: Sets common block /CHEMIC/ given experiment conditions
--     INPUTS:   IBF      Buffer number from which to take the
--                        experimental conditions.
--
--     OUTPUTS:  Sets values in the common block /CHEMIC/:
--                        SINPH, COSPH,
--                        COSIN, COSOUT,
--                        M1, Z1, JBEAM, ENERG
--
--     NOTE:     This subroutine MUST be called to preset values
--               listed above BEFORE any calls to one of the
--               following routines (which use the results):
--                        KAPPA, SIGMA, STOPER, EPSILO
--
--     COMMON BLOCKS:     CHEMIC, RUMP
--     CALLED FROM:       QueryElement, LOCATE
--     CALLS:             None
--
=========================================================================== */
static int RbsSetexp( SPECTRUM *ibf, CHEMIC *targ) {

/*  -- Local Variables -- */
#define radeg (3.1415927 / 180.)

	STOPPING_TABLE *table;

/*  -- Code begin -- */
	if (ibf->zbeam == 0)   {
		gen_err("Can't interpret data from unknown beam");
		return (FALSE);
	}

/* --- Find the stopping power tables */
 	if ( (table=RbsStpfind(ibf->zbeam, ibf->mbeam,
 			&(targ->e_scale), ibf->e0)) == NULL) {
		gen_err("No stopping power data found for given beam.");
		return(FALSE);
	}
	targ->stop_table = table;
	targ->z1         = ibf->zbeam;
	targ->m1         = ibf->mbeam;

	/* Actual scattering angle = 180-phi */
	/* Actual incident angle   = theta   */
	/* Actual exit angle       = (IBM) ? theta+phi | complex */
	targ->sinph  = (REAL)   sin(radeg * ibf->phi);		/* sin(180-phi) =  sin(phi) */
	targ->cosph  = (REAL) (-cos(radeg * ibf->phi));		/* cos(180-phi) = -cos(phi) */
	targ->cosin  = (REAL)   cos(radeg * ibf->theta);	/* Correct as is */
	targ->cosout = - targ->cosin * targ->cosph;			/* For Cornell - - sign from cosph above */
	if (ibf->geom == IBM)
		targ->cosout = (REAL) cos(radeg * (ibf->theta+ibf->phi) );
	if (ibf->geom == GENERAL)
		targ->cosout = (REAL) cos(radeg * ibf->psi);
	targ->energ = ibf->e0;

	return(TRUE);
}


/* =========================================================================== 
--  Usage Guide:
--
--      FUNCTION KAPPA(MASS)
--     FUNCTION TO RETURN KINEMATIC SCATTERING FACTOR FOR GIVEN MASS
--  Quick: Computes kinematic scattering factor
--
--     INPUTS:            MASS     ATOMIC MASS (AMU)
--     From /CHEMIC/:     SINPH,COSPH
--                        M1       Mass of ion beam
--
--     OUTPUTS:  KAPPA    VALUE OF KINEMATIC FACTOR (0 < KAPPA < 1.0)
--
--     COMMON BLOCKS:     CHEMIC
--     CALLED FROM:       ANLYTC, LOCATE
--     CALLS:             None
--
=========================================================================== */
static REAL RbsKappa(CHEMIC *targ) {

/*  -- Local Variables -- */
	REAL x1,x3, result;

/*  -- Code begin -- */
	x1 = targ->m1 / targ->weight;
	x3 = 1.0f - (REAL) pow((x1*targ->sinph),2);
	if ((x1 > 1.0 && targ->cosph < 0.0) || x3 < 0)   {
		result = 0.0f;
	} else {
		result = (REAL) pow((sqrt(x3) + x1*targ->cosph)/(1.0 + x1),2);
	}
	return(result);
}


/* ===========================================================================
--  Usage Guide:
--      REAL FUNCTION SIGMA(ENERGY)
--     FUNCTION SIGMA - RETURNS CROSS-SECTION FOR GIVEN Z
--  Quick: Computes scattering cross section
--
--     INPUTS:   ENERGY   INCIDENT ENERGY (MEV)
--
--     /CHEMIC/: Z        ATOMIC NUMBER OF ELEMENT
--               THETA    SCATTERING ANGLE (COMMON USE = 180.0 - REAL)
--               M1,Z1,SINPH,COSPH
--
--     OUTPUTS:  SIGMA    CROSS SECTION (10**-24 cm**2/steradian)
--
--     COMMON BLOCKS:     CHEMIC
--     CALLED FROM:       ANLYTC
--     CALLS:             None 
--
-- see Chu et al. pg.29-30
=========================================================================== */
static REAL RbsSigma(REAL energy, CHEMIC *targ) {

/*  -- Local Variables -- */
	REAL cosdel, x2, alpha, result;

/*  -- Code begin -- */

/*  .. (2/17/86) New version treats case sin(phi)=0 correctly. */
/*  .. (3/4/86)  Latest version works asymptotically - no roundoff */
/*               errors near sin(phi)=0.  Also corrects slight */
/*               error in sin(phi)=0 left over from previous fix, */
/*               and will not crash if scattering cannot occur. */
	alpha  = targ->m1 / targ->weight;
	cosdel = 1.0f - (REAL) pow((alpha*targ->sinph),2);    /*  Well, sort of */

/* ... Make sure scattering can occur */
	if ((alpha > 1.0 && targ->cosph < 0) || cosdel < 0) return(0.0f);

	cosdel = (REAL) sqrt(cosdel);       /*  This is cos(delta) */
													/* in Appendix A of Chu et al. */
	if (fabs(targ->sinph) > 0.02)   {
		x2 = (REAL) ((4.0f / pow(targ->sinph,4)) * pow(cosdel + targ->cosph,2));
	} else {
		x2 = (REAL) (pow( (1 - pow(alpha,2) + pow(targ->sinph,2)*(1.-pow(alpha,4))/4) ,2));
	}
	result = (REAL) (pow(targ->z * targ->z1 * .35995e-13 / energy, 2) * x2 / cosdel);
	return (result);
}


/* ===========================================================================
--  Usage Guide:
--
--      FUNCTION STOPER(ENERGY)
--     FUNCTION STOPER - CALCULATES THE STOPPING POWER OF ELEMENT Z AT
--                       A GIVEN ENERGY.
--  Quick: Computes stopping cross section
--
--     INPUTS:   ENERGY   INCIDENT ENERGY (MEV)
--
--     OUTPUTS:  STOPPER   STOPPING POWER (eV-cm**2/atom)
--
--     COMMON BLOCKS:     ATOMS
--     CALLED FROM:       ANLYTC, EPSILO, THICKN
--     CALLS:             None
--
--      NOTES: COEFFICIENTS ARE TAKEN ZIEGLER RBS SIMULATION PROGRAM.
--             SAME VALUES AS IN CHU et al. APPENDIX VII EXCEPT INCIDENT
--             ENERGY IS WANTED IN KEV.  THE FACTOR OF 10**-15 IS
--             PLACED IN THE RESULT, UNLIKE VALUES IN CHU et al.
=========================================================================== */
static REAL RbsStoper(REAL energy, CHEMIC *targ) {

	REAL te1, result;

	te1 = (REAL) S_XFORM(energy * targ->e_scale * 1000.0f);	/*  get keV for Ziegler values */
	result = S_POWER(targ->stopp,te1) * 1.0e-15f;

	return (result);
}


/* ===========================================================================
--  Usage Guide:
--
--      REAL FUNCTION EPSILO(E0)
--     FUNCTION TO RETURN THE SURFACE APPROXIMATION FOR [e] IN JIM'S BOOK.
--  Quick: Computes stopping cross section factor [e]
--
--     INPUTS:   Z        ATOMIC NUMBER
--               E0       INCIDENT ENERGY (MEV)
--               PHI      SCATTERING ANGLE (COMMON USE 180 - REAL DEGREES)
--               THETA    SAMPLE ROTATION (DEGREES - ORTHOGONAL TO PHI)
--
--     OUTPUTS:  EPSILO   TOTAL STOPPING POWER (eV-cm/atom) [e]
--
--     COMMON BLOCKS:     ATOMS
--     CALLED FROM:       ANLYTC
--     CALLS:             KAPPA, STOPER
--
--      NOTES: ALL ANGLES ARE ASSUMED POSITIVE AND < 90.0
--             ALSO CALLED STOPPING CROSS SECTION FACTOR. SEE
--             CHU,MAYER,NICOLET pg. 61-63.
--             SEE ALSO NOTES ON SAMPLE GEOMETRY ASSUMED.
=========================================================================== */
static REAL RbsEpsilon(REAL e0, CHEMIC *targ) {

/*  -- Local Variables -- */
	REAL k, result;

/*  -- Code begin -- */
	k = RbsKappa(targ);
	result = RbsStoper(e0,targ)*k/targ->cosin + RbsStoper(e0*k,targ)/targ->cosout;
	return (result);
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE LOCATE(EINC,IBF)
--     SUBROUTINE TO FIND THE 5 ELEMENTS CLOSEST TO A SPECIFIED ENERGY
--  Quick: Locates 5 elements closest to specified energy
--
--     INPUTS:   EINC     SURFACE ENERGY TO BE LOCATED
--               IBF      BUFFER TO TAKE PARAMETERS FROM
--               E0       INCIDENT ENERGY (OBTAINED FROM PMS1 COMMON BLOCK)
--               PHI      SCATTERING ANGLE (OBTAINED FROM PMS1 COMMON BLOCK)
--
--     OUTPUTS:  DRAWS VALUES ON ENABLED PLOTTING DEVICE
--               PRINTS OUT THE CLOSEST 5 VALUES AND RETURNS
--
--     ERRORS:   WARNING IF THE GRAPHIC UNIT IS NOT ENABLED. NOTHING DRAWN
--               ERRORS: E0 <= 0.0
--                       ENERGY < 0.0 OR > E0
--
--     COMMON BLOCKS:     RUMP, GRAPHICS, ATOMS
--     CALLED FROM:       ANLYTC
--     CALLS:             None
--
=========================================================================== */
static void RbsLocate(REAL einc, SPECTRUM *ibf) {

/*  -- Local Variables -- */
	REAL k1,k2,ehold,khold;
	char *enstr;
	int iz,izmatch,ii,j;
	int izx[5] = {     0,     -1,     -2,      1,      2};
	int key[5] = {MK_WH1, MK_WHL, MK_WHL, MK_WHR, MK_WHR};
	static CHEMIC target;

/*  -- Code begin -- */
	if (ibf->e0 <= 0.0) {     /*  MAKE SURE WE HAVE A VALID ENERGY */
		gen_err("Incident energy invalid. (LOCATE)");
		return;
	}

	khold = einc / ibf->e0;
	if ((khold >= 1.0) || (khold < 0.0)) {
		gen_err("Selected energy out of range. (LOCATE)");
		return;
	}

	RbsSetexp(ibf, &target);							/* Make sure is valid */
	k1 = 0.0f;
	for (iz=3; iz <= NumElements ; iz++ ) {
		izmatch = iz;
		target.weight = atomic_mass(iz);
		k1 = RbsKappa(&target);
		if (k1 > khold) break;
		k2 = k1;
	}

	if (izmatch != 3) { /*  check whether last better unless i=1 */
		if (fabs(k1-khold) > fabs(k2-khold)) {
			izmatch = izmatch - 1;      /*  LAST ONE WAS BETTER */
			k1 = k2;
		}
	}

	TTYputs("\n");							/*  Blank line */
	enstr = ToEngFormat(einc*1.0e6f);
	TTYprintf(" Selected energy was %8seV\n", enstr);

	for (iz = max(3,izmatch-2); iz <= min(NumElements,izmatch+2) ; iz++ ) {
		target.weight = atomic_mass(iz);
		ehold = RbsKappa(&target) * ibf->e0;
		ii = nint((1000.0f*ehold - ibf->kev0) / ibf->kevch);
		enstr = ToEngFormat(ehold*1.0e6f);
		TTYprintf(" %2s Mass %5.1f   expected at %8seV (Channel %4d)\n",
			atomic_symbol(iz), atomic_mass(iz), enstr, ii);
	}
	TTYputs("\n");

	if (! PlotSystem(2,NULL,0)) {
		gen_warn("Plot device not enabled. (LOCATE)");
		return;
	}

/* Proper sequence for plot:  0 -1 -2 1 2, with MK_WH1 for the first only, MK_WH2 thereafter */

/*  key = MK_WH1; */
	for (j=0; j < 5 ; j++ ) {
		iz = izmatch+izx[j];
/*  	iz = (j <= 3) ? imatchz-j+1 " : izmatch+j-3 ; */
		if ((iz < 3) || (iz > NumElements)) continue;
		target.weight = atomic_mass(iz);
		ehold = RbsKappa(&target) * ibf->e0;
		if ((ehold < Rmp->emin) || (ehold > Rmp->emax)) continue;
		RbsMark(key[j], ehold, 0.0f, atomic_symbol(iz) );
/*    key = MK_WH2; */
	}

	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE THICKN(OURKEY)
--     Subroutine THICKN does the integrations and thickness calculations
--     for ANLYTC.  Call sequence:
--  Quick: Integration and thickness calculator
--
--     INPUTS:   KEY   :  1 (TH$INT) for just integration
--                     :  2 (TH$THK) for thickness also
--                     :  3 (TH$SET) Only make settings
--
--     OUTPUTS:  Terminal display of results
--
--     COMMON BLOCKS:     RUMP, GRAPHICS, CHEMIC
--     CALLED FROM:       ANLYTC
--     CALLS:             GETMEV, ORDER, IDENT, SIGMA, KAPPA, STOPER
--
=========================================================================== */
static void RbsThickn(int ourkey, CHEMIC *targ) {

/*  -- Local Variables -- */
	int i, gmkey;
	REAL x, x1, x2, x3, x4, alpha, net, gross, surfi;
	REAL xst, xend;				/*  Real channel numbers */
	int   ist, iend;	       	/*  Integer indices to COUNTS array */
	REAL yst, yend;				/*  Interpolated function at XST,XEND */
	REAL dxst, dxend;				/*  Length of interpolated region */
	REAL sigma;						/*  Cross section */
	char token[OPTION_STR_SIZE];

/* These are static variables */
	static int interp=FALSE, qmode=0;
#define	THICKNESS_FORMAT	" (%5s) %11.4e Atoms/cm**2 (%7.1f Angstroms)\n"

/*  -- Statement Function Definitions -- */
#define quad(x1) (min(1.0,pow((1.0-x1),2)))

/*  -- Code begin -- Start with the mode setting part */
	if (ourkey == TH_SET)   {
		if (! LexGetTokenP(token, sizeof(token), "[Round|Interp|Surf|Est|Query|?] ")) 
			strcpy(token, "?");
		i = LexSelect(token, "Interp Round Surface Estimated Query ?");
		if (i == 1) {				/*  I command */
			interp = TRUE;
		} else if (i == 2)   {	/*  R command */
			interp = FALSE;
		} else if (i == 3)   {	/*  S command */
			qmode = 0;
		} else if (i == 4)   {	/*  E command */
			qmode = 1;
		} else if (i == 5)   {	/*  Q command */
			qmode = 2;
		} else if (i == 6)   {	/*  ? command */
			TTYprintf("\n"
				"* current mode - Single letter command - Explanation of mode\n"
				"%c R - Integrals are rounded to nearest channel\n"
				"%c I - Integrals are interpolated between channels\n"
				"%c S - Thickness calculation based on surface approximation only\n"
				"%c E - Compensated thickness calculation with an estimated alpha\n"
				"%c Q - Compensated thickness calculation with query for alpha\n\n",
				!interp   ? '*':' ',   interp   ? '*':' ',
				qmode==0 ? '*':' ',   qmode==1 ? '*':' ',   qmode==2 ? '*':' ');
		} else {
			TTYputs("No INTSET action taken.  Use INTSET ? for help\n");
		}
		return;
	}
	
/*  .. Start the real work */
   gmkey = 3;
	xst  = RbsGetMeV(&gmkey, Rmp->chmin, "Identify region with cursor.  Press any key.",
										"Lower limit of integral? ");
	xend = RbsGetMeV(&gmkey, Rmp->chmax, NULL, "Upper limit of integral?  ");
	xst  = RBSCNNLE(xst ,ibuf);			/* Remains a real number */
	xend = RBSCNNLE(xend,ibuf);			/* Remains a real number */
	OrderPair (&xst, &xend);

/*  .. Identify ourselves */
   i = RbsGetBufNum(ibuf);
	TTYprintf("%13s integration on Buffer %2d: %38s\n",
		interp?"Interpolated":"Discrete", i, ibuf->id);

	if (interp) {
/*  .. Interpolated case:  First force within bounds */
		xst  = max(xst ,            ibuf->first);
		xend = min(xend,ibuf->npt-1+ibuf->first);
/*  .. then round towards middle of integration region */
		ist  = RBSINDEX(xst +.5,ibuf);
		iend = RBSINDEX(xend-.5,ibuf);
	} else {
/*  .. Channel by channel: Round to nearest data point */
		ist  = RBSINDEX(xst    ,ibuf);
		iend = RBSINDEX(xend   ,ibuf);
/*  .. In this case, our report must be modified to reflect the rounding done */
		xst  = RBSCNNLI(ist    ,ibuf);
		xend = RBSCNNLI(iend   ,ibuf);
	}

/* Evaluate gross integral */
	ist  = max(ist, 0);
	iend = min(iend, ibuf->npt-1);
	for (i=ist,gross=0.0f; i<=iend; i++) gross += ibuf->counts[i];

	if (interp) {
		dxst  =        RBSCNNLI(ist ,ibuf) - xst;
		dxend = xend - RBSCNNLI(iend,ibuf);
		RbsIntegrate(ibuf->counts[ist], ibuf->counts[ist-1], dxst ,&yst ,&gross);
		RbsIntegrate(ibuf->counts[iend],ibuf->counts[iend+1],dxend,&yend,&gross);
		net = gross - 0.5f*(yst+yend)*(xend-xst);
	} else {
		net = gross - (ibuf->counts[ist]+ibuf->counts[iend])*
			(iend+1-ist)/2.0f;	/*  bugfix: added "+1" 8/5/84 */
	}

	if (Rmp->raw) {
		TTYprintf(" Region: %6.1f to %6.1f Gross: %11.2f Net: %11.2f  (counts)\n",
					 xst,xend,gross,net);
	} else {
		x = RbsNormK(ibuf) * ibuf->kevch;
		gross = gross*x;	/*  Eliminate /keV */
		net = net*x;	/*  Eliminate /keV */
		TTYprintf(" Region: %6.1f to %6.1f Gross: %11.2f Net: %11.2f  (#/uC/msr)\n",
					 xst,xend,gross,net);
	}

	if (ourkey == TH_INT) return;

/*  * Continue here for thickness calculation */

	if (iend == ist)   {
		gen_err("Thickness aborted - no region specified");
		LexFlush();
		return;
	}

/*  .. Find out what element the guy wants. */
	if (! RbsQueryElement(ibuf, targ)) return;

   sigma = RbsSigma(ibuf->e0, targ);						/* To handle 0 */
	if (sigma <= 0.0f) {
		ERRprintf("ERROR: Conventional backscattering does not occur for %s\n", targ->symbol);
		return;
	} 

/* *************************************************************************** */
/* ******** Remember to take in micro-Coulombs and milli-steradians ********** */
/* ******** in the calculations.  Adds a factor of 1E9.  Introduced ********** */
/* ******** in the number of particles in equivalent micro-Coulomb. ********** */
/* *************************************************************************** */

	if (targ->densit <= 0.0) targ->densit = 1.0e22f;	/*  Get real density */
	x = targ->densit / 6.022e23f * targ->weight;	/*  To g/cm**3 */
	TTYprintf( " %2s surface approximation with micro-density of %5.2f g/cc yields\n",
		targ->symbol,x);	/*  Output text */

	x = gross / sigma / 6.25e9f * targ->cosin;

	if (Rmp->raw) x = x * RbsNormK(ibuf) * ibuf->kevch;	/*  Correct if raw data */
	x1 = x / targ->densit * 1e8f;	/*  Other units */
	x2 = x * net / gross;	/*  Other units */
	x3 = x1 * net / gross;	/*  Other units */

	TTYprintf(THICKNESS_FORMAT,"Gross",x,x1);
	TTYprintf(THICKNESS_FORMAT," Net ",x2,x3);

	if (qmode == 0) return;	/*  Stop here for surface only */

/*  ... Do a compensated approximation of the thickness.  Valid mainly for */
/*  ... layers of one element.  Corrects for cross-section variation with */
/*  ... energy - however, must use the stopping power of the element to */
/*  ... determine the scattering energy correction. */
/*  ... Use the method of ENERGY LOSS RATIO (CHU et al. pg. 65) */
/*  ... This is identically equivalent to assigning a % of the total width */
/*  ... to the incident beam using e(E0)/[K*e(E0)+e(K*E0)] */
/*  ... E(scat) = E0 - WIDTH/(KAPPA+ALPHA) */
	TTYputs("\n");
	x2 = RbsKappa(targ);			/*  Kinematic factor */
	TTYputsnl ("Compensated calculation (refer to Chu et al. page 65)");
	if (qmode == 2)   {
		alpha = LexGetReal(0.0f ,"Value for alpha: (abort) ");
		if (alpha == 0) return;
	} else {
		alpha = RbsStoper(x2*ibuf->e0,targ) / RbsStoper(ibuf->e0,targ)*	/*  Alpha (3.21) */
			targ->cosin/targ->cosout;
		TTYprintf("For fixed alpha =%15.6g  (surface approx. for %s)\n",
			alpha, targ->symbol);
	}

/*  ... Determine channel of the surface peak to integrate back */
	surfi = RBSCNNLE(x2*ibuf->e0,ibuf) - ibuf->first + 1;

	x1 = (ibuf->kevch/1000.0f) / ibuf->e0 / (alpha+x2);		/* Change/channel	*/
	x2 = (ibuf->counts[iend]-ibuf->counts[ist])/(iend-ist);	/* Slope				*/
	x3 = ibuf->counts[ist] - ist*x2;									/* Intercept		*/
	gross = 0.0f;
	net   = 0.0f;
	for (i=ist; i <= iend ; i++ ) {
		x4     = (REAL) quad(x1*(surfi-i));
		gross += ibuf->counts[i]*x4;
		net    = net+(ibuf->counts[i]-x2*i-x3)*x4;	/*  Net correction */
	}

	if (interp) {
		RbsIntegrate(ibuf->counts[ist]  * (REAL) quad(x1*(surfi-ist   )),
						 ibuf->counts[ist-1]* (REAL) quad(x1*(surfi-ist+1 )),
						 dxst ,&yst ,&gross    );
		RbsIntegrate(ibuf->counts[iend  ]*(REAL) quad(x1*(surfi-iend  )),
						 ibuf->counts[iend+1]*(REAL) quad(x1*(surfi-iend-1)),
						 dxend,&yend,&gross    );
		net = gross - 0.5f*(yst+yend)*(xend-xst);
	}

	gross = gross*ibuf->kevch*RbsNormK(ibuf);	/*  Unit conversion */
	net   = net  *ibuf->kevch*RbsNormK(ibuf);	/*  Unit conversion */

	x = gross / sigma / 6.25e9f * targ->cosin;
	x1 = x / targ->densit * 1e8f;	/*  Other units */
	x2 = x * net / gross;	/*  Net value */
	x3 = x1 * net / gross;	/*  Other units */

	TTYprintf(THICKNESS_FORMAT,"Gross",x,x1);
	TTYprintf(THICKNESS_FORMAT," Net ",x2,x3);
	TTYputs("\n");
	return;
}


/* ===========================================================================
--  Usage Guide:
--
--      SUBROUTINE INTINT(Y0,Y1,DX,Y,INTEG)
--  Quick: Interpolation function for THICKN
--     Subtoutine INTINT Interpolates Integrals.  From the point of view
--     of this routine, the integral runs from -infinity to DX.
--     It assumes that a summation has been done on the values of the
--     function at non-positive integers, corresponding to associating
--     each "channel" with a triangular basis function, the uppermost
--     of which is:
--
--     Y0        ^
--              / \
--             /   \
--            /     \
--      0    /       \
--          -1   0    1
--
--     This has already been added into the value of GROSS.  This routine
--     corrects the value of GROSS to be the true integral of the interpolated
--     function up to exactly DX (assumed to lie between 0 and 1).
--     Also returned is the interpolated value of the function itself at DX.
--
--     INPUTS:   Y0       Function value at 0
--               Y1       Function value at 1
--               DX       X value of end of integration region
--               GROSS    Uncorrected summation of channels up to 0 inclusive
--
--     OUTPUTS:  Y        Interpolated value of function at DX
--               GROSS    Corrected value of Integral (from -infinity to DX)
--
--     COMMON BLOCKS:     None
--     CALLED FROM:       THICKN
--     CALLS:             None
--
=========================================================================== */
static void RbsIntegrate(REAL y0, REAL y1, REAL dx, REAL *y, REAL *gross) {

	REAL dx1;

	dx1 = 1 - dx;
	*y = dx1*y0 + dx*y1;
	*gross = *gross + 0.5f * (-dx1*y0 + dx*(*y));
	return;
}
