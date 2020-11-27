#pragma once

#include <type_traits>
#include <optional>
#include <tuple>
#include <functional>
#include <php.h>
#include <utility>
#include "zmm.hpp"
#include "strings.hpp"

namespace zend {

/** TRAITS **/
template<typename T>
struct is_optional : std::false_type {
    using base_type = T;
};
template<typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using base_type = T;
};
template<typename T>
constexpr auto is_optional_v = is_optional<T>::value;

template<typename T>
struct is_ref_type : std::false_type {};
template<typename T>
struct is_ref_type<T&> : std::true_type {};
template<typename T>
struct is_ref_type<std::reference_wrapper<T>> : std::true_type {
};

template<typename T /* tuple */>
struct cpp_args_traits {
    using types = T;

    template<size_t i>
    using is_elem_optional = is_optional<std::tuple_element_t<i, T>>;
    template<size_t i>
    static constexpr bool is_elem_optional_v = is_elem_optional<i>::value;

    template<size_t i>
    using elem_type = std::tuple_element_t<i, T>;

    template<size_t i>
    using elem_base_type = typename is_optional<std::tuple_element_t<i, T>>::base_type;

    template<size_t i>
    using is_elem_ref = is_ref_type<std::remove_cv_t<elem_base_type<i>>>;
    template<size_t i>
    static constexpr bool is_elem_ref_v = is_elem_ref<i>::value;

    static constexpr auto max_args = std::tuple_size_v<T>;

    template<size_t pos = max_args>
    constexpr static size_t num_trailing_opts() {
        // pos is 1-based (to avoid underflow if the initial tuple has size 0)
        if constexpr (pos <= 0) {
            return 0;
        } else {
            if constexpr (is_elem_optional_v<pos - 1>) {
                return 1 + num_trailing_opts<pos - 1>();
            } else {
                return 0;
            }
        }
    }
    static constexpr auto optional_args = num_trailing_opts();
    static constexpr auto min_args = max_args - optional_args;
};

template<const char * ... Names>
struct arg_names {
    static constexpr auto array =
            std::array<const char *, sizeof...(Names)>{Names...};
     static constexpr auto size = sizeof...(Names);
};
using arg_names_empty_t = arg_names<>;
template<typename ArgNames = arg_names_empty_t>
struct cpp_func_arg_names {
    using arg_names = ArgNames;
};

template<typename F /* function type */, typename ArgNames = arg_names_empty_t>
struct cpp_func_traits {
    static_assert(sizeof(F) > 0, "must use specialization");
};

template<typename ArgNames, typename ... Args>
struct cpp_func_free : cpp_func_arg_names<ArgNames> {
    using is_ctor = std::false_type;
    using is_member_func = std::false_type;
    using arg_traits = cpp_args_traits<std::tuple<Args...>>;
};
template<typename ArgNames, typename R, typename... Args>
struct cpp_func_traits<R (*)(Args...), ArgNames>
    : cpp_func_free<ArgNames, Args...> {
    using func_type = R (*)(Args...);
    using is_void = std::false_type;
    using ret_type = R;
};
template<typename ArgNames, typename... Args>
struct cpp_func_traits<void (*)(Args...), ArgNames> : cpp_func_free<ArgNames, Args...> {
    using func_type = void (*)(Args...);
    using is_void = std::true_type;
    using ret_type = void;
};

template<typename ArgNames, typename ... Args>
struct cpp_func_memb : cpp_func_arg_names<ArgNames> {
    using is_ctor = std::false_type;
    using is_member_func = std::true_type;
    using arg_traits = cpp_args_traits<std::tuple<Args...>>;
};
template<typename ArgNames, typename C, typename R, typename... Args>
struct cpp_func_traits<R (C::*)(Args...), ArgNames>
    : cpp_func_memb<ArgNames, Args...> {
    using func_type = R (C::*)(Args...);
    using is_void = std::false_type;
    using ret_type = R;
};
#define GEN_MEMB_FUNC_VARIANT(qualifiers)                                      \
    template<typename ArgNames, typename C, typename R, typename... Args>      \
    struct cpp_func_traits<R (C::*)(Args...) qualifiers, ArgNames>             \
        : cpp_func_traits<R (C::*)(Args...), ArgNames> {                       \
        using func_type = R (C::*)(Args...) qualifiers;                        \
    }
