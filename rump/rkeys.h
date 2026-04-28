/*  RKEYS.INS */
/*  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*  Larry Doolittle */
/*  Insert file for intra-RUMP communication */
/*  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*  ... Mark routine keys */
#define MK_XHR 1	/*  Plot and label Crosshair */
#define MK_TKL 2	/*  Plot and label Tickmark */
#define MK_TIK 3	/*  Just plot Tickmark */
#define MK_WH1 4	/*  First element of WHATISIT */
#define MK_WH2 5	/*  Following elements of WHATISIT */
#define MK_CUR 6	/*  Cursor return (fixes log/sqrt scales) */
#define MK_LAR 7	/*  Left-pointing Arrow and Text */
#define MK_TIT 8	/*  Title in Upper Left */
#define MK_TKH 9	/*  Title in Upper Left */
#define MK_WHL 10
#define MK_WHR 11

/*  ... Thickness/Integration routine keys - Routine THICKN */
#define TH_INT 1	/*  Just integration */
#define TH_THK 2	/*  Also do thickness calculations */
#define TH_SET 3	/*  Change settings */

/*  ... Spectrum plotting keys - Routine RBSPLT */
#define PLT_QY 1	/*  Ask user what buffer */
#define PLT_AX 2	/*  Draw axis first */
#define PLT_OV 4	/*  Put data on plot */
#define PLT_BL 8	/*  Blowup a segment of data */

/*  ... Miscellaneous special values - SIM files */
#define ELEM_INVALID		-1			/* Element is invalid			 */
#define LAYER_INVALID	-1			/* Layer is invalid				 */
#define BUFF_UNKNOWN	 -999			/* From RbsBuffFind()			 */
#define STOP_INVALID		-1			/* Stopping power data invalid */
