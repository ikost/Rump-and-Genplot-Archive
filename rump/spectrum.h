/* ===========================================================================
-- (1) The SPECTRUM structure contains all of the information concerning
--     the spectrum.  Recommend using these routines to manipulate the
--     buffer instead of local malloc() since variable may not be initialized.
--     If not, new elements added to this structure may make old programs fail.
-- (2) RumpReadSpectrum will allocate everything is you want it to.
-- (3) For RumpWriteSpectrum, all elements of RUMP_SPECTRUM must be valid and
--     defined.  No checking.
-- (4) If the number of channels is specified as 0 or negative in 
--     RumpAllocateSpectrum, the count buffer will be left NULL, allowing
--     automatic allocation later.  All other components are initialized.
--
-- Definition of MCA properties.
--     o counts[i] (0<=i<npt) stores all events with energy in the window
--           E[i]-kevch/2 < E < E[i]+kevch/2
--     o The center of energy window for E[i] is
--           E[i] = (i + ALTBUF->first) * kevch + kev0
=========================================================================== */
#define	IDCHR	80					/* Maximum size of identifiers */

typedef enum _SPECTRUM_TYPE {
	RBS=0,							/* Interpret as a RBS spectrum	*/
	FRES=1,							/* Forward recoil (ERD)				*/
	PIXE=2,							/* X-ray spectrum						*/
	NUCLEAR=3,						/* Nuclear reaction resonance		*/
	OTHER=-1
} SPECTRUM_TYPE;

typedef enum _GEOMETRY_TYPE {
	CORNELL =  0,					/* Cornell theta/phi definitions	*/
	IBM     =  1,					/* IBM theta/phi definitions		*/
	GENERAL = -1					/* Full theta/phi/psi definition	*/
} GEOMETRY_TYPE;

typedef struct _SPECTRUM {
	char filename[PATH_MAX];				/* Filename                         */
	char date[IDCHR];							/* Date spectrum collected          */
	char ltct[IDCHR];							/* Live time/Clock time information */
	char id[IDCHR];							/* Identifier string                */
	SPECTRUM_TYPE type;						/* Type of spectrum						*/
	REAL  e0;									/* Incident energy            (MeV) */
	int   zbeam;								/* Atomic Z of incident             */
	REAL  mbeam;								/* Atomic mass of incident    (amu) */
	int   cbeam;								/* |Charge| state of beam           */
	REAL  q;										/* Total accumulated charge    (uC) */
	REAL  current;								/* Average beam current        (nA) */
	REAL  kevch,kev0;							/* Conversion MCA chan # -> keV     */
	REAL  first;								/* Channel number of first data pt  */
	REAL  fwhm;									/* Detector resolution        (keV) */
	REAL  tau;									/* MCA shaping time constant   (uS) */
	GEOMETRY_TYPE geom;						/* Geometry identifier              */
	REAL  phi,theta,psi;						/* Scattering angles      (degrees) */
	REAL  omega;								/* Detector solid angle       (mSr) */
	REAL  corr;									/* Random correction factor         */
	int   nspectra;							/* Number of spectra in data			*/
	int   npt;									/* Number of data points            */
	int   nptmax;								/* Dimensioned size of counts			*/
	REAL *counts;								/* Pointer to actual data           */
	int   dirty;								/* Has data changed since read		*/
	int   modify;								/* Have parameters been modified		*/
	int   iddone;								/* Is ID been done for this spectra	*/

	void  *extra;								/* User defined additional space		*/
	void  *RdwrProc;							/* Read/Write procedure list (rump use only)	*/
} SPECTRUM ;

extern	SPECTRUM *RbsAllocateSpectrum(SPECTRUM *proto, int NumChannels);
extern	SPECTRUM *RbsResizeSpectrum(SPECTRUM *buf, int size);
extern	     int	 RbsFreeSpectrum(SPECTRUM *spectra);
extern	SPECTRUM *RbsCopySpectrum(SPECTRUM *dest, SPECTRUM *source);
extern		  int	 RbsSetFileWriteVersion(int minor_level);

/* Names common to all read modules */
#define	R_MAKE_DEFAULT	0x01			/* Replace RUMP routine as default rd/wr	*/
#define	R_HAS_WRITE		0x02			/* Includes routine to write data files	*/
#define	R_USE_BINARY	0x04			/* Request that files be open binary mode */
#define	R_USE_ASCII		0x00			/* Request that files be open ascii mode	*/
#define	R_DONT_OPEN		0x08			/* RUMP should not open handles				*/
#define	R_NO_PIPE		0x10			/* This routine cannot handle pipes			*/

extern		 char *RbsRdwrID;
extern		 char *RbsRdwrDesc;
extern	    char *RbsRdwrExtList;
extern		  int  RbsRdwrOptions;
extern   SPECTRUM *RbsReadProc(FILE *funit, char *path, SPECTRUM *spectra, int *err);
extern        int  RbsWriteProc(FILE *funit, char *path, SPECTRUM *spectra);

extern	     int	 RbsRdFileOld( char *filen, SPECTRUM *buf);
