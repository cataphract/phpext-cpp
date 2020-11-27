#include "classes.hpp"
#include "phpext/output.hpp"
#include "phpext/strings.hpp"

using zend::operator""_cs;

class ClassNoMoveNoCopy : public zend::PHPClass<ClassNoMoveNoCopy> {
public:
    using self = ClassNoMoveNoCopy;

    constexpr static auto php_class_name = "ClassNoMoveNoCopy"_cs;

    ClassNoMoveNoCopy(long i) : i{i} {
        zend::pout << "ClassNoMoveNoCopy constructor with i=" << i
                   << std::endl;
    }
    ClassNoMoveNoCopy(const ClassNoMoveNoCopy&) = delete;
    ClassNoMoveNoCopy(ClassNoMoveNoCopy&&) = delete;
    ~ClassNoMoveNoCopy() {
        zend::pout << "ClassNoMoveNoCopy destructor" << std::endl;
    }

    static void register_php_methods() {
        reg_constructor<arg_types<long>>();

        reg_instance_method<&self::ival>("ival");
        reg_instance_method<&self::addToThis>("addToThis");
        reg_instance_method<&self::newAdding, arg_names<"i"_cs>>("newAdding");
        reg_static_method<&self::refToStatic>("refToStatic");
        reg_static_method<&self::addTo>("addTo");
        reg_static_method<&self::addToOptional>("addToOptional");
    }

private:
    long ival() {
        return i;
    }
    ClassNoMoveNoCopy newAdding(long j) {
        // should fail: we need to move or copy to zval storage
        return {i + j};
    }

    ClassNoMoveNoCopy& addToThis(long j) {
        i += j;
        return *this;
    }
    static ClassNoMoveNoCopy& refToStatic() {
        // must fail because we're returning a reference to an unbound object
        // and it's not possible to copy it
        static ClassNoMoveNoCopy o{4};
        return o;
    }

    static void addTo(ClassNoMoveNoCopy& o, long j) {
        o.i += j;
    }

    static void
    addToOptional(std::optional<std::reference_wrapper<ClassNoMoveNoCopy>> o,
                  long j) {
        if (o) {
            o->get().i += j;
        }
    }

    long i;
};

class ClassMoveNoCopy : public zend::PHPClass<ClassMoveNoCopy> {
public:
    constexpr static auto php_class_name = "ClassMoveNoCopy"_cs;

    ClassMoveNoCopy(long i) : i{i} {
        zend::pout << "ClassMoveNoCopy constructor with i=" << i
                   << std::endl;
    }
    ClassMoveNoCopy(const ClassNoMoveNoCopy&) = delete;
    ClassMoveNoCopy(ClassMoveNoCopy&& other) : zend::PHPClass<ClassMoveNoCopy>{} {
        zend::pout << "ClassMoveNoCopy move constructor" << std::endl;
        assert(other.state == state::VALID || other.state == state::UNBOUND);
        other.state = state::DESTRUCTED;
        i = other.i;
    }
    ~ClassMoveNoCopy() {
        zend::pout << "ClassMoveNoCopy destructor" << std::endl;
    }

    static void register_php_methods() {
        reg_constructor<arg_types<long>, arg_names<"i"_cs>>();

        reg_instance_method<&ClassMoveNoCopy::newAdding>("newAdding");
        reg_instance_method<&ClassMoveNoCopy::ival>("ival");
    }

private:
    long ival() {
        return i;
    }
    ClassMoveNoCopy newAdding(long j) {
        return {i + j};
    }

    long i;
};

class ClassNoMoveCopy : public zend::PHPClass<ClassNoMoveCopy> {
public:
    constexpr static auto php_class_name = "ClassNoMoveCopy"_cs;

    ClassNoMoveCopy(long i) : i{i} {
        zend::pout << "ClassNoMoveCopy constructor with i=" << i
                   << std::endl;
    }
    ClassNoMoveCopy(const ClassNoMoveCopy& other) : i{other.i} {
        zend::pout << "ClassNoMoveCopy copy constructor" << std::endl;
    }
    ClassNoMoveCopy(ClassNoMoveCopy&& other) = delete;
    ~ClassNoMoveCopy() {
        zend::pout << "ClassNoMoveCopy destructor" << std::endl;
    }

    static void register_php_methods() {
        reg_constructor<arg_types<long>>();

        reg_instance_method<&ClassNoMoveCopy::newAdding>("newAdding");
        reg_instance_method<&ClassNoMoveCopy::newAddingRef>("newAddingRef");
        reg_instance_method<&ClassNoMoveCopy::ival>("ival");
    }
private:
    long i;

    long ival() {
        return i;
    }

    ClassNoMoveCopy newAdding(const ClassNoMoveCopy other) const noexcept {
        return {i + other.i};
    }
    ClassNoMoveCopy newAddingRef(const ClassNoMoveCopy& other) const noexcept {
        return {i + other.i};
    }
};

void register_classes() {
    ClassNoMoveNoCopy::register_class();
    ClassMoveNoCopy::register_class();
    ClassNoMoveCopy::register_class();
}
