require "calc/version"
require "calc/calc"
require "calc/numeric"
require "calc/q"
require "calc/c"

module Calc

  # builtins implemented as instance methods on Calc::Q or Calc::C
  BUILTINS1 = %i(abs acos acosh acot acoth acsc acsch agd arg asec asech asin
                 asinh atan atan2 atanh bit bernoulli cos cosh cot coth csc
                 csch den fact gd hypot im inverse iseven isimag isodd isreal
                 num power quomod re sec sech sin sinh tan tanh)
  # builtins implemented as module methods on Calc
  BUILTINS2 = %i(config freebernoulli pi polar)

  ALL_BUILTINS = BUILTINS1 + BUILTINS2

  # module versions of instance builtins; implemented by turning the first
  # argument into the right class and calling the instance method
  class << self
    BUILTINS1.each do |f|
      define_method f do |*args|
        x = args.shift
        if x.is_a?(Calc::Q) || x.is_a?(Calc::C)
          x.__send__(f, *args)
        elsif x.is_a?(Complex)
          Calc::C(x).__send__(f, *args)
        else
          Calc::Q(x).__send__(f, *args)
        end
      end
    end
  end

  def self.Q(*args)
    Q.new(*args)
  end
  
  def self.C(*args)
    C.new(*args)
  end
end
