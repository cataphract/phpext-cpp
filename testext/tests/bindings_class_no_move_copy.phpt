--TEST--
Function bindings for copy and non-move constructible class
--FILE--
<?php
$c = new ClassNoMoveCopy(1);
$c2 = new ClassNoMoveCopy(3);
$c3 = $c->newAdding($c2);
var_dump($c3->ival());
$c3 = null;
echo "\n";

$c3 = $c->newAddingRef($c2);
var_dump($c3->ival());


?>
--EXPECT--
ClassNoMoveCopy constructor with i=1
ClassNoMoveCopy constructor with i=3
ClassNoMoveCopy copy constructor
ClassNoMoveCopy constructor with i=4
ClassNoMoveCopy destructor
ClassNoMoveCopy copy constructor
ClassNoMoveCopy destructor
int(4)
ClassNoMoveCopy destructor

ClassNoMoveCopy constructor with i=4
ClassNoMoveCopy copy constructor
ClassNoMoveCopy destructor
int(4)
ClassNoMoveCopy destructor
ClassNoMoveCopy destructor
ClassNoMoveCopy destructor
