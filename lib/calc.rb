require "calc/version"
require "calc/calc"

module Calc
  def self.Z(p)
    Z.new(p)
  end

  def self.Q(*args)
    Q.new(*args)
  end

  class Z
    def inspect
      "Calc::Z(#{ to_s })"
    end

    def /(y)
      self.divmod(y).first
    end

    def remainder(y)
      z = self % y
      if ((!z.zero?) && ((self < 0 && y > 0) || (self > 0 && y < 0)))
        return z - y
      end
      return z
    end

    def to_f
      self.to_r.to_f
    end

    def to_r
      Rational(self.to_i, 1)
    end
  end

  class Q
    def inspect
      "Calc::Q(#{ to_s })"
    end

    # libcalc has no concept of floating point numbers.  so we use ruby's
    # Rational#to_f
    def to_f
      self.to_r.to_f
    end

    # convert to a core ruby Rational
    def to_r
      Rational(self.numerator.to_i, self.denominator.to_i)
    end

    # instance versions of trans functions
    %i(acos acot acsc asec asin atan cos cot csc exp ln log sec sin tan).each do |f|
      define_method f do |*args|
        Calc::Q.__send__(f, self, *args)
      end
    end
  end
end
