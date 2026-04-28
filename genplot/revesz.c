/* FIT$.F77 */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

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
void   g_sppfa(double ap[], int n, int *info);		/* Quasi linpack rtns */
void   g_sppsl(double ap[], int n, double b[]);
double g_sdot(int n, double *sx, double *sy);

/* Matrix inverter (for row pointers, not true matrix */
int matinv(double **matrix, int order);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */


/* ============================================================================
-- Subroutine for polynomial least squares fit
--
-- int polyfit(float x[], float y[], float sigma[], int npt, int order, 
--             float cf[], float cf_sigma[]) {
--
-- Inputs: X,Y,NPT - data points (and number of points)
--         sigma   - if not NULL, pointer to Y sigma on each point 
--         order   - order of fit (1 => linear, etc.)
--
-- Output: cf - if not NULL, fit coefficient.  cf[0] is x^0 term, etc.
--         cf_sigma - if not NULL, estimated error of each cf term
--
-- Also prints the results to the screen.  Other parameters in the code
-- may be returned if needed.
--
-- Uses full renormalization as necessary to stabilize results.
============================================================================ */
#define MAXFIT 12

int polyfit(float x[], float y[], float sigma[], int npt, int order, float cf[], float cf_sigma[]) {

/* -- Local Variables -- */
	double sumx[2*MAXFIT+1],							/* Sum X**N coefficients */
			 sumy[2*MAXFIT+1],							/* Sum Y*X**N coefficients */
			 ar[(MAXFIT+1)*(MAXFIT+2)/2],				/* Symmetric storage coeff's */
			 temp;											/* And temporary sum values */
	float	 xmin, xmax, ymin, ymax, ynorm, var;
	double weight,ax,bx,xtmp,ytmp,syy,det,r;

	int fSensitive;
	int i,j,k,nfit,nmax,ier,num_valid;
	float  my_cf[12],my_cf_sigma[12];

/* -- Code begin -- */
	if (cf == NULL) cf = my_cf;					/* So no crashes */
	if (cf_sigma == NULL) cf_sigma = my_cf_sigma;

	order = min(MAXFIT, max(1, order));			/* Constrain and limit */
	nfit = order+1;									/* My order (effective) */
	nmax = 2*nfit-1;									/* Number of terms to collect */

	for (i=0; i<nmax; i++) sumx[i] = 0.0;
	for (i=0; i<nfit; i++) {
		cf_sigma[i] = 0.0f;
		sumy[i]  = 0.0;
	}
	syy = 0.0;											/* Sum of Y*Y */
	
	xmin = xmax = x[0];
	for (i=0; i<npt; i++) {
			if (x[i] < xmin) xmin = x[i];
			if (x[i] > xmax) xmax = x[i];
	}

	if (xmin == xmax) {								/* ERROR!!!							*/
		fprintf(stderr, "ERROR: Check skull for grey matter.  Same X for all data - no fit possible\n");
		return(-1);
	}

	if (order != 1) {									/* Rescale if not linear		*/
		ax = 2.0/(xmax-xmin);						/* Slope for normed data [-1,1] */
		bx = -1.0-ax*xmin;							/* Xnorm= ax*Xreal+bx			*/
		ymin = ymax = y[0];
		for (i=0; i<npt; i++) {
			if (y[i] < ymin) ymin = y[i];
			if (y[i] > ymax) ymax = y[i];
		}
	} else {
		ax = 1.0;
		bx = 0.0;
		ymin = 0.0f;
		ymax = 1.0f;
	}
	ynorm = (ymax == ymin) ? 1 : ymax-ymin ;	/* Normalization on offset Y	*/

	fSensitive = (xmax-xmin) < ( fabs(xmax+xmin)/2 / pow(10,4.5/order) ) ;
	if (fSensitive) fprintf(stderr,
		"WARNING: The coefficients reported for this polynomial fit are subject to\n"
		"         roundoff errors and should not be trusted.  This is because the X\n"
		"         coordinates have a large zero-offset compared to the overall range of\n"
		"         %g %g.  Suggest subtracting %g from X before fitting.\n",
		xmin, xmax, (xmax+xmin)/2);
	weight = 1.0;                              /* In case no weighting */
	num_valid = 0;

/* ... Create the necessary sums of x**n, x**n*y, etc. */
	for (i=0; i<npt; i++) {
		num_valid++;

		if (sigma != NULL) weight = 1/sigma[i]/sigma[i];

		xtmp = x[i]*ax+bx;								/* Scaled coordinates */
		temp = weight;
		for (j=0; j<nmax; j++) {						/* Generate sums (x**n) */
			sumx[j] += temp;
			temp    *= xtmp;
		}
		temp = weight*(y[i]-ymin)/ynorm;				/* Scaled Y							*/
		syy  += temp*temp;								/* Special sum of Y*Y			*/
		for (j=0; j<nfit; j++) {						/* Generate the sums y*(x**n) */
			sumy[j] += temp;
			temp *= xtmp;
		}
	}

	if (num_valid < nfit) {
		fprintf(stderr, "ERROR: Please check skull for grey matter - Order of fit > # points!\n");
		return(-1);
	}

/* ... Now, split off if we have a linear fit problem */
	if (order == 1) {
		det = sumx[0]*sumx[2]-sumx[1]*sumx[1];
		if (det < 1E-35) goto SingularMatrix;
		cf[1] = (float) ((sumx[0]*sumy[1]-sumx[1]*sumy[0])/det);	/* m (s0*sxy-sx*sy) */
		cf[0] = (float) ((sumx[2]*sumy[0]-sumx[1]*sumy[1])/det);	/* b (sxx*sy-sx*sxy) */

/* ... Setup up a symmetric storage of the matrix a in solving AR*X=SUMY */
	} else {
		k = 0;
		for (i=0; i<nfit; i++) {
			for (j=0; j<=i; j++) ar[k++] = sumx[i+j];				/* x**(i+j) */
		}
		g_sppfa(ar,nfit,&ier);						/* Use LINPACK solutions */
		if (ier != 0) goto SingularMatrix;
		g_sppsl(ar,nfit,sumy);						/* Result to SUMY */

/* Correct for linear transform made on initial data. Xreal = a*x(fit) + b
   Keep CF(I) in SUX(I+1) till end so maintain double precision */
		for (i=0; i<=order; i++) sumx[i] = 0.0;		/* Clear CF(i) to start */
		sumx[0] = sumy[order];								/* Initial conditions */
		for (i=order-1; i>=0; i--) {						/* Loop down */
			for (j=order-i; j; j--)							/* Loop down */
				sumx[j] = bx*sumx[j] + ax*sumx[j-1];	/* AX+B operation */
			sumx[0] = bx*sumx[0] + sumy[i];
		}
		for (i=0; i<=order; i++) 							/* Copy & correct Y */
			cf[i] = (float) (sumx[i]*ynorm);				/* Final values */
		cf[0] = cf[0]+ymin;
	}

/* ---------------------------
... Calculate the residual
--------------------------- */
	xtmp = 0.0;												/* Just a summing register */
	for (i=0; i<npt; i++) {
		ytmp = 0.0;
		for (j=order; j>=0; j--) ytmp = ytmp*x[i] + cf[j];
		xtmp += (y[i]-ytmp)*(y[i]-ytmp);
	}

/* ... Calculate the variance and possibly the errors */
	if (num_valid != nfit) {						/* Is it possible? */
		var = (float) (xtmp/(num_valid-nfit));
		if (order == 1) {
			cf_sigma[1] = (float) sqrt(var*sumx[0]/det);		/* Error in slope */
			cf_sigma[0] = (float) sqrt(var*sumx[2]/det);		/* Error in offset */
			r = (sumx[0]*sumy[1]-sumx[1]*sumy[0]) / sqrt(det*(sumx[0]*syy-sumy[0]*sumy[0]));
		}
	} else {
		var = 0.0f;
	}

/* ... Output to the user? */
	if (order == 1) {
		fprintf(stdout, "   Slope         Offset       sigma(slope)  sigma(offset)    R\n"
					 "%13.6g%15.6g%14.4g%15.4g%8.4f\n", 
					 cf[1],cf[0],cf_sigma[1],cf_sigma[0],r);
	} else {
		fputs(" CF$(0-n): ", stdout);
		for (i=0; i<=order; i++) fprintf(stdout, "%13.5g", cf[i]);
		fprintf(stdout, "\n Variance: %14.7g\n", var);
	}

#if 0
	GVLinkArray("cf$",			GVF_USER, cf,    nfit, NULL);	/* Real coefficients */
	GVLinkArray("sigma$",		GVF_USER, cf_sigma, nfit, NULL);	/* Link the sigma		*/
	GVAllocReal("variance$",	GVF_USER, var);					/* Variance				*/
	GVAllocFnc ("fit(x)",		GVF_USER, "poly(x,cf$)");		/* Fit function		*/
#endif
	return(0);

SingularMatrix:
	fprintf(stderr, "ERROR: Unable to fit data - matrix was singular\n");
	return(-1);
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
		while (1) {
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
