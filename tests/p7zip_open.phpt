--TEST--
p7zip_open() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$handle = p7zip_open(dirname(__FILE__)."/test.7z");
echo is_resource($handle) ? "OK" : "Failure";
?>
--EXPECT--
OK