// ignore volatile qualifier
GEN_MEMB_FUNC_VARIANT(const);
GEN_MEMB_FUNC_VARIANT(&);
GEN_MEMB_FUNC_VARIANT(&&);
GEN_MEMB_FUNC_VARIANT(const &);
GEN_MEMB_FUNC_VARIANT(const &&);
GEN_MEMB_FUNC_VARIANT(noexcept);
GEN_MEMB_FUNC_VARIANT(const noexcept);
GEN_MEMB_FUNC_VARIANT(& noexcept);
GEN_MEMB_FUNC_VARIANT(&& noexcept);
GEN_MEMB_FUNC_VARIANT(const & noexcept);
GEN_MEMB_FUNC_VARIANT(const && noexcept);
#undef GEN_MEMB_FUNC_VARIANT
template<typename ArgNames, typename C, typename... Args>
struct cpp_func_traits<void (C::*)(Args...), ArgNames>
    : cpp_func_memb<ArgNames, Args...> {
    using func_type = void (C::*)(Args...);
    using is_void = std::true_type;
    using ret_type = void;
};
#define GEN_MEMB_FUNC_VARIANT(qualifiers)                                      \
    template<typename ArgNames, typename C, typename... Args>                  \
    struct cpp_func_traits<void (C::*)(Args...) qualifiers, ArgNames>          \
        : cpp_func_traits<void (C::*)(Args...), ArgNames> {                    \
        using func_type = void (C::*)(Args...) qualifiers;                     \
    }
GEN_MEMB_FUNC_VARIANT(const);
GEN_MEMB_FUNC_VARIANT(&);
GEN_MEMB_FUNC_VARIANT(&&);
GEN_MEMB_FUNC_VARIANT(const &);
GEN_MEMB_FUNC_VARIANT(const &&);
GEN_MEMB_FUNC_VARIANT(noexcept);
GEN_MEMB_FUNC_VARIANT(const noexcept);
GEN_MEMB_FUNC_VARIANT(& noexcept);
GEN_MEMB_FUNC_VARIANT(&& noexcept);
GEN_MEMB_FUNC_VARIANT(const & noexcept);
GEN_MEMB_FUNC_VARIANT(const && noexcept);
#undef GEN_MEMB_FUNC_VARIANT

template<typename ... Args>
struct ctor_ref {};
template<typename ArgNames, typename... Args>
struct cpp_func_traits<ctor_ref<Args...>, ArgNames>
    : cpp_func_arg_names<ArgNames> {
    using func_type = std::nullptr_t;
    using is_ctor = std::true_type;
    using is_member_func = std::true_type;
    using arg_traits = cpp_args_traits<std::tuple<Args...>>;
    using is_void = std::true_type;
    using ret_type = void;
};

template<typename T, std::nullptr_t = nullptr>
struct remove_ref_wrapper {
    using type = T;
};
template<typename T>
struct remove_ref_wrapper<std::reference_wrapper<T>> {
    using type = T;
};
template<typename T>
using remove_ref_wrapper_t = typename remove_ref_wrapper<T>::type;

template<typename C /* subclass of PHPClass */>
class PHPClass;

enum class ztype : zend_type {
    UNDEF_T = IS_UNDEF,
    NULL_T = IS_NULL,
    FALSE_T = IS_FALSE,
    TRUE_T = IS_TRUE,
    LONG_T = IS_LONG,
    DOUBLE_T = IS_DOUBLE,
    STRING_T = IS_STRING,
    ARRAY_T = IS_ARRAY,
    OBJECT_T = IS_OBJECT,
    RESOURCE_T = IS_RESOURCE,
    REFERENCE_T = IS_REFERENCE
};

template<ztype _type>
class zval_typed : public zval {
public:
    struct uninitialized_t {};
    constexpr static auto uninit = uninitialized_t{};
    explicit zval_typed(uninitialized_t) {
        ZVAL_UNDEF(this);
    }
    explicit zval_typed(const zval &zv) : zval{zv} {
        assert(Z_TYPE(zv) == static_cast<int>(_type));
    }
    explicit zval_typed(zval &&zv) : zval{std::move(zv)} {
        assert(Z_TYPE(zv) == static_cast<int>(_type));
        ZVAL_UNDEF(&zv);
    }
    explicit zval_typed(const zval_typed<_type> &tzv)
        : zval_typed{static_cast<const zval &>(tzv)} {}
    explicit zval_typed(zval_typed<_type> &&zvt)
        : zval_typed{static_cast<zval &&>(zvt)} {}

