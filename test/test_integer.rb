require 'minitest_helper'

class TestInteger < Minitest::Test
  def test_class_exists
    refute_nil Calc::Z
  end

  def test_initialization
    assert_instance_of Calc::Z, Calc::Z.new(42)                   # Fixnum
    assert_instance_of Calc::Z, Calc::Z.new(4611686018427387904)  # Bignum
  end
end
