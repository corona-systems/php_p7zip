dnl $Id$
dnl config.m4 for extension p7zip

PHP_ARG_ENABLE(p7zip, whether to enable p7zip support,
[  --enable-p7zip           Enable p7zip support])

7zip_sources="  lzma-sdk/C/7zAlloc.c lzma-sdk/C/7zArcIn.c\
                lzma-sdk/C/7zBuf.c lzma-sdk/C/7zBuf2.c\
                lzma-sdk/C/7zCrc.c lzma-sdk/C/7zCrcOpt.c\
                lzma-sdk/C/7zDec.c lzma-sdk/C/CpuArch.c\
                lzma-sdk/C/Delta.c lzma-sdk/C/LzmaDec.c\
                lzma-sdk/C/Lzma2Dec.c lzma-sdk/C/Bra.c\
                lzma-sdk/C/Bra86.c lzma-sdk/C/BraIA64.c\
                lzma-sdk/C/Bcj2.c lzma-sdk/C/Ppmd7.c\
                lzma-sdk/C/Ppmd7Dec.c lzma-sdk/C/7zFile.c\
                lzma-sdk/C/7zStream.c"

if test "$PHP_P7ZIP" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-p7zip -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/p7zip.h"  # you most likely want to change this
  dnl if test -r $PHP_P7ZIP/$SEARCH_FOR; then # path given as parameter
  dnl   P7ZIP_DIR=$PHP_P7ZIP
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for p7zip files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       P7ZIP_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$P7ZIP_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the p7zip distribution])
  dnl fi

  dnl # --with-p7zip -> add include path
  dnl PHP_ADD_INCLUDE($P7ZIP_DIR/include)

  dnl # --with-p7zip -> check for lib and symbol presence
  dnl LIBNAME=p7zip # you may want to change this
  dnl LIBSYMBOL=p7zip # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $P7ZIP_DIR/$PHP_LIBDIR, P7ZIP_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_P7ZIPLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong p7zip lib version or lib not found])
  dnl ],[
  dnl   -L$P7ZIP_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(P7ZIP_SHARED_LIBADD)
  

  PHP_NEW_EXTENSION(p7zip, p7zip.c $7zip_sources, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
