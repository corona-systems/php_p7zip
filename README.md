# php_p7zip
## A PHP extension for opening 7zip archives
---
The php_p7zip extension is based on the 7zip ANSI C Decoder, so all its limitations are the same.
See: [7cC.txt](https://github.com/corona-systems/lzma-sdk/blob/master/DOC/7zC.txt)
---
## Build instructions
```
cd php_p7zip
phpize
./configure
make
make test
```
If all tests work:
```
make install
```
---
## TODO:

* [ ] Support PHP Streams
* [ ] Build and test on Windows

