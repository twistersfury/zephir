
extern zend_class_entry *stub_8__closure_ce;

ZEPHIR_INIT_CLASS(stub_8__closure);

PHP_METHOD(stub_8__closure, __invoke);
PHP_METHOD(stub_8__closure, __construct);

ZEND_BEGIN_ARG_INFO_EX(arginfo_stub_8__closure___invoke, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_stub_8__closure___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, abc)
ZEND_END_ARG_INFO()

ZEPHIR_INIT_FUNCS(stub_8__closure_method_entry) {
PHP_ME(stub_8__closure, __invoke, arginfo_stub_8__closure___invoke, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(stub_8__closure, __construct, arginfo_stub_8__closure___construct, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL|ZEND_ACC_CTOR)
	PHP_FE_END
};