    // we don't implicit manage calls to dtor
    // because the purpose of these objects is to be passed
    // further along to the engine as plain zvals.
    // To keep things consistent, we don't manage calls to
    // add_ref/copy_ctor neither
    void add_ref() noexcept {
        Z_TRY_ADDREF_P(this);
    }
    // copy_ctor only matters for arrays, so add it there

    void zv_dtor() noexcept {
        zval_ptr_dtor_nogc(this);
    }
//    operator zvalu&() { return *static_cast<zvalu*>(this); }

    static constexpr ztype type() noexcept {
        return _type;
    }

protected:
    zval_typed() {}
};

class zval_s : public zval_typed<ztype::STRING_T> {
public:
    zval_s(uninitialized_t) : zval_typed<ztype::STRING_T>{uninit} {}
    zval_s(zend_string *zs) {
        ZVAL_STR(this, zs);
    }

    void operator=(zend_string *zs) {
        zval_dtor(this);
        ZVAL_STR(this, zs);
    }
    zstring_view val() const {
        return Z_STR_P(this);
    }
protected:
    zval_s() {}
};

class zval_l : public zval_typed<ztype::LONG_T> {
public:
    zval_l(uninitialized_t) : zval_typed<ztype::LONG_T>{uninit} {}
    zval_l(zend_long l) {
        ZVAL_LONG(this, l)
    }
    zend_long val() {
        return Z_LVAL_P(this);
    }
    void operator=(zend_long l) {
        Z_LVAL_P(this) = l;
    }
protected:
    zval_l() {}
};
static_assert(std::is_standard_layout_v<zval_l>);

template<typename C>
class zval_o : public zval_typed<ztype::OBJECT_T> {
public:
    using nat_class_t = C;

    zval_o(uninitialized_t) : zval_typed<ztype::OBJECT_T>{uninit} {}
    zval_o(typename C::zobj_t *zobj) {
        ZVAL_OBJ(this, &zobj->parent);
    }
    static zval_o create_unconstructed() {
        zval_o zv{};
        object_init_ex(&zv, C::ce);
        return zv;
    }

    C& val() const {
        return *C::fetch_nat_obj(this);
    }
protected:
    zval_o() {}
};
template<typename Z>
zval_o(Z *zobj) -> zval_o<std::decay_t<decltype(*zobj->nat_obj)>>;

/**** TO zval ****/
namespace zval_conversions {

    struct error_to {
        zmm::string message;
    };

    static void handle_error(const error_to &err) noexcept {
        if (EG(exception)) { // TODO chain exception?
            return;
        }
        zend_internal_type_error(1, "error converting value to zval: %s",
                                 err.message.c_str());
    }


    static auto to_zval(int i) {
        zval_l zv{static_cast<zend_long>(i)};
        return zv;
    }
    static auto to_zval(long i) {
        zval_l zv{static_cast<zend_long>(i)};
        return zv;
    }

    template<typename C>
    static zval_o<C> to_zval(const PHPClass<C> &cc) {
        if (cc.state == C::state::UNCONSTRUCTED ||
            cc.state == C::state::DESTRUCTED) {
                zmm::string msg;
                msg += "Cannot wrap object of type ";
                msg += ZSTR_VAL(C::ce->name);
                msg += " in an unconstructed or destroyed state";
                throw error_to{msg};
        }
        if (cc.zobj_self) {
            zend_object *zobj = cc.zobj_self;
            auto zv = zval_o{C::fetch_zobj(zobj)};
            zv.add_ref();
            return zv;
        } else {
            if (cc.state != C::state::UNBOUND) {
                throw error_to{"object not included in PHP variable is not in "
                               "the UNBOUND state"};
            } else {
                if constexpr(std::is_copy_constructible_v<C>) {
                    auto zv = zval_o<C>::create_unconstructed();
                    using zobj_t = typename C::zobj_t;
                    zobj_t *zobj_sub = C::fetch_zobj(&zv);
                    C *c = zobj_sub->nat_obj;
                    new(c) C(static_cast<const C&>(cc)); // call copy ctor
                    c->identify_owning_zobj(&zobj_sub->parent);
                    // TODO: option to do move instead of copy
                    return zv;
                } else {
                    zmm::string m =
                            "Cannot build PHP variable with native object "
                            "built outside of PHP because its class ";
                    m += C::get_php_class_name();
                    m += " is not copy constructible";
                    throw error_to{std::move(m)};
                }
            }
        }
    }

