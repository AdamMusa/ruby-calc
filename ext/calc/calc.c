#include "calc.h"

void Init_calc(void)
{
    VALUE m;
    libcalc_call_me_first();

    m = rb_define_module("Calc");
    define_calc_z(m);
}
