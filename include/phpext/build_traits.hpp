#pragma once

#include <php.h>

namespace zend {
#ifdef ZTS
#define ZEND_ENABLE_STATIC_TSRMLS_CACHE 1
using zts_build = std::true_type;
#else
using zts_build = std::false_type;
#endif
}
