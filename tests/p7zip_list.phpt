--TEST--
p7zip_list() function
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php
$handle = p7zip_open(dirname(__FILE__)."/test.7z");
var_dump(p7zip_list($handle));
p7zip_close($handle);
?>
--EXPECT--
array(1) {
  [0]=>
  string(9) "test.text"
}
