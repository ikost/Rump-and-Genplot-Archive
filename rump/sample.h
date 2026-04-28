/*  SAMX.INS */

/* ===========================================================================
-- This common block contains the user level sample description of a
-- simulation.  The structures defined here are passed to the simulation
-- routines to do the actual spectral creation
=========================================================================== */

/* Configuration limits */
#define MAXEL   20						/* Limit on number of Elements			*/
#define MAXDEN  10						/* # of unused density entries left		*/
#define MAXPAR  5							/* Maximum number of eqn parameters		*/

/* Default values to be used if not otherwise set */
#define	DFLT_SUBLAYER_NUM		0			/* Default number of sublayers	*/
#define	DFLT_SUBLAYER_THICK	200.0		/*	Default /cm2 sublayer thickness	*/

/* ---------------------------------------------
-- Layer information.  Doubly linked list of
-- descriptions.  Units specified by a table
--------------------------------------------- */
typedef enum _UNITTYPE {ABSOLUTE, ATOMIC, MOLECULAR, ANGSTROMS} UNITTYPE;

typedef struct _UNITS {
	char	ident[8];						/* Symbol for units (nm, um, ...)	*/
	UNITTYPE	type;							/* How to interpret density			*/
	REAL density;							/* Multiplicative scaling from type	*/
} UNITS;

typedef enum _EQNTYPE {
	EQ_NONE,			EQ_CONST,		EQ_ERFC,			EQ_EXP,		EQ_SEMI_INF, 
	EQ_THINFILM,	EQ_TIMEDEPE,	EQ_THICFILM,	EQ_LINEAR,	EQ_GAUSS,
	EQ_EDGEWORTH,	EQ_BURIED,		EQ_SPLINE,		EQ_USER
} EQNTYPE;

typedef struct _SIMEQN {				/* Equations recognized by SIM	*/
	char	  *name;							/* ASCII text for name (erfc)		*/
	int		minlen;						/* Minimum recognized chars		*/
	EQNTYPE  type;							/* Equation type (EQ_ERFC)			*/
	int		npar;							/* # of parameters required		*/
	int		prompts[MAXPAR];			/* Index to prompts for parms		*/
	UNITS		*dflt_units;				/* Defaults units for thickness	*/
	int		rcmd_sublayers;			/* Recommended # of sublayers		*/
	char		*description;				/* Description of equation			*/
} SIMEQN;

/* UNITS and SIMEQN should  point to static structures so can be copied */

/* Thickness must be both magnitude and units since we have A, nm, /cm2 */
typedef struct _THICKNESS {
	REAL magn;								/* Numerical value					*/
	UNITS *units;							/* Type of unit						*/
} THICKNESS;

/* Now the layer structure */
typedef struct _LAYER {
	struct _LAYER *previous;			/* Pointer to previous layer		*/
	struct _LAYER *next;					/* Pointer to next layer			*/
	REAL		matrix[MAXEL];				/* Matrix Concentration (by element)*/
	THICKNESS thick;						/* Thickness and units of layer	*/
	int		num_sublayers;				/* Number of sub-layers				*/
	THICKNESS thisub;						/* Thickness of sub-layer			*/
	SIMEQN  *eqn;							/* Diffusion equation				*/
	REAL		species[MAXEL];			/* Species (eqn) (by element)		*/
	REAL		par[MAXPAR];				/* Equation parameters				*/
	UNITS	  *eqn_units;					/* Units of dose thickness			*/
	char	  *diff_eqn;					/* Diffusion equation (if used)	*/
	REAL		fuzzd;						/* Amount of fuzzing (if any)		*/
	int		fuzzs;						/* Number of fuzz steps				*/
	int		locked;						/* Semaphore for layer in use		*/
} LAYER;

typedef char ELEM_NAME[10];

