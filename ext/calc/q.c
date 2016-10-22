#include "calc.h"

/* Document-class: Calc::Q
 *
 * Calc rational number (fraction).
 *
 * A rational number consists of an arbitrarily large numerator and
 * denominator.  The numerator and denominator are always in lowest terms, and
 * the sign of the number is contained in the numerator.
 *
 * Wraps the libcalc C type NUMBER*.
 */
VALUE cQ;

/*****************************************************************************
 * functions related to memory allocation and object initialization          *
 *****************************************************************************/

static ID id_add;
static ID id_and;
static ID id_coerce;
static ID id_divide;
static ID id_multiply;
static ID id_new;
static ID id_or;
static ID id_spaceship;
static ID id_subtract;
static ID id_xor;

void
cq_free(void *p)
{
    qfree((NUMBER *) p);
}

const rb_data_type_t calc_q_type = {
    "Calc::Q",
    {0, cq_free, 0},            /* TODO: 3rd param is optional dsize */
    0, 0
#ifdef RUBY_TYPED_FREE_IMMEDATELY
        , RUBY_TYPED_FREE_IMMEDIATELY   /* flags is in 2.1+ */
#endif
};

/* because the true type of rationals in calc is (NUMBER*), we don't do any
 * additional memory allocation in cq_alloc.  the 'data' elemnt of underlying
 * RTypedData struct is accessed directly via the DATA_PTR macro.
 *
 * DATA_PTR isn't documented, but it is used by some built in ruby ext libs.
 *
 * the data element can be replaced by assining to the DATA_PTR macro.  be
 * careful to free any existing value before replacing (most qmath.c functions
 * actually allocate a new NUMBER and return a pointer to it).
 */

/* no additional allocation beyond normal ruby alloc is required */
VALUE
cq_alloc(VALUE klass)
{
    return TypedData_Wrap_Struct(klass, &calc_q_type, 0);
}

/* Creates a new rational number.
 *
 * Arguments are either a numerator/denominator pair, or a single numerator.
 * With a single parameter, a denominator of 1 is implied.  Valid types are:
 * * Fixnum
 * * Bignum
 * * Rational
 * * Calc::Q
 * * String
 * * Float
 *
 * Strings can be in rational, floating point, exponential, hex or octal, eg:
 *   Calc::Q("3/10")   #=> Calc::Q(0.3)
 *   Calc::Q("0.5")    #=> Calc::Q(0.5)
 *   Calc::Q("1e10")   #=> Calc::Q(10000000000)
 *   Calc::Q("1e-10")  #=> Calc::Q(0.0000000001)
 *   Calc::Q("0x2a")   #=> Calc::Q(42)
 *   Calc::Q("052")    #=> Calc::Q(42)
 *
 * Note that a Float cannot precisely equal many values; it will be converted
 * the the closest rational number which may not be what you expect, eg:
 *   Calc::Q(0.3)  #=> Calc::Q(~0.29999999999999998890)
 * for this reason, it is best to avoid Floats.  Libcalc's string parsing will
 * work better:
 *   Calc::Q("0.3")  #=> Calc::Q(0.3)
 *
 * @param num [Numeric,Calc::Q,String]
 * @param den [Numeric,Calc::Q,String] (optional)
 * @return [Calc::Q]
 * @raise [ZeroDivisionError] if denominator of new number is zero
 */
static VALUE
cq_initialize(int argc, VALUE * argv, VALUE self)
{
    NUMBER *qself, *qnum, *qden;
    VALUE num, den;
    setup_math_error();

    if (rb_scan_args(argc, argv, "11", &num, &den) == 1) {
        /* single param */
        qself = value_to_number(num, 1);
    }
    else {
        /* 2 params. divide first by second. */
        qden = value_to_number(den, 1);
        if (qiszero(qden)) {
            qfree(qden);
            rb_raise(rb_eZeroDivError, "division by zero");
        }
        qnum = value_to_number(num, 1);
        qself = qqdiv(qnum, qden);
        qfree(qden);
        qfree(qnum);
    }
    DATA_PTR(self) = qself;

    return self;
}

static VALUE
cq_initialize_copy(VALUE obj, VALUE orig)
{
    NUMBER *qorig, *qobj;

    if (obj == orig) {
        return obj;
    }
    if (!CALC_Q_P(orig)) {
        rb_raise(rb_eTypeError, "wrong argument type");
    }

    qorig = DATA_PTR(orig);
    qobj = qlink(qorig);
    DATA_PTR(obj) = qobj;

    return obj;
}

/*****************************************************************************
 * private functions used by instance methods                                *
 *****************************************************************************/

static VALUE
numeric_op(VALUE self, VALUE other,
           NUMBER * (*fqq) (NUMBER *, NUMBER *), NUMBER * (*fql) (NUMBER *, long), ID func)
{
    NUMBER *qother, *qresult;
    VALUE ary;
    setup_math_error();

    if (fql && TYPE(other) == T_FIXNUM) {
        qresult = (*fql) (DATA_PTR(self), NUM2LONG(other));
    }
    else if (CALC_Q_P(other)) {
        qresult = (*fqq) (DATA_PTR(self), DATA_PTR(other));
    }
    else if (TYPE(other) == T_FIXNUM || TYPE(other) == T_BIGNUM || TYPE(other) == T_FLOAT
             || TYPE(other) == T_RATIONAL) {
        qother = value_to_number(other, 0);
        qresult = (*fqq) (DATA_PTR(self), qother);
        qfree(qother);
    }
    else if (rb_respond_to(other, id_coerce)) {
        if (TYPE(other) == T_COMPLEX) {
            other = rb_funcall(cC, id_new, 1, other);
        }
        ary = rb_funcall(other, id_coerce, 1, self);
        if (!RB_TYPE_P(ary, T_ARRAY) || RARRAY_LEN(ary) != 2) {
            rb_raise(rb_eTypeError, "coerce must return [x, y]");
        }
        return rb_funcall(RARRAY_AREF(ary, 0), func, 1, RARRAY_AREF(ary, 1));
    }
    else {
        rb_raise(rb_eTypeError,
                 "%" PRIsVALUE " (%" PRIsVALUE ") can't be coerced into %" PRIsVALUE, other,
                 rb_obj_class(other), rb_obj_class(self));
    }
    return wrap_number(qresult);
}

static VALUE
trans_function(int argc, VALUE * argv, VALUE self, NUMBER * (*f) (NUMBER *, NUMBER *),
               COMPLEX * (*fcomplex) (COMPLEX *, NUMBER *))
{
    NUMBER *qepsilon, *qresult;
    COMPLEX *cself, *cresult;
    VALUE epsilon, result;
    setup_math_error();

    if (rb_scan_args(argc, argv, "01", &epsilon) == 0) {
        qepsilon = NULL;
    }
    else {
        qepsilon = value_to_number(epsilon, 1);
    }
    qresult = (*f) (DATA_PTR(self), qepsilon ? qepsilon : conf->epsilon);
    if (qresult) {
        result = wrap_number(qresult);
    }
    else if (fcomplex) {
        /* non-real result, call complex version.  see calc's func.c */
        cself = comalloc();
        qfree(cself->real);
        cself->real = qlink((NUMBER *) DATA_PTR(self));
        cresult = (*fcomplex) (cself, qepsilon ? qepsilon : conf->epsilon);
        comfree(cself);
        if (cresult) {
            result = wrap_complex(cresult);
        }
        else {
            /* Can this happen? */
            rb_raise(e_MathError,
                     "Unhandled NULL from complex version of transcendental function");
        }
    }
    else {
        if (qepsilon) {
            qfree(qepsilon);
        }
        rb_raise(e_MathError, "Unhandled NULL from transcendental function");
    }
    if (qepsilon) {
        qfree(qepsilon);
    }
    return result;
}

/* same as trans_function(), except for functions where there are 2 NUMBER*
 * arguments, eg atan2.  the first param is the receiver (self). */
static VALUE
trans_function2(int argc, VALUE * argv, VALUE self,
                NUMBER * (*f) (NUMBER *, NUMBER *, NUMBER *))
{
    NUMBER *qarg, *qepsilon, *qresult;
    VALUE arg, epsilon;
    setup_math_error();

    if (rb_scan_args(argc, argv, "11", &arg, &epsilon) == 1) {
        qarg = value_to_number(arg, 0);
        qresult = (*f) (DATA_PTR(self), qarg, conf->epsilon);
        qfree(qarg);
    }
    else {
        qarg = value_to_number(arg, 0);
        qepsilon = value_to_number(epsilon, 1);
        qresult = (*f) (DATA_PTR(self), qarg, qepsilon);
        qfree(qarg);
        qfree(qepsilon);
    }
    if (!qresult) {
        rb_raise(e_MathError, "Transcendental function returned NULL");
    }
    return wrap_number(qresult);
}

static VALUE
rounding_function(int argc, VALUE * argv, VALUE self, NUMBER * (f) (NUMBER *, long, long))
{
    VALUE places, rnd;
    long n, p, r;
    setup_math_error();

    n = rb_scan_args(argc, argv, "02", &places, &rnd);
    p = (n >= 1) ? value_to_long(places) : 0;
    r = (n == 2) ? value_to_long(rnd) : conf->round;
    return wrap_number((*f) (DATA_PTR(self), p, r));
}

static VALUE
cand_navigation(int argc, VALUE * argv, VALUE self,
                BOOL(f) (ZVALUE, long, ZVALUE, ZVALUE, ZVALUE, ZVALUE *))
{
    VALUE count, skip, residue, modulus;
    NUMBER *qself, *qcount, *qskip, *qresidue, *qmodulus, *qresult;
    ZVALUE tmp;
    int n;
    const char *error = NULL;
    setup_math_error();

    n = rb_scan_args(argc, argv, "04", &count, &skip, &residue, &modulus);
    qself = DATA_PTR(self);
    qcount = (n >= 1) ? value_to_number(count, 1) : qlink(&_qone_);
    qskip = (n >= 2) ? value_to_number(skip, 1) : qlink(&_qone_);
    qresidue = (n >= 3) ? value_to_number(residue, 1) : qlink(&_qzero_);
    qmodulus = (n >= 4) ? value_to_number(modulus, 1) : qlink(&_qone_);
    qresult = NULL;

    if (!qisint(qself) || !qisint(qcount) || !qisint(qskip) || !qisint(qresidue)
        || !qisint(qmodulus)) {
        error = "receiver and all arguments must be integers";
    }
    else if (zge24b(qcount->num)) {
        error = "count must be < 2^24";
    }
    else {
        if ((*f)
            (qself->num, ztoi(qcount->num), qskip->num, qresidue->num, qmodulus->num, &tmp)) {
            qresult = qalloc();
            qresult->num = tmp;
        }
    }

    qfree(qcount);
    qfree(qskip);
    qfree(qresidue);
    qfree(qmodulus);

    if (error) {
        rb_raise(e_MathError, "%s", error);
    }
    else if (qresult) {
        return wrap_number(qresult);
    }
    return Qnil;
}

