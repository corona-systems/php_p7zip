--TEST--
p7zip_open() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
echo "Test case 1:";
$handle = p7zip_open("");
echo "Test case 2:";
$handle = p7zip_open("foo", "bar");
echo "Test case 3:\n";
$handle = p7zip_open("nonexisting.7z");
echo is_resource($handle) ? "OK" : "Failure";
?>
--EXPECTF--
Test case 1:
Warning: p7zip_open(): Invalid filename length in %s on line %d
Test case 2:
Warning: p7zip_open() expects exactly 1 parameter, 2 given in %s on line %d
Test case 3:
Failure
