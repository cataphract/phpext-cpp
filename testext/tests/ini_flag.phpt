--TEST--
Boolean ini flag
--FILE--
<?php
print_ini_flag();
ini_set('sample_flag', 'false');
print_ini_flag();
?>
--EXPECT--
true
false