typedef struct _GLOBAL_LAYER {
	struct _GLOBAL_LAYER *previous;	/* Pointer to previous layer				*/
	struct _GLOBAL_LAYER *next;		/* Pointer to next layer					*/
	CHAR	curve[32];						/* Curve giving global concentration	*/
	REAL	species[MAXEL];				/* Species for global profile spline	*/
	int   start;							/* Global starting layer #					*/
	int   mode;								/* Global curve fit mode					*/
	int	locked;							/* Semaphore for layer in use				*/
} GLOBAL_LAYER;

struct _SAMPLE {							/* Typedef's earlier 						*/
	LAYER *first;							/* First layer of sample					*/
	LAYER *layer;							/* Current layer being processed			*/
	GLOBAL_LAYER *g_first;				/* First global profile						*/
	GLOBAL_LAYER *g_layer;				/* Global profile being processed		*/
	int  nel;								/* Number of elements used in sample	*/
	int  z2[MAXEL];						/* List of elements involved				*/
	int  nukem[MAXEL];					/* Mass of isotope, 0 for nat. dist.	*/
	ELEM_NAME elem_names[MAXEL];		/* Actual names of the elements			*/
	int  absorber_layers;				/* # of layers in an absorber foil		*/
	BOOL fres_only_absorber;			/* FRES only absorber?						*/
	REAL maxpth;							/* For auto set of # of sublayers		*/
	REAL straggle;							/* Straggling (0 => no straggle) 		*/
	REAL multiple;							/* Multiple scattering (0 => none)		*/
	BOOL noise;								/* Add noise to spectrum?					*/
	SPECTRUM *background;				/* Background spectrum to add to sims	*/
	char desc[DFLT_STR_SIZE];			/* Description of simulation				*/
};

#ifndef TYPE_SAMPLE_DEFINED			/* May be done elsewhere also	*/
	typedef struct _SAMPLE SAMPLE;
	#define	TYPE_SAMPLE_DEFINED
#endif
	
extern	SAMPLE *SimDefaultSample;					/* Active sample structure */

extern	void	SimOldPileup(SPECTRUM *buf);		/* Old fast routine			*/
extern	void	SimNewPileup(SPECTRUM *buf);		/* New accurate routine		*/
extern	void (*SimPileup)(SPECTRUM *buf);		/* Current choice				*/

/* Stopping foil routines */
extern	void (*SimStopperFoilProc)(int z, REAL mass, REAL *energy, REAL *fwhm);
extern   void *SimStopperFoilDataTable;
extern	void SimCalcStopFoil(int z, REAL mass, REAL *energy, REAL *straggle);
extern	int  SimReadStopFoilData(char *filename);

/* Routines for filling the actual sample spectrum during SimMakeIt */
extern	void (*SimInitFillSpectrum)(SPECTRUM *buf);
extern	void (*SimTermFillSpectrum)(SPECTRUM *buf);
extern	void (*SimFillSpectrum)(int z, REAL mass, REAL efront, REAL eback,
		REAL hfront, REAL hback, REAL qqq, REAL sigf, REAL sigb);

/* Default fill routines */
extern	void	SimAnlyz(int z, REAL mass, REAL efront, REAL eback,
		REAL hfront, REAL hback, REAL qqq, REAL sigf, REAL sigb);

/* Collection for doing TOF spectrum */
extern	void SimTOFInitFillSpectrum(SPECTRUM *buf);
extern	void SimTOFTermFillSpectrum(SPECTRUM *buf);
extern	void SimTOFFillSpectrum(int z, REAL mass, REAL efront, REAL eback,
		REAL hfront, REAL hback, REAL qqq, REAL sigf, REAL sigb);

/* -- Local prototypes only for people using this include */
extern	BOOL SimSilent;

int		SimGetLayerNum(LAYER *layer);
void		SimShowSample(LAYER *layer, GLOBAL_LAYER *g_layer, int nlines, BOOL detail);
REAL		SimThickConvert(UNITS *units, REAL density, REAL sum, REAL *pdensity);
