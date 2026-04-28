typedef enum _PERT_TYPE {
	INVALID,
	LAYER_VAR,
	MEV, THETA, PHI, PSI, FWHM, TAU, KEVCH, KEV0, CURRENT, CORR,
	STRAGGLE, MULTIPLE, FUZZ,
	VARIABLE
} PERT_TYPE;

typedef struct _PERT_ADR {
	PERT_TYPE type;
	LAYER *layer;								/* Layer number (if LAYER)	*/
	REAL *x;										/* Actual address of var	*/
	char desc[DFLT_STR_SIZE];				/* Description of address	*/
	char brief[12];							/* Brief name/description	*/
	char write[DFLT_STR_SIZE];				/* Code needed to create	*/
} PERT_ADR;

typedef struct _PERT_EQN {
	struct _PERT_EQN *next;					/* Linked list pointer		*/
	PERT_ADR adr;								/* variable being set		*/
	char eqn[DFLT_STR_SIZE];				/* Actual equation			*/
} PERT_EQN;

typedef struct _PERT_VAR {
	struct _PERT_VAR *next;					/* Linked list pointer		*/
	PERT_ADR adr;								/* Variable being varied	*/
	REAL min, max;								/* Limits						*/
} PERT_VAR;

typedef struct _PERT_ERR {					/* Error window in spectrum	*/
	int low,high;								/* Low/high channel numbers	*/
} PERT_ERR;

typedef struct _PERT_NORM {				/* Normalize window				*/
	enum {SET,UNSET,VARY} mode;			/* Is normalization active?	*/
	int low,high;								/* Low/high channel numbers	*/
	int imin,imax;								/* data array min/max			*/
	REAL sum;									/* Sum of data[i] in window	*/
} PERT_NORM;

#define	NUM_ERR_WINS	10					/* Maximum # of error windows */

extern PERT_ERR  pert_err[NUM_ERR_WINS+1];
extern PERT_NORM pert_norm;
extern PERT_VAR *pert_vars;				/* List of varying parameters		*/
extern PERT_EQN *pert_eqns;				/* List of equations to be set	*/
