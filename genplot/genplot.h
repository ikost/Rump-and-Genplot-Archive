/* ===========================================================================
-- Externals visible from general world.  Other routines, in gptxtrn.h
-- are intended for general internal use.  These routines are published
-- in order to be generally visible to outside world.
--
-- Included in:  genplot.c  --> main definitions
--               main.c     --> needed to call GENPLOT
--               nlsfit.c   --> needed to recursively call GENPLOT
=========================================================================== */
int		Genplot(char *DefaultCurveName);
void		GptShutDown(void);
void		GptReset(LOGICAL FullReset);
int		GptLinkXYZ(char *CurveName);

int   FitPolynomial(REAL *x, REAL *y, REAL *sigma, int npt, int order,
                    REAL *scaling, REAL *coeff);

#ifdef GENPLOT_C_SOURCE
	EXPORT int (*GptRumpLink)(void);
#else
	IMPORT int (*GptRumpLink)(void);
#endif
