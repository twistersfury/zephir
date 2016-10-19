
/* This file was generated automatically by Zephir do not modify it! */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>

#include "php_ext.h"
#include "%PROJECT_LOWER_SAFE%.h"

#include <ext/standard/info.h>

#include <Zend/zend_operators.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_interfaces.h>

#include "kernel/globals.h"
#include "kernel/main.h"
#include "kernel/fcall.h"
#include "kernel/memory.h"

/* Includes For Zend_Extension */
    #include "php_ini.h"
    #include "zend.h"
    #include "zend_extensions.h"
/* End Includes */

%EXTRA_INCLUDES%

%CLASS_ENTRIES%

ZEND_DECLARE_MODULE_GLOBALS(%PROJECT_LOWER%)

PHP_INI_BEGIN()
	%PROJECT_INI_ENTRIES%
PHP_INI_END()

static PHP_MINIT_FUNCTION(%PROJECT_LOWER%)
{
	REGISTER_INI_ENTRIES();
	%CLASS_INITS%
	return SUCCESS;
}

#ifndef ZEPHIR_RELEASE
static PHP_MSHUTDOWN_FUNCTION(%PROJECT_LOWER%)
{
	zephir_deinitialize_memory(TSRMLS_C);
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
#endif

/**
 * Initialize globals on each request or each thread started
 */
static void php_zephir_init_globals(zend_%PROJECT_LOWER%_globals *%PROJECT_LOWER%_globals TSRMLS_DC)
{
	%PROJECT_LOWER%_globals->initialized = 0;

	/* Memory options */
	%PROJECT_LOWER%_globals->active_memory = NULL;

	/* Virtual Symbol Tables */
	%PROJECT_LOWER%_globals->active_symbol_table = NULL;

	/* Cache Enabled */
	%PROJECT_LOWER%_globals->cache_enabled = 1;

	/* Recursive Lock */
	%PROJECT_LOWER%_globals->recursive_lock = 0;

	/* Static cache */
	memset(%PROJECT_LOWER%_globals->scache, '\0', sizeof(zephir_fcall_cache_entry*) * ZEPHIR_MAX_CACHE_SLOTS);

%INIT_GLOBALS%
}

/**
 * Initialize globals only on each thread started
 */
static void php_zephir_init_module_globals(zend_%PROJECT_LOWER%_globals *%PROJECT_LOWER%_globals TSRMLS_DC)
{
%INIT_MODULE_GLOBALS%
}

static PHP_RINIT_FUNCTION(%PROJECT_LOWER%)
{

	zend_%PROJECT_LOWER%_globals *%PROJECT_LOWER%_globals_ptr;
#ifdef ZTS
	tsrm_ls = ts_resource(0);
#endif
	%PROJECT_LOWER%_globals_ptr = ZEPHIR_VGLOBAL;

	php_zephir_init_globals(%PROJECT_LOWER%_globals_ptr TSRMLS_CC);
	zephir_initialize_memory(%PROJECT_LOWER%_globals_ptr TSRMLS_CC);

%INITIALIZERS%
	return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(%PROJECT_LOWER%)
{
	%DESTRUCTORS%
	zephir_deinitialize_memory(TSRMLS_C);
	return SUCCESS;
}

static PHP_MINFO_FUNCTION(%PROJECT_LOWER%)
{
	php_info_print_box_start(0);
	php_printf("%s", PHP_%PROJECT_UPPER%_DESCRIPTION);
	php_info_print_box_end();

	php_info_print_table_start();
	php_info_print_table_header(2, PHP_%PROJECT_UPPER%_NAME, "enabled");
	php_info_print_table_row(2, "Author", PHP_%PROJECT_UPPER%_AUTHOR);
	php_info_print_table_row(2, "Version", PHP_%PROJECT_UPPER%_VERSION);
	php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__ );
	php_info_print_table_row(2, "Powered by Zephir", "Version " PHP_%PROJECT_UPPER%_ZEPVERSION);
	php_info_print_table_end();
%EXTENSION_INFO%
	DISPLAY_INI_ENTRIES();
}

static PHP_GINIT_FUNCTION(%PROJECT_LOWER%)
{
	php_zephir_init_globals(%PROJECT_LOWER%_globals TSRMLS_CC);
	php_zephir_init_module_globals(%PROJECT_LOWER%_globals TSRMLS_CC);
}

static PHP_GSHUTDOWN_FUNCTION(%PROJECT_LOWER%)
{

}

%FE_HEADER%
zend_function_entry php_%PROJECT_LOWER_SAFE%_functions[] = {
%FE_ENTRIES%
};

zend_module_entry %PROJECT_LOWER_SAFE%_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	NULL,
	PHP_%PROJECT_UPPER%_EXTNAME,
	php_%PROJECT_LOWER_SAFE%_functions,
	PHP_MINIT(%PROJECT_LOWER%),
#ifndef ZEPHIR_RELEASE
	PHP_MSHUTDOWN(%PROJECT_LOWER%),
#else
	NULL,
#endif
	PHP_RINIT(%PROJECT_LOWER%),
	PHP_RSHUTDOWN(%PROJECT_LOWER%),
	PHP_MINFO(%PROJECT_LOWER%),
	PHP_%PROJECT_UPPER%_VERSION,
	ZEND_MODULE_GLOBALS(%PROJECT_LOWER%),
	PHP_GINIT(%PROJECT_LOWER%),
	PHP_GSHUTDOWN(%PROJECT_LOWER%),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_%PROJECT_UPPER%
ZEND_GET_MODULE(%PROJECT_LOWER_SAFE%)
#endif

/* Zend_Extension Stuff */

int %PROJECT_LOWER_SAFE%_zend_startup(zend_extension *extension)
{
	TSRMLS_FETCH();
    CG(compiler_options) = CG(compiler_options) | ZEND_COMPILE_EXTENDED_INFO;
	return SUCCESS;
}

void %PROJECT_LOWER_SAFE%_zend_shutdown(zend_extension *extension)
{

}

void %PROJECT_LOWER_SAFE%_activate()
{

}

void %PROJECT_LOWER_SAFE%_deactivate()
{

}

void %PROJECT_LOWER_SAFE%_message_handler(int message, void *arg)
{

}

void %PROJECT_LOWER_SAFE%_op_array_handler(zend_op_array *op_array)
{

}

void %PROJECT_LOWER_SAFE%_statement_handler(zend_op_array *op_array)
{

}

void %PROJECT_LOWER_SAFE%_fcall_begin_handler(zend_op_array *op_array)
{

}

void %PROJECT_LOWER_SAFE%_fcall_end_handler(zend_op_array *op_array)
{

}

void %PROJECT_LOWER_SAFE%_op_array_ctor(zend_op_array *op_array)
{

}

void %PROJECT_LOWER_SAFE%_op_array_dtor(zend_op_array *op_array)
{

}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API    ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	PHP_%PROJECT_UPPER%_NAME,
    PHP_%PROJECT_UPPER%_VERSION,
	PHP_%PROJECT_UPPER%_AUTHOR,
	PHP_%PROJECT_UPPER%_WEBSITE,
	PHP_%PROJECT_UPPER%_COPYRIGHT,
	%PROJECT_LOWER_SAFE%_zend_startup,
	%PROJECT_LOWER_SAFE%_zend_shutdown,
    %PROJECT_LOWER_SAFE%_activate,             /* activate_func_t */
    %PROJECT_LOWER_SAFE%_deactivate,           /* deactivate_func_t */
    %PROJECT_LOWER_SAFE%_message_handler,      /* message_handler_func_t */
    %PROJECT_LOWER_SAFE%_op_array_handler,     /* op_array_handler_func_t */
	%PROJECT_LOWER_SAFE%_statement_handler,    /* statement_handler_func_t */
    %PROJECT_LOWER_SAFE%_fcall_begin_handler,  /* fcall_begin_handler_func_t */
    %PROJECT_LOWER_SAFE%_fcall_end_handler,    /* fcall_end_handler_func_t */
    %PROJECT_LOWER_SAFE%_op_array_ctor,        /* op_array_ctor_func_t */
    %PROJECT_LOWER_SAFE%_op_array_dtor,        /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};