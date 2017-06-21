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
#include "php_p7zip.h"

#include "lzma-sdk/C/7zCrc.h"

/* If you declare any globals in php_p7zip.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(p7zip)
*/

/* True global resources - no need for thread safety here */
static int le_p7zip;
#define le_p7zip_descriptor_name "7Zip File Descriptor"

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

static int le_p7zip_descriptor;

static void* SzAlloc(void *p, size_t size){
    return emalloc(size);
}

static void SzFree(void *p, void *address){
  efree(address);
}

static void p7zip_free(zend_resource* rsrc){
    p7zip_file_t* file = (p7zip_file_t*) rsrc->ptr;
    
    if(file){
        SzArEx_Free(&file->db, &file->allocImp);
        File_Close(&file->archiveStream.file);
        efree(rsrc->ptr);
    }
    
    rsrc->ptr = NULL;
}

/* {{{ proto Resource p7zip_open(string filename)
   Opens a 7zip file */

PHP_FUNCTION(p7zip_open){
    zend_string* filename;
    char resolved_path[MAXPATHLEN + 1];
    p7zip_file_t* file;
    SRes res;
    WRes wres
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
    
    if ((wres = InFile_Open(&file->archiveStream.file, resolved_path))){
        php_error_docref(NULL, E_ERROR, "Can't open file %s %d", resolved_path, wres);
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
        php_error_docref(NULL, E_ERROR, "Can't create 7zip database");
        SzArEx_Free(&file->db, &file->allocImp);
        File_Close(&file->archiveStream.file);
        efree(file);
        RETURN_FALSE;
    }
    
    RETURN_RES(zend_register_resource(file, le_p7zip_descriptor));
}
/* }}} */

PHP_FUNCTION(p7zip_close){
    zval* val;
    p7zip_file_t* file;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &val) == FAILURE) {
        return;
    }

    if ((file = (p7zip_file_t*)zend_fetch_resource(Z_RES_P(val), le_p7zip_descriptor_name, le_p7zip_descriptor)) == NULL) {
        RETURN_FALSE;
    }
    
    p7zip_free(Z_RES_P(val));
}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(p7zip)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
        
        le_p7zip = zend_register_list_destructors_ex(p7zip_free, NULL, le_p7zip_descriptor_name, module_number);
        
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
	ZEND_ARG_INFO(0, zip)
ZEND_END_ARG_INFO()

/* {{{ p7zip_functions[]
 *
 * Every user visible function must have an entry in p7zip_functions[].
 */
const zend_function_entry p7zip_functions[] = {
        PHP_FE(p7zip_open, arginfo_p7zip_open)
        PHP_FE(p7zip_close, arginfo_p7zip_close)
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
