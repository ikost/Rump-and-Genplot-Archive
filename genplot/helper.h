/* ===========================================================================
-- Internal routines passed among internal users that do not require advanced
-- definitions such as CURVE *, etc.  Those involving CURVE * are stored in
-- gptmain.h -- along with additional unused junk.
=========================================================================== */
void   g_sppfa(double ap[], int n, int *info);		/* Quasi linpack rtns */
void   g_sppsl(double ap[], int n, double b[]);
double g_sdot(int n, double *sx, double *sy);

/* Matrix inverter (for row pointers, not true matrix */
int matinv(double **matrix, int order);

#define	SORT_ON_MASK		0x00007			/* Just sort on MASK					*/
#define	SORT_FOR_MASK		0x00070			/* Just reordering MASK				*/

#define	SORT_ON_X			0x00001			/* Use X array for sort tests		*/
#define	SORT_ON_Y			0x00002			/* Use Y array for sort tests		*/
#define	SORT_ON_Z			0x00004			/* Use Z array for sort tests		*/
#define	SORT_ON_XY			0x00003			/* Use XY in a 3D situation		*/
#define	SORT_DOX				0x00010			/* Include X in resorted arrays	*/
#define	SORT_DOY				0x00020			/* Include Y in resorted arrays	*/
#define	SORT_DOZ				0x00040			/* Include Z in resorted arrays	*/
#define	SORT_SORT			0x00100			/* Do a normal sort					*/
#define  SORT_REVERSE		0x00200			/* When done, reverse everything	*/
#define	SORT_RANDOM			0x00400			/* Just randomize						*/
#define	SORT_STRICT			0x00800			/* Make it a strict sort			*/
#define	SORT_AVERAGE		0x01000			/* Average points when strict		*/
#define	SORT_SUM				0x02000			/* Sum points when strict			*/
#define	SORT_SILENT			0x04000			/* Turn off info/warn messages	*/
#define	SORT_NOCASE			0x08000			/* Case insensitive string sort	*/

void heap_sort(REAL *x, REAL *y, REAL *z, INT npt, int flag);
