/* helper.c - External routines of general usefulness */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#include "preload.h"

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "mytypes.h"
#include "extends.h"
#include "lexp.h"

#include "helper.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ============================================================================
-- Heap sorting routine
--
-- Usage: call heap_srt(x,y,z,npt,y_flag)
--
-- Inputs: X,Y,NPT - data set
--         Y_FLAG  - Bit flag for which are to be sorted
--                   0x01 - X sorted 
--                   0x02 - Y tracks X
--						   0x04 - Z tracks X
--
-- Output: X,Y,Z,NPT - sorted arrays (depending on FLAG)
--
-- Note: Added an option -XY which uses combination of X and Y for comparison.
--       Complicates life significantly!  Now have *s1 and *s2 as tests.
============================================================================= */
void heap_sort(REAL *x, REAL *y, REAL *z, INT npt, int flag) {

	INT i,j,hire,ir;
	REAL *s1, *s2, test1, test2;
	REAL xtmp, ytmp, ztmp;
	LOGICAL dox, doy, doz;

/* s is the array being sorted (normally X).  This allows treating all arrays
   symmetrically in the copy operations.  Note that the "s" array is always
	left in a sorted order */
	s1 = s2 = NULL;
	if (flag & SORT_ON_X && x != NULL)			{ s1 = x; s2 = NULL; }	/* -X option  */
	if (flag & SORT_ON_Y && y != NULL)			{ s1 = y; s2 = NULL; }	/* -Y option  */
	if (flag & SORT_ON_Z && z != NULL)			{ s1 = z; s2 = NULL; }	/* -Z option  */
	if (flag & SORT_ON_X && flag & SORT_ON_Y)	{ s1 = x; s2 = y;		}	/* -XY option */
	
	if (s1 == NULL) return;									/* Nothing to do */

/* Which additional components are to be sorted? */
	dox  = (flag & SORT_DOX) && (x != NULL) && (s1 != x) && (s2 != x);	/* Include X in sort?	*/
	doy  = (flag & SORT_DOY) && (y != NULL) && (s1 != y) && (s2 != y);	/* Include Y in sort?	*/
	doz  = (flag & SORT_DOZ) && (z != NULL) && (s1 != z) && (s2 != z);	/* Include Z in sort?	*/

	hire = npt/2;												/* Center of data set	*/
	ir   = npt-1;												/* On the way down		*/

	if (s2 == NULL) {
		while (TRUE) {												/* Repeat forever			*/
			if (hire > 0) {										/* Hiring, walk down		*/
				hire--;
				test1 = s1[hire]; 
				if (dox) xtmp = x[hire];
				if (doy) ytmp = y[hire];
				if (doz) ztmp = z[hire];
			} else {
				test1 = s1[ir];   
				if (dox) xtmp = x[ir];
				if (doy) ytmp = y[ir];   
				if (doz) ztmp = z[ir];
				s1[ir] = s1[0];
				if (dox) x[ir] = x[0];
				if (doy) y[ir] = y[0];
				if (doz) z[ir] = z[0];
				ir--;
				if (ir == 0) {
					s1[0] = test1;
					if (dox) x[0] = xtmp;
					if (doy) y[0] = ytmp;
					if (doz) z[0] = ztmp;
					return;
				}
			}

			i = hire;
			j = 2*hire+1;

			while (TRUE) {
				if (j > ir) {								/* Last element? */
					s1[i] = test1;
					if (dox) x[i] = xtmp;
					if (doy) y[i] = ytmp;
					if (doz) z[i] = ztmp;
					break;
				}
				if (j < ir) {								/* Repeat to put this in place */
					if (s1[j] < s1[j+1]) j++;
				}
				if (test1 < s1[j]) {
					s1[i] = s1[j];
					if (dox) x[i] = x[j];
					if (doy) y[i] = y[j];
					if (doz) z[i] = z[j];
					i = j;
					j = 2*j+1;
				} else {
					j = ir+1;
				}
			}
		}

/* 3D mode here */
	} else {
		while (TRUE) {												/* Repeat forever			*/
			if (hire > 0) {										/* Hiring, walk down		*/
				hire--;
				test1 = s1[hire]; test2 = s2[hire];
				if (dox) xtmp = x[hire];
				if (doy) ytmp = y[hire];
				if (doz) ztmp = z[hire];
			} else {
				test1 = s1[ir]; test2 = s2[ir];
				if (dox) xtmp = x[ir];
				if (doy) ytmp = y[ir];   
				if (doz) ztmp = z[ir];
				s1[ir] = s1[0];	s2[ir] = s2[0];
				if (dox) x[ir] = x[0];
				if (doy) y[ir] = y[0];
				if (doz) z[ir] = z[0];
				ir--;
				if (ir == 0) {
					s1[0] = test1; s2[0] = test2;
					if (dox) x[0] = xtmp;
					if (doy) y[0] = ytmp;
					if (doz) z[0] = ztmp;
					return;
				}
			}

			i = hire;
			j = 2*hire+1;

			while (TRUE) {
				if (j > ir) {								/* Last element? */
					s1[i] = test1; s2[i] = test2;
					if (dox) x[i] = xtmp;
					if (doy) y[i] = ytmp;
					if (doz) z[i] = ztmp;
					break;
				}
				if (j < ir) {								/* Repeat to put this in place */
					if (s1[j] < s1[j+1] || (s1[j] == s1[j+1] && s2[j] < s2[j+1])) j++;
				}
				if (test1 < s1[j] || (test1 == s1[j] && test2 < s2[j])) {
					s1[i] = s1[j]; s2[i] = s2[j];
					if (dox) x[i] = x[j];
					if (doy) y[i] = y[j];
					if (doz) z[i] = z[j];
					i = j;
					j = 2*j+1;
				} else {
					j = ir+1;
				}
			}
		}
	}

	return;
}


