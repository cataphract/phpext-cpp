#include <phpext.hpp>
#include <phpext/output.hpp>
#include "classes.hpp"

using zend::operator""_cs;

class MyClass : public zend::PHPClass<MyClass> {
public:
    static constexpr auto php_class_name = "MyClass"_cs;

    MyClass(std::optional<long> x) : x(x.value_or(2)) {
        php_printf("no-arg ctor called\n");
    }
    static void register_php_methods() {
        // reg_constructor<arg_types<std::optional<long>>>();

        // using succ_overload1_t = long (MyClass::*)(const long, std::optional<const volatile int>);
        // // passing string literals as template arguments is not possible
        // // Possible alternative would be to pass the names as args to
        // // constexpr functions, instead of template arguments. But anyway, this
        // // is trivially solvable in C++20, so no need to stress
        // static const char number[] = "number";
        // using succ1_anames = arg_names<number>;
        // reg_instance_method<static_cast<succ_overload1_t>(&MyClass::succ),
        //                     succ1_anames>("succ");
        // using succ_overload2_t = void (MyClass::*)(int&, std::optional<std::reference_wrapper<int>>);
        // reg_instance_method<static_cast<succ_overload2_t>(&MyClass::succ)>("succ2");

        // // reg_static_method<&MyClass::add>("add");
        // reg_static_method<&MyClass::add_const_ref>("add2");
        // reg_static_method<&MyClass::print_global>("print_global");
    }

    MyClass(const MyClass& oth) : PHPClass<MyClass>{oth} {
         php_printf("copy Ctor called\n");
    }
    ~MyClass() {
        php_printf("Dtor called\n");
    }
private:
    long succ(const long l, const std::optional<const volatile int> l2) {
        return l + l2.value_or(0) + x;
    }

    int succ(int &i) {
        return i + 1;
    }

    void succ(int& j, const std::optional<std::reference_wrapper<int>> opt_i) {
        j += 2;
        if (opt_i) {
            opt_i.value()++;
        }
    }

    // static MyClass add(MyClass o1, MyClass &o2) {
    //     php_printf("in function %s\n", __FUNCTION__);
    //     return o1;
    // }

    static const MyClass &add_const_ref(MyClass &o1) {
        php_printf("in function %s o1.x=%ld\n", __FUNCTION__, o1.x);
        o1.x += 10;
        return o1;
    }

    /*
    static long add(MyClass o1, MyClass &o2, const MyClass &o3,
                    std::optional<MyClass> o4, std::optional<MyClass &> o5) {
        return o1.x + o2.x + o3.x + (o4 ? o4.value().x : 0) +
               (o5 ? o5.value().x : 0);
    }
    */

    MyClass& ret_self() {
        return *this;
    }

    static void print_global();

    long x;
};

namespace zend {


class zvalu : zval {
public:
    zvalu(const zval &zv) : zval{zv} { zval_copy_ctor(this); }
    zvalu(zval &&zv) : zval{std::move(zv)} { ZVAL_UNDEF(&zv); }
    zvalu(const zvalu &zvw) : zvalu{static_cast<const zval&>(zvw)} {}
    zvalu(zvalu &&zvw) : zvalu{static_cast<zval &&>(zvw)} {}

    template<typename T>
    void operator=(const T& val) {
        auto cv = zval_conversions::to_zval(val); // throws error_to
        zval_dtor(this);
        ZVAL_ZVAL(this, cv, 1, 1 /* dtor on cv */);
    }
    void operator=(zval&& val) {
        zval_dtor(this);
        ZVAL_COPY_VALUE(this, &val);
        ZVAL_UNDEF(&val);
    }

    ~zvalu() { zval_dtor(this); }

    ztype type() const noexcept {
        return static_cast<ztype>(Z_TYPE_P(this));
    }

    const zvalu &deref() const {
        if (type() != ztype::REFERENCE_T) {
            return *this;
        } else {
            return *static_cast<zvalu *>(Z_REFVAL_P(this));
        }
    }