static VALUE
trunc_function(int argc, VALUE * argv, VALUE self, NUMBER * (f) (NUMBER *, NUMBER *))
{
    VALUE places;
    NUMBER *qplaces, *qresult;
    setup_math_error();

    if (rb_scan_args(argc, argv, "01", &places) == 1) {
        qplaces = value_to_number(places, 0);
        qresult = (*f) (DATA_PTR(self), qplaces);
        qfree(qplaces);
    }
    else {
        qresult = (*f) (DATA_PTR(self), &_qzero_);
    }
    return wrap_number(qresult);
}

/*****************************************************************************
 * instance method implementations                                           *
 *****************************************************************************/

/* Bitwise AND
 *
 * @param y [Integer]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(18) & 20 #=> Calc::Q(16)
 */
static VALUE
cq_and(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qand, NULL, id_and);
}

/* Performs multiplication.
 *
 * @param y [Numeric,Calc::Q]
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(2) * 3 #=> Calc::Q(6)
 */
static VALUE
cq_multiply(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qmul, &qmuli, id_multiply);
}

/* Performs addition.
 *
 * @param y [Numeric,Calc::Q]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1) + 2 #=> Calc::Q(3)
 */
static VALUE
cq_add(VALUE x, VALUE y)
{
    /* fourth arg was &qaddi, but this segfaults with ruby 2.1.x */
    return numeric_op(x, y, &qqadd, NULL, id_add);
}

/* Performs subtraction.
 *
 * @param y [Numeric,Calc::Q]
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1) - 2 #=> Calc::Q(-1)
 */
static VALUE
cq_subtract(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qsub, NULL, id_subtract);
}

/* Unary minus.  Returns the receiver's value, negated.
 *
 * @return [Calc::Q]
 * @example
 *  -Calc::Q(1) #=> Calc::Q(-1)
 */
static VALUE
cq_uminus(VALUE self)
{
    setup_math_error();
    return wrap_number(qsub(&_qzero_, DATA_PTR(self)));
}

/* Performs division.
 *
 * @param y [Numeric,Calc::Q]
 * @return [Calc::Q]
 * @raise [Calc::MathError] if other is zero
 * @example:
 *  Calc::Q(2) / 4 #=> Calc::Q(0.5)
 */
static VALUE
cq_divide(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qqdiv, &qdivi, id_divide);
}

/* Comparison - Returns -1, 0, +1 or nil depending on whether `y` is less than,
 * equal to, or greater than `x`.
 *
 * This is used by the `Comparable` module to implement `==`, `!=`, `<`, `<=`,
 * `>` and `>=`.
 *
 * nil is returned if the two values are incomparable.
 *
 * @param other [Numeric,Calc::Q]
 * @return [Fixnum,nil]
 * @example:
 *  Calc::Q(5) <=> 4     #=> 1
 *  Calc::Q(5) <=> 5.1   #=> -1
 *  Calc::Q(5) <=> 5     #=> 0
 *  Calc::Q(5) <=> "cat" #=> nil
 */
static VALUE
cq_spaceship(VALUE self, VALUE other)
{
    VALUE ary;
    NUMBER *qself, *qother;
    int result;
    setup_math_error();

    qself = DATA_PTR(self);
    /* qreli returns incorrect results if self > 0 and other == 0
       if (TYPE(other) == T_FIXNUM) {
       result = qreli(qself, NUM2LONG(other));
       }
     */
    if (CALC_Q_P(other)) {
        result = qrel(qself, DATA_PTR(other));
    }
    else if (TYPE(other) == T_FIXNUM || TYPE(other) == T_BIGNUM || TYPE(other) == T_FLOAT
             || TYPE(other) == T_RATIONAL) {
        qother = value_to_number(other, 0);
        result = qrel(qself, qother);
        qfree(qother);
    }
    else if (rb_respond_to(other, id_coerce)) {
        if (TYPE(other) == T_COMPLEX) {
            other = rb_funcall(cC, id_new, 1, other);
        }
        ary = rb_funcall(other, id_coerce, 1, self);
        if (!RB_TYPE_P(ary, T_ARRAY) || RARRAY_LEN(ary) != 2) {
            rb_raise(rb_eTypeError, "coerce must return [x, y]");
        }
        return rb_funcall(RARRAY_AREF(ary, 0), id_spaceship, 1, RARRAY_AREF(ary, 1));
    }
    else {
        return Qnil;
    }

    return INT2FIX(result);
}

/* Bitwise exclusive or (xor)
 *
 * Note that for ruby compatibility, ^ is an xor operator, unlike in calc
 * where it is a power operator.
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(5).xor(3) #=> Calc::Q(6)
 */
static VALUE
cq_xor(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qxor, NULL, id_xor);
}

/* Bitwise OR
 *
 * @param y [Integer]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(18) | 20 #=> Calc::Q(22)
 */
static VALUE
cq_or(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qor, NULL, id_or);
}

/* Bitwise NOT (complement)
 *
 * This is `-self - 1` if self is an integer, `-self` otherwise.
 *
 * @example
 *  ~Calc::Q(7)   #=> Calc::Q(-8)
 *  ~Calc::Q(0.5) #=> Calc::Q(-0.5)
 */
static VALUE
cq_comp(VALUE self)
{
    setup_math_error();
    return wrap_number(qcomp(DATA_PTR(self)));
}

/* Absolute value
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).abs  #=> Calc::Q(1)
 *  Calc::Q(-1).abs #=> Calc::Q(1)
 */
static VALUE
cq_abs(VALUE self)
{
    setup_math_error();
    return wrap_number(qqabs(DATA_PTR(self)));
}

/* Inverse trigonometric cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(0.5).acos #=> Calc::Q(1.04719755119659774615)
 *  Calc::Q(2.0).acos #=> Calc::C(1.31695789692481670863i)
 */
static VALUE
cq_acos(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacos, &c_acos);
}

/* Inverse hyperbolic cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(2).acosh #=> Calc::Q(1.31695789692481670862)
 *  Calc::Q(0).acosh #=> Calc::C(1.57079632679489661923i)
 */
static VALUE
cq_acosh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacosh, &c_acosh);
}

/* Inverse trigonometric cotangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2).acot #=> Calc::Q(0.46364760900080611621)
 */
static VALUE
cq_acot(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacot, NULL);
}

/* Inverse hyperbolic cotangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(2).acoth   #=> Calc::Q(0.5493061443340548457)
 *  Calc::Q(0.5).acoth #=> Calc::C(0.5493061443340548457+1.57079632679489661923i)
 */
static VALUE
cq_acoth(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacoth, &c_acoth);
}

/* Inverse trigonometric cosecant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(2).acsc   #=> Calc::Q(0.52359877559829887308)
 *  Calc::Q(0.5).acsc #=> Calc::C(1.57079632679489661923-1.31695789692481670863i)
 */
static VALUE
cq_acsc(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacsc, &c_acsc);
}

/* Inverse hyperbolic cosecant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(2).acsch #=> Calc::Q(0.4812118250596034475)
 */
static VALUE
cq_acsch(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacsch, &c_acsch);
}

/* Approximate numbers by multiples of a specified number
 *
 * Returns the approximate value of self as specified by an error (defaults to
 * Calc.config(:epsilon)) and rounding mode (defaults to Calc.config(:appr)).
 *
 * Type "help appr" in calc for a description of the rounding modes.
 *
 * @param y [Numeric,Calc::Q] (optional) error
 * @param z [Interger] (optional) rounding mode
 * @example
 *  Calc::Q("5.44").appr("0.1",0) #=> Calc::Q(5.4)
 */
static VALUE
cq_appr(int argc, VALUE * argv, VALUE self)
{
    VALUE result, epsilon, rounding;
    NUMBER *qepsilon, *qrounding;
    long R = 0;
    int n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "02", &epsilon, &rounding);
    if (n == 2) {
        if (FIXNUM_P(rounding)) {
            R = FIX2LONG(rounding);
        }
        else {
            qrounding = value_to_number(rounding, 1);
            if (qisfrac(qrounding)) {
                rb_raise(e_MathError, "fractional rounding for appr");
            }
            R = qtoi(DATA_PTR(qrounding));
            qfree(qrounding);
        }
    }
    else {
        R = conf->appr;
    }
    if (n >= 1) {
        qepsilon = value_to_number(epsilon, 1);
    }
    else {
        qepsilon = NULL;
    }
    result = wrap_number(qmappr(DATA_PTR(self), qepsilon ? qepsilon : conf->epsilon, R));
    if (qepsilon) {
        qfree(qepsilon);
    }
    return result;
}

/* Inverse trigonometric secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(2).asec #=> Calc::Q(1.04719755119659774615)
 */
static VALUE
cq_asec(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasec, &c_asec);
}

/* Inverse hyperbolic secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(0.5).asech #=> Calc::Q(1.31695789692481670862)
 */
static VALUE
cq_asech(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasech, &c_asech);
}

/* Inverse trigonometric sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(0.5).asin #=> Calc::Q(0.52359877559829887308)
 */
static VALUE
cq_asin(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasin, &c_asin);
}

/* Inverse hyperbolic sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2).asinh #=> Calc::Q(1.44363547517881034249)
 */
static VALUE
cq_asinh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasinh, NULL);
}

/* Inverse trigonometric tangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2).atan #=> Calc::Q(1.10714871779409050302)
 */
