/*  sample.ins */
/*  sample structure common block -- includes summed polynomial coeffs. */

/* ===========================================================================
-- This common block is the *simulation* level - normally no user settings
-- are obviously present at this level.  The user description from samx.h
-- is copied as the simulation is performed.
=========================================================================== */

#define MAXEL1  20						/*  Should be same as MAXEL				*/

typedef struct _SIMLAYER {
	int  layer;								/* Which layer # does this belong to?				*/
	REAL qq;									/* Rutherford integral									*/
	REAL ehit;								/* Interface beam energies								*/
	REAL strag;								/* Bohr model straggling factor						*/
	REAL density;							/* Density (1E23 at/cm^3 in this layer)			*/
	REAL sigma_scale, dedx_scale;		/* For channeling											*/
	STOPPING_POWER coffi, coffo;		/* Stopping power of each layer IN/OUT				*/
												/* Actually coeff's * thickness -> units of eV	*/
	REAL strct[MAXEL1];					/* Quantity of material matrix (10%15 at/cm^2)	*/
} SIMLAYER;

typedef struct _GLOLAYER {
	struct _GLOLAYER *next;
	REAL	*species;						/* Pointer to species composition		*/
	int	start;							/* Starting layer								*/
	REAL  sum, density;					/* Sum and estimated density				*/
	REAL  *spl;								/* Spline coefficients						*/
	REAL  gxmin, gxmax;					/* X range where relevant					*/
	REAL  start_cm2;						/* Depth (cm2) where this starts (set)	*/
	REAL  frac;			
} GLOLAYER;

typedef struct _SAMM {
	int num_layer_allocated;			/* Number of allocated layers				*/
	int num_elements;						/* Number of active elements				*/
	int num_layers;						/* Number of active layers					*/

	SIMLAYER *layer;						/* Pointer to array of layer objects	*/
	GLOLAYER *glofirst;					/* Pointer to first global diff'n		*/

	int maxit;								/* Number of iterations in fuzz			*/

	int  zproj;								/* Z of the projectile						*/
	REAL mproj;								/* Mass of the projectile					*/
	REAL phi, sinph, cosph;				/* Sin and cos of scattering angle		*/
	REAL secin, secout;					/* Secants of inward and exit beams		*/

	STOPPING_TABLE *pi;					/* Pointer to incident beam stop table	*/
	STOPPING_TABLE *po;					/* Pointer to exit     beam stop table	*/

	int   z[MAXEL1];						/* Z for each valid elements				*/
	REAL m2[MAXEL1];						/* Specific isotope if != 0				*/

	REAL cutoff;							/* Low energy cutoff for simulations	*/
	int fsurf;								/* First layer of the sample itself		*/
	int flops;								/* Running estimate of arith. ops.		*/
	REAL ampl;								/* Amplitude of this FUZZ cycle			*/

} SAMM;

extern SAMM *samm;

/* Added MOT - July 1988.  Modifications to CREATR to handle energy scaling */
/*                         in the determination of the stopping powers.     */
/* Changed LRD - June 1989.  Modifications extended for possible different  */
/*                           particle inward and outward.                   */

REAL    e1_scale, e2_scale;	/*  Scaling values */
