#ifndef CALC_H
#define CALC_H 1

#include "ruby.h"

/* cannot include calc/calc.h, which contains some things we need, because it
 * includes calc/value.h which defines VALUE, a name already used by ruby.
 * copying things we need here for now. */
extern void libcalc_call_me_first(void);
extern void reinitialize(void);

#include <calc/cmath.h>
#include <calc/lib_calc.h>

/* convert.c */
extern ZVALUE value_to_zvalue(VALUE arg, int string_allowed);
extern NUMBER *value_to_number(VALUE arg, int string_allowed);
extern double zvalue_to_double(ZVALUE *z);
extern VALUE zvalue_to_f(ZVALUE *z);
extern VALUE zvalue_to_i(ZVALUE *z);

/* math_error.c */
extern VALUE e_MathError;
extern void define_calc_math_error();

#ifdef JUMP_ON_MATH_ERROR
extern void setup_math_error();
#else
#define setup_math_error()
#endif

/* integer.c */
extern const rb_data_type_t calc_z_type;

extern void define_calc_z(VALUE m);

/* rational.h */
extern const rb_data_type_t calc_q_type;
extern VALUE cQ;

extern void define_calc_q(VALUE m);

/*** macros ***/

/* test ruby values match our TypedData classes */
#define ISZVALUE(v) (rb_typeddata_is_kind_of((v), &calc_z_type))
#define ISQVALUE(v) (rb_typeddata_is_kind_of((v), &calc_q_type))

/* shortcut for getting pointer to Calc::Z's ZVALUE */
#define get_zvalue(ruby_var,c_var) { TypedData_Get_Struct(ruby_var, ZVALUE, &calc_z_type, c_var); }

#endif                          /* CALC_H */
