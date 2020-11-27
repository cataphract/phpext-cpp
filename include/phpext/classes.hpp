#pragma once

#include <php.h>
#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <Zend/zend_exceptions.h>
#include "conversions.hpp"

namespace zend {

struct PHPObjectStateEmpty {};
class PHPObjectState {
public:
    enum class state : uint8_t { UNBOUND, UNCONSTRUCTED, VALID, DESTRUCTED };
    static constexpr auto state_names = std::array<const char *const, 4>{
        "UNBOUND", // not associated to a PHP object
        "UNCONSTRUCTED",
        "VALID", // storage resides in zend_object
        "DESTRUCTED"
    };

    PHPObjectState() noexcept : state{state::UNBOUND}, zobj_self{nullptr} {}

    PHPObjectState(zend_object *zobj) noexcept
        : state(state::UNCONSTRUCTED), zobj_self{zobj} {}

    PHPObjectState(state state, zend_object *zobj) noexcept
        : state{state}, zobj_self{zobj} {}

    ~PHPObjectState() {
        state = state::DESTRUCTED;
        zobj_self = nullptr;
    }

    state state;
    zend_object *zobj_self; // set iif UNCONSTRUCTED/VALID
};

template<typename C /* subclass of PHPClass */>
class PHPClass : protected PHPObjectState {
public:
    template<const char * ... Names>
    using arg_names = zend::arg_names<Names...>;

    PHPClass() noexcept {}
    explicit PHPClass(zend_object *zobj) noexcept : PHPObjectState{zobj} {}
    PHPClass(const PHPClass<C> &) noexcept
        : PHPObjectState{state::UNBOUND, nullptr} {}

    PHPClass(PHPClass<C> &&) = delete;
    PHPClass& operator=(const PHPClass&) = delete;

    constexpr static auto& get_php_class_name() {
        return C::php_class_name;
    }

    static inline zend_class_entry *ce;
private:
    // after building with object_init_ex and calling a constructor
    void identify_owning_zobj(zend_object *zobj) noexcept {
        assert(state == state::UNBOUND);
        zobj_self = zobj;
        state = state::VALID;
    }

    struct zobj_t {
        C *nat_obj;
        zend_object parent;
    };

    static zend_object *ce_create_object(zend_class_entry *obj_ce) noexcept {
        zobj_t *zobj = static_cast<zobj_t *>(
                emalloc(sizeof(*zobj) + zend_object_properties_size(obj_ce)));

        zend_object_std_init(&zobj->parent, obj_ce);
        zobj->parent.handlers = &handlers;
        zobj->nat_obj = static_cast<C *>(emalloc(sizeof *zobj->nat_obj));
        new (zobj->nat_obj) PHPClass<C>(&zobj->parent);

        return &zobj->parent;
    }
    static void free_object_handler(zend_object *zobj_p) noexcept {
        zobj_t *zobj = fetch_zobj(zobj_p);
        zend_object_std_dtor(zobj_p);
        if (zobj->nat_obj->state == state::VALID) {
            zobj->nat_obj->~C();
        }
        efree(zobj->nat_obj);
    }

    static zobj_t *fetch_zobj(zend_object *zobj) noexcept {
        return reinterpret_cast<zobj_t *>(
            reinterpret_cast<uintptr_t>(
                reinterpret_cast<char *>(zobj) -
                offsetof(zobj_t, parent)));
    }
    static zobj_t *fetch_zobj(zval *zv) noexcept {
        return fetch_zobj(Z_OBJ_P(zv));
    }
    static C *fetch_nat_obj(zval *zv) noexcept {
        return fetch_zobj(zv)->nat_obj;
    }

    template<typename T>
    struct ret_void : std::false_type {};
    template<typename ... Args>
    struct ret_void<void (C::*)(Args...)> : std::true_type {};
    template<typename ... Args>
    struct ret_void<void (*)(Args...)> : std::true_type {};

