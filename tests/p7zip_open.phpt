--TEST--
p7zip_open() function
--SKIPIF--
<?php
/* $Id$ */
if(!extension_loaded('zip')) die('skip');
?>
--FILE--
<?php
$zip = p7zip_open(dirname(__FILE__)."/test.7zip");
echo is_resource($zip) ? "OK" : "Failure";
?>
--EXPECT--
OK
