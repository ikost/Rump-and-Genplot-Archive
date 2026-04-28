#define	BOTTOM			0					/* Coordinate lookups in arrays */
#define	LEFT				1
#define	TOP				2
#define	RIGHT				3
#define  ZAXIS				4

#define	IS_OFF			0
#define	IS_ON				1
#define	IS_COPY			2
#define	IS_NONLINEAR	3

/* Various titles */
#define	BottomTitle		Gpt->titles[BOTTOM]
#define	TopTitle			Gpt->titles[TOP]
#define	LeftTitle		Gpt->titles[LEFT]
#define	RightTitle		Gpt->titles[RIGHT]
#define	ZTitle			Gpt->titles[ZAXIS]

/* Various axes ranges */
#define	xminb				Gpt->rmins[BOTTOM]
#define	xmint				Gpt->rmins[TOP]
#define	yminl				Gpt->rmins[LEFT]
#define	yminr				Gpt->rmins[RIGHT]
#define	xmaxb				Gpt->rmaxs[BOTTOM]
#define	xmaxt				Gpt->rmaxs[TOP]
#define	ymaxl				Gpt->rmaxs[LEFT]
#define	ymaxr				Gpt->rmaxs[RIGHT]
#define	zminz				Gpt->rmins[ZAXIS]
#define	zmaxz				Gpt->rmaxs[ZAXIS]

/* Logarithmic axes */
#define	logxb			Gpt->logtype[BOTTOM]
#define	logxt			Gpt->logtype[TOP]
#define	logyl			Gpt->logtype[LEFT]
#define	logyr			Gpt->logtype[RIGHT]
#define	logz			Gpt->logtype[ZAXIS]

#define AXIS_Y_TYPE				0x0001			/* Make it a Y type axis		*/
#define AXIS_BRIEF				0x0002			/* Use brief labelling mode	*/
#define AXIS_TICKS_INWARD		0x0004			/* Turn ticks marks in			*/
#define AXIS_VERTICAL			0x0008			/* Tick labels vertical			*/
#define AXIS_NO_TICK_LABELS	0x0010			/* Suppress tick labels			*/
#define AXIS_NO_SUBTICKS		0x0020			/* Suppress subtick marks		*/
#define AXIS_NO_TICKS			0x0040			/* Suppress all tick marks		*/
#define AXIS_NOTHING				0x0080			/* Suppress everything			*/
#define AXIS_SPECLOG				0x0100			/* Special logarithmic mode	*/
#define AXIS_NOJUSTIFY			0x0200			/* Don't edge justify labels	*/
#define AXIS_GRID					0x0400			/* Draw full grid at major/minor */

/* Flags for Gpt->OpMode bits */
#define	OpConfirmOverwrite	0x01			/* Check before overwriting files */
#define	OpBinaryWrite			0x02			/* Use binary mode for file write */

/* Configuration parameters & other programs registering procedure entry */
#ifdef GENPLOT_C_SOURCE
	EXPORT char GptSearchPath[LONG_STR_SIZE];
	EXPORT char GptSearchExts[DFLT_STR_SIZE];
	EXPORT int (*GptRumpLink)(void);
#else
	IMPORT char GptSearchPath[LONG_STR_SIZE];
	IMPORT char GptSearchExts[DFLT_STR_SIZE];
	IMPORT int (*GptRumpLink)(void);
#endif

typedef struct _CREATE_2D {
	char x_min[DFLT_STR_SIZE], x_max[DFLT_STR_SIZE];		/* 2D create parameters */
	char tok_npt[DFLT_STR_SIZE];
	char tok_dx[DFLT_STR_SIZE];
	char equation[LONG_STR_SIZE];									/* 3D create parameters */
} CREATE_2D;

typedef struct _CREATE_3D {
	char x_min[DFLT_STR_SIZE], x_max[DFLT_STR_SIZE], y_min[DFLT_STR_SIZE], y_max[DFLT_STR_SIZE];
	char tok_row[DFLT_STR_SIZE], tok_col[DFLT_STR_SIZE];
	char equation[LONG_STR_SIZE];
} CREATE_3D;


typedef struct _CREATE {							/* Parameters in a create fnc */
	CREATE_2D fnc;
	CREATE_3D surf;
} CREATE;

typedef struct _GPT_AXIS_LABELS {
	char major[VARNAME_STR_SIZE];						/* Name of REAL array for variable with major tick positions */
	char minor[VARNAME_STR_SIZE];						/* Name of REAL array for variable with minor tick positions */
	char text[VARNAME_STR_SIZE];						/* Name of STRING array for variable with major tick labels  */ 
	char csize[VARNAME_STR_SIZE];						/* Expression with label size */
} GPT_AXIS_LABELS;

typedef struct _GPTTYPE {
	LOGICAL	AutoSymbols, AutoLineType, AutoIDs;
	LOGICAL	MinorTicks, InTicks, BoxMode, AutoAxes;
	INTEGER	ForceRegions;							/* Force axis regions			*/
	INTEGER	npoint,									/* Point skipping					*/
				linetype, linetypestart,			/* Line types						*/
				symtype,	 symtypestart;				/* Current/initial symbol type */

	REAL		xcur, ycur;								/* Cursor position				*/
	REAL		xbox[2], ybox[2];						/* Box cursor position			*/
	INTEGER	ccur, icur;								/* Cursor key pressed, pt #	*/

	INTEGER	AutoFlag;
	INTEGER	plx, ply;
	CHAR		titles[5][LONG_STR_SIZE];			/* Axis titles						*/

	char XYZ_descriptor[3][DFLT_STR_SIZE];		/* Column descriptors read/write */

	int		mode_3d;
	REAL		symsiz,
				xmin, xmax, ymin, ymax, zmin, zmax,
				rmins[5], rmaxs[5];
	REAL		tilt, rotate, skew, view_d;		/* 3D tilt, rotation and skew */
	enum		{HIDDEN_OFF=FALSE, HIDDEN_ON=TRUE, HIDDEN_PANEL} HideLines;
	int		mesh[2], resolution[2];

/* ... AXMODE is real so can be published as an array in function evaluator */
	REAL		udx[5],						/* DX  values for 5 axes */
				udx2[5],						/* DX2 values for 5 axes */
				uxs[5],uys[5],uxl[5]; 	/* Start point, length (fraction). */
	int		umx[5],						/* MX  values for axes	*/
				color[5],					/* Override color for an axis */
				axmode[5],					/* Mode the axes are in	*/
				yright, xtop;				/* Type of top/right axes */
	LOGICAL logtype[5];
	LOGICAL do_user_labels[5];			/* Use user specified labels? */
	GPT_AXIS_LABELS user_labels[5];	/* User specified labels */

	int		OpMode;						/* Bitwise flag of Op flags */

	CREATE createcall, createfnc, createfit;	/* For create functions */
	CREATE *createuse;								/* And one to be used	*/

	struct _GPTTYPE *LastGpt;			/* Previous Gpt structure */
	int	 push_status;

} GPTTYPE;

#ifdef GENPLOT_C_SOURCE
	EXPORT GPTTYPE *Gpt;
#else
	IMPORT GPTTYPE *Gpt;
#endif

EXTERN char GptUserModule[PATH_MAX];
EXTERN LOGICAL (*GptUserCmd)  (int key, char *cmd, char *Curve);
EXTERN LOGICAL (*GptUserFnc)  (char *Curve);
EXTERN LOGICAL (*GptUserRead) (char *Filename, char *Curve);
EXTERN LOGICAL (*GptUserWrite)(char *Filename, char *Curve);
