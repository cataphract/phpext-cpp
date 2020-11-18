#pragma once
#include <php.h>
#include <array>
#include <streambuf>
#include <ostream>

namespace zend {

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wweak-vtables"
#endif
class PHPstreambuf : public std::streambuf {
    std::array<char, 4096> buffer;
protected:
    int_type overflow(int_type c) noexcept override {
        php_output_write(pbase(), static_cast<size_t>(pptr() - pbase()));
        setp(pbase(), epptr());
        if (c != traits_type::eof()) {
            *pptr() = static_cast<char>(c);
            pbump(1);
        }
        return 0;
    }

    int sync() noexcept override {
        return overflow(traits_type::eof());
    }

public:
    PHPstreambuf() {
        std::streambuf::setp(buffer.begin(), buffer.end());
    }
};

class PHPoutstream : public std::ostream {
    PHPstreambuf buf;
public:
    PHPoutstream() : std::ostream{&buf} {}
};
#ifdef __clang__
#   pragma clang diagnostic pop
#endif

inline PHPoutstream pout{};
}
