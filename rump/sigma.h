/* Sigma parameters - for calculating cross sections throughout */
typedef struct _SP {

	/* Set by CREATR.C as input to SIGMA.C */
		double phi, sinph, cosph;							/* Scattering angle and sin/cos	*/
																	/* PHI here is correct scattering angle */
		int z1,z2;												/* Incident/target Z					*/
		double m1,m2;											/* Incident/target mass (AMU)		*/
		double kev_max;										/* Maximum energy of simulation	*/

	/* Set by SIGMA.C for use in calls to calc */
		double csigma,											/* Cross section (1/E^2 term)		*/
				 csig_0,											/* Cross section (constant)		*/
				 csig_f;											/* Low energy rolloff				*/
		double (*calc)(double kev, struct _SP *sp);	/* Routine to get sigma				*/

	/* Set by various routines for internal use */
		double pf[10];											/* Parameters private				*/
} SP;

BOOL SetupSigmaScatter(SP *sp);
BOOL SetupSigmaRecoil(SP *sp);