    std::optional<zend_long> lvalue() const {
        const zvalu &dzv = deref();
        if (dzv.type() != ztype::LONG_T) {
            return {};
        } else {
            return Z_LVAL(dzv);
        }
    }

    zend_long lvalue_raw() const {
        assert(type() == ztype::LONG_T);
        return Z_LVAL_P(this);
    }

    std::optional<bool> bvalue() const {
        const zvalu &dzv = deref();
        if (dzv.type() == ztype::TRUE_T) {
            return true;
        } else if (dzv.type() == ztype::FALSE_T) {
            return false;
        } else {
            return {};
        }
    }

    bool bvalue_raw() const {
        assert(type() == ztype::TRUE_T || type() == ztype::FALSE_T);
        return type() == ztype::TRUE_T;
    }

    std::optional<double> dvalue() const {
        const zvalu &dzv = deref();
        if (dzv.type() != ztype::DOUBLE_T) {
            return {};
        } else {
            return Z_DVAL(dzv);
        }
    }

    double dvalue_raw() const {
        assert(type() == ztype::DOUBLE_T);
        return Z_DVAL_P(this);
    }

    std::optional<zstring_view> svalue() const {
        const zvalu &dzv = deref();
        if (dzv.type() != ztype::STRING_T) {
            return {};
        } else {
            return Z_STR(dzv);
        }
    }

    zstring_view svalue_raw() const {
        assert(type() == ztype::STRING_T);
        return Z_STR_P(this);
    }

    template<typename C>
    std::optional<std::reference_wrapper<C>> ovalue() const {
        const zvalu &dzv = deref();
        if (dzv.type() != ztype::OBJECT_T) {
            return {};
        }
        if (Z_OBJCE(dzv) != C::ce) {
            return {};
        }
        C *c = C::fetch_nat_obj(&dzv);
        return std::ref(*c);
    }

    template<typename C>
    C& ovalue_raw() const {
        assert(type() == ztype::OBJECT_T && Z_OBJCE_P(this) == C::ce);
        C *c = C::fetch_nat_obj(this);
        return *c;
    }
};
static_assert(std::is_standard_layout_v<zvalu>);

} // namespace zend


namespace global_funcs {
    static void print_ini_flag();
    static void print_global();
    static long sum_ints(int i, long j) {
        return i + j;
    }
    static long sum_ints_const(const int i, const long j) {
        return i + j;
    }
    static void add_to(long& i, long j) {
        i += j;
    }
    static void increment_opt(std::optional<std::reference_wrapper<long>> i) {
        if (!i) { return; }
        i->get()++;
    }
}

struct TestGlobals{
    TestGlobals() : str("foobar") {}
    bool ini_flag;
    std::string str;
};

class TestPHPExtension
    : public zend::PHPExtension<TestPHPExtension, TestGlobals> {
    friend class zend::PHPExtension<TestPHPExtension, TestGlobals>;
    constexpr static auto name = "testext";

    static const inline auto ini_entries = std::make_tuple(
            zend::BoolINIEntry{"sample_flag", "true", zend::INIPermission::ALL,
                               [](bool val) { globals().ini_flag = val; }});

    static void register_php_methods() {
        reg_function<&global_funcs::print_ini_flag>("print_ini_flag");
        reg_function<&global_funcs::print_global>("print_global");

        reg_function<&global_funcs::sum_ints>("sum_ints");
        reg_function<&global_funcs::sum_ints_const>("sum_ints_const");
        reg_function<&global_funcs::add_to>("add_to");
        reg_function<&global_funcs::increment_opt>("increment_opt");
    }

    static int startup(int, int) {
        MyClass::register_class();
        register_classes();
        return SUCCESS;
    }
};

namespace global_funcs {
    static void print_ini_flag() {
        zend::pout << std::boolalpha << TestPHPExtension::globals().ini_flag
                   << std::endl;
    }
    static void print_global() {
        zend::pout << TestPHPExtension::globals().str << std::endl;
    }
}


extern "C" ZEND_DLEXPORT zend_module_entry *get_module();
extern "C" ZEND_DLEXPORT zend_module_entry *get_module() {
    return TestPHPExtension::descriptor();
}
