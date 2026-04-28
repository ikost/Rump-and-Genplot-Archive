/* Routine for external fit via NLSFIT */

/* ---------------------------------------------------------------------------
This routine is an external program that can be called as the "fitting"
routine for NLSFIT in a complex mode.  NLSFIT requires only a test function
depending on some <n> parameters.  Normally, this test function is a simple
arithmetic function such as a*sin(b*x+c), but conceptually can be as complex
as a full simulation depending on a few parameters.  This program is an
example of the latter.  Any language can be used to write the program as
long as it can read/write data files, and can return program status.

Usage:

In NLSFIT, the specification "equation program ./test" (or similar)
will cause NLSFIT to call an external program (./test in this case) 
each time a trial function is evaluated.  The string specified may
contain arbitrary text, including additional parameters to be
interpreted by the external program.

NLSFIT will append to the program string
  (1) name of a data file initially filled with the X,Y (and possibly Z)
      data points.  This file must be rewritten to contain the trial
      function results, again a X,Y (Z) data file.  The original file
      will be created as an ASCII file when FIT begins.
  (2) ASCII representations of the parameters being varied, in the order
      they are listed in the STATUS of the NLSFIT.  These are written in
      %g format.

After running, the external program must rewrite the data file with the
trial function results (on the same X point grid).  A return value of 0
indicates that the program was successful, any other is taken as a failure
and processing is aborted in NLSFIT.

For example, a fit with two parameters might execute the following:
   ./test '#t_0000.dat' 4 -1
   ./test '#t_0000.dat' 4.004 -1
   ./test '#t_0000.dat' 4 -1.001
   ./test '#t_0000.dat' 1.01032 0.298676
   ./test '#t_0000.dat' 1.01133 0.298676
   ./test '#t_0000.dat' 1.01032 0.298974
   ./test '#t_0000.dat' 0.988869 0.310146
   ./test '#t_0000.dat' 0.989857 0.310146
   ./test '#t_0000.dat' 0.988869 0.310456
   ./test '#t_0000.dat' 0.988858 0.310148
Note that many calls to the test function are necessary to estimate the
curvature matrix.

This particular example is a very difficult method of doing a linear fit
of data.  The test function is just a*x+b where a is the first parameter
and b is the second parameter.  The program reads the data file, calculates
the new y value, and rewrites the data file.  A more complex program might
run a full simulation and return the simulated spectrum - as described below.

   GENPLOT: define f(x) = a*x+b
   GENPLOT: setv a = 1 setv b = 0.3
   GENPLOT: create y = f(x)+ndtri(rnd(x))*0.1
   GENPLOT: fit nlsfit
   NLSFIT: let a = -1 let b = 1
   NLSFIT: equ program ./test
   NLSFIT: vary a /
   Number of parameters now: 1
   NLSFIT: vary b /
   Number of parameters now: 2
   NLSFIT: fit
   --------------------------------------------------------------------------
       CHISQR           a          b
   --------------------------------------------------------------------------
        0.4375         -1          1
       0.01025      1.002     0.2858
       0.01024      1.013     0.2794
       0.01024      1.013     0.2794

       Variable                Value:              Sigma
       --------                ------              -----
        a                     1.013254         0.02449531
        b                    0.2793789         0.01427527

   NLSFIT:

In outline form, a more complex model might need to:
    a. Parse the parameters
    b. Shell out to run the simulation, from an automatic response file
       or via popen().
    c. Read results of the simulation - may be a curve for example.
    d. Read the data file, and map the simulation results onto the required
       X points - this may involve interpolating between simulated points
    e. Rewrite the file and return.
--------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* ===========================================================================
-- This routine expects to be called with arguments:
--
--     argv[0] - program name
--     argv[1] - name of a data file which must be rewritten
--     argv[2] - first argument to function (slope)
--     argv[3] - second argument to function (offset)
--
-- On return, 0 indicates successful generation of a trial function which
-- is now stored in the data file.  Any other value indicates a failure.
=========================================================================== */
int main(int argc, char *argv[]) {

	char *outfile, *aptr, inline[200];
	FILE *funit;
	int i,npt;
	float a,b, *x,*y;

/* Parse the incoming arguments - assume all is okay if # okay */
	if (argc != 4) return(1);			/* FAILURE */
	outfile = argv[1];					/* Parse the expected parameters */
	a = strtod(argv[2], NULL);			/* Two varying parameters			*/
	b = strtod(argv[3], NULL);

	x = malloc(1024*sizeof(*x));		/* Limit to 1024 point data files */
	y = malloc(1024*sizeof(*x));

/* Open and read the original data file */
	if ( (funit = fopen(outfile, "r")) == NULL) return(1);

	npt = 0;
	while (fgets(inline, sizeof(inline), funit) != NULL) {
		aptr = inline;
		x[npt] = strtod(aptr, &aptr);
		y[npt] = strtod(aptr, &aptr);
		npt++;
	}
	fclose(funit);
	if (npt == 0) return(1);			/* Another failure */

/* Evaluate the test function on the specified X grid */
	for (i=0; i<npt; i++) y[i] = a*x[i] + b;

/* Rewrite the data file - must include X since I will reread it next time */
	funit = fopen(outfile, "w");
	for (i=0; i<npt; i++) fprintf(funit, "%g %g\n", x[i], y[i]);
	fclose(funit);

/* Return with 0 value to indicate all is okay. */
   return(0);
}
