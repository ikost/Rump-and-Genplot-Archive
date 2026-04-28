/*  stopping.h */

/*    Pulled out from rumpdata.h  12/5/93  LRD */

/* ===========================================================================
-- STOPPING POWER routines and definitions.  
--
-- To first order, RUMP uses a polynomial description of the stopping power,
-- with a power level of 6 (NDEG).
-- 
-- IMPORTANT NOTE: not all references to NDEG can be efficiently done
-- in terms of the parameter.  Any change in the value of NDEG must
-- be accompanied by changes to the subsequent macros as well!
=========================================================================== */
#if NDEG != 6
	#error Reality check! Stopping power definition disagreement!
#endif

/* Macros for handling stopping power calculations.
-- S_XFORM   - form to pass energy in to S_POWER, DS_POWER, DDS_POWER
-- S_IXFORM  - inverse of S_XFORM
-- S_POWER   - stopping power at given energy with STOPPING_POWER entry
-- DS_POWER  - derivative of the stopping power
-- DDS_POWER - second derivative of the stopping power
*/

/* ---------------------------------------------------------------------------
-- For e=sqrt(E) parameterized stopping powers, the general derivatives are:
--           N                1     N                   1     N
--  S(e) = SUM a  e^i   DS = ---- SUM (i)a e^i   DDS = ---- SUM (i)(i-2)a e^i
--           0  i            2e^2   0     i            4e^4   0          i
---------------------------------------------------------------------------- */
/* Coefficients fit to sqrt(E) as independent variable */
	#define SQRT_S_XFORM(e)    sqrt(e)
	#define SQRT_S_IXFORM(x)   pow(x,2)
	#define SQRT_S_POWER(table,e)    (((((((table)->p[5])*(e) +   (table)->p[4])*(e) + \
												 (table)->p[3])*(e) +   (table)->p[2])*(e) + \
												 (table)->p[1])*(e) +   (table)->p[0])
	#define SQRT_DS_POWER(table,e) ((((((2.5*(table)->p[5])*(e) + 2*(table)->p[4])*(e) + \
	                                1.5*(table)->p[3])*(e) +   (table)->p[2])*(e) + \
	                                0.5*(table)->p[1])/(e))
	#define SQRT_DDS_POWER(table,e) (((((3.75*(table)->p[5])*(e) +2*(table)->p[4])*(e) + \
	                                0.75*(table)->p[3])*(e)*(e)                   - \
	                                0.25*(table)->p[2])/  ((e)*(e)*(e)))
/* Coefficients fit to (E) as independent variable */
	#define LINE_S_XFORM(e)    (e)
	#define LINE_S_IXFORM(x)   (x)
	#define LINE_S_POWER(table,e)    (((((((table)->p[5])*(e) +   (table)->p[4])*(e) + \
												 (table)->p[3])*(e) +   (table)->p[2])*(e) + \
												 (table)->p[1])*(e) +   (table)->p[0])
	#define LINE_DS_POWER(table,e)  (((((5*(table)->p[5])*(e) + 4*(table)->p[4])*(e) + \
	                                3*(table)->p[3])*(e) + 2*(table)->p[2])*(e) + \
	                                  (table)->p[1])
	#define LINE_DDS_POWER(table,e) ((((20*(table)->p[5])*(e) +12*(table)->p[4])*(e) + \
	                                6*(table)->p[3])*(e) + 2*(table)->p[2])

/* Note S_POWER same for LINEAR and SQRT, so no need for if form */
#define S_XFORM(e)			( (stop_type == STOP_LINEAR) ? (LINE_S_XFORM(e))  : (SQRT_S_XFORM(e)) )
#define S_IXFORM(x)			( (stop_type == STOP_LINEAR) ? (LINE_S_IXFORM(x)) : (SQRT_S_IXFORM(x)) )
#define S_POWER(table,e)	LINE_S_POWER(table,e)
#define DS_POWER(table,e)	( (stop_type == STOP_LINEAR) ? (LINE_DS_POWER(table,e)) : (SQRT_DS_POWER(table,e)) )
#define DDS_POWER(table,e) ( (stop_type == STOP_LINEAR) ? (LINE_DDS_POWER(table,e)) : (SQRT_DDS_POWER(table,e)) )
