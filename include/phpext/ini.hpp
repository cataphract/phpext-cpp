#pragma once
#include <php.h>
#include "strings.hpp"

namespace zend {

enum class INIStage : int {
    STARTUP = PHP_INI_STAGE_STARTUP,
    SHUTDOWN = PHP_INI_STAGE_SHUTDOWN,
    ACTIVATE = PHP_INI_STAGE_ACTIVATE,
    DEACTIVATE = PHP_INI_STAGE_DEACTIVATE,
    RUNTIME = PHP_INI_STAGE_RUNTIME,
    HTACCESS = PHP_INI_STAGE_HTACCESS,
};
enum class INIPermission : int {
    USER = ZEND_INI_USER,
    PERDIR = ZEND_INI_PERDIR,
    SYSTEM = ZEND_INI_SYSTEM,

    NOT_MODIFIABLE = 0,
    NOT_USER = ~ZEND_INI_USER,
    ALL = ZEND_INI_ALL,
};

template<typename T>
class INIEntry {
public:
    const char *const name;
    const char *const default_value;
    const INIPermission permission;

    zend_ini_entry_def get_entry() const {
        return zend_ini_entry_def{
                name,
                INIEntry::on_modify_handler,
                const_cast<INIEntry<T> *>(this),
                nullptr,
                nullptr,
                default_value,
                T::display_handler,
                static_cast<uint32_t>(strlen(default_value)),
                static_cast<uint16_t>(strlen(name)),
                permission != INIPermission::NOT_MODIFIABLE,
        };
    }

protected:
    static int on_modify_handler([[maybe_unused]] zend_ini_entry *entry,
                                 zend_string *new_value,
                                 void *mh_arg1, void *, void *,
                                 int stage) {
        T *ih = static_cast<T *>(mh_arg1);
        bool res = ih->on_modify(zend::zstring_view(new_value),
                                 static_cast<INIStage>(stage));
        return res ? SUCCESS : FAILURE;
    }
    bool on_modify(zend::zstring_view, INIStage) {
        return true;
    }

    static void display_handler(zend_ini_entry *ini_entry, int type) {
        T *ih = static_cast<T *>(ini_entry->mh_arg1);
        ih->display(ini_entry, type);
    }
    void display([[maybe_unused]] zend_ini_entry *ini_entry,
                 [[maybe_unused]] int type) {
        // TODO: massage ini_entry and type
    }

    INIEntry(const char *name, const char *default_value, INIPermission p)
        : name{name}, default_value{default_value}, permission{p} {}
};

template<typename S>
struct BoolINIEntry : INIEntry<BoolINIEntry<S>> {
    S setter;

    BoolINIEntry<S>(const char *name, const char *default_value,
                    INIPermission p, S setter)
        : INIEntry<BoolINIEntry<S>>{name, default_value, p},
          setter{setter} {}

    bool on_modify(zend::zstring_view new_value, INIStage) {
        bool value = zend_ini_parse_bool(new_value);
        setter(value);
        return true;
    }
};
template<typename S>
BoolINIEntry(const char *, const char *, INIPermission, S) -> BoolINIEntry<S>;
}

