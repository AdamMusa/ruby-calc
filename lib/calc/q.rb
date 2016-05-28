module Calc
  class Q
    NEGONE = new(-1)
    ZERO = new(0)
    ONE = new(1)
    TWO = new(2)

    def **(other)
      power(other)
    end

    # Inverse gudermannian function
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::Q,Calc::C]
    # @example
    #  Calc::Q(1).agd #=> Calc::Q(1.22619117088351707081)
    #  Calc::Q(2).agd #=> Calc::C(1.5234524435626735209-3.14159265358979323846i)
    def agd(*args)
      r = Calc::C(self).agd(*args)
      r.real? ? r.re : r
    end

    # Returns the argument (the angle or phase) of a complex number in radians
    #
    # This method is used by non-complex classes, it will be 0 for positive
    # values, pi() otherwise.
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::Q]
    # @example
    #  Calc::Q(-1).arg #=> Calc::Q(3.14159265358979323846)
    #  Calc::Q(1).arg  #=> Calc::Q(0)
    def arg(*args)
      if self < 0
        Calc.pi(*args)
      else
        ZERO
      end
    end
    alias angle arg
    alias phase arg

    # Returns 1 if binary bit y is set in self, otherwise 0.
    #
    # @param y [Numeric] bit position
    # @return [Calc::Q]
    # @example
    #  Calc::Q(9).bit(0) #=> Calc::Q(1)
    #  Calc::Q(9).bit(1) #=> Calc::Q(0)
    # @see bit?
    def bit(y)
      bit?(y) ? ONE : ZERO
    end
    alias [] bit

    # Returns the number of bits in the integer part of `self`
    #
    # Note that this is compatible with ruby's Fixnum#bit_length.  Libcalc
    # provides a similar function called `highbit` with different semantics.
    #
    # This returns the bit position of the highest bit which is different to
    # the sign bit.  If there is no such bit (zero or -1), zero is returned.
    #
    # @return [Calc::Q]
    # @example
    #   Calc::Q(0xff).bit_length #=> Calc::Q(8)
    def bit_length
      (negative? ? -self : self + ONE).log2.ceil
    end

    # Returns a string containing the character corresponding to a value
    #
    # Note that this is for compatibility with calc, normally in ruby you
    # should just #chr
    #
    # @return [String]
    # @example
    #  Calc::Q(88).char #=> "X"
    def char
      raise MathError, "Non-integer for char" unless int?
      raise MathError, "Out of range for char" unless between?(0, 255)
      if zero?
        ""
      else
        to_i.chr
      end
    end

    # Returns a string containing the character represented by `self` value
    # according to encoding.
    #
    # Unlike the calc version (`char`), this allows numbers greater than 255
    # if an encoding is specified.
    #
    # @return [String]
    # @param encoding [Encoding]
    # @example
    #  Calc::Q(88).chr                   #=> "X"
    #  Calc::Q(300).chr(Encoding::UTF_8) #=> Unicode I-breve
    def chr(*args)
      to_i.chr(*args)
    end

    # Complex conjugate
    #
    # As the conjugate of real x is x, this method returns self.
    #
    # @return [Calc::Q]
    # @example
    #  Calc::Q(3).conj #=> 3
    def conj
      self
    end
    alias conjugate conj

    # Ruby compatible integer division
    #
    # Calls `quo` to get the quotient of integer division, with rounding mode
    # which specifies behaviour compatible with ruby's Numeric#div
    #
    # @param y [Numeric]
    # @example
    #  Calc::Q(13).div(4)     #=> Calc::Q(3)
    #  Calc::Q("11.5").div(4) #=> Calc::Q(2)
    # @see Calc::Q#quo
    def div(y)
      quo(y, ZERO)
    end

    # Ruby compatible quotient/modulus
    #
    # Returns an array containing the quotient and modulus by dividing `self`
    # by `y`.  Rounding is compatible with the ruby method `Numeric#divmod`.
    #
    # Unlike `quomod`, this is not affected by `Calc.config(:quomod)`.
    #
    # @param y [Numeric]
    # @example
    #   Calc::Q(11).divmod(3)  #=> [Calc::Q(3), Calc::Q(2)]
    #   Calc::Q(11).divmod(-3) #=> [Calc::Q(-4), Calc::Q(-1)]
    def divmod(y)
      quomod(y, ZERO)
    end

    # Iterates the given block, yielding values from `self` decreasing by 1
    # down to and including `limit`
    #
    # x.downto(limit) is equivalent to x.step(by: -1, to: limit)
    #
    # If no block is given, an Enumerator is returned instead.
    #
    # @return [Enumerator,nil]
    # @param limit [Numeric] lowest value to return
    # @example
    #   Calc::Q(10).downto(5) { |i| print i, " " } #=> 10 9 8 7 6 5
    def downto(limit, &block)
      step(limit, NEGONE, &block)
    end

    # Returns a string which if evaluated creates a new object with the original value
    #
    # @return [String]
    # @example
    #  Calc::Q(0.5).estr #=> "Calc::Q(1,2)"
    def estr
      s = self.class.name
      s << "("
      s << (int? ? num.to_s : "#{ num },#{ den }")
      s << ")"
      s
    end

    # Returns an array; [gcd, lcm]
    #
    # This method exists for compatibility with ruby's Integer class, however
    # note that the libcalc version works on rational numbers and the lcm can
    # be negative.  You can also pass more than one value.
    #
    # @return [Array]
    # @example
    #   Calc::Q(2).gcdlcm(2)  #=> [Calc::Q(2), Calc::Q(2)]
    #   Calc::Q(3).gcdlcm(-7) #=> [Calc::Q(1), Calc::Q(-21)]
    def gcdlcm(*args)
      [gcd(*args), lcm(*args)]
    end

    # Gudermannian function
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::Q]
    # @example
    #  Calc::Q(1).gd #=> Calc::Q(0.86576948323965862429)
    def gd(*args)
      r = Calc::C(self).gd(*args)
      r.real? ? r.re : r
    end

    # Returns the corresponding imaginary number
    #
    # @return [Calc::C]
    # @example
    #  Calc::Q(1).i #=> Calc::C(1i)
    def i
      Calc::C(ZERO, self)
    end

    def im
      ZERO
    end
    alias imaginary im

    # Returns true if the number is imaginary.  Instances of this class always
    # return false.
    #
    # @return [Boolean]
    # @example
    #  Calc::Q(1).imag? #=> false
    def imag?
      false
    end

    def inspect
      "Calc::Q(#{ self })"
    end

    def iseven
      even? ? ONE : ZERO
    end

    # Returns 1 if the number is imaginary, otherwise returns 0.  Instance of
    # this class always return 0.
    #
    # @return [Calc::Q]
    # @example
    #  Calc::Q(1).isimag #=> Calc::Q(0)
    def isimag
      ZERO
    end

    # Returns 1 if self exactly divides y, otherwise return 0.
    #
    # @return [Calc::Q]
    # @example
    #  Calc::Q(6).ismult(2) #=> Calc::Q(1)
    #  Calc::Q(2).ismult(6) #=> Calc::Q(0)
    # @see Calc::Q#mult?
    def ismult(y)
      mult?(y) ? ONE : ZERO
    end

    def isodd
      odd? ? ONE : ZERO
    end

    # Returns 1 if self is prime, 0 if it is not prime.  This function can't
    # be used for odd numbers > 2^32.
    #
    # @return [Calc::Q]
    # @raise [Calc::MathError] if self is odd and > 2^32
    # @example
    #  Calc::Q(2**31 - 9).isprime #=> Calc::Q(0)
    #  Calc::Q(2**31 - 1).isprime #=> Calc::Q(1)
    def isprime
      prime? ? ONE : ZERO
    end

    # Returns 1 if this number has zero imaginary part, otherwise returns 0.
    # Instances of this class always return 1.
    #
    # @return [Calc::Q]
    # @example
    #  Calc::Q(1).isreal #=> Calc::Q(1)
    def isreal
      ONE
    end

    # Returns 1 if both values are relatively prime
    #
    # @param other [Integer]
    # @return [Calc::Q]
    # @raise [Calc::MathError] if either values are non-integers
    # @example
    #  Calc::Q(6).isrel(5) #=> Calc::Q(1)
    #  Calc::Q(6).isrel(2) #=> Calc::Q(0)
    # @see Calc::Q#rel?
    def isrel(y)
      rel?(y) ? ONE : ZERO
    end

    # Returns 1 if this value is a square
    #
    # @return [Calc::Q]
    # @example
    #  Calc::Q(25).issq     #=> Calc::Q(1)
    #  Calc::Q(3).issq      #=> Calc::Q(0)
    #  Calc::Q("4/25").issq #=> Calc::Q(1)
    # @see Calc::Q#sq?
    def issq
      sq? ? ONE : ZERO
    end

    # test for equaility modulo a specific number
    #
    # Returns 1 if self is congruent to y modulo md, otherwise 0.
    #
    # @param y [Numeric]
    # @param md [Numeric]
    # @return [Calc::Q]
    # @example
    #  Calc::Q(5).meq(33, 7) #=> Calc::Q(1)
    #  Calc::Q(5).meq(32, 7) #=> Calc::Q(0)
    # @see Calc::Q#meq?
    def meq(y, md)
      meq?(y, md) ? ONE : ZERO
    end

    # test for inequality modulo a specific number
    #
    # Reurns 1 if self is not congruent to y modulo md, otherwise 0.
    # This is the opposite of #meq.
    #
    # @param y [Numeric]
    # @param md [Numeric]
    # @return [Calc::Q]
    # @example
    #  Calc::Q(5).mne(33, 7) #=> Calc::Q(0)
    #  Calc::Q(5).mne(32, 7) #=> Calc::Q(1)
    # @see Calc::Q#mne?
    def mne(y, md)
      meq?(y, md) ? ZERO : ONE
    end

    # test for inequality modulo a specific number
    #
    # Returns true of self is not congruent to y modulo md.
    # This is the opposiute of #meq?.
    #
    # @param y [Numeric]
    # @param md [Numeric]
    # @return [Boolean]
    # @example
    #   Calc::Q(5).mne?(33, 7) #=> false
    #   Calc::Q(5).mne?(32, 6) #=> true
    def mne?(y, md)
      !meq?(y, md)
    end

    # Ruby compatible modulus
    #
    # Returns the modulus of `self` divided by `y`.
    #
    # Rounding is compatible with the ruby method Numeric#modulo.  Unlike
    # `mod`, this is not affected by `Calc.confg(:mod)`.
    #
    # @param y [Numeric]
    # @example
    #  Calc::Q(13).modulo(4)  #=> Calc::Q(1)
    #  Calc::Q(13).modulo(-4) #=> Calc::Q(-3)
    def modulo(y)
      mod(y, ZERO)
    end

    # Return true if `self` is less than zero.
    #
    # This method exists for ruby Fixnum/Rational compatibility
    #
    # @return [Boolean]
    # @example
    #  Calc::Q(-1).negative? #=> true
    #  Calc::Q(0).negative?  #=> false
    #  Calc::Q(1).negative?  #=> false
    def negative?
      self < ZERO
    end

    # Returns self.
    #
    # This method is for ruby Integer compatibility
    #
    # @return [Calc::Q]
    def ord
      self
    end

    # Return true if `self` is greater than zero.
    #
    # This method exists for ruby Fixnum/Rational compatibility
    #
    # @return [Boolean]
    # @example
    #  Calc::Q(-1).positive? #=> false
    #  Calc::Q(0).positive?  #=> false
    #  Calc::Q(1).positive?  #=> true
    def positive?
      self > ZERO
    end

    # Returns one less than self.
    #
    # This method exists for ruby Fixnum/Integer compatibility.
    #
    # @return [Calc::Q]
    # @example
    #   Calc::Q(1).pred #=> Calc::Q(0)
    def pred
      self - ONE
    end

    # Probabilistic primacy test
    #
    # Returns 1 if ptest? would have returned true, otherwise 0.
    #
    # @param count [Integer]
    # @param skip [Integer]
    # @return [Calc::Q]
    # @see Calc::Q#ptest?
    def ptest(*args)
      ptest?(*args) ? ONE : ZERO
    end

    # Returns a simpler approximation of the value if the optional argument eps
    # is given (rat-|eps| <= result <= rat+|eps|), self otherwise.
    #
    # Note that this method exists for ruby Numeric compatibility.  Libcalc has
    # an alternative approximation method with different semantics, see `appr`.
    #
    # @param eps [Float,Rational]
    # @return [Calc::Q]
    # @example
    #  Calc::Q(5033165, 16777216).rationalize                   #=> Calc::Q(5033165/16777216)
    #  Calc::Q(5033165, 16777216).rationalize(Rational('0.01')) #=> Calc::Q(3/10)
    #  Calc::Q(5033165, 16777216).rationalize(Rational('0.1'))  #=> Calc::Q(1/3)
    def rationalize(eps = nil)
      eps ? Calc::Q.new(to_r.rationalize(eps)) : self
    end

    def re
      self
    end

    # Returns true if this number has zero imaginary part.  Instances of this
    # class always return true.
    #
    # @return [Boolean]
    # @example
    #  Calc::Q(1).real? #=> true
    def real?
      true
    end

    # Remainder of `self` divided by `y`
    #
    # This method is provided for compatibility with ruby's
    # `Numeric#remainder`.  Unlike `%` and `mod`, this method behaves the same
    # as the ruby version, unaffected by `Calc.config(:mod).
    #
    # @param y [Numeric]
    # @return [Calc::C,Calc::Q]
    # @example
    #  Calc::Q(13).remainder(4) #=> Calc::Q(1)
    def remainder(y)
      z = modulo(y)
      if !z.zero? && ((self < 0 && y > 0) || (self > 0 && y < 0))
        z - y
      else
        z
      end
    end

    # Invokes the given block with the sequence of numbers starting at self
    # incrementing by step (default 1) on each call.
    #
    # In the first format, uses keyword parameters:
    #   x.step(by: step, to: limit)
    #
    # In the second format, uses positional parameters:
    #   x.step(limit = nil, step = 1)
    #
    # If step is negative, the sequence decrements instead of incrementing.
    # If step is zero, the sequence will yield self forever.
    #
    # If limit exists, the sequence will stop once the next item yielded would
    # be higher than limit (if step is positive) or lower than limit (if step
    # is negative).  If limit is nil, the sequence never stops.
    #
    # If no block is givem, an Enumerator is returned instead.
    #
    # This method was added for ruby Numeric compatibiliy; unlike Numeric#step,
    # it is not an error for step to be zero in the positional format.
    #
    # @param by [Numeric] amount to add to sequence each iteration
    # @param to [Numeric] end of sequence value
    # @return [Enumerator,nil]
    # @example
    #  Calc::Q(1).step(10, 3).to_a      #=> [Calc::Q(1), Calc::Q(4), Calc::Q(7), Calc::Q(10)]
    #  Calc::Q(10).step(by: -2).take(4) #=> [Calc::Q(10), Calc::Q(8), Calc::Q(6), Calc::Q(4)]
    #  Calc::Q(1).exp.step(to: Calc.pi, by: "0.2") { |q| print q, " " } #=> nil
    # prints:
    #  2.71828182845904523536 2.91828182845904523536 3.11828182845904523536
    def step(a1 = nil, a2 = ONE)
      return to_enum(:step, a1, a2) unless block_given?
      to, by = step_args(a1, a2)
      loop { yield self } if by.zero?
      i = self
      loop do
        break if to && ((by.positive? && i > to) || (by.negative? && i < to))
        yield i
        i += by
      end
    end

    # work out what the caller meant with their args to #step
    # returns an array of [to, by] parameters
    def step_args(a1, a2)
      if a1.is_a?(Hash)
        # fake keywords style
        badkeys = a1.keys - %i(to by)
        raise ArgumentError, "Unknown keywords: #{ badkeys.join(", ") }" if badkeys.any?
        to = a1.fetch(:to, nil)
        by = a1.fetch(:by, ONE)
      else
        # positional style (limit, step)
        to = a1
        by = a2
      end
      [to ? self.class.new(to) : nil, self.class.new(by)]
    end
    private :step_args

    # Returns one more than self.
    #
    # This method exists for ruby Fixnum/Integer compatibility.
    #
    # @return [Calc::Q]
    # @example
    #   Calc::Q(1).pred #=> Calc::Q(2)
    def succ
      self + ONE
    end
    alias next succ

    # Returns a ruby Complex number with self as the real part and zero
    # imaginary part.
    #
    # @return [Complex]
    # @example
    #  Calc::Q(2).to_c    #=> (2+0i)
    #  Calc::Q(2.5).to_c  #=> ((5/2)+0i)
    def to_c
      Complex(int? ? to_i : to_r, 0)
    end

    # Returns a Calc::C complex number with self as the real part and zero
    # imaginary part.
    #
    # @return [Calc::C]
    # @example
    #   Calc::Q(2).to_complex #=> Calc::C(2)
    def to_complex
      C.new(self, 0)
    end

    # libcalc has no concept of floating point numbers.  so we use ruby's
    # Rational#to_f
    def to_f
      to_r.to_f
    end

    # convert to a core ruby Rational
    def to_r
      Rational(numerator.to_i, denominator.to_i)
    end

    alias truncate trunc

    # Iterates the given block, yielding values from `self` increasing by 1
    # up to and including `limit`
    #
    # x.upto(limit) is equivalent to x.step(by: 1, to: limit)
    #
    # If no block is given, an Enumerator is returned instead.
    #
    # @return [Enumerator,nil]
    # @param limit [Numeric] highest value to return
    # @example
    #   Calc::Q(5).upto(10) { |i| print i, " " } #=> 5 6 7 8 9 10
    def upto(limit, &block)
      step(limit, ONE, &block)
    end

    # Bitwise exclusive or of a set of integers
    #
    # xor(a, b, c, ...) is equivalent to (((a ^ b) ^ c) ... )
    # note that ^ is the ruby xor operator, not the calc power operator.
    #
    # @return [Calc::Q]
    # @example
    #   Calc::Q(3).xor(5)           #=> Calc::Q(6)
    #   Calc::Q(5).xor(3, -7, 2, 9) #=> Calc::Q(-12)
    def xor(*args)
      args.inject(self, :^)
    end

    # aliases for compatibility with ruby Fixnum/Bignum/Rational
    alias imag im
    alias integer? int?
    alias real re
  end
end
