--TEST--
p7zip_close() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$zip = p7zip_open(dirname(__FILE__)."/test.7z");
if(!is_resource($zip))
    die("Failure");
p7zip_close($zip);
echo "OK";
?>
--EXPECT--
OK
