#ifndef __lexp
   #define __lexp

/* Command processor functions */
#define	U_INIT	-1							/* Initialize this routine		*/
#define	U_RESET	-2							/* Reset myself					*/
#define	U_HELP	-3							/* List help information		*/
#define	U_PARM	-4							/* List out any parameters		*/
#define	U_QUIT	-5							/* Clean up for a quit			*/

#ifdef GV_MATH_EXTENSIONS					/* Make the math extensions visible */
	#ifdef HAS_ERFC
		#define	ndtr(x)	(erfc(-(x)/sqrt(2.0))/2.0)
	#else
		TMPREAL	ERF_S(int key, TMPREAL x);
		#define	erf(x)	ERF_S(1,x)
		#define	erfc(x)	ERF_S(2,x)
		#define	ndtr(x)	ERF_S(3,x)
	#endif

	TMPREAL	NDTRI(TMPREAL p);			/* in GVCALC routines				*/							
	#define	ndtri(x)	NDTRI(x)

	TMPREAL	BETAI(TMPREAL a, TMPREAL b, TMPREAL x);
	#define	betai(a,b,x) BETAI(a,b,x)

	TMPREAL	PQ_CHI(TMPREAL x2, TMPREAL v, char type);
	#define	q_chi(x2,v)	PQ_CHI(x2,v,'Q')
	#define	p_chi(x2,v)	PQ_CHI(x2,v,'P')

#endif

void		 LexInitialize(void);			/* Must be initialized			*/

void		*LexCreateStream(void);			/* Dealing with streams */
INT	    LexDestroyStream(void *ls);	/* Destroying				*/
void		*LexSwitchStream(void *ls);	/* And switching			*/

void		 LexReset    (BOOL FullReset);
BOOL		 LexEscape   (BOOL flag);

BOOL		 LexParseLine(char *token, INT toklen, char *line, char **newline);
BOOL		 LexParseLineEx(char *token, INT toklen, char *line, char **newline, int mode, char *delims);

typedef enum _LEXCASEMODE {CASE_LOWER=-1, CASE_SAME=0, CASE_UPPER=1} LEXCASEMODE;
LEXCASEMODE	 LexSetCase(LEXCASEMODE flag);

#define	MAX_DELIMS			20				/* Maximum # of delimiters		*/
void		 LexSetDelim (char *delims);
char		*LexGetDelim (void);

BOOL		 LexAddAmpEntry(char *name, void (*fnc)(char *str, int len));

BOOL		 LexGetMath  (char *token, INT toklen);
BOOL		 LexGetMathP (char *token, INT toklen, const char *prompt);
BOOL		 LexGetFile  (char *token, INT toklen);
BOOL		 LexGetFileP (char *token, INT toklen, const char *prompt);
BOOL		 LexGetList  (char *token, INT toklen);
BOOL		 LexGetListP (char *token, INT toklen, const char *prompt);
BOOL		 LexGetStrExpr (char *token, INT toklen);
BOOL		 LexGetStrExprP(char *token, INT toklen, const char *prompt);
BOOL		 LexGetSearchPath (char *token, INT toklen);
BOOL		 LexGetSearchPathP(char *token, INT toklen, const char *prompt);
BOOL		 LexGetToken (char *token, INT toklen);
BOOL		 LexGetTokenP(char *token, INT toklen, const char *prompt);
BOOL		 LexIsEmpty  (void);
BOOL		 LexChkToken (char *token, INT toklen);
BOOL		 LexGetOption(char *token, INT toklen);
BOOL		 LexGetOptionEx(char *token, INT toklen, char *list);
INT		 LexGetInt   (INT dflt, const char *prompt);
REAL		 LexGetReal  (REAL    dflt, const char *prompt);

