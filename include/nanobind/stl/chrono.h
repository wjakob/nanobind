/*
    nanobind/stl/chrono.h: conversion between std::chrono and python's datetime

    Copyright (c) 2023 Hudson River Trading LLC <opensource@hudson-trading.com> and
                       Trent Houliston <trent@houliston.me> and
                       Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/
#pragma once

#include <nanobind/nanobind.h>

#if !defined(__STDC_WANT_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__ 1 // for localtime_s
#endif
#include <time.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <limits>

#include <nanobind/stl/detail/chrono.h>

// Casts a std::chrono type (either a duration or a time_point) to/from
// Python timedelta objects, or from a Python float representing seconds.
template <typename type> class duration_caster {
public:
    using rep = typename type::rep;
    using period = typename type::period;
    using duration_t = std::chrono::duration<rep, period>;

    bool from_python(handle src, uint8_t /*flags*/, cleanup_list*) noexcept {
        namespace ch = std::chrono;

        if (!src) return false;

        // support for signed 25 bits is required by the standard
        using days = ch::duration<int_least32_t, std::ratio<86400>>;

        // If invoked with datetime.delta object, unpack it
        int dd, ss, uu;
        try {
            if (unpack_timedelta(src.ptr(), &dd, &ss, &uu)) {
                value = type(ch::duration_cast<duration_t>(
                                 days(dd) + ch::seconds(ss) + ch::microseconds(uu)));
                return true;
            }
        } catch (python_error& e) {
            e.discard_as_unraisable(src.ptr());
            return false;
        }

        // If invoked with a float we assume it is seconds and convert
        int is_float;
#if defined(Py_LIMITED_API)
        is_float = PyType_IsSubtype(Py_TYPE(src.ptr()), &PyFloat_Type);
#else
        is_float = PyFloat_Check(src.ptr());
#endif
        if (is_float) {
            value = type(ch::duration_cast<duration_t>(
                             ch::duration<double>(PyFloat_AsDouble(src.ptr()))));
            return true;
        }
        return false;
    }

    // If this is a duration just return it back
    static const duration_t& get_duration(const duration_t& src) {
        return src;
    }

    // If this is a time_point get the time_since_epoch
    template <typename Clock>
    static duration_t get_duration(
            const std::chrono::time_point<Clock, duration_t>& src) {
        return src.time_since_epoch();
    }

    static handle from_cpp(const type& src, rv_policy, cleanup_list*) noexcept {
        namespace ch = std::chrono;

        // Use overloaded function to get our duration from our source
        // Works out if it is a duration or time_point and get the duration
        auto d = get_duration(src);

        // Declare these special duration types so the conversions happen with the correct primitive types (int)
        using dd_t = ch::duration<int, std::ratio<86400>>;
        using ss_t = ch::duration<int, std::ratio<1>>;
        using us_t = ch::duration<int, std::micro>;

        auto dd = ch::duration_cast<dd_t>(d);
        auto subd = d - dd;
        auto ss = ch::duration_cast<ss_t>(subd);
        auto us = ch::duration_cast<us_t>(subd - ss);
        return pack_timedelta(dd.count(), ss.count(), us.count());
    }

    #if PY_VERSION_HEX < 0x03090000
        NB_TYPE_CASTER(type, io_name("typing.Union[datetime.timedelta, float]",
                                     "datetime.timedelta"))
    #else
        NB_TYPE_CASTER(type, io_name("datetime.timedelta | float",
                                     "datetime.timedelta"))
    #endif
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

// Cast between times on the system clock and datetime.datetime instances
// (also supports datetime.date and datetime.time for Python->C++ conversions)
template <typename Duration>
class type_caster<std::chrono::time_point<std::chrono::system_clock, Duration>> {
public:
    using type = std::chrono::time_point<std::chrono::system_clock, Duration>;
    bool from_python(handle src, uint8_t /*flags*/, cleanup_list*) noexcept {
        namespace ch = std::chrono;

        if (!src)
            return false;

        std::tm cal;
        ch::microseconds msecs;
        int yy, mon, dd, hh, min, ss, uu;
        try {
            if (!unpack_datetime(src.ptr(), &yy, &mon, &dd,
                                 &hh, &min, &ss, &uu)) {
                return false;
            }
        } catch (python_error& e) {
            e.discard_as_unraisable(src.ptr());
            return false;
        }
        cal.tm_sec = ss;
        cal.tm_min = min;
        cal.tm_hour = hh;
        cal.tm_mday = dd;
        cal.tm_mon = mon - 1;
        cal.tm_year = yy - 1900;
        cal.tm_isdst = -1;
        msecs = ch::microseconds(uu);
        value = ch::time_point_cast<Duration>(
                ch::system_clock::from_time_t(std::mktime(&cal)) + msecs);
        return true;
    }

    static handle from_cpp(const type& src, rv_policy, cleanup_list*) noexcept {
        namespace ch = std::chrono;

        // Get out microseconds, and make sure they are positive, to
        // avoid bug in eastern hemisphere time zones
        // (cfr. https://github.com/pybind/pybind11/issues/2417). Note
        // that if us_t is 32 bits and we get a time_point that also
        // has a 32-bit time_since_epoch (perhaps because it's
        // measuring time in minutes or something), then writing `src
        // - us` below can lead to overflow based on how common_type
        // is defined on durations. Defining us_t to store 64-bit
        // microseconds works around this.
        using us_t = ch::duration<std::int64_t, std::micro>;
        auto us = ch::duration_cast<us_t>(src.time_since_epoch() %
                                          ch::seconds(1));
        if (us.count() < 0)
            us += ch::seconds(1);

        // Subtract microseconds BEFORE `system_clock::to_time_t`, because:
        // > If std::time_t has lower precision, it is implementation-defined
        //   whether the value is rounded or truncated.
        // (https://en.cppreference.com/w/cpp/chrono/system_clock/to_time_t)
        std::time_t tt = ch::system_clock::to_time_t(
                ch::time_point_cast<ch::system_clock::duration>(src - us));

        std::tm localtime;
        if (!localtime_thread_safe(&tt, &localtime)) {
            PyErr_Format(PyExc_ValueError,
                         "Unable to represent system_clock in local time; "
                         "got time_t %ld", static_cast<std::int64_t>(tt));
            return handle();
        }
        return pack_datetime(localtime.tm_year + 1900,
                             localtime.tm_mon + 1,
                             localtime.tm_mday,
                             localtime.tm_hour,
                             localtime.tm_min,
                             localtime.tm_sec,
                             (int) us.count());
    }
    #if PY_VERSION_HEX < 0x03090000
        NB_TYPE_CASTER(type, io_name("typing.Union[datetime.datetime, datetime.date, datetime.time]",
                                     "datetime.datetime"))
    #else
        NB_TYPE_CASTER(type, io_name("datetime.datetime | datetime.date | datetime.time",
                                     "datetime.datetime"))
    #endif
};

// Other clocks that are not the system clock are not measured as
// datetime.datetime objects since they are not measured on calendar
// time. So instead we just make them timedeltas; or if they have
// passed us a time as a float, we convert that.
template <typename Clock, typename Duration>
class type_caster<std::chrono::time_point<Clock, Duration>>
  : public duration_caster<std::chrono::time_point<Clock, Duration>> {};

template <typename Rep, typename Period>
class type_caster<std::chrono::duration<Rep, Period>>
  : public duration_caster<std::chrono::duration<Rep, Period>> {};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