    template<typename C>
    static zval_o<C> to_zval(PHPClass<C> &&cc) {
        if constexpr (std::is_move_constructible_v<C>) {
            if (cc.state == C::state::UNCONSTRUCTED ||
                cc.state == C::state::DESTRUCTED) {
                zmm::string msg;
                msg += "Cannot wrap object of type ";
                msg += ZSTR_VAL(C::ce->name);
                msg += " in an unconstructed or destroyed state";
                throw error_to{msg};
            }

            auto zv = zval_o<C>::create_unconstructed();
            using zobj_t = typename C::zobj_t;
            zobj_t *zobj_sub = C::fetch_zobj(&zv);
            C *c = zobj_sub->nat_obj;
            new (c) C(static_cast<C&&>(cc)); // call move ctor
            c->identify_owning_zobj(&zobj_sub->parent);
            // TODO: option to do move instead of copy
            return zv;
        } else {
            if constexpr (std::is_copy_constructible_v<C>) {
                return to_zval(static_cast<PHPClass<C> &>(cc));
            } else {
                zmm::string m = "Cannot build PHP variable with native object "
                                "built outside of PHP because its class ";
                m += C::get_php_class_name();
                m += " is not copy or move constructible";
                throw error_to{std::move(m)};
            }
        }
    }

    template<typename C>
    static zval_o<C> to_zval(const std::reference_wrapper<C> &rw) {
        return to_zval(rw.get());
    }
}

template<typename T>
static auto convert_to_zval(T &&i) noexcept {
    using R = decltype(zval_conversions::to_zval(std::forward<T>(i)));
    static_assert(std::is_base_of_v<zval, R>, "no valid conversion");
    try {
        return zval_conversions::to_zval(std::forward<T>(i));
    } catch (const zval_conversions::error_to& err) {
        zval_conversions::handle_error(err);
        return R{R::uninit};
    }
}

/**** FROM zval ****/
// custom errors
#define ZPP_ERROR_OVERFLOW (-1)
#define ZPP_ERROR_NO_REFERENCE (-2)
#define ZPP_ERROR_INVALID_OBJ (-3)
namespace zval_conversions {
    struct error_from_no_ctx {
        int error_code = ZPP_ERROR_OK;
        zend_expected_type expected_type;
        const char *name;
    };
    struct error_from : public error_from_no_ctx {
        size_t arg_num;
        zval *zv;
    };

    static void handle_error(const error_from &err) noexcept {
        switch (err.error_code) {
        case ZPP_ERROR_WRONG_ARG:
            zend_wrong_parameter_type_exception(static_cast<int>(err.arg_num),
                                                err.expected_type,
                                                err.zv);
            break;
        case ZPP_ERROR_OVERFLOW: {
            static const char *const expected_error[] = {
                    Z_EXPECTED_TYPES(Z_EXPECTED_TYPE_STR) nullptr};
            if (EG(exception)) { // TODO chain exception?
                break;
            }
            const char *space, *class_name;
            class_name = get_active_class_name(&space);
            zend_internal_type_error(
                    1,
                    "%s%s%s() has for parameter %zu a %s, but the value is not "
                    "within the accepted bounds",
                    class_name, space, get_active_function_name(), err.arg_num,
                    expected_error[err.expected_type]);
            break;
        }
        case ZPP_ERROR_NO_REFERENCE: {
            const char *space, *class_name;
            class_name = get_active_class_name(&space);
            zend_internal_type_error(
                    1, "%s%s%s() does not have a reference for parameter %zu",
                    class_name, space, get_active_function_name(), err.arg_num);
            break;
        }
        case ZPP_ERROR_WRONG_CLASS: {
            zend_wrong_parameter_class_exception(
                        static_cast<int>(err.arg_num),
                        const_cast<char *>(err.name) /* defect in API */,
                        err.zv);
            break;
        }
        case ZPP_ERROR_INVALID_OBJ: {
            const char *space, *class_name;
            class_name = get_active_class_name(&space);
            zend_internal_type_error(1,
                                     "%s%s%s() was passed an object of type %s "
                                     "in an invalid state for "
                                     "parameter %zu (probably unconstructed)",
                                     class_name, space,
                                     get_active_function_name(), err.name,
                                     err.arg_num);
            break;
        }
        }
    }

    template<typename T /* type of bound arg */, typename = void>
    struct from_zval_c; // do not define so we have errors for incomplete types
                        // when there's no specialization

    template<typename T>
    struct subclasses {};