BOOL		 LexAlias(char *token, int toklen);
BOOL		 LexGotoLabel(const char *label);
void		 LexReadLine (const char *prompt);
void		 LexPromptStr(char *str, INT toklen, const char *prmpt);
void		 LexPromptStrNT(char *str, INT toklen, const char *prmpt);
BOOL		 LexPromptLine(char *str, INT toklen, const char *prmpt, BOOL translate);
BOOL		 LexExecFile (char *name);
#ifdef	LEXP_C_SOURCE
	EXPORT	 char LexMacroSearchPath[LONG_STR_SIZE];
	EXPORT	 char LexMacroExtList[PATH_MAX];
#else
	IMPORT	 char LexMacroSearchPath[LONG_STR_SIZE];
	IMPORT	 char LexMacroExtList[PATH_MAX];
#endif
FILE		*LexQueryActiveInput(char *namebuf);
INT		 LexQueryActiveInputID(char *namebuf);
INT		 LexSetDebug(INT mode);
BOOL		 LexSetNoEcho(BOOL flag);
BOOL		 LexSetLocalNoEcho(BOOL flag);
void		 LexInsText  (const char *text);
void		 LexClrPtr	 (void);
void		 LexFlush    (void);
typedef enum _LEXFLUSHMODE {FLUSH_QUERY=0, FLUSH_YES=1, FLUSH_NO=2} LEXFLUSHMODE;
void		 LexFlushEx  (LEXFLUSHMODE mode);
void		 LexBackup	 (void);
void		 LexGetRest  (char *str, INT toklen);
void		 LexGetRestNT(char *str, INT toklen);

CHAR		*LexGetBlockStringNT(const char *prmpt);
BOOL		 LexExecLoop(char *block, char subst, int start, int end, int inc);
BOOL		 LexExecBlock(char *block, char subst, int count, char *condition, char *list);

BOOL		 LexSingle(BOOL dflt, const char *cmmd, const char *prmpt);
#define	 LexOnOff(dflt,prmpt) LexChoice(dflt, "ON", "OFF", prmpt)
#define	 LexYesNo(dflt,prmpt) LexChoice(dflt, "YES", "NO", prmpt)
BOOL		 LexChoice(BOOL dflt, const char *true, const char *false, const char *prmpt);
INT		 LexComand(const char *token, const char *table[]);
INT		 LexSelect(const char *token, const char *table);
void		*LexCmdl(const char *token, const void *cmlist, const size_t cmlen);
void		 LexCmdlPrint(const void *cmlist, size_t cmlen, const char *header);
void		 LexCmdlPrintEx(const void *cmlist, size_t cmlen, int width, const char *header);
void		 LexCmdlCheck(const void *cmlist, const size_t cmlen);
char     *LexEncodeString(char *out, size_t length, const char *format);

BOOL	    LexSystem(int key, const char *token);
BOOL		 LexCheckHelp(char *name, char *msg, void (*LongHelp)(void));

void		 LexFMCount(int count, char *command);
void		 LexFMMake(char *wild, char subst, char *command);

char		*ToEngFormat(REAL x);

/* ---------------------------------------------------------------------------
-- LEX structures limited to use only when one really understands!!!
--
-- Interpreted and obeyed by LexParseLine
--     FIXED         - no options
--     MATH          - obey () parenthesis rules parsing
--     RETAIN_QUOTES - return strings with " around them if passed
--     LIST          - return a , delimited list of tokens
-- Interpreted and obeyed by LexGetToken
--     NO_AMP_EXPAND - don't expand and deal with &encode, similar expr's
--     NO_TRANSLATE  - don't expand %var% expressions in returned token
--
-- Returned by either or both:
--     QUOTED        - Explicitly quoted strings.  All except &yesno are
--                     considered implicitly quoted strings.
---------------------------------------------------------------------------*/
#ifdef LEXP_EXTENSIONS
	#define	PARSE_CONVERT_MASK	0x000F	/* LexConvert used by parse		*/
	#define	GETTOK_CONVERT_MASK	0x00F0	/* LexConvert used by gettok		*/
	#define	FIXED						0x0000	/* No options on parse				*/
	#define	MATH						0x0001	/* Math expression mode				*/
	#define	RETAIN_QUOTE			0x0002	/* Retain quotes on strings		*/
	#define	LIST						0x0004	/* Requesting a list of expr		*/
	#define	NO_AMP_EXPAND			0x0010	/* Don't expand &encode forms		*/
	#define	NO_TRANSLATE			0x0020	/* Don't translate %var% exprs	*/
	#define	QUOTED					0x0800	/* Token was quoted (internal)	*/
	typedef	int	LEXCONVERTMODE;
	extern LEXCONVERTMODE LexConvertMode;
	extern LEXFLUSHMODE   LexFlushMode;
