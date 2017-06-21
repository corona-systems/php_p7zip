--TEST--
p7zip_test() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$handle = p7zip_open(dirname(__FILE__)."/test_corrupt.7z");
$error = p7zip_test($handle);
$error == SZ_ERROR_CRC ? "OK" : "Failure";
p7zip_close($handle);
?>
--EXPECT--
OK