static VALUE
cq_atan(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qatan, NULL);
}

/* Angle to point (arctangent with 2 arguments)
 *
 * To match normal calling conventions, `y.atan2(x)` is equivalent to
 * `Math.atan2(y,x)`.  To avoid confusion, the class method may be
 * preferrable: `Calc::Q.atan2(y,x)`.
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(0).atan2(0)   #=> Calc::Q(0)
 *  Calc::Q(17).atan2(52) #=> Calc::Q(0.31597027195298044266)
 */
static VALUE
cq_atan2(int argc, VALUE * argv, VALUE self)
{
    return trans_function2(argc, argv, self, &qatan2);
}

/* Inverse hyperbolic tangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @example
 *  Calc::Q(0.5).atanh #=> Calc::Q(0.87758256189037271612)
 */
static VALUE
cq_atanh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qatanh, &c_atanh);
}

/* Returns the bernoulli number with index self.  Self must be an integer,
 * and < 2^31 if even.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is fractional or even and >= 2^31
 * @example
 *  Calc::Q(20).bernoulli.to_s(:frac) #=> "-174611/330"
 */
static VALUE
cq_bernoulli(VALUE self)
{
    NUMBER *qself, *qresult;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "Non-integer argument for bernoulli");
    }
    qresult = qbern(qself->num);
    if (!qresult) {
        rb_raise(e_MathError, "Bad argument for bern");
    }
    return wrap_number(qresult);
}

/* Returns true if binary bit y is set in self, otherwise false.
 *
 * @param y [Numeric] bit position
 * @return [Boolean]
 * @example
 *  Calc::Q(9).bit?(0) #=> true
 *  Calc::Q(9).bit?(1) #=> false
 * @see bit
 */
static VALUE
cq_bitp(VALUE self, VALUE y)
{
    /* this is an "opcode" in calc rather than a builtin ("help bit" is
     * wrong!).  this is based on calc's opcodes.c#o_bit() */
    NUMBER *qself, *qy;
    long index;
    int r;
    setup_math_error();

    qself = DATA_PTR(self);
    qy = value_to_number(y, 0);
    if (qisfrac(qy)) {
        qfree(qy);
        rb_raise(e_MathError, "Bad argument type for bit");     /* E_BIT1 */
    }
    if (zge31b(qy->num)) {
        qfree(qy);
        rb_raise(e_MathError, "Index too large for bit");       /* E_BIT2 */
    }
    index = qtoi(qy);
    qfree(qy);
    r = qisset(qself, index);
    return r ? Qtrue : Qfalse;
}

/* Round to a specified number of binary digits
 *
 * Rounds self rounded to the specified number of significant binary digits.
 * For the meanings of the rounding flags, see "help bround".
 *
 * @return [Calc::Q]
 * @param places [Integer] number of binary digits to round to (default 0)
 * @param rnd [Integer] rounding flags (default Calc.config(:round)
 * @example
 *  Calc::Q(7,32).bround(3)  #=> Calc::Q(0.25)
 */
static VALUE
cq_bround(int argc, VALUE * argv, VALUE self)
{
    return rounding_function(argc, argv, self, &qbround);
}

/* Truncate to a number of binary places
 *
 * Truncates to j binary places.  If j is omitted, 0 places is assumed.
 * Truncation of a non-integer prouces values nearer to zero.
 *
 * @param j [Integer]
 * @return [Calc::Q]
 * @example
 *  Calc.pi.btrunc    #=> Calc::Q(3)
 *  Calc.pi.btrunc(5) #=> Calc::Q(3.125)
 */
static VALUE
cq_btrunc(int argc, VALUE * argv, VALUE self)
{
    return trunc_function(argc, argv, self, &qbtrunc);
}

/* Returns the Catalan number for index self.  If self is negative, zero is
 * returned.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is not an integer or >= 2^31
 * @example
 *  Calc::Q(2).catalan  #=> Calc::Q(2)
 *  Calc::Q(5).catalan  #=> Calc::Q(42)
 *  Calc::Q(20).catalan #=> Calc::Q(6564120420)
 */
static VALUE
cq_catalan(VALUE self)
{
    NUMBER *qself, *qresult;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "Non-integer value for catalan");
    }
    else if (zge31b(qself->num)) {
        rb_raise(e_MathError, "Value too large for catalan");
    }
    qresult = qcatalan(qself);
    if (!qresult) {
        rb_raise(e_MathError, "qcatalan() returned NULL");
    }
    return wrap_number(qresult);
}

/* Approximation using continued fractions
 *
 * If self is an integer or eps is zero, returns x.
 *
 * If abs(eps) < 1, returns the smallest denominator number in one of the three
 * intervals [self, self+abs(eps)], [self-abs(eps), self], [self-abs(eps)/2,
 * self+abs(eps)/2].
 *
 * If eps >= 1 and den(self) > n, returns the nearest above, below or
 * approximation with denominatior less than or equal to n. 
 *
 * If den(self) <= eps, returns self.
 *
 * When the result is not self, the rounding is controlled by the final
 * parameter; see "help cfappr" for details.
 *
 * @return [Calc::Q]
 * @param eps [Numeric] epsilon or upper limit of denominator (default: Calc.config("epsilon"))
 * @param rnd [Integer] rounding flags (default: Calc.config("cfappr"))
 * @example
 *  Calc.pi.cfappr(1).to_s(:frac)   #=> "3"
 *  Calc.pi.cfappr(10).to_s(:frac)  #=> "25/8"
 *  Calc.pi.cfappr(50).to_s(:frac)  #=> "157/50"
 *  Calc.pi.cfappr(100).to_s(:frac) #=> "311/99"
 */
static VALUE
cq_cfappr(int argc, VALUE * argv, VALUE self)
{
    VALUE eps, rnd, result;
    NUMBER *q;
    long n, R;
    setup_math_error();

    n = rb_scan_args(argc, argv, "02", &eps, &rnd);
    q = (n >= 1) ? value_to_number(eps, 1) : conf->epsilon;
    R = (n == 2) ? value_to_long(rnd) : conf->cfappr;
    result = wrap_number(qcfappr(DATA_PTR(self), q, R));
    if (n >= 1) {
        qfree(q);
    }
    return result;
}

/* Simplify using continued fractions
 *
 * If self is not an integer, returns either the nearest above or below number
 * with denominator less than self.den.
 *
 * Rounding is controlled by rnd (default: Calc.config(:cfsim)).
 *
 * See "help cfsim" for details of rounding values.
 *
 * Repeated calls to cfsim give a sequence of good approximations with
 * decreasing denominators and correspondinlgy decreasing accuracy.
 *
 * @return [Calc::Q]
 * @param rnd [Integer] rounding flags (default: Calc.config(:cfsim))
 * @example
 *   x = Calc.pi; while (!x.int?) do; x = x.cfsim; puts x.to_s(:frac) if x.den < 1e6; end
 *   1146408/364913
 *   312689/99532
 *   104348/33215
 *   355/113
 *   22/7
 *   3
 */
static VALUE
cq_cfsim(int argc, VALUE * argv, VALUE self)
{
    VALUE rnd;
    long n, R;
    setup_math_error();

    n = rb_scan_args(argc, argv, "01", &rnd);
    R = (n >= 1) ? value_to_long(rnd) : conf->cfsim;
    return wrap_number(qcfsim(DATA_PTR(self), R));
}

/* Cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).cos #=> Calc::Q(0.5403023058681397174)
 */
static VALUE
cq_cos(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcos, NULL);
}

/* Hyperbolic cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).cosh #=> Calc::Q(1.54308063481524377848)
 */
static VALUE
cq_cosh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcosh, NULL);
}

/* Trigonometric cotangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(1).cot #=> Calc::Q(0.64209261593433070301)
 */
static VALUE
cq_cot(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcot, NULL);
}

/* Hyperbolic cotangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(1).coth #=> Calc::Q(1.31303528549933130364)
 */
static VALUE
cq_coth(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcoth, NULL);
}

/* Trigonometric cosecant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).csc #=> Calc::Q(1.18839510577812121626)
 */
static VALUE
cq_csc(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcsc, NULL);
}

/* Hyperbolic cosecant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(1).csch #=> Calc::Q(0.85091812823932154513)
 */
static VALUE
cq_csch(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qcsch, NULL);
}

/* Returns the denominator.  Always positive.
 *
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1,3).den  #=> Calc::Q(3)
 *  Calc::Q(-1,3).den #=> Calc::Q(3)
 */
static VALUE
cq_den(VALUE self)
{
    setup_math_error();
    return wrap_number(qden(DATA_PTR(self)));
}

/* Returns the digit at the specified position on decimal or any other base.
 *
 * @return [Calc::Q]
 * @param n [Integer] index. negative indices are to the right of any decimal point
 * @param b [Integer] (optional) base >= 2 (default 10)
 * @example
 *  Calc::Q("123456.789").digit(3)  #=> Calc::Q(3)
 *  Calc::Q("123456.789").digit(-3) #=> Calc::Q(9)
 */
static VALUE
cq_digit(int argc, VALUE * argv, VALUE self)
{
    VALUE pos, base;
    NUMBER *qpos, *qbase, *qresult;
    long n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "11", &pos, &base);
    qpos = value_to_number(pos, 1);
    if (qisfrac(qpos)) {
        qfree(qpos);
        rb_raise(e_MathError, "non-integer position for digit");
    }
    if (n >= 2) {
        qbase = value_to_number(base, 1);
        if (qisfrac(qbase)) {
            qfree(qpos);
            qfree(qbase);
            rb_raise(e_MathError, "non-integer base for digit");
        }
    }
    else {
        qbase = NULL;
    }
    qresult = qdigit(DATA_PTR(self), qpos->num, qbase ? qbase->num : _ten_);
    qfree(qpos);
    if (qbase)
        qfree(qbase);
    if (qresult == NULL) {
        rb_raise(e_MathError, "Invalid arguments for digit");
    }
    return wrap_number(qresult);
}