#endif

/* ------------------------------------------------------------------------- */
/* Variables and mask for shutting down message levels							  */
/* ------------------------------------------------------------------------- */
#define  MATHFATAL			0x0001		/* Fatal math errors (see system.c) */
#define  MATHERROR			0x0002		/* Print math errors						*/
#define  MATHWARN				0x0004		/* Print math warnings					*/
#define	MATHINFO				0x0008		/* Print usage info message			*/
#define	MATHDEBUG			0x0010		/* Print usage debug messages			*/
#define  MATHCOMPLEX			0x0100		/* Use complex math by default		*/
#define  MATHFORCEREAL		0x0200		/* Limit to real only					*/
#define  MATH_BRANCH_PLUS	0x0400		/* Take branch cut along +x			*/
#define	MATH_INPLACE		0x0800		/* Do assignment "lets" in place		*/
#define	MATH_SAFETY_ON		0x1000		/* Try to prevent bad memory writes	*/
#define	MATH_NO_PARSE_MSG	0x2000		/* Don't print parse error messages	*/
#define	MATH_NO_CALC_MSG	0x4000		/* Don't print eval error messages	*/

#ifdef GVCODE_C_SOURCE
	EXPORT INT GVMathMode;					/* Math mode message mask				*/
#else
	IMPORT INT GVMathMode;					/* Math mode message mask				*/
#endif

/* -------------------------------------------------------------------------
-- Variable and definitions from GV code
---------------------------------------------------------------------------- */
typedef int (EXT_FNC_LINK) (int type, TMPREAL *result, TMPREAL *arg);
typedef int (EXT_FNCA_LINK) (int type, TMPREAL *result, TMPREAL *arg, char *sargs[]);
typedef int (EXT_STR_FNC_LINK) (int type, char **result, TMPREAL *arg, char *sargs[]);

typedef struct _GVPARSE_INFO {
	int	type;									/* Bit info on type of expression		*/
	int	length;								/* Number of elements defined by expr	*/
} GVPARSEINFO;

/* Bit settings for type */
#define	GV_INFO_IS_STRING_EXPR					0x01		/* Returns a numeric value	*/
#define	GV_INFO_IS_NUMERIC_EXPR					0x02		/* Returns a string value	*/
#define	GV_INFO_IS_FUNIT_EXPR					0x04		/* Returns a file handle	*/
#define	GV_INFO_INEQUALITY_COMPARISONS		0x08		/* Did a <>, <=, ... ref	*/
#define	GV_INFO_BITWISE_OPERATIONS				0x10		/* Did an & or | bitwise	*/
#define	GV_INFO_UNINDEXED_ARRAY_REFERENCE	0x20		/* Indirect array ref		*/
#define	GV_INFO_COMPLEX_REFERENCE				0x40		/* Complex variable used	*/
#define	GV_INFO_STRING_REFERENCE				0x80		/* Used a string ref			*/

typedef unsigned char GVCMDS;				/* Cmds for GV parse commands */

typedef struct _SOLVE_PARMS {
	double LowerBound, UpperBound;		/* Bound on root position		*/
	double Guess;								/* Initial guess					*/
	double epsilon;							/* Precision required			*/
	int    MaxIterate;						/* Maximum # of iterations		*/
	TMPREAL (*fnc)(double root, int idx, int *err);	/* Function to solve	*/
	GVCMDS *cmds;								/* Function commands to solve	*/
} SOLVE_PARMS;

