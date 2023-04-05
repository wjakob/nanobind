#include <nanobind/nanobind.h>
#include "nb_internals.h"

#include <limits>

#ifndef Py_LIMITED_API
#include <datetime.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#ifdef Py_LIMITED_API

// <datetime.h> doesn't export any symbols under the limited API, so we'll
// do it the hard way.

NAMESPACE_BEGIN()

struct datetime_types_t {
    // Types defined by the datetime module
    handle datetime;
    handle time;
    handle date;
    handle timedelta;

    explicit operator bool() const { return datetime.ptr() != nullptr; }

    bool do_import() {
        auto mod = module_::import_("datetime");
        object datetime_o, time_o, date_o, timedelta_o;
        try {
            datetime_o = mod.attr("datetime");
            time_o = mod.attr("time");
            date_o = mod.attr("date");
            timedelta_o = mod.attr("timedelta");
        } catch (python_error& err) {
            err.restore();
            return false;
        }
        // Leak references to these datetime types. We could improve upon
        // this by storing them in the internals structure and decref'ing
        // in internals_cleanup(), but it doesn't seem worthwhile for
        // something this fundamental. We can't store nb::object in this
        // structure because it might be destroyed after the Python
        // interpreter has finalized.
        datetime = datetime_o.release();
        time = time_o.release();
        date = date_o.release();
        timedelta = timedelta_o.release();
        return true;
    }
};

datetime_types_t datetime_types;

bool set_from_int_attr(int *dest, PyObject *o, const char *name) {
    PyObject *value = PyObject_GetAttrString(o, name);
    if (!value) return false;
    long lval = PyLong_AsLong(value);
    if (lval == -1 && PyErr_Occurred()) {
        Py_DECREF(value);
        return false;
    }
    Py_CLEAR(value);
    if (lval < std::numeric_limits<int>::min() ||
        lval > std::numeric_limits<int>::max()) {
        PyErr_Format(PyExc_OverflowError,
                     "%R attribute '%s' (%R) does not fit in an int",
                     o, name, value);
        return false;
    }
    *dest = static_cast<int>(lval);
    return true;
}

NAMESPACE_END()

#endif

// ========================================================================

NB_CORE bool unpack_timedelta(PyObject *o,
                              int *days, int *secs, int *usecs) noexcept {
#ifndef Py_LIMITED_API
    if (!PyDateTimeAPI) {
        PyDateTime_IMPORT;
        if (!PyDateTimeAPI) {
            return false;
        }
    }
    if (PyDelta_Check(o)) {
        *days = PyDateTime_DELTA_GET_DAYS(o);
        *secs = PyDateTime_DELTA_GET_SECONDS(o);
        *usecs = PyDateTime_DELTA_GET_MICROSECONDS(o);
        return true;
    }
#else
    if (!datetime_types && !datetime_types.do_import()) {
        return false;
    }
    if (int is_td = PyObject_IsInstance(o, datetime_types.timedelta.ptr());
        is_td < 0) {
        return false;
    } else if (is_td > 0) {
        return (set_from_int_attr(days, o, "days") &&
                set_from_int_attr(secs, o, "seconds") &&
                set_from_int_attr(usecs, o, "microseconds"));
    }
#endif
    return false;
}

NB_CORE bool unpack_datetime(PyObject *o,
                             int *year, int *month, int *day,
                             int *hour, int *minute, int *second,
                             int *usec) noexcept {
#ifndef Py_LIMITED_API
    if (!PyDateTimeAPI) {
        PyDateTime_IMPORT;
        if (!PyDateTimeAPI) {
            return false;
        }
    }
    if (PyDateTime_Check(o)) {
        *usec = PyDateTime_DATE_GET_MICROSECOND(o);
        *second = PyDateTime_DATE_GET_SECOND(o);
        *minute = PyDateTime_DATE_GET_MINUTE(o);
        *hour = PyDateTime_DATE_GET_HOUR(o);
        *day = PyDateTime_GET_DAY(o);
        *month = PyDateTime_GET_MONTH(o);
        *year = PyDateTime_GET_YEAR(o);
        return true;
    }
    if (PyDate_Check(o)) {
        *usec = 0;
        *second = 0;
        *minute = 0;
        *hour = 0;
        *day = PyDateTime_GET_DAY(o);
        *month = PyDateTime_GET_MONTH(o);
        *year = PyDateTime_GET_YEAR(o);
        return true;
    }
    if (PyTime_Check(o)) {
        *usec = PyDateTime_TIME_GET_MICROSECOND(o);
        *second = PyDateTime_TIME_GET_SECOND(o);
        *minute = PyDateTime_TIME_GET_MINUTE(o);
        *hour = PyDateTime_TIME_GET_HOUR(o);
        *day = 1;
        *month = 1;
        *year = 1970;
        return true;
    }
#else
    if (!datetime_types && !datetime_types.do_import()) {
        return false;
    }
    if (int is_dt = PyObject_IsInstance(o, datetime_types.datetime.ptr());
        is_dt < 0) {
        return false;
    } else if (is_dt > 0) {
        return (set_from_int_attr(usec, o, "microsecond") &&
                set_from_int_attr(second, o, "second") &&
                set_from_int_attr(minute, o, "minute") &&
                set_from_int_attr(hour, o, "hour") &&
                set_from_int_attr(day, o, "day") &&
                set_from_int_attr(month, o, "month") &&
                set_from_int_attr(year, o, "year"));
    }
    if (int is_date = PyObject_IsInstance(o, datetime_types.date.ptr());
        is_date < 0) {
        return false;
    } else if (is_date > 0) {
        *usec = *second = *minute = *hour = 0;
        return (set_from_int_attr(day, o, "day") &&
                set_from_int_attr(month, o, "month") &&
                set_from_int_attr(year, o, "year"));
    }
    if (int is_time = PyObject_IsInstance(o, datetime_types.time.ptr());
        is_time < 0) {
        return false;
    } else if (is_time > 0) {
        *day = 1;
        *month = 1;
        *year = 1970;
        return (set_from_int_attr(usec, o, "microsecond") &&
                set_from_int_attr(second, o, "second") &&
                set_from_int_attr(minute, o, "minute") &&
                set_from_int_attr(hour, o, "hour"));
    }
#endif
    return false;
}

NB_CORE PyObject* pack_timedelta(int days, int secs, int usecs) noexcept {
#ifndef Py_LIMITED_API
    if (!PyDateTimeAPI) {
        PyDateTime_IMPORT;
        if (!PyDateTimeAPI) {
            return nullptr;
        }
    }
    return PyDelta_FromDSU(days, secs, usecs);
#else
    if (!datetime_types && !datetime_types.do_import()) {
        return nullptr;
    }
    return datetime_types.timedelta(days, secs, usecs).release().ptr();
#endif
}

NB_CORE PyObject* pack_datetime(int year, int month, int day,
                                int hour, int minute, int second,
                                int usec) noexcept {
#ifndef Py_LIMITED_API
    if (!PyDateTimeAPI) {
        PyDateTime_IMPORT;
        if (!PyDateTimeAPI) {
            return nullptr;
        }
    }
    return PyDateTime_FromDateAndTime(year, month, day,
                                      hour, minute, second, usec);
#else
    if (!datetime_types && !datetime_types.do_import()) {
        return nullptr;
    }
    return datetime_types.datetime(year, month, day,
                                   hour, minute, second, usec).release().ptr();
#endif
}

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
