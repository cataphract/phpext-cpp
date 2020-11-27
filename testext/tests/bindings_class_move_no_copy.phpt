--TEST--
Function bindings for move constructible-only class
--FILE--
<?php
$c = new ClassMoveNoCopy(1);
try {
    $c2 = $c->newAdding(3);
    var_dump($c2->ival());
    var_dump($c->ival()); // exception (DESTRUCTED state)
} catch (Throwable $e) {
    var_dump($e->getMessage());
}

?>
--EXPECT--
ClassMoveNoCopy constructor with i=1
ClassMoveNoCopy constructor with i=4
ClassMoveNoCopy move constructor
ClassMoveNoCopy destructor
int(4)
int(1)
ClassMoveNoCopy destructor
ClassMoveNoCopy destructor