/* --------------------------------------------------
-- Bit pattern w/ subclass possibilities.  
--    0x0001 -- linked variable subclass
--    0x0002 -- 2D/3D subclass
--    0x0004 -- alternate precision subclass
--    0xFFF0 -- main class definition (12 maximum)
-------------------------------------------------- */
#define	GV_REAL						0x0010	/* Real/double variable class	*/
#define	GV_REAL_LINK				0x0011
#define	GV_DOUBLE					0x0014	/* Alternate precision			*/
#define	GV_DOUBLE_LINK				0x0015
#define	GV_INT						0x0020	/* Integer variable class		*/
#define	GV_INT_LINK					0x0021
#define	GV_COMPLEX					0x0040	/* Complex variable class		*/
#define	GV_COMPLEX_LINK			0x0041
#define	GV_ARRAY						0x0080	/* Real array class				*/
#define	GV_ARRAY_LINK				0x0081
#define	GV_DOUBLE_ARRAY         0x0084	/* Alternate precision			*/
#define	GV_DOUBLE_ARRAY_LINK    0x0085
#define	GV_COMPLEX_ARRAY			0x0100	/* Complex array class			*/
#define	GV_COMPLEX_ARRAY_LINK	0x0101
#define	GV_INT_ARRAY				0x0200	/* Integer arrays					*/
#define	GV_INT_ARRAY_LINK			0x0201
#define	GV_STRING					0x0400	/* String class					*/
#define	GV_STRING_LINK				0x0401
#define	GV_STRING_ARRAY			0x0800	/* String arrays					*/
#define	GV_STRING_ARRAY_LINK		0x0801
#define	GV_FUNCTION					0x1000	/* Function f(x) class			*/
#define	GV_FUNCTION_LINK			0x1001	/* External fnc of real ags	*/
#define	GV_FUNCTION_A_LINK		0x1002	/* External fnc of real/str	*/
#define	GV_STR_FUNCTION_LINK		0x1003	/* String function link			*/
#define	GV_2DCURVE					0x2000	/* Curve class						*/
#define	GV_3DCURVE					0x2002
#define	GV_SURFACE					0x4000	/* Surface class					*/
#define	GV_POINTER					0x8000	/* Generic pointer class		*/
#define	GV_FILEPTR					0x8002	/* File pointer specifically	*/

#define	GVF_HIDDEN					0x01	/* Variable not listed in gvlist()	*/
#define	GVF_NODELETE				0x02	/* Variable cannot be deleted			*/
#define	GVF_CONSTANT				0x04	/* Value cannot be changed				*/
#define	GVF_NORESIZE				0x08	/* Size cannot be changed				*/
#define	GVF_ARRAY_ALLOCATED		0x10	/* GV code malloc'd space for array	*/
#define	GVF_GLOBAL					0x20	/* Set without regard for SetLocal	*/
#define	GVF_INTERNAL				0x100	/* Variable is internal structure	*/
#define	GVF_USER						0x200	/* User allocated variable				*/
#define	GVF_ALIAS					0x400	/* Alias allocated variable			*/

#ifdef MSC60
	#define GVI_MAX_LENGTH	16378			/* Maximum curve/array length 65512 bytes */
#else
	#define GVI_MAX_LENGTH	67108864		/* Maximum curve/array length (2^26) */
#endif

/* Continuum palette - easiest to implement as part of function evaluator */
typedef enum {GV_PAL_ERROR=-2, GV_PAL_DEFAULT=-1, 
   GV_PAL_MOT=0, GV_PAL_USER, 
	  GV_PAL_AFM,		GV_PAL_HEAT,		GV_PAL_COLD,		GV_PAL_GREY,
	  GV_PAL_RANDOM,
	  GV_PAL_JET,		GV_PAL_HSV,			GV_PAL_COOL,		GV_PAL_HOT,			GV_PAL_HOT2,
	  GV_PAL_SPRING,	GV_PAL_SUMMER,		GV_PAL_AUTUMN,		GV_PAL_WINTER,
	  GV_PAL_GOLD,		GV_PAL_COPPER,		GV_PAL_BONE,		GV_PAL_PINK,
	  GV_PAL_PRISM
} GVP_CONTINUUMPALETTE;
#ifdef GVCALC_C_SOURCE
	EXPORT GVP_CONTINUUMPALETTE GVContinuumPalette;
