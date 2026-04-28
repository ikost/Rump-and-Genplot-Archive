#define RESONANCE

typedef struct _RES_ENTRY {	
	REAL	kev;						/* Range [i].xenerg -> [i+1].xenerg	*/
	REAL	sigma;					/* Piecewise linear cross-section	*/
	REAL  slope;					/* Slope to next point					*/
} RES_ENTRY;

typedef enum _RES_MODE {M_BARNS, M_RELATIVE} RES_MODE;

typedef struct _RES_TABLE {
	struct _RES_TABLE	*next;	/* next entry for same Z				*/
	char	pathname[PATH_MAX];	/* Full pathname of resonance data	*/
	RES_MODE mode;					/* What form is the data?				*/
	int	checked;					/* Table checked for Ruth limit?		*/
	int	overrun;					/* Warning given for table overrun?	*/
	int	z1, m1;					/* Z,isotope of projectile				*/
	int	z2, m2;					/* Z,isotope of target					*/
	REAL	phi;						/* Scattering angle (degrees)			*/
	int	npt;						/* Number of points in table			*/
	RES_ENTRY fit[1];				/* Space at end for data				*/
} RES_TABLE;

/* Changed MXNELM to MXNEL to avoid name collision with ATOMS.INS   3/4/89 LRD */
#define MXNEL 93								/* # of elements to track		*/

/* We do a quick indexing on the target particle Z.  For simplicity, define
-- one larger so can directly index as [Z2] */
extern RES_TABLE *reschk[MXNEL+1];
