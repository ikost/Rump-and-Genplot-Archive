/*  rumpdata.h */

/*    Seriously hacked from Fortran RUMP.INS, RKEYS.INS and GRAPHICS.INS
 *    1/25/93 Larry Doolittle   */
/*    2/13/93 LRD  Changed internal structure to pointers and C,
                   #defines make it almost invisible to the user  */
/*    12/17/93 MOT Made real C code out of the #defines - debugger happy */

/* ========================================================================
 * There are a total of NBUF+2 buffers around for program use.  Buffer 0 is
 * dedicated to simulation plots, and buffer -1 is reserved for general    
 * program buffer work space.  Various indices identify buffers for use   
 *  IBUF   - Current active buffer
 *  MAIN   - Always buffer 1
 *  ALT    - Always buffer 0 - Also called simulation etc.
 *  BUFPTR - Lookup correspondence between buffer number and index into the
 *           array.  Allowed so scrolling buffers just involves exchanging 2
 *           numbers.  Index for buffer 4, is BUFPTR[4].  MAIN is equivalenced
 *           to BUFPTR[1].
 * ======================================================================== */
/* Definitions for the length of buffers and experiment types */

#define	CMAX		16384  	/*  Default Maximum number of data points	*/

/* ===========================================================================
-- STOPPING POWER routines and definitions.  
--
-- To first order, RUMP uses a polynomial description of the stopping power,
-- with a power level of 6 (NDEG).
-- 
-- IMPORTANT NOTE: not all references to NDEG can be efficiently done
-- in terms of the parameter.  Any change in the value of NDEG must
-- be accompanied by changes to the subsequent macros as well!
--
-- Stopping power usage definitions now in stopping.h
=========================================================================== */
#define NDEG   6                       /* Degree of polynomials */

typedef enum {STOP_LINEAR, STOP_SQRT} STOPPING_TYPE;

typedef struct _STOPPING_POWER {
	REAL p[NDEG];
} STOPPING_POWER;

typedef struct _STOPPING_TABLE {		/* **NOTE** sizeof() is invalid	*/
	struct _STOPPING_TABLE *next;		/* Pointer to next table			*/
	int   z;									/* incident particle Z for table */
	REAL	mass;								/* incident particle mass 			*/
	REAL emin,emax;						/* Range of full validity			*/
	REAL cutoff;							/* Low energy cutoff point			*/
	int   nelem;							/* # of elements in table			*/
	STOPPING_TYPE type;					/* Type of fitting table			*/
	STOPPING_POWER	stop[1];				/* Dummy first array element		*/
} STOPPING_TABLE;


/* ===========================================================================
-- ISOTOPE list routines and definitions.  
--
-- The isotope list contains fractional distributions and exact masses for
-- up to NISOT isotopes of each element.
=========================================================================== */
#define NISOT  6                       /* Number of isotopes per element  */

typedef struct _ISOTOPE {
	REAL mass;
	REAL fraction;
} ISOTOPE;

/* Elemental data base (combined Ziegler and RUMP) */
#define ZIEGLER
typedef struct _ATOMS {
	int    index;							/* Index into database tables			 */
	int    z;	                    	/* Atomic number							 */
	char   name[3]; 	              	/* Chemical symbol						 */
	REAL   mass;							/* Average mass (a.m.u.)				 */
	REAL	 dense;							/* Atomic density (atoms/cc)			 */
	REAL   scale;							/* Stopping power fudge factor		 */
	ISOTOPE isotop[NISOT];				/* Isotope abundances and masses		 */
   /* ------------------------------------------------------------------ */
#ifdef ZIEGLER
   int    zmm1;              			/* Most common isotope mass number   */
   REAL   zm1;               			/* Most common isotope weight (amu)  */
   REAL   zm2;               			/* Average atomic weight (amu)       */
   REAL   zrho;              			/* Target density (g/cm3)            */
   REAL   zatrho;            			/* Target density (atoms/cm3)        */
   REAL   zvferm;            			/* Fermi velocity of solid / v0      */
   REAL   zlfctr;            			/* Lambda screening factor for Ions  */
   REAL   zpcoef[8];         			/* Stopping coefficients for protons */
#endif
} ATOMS;

typedef struct _CHEMIC {
	int    z;							/* Its Z										*/
	int	 isotmp;						/* and mass number						*/
	int 	 z1;							/* and its Z								*/

	char   symbol[2];					/* Chemical symbol						*/

	ISOTOPE isoto[NISOT];			/* Isotope table list					*/
	STOPPING_TABLE *stop_table;	/* Stop-power array for this beam	*/
	STOPPING_POWER	*stopp;			/* Stop-power of this element?		*/
	REAL e_scale;						/* Scaling for stopping tables		*/


	REAL weight;	             	/* Atomic weight or isotope mass		*/
	REAL densit;	              	/* Atomic density (atoms/cc)			*/
	REAL m1;								/* Mass of ion beam						*/
	REAL cosin,							/* Cosine of beam entry angle			*/
		  cosout,						/* Cosine of beam exit angle			*/
		  sinph,							/* Scattering angle sine				*/
		  cosph;							/*  and cosine								*/
	REAL energ;							/* Incident beam energy					*/
} CHEMIC;


/* User modifiable routines */
		/* ... stopping.c ... */
extern int (*UserZStop)(int z1, double m1, int z2, double m2, REAL *keV, REAL *stop, int npoints);
		/* ... creatr.c ... */