    /* this is used in the entry from_zval function. It has a circularity
     * problem when the conversion functions themselves use the from_zval
     * as then no auto return type deduction can be made. For those,
     * specializations can be added */
    template<typename T>
    struct has_conversion {
        template<typename U, typename = decltype(from_zval_c<U>::from_zval(
                                     std::declval<zval&>()))>
        static std::true_type test(long);
        template<typename U>
        static std::false_type test(int);

        using type = decltype(test<std::remove_cv_t<T>>(0L));
    };
    template<typename U>
    struct has_conversion<std::optional<U>> {
        using type = typename has_conversion<U>::type;
    };

    template<typename T /* type of bound arg */>
    static auto from_zval(zval &zv) { // not const because of the arg parse API
        using T_ = std::remove_cv_t<T>;
        using T_nonref = remove_ref_wrapper_t<std::decay_t<T_>>;

        /* type of value passed to from_zval */
        using C = std::conditional_t<
                std::conjunction_v<
                        std::is_class<T_nonref>,
                        typename has_conversion<subclasses<T_nonref&>>::type>,
                subclasses<T_nonref&>, T_>;

        static_assert(has_conversion<C>::type::value, "no conversion avail");
        using R = decltype(from_zval_c<C>::from_zval(zv));
        // if R != T, at least T must be passable from R
        // static_assert(sizeof(R)==0);
        auto test_lambda = [](T) { return 1; };
        static_assert(sizeof(decltype(test_lambda(std::declval<R>()))) > 0,
                      "result of conversion from zval cannot be used to build "
                      "the target native function parameter type");

        /* start hacky workaround for stdlibc++ */
        using T_is_opt = is_optional<T>;
        using T_base_type = typename T_is_opt::base_type;
        using R_is_opt = is_optional<R>;
        using R_base_type = typename R_is_opt::base_type;
        // stdlibc++ has a bug building std::optional<volatile X>
        // from std::optional<X>
        if constexpr (T_is_opt::value && R_is_opt::value &&
                      std::is_volatile_v<T_base_type> &&
                      !std::is_volatile_v<R_base_type> &&
                      std::is_same_v<std::remove_cv_t<T_base_type>,
                                     std::remove_cv_t<R_base_type>>) {
            auto res = from_zval_c<T_>::from_zval(zv);
            if (res) {
                T_base_type &value = res.value();
                return std::optional<T_base_type>{std::move(value)};
            } else {
                return std::optional<T_base_type>{};
            }
        /* end hacky workaround */
        } else {
            return from_zval_c<C>::from_zval(zv);
        }
    }


    // integer values
    template<typename O, typename F, zend_expected_type expected>
    static F enforce_bounds(O orig) {
        if (orig > static_cast<O>(std::numeric_limits<F>::max())) {
            throw error_from_no_ctx{ZPP_ERROR_OVERFLOW, expected, nullptr};
        }
        if (orig < static_cast<O>(std::numeric_limits<F>::min())) {
            throw error_from_no_ctx{ZPP_ERROR_OVERFLOW, expected, nullptr};
        }
        return static_cast<F>(orig);
    }

    template<typename I>
    static I from_zval_to_int(zval &zv) {
        zend_long res;
        zend_bool is_null;
        bool success =
                zend_parse_arg_long(&zv, &res, &is_null, 1 /* check null */, 0);
        if (!success || is_null) {
            throw error_from_no_ctx{ZPP_ERROR_WRONG_ARG, Z_EXPECTED_LONG,
                                    nullptr};
        }
        if constexpr (std::is_same_v<I, zend_long>) {
            return res;
        } else {
            return enforce_bounds<zend_long, I, Z_EXPECTED_LONG>(res);
        }
    }

    template<>
    struct from_zval_c<long> {
        static long from_zval(zval &zv) {
            return from_zval_to_int<long>(zv);
        }
    };

    template<>
    struct from_zval_c<int> {
        static int from_zval(zval &zv) {
            return from_zval_to_int<int>(zv);
        }
    };

    // references
    template<typename T>
    struct from_zval_c<std::optional<T>> {
        static auto from_zval(zval &zv) {
            // R may not be std::optional<T>, as from_zval<T> may not return T
            using R = decltype(zval_conversions::from_zval<T>(zv));
            if (Z_TYPE(zv) == IS_NULL) {
                return std::optional<R>{};
            }
            auto t_conv = zval_conversions::from_zval<T>(zv);
            return std::make_optional<R>(std::move(t_conv));
        }
    };

