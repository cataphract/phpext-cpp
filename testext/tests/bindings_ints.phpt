--TEST--
Function int bindings
--FILE--
<?php
var_dump(sum_ints(1,3));
var_dump(sum_ints("1", "3"));
var_dump(sum_ints_const(1,3));

$x = 1;
add_to($x, 3);
echo "Value of \$x is $x after add_to\n";

increment_opt();
increment_opt($x);
echo "Value of \$x is $x after increment_opt\n";

// error conditions
try {
    var_dump(sum_ints(2147483648, 3));
} catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try {
    var_dump(sum_ints("a", 3));
} catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try {
    var_dump(sum_ints(3));
} catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
int(4)
int(4)
int(4)
Value of $x is 4 after add_to
Value of $x is 5 after increment_opt
sum_ints() has for parameter 0 a int, but the value is not within the accepted bounds
Argument 1 passed to sum_ints() must be of the type int, string given
sum_ints() expects exactly 2 parameters, 1 given