/* Returns the number of digits of the integral part of self in decimal or another base
 *
 * @return [Calc::Q]
 * @param b [Integer] (optional) base >= 2 (default 10)
 * @example
 *  Calc::Q("12.3456").digits   #=> Calc::Q(2)
 *  Calc::Q(-1234).digits       #=> Calc::Q(4)
 *  Calc::Q(0).digits           #=> Calc::Q(1)
 *  Calc::Q("-0.123").digits    #=> Calc::Q(1)
 */
static VALUE
cq_digits(int argc, VALUE * argv, VALUE self)
{
    VALUE base;
    NUMBER *qbase, *qresult;
    long n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "01", &base);
    if (n >= 1) {
        qbase = value_to_number(base, 1);
        if (qisfrac(qbase) || qiszero(qbase) || qisunit(qbase)) {
            qfree(qbase);
            rb_raise(e_MathError, "base must be integer greater than 1 for digits");
        }
    }
    qresult = itoq(qdigits(DATA_PTR(self), n >= 1 ? qbase->num : _ten_));
    if (n >= 1)
        qfree(qbase);
    return wrap_number(qresult);
}

/* Euler number
 *
 * Returns the euler number of a specified index.
 *
 * Considerable runtime and memory are required for calculating the euler
 * number for large even indices.  Calculated values are stored in a table so
 * that later calls are executed quickly.  This memory can be freed with
 * `Calc.freeeuler`.
 *
 * @example
 *  Calc::Q(18).euler   #=> Calc::Q(-2404879675441)
 *  Calc::Q(19).euler   #=> Calc::Q(0)
 *  Calc::Q(20).euler   #=> Calc::Q(370371188237525)
 */
static VALUE
cq_euler(VALUE self)
{
    NUMBER *qself, *qresult;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integer value for euler");
    }
    qresult = qeuler(qself->num);
    if (qresult == NULL) {
        rb_raise(e_MathError, "number too big or out of memory for euler");
    }
    return wrap_number(qresult);
}

/* Returns true if the number is an even integer
 *
 * @return [Boolean]
 * @example
 *  Calc::Q(1).even? #=> false
 *  Calc::Q(2).even? #=> true
 */
static VALUE
cq_evenp(VALUE self)
{
    return qiseven((NUMBER *) DATA_PTR(self)) ? Qtrue : Qfalse;
}

/* Exponential function
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).exp #=> Calc::Q(2.71828182845904523536)
 *  Calc::Q(2).exp #=> Calc::Q(7.38905609893065022723)
 */
static VALUE
cq_exp(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qexp, NULL);
}

/* Returns the factorial of a number.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is negative or not an integer
 * @raise [Calc::MathError] if abs(self) >= 2^31
 * @example:
 *  Calc::Q(10).fact #=> Calc::Q(3628800)
 */
static VALUE
cq_fact(VALUE self)
{
    setup_math_error();
    return wrap_number(qfact(DATA_PTR(self)));
}

/* Smallest prime factor not exceeding specified limit
 *
 * Ignoring signs of self and limit; if self has a prime factor less than or
 * equal to limit, then returns the smallest such factor.
 *
 * @param limit [Numeric] (optional) limit, defaults to 2^32-1
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or limit are not integers
 * @raise [Calc::MathError] if limit is >= 2^32
 * @example
 *  Calc::Q(2).power(32).+(1).factor #=> Calc::Q(641)
 */
static VALUE
cq_factor(int argc, VALUE * argv, VALUE self)
{
    VALUE limit;
    NUMBER *qself, *qlimit, *qfactor;
    ZVALUE zlimit;
    long a;
    int res;
    setup_math_error();

    a = rb_scan_args(argc, argv, "01", &limit);
    if (a >= 1) {
        qlimit = value_to_number(limit, 0);
        if (qisfrac(qlimit)) {
            qfree(qlimit);
            rb_raise(e_MathError, "non-integer limit for factor");
        }
        zcopy(qlimit->num, &zlimit);
        qfree(qlimit);
    }
    else {
        /* default limit is 2^32-1 */
        utoz((FULL) 0xffffffff, &zlimit);
    }
    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        zfree(zlimit);
        rb_raise(e_MathError, "non-integer for factor");
    }

    qfactor = qalloc();
    res = zfactor(qself->num, zlimit, &(qfactor->num));
    if (res < 0) {
        qfree(qfactor);
        zfree(zlimit);
        rb_raise(e_MathError, "limit >= 2^32 for factor");
    }
    zfree(zlimit);
    return wrap_number(qfactor);
}

/* Count number of times an integer divides self.
 *
 * Returns the greatest non-negative n for which y^n is a divisor of self.
 * Zero is returns if self is not divisible by y.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or y is non-integer
 * @param y [Integer]
 * @example
 *  Calc::Q(24).fcnt(4) #=> Calc::Q(1)
 *  Calc::Q(48).fcnt(4) #=> Calc::Q(2)
 */
static VALUE
cq_fcnt(VALUE self, VALUE y)
{
    VALUE result;
    NUMBER *qself, *qy;
    setup_math_error();

    qself = DATA_PTR(self);
    qy = value_to_number(y, 0);
    if (qisfrac(qself) || qisfrac(qy)) {
        qfree(qy);
        rb_raise(e_MathError, "non-integral argument for fcnt");
    }
    result = wrap_number(itoq(zdivcount(qself->num, qy->num)));
    qfree(qy);
    return result;
}

/* Return the fractional part of self
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(22,7).frac.to_s(:frac) #=> "1/7"
 */
static VALUE
cq_frac(VALUE self)
{
    NUMBER *qself;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisint(qself)) {
        return wrap_number(qlink(&_qzero_));
    }
    else {
        return wrap_number(qfrac(qself));
    }
}

/* Remove specified integer factors from self.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or y is non-integer
 * @param y [Integer]
 * @example
 *  Calc::Q(7).frem(4)   #=> 7
 *  Calc::Q(24).frem(4)  #=> 6
 *  Calc::Q(48).frem(4)  #=> 3
 *  Calc::Q(-48).frem(4) #=> 3
 */
static VALUE
cq_frem(VALUE self, VALUE y)
{
    VALUE result;
    NUMBER *qy;

    qy = value_to_number(y, 0);
    result = wrap_number(qfacrem(DATA_PTR(self), qy));
    qfree(qy);
    return result;
}

/* Returns the Fibonacci number with index self.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is not an integer
 * @raise [Calc::MathError] if abs(self) >= 2^31
 * @example
 *  Calc::Q(10).fib #=> Calc::Q(55)
 */
static VALUE
cq_fib(VALUE self)
{
    setup_math_error();
    return wrap_number(qfib(DATA_PTR(self)));
}

/* Greatest common divisor
 *
 * Returns the greatest common divisor of self and all arguments.  If no
 * arguments, returns self.
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(12).gcd(8)               #=> Calc::Q(4)
 *  Calc::Q(12).gcd(8, 6)            #=> Calc::Q(2)
 *  Calc.gcd("9/10", "11/5", "4/25") #=> Calc::Q(0.02)
 */
static VALUE
cq_gcd(int argc, VALUE * argv, VALUE self)
{
    NUMBER *qresult, *qarg, *qtmp;
    int i;
    setup_math_error();

    qresult = qqabs(DATA_PTR(self));
    for (i = 0; i < argc; i++) {
        qarg = value_to_number(argv[i], 1);
        qtmp = qgcd(qresult, qarg);
        qfree(qarg);
        qfree(qresult);
        qresult = qtmp;
    }
    return wrap_number(qresult);
}

/* Returns greatest integer divisor of self relatively prime to other
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(6).gcdrem(15) #=> Calc::Q(2)
 *  Calc::Q(15).gcdrem(6) #=> Calc::Q(5)
 */
static VALUE
cq_gcdrem(VALUE self, VALUE other)
{
    NUMBER *qother, *qresult;
    setup_math_error();

    qother = value_to_number(other, 0);
    qresult = qgcdrem(DATA_PTR(self), qother);
    qfree(qother);
    return wrap_number(qresult);
}

/* Returns index of highest bit in binary representation of self
 *
 * If self is a non-zero integer, higbit  returns the index of the highest bit
 * in the binary representation of abs(self).  Equivalently, x.highbit = n
 * if 2^n <= abs(x) < 2^(n+1); the binary representation of x then has n + 1
 * digits.
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(4).highbit        #=> Calc::Q(2)
 *  Calc::Q(2).**(27).highbit #=> Calc::Q(27)
 */
static VALUE
cq_highbit(VALUE self)
{
    NUMBER *qself;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integer argument for highbit");
    }
    if (qiszero(qself)) {
        return wrap_number(qlink(&_qnegone_));
    }
    else {
        return wrap_number(itoq(zhighbit(qself->num)));
    }
}

/* Returns the hypotenuse of a right-angled triangle given the other sides
 *
 * @param y [Numeric,Calc::Numeric] other side
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(3).hypot(4)  #=> Calc::Q(5)
 *  Calc::Q(2).hypot(-3) #=> Calc::Q(3.60555127546398929312)
 */
static VALUE
cq_hypot(int argc, VALUE * argv, VALUE self)
{
    return trans_function2(argc, argv, self, &qhypot);
}

/* Integer part of the number
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(3).int      #=> Calc::Q(3)
 *  Calc::Q("30/7").int #=> Calc::Q(4)
 *  Calc::Q(-3.125).int #=> Calc::Q(-3)
 */
static VALUE
cq_int(VALUE self)
{
    NUMBER *qself;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisint(qself)) {
        return self;
    }
    return wrap_number(qint(qself));
}

/* Returns true if the number is an integer.
 *
 * @return [Boolean]
 * @example
 *  Calc::Q(2).int?     #=> true
 *  Calc::Q(0.1).int?   #=> false
 */
static VALUE
cq_intp(VALUE self)
{
    return qisint((NUMBER *) DATA_PTR(self)) ? Qtrue : Qfalse;
}

/* Inverse of a real number
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example:
 *  Calc::Q(3).inverse #=> Calc::Q(0.25)
 */
static VALUE
cq_inverse(VALUE self)
{
    setup_math_error();
    return wrap_number(qinv(DATA_PTR(self)));
}

/* Integer part of specified root
 *
 * x.iroot(n) returns the greatest integer v for which v^n <= x.
 *
 * @param n [Integer]
 * @return [Calc::Q]
 * @raise [Calc::MathError] if n is not a positive integer
 * @example
 *  Calc::Q(100).iroot(3) #=> Calc::Q(4)
 */