/* ============================================================================
--     SPPFA FACTORS A REAL SYMMETRIC POSITIVE DEFINITE MATRIX
--     STORED IN PACKED FORM.
--
--     SPPFA IS USUALLY CALLED BY SPPCO, BUT IT CAN BE CALLED
--     DIRECTLY WITH A SAVING IN TIME IF  RCOND  IS NOT NEEDED.
--     (TIME FOR SPPCO) = (1 + 18/N)*(TIME FOR SPPFA) .
--
--     ON ENTRY
--
--        AP      REAL (N*(N+1)/2)
--                THE PACKED FORM OF A SYMMETRIC MATRIX  A .  THE
--                COLUMNS OF THE UPPER TRIANGLE ARE STORED SEQUENTIALLY
--                IN A ONE-DIMENSIONAL ARRAY OF LENGTH  N*(N+1)/2 .
--                SEE COMMENTS BELOW FOR DETAILS.
--
--        N       INTEGER
--                THE ORDER OF THE MATRIX  A .
--
--     ON RETURN
--
--        AP      AN UPPER TRIANGULAR MATRIX  R , STORED IN PACKED
--                FORM, SO THAT  A = TRANS(R)*R .
--
--        INFO    INTEGER
--                = 0  FOR NORMAL RETURN.
--                = K  IF THE LEADING MINOR OF ORDER  K  IS NOT
--                     POSITIVE DEFINITE.
--
--
--     PACKED STORAGE
--
--          THE FOLLOWING PROGRAM SEGMENT WILL PACK THE UPPER
--          TRIANGLE OF A SYMMETRIC MATRIX.
--
--                K = 0
--                DO 20 J = 1, N
--                   DO 10 I = 1, J
--                      K = K + 1
--                      AP(K) = A(I,J)
--             10    CONTINUE
--             20 CONTINUE
--
--     LINPACK.  THIS VERSION DATED 08/14/78 .
--     CLEVE MOLER, UNIVERSITY OF NEW MEXICO, ARGONNE NATIONAL LAB.
--
-- Modification to C:  The variable jj is left with same "value" as in the
--                     F77 code.  Other indicies changed to 0 norm.
============================================================================ */
void g_sppfa(double ap[], int n, int *info) {

	double s,t;
	int j,jj,k,kj,kk;

/*  begin block with ...exits to 40 */
	jj = 0;
	for (j=0; j<n; j++) {
		*info = j;
		s = 0.0;
		if (j > 0) {
			kk  = 0;
			kj  = jj;
			for (k=0; k<j; k++) {
				t = ap[kj] - g_sdot(k,ap+kk,ap+jj);
				kk = kk + (k+1);
				t = t/ap[kk-1];
				ap[kj] = t;
				kj++;
				s += t*t;
			}	
		}
		jj += (j+1);
		s = ap[jj-1] - s;
		if	(s <= 0.0) return;							/* ERROR!!! */
		ap[jj-1] = sqrt(s);
	}

/* Successful exit */
	*info = 0;
	return;
}