    template<typename T>
    struct ref_arg {
        static T default_value() { return T(); }
        zval *zv_deref;
        mutable T nat_value;
        ref_arg(zval *zv_deref) : zv_deref{zv_deref}, nat_value{default_value()} { }
        ref_arg(zval *zv_deref, T &&initial_value)
            : zv_deref{zv_deref}, nat_value{initial_value} {}

        operator T&() const {
            return nat_value;
        }
        operator std::reference_wrapper<T>() const {
            return std::ref(nat_value);
        }

        ~ref_arg() {
            if (!zv_deref) {
                return;
            }
            zval_dtor(zv_deref);
            *zv_deref = convert_to_zval(std::move(nat_value));
        }

        /* no copy */
        ref_arg(const ref_arg &) = delete;
        ref_arg &operator=(const ref_arg &) = delete;
        /* move constructor (T will need to be moveable or at least copyable) */
        // ref_arg(ref_arg &&) = default;
        ref_arg(ref_arg &&orig)
            : ref_arg{orig.zv_deref, std::move(orig.nat_value)} {
            orig.zv_deref = nullptr;
        }
        ref_arg &operator=(ref_arg &&) = delete;
    };
    template<typename>
    struct is_ref_arg : std::false_type {};
    template<typename T>
    struct is_ref_arg<ref_arg<T>> : std::true_type {};
    template<typename T>
    struct is_ref_arg<std::optional<ref_arg<T>>> : std::true_type {};

    template<typename T>
    struct from_zval_c<T&> {
        static ref_arg<T> from_zval(zval &zv) {
            // TODO: assertion probably should be limited to PHPClasses
            static_assert(!std::is_class_v<T>, "unexpected use with classes");
            if (Z_TYPE(zv) != IS_REFERENCE) {
                throw error_from_no_ctx{ZPP_ERROR_NO_REFERENCE};
            }
            zval *zv_deref = &zv;
            ZVAL_DEREF(zv_deref);
            if (Z_TYPE_P(zv_deref) == IS_NULL ||
                Z_TYPE_P(zv_deref) == IS_UNDEF) {
                return {zv_deref};
            } else {
                return {zv_deref, zval_conversions::from_zval<T>(*zv_deref)};
            }

            return {zv_deref};
        }
    };
    template<typename T>
    struct from_zval_c<std::reference_wrapper<T>> {
        static ref_arg<T> from_zval(zval &zv) {
            return from_zval_c<T&>::from_zval(zv);
        }
    };

    // classes
    // return as pointer to avoid the object being copied (or moved) as it's
    // passed around. The call to operator T& happens at the last possible
    // moment, so have at most one copy happening
    template<typename T>
    struct cvt_ptr {
        cvt_ptr(T *ptr) : ptr(ptr) {}
        mutable T *ptr;
        operator T&() const { return *ptr; }
        operator std::reference_wrapper<T>() const { return std::ref(*ptr); }
    };
    template<typename T>
    cvt_ptr(T *ptr) -> cvt_ptr<T>;

    template<typename C>
    struct from_zval_c<subclasses<C&>,
                       std::enable_if_t<std::is_base_of_v<PHPClass<C>, C>>> {
        static auto from_zval(zval &zv) {
            zval *zv_deref = &zv;
            ZVAL_DEREF(zv_deref);

            zval *o;
            zend_class_entry *ce = C::ce;
            bool success = zend_parse_arg_object(zv_deref, &o, ce,
                                                 1 /* check null */);
            if (!success) {
                throw error_from_no_ctx{ZPP_ERROR_WRONG_CLASS,
                                        Z_EXPECTED_OBJECT, ZSTR_VAL(ce->name)};
            }

            C *c = C::fetch_nat_obj(&zv);
            if (c->state != C::state::VALID) {
                throw error_from_no_ctx{ZPP_ERROR_INVALID_OBJ,
                                        Z_EXPECTED_OBJECT, ZSTR_VAL(ce->name)};
            }
            return cvt_ptr{c};
        }
    };
    template<typename C>
    struct from_zval_c<subclasses<const C&>,
                       std::enable_if_t<std::is_base_of_v<PHPClass<C>, C>>> {
        static auto from_zval(zval &zv) {
            return from_zval_c<subclasses<C&>>::from_zval(zv);
        }
    };

