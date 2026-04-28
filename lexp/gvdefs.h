#define MY_GV_ID					((int) 121057)				/* ID of structure type */
#define MAGIC(magic, value)	(((magic)<<1)+(value))	/* Magic # definition	*/

/* Character used to define the stack reference character */
#define	STACK_REF_CHAR	(0x7F)

/* -- If can confirm that typecast buffer access works, enable first form */
#if (defined CSET2 || defined MSC60 || defined CONVEX_C)
	#define GVPUTITEM(cmd,val,typ)	*((typ *)cmd) = (val) ,  cmd += sizeof(typ)
	#define GVGETITEM(dest,cmds,typ) *(dest)=*((typ *)cmds) , cmds+= sizeof(typ)
	#define GVGETITEM2(dest,cmds,typ,over) *(dest)=*((typ *)cmds) , cmds+= sizeof(typ)
#else												/* Safer and very legal way */
	#define GVPUTITEM(cmd,val,typ)	memcpy(cmd,&(val),sizeof(typ)) , cmd += sizeof(typ)
	#define GVPUTITEM2(cmd,val,typ, over)	memcpy(cmd,(over) &(val),sizeof(typ)) , cmd += sizeof(typ)
	#define GVGETITEM(dest,cmds,typ)	memcpy(dest,cmds,sizeof(typ)) ,  cmds+= sizeof(typ)
#endif
#define GVPUTCMD(cmd,val)	{if ((val) < 250) { *((cmd)++) = (GVCMDS) (val); } else { *((cmd)++) = 254; *((cmd)++) = (GVCMDS)((val)-250); }}

int		  gv_make_magic(char *name);
void		  gv_force_complex(GVCMDS *cmd);
int		  gv_eval_array_exp(unsigned mycmd, TMPREAL *rval, GVCMDS *cmd, int length);
TMPREAL	  gv_eval_array_fnc(unsigned todo, ARRAY *arrayptr, int imin, int imax);
TMPREAL	  gv_eval_array_fnc2(unsigned int todo, ARRAY *arrayptr, ARRAY *arrayptr2, int imin, int imax);
TMPREAL	  gv_eval_curve_fnc(unsigned int todo, CURVE *curveptr, REAL xmin, REAL xmax);
TMPREAL	  gv_eval_curve(unsigned mycmd, CURVE *curve, REAL xmin, REAL xmax, int *rc);
int		  gv_eval_surface(unsigned mycmd, TMPREAL *rval, SURFACE *surface);
char      *gv_eval_str_cmds(GVCMDS *cmds, int *err);
void      *gv_eval_ptr_cmds(GVCMDS *cmds, int *err, GVP_PARSEMODE mode);
TMPCOMPLEX gv_eval_cmds(GVCMDS *cmds, int *err);
void		  gv_math_error_msg(char *msg);

/* Math function from gvcalc (needed in GVPARSE for Student t-test */
TMPREAL BETAI_Ix(TMPREAL a, TMPREAL b, TMPREAL x);

/* Special reverse link from TPLOT to handle color reverse lookup */
int  (*RGB_Color)(char *);

#define	MAX_FNC_ARGS	(32)
typedef enum _ARGTYPE { ARG_REAL=0, ARG_STRING, ARG_ARRAY } ARGTYPE;
typedef enum _FNCTYPE { FNC_REAL=0, FNC_COMPLEX, FNC_STRING } FNCTYPE;

int gv_replace_args(char *str, char *var, ARGTYPE type, int iarg, int sarg, int aarg);

typedef struct _FUNCTION_STRUCT {
	char *def;
	char *givenname;
	char *givendef;
	FNCTYPE fnctype;							/* Type of function return */
	int  nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];
	char vars[1];
} FUNCTION;

typedef struct _FNC_LINK {
	EXT_FNC_LINK *fnc;
	int  nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];
} FNC_LINK;

typedef struct _FNCA_LINK {
	EXT_FNCA_LINK *fnc;
	int  nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];
} FNCA_LINK;

typedef struct _STR_FNC_LINK {
	EXT_STR_FNC_LINK *fnc;
	int  nargs;
	ARGTYPE argtypes[MAX_FNC_ARGS];
} STR_FNC_LINK;