/* ============================================================================
--     SPPSL SOLVES THE REAL SYMMETRIC POSITIVE DEFINITE SYSTEM
--     A * X = B
--     USING THE FACTORS COMPUTED BY SPPCO OR SPPFA.
--
--     ON ENTRY
--
--        AP      REAL (N*(N+1)/2)
--                THE OUTPUT FROM SPPCO OR SPPFA.
--
--        N       INTEGER
--                THE ORDER OF THE MATRIX  A .
--
--        B       REAL(N)
--                THE RIGHT HAND SIDE VECTOR.
--
--     ON RETURN
--
--        B       THE SOLUTION VECTOR  X .
--
--     ERROR CONDITION
--
--        A DIVISION BY ZERO WILL OCCUR IF THE INPUT FACTOR CONTAINS
--        A ZERO ON THE DIAGONAL.  TECHNICALLY THIS INDICATES
--        SINGULARITY BUT IT IS USUALLY CAUSED BY IMPROPER SUBROUTINE
--        ARGUMENTS.  IT WILL NOT OCCUR IF THE SUBROUTINES ARE CALLED
--        CORRECTLY AND  INFO == 0 .
--
--     TO COMPUTE  INVERSE(A) * C  WHERE  C  IS A MATRIX
--     WITH  P  COLUMNS
--           CALL SPPCO(AP,N,RCOND,Z,INFO)
--           IF (RCOND IS TOO SMALL .OR. INFO != 0) GO TO ...
--           DO 10 J = 1, P
--              CALL SPPSL(AP,N,C(1,J))
--        10 CONTINUE
--
--     LINPACK.  THIS VERSION DATED 08/14/78 .
--     CLEVE MOLER, UNIVERSITY OF NEW MEXICO, ARGONNE NATIONAL LAB.
--
-- Modification to C:  The variable kk is left with same "value" as in the
--                     F77 code.  Other indicies changed to 0 norm.
============================================================================ */
void g_sppsl(double ap[], int n, double b[]) {

	double t;
	int i,k,kb,kk;

	kk = 0;
	for (k=0; k<n; k++) {
		t = g_sdot(k, ap+kk, b);
		kk = kk+k+1;
		b[k] = (b[k] - t)/ap[kk-1];
	}

	for (kb=0; kb<n; kb++) {
		k = n - kb - 1;
		b[k] = b[k]/ap[kk-1];
		kk = kk-(k+1);
		t  = -b[k];
		for (i=0; i<k; i++) b[i] += t*ap[kk+i];		/* Multipy+constant */
	}

	return;
}


/* ============================================================================
-- Usage: REAL = SDOT(N,V1,V2)
--
-- Inputs: V1,V2 - Two vectors of length N
--
-- Output: SDOT$ - Dot product
============================================================================ */
double g_sdot(int dim, double *xx, double *yy) {
   double sum=0.0;
   int i;
   for (i=0; i<dim; i++) sum += xx[i]*yy[i];
   return(sum);
}


/* ============================================================================
-- Subroutine to invert a matrix of arbitrary order
--
-- Usage: matinv(double **matrix, int order);
--
-- Inputs: matrix - pointer to array of data (must be row pointers since the
--                  size is arbitrary.  matrix[0] must resolve to a pointer.
--         order  - size of matrix (number of rows/columns)
--
-- Output: matrix  - Inverted matrix.
--
-- Note: Routine uses full pivoting but order is returned on exit.  Row 
--       are not modified by a call to this routine.
============================================================================ */
int matinv(double **matrix, int order) {

	double amax, save, det=1.0;
	int i,j,k,l, *ik, *jk;

	ik = (int *) malloc(order*sizeof(*ik));
	jk = (int *) malloc(order*sizeof(*jk));

	for (k=0; k<order; k++) {
		amax = 0;											/* Find next largest element */
		while (TRUE) {
			for (i=k; i<order; i++) {
				for (j=k; j<order; j++) {
					if (fabs(matrix[j][i]) > fabs(amax)) {
						amax = matrix[j][i];
						ik[k] = i;							/* Pivoting indices */
						jk[k] = j;
					}
				}
			}
			if (amax == 0) {								/* If element is zero, error! */
				free(ik); free(jk); 
				return(-1);									/* Error -- determinant=0 */
			}
			i = ik[k];										/* Interchange rows/columns */
			if (i < k) continue;
			if (i > k) {
				for (j=0; j<order; j++) {
					save = matrix[j][k];
					matrix[j][k] = matrix[j][i];
					matrix[j][i] = -save;
				}
			}
			j = jk[k];
			if (j < k) continue;
			if (j > k) {
				for (i=0; i<order; i++) {
					save = matrix[k][i];
					matrix[k][i] = matrix[j][i];
					matrix[j][i] = -save;
				}
			}
			break;
		}

/* ... Accumulate elements of inverse matrix */
		for (i=0; i<order; i++) if (i!=k) matrix[k][i] /= -amax;
		for (i=0; i<order; i++) {
			for (j=0; j<order; j++) {
				if (i!=k && j!=k) matrix[j][i] += matrix[k][i]*matrix[j][k];
			}
		}
		for (j=0; j<order; j++) if (j!=k) matrix[j][k] /= amax;
		matrix[k][k] = 1.0/amax;
		det *= amax;
	}

/* ... Restore ordering of matrix */
	for (l=0; l<order; l++) {
		k = order-l-1;
		j = ik[k];
		if (j > k) {
			for (i=0; i<order; i++) {
				save = matrix[k][i];
				matrix[k][i] = -matrix[j][i];
				matrix[j][i] = save;
			}
		}
		i = jk[k];
		if (i > k) {
			for (j=0; j<order; j++) {
				save = matrix[j][k];
				matrix[j][k] = -matrix[j][i];
				matrix[j][i] = save;
			}
		}
	}
	free(jk); free(ik);
	return(0);
}
