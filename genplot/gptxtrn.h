/* ===========================================================================
-- Simple definitions which can be used by all and are relatively painless.
=========================================================================== */
#define	LEXESCAPE		if (LexEscape(TRUE)) return(OKAY)

/* --- Before changing these, be sure to change user.h also */
#define OKAY				 TRUE				/* Return codes from elements			*/
#define NOMORE				 FALSE			/* Error, strip command line only	*/
#define NOPLOTTER			 0x20				/* Plotter needed but not there		*/
#define RETURN				 0x40				/* Final quit request					*/
#define NOTKNOWN			 0x60				/* Unknown command						*/
#define UNIMPLEMENTED	 0x80				/* Non-implemented command				*/

EXTERN	char		GptMainCurve[VARNAME_STR_SIZE];		/* Default main curve				*/
EXTERN	char		GptUseCurve[VARNAME_STR_SIZE];		/* Pointer to alternate				*/

#ifdef GENPLOT_C_SOURCE
	EXPORT	CURVE		*GptCurve;				/* Points to a curve structure	*/
	EXPORT	SURFACE	*GptSurface;			/* Genplot surface					*/
#else
	IMPORT	CURVE		*GptCurve;				/* Points to a curve structure	*/
	IMPORT	SURFACE	*GptSurface;			/* Genplot surface					*/
#endif


/* ===========================================================================
-- Routine external definitions.
=========================================================================== */

/* curfit.c  -- in curfit.h */
/* fft2c.c   -- in fft2c.h  */

/* fft_me.c */
int  fft_me(CURVE *cv, char *cvname);
BOOL autocorr(CURVE *cv, char *cvname);
BOOL FFTSmooth(REAL *y, INT npt, INT nptmax, REAL pts);
BOOL FFTGaussSmooth(REAL *y, INT npt, INT nptmax, REAL pts);
BOOL filter(CURVE *cv, char *cvname, char *filter_expr);

/* fsetup.c */
int gpt_do_setup(void);

/* genplot.c */
int Genplot(char *tokin);
int GptMainCommands(int key, char *token);
int GptLinkXYZ(char *Curve);
void GptSetRange(void);
void GptShutDown(void);
void GptReset(LOGICAL FullReset);

/* gptaxis.c */
void GptDrawAxes(void);
void GptDraw3DAxes(void);

/* gptcull.c */
int gpt_do_cull(void);

/* gptfit.c */
int gpt_do_lsqfit(void);
int GptFit(void);
int FitPolynomial(REAL *x, REAL *y, REAL *sigma_me, int npt, int order, 
                  REAL *scaling, REAL *coeff);

/* gptplot.c */
int GptPlotCurve(void);
void *GptCreateBitmap(SURFACE *surf);

/* gptread.c */
int GptRead(char *PassedCurve);
int GptSimpleRead(char *filename, int nptmax, REAL *x, REAL *y, REAL *z);
int GptWrite(char *PassedCurve);

/* gptsubs.c */
void gpt_SetBoxCursorCoords(REAL xl, REAL yl, REAL xr, REAL yr);
int gpt_do_cursor(void);
int gpt_do_sort(void);
int gpt_do_zoom(void);
int gpt_do_unzoom(void);
int gpt_do_fixgrid(void);
int gpt_do_grid(void);
int gpt_do_editdata(void);
int gpt_do_2d(void);
int gpt_do_3d(void);
int gpt_do_user(void);
int gpt_do_load(void);

/* gptmatrx.c */
int gpt_do_matrix(void);
void Smooth_3D_Surface(SURFACE *s1);
void Rotate_3D_Surface(SURFACE *s1, char *sname, double angle, BOOL FixedSize);
int  Contour_3D_Surface(SURFACE *s1, double zt, int flags);
typedef struct _CONTOUR_DATA {
	REAL *x, *y;
	int npt;
	int chain_count;
	int *chain_list;
} CONTOUR_DATA;
CONTOUR_DATA Enum_Contour_3D_Surface(SURFACE *s1, double zt, int maxpts);

/* gpttrans.c */
int gpt_do_transform(void);

/* gptuser.c */
int GptFreeUserDLL(LOGICAL CallExit);
int GptLoadUserDLL(char *module);
int GptLoadUserMDL(char *module);

/* gpt_3d.c */
int gpt_do_3Dgrid(void);

/* helper.c  -- in helper.h -- */
/* locmin.c  -- in curfit.h -- */
/* nlsfit.c  -- in nlsfit.h -- */
/* spline.c  -- in spline.h -- */

/* userrdwr.c */
int GptDefaultUserFnc(char *UseCurve);
int GptDefaultUserRead(char *FileName, char *UseCurve);
int GptDefaultUserWrite(char *FileName, char *UseCurve);