static VALUE
cq_iroot(VALUE self, VALUE other)
{
    NUMBER *qother, *qresult;
    setup_math_error();

    qother = value_to_number(other, 0);
    qresult = qiroot(DATA_PTR(self), qother);
    qfree(qother);
    return wrap_number(qresult);
}

/* Integer part of square root
 *
 * x.isqrt returns the greatest integer n for which n^2 <= x.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is negative
 * @example
 *  Calc::Q("8.5").isqrt #=> Calc::Q(2)
 *  Calc::Q(200).isqrt   #=> Calc::Q(14)
 *  Calc::Q("2e6").isqrt #=> Calc::Q(1414)
 */
static VALUE
cq_isqrt(VALUE self)
{
    setup_math_error();
    return wrap_number(qisqrt(DATA_PTR(self)));
}

/* Compute the Jacobi function (x = self / y)
 *
 * Returns:
 * -1 if x is not quadratic residue mod y
 *  1 if y is composite, or x is a quadratic residue of y]
 *  0 if y is even or y is < 0
 *
 * @param y [Integer]
 * @return [Calc::Q]
 * @raise [Calc::MathError] if either value is not an integer
 * @example
 *  Calc::Q(2).jacobi(5)  #=> Calc::Q(-1)
 *  Calc::Q(2).jacobi(15) #=> Calc::Q(1)
 */
static VALUE
cq_jacobi(VALUE self, VALUE y)
{
    NUMBER *qy, *qresult;
    setup_math_error();

    qy = value_to_number(y, 0);
    qresult = qjacobi(DATA_PTR(self), qy);
    qfree(qy);
    return wrap_number(qresult);
}

/* Least common multiple
 *
 * If no value is zero, lcm returns the least positive number which is a
 * multiple of all values.  If at least one value is zero, the lcm is zero.
 *
 * @param v [Numeric] zero or more values
 * @return [Calc::Q]
 * @example
 *  Calc::Q(12).lcm(24, 30) #=> Calc::Q(120)
 */
static VALUE
cq_lcm(int argc, VALUE * argv, VALUE self)
{
    NUMBER *qresult, *qarg, *qtmp;
    int i;
    setup_math_error();

    qresult = qqabs(DATA_PTR(self));
    for (i = 0; i < argc; i++) {
        qarg = value_to_number(argv[i], 1);
        qtmp = qlcm(qresult, qarg);
        qfree(qarg);
        qfree(qresult);
        qresult = qtmp;
        if (qiszero(qresult))
            break;
    }
    return wrap_number(qresult);
}

/* Least common multiple of positive integers up to specified integer
 *
 * Retrurns the lcm of the integers 1, 2, ..., self
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(6).lcmfact #=> Calc::Q(60)
 *  Calc::Q(7).lcmfact #=> Calc::Q(420)
 */
static VALUE
cq_lcmfact(VALUE self)
{
    setup_math_error();
    return wrap_number(qlcmfact(DATA_PTR(self)));
}

/* Smallest prime factor in first specified number of primes
 *
 * If n is nonzero and abs(n) has a prime factor in the first m primes (2, 3,
 * 5, ...), then n.lfactor(m) returns the smallest such factor.  Otherwise it
 * returns 1.
 *
 * @param m [Numeric]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2**32 + 1).lfactor(116) #=> Calc::Q(641)
 */
static VALUE
cq_lfactor(VALUE self, VALUE other)
{
    NUMBER *qother, *qresult;
    setup_math_error();

    qother = value_to_number(other, 1);
    qresult = qlowfactor(DATA_PTR(self), qother);
    qfree(qother);
    return wrap_number(qresult);
}

/* Index of lowest nonzero bit in binary representation
 *
 * Returns the index of the lowest nonzero bit in the binary representation of
 * abs(self).  If self is zero, returns -1.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is not an integer
 * @example
 *  Calc::Q(2).lowbit     #=> Calc::Q(1)
 *  Calc::Q(2**27).lowbit #=> Calc::Q(27)
 */
static VALUE
cq_lowbit(VALUE self)
{
    NUMBER *qself;
    long index;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qiszero(qself)) {
        index = -1;
    }
    else if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integer argument for lowbit");
    }
    else {
        index = zlowbit(qself->num);
    }
    return wrap_number(itoq(index));
}

/* leg-to-leg - third side of a right angled triangle
 *
 * Returns the third side of a right-angled triangle with unit hypotenuse,
 * given one other side.  x.ltol is equivalent to sqrt(1 - x**2).  Result
 * is to nearest multiple of eps which defaults to Calc.config(:epsilon).
 *
 * @param eps [Numeric] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self it too large
 * @example
 *  Calc::Q("0.5").ltol #=> Calc::Q(0.86602540378443864676)
 */
static VALUE
cq_ltol(int argc, VALUE * argv, VALUE self)
{
    VALUE epsilon;
    NUMBER *qresult, *qepsilon;
    setup_math_error();

    if (rb_scan_args(argc, argv, "01", &epsilon) == 0) {
        qresult = qlegtoleg(DATA_PTR(self), conf->epsilon, FALSE);
    }
    else {
        qepsilon = value_to_number(epsilon, 1);
        qresult = qlegtoleg(DATA_PTR(self), qepsilon, FALSE);
        qfree(qepsilon);
    }
    return wrap_number(qresult);
}

/* test for equality modulo a specific number
 *
 * Returns true if self is congruent to y modulo md.
 *
 * @param y [Numeric]
 * @param md [Numeric]
 * @return [Boolean]
 * @example
 *  Calc::Q(5).meq?(33, 7) #=> true
 *  Calc::Q(5).meq?(32, 7) #=> false
 * @see Calc::Q#meq
 */
static VALUE
cq_meqp(VALUE self, VALUE y, VALUE md)
{
    VALUE result;
    NUMBER *qy, *qmd, *qtmp;
    setup_math_error();

    qy = value_to_number(y, 1);
    qmd = value_to_number(md, 1);
    qtmp = qsub(DATA_PTR(self), qy);
    result = qdivides(qtmp, qmd) ? Qtrue : Qfalse;
    qfree(qtmp);
    qfree(qmd);
    qfree(qy);
    return result;
}

/* Inverse of an integer modulo a specified integer
 *
 * Finds x such that:
 *   self * x = 1 (mod md)
 * If self and md are not relatively prime, zero is returned.
 *
 * The canonical residues modulo md are determined by Calc.config(:mod)
 * (run "help minv" in calc for details).
 *
 * @param md [Integer]
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or md are non-integers
 * @example
 *  Calc::Q(3).minv(10)  #=> Calc::Q(7)
 *  Calc::Q(-3).minv(10) #=> Calc::Q(3)
 */
static VALUE
cq_minv(VALUE self, VALUE md)
{
    NUMBER *qmd, *qresult;
    setup_math_error();

    qmd = value_to_number(md, 1);
    qresult = qminv(DATA_PTR(self), qmd);
    qfree(qmd);
    return wrap_number(qresult);
}

/* Computes the remainder for an integer quotient
 *
 * @param y [Numeric,Calc::Q]
 * @param rnd [Integer] rounding flags (default Calc.config(:mod))
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(11).mod(5) #=> Calc::Q(1)
 */
static VALUE
cq_mod(int argc, VALUE * argv, VALUE self)
{
    VALUE other, rnd;
    NUMBER *qother, *qresult;
    long n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "11", &other, &rnd);
    qother = value_to_number(other, 0);
    if (qiszero(qother)) {
        qfree(qother);
        rb_raise(rb_eZeroDivError, "division by zero in mod");
    }
    qresult = qmod(DATA_PTR(self), qother, (n == 2) ? value_to_long(rnd) : conf->mod);
    qfree(qother);
    return wrap_number(qresult);
}

/* Returns true if self exactly divides y, otherwise return false.
 *
 * @return [Boolean]
 * @example
 *  Calc::Q(6).mult?(2) #=> true
 *  Calc::Q(2).mult?(6) #=> false
 * @see Calc::Q#ismult
 */
static VALUE
cq_multp(VALUE self, VALUE other)
{
    VALUE result;
    NUMBER *qother;
    setup_math_error();

    qother = value_to_number(other, 0);
    result = qdivides(DATA_PTR(self), qother) ? Qtrue : Qfalse;
    qfree(qother);
    return result;
}

/* Compare nearness of two numbers with a standard
 *
 * Returns:
 *  -1 if abs(self - other) < abs(eps)
 *   0 if abs(self - other) = abs(eps)
 *   1 if abs(self - other) > abs(eps)
 *
 * @param other [Numeric]
 * @param eps [Numeric] (optional) defaults to Calc.config(:epsilon)
 * @return [Calc::Q]
 * @example
 *  Calc::Q("22/7").near("3.15", ".01")  #=> Calc::Q(-1)
 *  Calc::Q("22/7").near("3.15", ".005") #=> Calc::Q(1)
 */
static VALUE
cq_near(int argc, VALUE * argv, VALUE self)
{
    VALUE other, epsilon;
    NUMBER *qother, *qepsilon, *qresult;
    int n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "11", &other, &epsilon);
    qother = value_to_number(other, 1);
    qepsilon = (n == 2) ? value_to_number(epsilon, 1) : conf->epsilon;
    qresult = itoq((long) qnear(DATA_PTR(self), qother, qepsilon));
    qfree(qother);
    if (n == 2)
        qfree(qepsilon);
    return wrap_number(qresult);
}

/* Next candidate for primeness
 *
 * Returns the least positive integer i greater than abs(self) expressible as
 * residue + k * modulus, where k is an integer, for which i.ptest?(count, skip)
 * is true, or if there is no such integer i, nil.
 *
 * See `ptest?` for a description of `count` and `skip`.  For basic purposes,
 * use default values and count > 1.  Higher counts increase the probability
 * that the returned value is prime.
 *
 * @param count [Integer] number of tests for ptest (default 1)
 * @param skip [Integer] base selection mode for ptest (default 1)
 * @param residue [Integer] (default 0)
 * @param modulus [Integer] (default 1)
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or any parameter is not an integer
 * @example
 *  Calc::Q(100).nextcand(10)        #=> Calc::Q(101)
 *  Calc::Q(5000000000).nextcand(10) #=> Calc::Q(5000000029)
 */
