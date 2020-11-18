#pragma once
#include <php.h>
#include <cstddef>
#include <vector>
#include <string>

namespace zend {

namespace zmm {
template<typename T>
struct ZendMMAllocator {
    using value_type = T;

    T *allocate(size_t num) { return static_cast<T *>(emalloc(num)); }
    T *allocate(size_t num, [[maybe_unused]] const void *hint) {
        return static_cast<T *>(allocate(num));
    }
    void deallocate(T *ptr, [[maybe_unused]] size_t num) {
        if (ptr) {
            efree(static_cast<void *>(ptr));
        }
    }

    ZendMMAllocator() = default;
    ZendMMAllocator(const ZendMMAllocator &) = default;
    ZendMMAllocator(ZendMMAllocator &&) = default;
    ZendMMAllocator &operator=(const ZendMMAllocator &) = default;
    ZendMMAllocator &operator=(ZendMMAllocator &&) = default;
};

template<typename T>
using vector = std::vector<T, ZendMMAllocator<T>>;
using string =
        std::basic_string<char, std::char_traits<char>, ZendMMAllocator<char>>;
} // namespace zmm
}
