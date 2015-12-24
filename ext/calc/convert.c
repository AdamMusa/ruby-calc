#include "calc.h"

/* attempt to convert a bignum a long then to Calc::Z using itoz().
 * NUM2LONG will raise an exception if arg doesn't fit in a long */
static VALUE
bignum_to_calc_z_via_long(VALUE arg)
{
    VALUE result;
    ZVALUE *zresult;

    result = cz_new();
    get_zvalue(result, zresult);
    itoz(NUM2LONG(arg), zresult);
    return result;
}

/* handles exceptions during via_long.  convert the arg to a string, then
 * Calc::Z using str2z */
static VALUE
bignum_to_calc_z_via_string(VALUE arg, VALUE e)
{
    ZVALUE *zresult;
    VALUE result;
    VALUE string;

    if (rb_obj_is_kind_of(e, rb_eRangeError)) {
        result = cz_new();
        get_zvalue(result, zresult);
        string = rb_funcall(arg, rb_intern("to_s"), 0);
        str2z(StringValueCStr(string), zresult);
    }
    else {
        /* something other than RangeError; re-raise it */
        rb_exc_raise(e);
    }

    return result;
}

/* convert a Bignum to Calc::Z
 * first tries to convert via a long; if that raises an exception, convert via
 * a string */
static VALUE
bignum_to_calc_z(VALUE arg)
{
    return rb_rescue(&bignum_to_calc_z_via_long, arg, &bignum_to_calc_z_via_string, arg);
}

/* converts a ruby value into a ZVALUE.  Allowed types:
 *  - Fixnum
 *  - Bignum
 *  - Calc::Z
 *  - String (using libcalc str2z)
 */
ZVALUE
value_to_zvalue(VALUE arg, int string_allowed)
{
    ZVALUE *zarg, *ztmp;
    ZVALUE result;
    VALUE tmp;
    setup_math_error();

    if (TYPE(arg) == T_FIXNUM) {
        itoz(NUM2LONG(arg), &result);
    }
    else if (TYPE(arg) == T_BIGNUM) {
        tmp = bignum_to_calc_z(arg);
        get_zvalue(tmp, ztmp);
        zcopy(*ztmp, &result);
    }
    else if (ISZVALUE(arg)) {
        get_zvalue(arg, zarg);
        zcopy(*zarg, &result);
    }
    else if (string_allowed && TYPE(arg) == T_STRING) {
        str2z(StringValueCStr(arg), &result);
    }
    else {
        if (string_allowed) {
            rb_raise(rb_eArgError, "expected Fixnum, Bignum, Calc::Z or String");
        }
        else {
            rb_raise(rb_eArgError, "expected Fixnum, Bignum or Calc::Z");
        }
    }

    return result;
}

/* converts a ruby value into a NUMBER*.  Allowed types:
 *  - Fixnum
 *  - Bignum (has to fit in a long)
 *  - Calc::Z
 *  - Calc::Q
 *  - Rational
 *  - String (using libcalc str2q)
 *  - Float (will be converted to a Rational first)
 *
 * the caller is responsible for freeing the returned number.  storing it in
 * a Calc::Q is sufficient for the ruby GC to get it.
 */
NUMBER *
value_to_number(VALUE arg, int string_allowed)
{
    NUMBER *qresult;
    ZVALUE *zarg;
    VALUE tmp;
    setup_math_error();

    if (TYPE(arg) == T_FIXNUM || TYPE(arg) == T_BIGNUM) {
        qresult = itoq(NUM2LONG(arg));
    }
    else if (ISZVALUE(arg)) {
        get_zvalue(arg, zarg);
        qresult = qalloc();
        zcopy(*zarg, &qresult->num);
    }
    else if (ISQVALUE(arg)) {
        qresult = qlink((NUMBER *) DATA_PTR(arg));
    }
    else if (TYPE(arg) == T_RATIONAL) {
        qresult = iitoq(NUM2LONG(rb_funcall(arg, rb_intern("numerator"), 0)),
                        NUM2LONG(rb_funcall(arg, rb_intern("denominator"), 0)));
    }
    else if (TYPE(arg) == T_FLOAT) {
        tmp = rb_funcall(arg, rb_intern("to_r"), 0);
        qresult = iitoq(NUM2LONG(rb_funcall(tmp, rb_intern("numerator"), 0)),
                        NUM2LONG(rb_funcall(tmp, rb_intern("denominator"), 0)));
    }
    else if (string_allowed && TYPE(arg) == T_STRING) {
        qresult = str2q(StringValueCStr(arg));
        /* libcalc str2q allows a 0 denominator */
        if (ziszero(qresult->den)) {
            qfree(qresult);
            rb_raise(rb_eZeroDivError, "division by zero");
        }
    }
    else {
        if (string_allowed) {
            rb_raise(rb_eArgError,
                     "expected number, Rational, Float, Calc::Z, Calc::Q or string");
        }
        else {
            rb_raise(rb_eArgError, "expected number, Rational, Float, Calc::Z or Calc::Q");
        }
    }
    return qresult;
}

VALUE
zvalue_to_f(ZVALUE * z)
{
    return rb_funcall(zvalue_to_i(z), rb_intern("to_f"), 0);
}

/* convert a ZVALUE to a ruby numeric (Fixnum or Bignum)
 */
VALUE
zvalue_to_i(ZVALUE * z)
{
    VALUE tmp;
    char *s;

    if (zgtmaxlong(*z)) {
        /* too big to fit in a long, ztoi would return MAXLONG.  use a string
         * intermediary. */
        math_divertio();
        zprintval(*z, 0, 0);
        s = math_getdivertedio();
        tmp = rb_str_new2(s);
        free(s);
        return rb_funcall(tmp, rb_intern("to_i"), 0);
    }
    else {
        return LONG2NUM(ztoi(*z));
    }
}

/* converts a ZVALUE to the nearest double.  libcalc doesn't use floats/doubles
 * at all so the simplest thing to do is convert to a Fixnum/Bignum, then use
 * ruby's Fixnum#to_f.
 */
double
zvalue_to_double(ZVALUE * z)
{
    return NUM2DBL(zvalue_to_i(z));
}