extern int (*UserCrossSection)(int type, int z1, double m1, int z2, double m2, double energy,
								double cosph, double *s0, double *sm2);

/* atomio.c owned variables */
extern int  NumElements;				/* Number of elements in atom		*/
extern int  RumpDataValid;          /* RUMP data present?            */
extern ATOMS *atom;						/* Atomic data tables				*/

/* stopping.c owned variables */
extern STOPPING_TABLE *stop_tables;	/* And actual tables					*/
extern int   ZieglerDataValid;      /* Ziegler data present?         */
extern STOPPING_TYPE stop_type;		/* Type of stop fit to use			*/

/* rump.c owned variables */
extern REAL sigtab[2];						/* for Hydrogen scattering			*/
extern REAL coffe2[2];						/* for Hydrogen scattering			*/

/* Buffers - names appropriate for external release */
#ifdef CONFIG_C_SOURCE
	EXPORT SPECTRUM **RbsBuffers;				/* Index to buffer					*/
	EXPORT SPECTRUM *RbsTempBuf;				/* Temporary work buffer			*/
	EXPORT SPECTRUM *RbsActiveBuf;			/* Current active buffer			*/
	EXPORT int		  RbsNumBuf;				/* Number of buffers configured	*/
#else
	IMPORT SPECTRUM **RbsBuffers;				/* Index to buffer					*/
	IMPORT SPECTRUM *RbsTempBuf;				/* Temporary work buffer			*/
	IMPORT SPECTRUM *RbsActiveBuf;			/* Current active buffer			*/
	IMPORT int		  RbsNumBuf;				/* Number of buffers configured	*/
#endif

#define	ibuf		RbsActiveBuf			/* Names for general use internally */
#define	tmpbuf	RbsTempBuf
#define	buffers	RbsBuffers

/* Minor hack to make bufptr(-1) work right */
#define	ALTBUF		RbsBuffers[0]
#define	MAINBUF		RbsBuffers[1]
#define	TMPBUF		RbsTempBuf

/* Hooks from creatr.c */
extern int RbsRecoilZLimit;

/* Definition of VERTMODE must agree with PlotYPlot in COMPLOT */
typedef enum _VERTMODE  {GR_LIN=0, GR_LOG=1, GR_SQR=2} VERTMODE;
typedef enum _LABELMODE {GR_OFF=0, GR_BRF=1, GR_ON=2}  LABELMODE;
typedef enum _XAXIS_MODE {X_CHANNEL, X_ENERGY} XAXIS_MODE;

typedef struct _RMPTYPE {

	int	trtype,			/*  Transfer Region/Full mode 			*/
			autsim,			/*  State of simulation for autorecalc	*/
			taplot;			/*  Transfer Autoplot mode					*/

	REAL chrsiz;		  	/* = 0.16 What, me worry?					*/

	REAL ymin,ymax,		/*  Min and Max counts on plot			*/
		  emin,emax,		/*  Min and Max energy on plot			*/
		  chmin,chmax,		/*  User input Channel min/max			*/
		  cospec;			/*  User input max Counts					*/

	BOOL	forcex,			/*  Force X axis (Channels)?				*/
			autoy,			/*  Autoscale counts?						*/
			mtick,			/*  Minor ticks?								*/
			autoid;			/*  Is AUTOID enabled						*/
	BOOL	raw;				/*  TRUE => Raw Data							*/

	BOOL	AutoSymbols, AutoLineType;
	REAL		SymSize;									/* Symbol size for plots		*/
	INTEGER	npoint,									/* Point skipping					*/
				linetype, linetypestart,			/* Line types						*/
				symtype,	 symtypestart;				/* Current/initial symbol type */

	XAXIS_MODE xaxis_mode;							/* Plot vs energy or channel	*/
	VERTMODE  linear;									/* Vertical scale style			*/
	LABELMODE labels;									/* Labels mode?					*/

	struct _RMPTYPE *LastRmp;						/* Previous Gpt structure		*/

} RMPTYPE;

#define	Rmp	RbsDataBlock
#ifdef RUMP_C_SOURCE
	EXPORT RMPTYPE *RbsDataBlock;
#else
	IMPORT RMPTYPE *RbsDataBlock;
#endif

/* Aplot/Tplot structures */
typedef struct _PLOT_PARMS {
	SPECTRUM *ibf;
	REAL		xmin, xmax;									/* Range of data			*/
	int		pen;
	int		linewidth;
	int		linetype;
	int		symbol;
	int		npoint;										/* Skip factor				*/
	REAL		symsize;										/* Symbol size				*/
	REAL     shift, offset;								/* Movement of graph		*/
	BOOL	Line_Syms;
	BOOL	doids;											/* Do ident on string	*/
	BOOL  localids;
	char		ids[DFLT_STR_SIZE];						/* Local identifier		*/
} PLOT_PARMS;


/* Configuration parameters */
typedef enum _RBSPROMPT   {NICE, ABUSIVE} RBSPROMPT;
typedef enum _DENSITYCALC {IMPROVED, COMPATIBLE} DENSITYCALC;
extern char RbsSearchPath[LONG_STR_SIZE];
extern char RbsSearchExts[DFLT_STR_SIZE];

extern BOOL		    RbsAutoReturn;		/* Autoreturn from sub-process	*/
extern RBSPROMPT   RbsPromptMode;		/* Prompting mode (nice/abusive) */
extern BOOL			 RbsQueryOnExit;		/* Query before exit if mods		*/
extern DENSITYCALC RbsDensityCalc;		/* Mode for density calculations	*/
