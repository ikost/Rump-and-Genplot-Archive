/* Define routines for curfit.c and locmin.c */

#define	NKEY_INIT				 0
#define	NKEY_TRY_VERBOSE		 1
#define	NKEY_TRY_SILENT		-1
#define	NKEY_TRY_DEBUG        3
#define	NKEY_EXIT				 2

typedef struct _NLS_DATA {
	REAL *data;			/* Ptr to dependent variable array (Y or Z)	(input)	*/
	BOOL *valid;		/* If not NULL, ptr to array for pt validity (input)	*/
	REAL *errorbar;	/* If !NULL, pptr to NPT error bars on data	(input)	*/
							/* Any data point with 0 error is ignored					*/
	int  npt;			/* Number of points in data set								*/
	REAL **xy;			/* Pointer to array of ptrs containing any	(input)	*/
							/* independent vars needed to generate fit.				*/
							/* Unused by fit(), but passed to evalfnc()				*/
	int  nvars;			/* Number of parms which are to be varied		(input)	*/
	REAL **vars;		/* Ptr to array of nvars ptrs to actual vars	(input)	*/
	REAL *lower;		/* If !NULL, ptr to lower limit of vars		(input)	*/
	REAL *upper;		/* If !NULL, ptr to upper limit of vars		(input)	*/
	REAL *sigma;		/* If !NULL, ptr to vector to be filled with (output)	*/
							/* program's estimate of the parm sigmas					*/
	REAL *outchi;		/* If !NULL, gets chi vector from evalchi()	(output)	*/
	REAL *correlate;	/* if !NULL, correlation matrix (nvars^2)    (output) */
                     /* Although 2D, stored as a linear array					*/
	REAL chisqr;		/* Chisqr value of the fit							(output)	*/
	REAL chiold;		/* Chisqr value of previous fit					(intern)	*/
	REAL sigmaest;		/* Estimate of Y sigma if errorbar == NULL   (output) */
	REAL EpsCrit;		/* epsilon criteria for quitting					(input)	*/
	int  dof;			/* Degrees of freedom (npt-nvars-unused pts)	(output)	*/
	REAL flamda;		/* Lamda parameter									(in/out) */

	int  (*evalfnc)(struct _NLS_DATA *nls);
	int  (*fderiv) (REAL *deriv, struct _NLS_DATA *nls, int ipt);
	int  (*evalchi)(struct _NLS_DATA *nls);

/* Space will be allocated if NULL initially.  User responsibility to free  */
	REAL *yfit;			/* Best fit (allocated on init if NULL)		(in/out)	*/

/* Private vars - dealt with on key = NKEY_INIT, free'd on key = NKEY_FREE */
	int  magic_cookie;			/* Private value to indicate initialized	*/
	void *workspace;				/* Private structures for fit routines		*/

} NLS_DATA;

int CurveFit(int key, int iter, NLS_DATA *nls);
int LocateMin(int key, int iter, NLS_DATA *nls);

int EvalChiGauss( NLS_DATA *nls );
int EvalChiPoisson( NLS_DATA *nls );
