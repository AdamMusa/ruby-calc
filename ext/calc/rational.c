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
 * * Calc::Z
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
 * @param num [Numeric,Calc::Z,Calc::Q,String]
 * @param den [Numeric,Calc::Z,Calc::Q,String] (optional)
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
           NUMBER * (*fqq) (NUMBER *, NUMBER *), NUMBER * (*fql) (NUMBER *, long))
{
    NUMBER *qother, *qresult;
    VALUE result;
    setup_math_error();

    if (fql && TYPE(other) == T_FIXNUM) {
        qresult = (*fql) (DATA_PTR(self), NUM2LONG(other));
    }
    else if (CALC_Q_P(other)) {
        qresult = (*fqq) (DATA_PTR(self), DATA_PTR(other));
    }
    else if (TYPE(other) == T_FIXNUM || TYPE(other) == T_BIGNUM || TYPE(other) == T_FLOAT
             || TYPE(other) == T_RATIONAL || CALC_Z_P(other)) {
        qother = value_to_number(other, 0);
        qresult = (*fqq) (DATA_PTR(self), qother);
        qfree(qother);
    }
    else {
        rb_raise(rb_eArgError, "expected number");
    }

    result = cq_new();
    DATA_PTR(result) = qresult;
    return result;
}

static VALUE
shift(VALUE self, VALUE other, int sign)
{
    NUMBER *qother;
    VALUE result;
    long n;
    setup_math_error();

    qother = value_to_number(other, 0);
    if (qisfrac(qother)) {
        qfree(qother);
        rb_raise(rb_eArgError, "shift by non-integer");
    }
    /* check it will actually fit in a long (otherwise qtoi will be wrong) */
    if (zge31b(qother->num)) {
        qfree(qother);
        rb_raise(rb_eArgError, "shift by too many bits");
    }
    n = qtoi(qother);
    qfree(qother);
    result = cq_new();
    DATA_PTR(result) = qshift(DATA_PTR(self), n * sign);
    return result;
}

static VALUE
trans_function(int argc, VALUE * argv, VALUE self, NUMBER * (*f) (NUMBER *, NUMBER *))
{
    NUMBER *qepsilon;
    VALUE epsilon, result;
    setup_math_error();

    result = cq_new();
    if (rb_scan_args(argc, argv, "01", &epsilon) == 0) {
        DATA_PTR(result) = (*f) (DATA_PTR(self), conf->epsilon);
    }
    else {
        qepsilon = value_to_number(epsilon, 1);
        DATA_PTR(result) = (*f) (DATA_PTR(self), qepsilon);
        qfree(qepsilon);
    }
    /* there are a few functions which can return NULL with invalid args
     * instead of calling math_error */
    if (!DATA_PTR(result)) {
        rb_raise(e_MathError, "Transcendental function returned NULL");
    }
    return result;
}

/* same as trans_function(), except for functions where there are 2 NUMBER*
 * arguments, eg atan2.  the first param is the receiver (self). */
static VALUE
trans_function2(int argc, VALUE * argv, VALUE self,
                NUMBER * (*f) (NUMBER *, NUMBER *, NUMBER *))
{
    NUMBER *qarg, *qepsilon;
    VALUE arg, epsilon, result;
    setup_math_error();

    result = cq_new();
    if (rb_scan_args(argc, argv, "11", &arg, &epsilon) == 1) {
        qarg = value_to_number(arg, 0);
        DATA_PTR(result) = (*f) (DATA_PTR(self), qarg, conf->epsilon);
        qfree(qarg);
    }
    else {
        qarg = value_to_number(arg, 0);
        qepsilon = value_to_number(epsilon, 1);
        DATA_PTR(result) = (*f) (DATA_PTR(self), qarg, qepsilon);
        qfree(qarg);
        qfree(qepsilon);
    }
    if (!DATA_PTR(result)) {
        rb_raise(e_MathError, "Transcendental function returned NULL");
    }
    return result;
}