    // from outer functions
    template<typename R>
    static auto from_zval_entry(size_t idx, zval& zv) {
        zval *zvp = &zv;
        using is_ref = typename cpp_args_traits<
                std::tuple<R>>::template is_elem_ref<0>;
        if constexpr (!is_ref::value) {
            if (Z_TYPE_P(zvp) == IS_REFERENCE) {
                ZVAL_DEREF(zvp);
            }
        }

        // calls from_zval and completes the exception with context
        try {
            return from_zval<R>(*zvp);
        } catch (const error_from_no_ctx &err) {
            throw error_from{{err.error_code, err.expected_type, nullptr}, idx,
                             &zv};
        }
    }

    template <typename Ps, size_t... Is>
    static auto convert(size_t num_args, zval *args, std::index_sequence<Is...>) {
        static zval null_zv = []() {
            zval zv;
            ZVAL_NULL(&zv);
            return zv;
        }();
        auto zval_or_null_zval = [&](size_t i) -> zval& {
            // if requested an optional argument not given, provide a null zv
            if (i >= num_args) {
                return null_zv;
            }
            return args[i];
        };
        auto do_conv = [&]() {
            return std::make_tuple(
                    from_zval_entry<std::tuple_element_t<Is, Ps>>(
                        Is, zval_or_null_zval(Is))...);
        };
        try {
            return std::optional{do_conv()};
        } catch (const error_from &r) {
            handle_error(r);
            return std::optional<decltype(do_conv())>{};
        }
    }

} // namespace zval_conversions

// TODO: this is for params. We prob need different conversions
// in other circumstances
template<typename Ps /* tuple */>
static auto convert_from_zval(size_t num_args, zval *args) noexcept {
    return zval_conversions::convert<Ps>(num_args, args,
                       std::make_index_sequence<std::tuple_size<Ps>::value>{});
}


/** arginfo **/
template<typename FT /* function traits */>
class php_arginfo {
    using arg_traits = typename FT::arg_traits;

    php_arginfo() = delete;

    struct php_arg_info_base {
        zend_internal_function_info gen_info;

        constexpr php_arg_info_base() noexcept
            : gen_info{arg_traits::min_args, 0, 0 /* TODO: ret by ref */, 0} {}
    };

    // we need to generalize zend_internal_arg_info with a union because
    // a cast of char * into zend_type (aka uintptr_r) is not allowed in
    // constexpr code
    struct zend_internal_arg_info_gen {
        const char *name;
        union type_union {
            zend_type ztype;
            const char *class_name;
            constexpr type_union(zend_type ztype) : ztype(ztype) {}
            constexpr type_union(const char *cname) : class_name(cname) {}
        } type;
        zend_uchar pass_by_reference;
        zend_bool is_variadic;
    };

    struct php_arginfo_agg_no_args : php_arg_info_base {
        static constexpr bool no_args = true;

        zend_internal_arg_info terminator;
        constexpr php_arginfo_agg_no_args() : terminator{} {}
    };

    struct php_arginfo_agg_with_args : php_arg_info_base {
        static constexpr bool no_args = false;

        std::array<zend_internal_arg_info_gen, arg_traits::max_args> arg_info;
        zend_internal_arg_info terminator;

        constexpr php_arginfo_agg_with_args() noexcept
            : php_arginfo_agg_with_args(
                      std::make_index_sequence<arg_traits::max_args>{}) {}

        /* create static with auto arg names */
        static constexpr auto auto_arg_name_size = sizeof("arg1");
        template<size_t i>
        static constexpr auto auto_arg_for_i() {
            return std::array<char, auto_arg_name_size>{
                'a', 'r', 'g', '1' + i, 0
            };
        }
        template<size_t... Is>
        static constexpr auto create_auto_args(std::index_sequence<Is...>) {
            return std::array<decltype(auto_arg_for_i<0>()),
                              arg_traits::max_args>{auto_arg_for_i<Is>()...};
        }
        static constexpr auto auto_param_names = create_auto_args(
                std::make_index_sequence<arg_traits::max_args>{});
        static constexpr auto expl_param_names = FT::arg_names::array;

        /* create optional hint for classes: ?<class name> */
        template<char ... C>
        struct hint_for_opt_cls_holder {
            constexpr static const char value[] { '?', C..., 0 };
        };
        template<char ... Cs>
        static constexpr auto
        create_hint_for_opt_cls(ct_string<char, Cs...>) {
            return hint_for_opt_cls_holder<Cs...>::value;
        }