#else
	IMPORT GVP_CONTINUUMPALETTE GVContinuumPalette;
#endif

int GVSelectContinuumColor(double x, double xlow, double xhigh, GVP_CONTINUUMPALETTE palettte);
GVP_CONTINUUMPALETTE GVSelectContinuumPalette(char *token, GVP_CONTINUUMPALETTE dflt);

typedef enum {GVP_UNKNOWN=-1, GVP_NUMERIC=0, GVP_STRING=1, GVP_FILEPTR=2, GVP_STRINGPTR=3} GVP_PARSEMODE;

BOOL		GVIsNameValid  (char *name);
int		GVSetLocal		(void);
int		GVEndLocal		(void);
BOOL		GVMakeGlobal	(char *name);

BOOL		GVLinkReal     (char *name, int flags, REAL *value);
BOOL		GVLinkDouble   (char *name, int flags, DOUBLE *value);
BOOL		GVLinkComplex  (char *name, int flags, COMPLEX *value);
BOOL		GVLinkInt      (char *name, int flags, INT *value);
BOOL		GVAllocReal    (char *name, int flags, REAL inival);
BOOL		GVAllocDouble	(char *name, int flags, DOUBLE inival);
BOOL		GVAllocComplex (char *name, int flags, COMPLEX inival);
BOOL		GVAllocInt     (char *name, int flags, INT inival);
BOOL		GVAllocArray   (char *name, int flags, INT size);
BOOL		GVAllocStrArray(char *name, int flags, INT size);
BOOL		GVAllocComplexArray(char *name, int flags, INT size);
BOOL		GVLinkArray       (char *name, int flags, REAL x[],    INT maxsize, INT *size);
BOOL		GVLinkIntArray    (char *name, int flags, INT i[],     INT maxsize, INT *size);
BOOL		GVLinkDoubleArray (char *name, int flags, DOUBLE x[],  INT maxsize, INT *size);
BOOL		GVLinkComplexArray(char *name, int flags, COMPLEX z[], INT maxsize, INT *size);
BOOL		GVLinkStrArray    (char *name, int flags, CHAR *s[],   INT maxsize, INT *size);
BOOL		GVAlloc2DCurve (char *name, int flags, INT length);
BOOL		GVAlloc3DCurve (char *name, int flags, INT length);
CURVE   *GVLink2DCurve  (char *name, int flags, REAL *x, REAL*y, int nptmax);
CURVE   *GVLink3DCurve  (char *name, int flags, REAL *x, REAL*y, REAL *z, int nptmax);
BOOL		GVRelinkSurface(char *name, SURFACE *surface);
BOOL		GVAllocSurface (char *name, int flags, INT nrow, INT ncol);
BOOL		GVAllocString  (char *name, int flags, int length);
BOOL		GVLinkString   (char *name, int flags, char *string, INT length);
BOOL		GVAllocFnc     (char *name, int flags, char *definition);
BOOL		GVLinkFnc		(char *name, int flags, int nargs, EXT_FNC_LINK *fnc);
BOOL		GVLinkFncA		(char *name, int flags, char *arglist, EXT_FNCA_LINK *fnc);
BOOL		GVLinkStrFnc	(char *name, int flags, char *arglist, EXT_STR_FNC_LINK *fnc);
BOOL		GVAllocFilePtr (char *name, int flags);

