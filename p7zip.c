/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "Zend/zend_hash.h"
#include "Zend/zend_smart_str.h"

#include "php_p7zip.h"

#include "lzma-sdk/C/7zCrc.h"
#include "lzma-sdk/C/7zBuf.h"

/* If you declare any globals in php_p7zip.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(p7zip)
*/

/* True global resources - no need for thread safety here */
static int le_p7zip;
#define le_p7zip_name "7Zip File Descriptor"

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("p7zip.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_p7zip_globals, p7zip_globals)
    STD_PHP_INI_ENTRY("p7zip.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_p7zip_globals, p7zip_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_p7zip_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_p7zip_init_globals(zend_p7zip_globals *p7zip_globals)
{
	p7zip_globals->global_value = 0;
	p7zip_globals->global_string = NULL;
}
*/
/* }}} */

//static int le_p7zip_descriptor;

static void p7zip_free(zend_resource* rsrc){
    p7zip_file_t* file = (p7zip_file_t*) rsrc->ptr;
    
    if(file){
        SzArEx_Free(&file->db, &file->allocImp);
        File_Close(&file->archiveStream.file);
        efree(rsrc->ptr);
    }
    
    rsrc->ptr = NULL;
}

static void* SzAlloc(void *p, size_t size){
    return emalloc(size);
}

static void SzFree(void *p, void *address){
    efree(address);
}

static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int Buf_EnsureSize(CBuf *dest, size_t size){
    if (dest->size >= size)
        return 1;
    Buf_Free(dest, &g_Alloc);
    return Buf_Create(dest, size, &g_Alloc);
}

#ifndef _WIN32
#define _USE_UTF8
#endif

/* #define _USE_UTF8 */

#ifdef _USE_UTF8

#define _UTF8_START(n) (0x100 - (1 << (7 - (n))))

#define _UTF8_RANGE(n) (((UInt32)1) << ((n) * 5 + 6))

#define _UTF8_HEAD(n, val) ((Byte)(_UTF8_START(n) + (val >> (6 * (n)))))
#define _UTF8_CHAR(n, val) ((Byte)(0x80 + (((val) >> (6 * (n))) & 0x3F)))

static size_t Utf16_To_Utf8_Calc(const UInt16 *src, const UInt16 *srcLim){
    size_t size = 0;
    for (;;){
        UInt32 val;
        if (src == srcLim)
            return size;
        
        size++;
        val = *src++;
        
        if (val < 0x80)
            continue;
        
        if (val < _UTF8_RANGE(1)){
            size++;
            continue;
        }
        
        if (val >= 0xD800 && val < 0xDC00 && src != srcLim){
            UInt32 c2 = *src;
            if (c2 >= 0xDC00 && c2 < 0xE000){
                src++;
                size += 3;
                continue;
            }
        }
        
        size += 2;
    }
}

static Byte *Utf16_To_Utf8(Byte *dest, const UInt16 *src, const UInt16 *srcLim){
    for (;;){
        UInt32 val;
        if (src == srcLim)
            return dest;
        
        val = *src++;
        
        if (val < 0x80){
            *dest++ = (char)val;
            continue;
        }
        
        if (val < _UTF8_RANGE(1)){
            dest[0] = _UTF8_HEAD(1, val);
            dest[1] = _UTF8_CHAR(0, val);
            dest += 2;
            continue;
        }
        
        if (val >= 0xD800 && val < 0xDC00 && src != srcLim){
            UInt32 c2 = *src;
            if (c2 >= 0xDC00 && c2 < 0xE000){
                src++;
                val = (((val - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
                dest[0] = _UTF8_HEAD(3, val);
                dest[1] = _UTF8_CHAR(2, val);
                dest[2] = _UTF8_CHAR(1, val);
                dest[3] = _UTF8_CHAR(0, val);
                dest += 4;
                continue;
            }
        }
        
        dest[0] = _UTF8_HEAD(2, val);
        dest[1] = _UTF8_CHAR(1, val);
        dest[2] = _UTF8_CHAR(0, val);
        dest += 3;
    }
}

static SRes Utf16_To_Utf8Buf(CBuf *dest, const UInt16 *src, size_t srcLen){
    size_t destLen = Utf16_To_Utf8_Calc(src, src + srcLen);
    destLen += 1;
    if (!Buf_EnsureSize(dest, destLen))
        return SZ_ERROR_MEM;
    *Utf16_To_Utf8(dest->data, src, src + srcLen) = 0;
    return SZ_OK;
}

#endif

static SRes Utf16_To_Char(CBuf *buf, const UInt16 *s
    #ifndef _USE_UTF8
    , UINT codePage
    #endif
    ){
    unsigned len = 0;
    for (len = 0; s[len] != 0; len++);
    
    #ifndef _USE_UTF8
    {
        unsigned size = len * 3 + 100;
        if (!Buf_EnsureSize(buf, size))
            return SZ_ERROR_MEM;
        {
            buf->data[0] = 0;
            if (len != 0){
                char defaultChar = '_';
                BOOL defUsed;
                unsigned numChars = 0;
                numChars = WideCharToMultiByte(codePage, 0, s, len, (char *)buf->data, size, &defaultChar, &defUsed);
                if (numChars == 0 || numChars >= size)
                    return SZ_ERROR_FAIL;
                buf->data[numChars] = 0;
            }
            return SZ_OK;
        }
    }
    #else
    return Utf16_To_Utf8Buf(buf, s, len);
    #endif
}

#ifdef _WIN32
  #ifndef USE_WINDOWS_FILE
    static UINT g_FileCodePage = CP_ACP;
  #endif
  #define MY_FILE_CODE_PAGE_PARAM ,g_FileCodePage
#else
  #define MY_FILE_CODE_PAGE_PARAM
#endif

static WRes MyCreateDir(const UInt16 *name){
    #ifdef USE_WINDOWS_FILE
    
    return CreateDirectoryW(name, NULL) ? 0 : GetLastError();
    
    #else
    
    CBuf buf;
    WRes res;
    Buf_Init(&buf);
    RINOK(Utf16_To_Char(&buf, name MY_FILE_CODE_PAGE_PARAM));
    
    res =
    #ifdef _WIN32
    _mkdir((const char *)buf.data)
    #else
    mkdir((const char *)buf.data, 0777)
    #endif
    == 0 ? 0 : errno;
    Buf_Free(&buf, &g_Alloc);
    return res;
    
    #endif
}

static WRes OutFile_OpenUtf16(CSzFile *p, const UInt16 *name){
    #ifdef USE_WINDOWS_FILE
    return OutFile_OpenW(p, name);
    #else
    CBuf buf;
    WRes res;
    Buf_Init(&buf);
    RINOK(Utf16_To_Char(&buf, name MY_FILE_CODE_PAGE_PARAM));
    res = OutFile_Open(p, (const char *)buf.data);
    Buf_Free(&buf, &g_Alloc);
    return res;
    #endif
}

static SRes ConvertString(zend_string** str, const UInt16 *s, unsigned isDir){
    CBuf buf;
    SRes res;
    Buf_Init(&buf);
    res = Utf16_To_Char(&buf, s
    #ifndef _USE_UTF8
    , CP_OEMCP
    #endif
    );
    if (res == SZ_OK){
        *str = zend_string_init((const char*)buf.data, buf.size + (!isDir ? -1 : 0), 0);
        if(isDir)
            (*str)->val[buf.size - (size_t)2] = '/';
    }
    Buf_Free(&buf, &g_Alloc);
    return res;
}

static void UInt64ToStr(UInt64 value, char *s){
    char temp[32];
    int pos = 0;
    do{
        temp[pos++] = (char)('0' + (unsigned)(value % 10));
        value /= 10;
    }
    while (value != 0);
    do
        *s++ = temp[--pos];
    while (pos);
    *s = '\0';
}

static char *UIntToStr(char *s, unsigned value, int numDigits){
    char temp[16];
    int pos = 0;
    do
        temp[pos++] = (char)('0' + (value % 10));
    while (value /= 10);
    for (numDigits -= pos; numDigits > 0; numDigits--)
        *s++ = '0';
    do
        *s++ = temp[--pos];
    while (pos);
    *s = '\0';
    return s;
}

static void UIntToStr_2(char *s, unsigned value){
    s[0] = (char)('0' + (value / 10));
    s[1] = (char)('0' + (value % 10));
}

#define PERIOD_4 (4 * 365 + 1)
#define PERIOD_100 (PERIOD_4 * 25 - 1)
#define PERIOD_400 (PERIOD_100 * 4 + 1)

static void ConvertFileTimeToString(const CNtfsFileTime *nt, char *s){
    unsigned year, mon, hour, min, sec;
    Byte ms[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    unsigned t;
    UInt32 v;
    UInt64 v64 = nt->Low | ((UInt64)nt->High << 32);
    v64 /= 10000000;
    sec = (unsigned)(v64 % 60); v64 /= 60;
    min = (unsigned)(v64 % 60); v64 /= 60;
    hour = (unsigned)(v64 % 24); v64 /= 24;
    
    v = (UInt32)v64;
    
    year = (unsigned)(1601 + v / PERIOD_400 * 400);
    v %= PERIOD_400;
    
    t = v / PERIOD_100; if (t ==  4) t =  3; year += t * 100; v -= t * PERIOD_100;
    t = v / PERIOD_4;   if (t == 25) t = 24; year += t * 4;   v -= t * PERIOD_4;
    t = v / 365;        if (t ==  4) t =  3; year += t;       v -= t * 365;
    
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
        ms[1] = 29;
    for (mon = 0;; mon++){
        unsigned d = ms[mon];
        if (v < d)
            break;
        v -= d;
    }
    s = UIntToStr(s, year, 4); *s++ = '-';
    UIntToStr_2(s, mon + 1); s[2] = '-'; s += 3;
    UIntToStr_2(s, (unsigned)v + 1); s[2] = ' '; s += 3;
    UIntToStr_2(s, hour); s[2] = ':'; s += 3;
    UIntToStr_2(s, min); s[2] = ':'; s += 3;
    UIntToStr_2(s, sec); s[2] = 0;
}

static void GetAttribString(UInt32 wa, Bool isDir, char *s){
    #ifdef USE_WINDOWS_FILE
    s[0] = (char)(((wa & FILE_ATTRIBUTE_DIRECTORY) != 0 || isDir) ? 'D' : '.');
    s[1] = (char)(((wa & FILE_ATTRIBUTE_READONLY ) != 0) ? 'R': '.');
    s[2] = (char)(((wa & FILE_ATTRIBUTE_HIDDEN   ) != 0) ? 'H': '.');
    s[3] = (char)(((wa & FILE_ATTRIBUTE_SYSTEM   ) != 0) ? 'S': '.');
    s[4] = (char)(((wa & FILE_ATTRIBUTE_ARCHIVE  ) != 0) ? 'A': '.');
    s[5] = 0;
    #else
    s[0] = (char)(((wa & (1 << 4)) != 0 || isDir) ? 'D' : '.');
    s[1] = 0;
    #endif
}

/* {{{ proto Resource p7zip_open(string filename)
   Opens a 7zip file */

PHP_FUNCTION(p7zip_open){
    zend_string* filename;
    char resolved_path[MAXPATHLEN + 1];
    p7zip_file_t* file;
    SRes res;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "P", &filename) == FAILURE) {
        return;
    }
    
    if(ZSTR_LEN(filename) == 0){
        php_error_docref(NULL, E_WARNING, "Invalid filename length");
        RETURN_FALSE;
    }
    
    if(php_check_open_basedir(ZSTR_VAL(filename))){
        RETURN_FALSE;
    }
    
    if(!expand_filepath(ZSTR_VAL(filename), resolved_path)) {
        RETURN_FALSE;
    }
        
    file = (p7zip_file_t*) emalloc(sizeof(p7zip_file_t));
    
    file->allocImp.Alloc = SzAlloc;
    file->allocImp.Free = SzFree;

    file->allocTempImp.Alloc = SzAlloc;
    file->allocTempImp.Free = SzFree;
    
    if (InFile_Open(&file->archiveStream.file, resolved_path)){
        efree(file);
        RETURN_FALSE;
    }
    
    FileInStream_CreateVTable(&file->archiveStream);
    LookToRead_CreateVTable(&file->lookStream, False);
  
    file->lookStream.realStream = &file->archiveStream.s;
    LookToRead_Init(&file->lookStream);
    
    SzArEx_Init(&file->db);
  
    res = SzArEx_Open(&file->db, &file->lookStream.s, &file->allocImp, &file->allocTempImp);
    
    if(res != SZ_OK){
        php_error_docref(NULL, E_ERROR, "Can't open archive");
        SzArEx_Free(&file->db, &file->allocImp);
        File_Close(&file->archiveStream.file);
        efree(file);
        RETURN_FALSE;
    }
    
    RETURN_RES(zend_register_resource(file, le_p7zip));
}
/* }}} */

/* {{{ proto void p7zip_close(Resource zip)
   Closes a 7zip file */

PHP_FUNCTION(p7zip_close){
    zval* val;
    p7zip_file_t* file;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &val) == FAILURE) {
        return;
    }

    if ((file = (p7zip_file_t*) zend_fetch_resource(Z_RES_P(val), le_p7zip_name, le_p7zip)) == NULL) {
        RETURN_FALSE;
    }
    
    zend_list_close(Z_RES_P(val));
}
/* }}} */

PHP_FUNCTION(p7zip_test){
    zval* val;
    p7zip_file_t* file;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &val) == FAILURE) {
        return;
    }

    if ((file = (p7zip_file_t*) zend_fetch_resource(Z_RES_P(val), le_p7zip_name, le_p7zip)) == NULL) {
        RETURN_FALSE;
    }
    
    UInt32 blockIndex = 0xFFFFFFFF;
    Byte *outBuffer = 0;
    size_t outBufferSize = 0;
    UInt32 i;
    SRes res;
  
    for (i = 0; i < file->db.NumFiles; i++){
        size_t offset = 0;
        size_t outSizeProcessed = 0;
        
        unsigned isDir = SzArEx_IsDir(&file->db, i);
        
        if(isDir)
            continue;
        
        res = SzArEx_Extract(&file->db, &file->lookStream.s, i,
              &blockIndex, &outBuffer, &outBufferSize,
              &offset, &outSizeProcessed,
              &file->allocImp, &file->allocTempImp);
        
        if (res != SZ_OK)
            break;
    }
    
    IAlloc_Free(&file->allocImp, outBuffer);
    
    if(res != SZ_OK)
        RETURN_LONG(res);
    
    RETURN_TRUE;
    
}