/*****************************************************************************
 * instance method implementations                                           *
 *****************************************************************************/

/* Unary plus.  Returns the receiver's value.
 *
 * @return [Calc::Q]
 * @example
 *  +Calc::Q(1) #=> Calc::Q(1)
 */
static VALUE
cq_uplus(VALUE self)
{
    return self;
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
    VALUE result;
    setup_math_error();

    result = cq_new();
    DATA_PTR(result) = qsub(&_qzero_, DATA_PTR(self));
    return result;
}

/* Performs addition.
 *
 * @param y [Numeric,Calc::Z,Calc::Q]
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1) + 2 #=> Calc::Q(3)
 */
static VALUE
cq_add(VALUE x, VALUE y)
{
    /* fourth arg was &qaddi, but this segfaults with ruby 2.1.x */
    return numeric_op(x, y, &qqadd, NULL);
}

/* Performs subtraction.
 *
 * @param y [Numeric,Calc::Z,Calc::Q]
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1) - 2 #=> Calc::Q(-1)
 */
static VALUE
cq_subtract(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qsub, NULL);
}

/* Performs multiplication.
 *
 * @param y [Numeric,Calc::Z,Calc::Q]
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(2) * 3 #=> Calc::Q(6)
 */
static VALUE
cq_multiply(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qmul, &qmuli);
}

/* Performs division.
 *
 * @param y [Numeric,Calc::Z,Calc::Q]
 * @return [Calc::Q]
 * @raise [Calc::MathError] if other is zero
 * @example:
 *  Calc::Q(2) / 4 #=> Calc::Q(0.5)
 */
static VALUE
cq_divide(VALUE x, VALUE y)
{
    return numeric_op(x, y, &qqdiv, &qdivi);
}

/* Computes the remainder for an integer quotient
 *
 * @param y [Numeric,Calc::Z,Calc::Q]
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(11) % 5 #=> Calc::Q(1)
 */
static VALUE
cq_mod(VALUE x, VALUE y)
{
    NUMBER *qy;
    VALUE result;
    setup_math_error();

    qy = value_to_number(y, 0);
    result = cq_new();
    DATA_PTR(result) = qmod(DATA_PTR(x), qy, 0);
    return result;
}

/* Left shift an integer by a given number of bits.  This multiplies the number
 * by the appropriate power of 2.
 *
 * @param n [Numeric,Calc::Z,Calc::Q] number of bits to shift
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is a non-integer
 * @raise [Calc::MathError] if abs(n) is >= 2^31
 * @example:
 *  Calc::Q(2) << 3 #=> Calc::Q(16)
 */
static VALUE
cq_shift_left(VALUE x, VALUE n)
{
    return shift(x, n, 1);
}

/* Right shift an integer by a given number of bits.  This multiplies the
 * number by the appropriate power of 2.  Low bits are truncated.
 *
 * @param n [Numeric,Calc::Z,Calc::Q] number of bits to shift
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is a non-integer
 * @raise [ArgumentError] if abs(n) is >= 2^31
 * @example:
 *  Calc::Q(8) >> 2 #=> Calc::Q(2)
 */
static VALUE
cq_shift_right(VALUE self, VALUE other)
{
    return shift(self, other, -1);
}

/* Comparison - Returns -1, 0, +1 or nil depending on whether `y` is less than,
 * equal to, or greater than `x`.
 *
 * This is used by the `Comparable` module to implement `==`, `!=`, `<`, `<=`,
 * `>` and `>=`.
 *
 * nil is returned if the two values are incomparable.
 *
 * @param other [Numeric,Calc::Z,Calc::Q]
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
             || TYPE(other) == T_RATIONAL || CALC_Z_P(other)) {
        qother = value_to_number(other, 0);
        result = qrel(qself, qother);
        qfree(qother);
    }
    else {
        return Qnil;
    }

    return INT2FIX(result);
}

/* Returns the denominator.  Always positive.
 *
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1,3).denominator  #=> Calc::Q(3)
 *  Calc::Q(-1,3).denominator #=> Calc::Q(3)
 */