BOOL		GVGetNextEntry (void **entry, char **name, INT *type, INT *flags, void **adr);
BOOL		GVDeallocate   (char *name);
BOOL		GVResize       (char *name, int length);
BOOL		GVModifyCurve  (char *name, int type);
BOOL		GVSetValue     (char *name, char *expression);
char    *GVFindString   (char *varname);
BOOL		GVGetInfo		(char *varname, INT *type, void **ptr);
BOOL		GVGetAdrInfo   (char *varname, INT *type, void **ptr, INT *length);

char *GVEvalStrCmdsI(GVCMDS *cmds, int i, int *err);
char *GVEvalStrCmds(GVCMDS *cmds, int *err);
char *GVEvalStrExpr(char *expr, int *err);
void *GVEvalPtrExpr(char *expr, int *err, GVP_PARSEMODE mode);

TMPREAL	GVEvalExpr     (char *expr, int *err);
BOOL		GVEvalArrayExpr(REAL *array, INT npt, char *expression);
TMPREAL	GVEvalCmds     (GVCMDS *cmds, int *err);
TMPREAL	GVEvalCmdsI    (GVCMDS *cmds, int i, int *err);

TMPCOMPLEX GVEvalComplexExpr (char *expr, int *err);
TMPCOMPLEX GVEvalComplexCmds (GVCMDS *cmds, int *err);
TMPCOMPLEX GVEvalComplexCmdsI(GVCMDS *cmds, int i, int *err);

void    GVListVars (int masktypes, int maskflags, int options);
void	  GVWriteVars(FILE *funit, int masktypes, int maskflags);
void	 *GVEnumVars (void *start, int masktypes, int maskflags,
						  char *name[], void *adr[], int type[], int *nmax);

GVP_PARSEMODE GVGuessExprType(char *expr);
GVCMDS *GVParse   (char *myexp, GVPARSEINFO *info);
GVCMDS *GVParseEx (char *myexp, GVPARSEINFO *info, GVP_PARSEMODE mode);
GVCMDS *GVChkParse(char *myexp, GVPARSEINFO *info);
GVCMDS *GVDupCmds (GVCMDS *local);
void    GVFreeCmds(GVCMDS *local);
int	  GVTellCmds(GVCMDS *local, GVPARSEINFO *info);
void    GVPrintFncList(int detail, char *name);

float		GVTrimToFloat (TMPREAL x);						/* Always valid		*/
double	GVTrimToDouble(TMPREAL x);						/* Could be simpler	*/
long		GVTrimToNint(TMPREAL x);						/* Trim to nearest integer */
INT64		GVTrimToInt64(TMPREAL x);						/* Trim to nearest 64-bit integer */
INT32		GVTrimToInt32(TMPREAL x);						/* Trim to nearest 32-bit integer */
INT16		GVTrimToInt16(TMPREAL x);						/* Trim to nearest 16-bit integer */
UINT64	GVTrimToUint64(TMPREAL x);						/* Trim to nearest 64-bit integer */
UINT32	GVTrimToUint32(TMPREAL x);						/* Trim to nearest 32-bit integer */
UINT16	GVTrimToUint16(TMPREAL x);						/* Trim to nearest 16-bit integer */
COMPLEX	GVTrimToComplex(TMPCOMPLEX z);				/* Trim to valid complex */

int GVSolve(double *root, int iIndex, SOLVE_PARMS *parms, int *ic);

/* Spline helpers */
void *GVFitSpline(void *work, REAL *x, REAL *y, int npt, int opts);
void *GVFitSmoothSpline(void *work, REAL *x, REAL *y, int npt, REAL error, BOOL silent);
REAL GVEvalSpline(void *work, REAL x);
REAL GVEvalSplineIntegral(void *work, REAL xlow, REAL xhigh);

/* Special helper for GENPLOT */
int  GVValidateGenplotVars(char *type, char *var, char *cname);

#ifdef REAL_IS_DOUBLE
	#define GVTrimToReal(x) GVTrimToDouble(x)
#else
	#define GVTrimToReal(x) GVTrimToFloat(x)
#endif

#endif /* __lexp */
