require 'minitest_helper'

class TestComplex < MiniTest::Test

  BIG  =  0x4000000000000000  # first BigNum
  BIG2 =  0x8000000000000000  # first Bignum that won't fit in a long
  BIG3 = -0x8000000000000001  # first negative bignum that won't fit in a long

  def test_class_exists
    refute_nil Calc::C
  end

  def test_initialization
    assert_instance_of Calc::C, Calc::C(1,1)
    assert_instance_of Calc::C, Calc::C(BIG, BIG)
    assert_instance_of Calc::C, Calc::C(BIG2, BIG2)
    assert_instance_of Calc::C, Calc::C(BIG3, BIG3)
    assert_instance_of Calc::C, Calc::C("1", "1e3")
    assert_instance_of Calc::C, Calc::C("1/3", "1e-3")
    assert_instance_of Calc::C, Calc::C(Rational(1,3), Rational(1,3))
    assert_instance_of Calc::C, Calc::C(0.3, 0.3)
    assert_instance_of Calc::C, Calc::C(Calc::Q(1), Calc::Q(2))

    assert_instance_of Calc::C, Calc::C(1)
    assert_instance_of Calc::C, Calc::C(BIG)
    assert_instance_of Calc::C, Calc::C(BIG2)
    assert_instance_of Calc::C, Calc::C(BIG3)
    assert_instance_of Calc::C, Calc::C("1")
    assert_instance_of Calc::C, Calc::C("1/3")
    assert_instance_of Calc::C, Calc::C(Rational(1,3))
    assert_instance_of Calc::C, Calc::C(0.3)
    assert_instance_of Calc::C, Calc::C(Calc::Q(1))
    assert_instance_of Calc::C, Calc::C(Complex(1,1))
    assert_instance_of Calc::C, Calc::C(Calc::C(1,1))
  end

  def test_re
    assert_instance_of Calc::Q, Calc::C(1,2).re
    assert_equal 1, Calc::C(1,2).re
    assert_alias Calc::C(1,2), :re, :real
  end

  def test_im
    assert_instance_of Calc::Q, Calc::C(1,2).im
    assert_equal 2, Calc::C(1,2).im
    assert_alias Calc::C(1,2), :im, :imag
  end

  def test_equal
    assert Calc::C(1,2) == Calc::C(1,2)
    assert Calc::C(BIG, BIG2) == Complex(BIG, BIG2)
    assert Calc::C(10, 0) == 10
    assert Calc::C(BIG2, 0) == BIG2
    assert Calc::C(5, 0) == Rational(5)
    assert Calc::C(5, 0) == 5.0
    assert Calc::C(5, 0) == Calc::Q(5)

    refute Calc::C(1,2) == Calc::C(1,3)
    refute Calc::C(BIG, BIG2) == Complex(BIG2, BIG)
    refute Calc::C(10, 1) == 10
    refute Calc::C(BIG2, 1) == BIG2
    refute Calc::C(5, 1) == Rational(5)
    refute Calc::C(5, 1) == 5.0
    refute Calc::C(5, 1) == Calc::Q(5)
  end

  def test_to_s
    assert_equal "1+1i", Calc::C(1,1).to_s
    assert_equal "1", Calc::C(1,0).to_s
    assert_equal "1i", Calc::C(0,1).to_s
    assert_equal "0", Calc::C(0,0).to_s
    assert_equal "-1i", Calc::C(0,-1).to_s
    assert_equal "-1", Calc::C(-1,0).to_s
    assert_equal "-1-1i", Calc::C(-1,-1).to_s
    assert_equal "0.2+0.4i", Calc::C(Calc::Q(1,5), Calc::Q(2,5)).to_s
    assert_equal "1/5+2i/5", Calc::C(Calc::Q(1,5), Calc::Q(2,5)).to_s(:frac)
    assert_equal "-0.2-0.4i", Calc::C(Calc::Q(-1,5), Calc::Q(-2,5)).to_s
    assert_equal "-1/5-2i/5", Calc::C(Calc::Q(-1,5), Calc::Q(-2,5)).to_s(:frac)
  end

  def test_inspect
    assert_equal "Calc::C(1+1i)", Calc::C(1,1).inspect
  end

  def test_unary
    assert_instance_of Calc::C, +Calc::C(1,1)
    assert_instance_of Calc::C, -Calc::C(1,1)
    assert_equal Complex(1,1), +Calc::C(1,1)
    assert_equal Complex(-1,-1), +Calc::C(-1,-1)
    assert_equal Complex(-1,-1), -Calc::C(1,1)
    assert_equal Complex(1,1), -Calc::C(-1,-1)
  end

  def test_add
    assert_instance_of Calc::C, Calc::C(1,1) + Calc::C(2,-2)
    assert_equal Calc::C(3,-1), Calc::C(1,1) + Calc::C(2,-2)
    assert_equal Calc::C(3,1), Calc::C(1,1) + Calc::Q(2)
    assert_equal Calc::C(3,3), Calc::C(1,1) + Complex(2,2)
    assert_equal Calc::C(Calc::Q(5,3), 1), Calc::C(1,1) + Rational(2,3)
    assert_equal Calc::C(3,1), Calc::C(1,1) + 2
    assert_equal Calc::C(1+BIG2,1), Calc::C(1,1) + BIG2
    assert_equal Calc::C(Calc::Q(3,2), 1), Calc::C(1,1) + 0.5
  end

  def test_subtract
    assert_instance_of Calc::C, Calc::C(1,1) - Calc::C(1,1)
    assert_equal Calc::C(0), Calc::C(1,1) - Calc::C(1,1)
    assert_equal Calc::C(Calc::Q(1,3),1), Calc::C(1,1) - Calc::Q(2,3)
  end

  def test_multiply
    assert_instance_of Calc::C, Calc::C(1,1) * Calc::C(1,1)
    assert_equal Calc::C(0,2), Calc::C(1,1) * Calc::C(1,1)
    assert_equal Calc::C(0,1), Calc::C(1,0) * Calc::C(0,1)
    assert_equal Calc::C(0,-2), Calc::C(1,1) * Calc::C(-1,-1)
  end

  def test_divide
    assert_instance_of Calc::C, Calc::C(1,1) / Calc::C(1,1)
    assert_equal 1, Calc::C(1,1) / Calc::C(1,1)
    assert_equal Calc::C(1,-1), Calc::C(1,1) / Calc::C(0,1)
    assert_equal Calc::C(1,1), Calc::C(1,1) / Calc::C(1,0)
    assert_equal Calc::C(2,-2), Calc::C(4,-4) / 2
    assert_equal Calc::C(-2,-2), Calc::C(4,-4) / Calc::C(0,2)
    assert_equal Calc::C(0,-2), Calc::C(4,-4) / Calc::C(2,2)

    assert_raises(Calc::MathError) { Calc::C(1,1) / 0 }
  end

  def test_power
    assert_instance_of Calc::C, Calc::C(1,1) ** 2
    assert_instance_of Calc::C, Calc::C(1,1).power(2)

    assert_equal -4, Calc::C(1,1) ** 4
    assert_equal 8.22074, Calc::C("1.2345").power(10, "1e-5").re.to_f
    assert_equal       0, Calc::C("1.2345").power(10, "1e-5").im
    assert_equal -26, Calc::C(1,3).power(3).re
    assert_equal -18, Calc::C(1,3).power(3).im
    assert_equal Calc::C("-2.50593","-1.39445"), Calc::C(1,3).power(Calc::C(2,1), "1e-5")
    assert_equal Calc::C(".20787957635076190855"), Calc::C(0,1) ** Calc::C(0,1)
  end

  def test_coerce
    assert_instance_of Calc::C, Complex(1,1) + Calc::C(2,2)
    assert_equal Calc::C(3,3), Complex(1,1) + Calc::C(2,2)
    assert_equal Calc::C(3,2), 1 + Calc::C(2,2)
    assert_equal Calc::C(4,4), 2 * Calc::C(2,2)
    assert_equal Calc::C("0.5","-0.5"), 2 / Calc::C(2,2)
    assert_equal Calc::C(1,1), 0.5 * Calc::C(2,2)
  end

  def test_abs
    assert_instance_of Calc::Q, Calc::C(1,0).abs
    assert_instance_of Calc::Q, Calc::C(1,1).abs
    assert_instance_of Calc::Q, Calc::C(0,1).abs
    assert_equal 1, Calc::C(1,0).abs
    assert_in_epsilon 1.4142135623730950488, Calc::C(1,1).abs
    assert_equal 1, Calc::C(0,1).abs
    assert_equal 0, Calc::C(0,0).abs
    assert_equal 1, Calc::C(-1).abs
    assert_equal 5, Calc::C(3,-4).abs
  end

  def test_real?
    assert_instance_of TrueClass, Calc::C(1,0).real?
    assert_instance_of FalseClass, Calc::C(1,1).real?
    assert_instance_of FalseClass, Calc::C(0,1).real?
  end

  def test_imag?
    assert_instance_of TrueClass, Calc::C(0,1).imag?
    assert_instance_of FalseClass, Calc::C(1,1).imag?
    assert_instance_of FalseClass, Calc::C(1,0).imag?
  end

  def test_arg
    assert_instance_of Calc::Q, Calc::C(1,1).arg
    assert_equal 0, Calc::C(2).arg
    assert_in_epsilon Calc::Q.pi(), Calc::C(-2).arg
    assert_in_epsilon 0.98279372324732906799, Calc::C(2,3).arg
    assert_in_epsilon 0.98279, Calc::C(2,3).arg("1e-5")
  end

  def test_polar
    assert_instance_of Calc::C, Calc::C.polar(2,0)
    assert_instance_of Calc::C, Calc::C.polar(1,2, "1e-5")
    assert_instance_of Calc::C, Calc::C.polar(2, Calc::Q.pi()/4)
    assert_equal 2, Calc::C.polar(2,0)
    assert_equal Calc::C("-0.41615","0.9093"), Calc::C.polar(1,2,"1e-5")
    assert_in_epsilon 1.4142135623730950488, Calc::C.polar(2, Calc::Q.pi()/4).re
    assert_in_epsilon 1.4142135623730950488, Calc::C.polar(2, Calc::Q.pi()/4).im
  end

  def test_cos
    assert_complex_parts Calc::C(2,3).cos, -4.18962569096880723013, -9.10922789375533659798
  end

  def test_sin
    assert_complex_parts Calc::C(2,3).sin, 9.15449914691142957347, -4.16890695996656435076
  end

  def test_cosh
    assert_complex_parts Calc::C(2,3).cosh, -3.72454550491532256548, 0.51182256998738460884
  end

  def test_sinh
    assert_complex_parts Calc::C(2,3).sinh, -3.59056458998577995202, 0.53092108624851980526
  end

  def test_asin
    assert_complex_parts Calc::C(2,3).asin, 0.57065278432109940071, 1.98338702991653543235
  end

  def test_acos
    assert_complex_parts Calc::C(2,3).acos, 1.00014354247379721852, -1.98338702991653543235
  end

  def test_atan
    assert_complex_parts Calc::C(2,3).atan, 1.40992104959657552253, 0.22907268296853876630
  end

  def test_acot
    assert_complex_parts Calc::C(2,3).acot, 0.1608752771983210967, -0.22907268296853876630
  end

  def test_asec
    assert_complex_parts Calc::C(2,3).asec, 1.42041072246703465598, 0.2313346985739733145
  end

  def test_acsc
    assert_complex_parts Calc::C(2,3).acsc, 0.15038560432786196325, -0.23133469857397331455
  end

  def test_asinh
    assert_complex_parts Calc::C(2,3).asinh, 1.96863792579309629179, 0.96465850440760279204
  end

  def test_acosh
    assert_complex_parts Calc::C(2,3).acosh, 1.98338702991653543235, 1.00014354247379721852
  end

  def test_atanh
    assert_complex_parts Calc::C(2,3).atanh, 0.14694666622552975204, 1.33897252229449356112
  end

  def test_acoth
    assert_complex_parts Calc::C(2,3).acoth, 0.14694666622552975204, -0.23182380450040305810
  end

  def test_asech
    assert_complex_parts Calc::C(2,3).asech, 0.23133469857397331455, -1.42041072246703465598
  end

  def test_acsch
    assert_complex_parts Calc::C(2,3).acsch, 0.15735549884498542878, -0.22996290237720785451
  end
  
  def test_cot
    assert_complex_parts Calc::C(2,3).cot, -0.00373971037633695666, -0.99675779656935831046
  end

  def test_coth
    assert_complex_parts Calc::C(2,3).coth, 1.03574663776499539611, 0.01060478347033710175
  end

  def test_csc
    assert_complex_parts Calc::C(2,3).csc, 0.09047320975320743981, 0.04120098628857412646
  end

  def test_csch
    assert_complex_parts Calc::C(2,3).csch, -0.27254866146294019951, -0.04030057885689152187
  end

  def test_sec
    assert_complex_parts Calc::C(2,3).sec, -0.04167496441114427005, 0.09061113719623759653
  end

  def test_sech
    assert_complex_parts Calc::C(2,3).sech, -0.26351297515838930964, -0.03621163655876852087
  end

  def test_tan
    assert_complex_parts Calc::C(2,3).tan, -0.00376402564150424829, 1.00323862735360980145
  end

  def test_tanh
    assert_complex_parts Calc::C(2,3).tanh, 0.96538587902213312428, -0.00988437503832249372
  end

  def test_agd
    assert_complex_parts Calc::C(1,2).agd, 0.22751065843194319695, 1.422911462459226797
    assert_equal 0, Calc::C(0,0).agd
  end

  def test_gd
    assert_rational_in_epsilon 0.86576948323965862429, Calc::C(1).gd
    assert_complex_parts Calc::C(2,1).gd, 1.422911462459226797, 0.22751065843194319695
    assert_equal 0, Calc::C(0,0).gd

    assert_raises(Calc::MathError) { Calc::C(0, Calc::Q.pi/2).gd }
  end

  def test_inverse
    assert_complex_parts Calc::C(3,4).inverse, 0.12, -0.16
    assert_raises(Calc::MathError) { Calc::C(0,0).inverse }
  end

end
