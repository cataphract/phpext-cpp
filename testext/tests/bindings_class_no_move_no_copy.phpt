--TEST--
Function bindings for non-copy and non-move constructible class
--FILE--
<?php
$c = new ClassNoMoveNoCopy(1);
try {
    $c2 = $c->newAdding(3);
} catch (Throwable $e) {
    var_dump($e->getMessage());
}
echo "\n";

$c2 = $c->addToThis(3);
var_dump($c2 === $c);
var_dump($c2->ival());
unset($c2);
echo "\n";

try {
    ClassNoMoveNoCopy::refToStatic();
} catch (Throwable $e) {
    var_dump($e->getMessage());
}
echo "\n";

ClassNoMoveNoCopy::addTo($c, 3);
var_dump($c->ival());
echo "\n";

ClassNoMoveNoCopy::addToOptional(null, 3);
ClassNoMoveNoCopy::addToOptional($c, 3);
var_dump($c->ival());
echo "\n";


?>
--EXPECT--
ClassNoMoveNoCopy constructor with i=1
ClassNoMoveNoCopy constructor with i=4
ClassNoMoveNoCopy destructor
string(167) "error converting value to zval: Cannot build PHP variable with native object built outside of PHP because its class ClassNoMoveNoCopy is not copy or move constructible"

bool(true)
int(4)

ClassNoMoveNoCopy constructor with i=4
string(159) "error converting value to zval: Cannot build PHP variable with native object built outside of PHP because its class ClassNoMoveNoCopy is not copy constructible"

int(7)

int(10)

ClassNoMoveNoCopy destructor
ClassNoMoveNoCopy destructor
