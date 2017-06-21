--TEST--
Check for p7zip presence
--SKIPIF--
<?php if (!extension_loaded("p7zip")) print "skip"; ?>
--FILE--
<?php 
echo "p7zip extension is available";
?>
--EXPECT--
p7zip extension is available
