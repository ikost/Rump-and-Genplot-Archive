typedef enum _VERBOSE   {NONE=0, RESULT=1, INFO=2, RUNNING=3, DEBUGALL=4} VERBOSE;
typedef enum _WEIGHTING {USE_NONE, USE_SIGMA, USE_WEIGHT} WEIGHTING;
typedef enum _METHOD    {CURVEFIT, LOCMIN} METHOD;

typedef struct _NLSVARS {			/* Variables being fit				*/
	char		name[VARNAME_STR_SIZE];/* Variable name					*/
	char		dbyda[DFLT_STR_SIZE];/* Analytical derivative			*/
	int		type;						/* Type of variable					*/
	REAL	  *ptr;						/* Pointer to variable				*/
	REAL	  *derivptr;				/* Pointer to derivative vector	*/
	REAL	  delta;						/* Fractional delta for dy/da		*/
	REAL	  lower, upper;			/* Lower/upper limit on variable	*/
} NLSVAR;

typedef enum _MACROTYPE {SIMPLE, GENPLOT, RUMP} MACROTYPE;

typedef struct _NLSDATA {

	char	equation[LONG_STR_SIZE];	/* Equation to fit					*/
	char	*result_file;					/* Result file from PROGRAM mode	*/
	char	weight_eqn[LONG_STR_SIZE];	/* Weight function					*/
	enum	{EQUATION, PROGRAM, PIPE} eqn_mode;

	NLSVAR *vars;							/* Array of variables				*/

	VERBOSE verbose;
	WEIGHTING weighting;
	METHOD method;							/* Fit algorithm to use				*/

	int		NumVars,						/* Number of terms					*/
				NumAllocated;				/* Number space allocated for		*/

	LOGICAL TryAllocation;				/* Use allocated curves for d/da	*/
	int  MaxIterations;					/* Maximum number of iterations	*/
	REAL EpsCrit;							/* epsilon criteria for quitting	*/
	REAL DeltaFrac;						/* Fractional delta for dy/da		*/
	REAL DeltaZero;						/* Fractional delta if 0			*/

	REAL xlow, xhigh;						/* Limit range for comparision	*/
	REAL ylow, yhigh;
	REAL zlow, zhigh;

	MACROTYPE MacroType;					/* External macro type				*/
	char ExternMacro[PATH_MAX];		/* External macro to be executed */

} NLSDATA;

int      nlsfit(CURVE *cv);
int      GptNLSRemoveVar(char *token);
int		GptNLSFindVar(char *token);
NLSVAR  *GptNLSAddVar(char *token, int *ierr);
NLSDATA *GptNLSAllocData(NLSDATA *ns);