/* Warning: cannot shift things too far */
typedef struct _GV_ENTRY GV_ENTRY;
struct _GV_ENTRY {
	int	ID;								/* ID value signifying GV struct	*/
	char	*varname;						/* Pointer to variable name		*/
	int	magic;							/* Quick value test for compare	*/
	int	type;								/* Type of variable/link			*/
	int	flags;							/* Option flags						*/
	int	level;							/* Var level (setlocal/endlocal)	*/
	union {
		INT		*intadr;					/* Address of a  integer link			*/
		REAL		*floatadr;				/* Address of a  float   link			*/
		DOUBLE	*doubleadr;				/* Address of a  double  link			*/
		COMPLEX	*complexadr;			/* Address of a  complex link			*/
		INT		intvalue;				/* Value of an   int    variable		*/
		REAL		floatvalue;				/* Value of a    float  variable		*/
		DOUBLE	doublevalue;			/* Value of a    double variable		*/
		COMPLEX	complexvalue;			/* Value of a    complex variable	*/
		char   	*stradr;					/* Address of a  string structure	*/
		FILE		*fileptr;				/* Open file handle						*/
		void		*pointer;				/* Generic pointer						*/
		ARRAY			  *array;			/* Address of an array   link			*/
		DOUBLE_ARRAY  *d_array;			/* Address of an double array link  */
		INT_ARRAY	  *i_array;			/* Address of an integer array link */
		COMPLEX_ARRAY *c_array;			/* Address of an complex array link */
		STRING_ARRAY  *s_array;			/* Address of a string array ptrs	*/
		CURVE	   *curve;					/* Address of a curve structure		*/
		SURFACE	*surface;				/* Address of a surface structure	*/
		FUNCTION *function;				/* Space for a function					*/
		FNC_LINK *ext_fnc;				/* Real function link					*/
		FNCA_LINK *ext_fnca;				/* Real function link w/ str args	*/
		STR_FNC_LINK *str_ext_fnc;		/* String function link					*/
	} var;
	int		extra;						/* Extra parameter for various		*/	/* See GVValidateGenplotVars before deleting */
	int		tmpuse;						/* Only for temporary (kludge) use	*/ /* Same - especially as going to 64 bit */
	int		(*validate)(GV_ENTRY *entry);

	struct _GV_ENTRY *next;				/* Where is next  structure */
	struct _GV_ENTRY *last;				/* Where was last structure */
	char	namechars[1];					/* And actual storage of name */

};

/* Bit settings for cmdinfo.type variable in lexp.h now */
EXTERN INT   GVLocalIndex;						/* Index when accessing arrays */
EXTERN INT   GVMaxIndex;						/* Pointer index for "find"	*/
EXTERN REAL  GVLnZero;							/* Added to all LOG expressions */

EXTERN GV_ENTRY *GVFirstEntry;					/* First entry in linked list	*/

/* -------------  Function execution values  ------------------------ */
#ifdef TMPREAL_IS_LONG
	#define PI 3.14159265358979323846L
	#define e  2.71828182845904523536L
	#define DEGREES_TO_RADIANS 0.01745329251994329576847L
	#define RADIANS_TO_DEGREES    57.2957795130823208778L
	#define SIN		sinl
	#define COS		cosl
	#define TAN		tanl
	#define TAN2	tan2l
	#define ASIN	asinl
	#define ACOS	acosl
	#define ATAN	atanl
	#define ATAN2	atan2l
	#define SINH	sinhl
	#define COSH	coshl
	#define TANH	tanhl
	#define LOG		logl
	#define LOG10	log10l
	#define LOGZ(z)		logl(z+GVLnZero)
	#define LOG10Z(z)		log10l(z+GVLnZero)
	#define EXP		expl
	#define SQRT	sqrtl
	#define POW		powl
	#define FABS	fabsl
	#define FMOD	fmodl
	#define CEIL	ceill
	#define FLOOR	floorl
	#define LDEXP	ldexpl
	#ifdef HAS_BESSEL
		#define J0		_j0l
		#define J1		_j1l
		#define JN		_jnl
		#define Y0		_y0l
		#define Y1		_y1l
		#define YN		_ynl
	#endif
	#ifdef HAS_ERFC
		#define	ERF		erfl
		#define	ERFC		erfcl
		#define	NDTR(x)	(erfcl(-x/SQRT(2.0))/2.0)
	#else
		#define	ERF(x)	ERF_S(1,x)
		#define	ERFC(x)	ERF_S(2,x)
		#define	NDTR(x)	ERF_S(3,x)
	#endif
	#define STRTOD _strtold
#else
	#define PI 3.14159265358979323846
	#define e  2.71828182845904523536
	#define DEGREES_TO_RADIANS 0.01745329251994329576847
	#define RADIANS_TO_DEGREES    57.2957795130823208777
	#define SIN		sin
	#define COS		cos
	#define TAN		tan
	#define TAN2	tan2
	#define ASIN	asin
	#define ACOS	acos
	#define ATAN	atan
	#define ATAN2	atan2
	#define SINH	sinh
	#define COSH	cosh
	#define TANH	tanh
	#define LOG		log
	#define LOG10	log10
	#define LOGZ(z)		log(z+GVLnZero)
	#define LOG10Z(z)		log10(z+GVLnZero)
	#define EXP		exp
	#define SQRT	sqrt
	#define POW		pow
	#define FABS	fabs
	#define FMOD	fmod
	#define CEIL	ceil
	#define FLOOR	floor
	#define LDEXP	ldexp
	#define STRTOD	strtod

	#ifdef HAS_BESSEL							/* If not in compiler, ignore */
		#define J0		j0
		#define J1		j1
		#define JN		jn
		#define Y0		y0
		#define Y1		y1
		#define YN		yn
	#endif

	#ifdef HAS_ERFC
		#define	ERF		erf				/* Intrinsic versions */
		#define	ERFC		erfc
		#define	NDTR(x)	(erfc(-x/SQRT(2.0))/2.0)
	#else
		#define	ERF(x)	ERF_S(1,x)		/* local versions		*/
		#define	ERFC(x)	ERF_S(2,x)
		#define	NDTR(x)	ERF_S(3,x)
	#endif

	#ifdef HAS_GAMMA
		#define	LN_GAMMA	gamma				/* Intrinsic version */
	#else
		#define	LN_GAMMA	r_gamma			/* local version		*/
	#endif
