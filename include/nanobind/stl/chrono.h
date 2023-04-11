/*
    nanobind/stl/chrono.h: Transparent conversion between std::chrono and python's datetime

    Copyright (c) 2016 Trent Houliston <trent@houliston.me> and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>

#define __STDC_WANT_LIB_EXT1__ 1 // for localtime_s
#include <time.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <limits>
#include <type_traits>

#ifndef Py_LIMITED_API
#include <datetime.h>
#endif

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

#ifdef Py_LIMITED_API

// <datetime.h> doesn't export any symbols under the limited API, so we'll
// do it the hard way.

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

inline datetime_types_t datetime_types;

// The next few functions are marked 'inline' for linkage purposes
// (since they might be in multiple translation units and the linker
// should pick one) but NB_NOINLINE because we don't want the bloat
// of actually inlining them. They are defined in this header instead
// of in the built nanobind library in order to avoid increasing the
// library size for users who don't care about datetimes.

NB_NOINLINE inline bool set_from_int_attr(int *dest, PyObject *o,
                                          const char *name) {
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

#endif // Py_LIMITED_API

// Unpack a datetime.timedelta object into integer days, seconds, and
// microseconds. Returns true if successful, false if `o` is not a timedelta,
// or false and sets the Python error indicator if something else went wrong.
NB_NOINLINE inline bool unpack_timedelta(PyObject *o, int *days,
                                         int *secs, int *usecs) noexcept {
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

// Unpack a datetime.date, datetime.time, or datetime.datetime object into
// integer year, month, day, hour, minute, second, and microsecond fields.
// Time objects will be considered to represent that time on Jan 1, 1970.
// Date objects will be considered to represent midnight on that date.
// Returns true if succesful, false if `o` is not a date, time, or datetime,
// or false and sets the Python error indicator if something else went wrong.
NB_NOINLINE inline bool unpack_datetime(PyObject *o,
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

// Create a datetime.timedelta object from integer days, seconds, and
// microseconds.  Returns a new reference, or nullptr and sets the
// Python error indicator on error.
inline PyObject* pack_timedelta(int days, int secs, int usecs) noexcept {
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

// Create a timezone-naive datetime.datetime object from its components.
// Returns a new reference, or nullptr and sets the Python error indicator
// on error.
inline PyObject* pack_datetime(int year, int month, int day,
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

template <typename type> class duration_caster {
public:
    using rep = typename type::rep;
    using period = typename type::period;

    using days = std::chrono::duration<int_least32_t, std::ratio<86400>>; // signed 25 bits required by the standard.

    bool from_python(handle src, uint8_t /*flags*/, cleanup_list*) noexcept {
        using namespace std::chrono;

        if (!src) return false;

        // If invoked with datetime.delta object, unpack it
        int dd, ss, uu;
        if (unpack_timedelta(src.ptr(), &dd, &ss, &uu)) {
            value = type(duration_cast<duration<rep, period>>(
                             days(dd) + seconds(ss) + microseconds(uu)));
            return true;
        } else if (PyErr_Occurred()) {
            return false;
        }

        // If invoked with a float we assume it is seconds and convert
        int is_float;
#ifdef Py_LIMITED_API
        is_float = PyObject_IsInstance(src.ptr(), (PyObject *) &PyFloat_Type);
        if (is_float < 0) {
            return false;
        }
#else
        is_float = PyFloat_Check(src.ptr());
#endif
        if (is_float) {
            value = type(duration_cast<duration<rep, period>>(duration<double>(PyFloat_AsDouble(src.ptr()))));
            return true;
        }
        return false;
    }

    // If this is a duration just return it back
    static const std::chrono::duration<rep, period>& get_duration(const std::chrono::duration<rep, period> &src) {
        return src;
    }

    // If this is a time_point get the time_since_epoch
    template <typename Clock> static std::chrono::duration<rep, period> get_duration(const std::chrono::time_point<Clock, std::chrono::duration<rep, period>> &src) {
        return src.time_since_epoch();
    }

    static handle from_cpp(const type &src, rv_policy, cleanup_list*) noexcept {
        using namespace std::chrono;

        // Use overloaded function to get our duration from our source
        // Works out if it is a duration or time_point and get the duration
        auto d = get_duration(src);

        // Declare these special duration types so the conversions happen with the correct primitive types (int)
        using dd_t = duration<int, std::ratio<86400>>;
        using ss_t = duration<int, std::ratio<1>>;
        using us_t = duration<int, std::micro>;

        auto dd = duration_cast<dd_t>(d);
        auto subd = d - dd;
        auto ss = duration_cast<ss_t>(subd);
        auto us = duration_cast<us_t>(subd - ss);
        return pack_timedelta(dd.count(), ss.count(), us.count());
    }

    NB_TYPE_CASTER(type, const_name("datetime.timedelta"));
};