    template<typename FT, typename FT::func_type func>
    static zif_handler wrap_method() {
        zif_handler wrapped = [](INTERNAL_FUNCTION_PARAMETERS) -> void {
            using arg_traits = typename FT::arg_traits;
            constexpr auto max_params = arg_traits::max_args;
            constexpr auto min_params = arg_traits::min_args;
            constexpr auto is_inst_meth = FT::is_member_func::value;
            constexpr bool is_ctor = FT::is_ctor::value;

            auto given_args = ZEND_NUM_ARGS();
            if (given_args > max_params || given_args < min_params) {
                zend_wrong_parameters_count_exception(min_params, max_params);
                return;
            }

            C *c;
            zobj_t *zobj;
            if constexpr (is_inst_meth) {
                zval *this_zv = getThis();
                if (!this_zv) {
                    zend_throw_exception_ex(
                            zend_ce_exception, 0,
                            "Instance method called statically");
                    return;
                }
                zobj = C::fetch_zobj(this_zv);
                c = zobj->nat_obj;
                constexpr enum state expected_state =
                        is_ctor ? state::UNCONSTRUCTED : state::VALID;
                if (c->state != expected_state) {
                    zend_throw_exception_ex(
                            zend_ce_exception, 0,
                            "Expected the object to have been in the state %s, "
                            "but it's in state %s",
                            state_names[static_cast<size_t>(expected_state)],
                            state_names[static_cast<size_t>(c->state)]);
                    return;
                }
            }

            std::array<zval, max_params> args_zv;
            zend_get_parameters_array_ex(static_cast<int>(given_args),
                                         args_zv.data());
            // must last until after the call
            auto opt_tuple = convert_from_zval<
                    typename arg_traits::types>(given_args, args_zv.data());
            if (!opt_tuple.has_value()) {
                return;
            }

            const auto &tuple_conv_args = opt_tuple.value();

            auto f_this = [&](auto &&... args) -> typename FT::ret_type {
                if constexpr (is_ctor) {
                    new(c) C(std::forward<decltype(args)>(args)...);
                } else if constexpr (is_inst_meth) {
                    return (*c.*func)(std::forward<decltype(args)>(args)...);
                } else {
                    return (*func)(std::forward<decltype(args)>(args)...);
                }
            };

            if constexpr (is_ctor || FT::is_void::value) {
                call_tuple(f_this, tuple_conv_args);
                if constexpr (is_ctor) {
                    c->state = state::VALID;
                    // restore. The constructor call resets it to null
                    c->zobj_self = &zobj->parent; 
                }
            } else {
                decltype(auto) res = call_tuple(f_this, tuple_conv_args);
                static_assert(
                        std::is_same_v<typename FT::ret_type, decltype(res)>);
                *return_value =
                        convert_to_zval(std::forward<decltype(res)>(res));
            }
        };
        return wrapped;
    }

    template<typename FT>
    static zif_handler wrap_constructor() {
        return wrap_method<FT, nullptr>();
    }

    static inline zend_object_handlers handlers;
    static inline std::vector<zend_function_entry> functions;
protected:
    enum class AccFlags : decltype(zend_function_entry::flags) {
        PUBLIC    = ZEND_ACC_PUBLIC,
        PROTECTED = ZEND_ACC_PROTECTED,
        PRIVATE   = ZEND_ACC_PRIVATE,
        // STATIC    = ZEND_ACC_STATIC, (dedicated method)
        FINAL     = ZEND_ACC_FINAL,
        // ABSTRACT  = ZEND_ACC_ABSTRACT, (dedicated method)
    };

    template<typename FT, typename FT::func_type func>
    static void reg_method_ex(const char *name, AccFlags flags) {
        auto wrapped_func = wrap_method<FT, func>();
        const auto arginfo = php_arg_info_holder<FT>::as_ziai_array();
        functions.push_back({
                name, wrapped_func, arginfo, FT::arg_traits::max_args,
                static_cast<decltype(zend_function_entry::flags)>(flags)});
    }

    template<typename... Args>
    struct arg_types {
        using ctor_ref_of = ctor_ref<Args...>;
    };

    template<typename AT, typename A = arg_names_empty_t>
    static void reg_constructor(AccFlags flags = AccFlags::PUBLIC) {
        using func_traits = cpp_func_traits<typename AT::ctor_ref_of, A>;
        auto wrapped_func = wrap_constructor<func_traits>();
        const auto arginfo = php_arg_info_holder<func_traits>::as_ziai_array();
        functions.push_back(
                {"__construct", wrapped_func, arginfo,
                 func_traits::arg_traits::max_args,
                 static_cast<decltype(zend_function_entry::flags)>(flags)});
    }

    template<auto func, typename A = arg_names_empty_t>
    static void reg_instance_method(const char *name,
                                    AccFlags flags = AccFlags::PUBLIC) {
        using func_traits = cpp_func_traits<decltype(func), A>;
        static_assert(func_traits::is_member_func::value);
        reg_method_ex<func_traits, func>(name, flags);
    }

    template<auto func, typename A = arg_names_empty_t>
    static void reg_static_method(const char *name,
                                  AccFlags flags = AccFlags::PUBLIC) {
        using func_traits = cpp_func_traits<decltype(func), A>;
        static_assert(!func_traits::is_member_func::value);
        flags = static_cast<AccFlags>(
                static_cast<std::underlying_type_t<AccFlags>>(flags) |
                ZEND_ACC_STATIC);
        reg_method_ex<func_traits, func>(name, flags);
    }

    static void register_php_methods() {}
public:
    static void register_class() noexcept {
        handlers = *zend_get_std_object_handlers();
        handlers.free_obj = free_object_handler;
        handlers.clone_obj = nullptr; // TODO
        handlers.offset = offsetof(zobj_t, parent);

        {
          zend_class_entry temp_ce;
          C::register_php_methods();
          functions.emplace_back();
          auto& cname = C::php_class_name;
          INIT_CLASS_ENTRY_EX(temp_ce, cname, cname.length(),
                              functions.data())
          ce = zend_register_internal_class(&temp_ce);
        }
        ce->ce_flags |= ZEND_ACC_FINAL;
        ce->clone = nullptr;
        ce->create_object = ce_create_object;
    }

    friend zval_o<C> zval_conversions::to_zval(const PHPClass<C> &);
    friend zval_o<C> zval_conversions::to_zval(PHPClass<C> &&);
    friend struct zval_conversions::from_zval_c<zval_conversions::subclasses<C&>, void>;
    friend struct php_arginfo_agg_with_args;
    friend class zval_o<C>;
};
}
