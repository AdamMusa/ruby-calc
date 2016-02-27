module Calc
  class C
    # Returns the absolute value of a complex number.  For purely real or
    # purely imaginary values, returns the absolute value of the non-zero
    # part.  Otherwise returns the absolute part of its complex form within
    # the specified accuracy.
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::Q]
    # @example
    #   Calc::C(-1).abs          #=> Calc::Q(1)
    #   Calc::C(3,-4).abs        #=> Calc::Q(5)
    #   Calc::C(4,5).abs("1e-5") #=> Calc::Q(6.40312)
    def abs(*args)
      # see absvalue() in value.c
      re.hypot(im, *args)
    end

    # Approximate numbers of multiples of a specific number.
    #
    # c.appr(y,z) is equivalent to c.re.appr(y,z) + c.im.appr(y,z) * Calc::C(0,1)
    def appr(*args)
      q1 = re.appr(*args)
      q2 = im.appr(*args)
      if q2.zero?
        q1
      else
        self.class.new(q1, q2)
      end
    end

    # Returns the argument (the angle or phase) of a complex number in radians.
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::Q]
    # @example
    #  Calc::C(1,0)  #=> 0
    #  Calc::C(-1,0) #=> -pi
    #  Calc::C(1,1)  #=> Calc::Q(0.78539816339744830962)
    def arg(*args)
      # see f_arg() in func.c
      im.atan2(re, *args)
    end

    # Trigonometric cotangent
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).cot #=> Calc::C(~-0.00373971037633695666-~0.99675779656935831046i)
    def cot(*args)
      # see f_cot() in func.c
      cos(*args) / sin(*args)
    end

    # Hyperbolic cotangent
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).coth #=> Calc::C(~1.03574663776499539611+~0.01060478347033710175i)
    def coth(*args)
      # see f_coth() in func.c
      cosh(*args) / sinh(*args)
    end

    # Trigonometric cosecant
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).csc #=> Calc::C(~0.09047320975320743981+~0.04120098628857412646i)
    def csc(*args)
      # see f_csc() in func.c
      sin(*args).inverse
    end

    # Hyperbolic cosecant
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).csch #=> Calc::C(~-0.27254866146294019951-~0.04030057885689152187i)
    def csch(*args)
      # see f_csch() in func.c
      sinh(*args).inverse
    end

    def iseven
      even? ? Calc::Q(1) : Calc::Q(0)
    end

    # Returns 1 if the number is imaginary (zero real part and non-zero
    # imaginary part) otherwise returns 0.  See also [imag?].
    #
    # @return [Calc::Q]
    # @example
    #  Calc::C(0,1).isimag #=> Calc::Q(1)
    #  Calc::C(1,1).isimag #=> Calc::Q(0)
    def isimag
      imag? ? Calc::Q(1) : Calc::Q(0)
    end

    def isodd
      odd? ? Calc::Q(1) : Calc::Q(0)
    end

    # Returns 1 if the number has zero imaginary part, otherwise returns 0.
    # See also [real?].
    #
    # @return [Calc::Q]
    # @example
    #  Calc::C(1,1).isreal #=> Calc::Q(0)
    #  Calc::C(1,0).isreal #=> Calc::Q(1)
    def isreal
      real? ? Calc::Q(1) : Calc::Q(0)
    end

    # Trigonometric secant
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).sec #=> Calc::C(~-0.04167496441114427005+~0.09061113719623759653i)
    def sec(*args)
      # see f_sec() in func.c
      cos(*args).inverse
    end

    # Hyperbolic secant
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(2,3).sech #=> Calc::C(~-0.26351297515838930964-~0.03621163655876852087i)
    def sech(*args)
      # see f_sech() in func.c
      cosh(*args).inverse
    end

    # Trigonometric tangent
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(1,2).tan #=> Calc::C(~-0.00376402564150424829+~1.00323862735360980145i)
    def tan(*args)
      # see f_tan() in func.c
      sin(*args) / cos(*args)
    end

    # Hyperbolic tangent
    #
    # @param eps [Calc::Q] (optional) calculation accuracy
    # @return [Calc::C]
    # @example
    #  Calc::C(1,2).tanh #=> Calc::C(~0.96538587902213312428-~0.00988437503832249372i)
    def tanh(*args)
      # see f_tanh() in func.c
      sinh(*args) / cosh(*args)
    end

    def to_s(*args)
      r = self.re
      i = self.im
      if i.zero?
        r.to_s(*args)
      elsif r.zero?
        imag_part(i, *args)
      elsif i > 0
        r.to_s(*args) + "+" + imag_part(i, *args)
      else
        r.to_s(*args) + "-" + imag_part(i.abs, *args)
      end
    end

    def inspect
      "Calc::C(#{ to_s })"
    end

    private

    # for formatting imaginary parts; if a fraction, put the "i" after the
    # denominator (eg 2i/3).  otherwise it goes at the end (eg 0.5i).
    def imag_part(number, *args)
      string = number.to_s(*args)
      string.insert(string.index("/") || -1, "i")
    end
  end
end
