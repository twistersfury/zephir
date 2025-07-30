
#ifdef HAVE_CONFIG_H
#include "../ext_config.h"
#endif

#include <php.h>
#include "../php_ext.h"
#include "../ext.h"

#include <Zend/zend_operators.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_interfaces.h>

#include "kernel/main.h"
#include "kernel/object.h"
#include "kernel/operators.h"
#include "kernel/memory.h"


ZEPHIR_INIT_CLASS(stub_8__closure)
{
	ZEPHIR_REGISTER_CLASS(stub, 8__closure, stub, 8__closure, stub_8__closure_method_entry, ZEND_ACC_FINAL_CLASS);

	zend_declare_property_null(stub_8__closure_ce, SL("abc"), ZEND_ACC_PUBLIC);
	return SUCCESS;
}

PHP_METHOD(stub_8__closure, __invoke)
{
	zval abc, _0;
	zephir_method_globals *ZEPHIR_METHOD_GLOBALS_PTR = NULL;
	zval *this_ptr = getThis();

	ZVAL_UNDEF(&abc);
	ZVAL_UNDEF(&_0);
	ZEPHIR_METHOD_GLOBALS_PTR = pecalloc(1, sizeof(zephir_method_globals), 0);
	zephir_memory_grow_stack(ZEPHIR_METHOD_GLOBALS_PTR, __func__);

	zephir_read_property(&_0, this_ptr, ZEND_STRL("abc"), PH_NOISY_CC | PH_READONLY);
	ZEPHIR_CPY_WRT(&abc, &_0);
	RETURN_MM_LONG((zephir_get_numberval(&abc) + 1));
}

PHP_METHOD(stub_8__closure, __construct)
{
	zephir_method_globals *ZEPHIR_METHOD_GLOBALS_PTR = NULL;
	zval *abc = NULL, abc_sub, _0;
	zval *this_ptr = getThis();

	ZVAL_UNDEF(&abc_sub);
	ZVAL_UNDEF(&_0);
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(abc)
	ZEND_PARSE_PARAMETERS_END();
	ZEPHIR_METHOD_GLOBALS_PTR = pecalloc(1, sizeof(zephir_method_globals), 0);
	zephir_memory_grow_stack(ZEPHIR_METHOD_GLOBALS_PTR, __func__);
	zephir_fetch_params(1, 1, 0, &abc);
	ZEPHIR_SEPARATE_PARAM(abc);
	zephir_read_property(&_0, this_ptr, ZEND_STRL("abc"), PH_NOISY_CC | PH_READONLY);
	ZEPHIR_CPY_WRT(abc, &_0);
	ZEPHIR_MM_RESTORE();
}

