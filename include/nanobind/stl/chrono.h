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
#include <type_traits>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

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
