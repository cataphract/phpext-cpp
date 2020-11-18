#include <functional>
#include <phpext.hpp>
#include <phpext/output.hpp>

class MyClass : public zend::PHPClass<MyClass> {
public:
    MyClass(std::optional<long> x) : x(x.value_or(2)) {
        php_printf("no-arg ctor called\n");
    }
    static void register_php_methods() {
        reg_constructor<arg_types<std::optional<long>>>();

        using succ_overload1_t = long (MyClass::*)(const long, std::optional<const volatile int>);
        // passing string literals as template arguments is not possible
        // Possible alternative would be to pass the names as args to
        // constexpr functions, instead of template arguments. But anyway, this
        // is trivially solvable in C++20, so no need to stress
        static const char number[] = "number";
        using succ1_anames = arg_names<number>;
        reg_instance_method<static_cast<succ_overload1_t>(&MyClass::succ),
                            succ1_anames>("succ");
        using succ_overload2_t = void (MyClass::*)(int&, std::optional<std::reference_wrapper<int>>);
        reg_instance_method<static_cast<succ_overload2_t>(&MyClass::succ)>("succ2");

        // reg_static_method<&MyClass::add>("add");
        reg_static_method<&MyClass::add_const_ref>("add2");
        reg_static_method<&MyClass::print_global>("print_global");
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
