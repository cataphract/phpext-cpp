#pragma once

#include <php.h>
#include <utility>
#include <string>

namespace zend {

template<typename Char, Char ... Cs>
class ct_string : std::integer_sequence<Char, Cs...> {
    constexpr static const Char value[] {Cs..., 0};
public:
    constexpr ct_string() noexcept {}
    static constexpr size_t length() noexcept{
        return sizeof...(Cs);
    }
    constexpr operator const Char *() const noexcept {
        return value;
    }
};

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif
template<typename Char, Char ... Cs>
constexpr auto operator"" _cs() -> ct_string<Char, Cs ...> {
    return {};
}
#ifdef __clang__
#   pragma clang diagnostic pop
#endif

static constexpr zend_ulong ze_hash(const char *str, size_t len)
{
    zend_ulong hash = Z_UL(5381);
    for (size_t i = 0; i < len; i++) {
        hash = hash * 33 + static_cast<zend_ulong>(str[i]);
    }

    return hash | (Z_UL(1) << (sizeof(zend_ulong) * 8 - 1));
}
static_assert(ze_hash("foobar foobar", 13) == 0x8f6ab01693615317UL);


template<size_t N>
struct zend_string_static {
    zend_refcounted_h gc;
    zend_ulong h; /* hash value */
    size_t len;
    char val[N];

    template<size_t... Is>
    constexpr zend_string_static(const char (&c)[N], std::index_sequence<Is...>)
        : gc{1, {IS_STRING | (IS_STR_PERSISTENT << GC_FLAGS_SHIFT)}},
          h{ze_hash(c, N - 1)}, len{N - 1}, val{c[Is]...} {}

    constexpr zend_string_static(const char (&c)[N])
        : zend_string_static{c, std::make_index_sequence<N>()} {}

    constexpr operator zend_string *() {
        return reinterpret_cast<zend_string*>(this);
    }
};
// suppress CTAD warning
template<size_t N>
zend_string_static(const char (&)[N]) -> zend_string_static<N>;
static_assert(
        zend_string_static{"foobar foobar"}.h == 0x8f6ab01693615317UL,
        "Objects of type zend_string_static cannot be created at compile time");


struct zstring_view : std::string_view {
    zstring_view(zend_string *zstr) :
        std::string_view{ZSTR_VAL(zstr), ZSTR_LEN(zstr)} {}

    operator ::zend_string*() {
        return reinterpret_cast<zend_string *>(const_cast<char *>(
                this->data() - offsetof(zend_string, val)));
    }
};
}