#endif

typedef enum _MATH_CMDS {
/* --- These basic operations (priority leveled) must be within first 255 of this list --- */
	load_immed_real,		/* Load the next double to stack */
	load_immed_imag,
	add_me,					/* Add top 2 elements */
	sub_me,					/* Sub top 2 elements */
	mul_me,					/* Multiply top 2 elements */
	div_me,					/* Divide top 2 elements */
	pow_me,					/* Raise Y**X */
	lt_me,
	le_me,
	ne_me,
	gt_me,
	ge_me,
	eq_me,
	not_me,
	and_me,
	or_me,
	eqv_me,
	neqv_me,
	bit_and_me,						/* Bitwise AND on integers		*/
	bit_or_me,						/* Bitwise OR  on integers		*/
	bit_eor_me,						/* Bitwise EOR on integers		*/
	bit_not_me,						/* Bitwise NOT on integers		*/

/* Specialized functions */
	load_e,
	load_pi,
	load_dummy_value,				/* Loads a dummy value for testing */
	load_realmin,
	load_realmax,
	load_i,
	load_j,

	load_stack_val,				/*  Load SS:SP-4-4*[DS:DI]			*/
	save_stack_val,				/*  Push ST(0) on stack				*/
	pop_stack,						/*  Decrement stack by [DS:DI]	*/

	load_string_stack_val,		/*  Load string stack value		*/
	save_string_stack_val,		/*  Put current string on stack	*/
	pop_string_stack,				/*  Pop all strings off stack		*/
	interp_string_stack_val,	/*  Interpret string stack as float */

	conditional_me,				/*  (st(0)) ? code : code			*/
	jump_me,							/*	 Increment cmd by next int		*/

	xeq_function,					/*  Execute linked function		*/
	xeq_function_a,				/*  Execute alt function w/ str args */
	xeq_str_function,				/*  Execute linked string function */

	load_real,						/*  Load simple real				*/
	load_double,					/*  Load simple double			*/
	load_complex,					/*  Load simple complex			*/
	load_int,						/*  Load simple integer			*/
	load_real_array,				/*  Load real array value		*/  /* NOTE - MUST REMAIN BELOW 250 */
	load_double_array,			/*  Load double array value	*/
	load_complex_array,			/*  Load complex array value	*/
	load_int_array,				/*  Load integer value array	*/
	load_real_idx,					/*  Load indexed real value	*/
	load_double_idx,				/*  Load indexed double value	*/
	load_complex_idx,				/*  Load indexed complex value */
	load_int_idx,					/*  Load index integer value	*/
	load_string_idx,				/*  Load single char of string */
	load_string_tmp,				/*  Load temporary local string */
	load_string_ptr,				/*  Load string pointer			*/
	load_strarray_idx,			/*  Load index string array	*/
	load_surface_pt,				/*  Load a [row,col] point		*/

	load_pfile_ptr,				/*  Load a FILE ** ptr on string stack */
	load_varentry_ptr,			/*  Load an variable entry ptr on the string stack */

/* Function calls - Must actually be defined in gvparse as functions. */
	chs_me,
	abs_me,
	sign_me,
	min_me,					/* Minimum of list */
	max_me,					/* Maximum of list */
	ave_me,					/* Average of list */
	std_me,					/* Standard deviation of list */
	sdom_me,					/* Standard deviation of list */
	median_me,				/* Median of a list */
	mad_me,					/* MAD of a list */
	count_me,				/* Number of items in a list */
	limit_me,
	int_me,
	nint_me,
	frac_me,
	mod_me,
	mantissa_me,			/*  Mantissa of a big number */
	exponent_me,			/*  Exponent of a big number */
	m1n_me,					/*  Minus 1 to the Nth power			*/
	real_me,					/*  Return only real component		*/
	imag_me,					/*	 Return only imaginary component */
	conj_me,					/*  Return complex conjugate			*/
	arg_me,					/*  Return the angle of complex #	*/
	sin_me,
	cos_me,
	tan_me,
	cot_me,
	sind_me,
	cosd_me,
	tand_me,
	cotd_me,
	asin_me,
	acos_me,
	atan_me,
	acot_me,
	atan2_me,
	asind_me,
	acosd_me,
	atand_me,
	acotd_me,
	atan2d_me,
	sinh_me,
	cosh_me,
	tanh_me,
	coth_me,
	sech_me,
	csch_me,
	asinh_me,
	acosh_me,
	atanh_me,
	acoth_me,
	asech_me,
	acsch_me,
	ln_me,
	log_me,
	exp_me,
	sqrt_me,
	fact_me,					/* Factorial */
	gamma_me,				/* Gamma function */
	lngamma_me,
	digamma_me,				/* psi function or digamma */
	rnd_me,					/* Random numbers */
	srand_me,

	drand48_me,
	lrand48_me,
	mrand48_me,
	srand48_me,

	rnd_seed_me,
	rnd_lrand_me,
	rnd_drand_me,
	rnd_iuniform_me,
	rnd_uniform_me,
	rnd_exponential_me,
	rnd_erlang_me,
	rnd_weibull_me,
	rnd_norm_me,
	rnd_normal_me,
	rnd_lognormal_me,
	rnd_triangle_me,
	ceil_me,
	floor_me,
	ldexp_me,
	round_me,
	j0_me,					/* J0(x)   */
	j1_me,					/* J1(x)	  */
	jn_me,					/* JN(n,x) */
	y0_me,					/* y0(x)	  */
	y1_me,					/* y1(x)	  */
	yn_me,					/* YN(n,x) */
	tn_me,					/* TN(n,x) */
			
	erf_me,
	erfc_me,
	lnerfc_me,
	ndtr_me,
	ndtri_me,
	erfi_me,
	erfci_me,
	chi2_me,					/* Chi-squared function						*/
	beta_me,					/* Beta function								*/
	lnbeta_me,				/* Ln(beta) function							*/
	betai_me,				/* Incomplete beta function				*/
	betai_Ix_me,			/* Regularized incomplete beta function */

	fdm0p5_me,				/* Fermi-Dirac order -0.5 */
	fdp0p5_me,				/* Fermi-Dirac order  0.5 */
	fdp1p5_me,				/* Fermi-Dirac order  1.5 */
	fdp2p5_me,				/* Fermi-Dirac order  2.5 */

	z_test_me,				/* Z-test on array w/ known mean/stdev	*/
	t1_test_me,				/* T-test on one array w/ known mean	*/
	t_test_me,				/* A(t|v) v degrees of freedom, t diff	*/
	f_test_me,				/* Q(F|v1,v2) F-Distribution test		*/
	p_chi_me,				/* P(chi2|v) chi-square distribution	*/
	q_chi_me,				/* Q(chi2|v) chi-square distribution	*/

	gauss_me,				/* gauss(x,x0,sigma)   */
	gaussn_me,				/* gaussn(x,x0,sigma)  */
	poisson_me,				/* poisson(x,x0)       */
	lorentz_me,				/* lorentz(x,x0,width) */
	binomial_me,			/* binomial(x,n,p)	  */
	edgeworth_me,			/* edgeworth(x,x0,sigma,skew,kurt) */
	weibull_me,				/* weibull(x,gamma,eta,beta) */
	pearson_IV,				/* Currently undefined */
	pearson_VI,				/* Currently undefined */
	gnoise_me,
	pnoise_me,

	spline_me,
	ispln_me,
	dspln_me,
	ddspln_me,
	hv_me,
	poly_me,
	dpoly_me,
	complex_poly_me,
	cheby_me,

	time_me,
	clock_me,
	ctime_me,
	timer_me,
	strftime_me,
	rgb_me,
	rainbow_me,

	rgb_color_me,
	strcmp_me,				/* Comparsion - returns -1,0,+1	*/
	stricmp_me,				/* Case independent strcmp			*/
	strncmp_me,				/* Compare only first n chars		*/
	strnicmp_me,			/* Case independent strncmp		*/
	strlen_me,				/* Full length of a string			*/
	strnlen_me,				/* Length w/o trailing white sp	*/
	strcspn_me,				/* First element matching any		*/
	strspn_me,				/* First element matching none	*/
	strcoll_me,				/* Collating comparison				*/
	strchr_me,				/* Where string has a char			*/
	strrchr_me,				/* Last point where str has char	*/
	lexequal_me,			/* LexEqual function					*/

	rexx_words,
	rexx_wordindex,
	rexx_wordlength,
	rexx_compare,
	rexx_xrange,
	rexx_space,
	rexx_delstr,
	rexx_delword,
	rexx_insert,
	rexx_overlay,
	rexx_justify,
	rexx_pos,
	rexx_lastpos,
	rexx_wordpos,
	rexx_abbrev,
	rexx_verify,

	rexx_upcase,
	rexx_lowercase,
	rexx_subword,
	rexx_word,
	rexx_translate,
	rexx_substr,
	rexx_strip,
	rexx_reverse,
	rexx_left,
	rexx_right,
	rexx_center,
	rexx_concat,
	rexx_char,
	rexx_copies,
	rexx_ichar,
	
/* ctype.h functions */
	ctype_isalnum,
	ctype_isalpha,
	ctype_iscntrl,
	ctype_isdigit,
	ctype_isgraph,
	ctype_islower,
	ctype_isprint,
	ctype_ispunct,
	ctype_isspace,
	ctype_isupper,
	ctype_isxdigit,
	ctype_tolower,
	ctype_toupper,

/* Was rexx_d2x and rexx_x2d for hex conversion */
	int2hex_me,					/* Hexadecimal conversion */
	hex2int_me,
	oct2int_me,					/* Octal conversion */
	int2oct_me,
	int2bin_me,					/* Binary conversion */
	bin2int_me,
	base2int_me,				/* Arbitrary base */
	int2base_me,

	hex2bin_me,					/* String level conversion */
	bin2hex_me,

	float2hex_me,				/* Internal structure changes */
	hex2float_me,
	double2hex_me,
	hex2double_me,
	time2double_me,			/* Deal with time structures */

	atof_me,
	atoi_me,
	isatoi_me,
	isatof_me,
	strtol_me,
	pwd_me,
	getenv_me,

	array_index,		/* Index of first point >= given value */
	solve_me,			/* Zero of a function						*/
	dydx_me,				/* Numeric derivative of a function		*/
	integrate_me,		/* Numeric integral of a function		*/
	sum_me,				/* Numeric sum of a function				*/
	prod_me,				/* Numeric product of a function			*/

	array_min,
	array_max,
	array_sum,
	array_avg,
	array_count,
	array_median,
	array_mad,
	array_var,
	array_covar,
	array_std,
	array_sdom,
	array_skew,
	array_kurt,
	array_rms,
	array_span,
	array_absmin,
	array_absmax,
	array_abssum,
	array_absavg,

/* Versions including sigma weighting of points */
	array_weight_avg,
	array_weight_sigma,
	array_weight_var,
	array_weight_std,
	array_weight_sdom,
	array_weight_absavg,
	curve_integral,
	curve_correlate,
	curve_pintegral,
	curve_avg,
	curve_median,
	curve_var,
	curve_std,
	curve_skew,
	curve_kurt,
	curve_near,
	curve_3d_near,
	surf_interp,
	surf_integral,
		
	file_sizeof_me,
	file_dateof_me,
	file_isfile_me,
	file_isdir_me,
	fullpath_me,

	fopen_me,				/*  File open command				*/
	fclose_me,				/*  File close command				*/
	feof_me,					/*  Return if at end of stream	*/
	ferror_me,				/*  Return last error of stream	*/
	fflush_me,				/*  Flush the file handle buffer	*/
	ftell_me,
	fseek_me,
	fgetc_me,				/*  Get one character command		*/
	fgets_me,				/*  Return next line	as string	*/
	fputc_me,
	fputs_me,
	sprintf_me,				/*  This is tough!					*/
	fprintf_me,				/*  Not so bad with sprintf		*/
	printf_me,				/*  Output to console				*/
	popen_me,				/*  Pipe open command				*/
	pclose_me,				/*  Pipe close command				*/

/* Low level IO functions	*/
#ifdef NT
	open_me,					/* Open file descriptor				*/
	creat_me,				/* Create file descriptor			*/
	close_me,				/* Close file descriptor			*/
	read_me,					/* Read from file descriptor		*/
	write_me,				/* Write to file descriptor		*/
	query_me,				/* Write/read to file desriptor	*/
	lseek_me,				/* Seek within file descriptor	*/
	eof_me,					/* End of file test					*/
	tell_me,					/* Tell file position				*/
	open_comx_me,			/* Open com port as a fd			*/
	set_baud_me,			/* Set last comx open baud rates	*/
	get_baud_me,			/* Get last comx open baud rates	*/
	set_timeout_me,		/* Set last comx open timeouts	*/
	get_timeout_me,		/* Get last comx open timeouts	*/
	beep_me,					/* Generate a tone					*/
#endif

/* The OS functions return 0 if successful, -1 if fail */
	chdir_me,				/* Change working directory		  */
	mkdir_me,				/* Create a directory via _mkdir() */
	rmdir_me,				/* Remove a directory via _rmdir() */
	rm_me,					/* Remove a file via remove()      */
	unlink_me,				/* Unlink a file via unlink()      */
	mv_me,					/* Move a file via _move()			  */

/* WARNING: Sequential order of "or", "and", "xor" must stay.  See gvcalc.c */
	bin_or_me,				/* Binary character or operation		*/
	bin_and_me,				/* Binary character and operation	*/
	bin_xor_me,				/* Binary character xor operation	*/
	hex_or_me,				/* Hex character or operation			*/
	hex_and_me,				/* Hex character and operation		*/
	hex_xor_me,				/* Hex character xor operation		*/

/* String functions */
	lex_get_token,			/*  Get a string token from the command line */
	lex_get_token_p,		/*  Get a string token from command line, prompt if necessary */
	lex_chk_token,			/*  Check what the next token on the line is */

	math_cmd_end }			/* End of the list */