        template<size_t i>
        static constexpr auto to_zend_internal_arg_info() {
            using arg_type = typename arg_traits::template elem_base_type<i>;
            constexpr auto num_prov_arg_names = FT::arg_names::size;
            constexpr auto is_opt = arg_traits::template is_elem_optional_v<i>;
            using conv_type = decltype(convert_to_zval(
                    std::declval<arg_type>()));
            // TODO: refs for classes
            constexpr auto is_ref = arg_traits::template is_elem_ref_v<i> &&
                          conv_type::type() != ztype::OBJECT_T;
            constexpr const char *arg_name = [&]() {
                if constexpr (i < num_prov_arg_names) {
                    return expl_param_names[i];
                } else {
                    return auto_param_names[i].data();
                }
            }();

            if constexpr (conv_type::type() == ztype::OBJECT_T) {
                constexpr auto cname = // decltype to avoid decay
                        conv_type::nat_class_t::get_php_class_name();
                constexpr const char *hint = [&]() -> const char * {
                    if constexpr (is_opt) {
                        return create_hint_for_opt_cls(cname);
                    } else {
                        return cname;
                    }
                }();

                constexpr auto r = zend_internal_arg_info_gen{
                        arg_name,
                        hint,
                        0,     // TODO: refs for classes
                        0      // TODO: variadic
                };
                return r;
            } else {
                constexpr auto r = zend_internal_arg_info_gen{
                        arg_name,
                        ZEND_TYPE_ENCODE(
                                static_cast<zend_type>(conv_type::type()),
                                is_opt),
                        is_ref,
                        0, // TODO: variadic
                };
                return r;
            }
        }

        template<size_t... Is>
        constexpr php_arginfo_agg_with_args(std::index_sequence<Is...>) noexcept
            : arg_info{to_zend_internal_arg_info<Is>()...}, terminator{} {}
    };

public:
    // TODO: clunky. Can this be done without outside struct and/or enable_if?
    using type = std::conditional_t<arg_traits::max_args == 0,
                                    php_arginfo_agg_no_args,
                                    php_arginfo_agg_with_args>;
};

static_assert((php_arginfo<cpp_func_traits<void (*)(std::optional<int>)>>::type{})
                              .arg_info[0]
                              .type.ztype == ZEND_TYPE_ENCODE(IS_LONG, 1),
              "php_arginfo should be statically instantiable");
static_assert((php_arginfo<cpp_func_traits<void (*)()>>::type{}).gen_info.required_num_args == 0,
              "php_arginfo should be statically instantiable (0 args)");
static_assert(!php_arginfo<cpp_func_traits<void (*)(int)>>::type::no_args,
              "Selects args variant");
static_assert(php_arginfo<cpp_func_traits<void (*)()>>::type::no_args,
              "Selects no args variant");

template<typename FT>
class php_arg_info_holder {
    static constexpr auto value = typename php_arginfo<FT>::type{};

    static_assert(sizeof(value) == (FT::arg_traits::max_args + 2) *
                                           sizeof(zend_internal_arg_info));

public:
    static auto *as_ziai_array() {
        return reinterpret_cast<const zend_internal_arg_info*>(&value);
    }
};

/* C++17: replaceable with std::apply? */
template<typename F, typename T, size_t... Is>
static decltype(auto) call_tuple(F f, const T &tuple,
                                 std::index_sequence<Is...>) {
    return f(std::get<Is>(tuple)...);
}

template<typename F, typename T>
static decltype(auto) call_tuple(F f, const T &t) {
    constexpr auto size = std::tuple_size_v<T>;
    return call_tuple(f, t, std::make_index_sequence<size>{});
}

template<typename FT, typename FT::func_type func>
static inline zif_handler wrap_free_function() {
    zif_handler wrapped = [](INTERNAL_FUNCTION_PARAMETERS) -> void {
        using arg_traits = typename FT::arg_traits;
        constexpr auto max_params = arg_traits::max_args;
        constexpr auto min_params = arg_traits::min_args;

        auto given_args = ZEND_NUM_ARGS();
        if (given_args > max_params || given_args < min_params) {
            zend_wrong_parameters_count_exception(min_params, max_params);
            return;
        }

        std::array<zval, max_params> args_zv;
        zend_get_parameters_array_ex(given_args, args_zv.data());
        // must last until after the call
        auto opt_tuple = convert_from_zval<
                typename arg_traits::types>(given_args, args_zv.data());
        if (!opt_tuple.has_value()) {
            return;
        }

        const auto &tuple_conv_args = opt_tuple.value();
        if constexpr (FT::is_void::value) {
            call_tuple(func, tuple_conv_args);
        } else {
            auto res = call_tuple(func, tuple_conv_args);
            *return_value = convert_to_zval(res);
        }
    };
    return wrapped;
}
}