static VALUE
cq_denominator(VALUE self)
{
    VALUE result;
    setup_math_error();

    result = cq_new();
    DATA_PTR(result) = qden(DATA_PTR(self));

    return result;
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
    VALUE result;
    setup_math_error();

    result = cq_new();
    DATA_PTR(result) = qfact(DATA_PTR(self));

    return result;
}

/* Returns the numerator.  Return value has the same sign as self.
 *
 * @return [Calc::Q]
 * @example:
 *  Calc::Q(1,3).numerator  #=> Calc::Q(1)
 *  Calc::Q(-1,3).numerator #=> Calc::Q(-1)
 */
static VALUE
cq_numerator(VALUE self)
{
    VALUE result;
    setup_math_error();

    result = cq_new();
    DATA_PTR(result) = qnum(DATA_PTR(self));

    return result;
}

/* Converts this number to a core ruby integer (Fixnum or Bignum).
 *
 * If self is a fraction, the fractional part is truncated.
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
 * @param mode [String,Symbol] (optional) output mode, see [Calc::Config]
 * @return [String]
 * @example
 *  Calc::Q(1,2).to_s        #=> "0.5"
 *  Calc::Q(1,2).to_s(:frac) #=> "1/2"
 *  Calc::Q(42).to_s(:hex)   #=> "0x2a"
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
    math_divertio();
    if (args == 0) {
        qprintnum(qself, MODE_DEFAULT);
    }
    else {
        qprintnum(qself, value_to_mode(mode));
    }
    s = math_getdivertedio();
    rs = rb_str_new2(s);
    free(s);

    return rs;
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
    VALUE result;
    setup_math_error();

    result = cq_new();
    DATA_PTR(result) = qqabs(DATA_PTR(self));
    return result;
}

/* Inverse trigonometric cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(0.5).acos #=> Calc::Q(1.04719755119659774615)
 */
static VALUE
cq_acos(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacos);
}

/* Inverse hyperbolic cosine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(2).acosh #=> Calc::Q(1.31695789692481670862)
 */
static VALUE
cq_acosh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacosh);
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
    return trans_function(argc, argv, self, &qacot);
}

/* Inverse hyperbolic cotangent
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if abs(self) <= 1
 * @example
 *  Calc::Q(2).acoth #=> Calc::Q(0.5493061443340548457)
 */
static VALUE
cq_acoth(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacoth);
}

/* Inverse trigonometric cosecant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if abs(self) < 1
 * @example
 *  Calc::Q(2).acsc #=> Calc::Q(0.52359877559829887308)
 */
static VALUE
cq_acsc(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qacsc);
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
    return trans_function(argc, argv, self, &qacsch);
}

/* Inverse trigonometric secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if abs(self) is < 1
 * @example
 *  Calc::Q(2).asec #=> Calc::Q(1.04719755119659774615)
 */
static VALUE
cq_asec(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasec);
}

/* Inverse hyperbolic secant
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] unless 0 < self <= 1
 * @example
 *  Calc::Q(0.5).asech #=> Calc::Q(1.31695789692481670862)
 */
static VALUE
cq_asech(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasech);
}

/* Inverse trigonometric sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self < -1 or self > 1
 * @example
 *  Calc::Q(0.5).asin #=> Calc::Q(0.52359877559829887308)
 */
static VALUE
cq_asin(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qasin);
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
    return trans_function(argc, argv, self, &qasinh);
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
    return trans_function(argc, argv, self, &qatan);
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
 * @return [Calc::Q]
 * @example
 *  Calc::Q(0.5).atanh #=> Calc::Q(0.87758256189037271612)
 */
