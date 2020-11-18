#pragma once

#include <php.h>
#include <vector>
#include <array>
#include <tuple>
#include <utility>
#include "build_traits.hpp"
#include "conversions.hpp"

namespace zend {
    
struct empty_globals {};

template<typename E, typename G = empty_globals>
class PHPExtension {
    static inline std::vector<zend_function_entry> global_functions;
    static inline zend_module_entry zme;

    template<typename T = E, typename = void>
    struct has_ini_entries : std::false_type {};
    template<typename T>
    struct has_ini_entries<T, decltype((void)T::ini_entries)> : std::true_type {};

    static void make_zme() {
        E::register_php_methods();
        global_functions.emplace_back();
        zme = {STANDARD_MODULE_HEADER_EX,
               nullptr, // ini entry
               nullptr, // deps
               E::name,
               global_functions.data(),
               prv_startup,
               E::shutdown,
               E::request_start,
               E::request_end,
               [](zend_module_entry *) { E::extension_info(); },
               E::version,
               sizeof(G),
               &_globals,
               [](void *glob) {
                   if constexpr (zts_build::value) {
                       tsrmls_cache = tsrm_get_ls_cache();
                       new (glob) G();
                   }
               },
               [](void *glob) {
                   if constexpr (zts_build::value) {
                       static_cast<G *>(glob)->~G();
                   }
               },
               E::post_request_end,
               STANDARD_MODULE_PROPERTIES_EX};
    }

    template<typename A, typename T, size_t ... Is>
    static void copy_n_ini_def(A& arr, T& tuple, std::index_sequence<Is...>) {
        arr.operator[](Is...) = std::get<Is...>(tuple).get_entry();
    }

    static int prv_startup(int type, int module_number) {
        if constexpr (has_ini_entries<>::value) {
            auto& ini_entries = E::ini_entries;
            using tuple_type = std::remove_reference_t<decltype(ini_entries)>;
            constexpr size_t size = std::tuple_size_v<tuple_type>;

            std::array<zend_ini_entry_def, size + 1> ini_defs;
            copy_n_ini_def(ini_defs, ini_entries,
                           std::make_index_sequence<size>());
            ini_defs[size] = {}; // zero the last element

            int res = zend_register_ini_entries(ini_defs.data(), module_number);
            if (res != SUCCESS) {
                return res;
            }
        }

        return E::startup(type, module_number);
    }
protected:
    using globals_type = std::conditional_t<zts_build::value, ts_rsrc_id, G>;
    static inline globals_type _globals;
#ifdef ZEND_ENABLE_STATIC_TSRMLS_CACHE
    static inline thread_local void *tsrmls_cache = nullptr;
#endif

    static int startup([[maybe_unused]] int type,
                       [[maybe_unused]] int module_number) noexcept {
        return SUCCESS;
    }

    static int shutdown([[maybe_unused]] int type,
                        [[maybe_unused]] int module_number) noexcept {
        return SUCCESS;
    }

    static int request_start([[maybe_unused]] int type,
                             [[maybe_unused]] int module_number) noexcept {
        return SUCCESS;
    }

    static int request_end([[maybe_unused]] int type,
                           [[maybe_unused]] int module_number) noexcept {
        return SUCCESS;
    }

    static int post_request_end() noexcept {
        return SUCCESS;
    }

    static void extension_info() noexcept {}

    static void register_php_methods() noexcept {}

    template<auto func, typename A = arg_names_empty_t>
    static void reg_function(const char *name) {
        using FT = cpp_func_traits<decltype(func), A>;
        auto zif_handler = wrap_non_inst_meth<FT, func>();
        const auto arginfo = php_arg_info_holder<FT>::as_ziai_array();
        zend_function_entry zfe = {
            name, zif_handler, arginfo, FT::arg_traits::max_args, 0
        };
        global_functions.push_back(zfe);
    }

public:
    static constexpr auto version = "0.1.0";
    PHPExtension() = delete;

    static G& globals() {
        if constexpr(zts_build::value) {
            void **deref_cache = *static_cast<void ***>(tsrmls_cache);
            void *v = deref_cache[TSRM_UNSHUFFLE_RSRC_ID(_globals)];
            return *static_cast<G*>(v);
        } else {
            return _globals;
        }
    }

    static zend_module_entry *descriptor() {
        make_zme();
        return &zme;
    }
};
}