static VALUE
cq_nextcand(int argc, VALUE * argv, VALUE self)
{
    return cand_navigation(argc, argv, self, &znextcand);
}

/* libcalc constant for 2^32+15 - can't include prime.h for this */
extern NUMBER _nxtprime_;

/* Next prime number
 *
 * If self is >= 2**32, raises an exception.  Otherwise returns the next prime
 * number.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is >= 2**32
 * @example
 *  Calc::Q(2).nextprime         #=> Calc::Q(3)
 *  Calc::Q(10).nextprime        #=> Calc::Q(11)
 *  Calc::Q(100).nextprime       #=> Calc::Q(101)
 *  Calc::Q("1e6").nextprime     #=> Calc::Q(1000003)
 *  Calc::Q(2**32 - 1).nextprime #=> Calc::Q(4294967311)
 */
static VALUE
cq_nextprime(VALUE self)
{
    NUMBER *qself;
    FULL next_prime;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integral for nextprime");
    }
    next_prime = znprime(qself->num);
    if (next_prime == 0) {
        /* return 2^32+15 */
        return wrap_number(qlink(&_nxtprime_));
    }
    else if (next_prime == 1) {
        rb_raise(e_MathError, "nextprime arg is >= 2^32");
    }
    return wrap_number(utoq(next_prime));
}

/* Norm of a value
 *
 * For real values, norm is the square of the absolute value.
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q("3.4").norm  #=> Calc::Q(11.56)
 *  Calc::Q("-3.4").norm #=> Calc::Q(11.56)
 */
static VALUE
cq_norm(VALUE self)
{
    setup_math_error();
    return wrap_number(qsquare(DATA_PTR(self)));
}

/* Returns the numerator.  Return value has the same sign as self.
 *
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1,3).num  #=> Calc::Q(1)
 *  Calc::Q(-1,3).num #=> Calc::Q(-1)
 */
static VALUE
cq_num(VALUE self)
{
    setup_math_error();
    return wrap_number(qnum(DATA_PTR(self)));
}

/* Returns true if the number is an odd integer
 *
 * @return [Boolean]
 * @example
 *  Calc::Q(1).odd? #=> true
 *  Calc::Q(2).odd? #=> false
 */
static VALUE
cq_oddp(VALUE self)
{
    return qisodd((NUMBER *) DATA_PTR(self)) ? Qtrue : Qfalse;
}

/* Permutation number
 *
 * Returns the number of permutations in which `other` things may be chosen
 * from `self` items where order in which they are chosen matters.
 *
 * @return [Calc::Q]
 * @param other [Integer]
 * @example
 *  Calc::Q(7).perm(3) #=> Calc::Q(210)
 */
static VALUE
cq_perm(VALUE self, VALUE other)
{
    NUMBER *qresult, *qother;
    setup_math_error();

    qother = value_to_number(other, 0);
    qresult = qperm(DATA_PTR(self), qother);
    qfree(qother);
    return wrap_number(qresult);
}

/* Product of primes up to specified integer
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is not a positive integer
 * @example
 *  Calc::Q(2).pfact   #=> Calc::Q(2)
 *  Calc::Q(10).pfact  #=> Calc::Q(210)
 *  Calc::Q(100).pfact #=> Calc::Q(2305567963945518424753102147331756070)
 */
static VALUE
cq_pfact(VALUE self)
{
    setup_math_error();
    return wrap_number(qpfact(DATA_PTR(self)));
}

/* Number of primes not exceeded specified number
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is >= 2**32
 * @example
 *  Calc::Q(10).pix    #=> Calc::Q(4)
 *  Calc::Q(100).pix   #=> Calc::Q(25)
 *  Calc::Q(10**9).pix #=> Calc::Q(50847534)
 */
static VALUE
cq_pix(VALUE self)
{
    NUMBER *qself;
    long value;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integer value for pix");
    }
    value = zpix(qself->num);
    if (value >= 0) {
        return wrap_number(utoq(value));
    }
    rb_raise(e_MathError, "pix arg is >= 2^32");
}

/* Number of decimal (or other) places in fractional part
 *
 * Returns the number of digits needed to express the fractional part of this
 * number in base b.  If self is an integer, returns 0.  If the expansion in
 * base b is infinite, returns nil.
 *
 * @param b [Integer] base (default 10)
 * @return [Calc::Q]
 * @raise [Calc::MathError] if base is invalid
 * @example
 *  Calc::Q(3).places           #=> Calc::Q(0)
 *  Calc::Q("0.0123").places    #=> Calc::Q(4)
 *  Calc::Q("0.0123").places(2) #=> nil
 *  Calc::Q(".625").places(2)   #=> Calc::Q(3)
 */
static VALUE
cq_places(int argc, VALUE * argv, VALUE self)
{
    VALUE base;
    NUMBER *qbase;
    long places;
    setup_math_error();

    if (rb_scan_args(argc, argv, "01", &base) == 0) {
        places = qdecplaces(DATA_PTR(self));
    }
    else {
        qbase = value_to_number(base, 0);
        if (qisfrac(qbase)) {
            qfree(qbase);
            rb_raise(e_MathError, "non-integer base for places");
        }
        places = qplaces(DATA_PTR(self), qbase->num);
        qfree(qbase);
        if (places == -2) {
            rb_raise(e_MathError, "invalid base for places");
        }
    }
    if (places == -1) {
        return Qnil;
    }
    return wrap_number(itoq(places));
}

/* Integral power of an interger modulo a specified integer
 *
 * x.pmod(n, md) returns the integer value of the canonical reidue of
 * x^n modulo md.  The canonical residue is determined by Calc.config(:mod).
 * See "help pmod" for full details.
 *
 * @param n [Integer]
 * @param md [Integer]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2).pmod(3, 10) #=> Calc::Q(8)
 *  Calc::Q(2).pmod(5, 10) #=> Calc::Q(2)
 */
static VALUE
cq_pmod(VALUE self, VALUE n, VALUE md)
{
    NUMBER *qn, *qmd, *qresult;
    setup_math_error();

    qn = value_to_number(n, 0);
    qmd = value_to_number(md, 0);
    qresult = qpowermod(DATA_PTR(self), qn, qmd);
    qfree(qn);
    qfree(qmd);
    return wrap_number(qresult);
}

/* Number of bits that match 0 or 1
 *
 * Counts of number of bits in abs(self) that match bitval (1 or 0, default 1)
 *
 * @param bitval [Integer] 0 or 1 (default 1)
 * @return [Calc::Q]
 * @example
 *  Calc::Q(32767).popcnt    #=> Calc::Q(15)
 *  Calc::Q(32767).popcnt(0) #=> Calc::Q(0)
 */
static VALUE
cq_popcnt(int argc, VALUE * argv, VALUE self)
{
    VALUE bitval;
    NUMBER *qself, *qbitval, *qresult;
    int b = 1;
    setup_math_error();

    if (rb_scan_args(argc, argv, "01", &bitval) == 1) {
        qbitval = value_to_number(bitval, 0);
        if (qiszero(qbitval)) {
            b = 0;
        }
        qfree(qbitval);
    }
    qself = DATA_PTR(self);
    if (qisint(qself)) {
        qresult = itoq(zpopcnt(qself->num, b));
    }
    else {
        qresult = itoq(zpopcnt(qself->num, b) + zpopcnt(qself->den, b));
    }
    return wrap_number(qresult);
}

/* Evaluates a numeric power
 *
 * @param y [Numeric] power to raise by
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q,Calc::C]
 * @raise [Calc::MathError] if raising to a VERY large power
 * @example
 *  Calc::Q("1.2345").power(10) #=> Calc::Q(8.2207405646327461795)
 *  Calc::Q(-1).power("0.1")    #=> Calc::C(0.95105651629515357212+0.3090169943749474241i)
 */
static VALUE
cq_power(int argc, VALUE * argv, VALUE self)
{
    /* ref: powervalue() in calc value.c.  handle cases NUM,NUM and NUM,COM */
    VALUE arg, epsilon, result;
    NUMBER *qself, *qarg, *qepsilon;
    COMPLEX *cself, *carg;
    setup_math_error();

    if (rb_scan_args(argc, argv, "11", &arg, &epsilon) == 1) {
        qepsilon = NULL;
    }
    else {
        qepsilon = value_to_number(epsilon, 1);
    }
    qself = DATA_PTR(self);
    if (CALC_C_P(arg) || TYPE(arg) == T_COMPLEX || qisneg(qself)) {
        cself = comalloc();
        qfree(cself->real);
        cself->real = qlink(qself);
        if (TYPE(arg) == T_STRING) {
            carg = comalloc();
            qfree(carg->real);
            carg->real = value_to_number(arg, 1);
        }
        else {
            carg = value_to_complex(arg);
        }
        result = wrap_complex(c_power(cself, carg, qepsilon ? qepsilon : conf->epsilon));
        comfree(cself);
        comfree(carg);
    }
    else {
        qarg = value_to_number(arg, 1);
        result = wrap_number(qpower(qself, qarg, qepsilon ? qepsilon : conf->epsilon));
        qfree(qarg)
    }
    if (qepsilon) {
        qfree(qepsilon);
    }
    return result;
}

/* Previous candidate for primeness
 *
 * Returns the greatest positive integer i less than abs(self) expressible as
 * residue + k * modulus, where k is an integer, for which ptest?(count, skip)
 * is true, or if there is no such integer i, nil.
 *
 * See `ptest?` for a description of `count` and `skip`.  For basic purposes,
 * use default values and count > 1.  Higher counts increase the probability
 * that the returned value is prime.
 *
 * @param count [Integer] number of tests for ptest (default 1)
 * @param skip [Integer] base selection mode for ptest (default 1)
 * @param residue [Integer] (default 0)
 * @param modulus [Integer] (default 1)
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self or any parameter is not an integer
 * @example
 *  Calc::Q(100).prevcand(10)        #=> Calc::Q(97)
 *  Calc::Q(5000000000).prevcand(10) #=> Calc::Q(4999999937)
 */