static VALUE
cq_atanh(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qatanh);
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
    return trans_function(argc, argv, self, &qcos);
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
    return trans_function(argc, argv, self, &qcosh);
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
    return trans_function(argc, argv, self, &qcot);
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
    return trans_function(argc, argv, self, &qcoth);
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
    return trans_function(argc, argv, self, &qcsc);
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
    return trans_function(argc, argv, self, &qcsch);
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
    return trans_function(argc, argv, self, &qexp);
}

/* Logarithm
 *
 * Note that this is like using ruby's Math.log.
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is zero
 * @example
 *  Calc::Q(10).ln #=> Calc::Q(2.30258509299404568402)
 */
static VALUE
cq_ln(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qln);
}

/* Base 10 logarithm
 *
 * Note that this is like using ruby's Math.log10.
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self < -1 or self > 1
 * @example
 *  Calc::Q(10).log     #=> Calc::Q(1)
 *  Calc::Q(100).log    #=> Calc::Q(2)
 *  Calc::Q("1e10").log #=> Calc::Q(10)
 */
static VALUE
cq_log(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qlog);
}

/* Evaluates п (pi) to a specified accuracy
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q.pi          #=> Calc::Q(3.14159265358979323846)
 *  Calc::Q.pi("1e-40") #=> Calc::Q(3.1415926535897932384626433832795028841972)
 */
static VALUE
cq_pi(int argc, VALUE * argv, VALUE self)
{
    NUMBER *qepsilon;
    VALUE epsilon, result;
    setup_math_error();

    result = cq_new();
    if (rb_scan_args(argc, argv, "01", &epsilon) == 0) {
        DATA_PTR(result) = qpi(conf->epsilon);
    }
    else {
        qepsilon = value_to_number(epsilon, 1);
        DATA_PTR(result) = qpi(qepsilon);
        qfree(qepsilon);
    }

    return result;
}

/* Evaluates a numeric power
 *
 * @param y [Numeric,Calc::Z,Calc::Q] power to raise by
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if self is negative or raising to a VERY large power
 * @example
 *  Calc::Q("1.2345").power(10) #=> Calc::Q(8.2207405646327461795)
 */
static VALUE
cq_power(int argc, VALUE * argv, VALUE self)
{
    return trans_function2(argc, argv, self, &qpower);
}

/* Returns the quotient and remainder from division
 *
 * @param y [Numeric,Calc::Z,Calc::Q] number to divide by
 * @return [Array<Calc::Q>] Array containing quotient and remainder
 * @todo add parameter to control rounding
 * @example
 *  Calc::Q(13).quomod(5) #=> [Calc::Q(2), Calc::Q(3)]
 */
static VALUE
cq_quomod(VALUE self, VALUE other)
{
    NUMBER *qother, *qquo, *qmod;
    VALUE quo, mod;
    setup_math_error();

    qother = value_to_number(other, 0);
    qquomod(DATA_PTR(self), qother, &qquo, &qmod, 0);
    qfree(qother);
    quo = cq_new();
    mod = cq_new();
    DATA_PTR(quo) = qquo;
    DATA_PTR(mod) = qmod;

    return rb_assoc_new(quo, mod);
}

/* Returns the nth root
 *
 * @param n [Numeric,Calc::Z,Calc::Q] positive integer
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @raise [Calc::MathError] if n is not a positive integer
 * @example
 *  Calc::Q(7).root(4) #=> Calc::Q(1.62657656169778574321)
 */
static VALUE
cq_root(int argc, VALUE * argv, VALUE self)
{
    return trans_function2(argc, argv, self, &qroot);
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
    return trans_function(argc, argv, self, &qsec);
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
    return trans_function(argc, argv, self, &qsech);
}

/* Trigonometric sine
 *
 * @param eps [Numeric,Calc::Q] (optional) calculation accuracy
 * @return [Calc::Q]
 * @example
 *  Calc::Q(1).xxx #=> Calc::Q(0.84147098480789650665)
 */
