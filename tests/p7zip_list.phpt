--TEST--
p7zip_list() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$handle = p7zip_open(dirname(__FILE__)."/test.7z");
var_dump(p7zip_list($handle));
p7zip_close($handle);
$handle = p7zip_open(dirname(__FILE__)."/test2.7z");
var_dump(p7zip_list($handle));
p7zip_close($handle);
$handle = p7zip_open(dirname(__FILE__)."/test_dir.7z");
var_dump(p7zip_list($handle));
p7zip_close($handle);
?>
--EXPECT--
array(1) {
  ["test.txt"]=>
  NULL
}
array(2) {
  ["test.text"]=>
  NULL
  ["test2.text"]=>
  NULL
}
array(1) {
  ["test/test.text"]=>
  NULL
}