static VALUE
cq_prevcand(int argc, VALUE * argv, VALUE self)
{
    return cand_navigation(argc, argv, self, &zprevcand);
}

/* Previous prime number
 *
 * If self <= 2, returns nil.  If self is >= 2**32, raises an exception.
 * Otherwise returns the previous prime number.
 *
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is >= 2**32
 * @example
 *  Calc::Q(2).prevprime         #=> nil
 *  Calc::Q(10).prevprime        #=> Calc::Q(7)
 *  Calc::Q(100).prevprime       #=> Calc::Q(97)
 *  Calc::Q("1e6").prevprime     #=> Calc::Q(999983)
 *  Calc::Q(2**32 - 1).prevprime #=> Calc::Q(4294967291)
 */
static VALUE
cq_prevprime(VALUE self)
{
    NUMBER *qself;
    FULL prev_prime;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integral for prevprime");
    }
    prev_prime = zpprime(qself->num);
    if (prev_prime == 0) {
        return Qnil;
    }
    else if (prev_prime == 1) {
        rb_raise(e_MathError, "prevprime arg is >= 2^32");
    }
    return wrap_number(utoq(prev_prime));
}

/* Small integer prime test
 *
 * Returns true if self is prime, false if it is not prime.
 * This function can't be used for odd numbers > 2^32.
 *
 * @return [Boolean]
 * @raise [Calc::MathError] if self is odd and > 2^32.
 * @example
 *  Calc::Q(2**31 - 9).prime? #=> false
 *  Calc::Q(2**31 - 1).prime? #=> true
 * @see Calc::Q#isprime
 */
static VALUE
cq_primep(VALUE self)
{
    NUMBER *qself;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisfrac(qself)) {
        rb_raise(e_MathError, "non-integral for prime?");
    }
    switch (zisprime(qself->num)) {
    case 0:
        return Qfalse;
    case 1:
        return Qtrue;
    default:
        rb_raise(e_MathError, "prime? argument is an odd value > 2^32");
    }
}

/* Probabilistic test of primality
 *
 * Returns false if self is definitely not a prime.  Returns true if self is
 * probably prime.
 *
 * If self is < 2**32, essentially calles prime? and returns true only if self
 * is prime.
 *
 * If self is > 2**32 and is divisible by a prime <= 101, returns false.
 *
 * In other cases, performs abs(count) tests of bases of possible primality.  
 *
 * `skip` specifies how to select bases for testing:
 *  0: random in [2, self-2]
 *  1: successive primes [2, 3, 5, ...] not exceeding min(self, 65536)
 *  otherwise: integers starting from `skip`
 *
 * For a full explanation of the tests, see "help ptest".
 *
 * Returning true from this function means self is either prime or a strong
 * psuedoprime.  The probability that a composite number returns true is less
 * than (1/4)**count.  For example, ptest(10) incorrectly returns true less
 * than once in a million numbers; ptest(20) incorrectly returns true less
 * than once in a quadrillion numbers.
 *
 * @param count [Integer] (optional: default 1)
 * @param skip [Integer] (optional: default 1)
 * @return [Boolean]
 * @example
 *  Calc::Q(4294967291).ptest?(10) #=> true
 */
static VALUE
cq_ptestp(int argc, VALUE * argv, VALUE self)
{
    VALUE count, skip, result;
    NUMBER *qcount, *qskip;
    int n;
    setup_math_error();

    n = rb_scan_args(argc, argv, "02", &count, &skip);
    qcount = (n >= 1) ? value_to_number(count, 0) : qlink(&_qone_);
    qskip = (n >= 2) ? value_to_number(skip, 0) : qlink(&_qone_);
    result = qprimetest(DATA_PTR(self), qcount, qskip) ? Qtrue : Qfalse;
    qfree(qcount);
    qfree(qskip);
    return result;
}

/* Returns the quotient and remainder from division
 *
 * @param y [Numeric,Calc::Q] number to divide by
 * @param rnd [Integer] optional rounding mode, default Calc.config(:quomod)
 * @return [Array<Calc::Q>] Array containing quotient and remainder
 * @todo add parameter to control rounding
 * @example
 *  Calc::Q(13).quomod(5) #=> [Calc::Q(2), Calc::Q(3)]
 */
static VALUE
cq_quomod(int argc, VALUE * argv, VALUE self)
{
    VALUE other, rnd;
    NUMBER *qother, *qquo, *qmod;
    long r;
    setup_math_error();

    if (rb_scan_args(argc, argv, "11", &other, &rnd) == 2) {
        r = value_to_long(rnd);
    }
    else {
        r = conf->quomod;
    }
    qother = value_to_number(other, 0);
    if (qiszero(qother)) {
        qfree(qother);
        rb_raise(rb_eZeroDivError, "division by zero in quomod");
    }
    qquomod(DATA_PTR(self), qother, &qquo, &qmod, r);
    qfree(qother);
    return rb_assoc_new(wrap_number(qquo), wrap_number(qmod));
}

/* Returns true if both values are relatively prime
 *
 * @param other [Integer]
 * @return [Boolean]
 * @raise [Calc::MathError] if either values are non-integers
 * @example
 *  Calc::Q(6).rel?(5) #=> true
 *  Calc::Q(6).rel?(2) #=> false
 * @see Calc::Q#isrel
 */
static VALUE
cq_relp(VALUE self, VALUE other)
{
    VALUE result;
    NUMBER *qself, *qother;
    setup_math_error();

    qself = DATA_PTR(self);
    qother = value_to_number(other, 0);
    if (qisfrac(qself) || qisfrac(qother)) {
        qfree(qother);
        rb_raise(e_MathError, "non-integer for rel?");
    }
    result = zrelprime(qself->num, qother->num) ? Qtrue : Qfalse;
    qfree(qother);
    return result;
}

/* Round to a specified number of decimal places
 *
 * Rounds self rounded to the specified number of significant binary digits.
 * For the meanings of the rounding flags, see "help round".
 *
 * @return [Calc::Q]
 * @param places [Integer] number of decimal digits to round to (default 0)
 * @param rnd [Integer] rounding flags (default Calc.config(:round))
 * @example
 *  Calc::Q(7,32).round(3)  #=> Calc::Q(0.219)
 */
static VALUE
cq_round(int argc, VALUE * argv, VALUE self)
{
    return rounding_function(argc, argv, self, &qround);
}

/* Trigonometric secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).sec #=> Calc::Q(1.85081571768092561791)
 */
static VALUE
cq_sec(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qsec, NULL);
}

/* Hyperbolic secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).sech #=> Calc::Q(0.64805427366388539958)
 */
static VALUE
cq_sech(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qsech, NULL);
}

/* Trigonometric sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).sin #=> Calc::Q(0.84147098480789650665)
 */
static VALUE
cq_sin(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qsin, NULL);
}

/* Hyperbolic sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).sin #=> Calc::Q(1.17520119364380145688)
 */
static VALUE
cq_sinh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qsinh, NULL);
}

/* Returns the number of bytes in the machine representation of `self`
 *
 * This method acts like ruby's Fixnum#size, except that is works on fractions
 * in which case the result is the number of bytes for both the numerator and
 * denominator.  As the internal representation of numbers differs between
 * ruby and libcalc, it wil not necessary return the same values as
 * Fixnum#size.
 *
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).size     #=> Calc;:Q(4)
 *  Calc::Q(2**32).size #=> Calc::Q(8)
 *  Calc::Q("1/3").size #=> Calc::Q(8)
 */
static VALUE
cq_size(VALUE self)
{
    NUMBER *qself;
    size_t s;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisint(qself)) {
        s = qself->num.len * sizeof(HALF);
    }
    else {
        s = (qself->num.len + qself->den.len) * sizeof(HALF);
    }
    return wrap_number(itoq(s));
}

/* Return true if this value is a square
 *
 * Returns true if there exists integers, b such that:
 *   self == a^2 / b^2  (b != 0)
 *
 * Note that this function works on rationals, so:
 *   Calc::Q(25, 15).sq? #=> true
 * 
 * If you want to test for perfect square integers, you need to exclude
 * non-integer values before you test.
 *
 * @return [Boolean]
 * @example
 *  Calc::Q(25).sq?     #=> true
 *  Calc::Q(3).sq?      #=> false
 *  Calc::Q("4/25").sq? #=> true
 * @see Calc::Q#issq
 */
static VALUE
cq_sqp(VALUE self)
{
    setup_math_error();
    return qissquare(DATA_PTR(self)) ? Qtrue : Qfalse;
}

/* Trigonometric tangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).tan #=> Calc::Q(1.55740772465490223051)
 */
static VALUE
cq_tan(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qtan, NULL);
}

/* Hyperbolic tangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).tanh #=> Calc::Q(0.76159415595576488812)
 */
static VALUE
cq_tanh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qtanh, NULL);
}

/* Converts this number to a core ruby integer (Fixnum or Bignum).
 *
 * If self is a fraction, the fractional part is truncated.
 *
 * Note that the return value is a ruby Fixnum or Bignum.  If you want to
 * convert to an integer but have the result be a `Calc::Q` object, use
 * `trunc` or `round`.
 *
 * @return [Fixnum,Bignum]
 * @example
 *  Calc::Q(42).to_i     #=> 42
 *  Calc::Q("1e19").to_i #=> 10000000000000000000
 *  Calc::Q(1,2).to_i    #=> 0
 */
static VALUE
cq_to_i(VALUE self)
{
    NUMBER *qself;
    ZVALUE ztmp;
    VALUE string, result;
    char *s;
    setup_math_error();

    qself = DATA_PTR(self);
    if (qisint(qself)) {
        zcopy(qself->num, &ztmp);
    }
    else {
        zquo(qself->num, qself->den, &ztmp, 0);
    }
    if (zgtmaxlong(ztmp)) {
        /* too big to fit in a long, ztoi would return MAXLONG.  use a string
         * intermediary */
        math_divertio();
        zprintval(ztmp, 0, 0);
        s = math_getdivertedio();
        string = rb_str_new2(s);
        free(s);
        result = rb_funcall(string, rb_intern("to_i"), 0);
    }
    else {
        result = LONG2NUM(ztoi(ztmp));
    }
    zfree(ztmp);
    return result;
}