MATH_CMDS;


/* List of the math commands so can print as "english" for debugging */
static struct _MATH_CMD_HELP {
	MATH_CMDS cmd;
	char *text;
} math_cmd_help[] = {
	{load_immed_real,						"load_immed_real"},
 	{load_immed_imag,						"load_immed_imag"},
	{add_me,									"add"},
	{sub_me,									"sub"},
	{mul_me,									"mul"},
	{div_me,									"div"},
	{pow_me,									"pow"},
	{lt_me,									"lt"},
	{le_me,									"le"},
	{ne_me,									"ne"},
	{gt_me,									"gt"},
	{ge_me,									"ge"},
	{eq_me,									"eq"},
	{not_me,									"not"},
	{and_me,									"and"},
	{or_me,									"or"},
	{eqv_me,									"eqv"},
	{neqv_me,								"neqv"},
	{bit_and_me,							"bit_and"},
	{bit_or_me,								"bit_or"},
	{bit_eor_me,							"bit_eor"},
	{bit_not_me,							"bit_not"},
	{load_e,									"load_e"},
	{load_pi,								"load_pi"},
	{load_dummy_value,					"load_dummy_value"},
	{load_realmin,							"load_realmin"},
	{load_realmax,							"load_realmax"},
	{load_i,									"load_i"},
	{load_j,									"load_j"},
	{load_stack_val,						"load_stack_val"},
	{save_stack_val,						"save_stack_val"},
	{pop_stack,								"pop_stack"},
	{load_string_stack_val,				"load_string_stack_val"},
	{save_string_stack_val,				"save_string_stack_val"},
	{pop_string_stack,					"pop_string_stack"},
	{interp_string_stack_val,			"interp_string_stack_val"},
	{conditional_me,						"conditional"},
	{jump_me,								"jump"},
	{xeq_function,							"xeq_function"},
	{xeq_function_a,						"xeq_function_a"},
	{xeq_str_function,					"xeq_str_function"},
	{load_real,								"load_real"},
	{load_double,							"load_double"},
	{load_complex,							"load_complex"},
	{load_int,								"load_int"},
	{load_real_array,						"load_real_array"},
	{load_double_array,					"load_double_array"},
	{load_complex_array,					"load_complex_array"},
	{load_int_array,						"load_int_array"},
	{load_real_idx,						"load_real_idx"},
	{load_double_idx,						"load_double_idx"},
	{load_complex_idx,					"load_complex_idx"},
	{load_int_idx,							"load_int_idx"},
	{load_string_idx,						"load_string_idx"},
	{load_string_tmp,						"load_string_tmp"},
	{load_string_ptr,						"load_string_ptr"},
	{load_strarray_idx,					"load_strarray_idx"},
	{load_surface_pt,						"load_surface_pt"},
	{load_pfile_ptr,						"load_pfile_ptr"},
	{load_varentry_ptr,					"load_varentry_ptr"},
	{chs_me,									"chs"},
	{abs_me,									"abs"},
	{sign_me,								"sign"},
	{min_me,									"min"},
	{max_me,									"max"},
	{ave_me,									"ave"},
	{std_me,									"std"},
	{sdom_me,								"sdom"},
	{median_me,								"median"},
	{mad_me,									"mad"},
	{count_me,								"count"},
	{limit_me,								"limit"},
	{int_me,									"int"},
	{nint_me,								"nint"},
	{frac_me,								"frac"},
	{mod_me,									"mod"},
	{mantissa_me,							"mantissa"},
	{exponent_me,							"exponent"},
	{m1n_me,									"m1n"},
	{real_me,								"real"},
	{imag_me,								"imag"},
	{conj_me,								"conj"},
	{arg_me,									"arg"},
	{sin_me,									"sin"},
	{cos_me,									"cos"},
	{tan_me,									"tan"},
	{cot_me,									"cot"},
	{sind_me,								"sind"},
	{cosd_me,								"cosd"},
	{tand_me,								"tand"},
	{cotd_me,								"cotd"},
	{asin_me,								"asin"},
	{acos_me,								"acos"},
	{atan_me,								"atan"},
	{acot_me,								"acot"},
	{atan2_me,								"atan2"},
	{asind_me,								"asind"},
	{acosd_me,								"acosd"},
	{atand_me,								"atand"},
	{acotd_me,								"acotd"},
	{atan2d_me,								"atan2d"},
	{sinh_me,								"sinh"},
	{cosh_me,								"cosh"},
	{tanh_me,								"tanh"},
	{coth_me,								"coth"},
	{sech_me,								"sech"},
	{csch_me,								"csch"},
	{asinh_me,								"asinh"},
	{acosh_me,								"acosh"},
	{atanh_me,								"atanh"},
	{acoth_me,								"acoth"},
	{asech_me,								"asech"},
	{acsch_me,								"acsch"},
	{ln_me,									"ln"},
	{log_me,									"log"},
	{exp_me,									"exp"},
	{sqrt_me,								"sqrt"},
	{fact_me,								"fact"},
	{gamma_me,								"gamma"},
	{lngamma_me,							"lngamma"},
	{digamma_me,							"digamma"},
	{rnd_me,									"rnd"},
	{srand_me,								"srand"},
	{drand48_me,							"drand48"},
	{lrand48_me,							"lrand48"},
	{mrand48_me,							"mrand48"},
	{srand48_me,							"srand48"},
	{rnd_seed_me,							"rnd_seed"},
	{rnd_lrand_me,							"rnd_lrand"},
	{rnd_drand_me,							"rnd_drand"},
	{rnd_iuniform_me,						"rnd_iuniform"},
	{rnd_uniform_me,						"rnd_uniform"},
	{rnd_exponential_me,					"rnd_exponential"},
	{rnd_erlang_me,						"rnd_erlang"},
	{rnd_weibull_me,						"rnd_weibull"},
	{rnd_norm_me,							"rnd_norm"},
	{rnd_normal_me,						"rnd_normal"},
	{rnd_lognormal_me,					"rnd_lognormal"},
	{rnd_triangle_me,						"rnd_triangle"},
	{ceil_me,								"ceil"},
	{floor_me,								"floor"},
	{ldexp_me,								"ldexp"},
	{round_me,								"round"},
	{j0_me,									"j0"},
	{j1_me,									"j1"},
	{jn_me,									"jn"},
	{y0_me,									"y0"},
	{y1_me,									"y1"},
	{yn_me,									"yn"},
	{tn_me,									"tn"},
	{erf_me,									"erf"},
	{erfc_me,								"erfc"},
	{lnerfc_me,								"lnerfc"},
	{ndtr_me,								"ndtr"},
	{ndtri_me,								"ndtri"},
	{erfi_me,								"erfi"},
	{erfci_me,								"erfci"},
	{chi2_me,								"chi2"},
	{beta_me,								"beta"},
	{lnbeta_me,								"lnbeta"},
	{betai_me,								"betai"},
	{betai_Ix_me,							"betai_Ix"},
	{z_test_me,								"z_test"},
	{t1_test_me,							"t1_test"},
	{t_test_me,								"t_test"},
	{f_test_me,								"f_test"},
	{p_chi_me,								"p_chi"},
	{q_chi_me,								"q_chi"},
	{gauss_me,								"gauss"},
	{gaussn_me,								"gaussn"},
	{poisson_me,							"poisson"},
	{lorentz_me,							"lorentz"},
	{binomial_me,							"binomial"},
	{edgeworth_me,							"edgeworth"},
	{weibull_me,							"weibull"},
	{pearson_IV,							"pearson_IV"},
	{pearson_VI,							"pearson_VI"},
	{gnoise_me,								"gnoise"},
	{pnoise_me,								"pnoise"},
	{spline_me,								"spline"},
	{ispln_me,								"ispln"},
	{dspln_me,								"dspln"},
	{ddspln_me,								"ddspln"},
	{hv_me,									"hv"},
	{poly_me,								"poly"},
	{dpoly_me,								"dpoly"},
	{complex_poly_me,						"complex_poly"},
	{cheby_me,								"cheby"},
	{time_me,								"time"},
	{clock_me,								"clock"},
	{ctime_me,								"ctime"},
	{timer_me,								"timer"},
	{strftime_me,							"strftime"},
	{rgb_me,									"rgb"},
	{rainbow_me,							"rainbow"},
	{rgb_color_me,							"rgb_color"},
	{strcmp_me,								"strcmp"},
	{stricmp_me,							"stricmp"},
	{strncmp_me,							"strncmp"},
	{strnicmp_me,							"strnicmp"},
	{strlen_me,								"strlen"},
	{strnlen_me,							"strnlen"},
	{strcspn_me,							"strcspn"},
	{strspn_me,								"strspn"},
	{strcoll_me,							"strcoll"},
	{strchr_me,								"strchr"},
	{strrchr_me,							"strrchr"},
	{lexequal_me,							"lexequal"},
	{rexx_words,							"rexx_words"},
	{rexx_wordindex,						"rexx_wordindex"},
	{rexx_wordlength,						"rexx_wordlength"},
	{rexx_compare,							"rexx_compare"},
	{rexx_xrange,							"rexx_xrange"},
	{rexx_space,							"rexx_space"},
	{rexx_delstr,							"rexx_delstr"},
	{rexx_delword,							"rexx_delword"},
	{rexx_insert,							"rexx_insert"},
	{rexx_overlay,							"rexx_overlay"},
	{rexx_justify,							"rexx_justify"},
	{rexx_pos,								"rexx_pos"},
	{rexx_lastpos,							"rexx_lastpos"},
	{rexx_wordpos,							"rexx_wordpos"},
	{rexx_abbrev,							"rexx_abbrev"},
	{rexx_verify,							"rexx_verify"},
	{rexx_upcase,							"rexx_upcase"},
	{rexx_lowercase,						"rexx_lowercase"},
	{rexx_subword,							"rexx_subword"},
	{rexx_word,								"rexx_word"},
	{rexx_translate,						"rexx_translate"},
	{rexx_substr,							"rexx_substr"},
	{rexx_strip,							"rexx_strip"},
	{rexx_reverse,							"rexx_reverse"},
	{rexx_left,								"rexx_left"},
	{rexx_right,							"rexx_right"},
	{rexx_center,							"rexx_center"},
	{rexx_concat,							"rexx_concat"},
	{rexx_char,								"rexx_char"},
	{rexx_copies,							"rexx_copies"},
	{rexx_ichar,							"rexx_ichar"},
	{ctype_isalnum,						"ctype_isalnum"},
	{ctype_isalpha,						"ctype_isalpha"},
	{ctype_iscntrl,						"ctype_iscntrl"},
	{ctype_isdigit,						"ctype_isdigit"},
	{ctype_isgraph,						"ctype_isgraph"},
	{ctype_islower,						"ctype_islower"},
	{ctype_isprint,						"ctype_isprint"},
	{ctype_ispunct,						"ctype_ispunct"},
	{ctype_isspace,						"ctype_isspace"},
	{ctype_isupper,						"ctype_isupper"},
	{ctype_isxdigit,						"ctype_isxdigit"},
	{ctype_tolower,						"ctype_tolower"},
	{ctype_toupper,						"ctype_toupper"},
	{int2hex_me,							"int2hex"},
	{hex2int_me,							"hex2int"},
	{oct2int_me,							"oct2int"},
	{int2oct_me,							"int2oct"},
	{int2bin_me,							"int2bin"},
	{bin2int_me,							"bin2int"},
	{base2int_me,							"base2int"},
	{int2base_me,							"int2base"},
	{hex2bin_me,							"hex2bin"},
	{bin2hex_me,							"bin2hex"},
	{float2hex_me,							"float2hex"},
	{hex2float_me,							"hex2float"},
	{double2hex_me,						"double2hex"},
	{hex2double_me,						"hex2double"},
	{time2double_me,						"time2double"},
	{atof_me,								"atof"},
	{atoi_me,								"atoi"},
	{isatoi_me,								"isatoi"},
	{isatof_me,								"isatof"},
	{strtol_me,								"strtol"},
	{pwd_me,									"pwd"},
	{getenv_me,								"getenv"},
	{array_index,							"array_index"},
	{solve_me,								"solve"},
	{dydx_me,								"dydx"},
	{integrate_me,							"integrate"},
	{sum_me,									"sum"},
	{prod_me,								"prod"},
	{array_min,								"array_min"},
	{array_max,								"array_max"},
	{array_sum,								"array_sum"},
	{array_avg,								"array_avg"},
	{array_count,							"array_count"},
	{array_median,							"array_median"},
	{array_mad,								"array_mad"},
	{array_var,								"array_var"},
	{array_covar,							"array_covar"},
	{array_std,								"array_std"},
	{array_sdom,							"array_sdom"},
	{array_skew,							"array_skew"},
	{array_kurt,							"array_kurt"},
	{array_rms,								"array_rms"},
	{array_span,							"array_span"},
	{array_absmin,							"array_absmin"},
	{array_absmax,							"array_absmax"},
	{array_abssum,							"array_abssum"},
	{array_absavg,							"array_absavg"},
	{array_weight_avg,					"array_weight_avg"},
	{array_weight_sigma,					"array_weight_sigma"},
	{array_weight_var,					"array_weight_var"},
	{array_weight_std,					"array_weight_std"},
	{array_weight_sdom,					"array_weight_sdom"},
	{array_weight_absavg,				"array_weight_absavg"},
	{curve_integral,						"curve_integral"},
	{curve_correlate,						"curve_correlate"},
	{curve_pintegral,						"curve_pintegral"},
	{curve_near,							"curve_near"},
	{curve_3d_near,						"curve_3d_near"},
	{surf_interp,							"surf_interp"},
	{surf_integral,						"surf_integral"},
	{file_sizeof_me,						"file_sizeof"},
	{file_dateof_me,						"file_dateof"},
	{file_isfile_me,						"file_isfile"},
	{file_isdir_me,						"file_isdir"},
	{fullpath_me,							"fullpath"},
	{fopen_me,								"fopen"},
	{fclose_me,								"fclose"},
	{feof_me,								"feof"},
	{ferror_me,								"ferror"},
	{fflush_me,								"fflush"},
	{ftell_me,								"ftell"},
	{fseek_me,								"fseek"},
	{fgetc_me,								"fgetc"},
	{fgets_me,								"fgets"},
	{fputc_me,								"fputc"},
	{fputs_me,								"fputs"},
	{sprintf_me,							"sprintf"},
	{fprintf_me,							"fprintf"},
	{printf_me,								"printf"},
	{popen_me,								"popen"},
	{pclose_me,								"pclose"},
#ifdef NT
	{open_me,								"open"},
	{creat_me,								"creat"},
	{close_me,								"close"},
	{read_me,								"read"},
	{write_me,								"write"},
	{query_me,								"query"},
	{lseek_me,								"lseek"},
	{eof_me,									"eof"},
	{tell_me,								"tell"},
	{open_comx_me,							"open_comx"},
	{set_baud_me,							"set_baud"},
	{get_baud_me,							"get_baud"},
	{set_timeout_me,						"set_timeout"},
	{get_timeout_me,						"get_timeout"},
	{beep_me,								"beep"},
#endif
	{chdir_me,								"chdir"},
	{mkdir_me,								"mkdir"},
	{rmdir_me,								"rmdir"},
	{rm_me,									"rm"},
	{unlink_me,								"unlink"},
	{mv_me,									"mv"},
	{bin_or_me,								"bin_or"},
	{bin_and_me,							"bin_and"},
	{bin_xor_me,							"bin_xor"},
	{hex_or_me,								"hex_or"},
	{hex_and_me,							"hex_and"},
	{hex_xor_me,							"hex_xor"},
	{lex_get_token,						"lex_get_token"},
	{lex_get_token_p,						"lex_get_token_p"},
	{lex_chk_token,						"lex_chk_token"},
	{math_cmd_end,							"math_cmd_end"}
};