template <class... Args>
auto can_localtime_s(Args*... args) ->
    decltype((localtime_s(args...), std::true_type{}));
std::false_type can_localtime_s(...);

template <class... Args>
auto can_localtime_r(Args*... args) ->
    decltype((localtime_r(args...), std::true_type{}));
std::false_type can_localtime_r(...);

template <class Time, class Buf>
inline std::tm *localtime_thread_safe(const Time *time, Buf *buf) {
    if constexpr (decltype(can_localtime_s(time, buf))::value) {
        // C11 localtime_s
        std::tm* ret = localtime_s(time, buf);
        return ret;
    } else if constexpr (decltype(can_localtime_s(buf, time))::value) {
        // Microsoft localtime_s (with parameters switched and errno_t return)
        int ret = localtime_s(buf, time);
        return ret == 0 ? buf : nullptr;
    } else {
        static_assert(decltype(can_localtime_r(time, buf))::value,
                      "<nanobind/stl/chrono.h> type caster requires "
                      "that your C library support localtime_r or localtime_s");
        std::tm* ret = localtime_r(time, buf);
        return ret;
    }
}

// This is for casting times on the system clock into datetime.datetime instances
template <typename Duration> class type_caster<std::chrono::time_point<std::chrono::system_clock, Duration>> {
public:
    using type = std::chrono::time_point<std::chrono::system_clock, Duration>;
    bool from_python(handle src, uint8_t /*flags*/, cleanup_list*) noexcept {
        using namespace std::chrono;

        if (!src) return false;

        std::tm cal;
        microseconds msecs;
        int yy, mon, dd, hh, min, ss, uu;
        if (!unpack_datetime(src.ptr(), &yy, &mon, &dd, &hh, &min, &ss, &uu)) {
            return false;
        }
        cal.tm_sec = ss;
        cal.tm_min = min;
        cal.tm_hour = hh;
        cal.tm_mday = dd;
        cal.tm_mon = mon - 1;
        cal.tm_year = yy - 1900;
        cal.tm_isdst = -1;
        msecs = microseconds(uu);
        value = time_point_cast<Duration>(system_clock::from_time_t(std::mktime(&cal)) + msecs);
        return true;
    }

    static handle from_cpp(const std::chrono::time_point<std::chrono::system_clock, Duration> &src, rv_policy, cleanup_list*) noexcept {
        using namespace std::chrono;

        // Get out microseconds, and make sure they are positive, to avoid bug in eastern hemisphere time zones
        // (cfr. https://github.com/pybind/pybind11/issues/2417)
        using us_t = duration<int, std::micro>;
        auto us = duration_cast<us_t>(src.time_since_epoch() % seconds(1));
        if (us.count() < 0)
            us += seconds(1);

        // Subtract microseconds BEFORE `system_clock::to_time_t`, because:
        // > If std::time_t has lower precision, it is implementation-defined whether the value is rounded or truncated.
        // (https://en.cppreference.com/w/cpp/chrono/system_clock/to_time_t)
        std::time_t tt = system_clock::to_time_t(time_point_cast<system_clock::duration>(src - us));

        std::tm localtime;
        if (!localtime_thread_safe(&tt, &localtime)) {
            PyErr_SetString(PyExc_ValueError,
                            "Unable to represent system_clock in local time");
            return handle();
        }
        return pack_datetime(localtime.tm_year + 1900,
                             localtime.tm_mon + 1,
                             localtime.tm_mday,
                             localtime.tm_hour,
                             localtime.tm_min,
                             localtime.tm_sec,
                             us.count());
    }
    NB_TYPE_CASTER(type, const_name("datetime.datetime"));
};

// Other clocks that are not the system clock are not measured as datetime.datetime objects
// since they are not measured on calendar time. So instead we just make them timedeltas
// Or if they have passed us a time as a float we convert that
template <typename Clock, typename Duration> class type_caster<std::chrono::time_point<Clock, Duration>>
: public duration_caster<std::chrono::time_point<Clock, Duration>> {
};

template <typename Rep, typename Period> class type_caster<std::chrono::duration<Rep, Period>>
: public duration_caster<std::chrono::duration<Rep, Period>> {
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
