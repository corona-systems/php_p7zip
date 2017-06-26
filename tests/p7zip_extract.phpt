--TEST--
p7zip_extract() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$handle = p7zip_open(dirname(__FILE__)."/test.7z");
echo p7zip_extract($handle) ? "OK\n" : "Failure";
p7zip_close($handle);
$test1 = file_get_contents(dirname(__FILE__)."/test.text");
$test2 = file_get_contents(dirname(__FILE__)."/test2.text");
echo $test1 == $test2 ? "OK\n" : "Failure";
unlink(dirname(__FILE__)."/test.text");
<<<<<<< HEAD
$handle = p7zip_open(dirname(__FILE__)."/test.7z");
echo p7zip_extract($handle, FALSE, "../") ? "OK\n" : "Failure";
p7zip_close($handle);
$test1 = file_get_contents(dirname(__FILE__)."/../test.text");
=======
$handle = p7zip_open(dirname(__FILE__)."/test.7z", FALSE, "../");
echo p7zip_extract($handle) ? "OK\n" : "Failure";
p7zip_close($handle);
$test1 = file_get_contents(dirname(__FILE__)."../test.text");
>>>>>>> 07ef6e1de351e9cff120618739ac64c87a661128
$test2 = file_get_contents(dirname(__FILE__)."/test2.text");
echo $test1 == $test2 ? "OK" : "Failure";
unlink(dirname(__FILE__)."/test.text");
?>
--EXPECT--
OK
OK
OK
OK
