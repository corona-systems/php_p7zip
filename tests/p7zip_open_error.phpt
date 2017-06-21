--TEST--
p7zip_open() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
echo "Test case 1:"
$zip = p7zip_open("");
echo "Test case 2:"
$zip = p7zip_open("foo", "bar");
echo "Test case 3:"
$zip = p7zip_open("nonexisting.7z");
echo is_resource($zip) ? "OK" : "Failure";
echo "Test case 4:"
$zip = p7zip_open(dirname(__FILE__)."/test_corrupted.7z");
?>
--EXPECTF--
Test case 1:
Invalid filename length
Test case 2:
Warning: p7zip_open() expects exactly 1 parameter, 2 given in %s on line %d
Test case 3:
Failure
Test case 4:
CRC error
