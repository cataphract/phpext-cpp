INCLUDES=$(sed 's/-I/-isystem /g' <<< $INCLUDES)

PHP_ARG_ENABLE(testext, whether to enable test extension,
  [  --enable-testext         Enable test extension], yes)
PHP_REQUIRE_CXX()
PHP_ADD_INCLUDE(../include)
PHP_SUBST(TESTEXT_SHARED_LIBADD)
PHP_NEW_EXTENSION(testext, main.cpp classes.cpp, $ext_shared,,-std=c++17 -Wall -pedantic -fvisibility=hidden -Weverything -Wno-nullability-completeness -Wno-missing-braces -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-exit-time-destructors -Wno-global-constructors -Wno-shadow-field-in-constructor -Wno-shadow-field -Wno-cast-align -Wno-missing-field-initializers)