PHP_FUNCTION(p7zip_list){
    zval* val;
    p7zip_file_t* file;
    zend_bool full_info = 0;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|b", &val, &full_info) == FAILURE) {
        return;
    }

    if ((file = (p7zip_file_t*) zend_fetch_resource(Z_RES_P(val), le_p7zip_name, le_p7zip)) == NULL) {
        RETURN_FALSE;
    }
    
    UInt32 i;
    SRes res = SZ_OK;
    UInt16 *temp = NULL;
    size_t tempSize = 0;
    
    HashTable* ht;
    ALLOC_HASHTABLE(ht);
    zend_hash_init(ht, file->db.NumFiles, NULL, NULL, 0);
    
    for (i = 0; i < file->db.NumFiles; i++){
        zend_string* filename;
        size_t len;
        unsigned isDir = SzArEx_IsDir(&file->db, i);
        len = SzArEx_GetFileNameUtf16(&file->db, i, NULL);
        if (len > tempSize){
          SzFree(NULL, temp);
          tempSize = len;
          temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
          if (!temp){
            res = SZ_ERROR_MEM;
            break;
          }
        }
        
        if (res != SZ_OK)
            break;  
        
        SzArEx_GetFileNameUtf16(&file->db, i, temp);
        res = ConvertString(&filename, temp, isDir);
        
        if(res != SZ_OK){
            zend_string_release(filename);
            break;
        }
        
        zval entry;
        ZVAL_STR(&entry, filename);
        
        if(zend_hash_index_add_new(ht, i, &entry) == NULL){
            zend_string_release(filename);
            RETURN_FALSE;
        }
        
        zend_string_release(filename);
    }
    
    SzFree(NULL, temp);
    
    if(res != SZ_OK)
        RETURN_LONG(res);
    
    ZVAL_ARR(return_value, ht);
    
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(p7zip)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
        
        le_p7zip = zend_register_list_destructors_ex(p7zip_free, NULL, le_p7zip_name, module_number);
        
        REGISTER_LONG_CONSTANT("SZ_OK", SZ_OK, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("SZ_ERROR_UNSUPPORTED", SZ_ERROR_UNSUPPORTED, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("SZ_ERROR_CRC", SZ_ERROR_CRC, CONST_CS | CONST_PERSISTENT);
        
        CrcGenerateTable();
        
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(p7zip)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(p7zip)
{
#if defined(COMPILE_DL_P7ZIP) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(p7zip)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(p7zip)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "p7zip support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_p7zip_open, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_p7zip_close, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_p7zip_test, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_p7zip_list, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
        ZEND_ARG_INFO(0, full_info)
ZEND_END_ARG_INFO()

/* {{{ p7zip_functions[]
 *
 * Every user visible function must have an entry in p7zip_functions[].
 */
const zend_function_entry p7zip_functions[] = {
        PHP_FE(p7zip_open, arginfo_p7zip_open)
        PHP_FE(p7zip_close, arginfo_p7zip_close)
        PHP_FE(p7zip_test, arginfo_p7zip_test)
        PHP_FE(p7zip_list, arginfo_p7zip_list)
	PHP_FE_END	/* Must be the last line in p7zip_functions[] */
};
/* }}} */

/* {{{ p7zip_module_entry
 */
zend_module_entry p7zip_module_entry = {
	STANDARD_MODULE_HEADER,
	"p7zip",
	p7zip_functions,
	PHP_MINIT(p7zip),
	PHP_MSHUTDOWN(p7zip),
	PHP_RINIT(p7zip),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(p7zip),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(p7zip),
	PHP_P7ZIP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_P7ZIP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(p7zip)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