/* Converts this number to a string.
 *
 * Format depends on the configuration parameters "mode" and "display.  The
 * mode can be overridden for individual calls.
 *
 * @param mode [String,Symbol,Integer] (optional) output mode, see [Calc::Config]
 * @return [String]
 * @example
 *  Calc::Q(1,2).to_s        #=> "0.5"
 *  Calc::Q(1,2).to_s(:frac) #=> "1/2"
 *  Calc::Q(42).to_s(:hex)   #=> "0x2a"
 *  Calc::Q(42).to_s(8)      #=> "52"
 */
static VALUE
cq_to_s(int argc, VALUE * argv, VALUE self)
{
    NUMBER *qself = DATA_PTR(self);
    char *s;
    int args;
    VALUE rs, mode;
    setup_math_error();

    args = rb_scan_args(argc, argv, "01", &mode);
    if (args == 1 && FIXNUM_P(mode)) {
        if (qisint((NUMBER *) DATA_PTR(self))) {
            rs = rb_funcall(cq_to_i(self), rb_intern("to_s"), 1, mode);
        }
        else {
            rb_raise(rb_eArgError, "can't convert non-integer to string with base");
        }
    }
    else {
        math_divertio();
        if (args == 0) {
            qprintnum(qself, MODE_DEFAULT);
        }
        else {
            qprintnum(qself, (int) value_to_mode(mode));
        }
        s = math_getdivertedio();
        rs = rb_str_new2(s);
        free(s);
    }

    return rs;
}

/* Truncate to a number of decimal places
 *
 * Truncates to j decimal places.  If j is omitted, 0 places is assumed.
 * Truncation of a non-integer prouces values nearer to zero.
 *
 * @param j [Integer]
 * @return [Calc::Q]
 * @example
 *  Calc.pi.trunc    #=> Calc::Q(3)
 *  Calc.pi.trunc(5) #=> Calc::Q(3.14159)
 */
static VALUE
cq_trunc(int argc, VALUE * argv, VALUE self)
{
    return trunc_function(argc, argv, self, &qtrunc);
}

/* Returns true if self is zero
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(0).zero? #=> true
 *  Calc::Q(1).zero? #=> false
 */
static VALUE
cq_zerop(VALUE self)
{
    return qiszero((NUMBER *) DATA_PTR(self)) ? Qtrue : Qfalse;
}

/*****************************************************************************
 * class definition, called once from Init_calc when library is loaded       *
 *****************************************************************************/
void
define_calc_q(VALUE m)
{
    cQ = rb_define_class_under(m, "Q", cNumeric);
    rb_define_alloc_func(cQ, cq_alloc);
    rb_define_method(cQ, "initialize", cq_initialize, -1);
    rb_define_method(cQ, "initialize_copy", cq_initialize_copy, 1);

    rb_define_method(cQ, "&", cq_and, 1);
    rb_define_method(cQ, "*", cq_multiply, 1);
    rb_define_method(cQ, "+", cq_add, 1);
    rb_define_method(cQ, "-", cq_subtract, 1);
    rb_define_method(cQ, "-@", cq_uminus, 0);
    rb_define_method(cQ, "/", cq_divide, 1);
    rb_define_method(cQ, "<=>", cq_spaceship, 1);
    rb_define_method(cQ, "^", cq_xor, 1);
    rb_define_method(cQ, "|", cq_or, 1);
    rb_define_method(cQ, "~", cq_comp, 0);
    rb_define_method(cQ, "abs", cq_abs, 0);
    rb_define_method(cQ, "acos", cq_acos, -1);
    rb_define_method(cQ, "acosh", cq_acosh, -1);
    rb_define_method(cQ, "acot", cq_acot, -1);
    rb_define_method(cQ, "acoth", cq_acoth, -1);
    rb_define_method(cQ, "acsc", cq_acsc, -1);
    rb_define_method(cQ, "acsch", cq_acsch, -1);
    rb_define_method(cQ, "appr", cq_appr, -1);
    rb_define_method(cQ, "asec", cq_asec, -1);
    rb_define_method(cQ, "asech", cq_asech, -1);
    rb_define_method(cQ, "asin", cq_asin, -1);
    rb_define_method(cQ, "asinh", cq_asinh, -1);
    rb_define_method(cQ, "atan", cq_atan, -1);
    rb_define_method(cQ, "atan2", cq_atan2, -1);
    rb_define_method(cQ, "atanh", cq_atanh, -1);
    rb_define_method(cQ, "bernoulli", cq_bernoulli, 0);
    rb_define_method(cQ, "bit?", cq_bitp, 1);
    rb_define_method(cQ, "bround", cq_bround, -1);
    rb_define_method(cQ, "btrunc", cq_btrunc, -1);
    rb_define_method(cQ, "catalan", cq_catalan, 0);
    rb_define_method(cQ, "cfappr", cq_cfappr, -1);
    rb_define_method(cQ, "cfsim", cq_cfsim, -1);
    rb_define_method(cQ, "cos", cq_cos, -1);
    rb_define_method(cQ, "cosh", cq_cosh, -1);
    rb_define_method(cQ, "cot", cq_cot, -1);
    rb_define_method(cQ, "coth", cq_coth, -1);
    rb_define_method(cQ, "csc", cq_csc, -1);
    rb_define_method(cQ, "csch", cq_csch, -1);
    rb_define_method(cQ, "den", cq_den, 0);
    rb_define_method(cQ, "digit", cq_digit, -1);
    rb_define_method(cQ, "digits", cq_digits, -1);
    rb_define_method(cQ, "euler", cq_euler, 0);
    rb_define_method(cQ, "even?", cq_evenp, 0);
    rb_define_method(cQ, "exp", cq_exp, -1);
    rb_define_method(cQ, "fact", cq_fact, 0);
    rb_define_method(cQ, "factor", cq_factor, -1);
    rb_define_method(cQ, "fcnt", cq_fcnt, 1);
    rb_define_method(cQ, "frac", cq_frac, 0);
    rb_define_method(cQ, "frem", cq_frem, 1);
    rb_define_method(cQ, "fib", cq_fib, 0);
    rb_define_method(cQ, "gcd", cq_gcd, -1);
    rb_define_method(cQ, "gcdrem", cq_gcdrem, 1);
    rb_define_method(cQ, "highbit", cq_highbit, 0);
    rb_define_method(cQ, "hypot", cq_hypot, -1);
    rb_define_method(cQ, "int", cq_int, 0);
    rb_define_method(cQ, "int?", cq_intp, 0);
    rb_define_method(cQ, "inverse", cq_inverse, 0);
    rb_define_method(cQ, "iroot", cq_iroot, 1);
    rb_define_method(cQ, "isqrt", cq_isqrt, 0);
    rb_define_method(cQ, "jacobi", cq_jacobi, 1);
    rb_define_method(cQ, "lcm", cq_lcm, -1);
    rb_define_method(cQ, "lcmfact", cq_lcmfact, 0);
    rb_define_method(cQ, "lfactor", cq_lfactor, 1);
    rb_define_method(cQ, "lowbit", cq_lowbit, 0);
    rb_define_method(cQ, "ltol", cq_ltol, -1);
    rb_define_method(cQ, "meq?", cq_meqp, 2);
    rb_define_method(cQ, "minv", cq_minv, 1);
    rb_define_method(cQ, "mod", cq_mod, -1);
    rb_define_method(cQ, "mult?", cq_multp, 1);
    rb_define_method(cQ, "near", cq_near, -1);
    rb_define_method(cQ, "nextcand", cq_nextcand, -1);
    rb_define_method(cQ, "nextprime", cq_nextprime, 0);
    rb_define_method(cQ, "norm", cq_norm, 0);
    rb_define_method(cQ, "num", cq_num, 0);
    rb_define_method(cQ, "odd?", cq_oddp, 0);
    rb_define_method(cQ, "perm", cq_perm, 1);
    rb_define_method(cQ, "pfact", cq_pfact, 0);
    rb_define_method(cQ, "pix", cq_pix, 0);
    rb_define_method(cQ, "places", cq_places, -1);
    rb_define_method(cQ, "pmod", cq_pmod, 2);
    rb_define_method(cQ, "popcnt", cq_popcnt, -1);
    rb_define_method(cQ, "power", cq_power, -1);
    rb_define_method(cQ, "prevcand", cq_prevcand, -1);
    rb_define_method(cQ, "prevprime", cq_prevprime, 0);
    rb_define_method(cQ, "prime?", cq_primep, 0);
    rb_define_method(cQ, "ptest?", cq_ptestp, -1);
    rb_define_method(cQ, "quomod", cq_quomod, -1);
    rb_define_method(cQ, "rel?", cq_relp, 1);
    rb_define_method(cQ, "round", cq_round, -1);
    rb_define_method(cQ, "sec", cq_sec, -1);
    rb_define_method(cQ, "sech", cq_sech, -1);
    rb_define_method(cQ, "sin", cq_sin, -1);
    rb_define_method(cQ, "sinh", cq_sinh, -1);
    rb_define_method(cQ, "size", cq_size, 0);
    rb_define_method(cQ, "sq?", cq_sqp, 0);
    rb_define_method(cQ, "tan", cq_tan, -1);
    rb_define_method(cQ, "tanh", cq_tanh, -1);
    rb_define_method(cQ, "to_i", cq_to_i, 0);
    rb_define_method(cQ, "to_s", cq_to_s, -1);
    rb_define_method(cQ, "trunc", cq_trunc, -1);
    rb_define_method(cQ, "zero?", cq_zerop, 0);

    /* include Comparable */
    rb_include_module(cQ, rb_mComparable);

    rb_define_alias(cQ, "denominator", "den");
    rb_define_alias(cQ, "magnitude", "abs");
    rb_define_alias(cQ, "numerator", "num");

    id_add = rb_intern("+");
    id_and = rb_intern("&");
    id_coerce = rb_intern("coerce");
    id_divide = rb_intern("/");
    id_multiply = rb_intern("*");
    id_new = rb_intern("new");
    id_or = rb_intern("|");
    id_spaceship = rb_intern("<=>");
    id_subtract = rb_intern("-");
    id_xor = rb_intern("^");
}