static VALUE
cq_sin(int argc, VALUE * argv, VALUE self)
{
    return trans_function(argc, argv, self, &qsin);
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
    return trans_function(argc, argv, self, &qsinh);
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
    return trans_function(argc, argv, self, &qtan);
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
    return trans_function(argc, argv, self, &qtanh);
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
cq_iszero(VALUE self)
{
    NUMBER *qself;
    qself = DATA_PTR(self);
    return qiszero(qself) ? Qtrue : Qfalse;
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

    rb_define_method(cQ, "%", cq_mod, 1);
    rb_define_method(cQ, "*", cq_multiply, 1);
    rb_define_method(cQ, "+", cq_add, 1);
    rb_define_method(cQ, "+@", cq_uplus, 0);
    rb_define_method(cQ, "-", cq_subtract, 1);
    rb_define_method(cQ, "-@", cq_uminus, 0);
    rb_define_method(cQ, "/", cq_divide, 1);
    rb_define_method(cQ, "<<", cq_shift_left, 1);
    rb_define_method(cQ, "<=>", cq_spaceship, 1);
    rb_define_method(cQ, ">>", cq_shift_right, 1);
    rb_define_method(cQ, "abs", cq_abs, 0);
    rb_define_method(cQ, "acos", cq_acos, -1);
    rb_define_method(cQ, "acosh", cq_acosh, -1);
    rb_define_method(cQ, "acot", cq_acot, -1);
    rb_define_method(cQ, "acoth", cq_acoth, -1);
    rb_define_method(cQ, "acsc", cq_acsc, -1);
    rb_define_method(cQ, "acsch", cq_acsch, -1);
    rb_define_method(cQ, "asec", cq_asec, -1);
    rb_define_method(cQ, "asech", cq_asech, -1);
    rb_define_method(cQ, "asin", cq_asin, -1);
    rb_define_method(cQ, "asinh", cq_asinh, -1);
    rb_define_method(cQ, "atan", cq_atan, -1);
    rb_define_method(cQ, "atan2", cq_atan2, -1);
    rb_define_method(cQ, "atanh", cq_atanh, -1);
    rb_define_method(cQ, "cos", cq_cos, -1);
    rb_define_method(cQ, "cosh", cq_cosh, -1);
    rb_define_method(cQ, "cot", cq_cot, -1);
    rb_define_method(cQ, "coth", cq_coth, -1);
    rb_define_method(cQ, "csc", cq_csc, -1);
    rb_define_method(cQ, "csch", cq_csch, -1);
    rb_define_method(cQ, "denominator", cq_denominator, 0);
    rb_define_method(cQ, "exp", cq_exp, -1);
    rb_define_method(cQ, "fact", cq_fact, 0);
    rb_define_method(cQ, "ln", cq_ln, -1);
    rb_define_method(cQ, "log", cq_log, -1);
    rb_define_method(cQ, "numerator", cq_numerator, 0);
    rb_define_method(cQ, "power", cq_power, -1);
    rb_define_method(cQ, "quomod", cq_quomod, 1);
    rb_define_method(cQ, "root", cq_root, -1);
    rb_define_method(cQ, "sec", cq_sec, -1);
    rb_define_method(cQ, "sech", cq_sech, -1);
    rb_define_method(cQ, "sin", cq_sin, -1);
    rb_define_method(cQ, "sinh", cq_sinh, -1);
    rb_define_method(cQ, "tan", cq_tan, -1);
    rb_define_method(cQ, "tanh", cq_tanh, -1);
    rb_define_method(cQ, "to_i", cq_to_i, 0);
    rb_define_method(cQ, "to_s", cq_to_s, -1);
    rb_define_method(cQ, "zero?", cq_iszero, 0);
    rb_define_module_function(cQ, "pi", cq_pi, -1);

    /* include Comparable */
    rb_include_module(cQ, rb_mComparable);

    rb_define_alias(cQ, "divmod", "quomod");
    rb_define_alias(cQ, "magnitude", "abs");
    rb_define_alias(cQ, "modulo", "%");
}
