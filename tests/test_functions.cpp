#include <nanobind/nanobind.h>

#include <limits>
#include <string.h>
#include <thread>

#include <nanobind/stl/function.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

int call_guard_value = 0;

struct my_call_guard {
    my_call_guard() { call_guard_value = 1; }
    ~my_call_guard() { call_guard_value = 2; }
};

// Example call policy for use with nb::call_policy<>. Each call will add
// an entry to `calls` containing the arguments tuple and return value.
// The return value will be recorded as "<unfinished>" if the function
// did not return (still executing or threw an exception) and as
// "<return conversion failed>" if the function returned something that we
// couldn't convert to a Python object.
// Additional features to test particular interactions:
// - the precall hook will throw if any arguments are not strings
// - any argument equal to "swapfrom" will be replaced by a temporary
//   string object equal to "swapto", which will be destroyed at end of call
// - the postcall hook will throw if any argument equals "postthrow"
struct example_policy {
    static inline std::vector<std::pair<nb::tuple, nb::object>> calls;
    static void precall(PyObject **args, size_t nargs,
                        nb::detail::cleanup_list *cleanup) {
        PyObject* tup = PyTuple_New((Py_ssize_t) nargs);
        for (size_t i = 0; i < nargs; ++i) {
            if (!PyUnicode_CheckExact(args[i])) {
                Py_DECREF(tup);
                throw std::runtime_error("expected only strings");
            }
            if (0 == PyUnicode_CompareWithASCIIString(args[i], "swapfrom")) {
                nb::object replacement = nb::cast("swapto");
                args[i] = replacement.ptr();
                cleanup->append(replacement.release().ptr());
            }
            Py_INCREF(args[i]);
            PyTuple_SetItem(tup, (Py_ssize_t) i, args[i]);
        }
        calls.emplace_back(nb::steal<nb::tuple>(tup), nb::cast("<unfinished>"));
    }
    static void postcall(PyObject **args, size_t nargs, nb::handle ret) {
        if (!ret.is_valid()) {
            calls.back().second = nb::cast("<return conversion failed>");
        } else {
            calls.back().second = nb::borrow(ret);
        }
        for (size_t i = 0; i < nargs; ++i) {
            if (0 == PyUnicode_CompareWithASCIIString(args[i], "postthrow")) {
                throw std::runtime_error("postcall exception");
            }
        }
    }
};

struct numeric_string {
    unsigned long number;
};

template <> struct nb::detail::type_caster<numeric_string> {
    NB_TYPE_CASTER(numeric_string, const_name("str"))

    bool from_python(handle h, uint32_t flags, cleanup_list* cleanup) noexcept {
        make_caster<const char*> str_caster;
        if (!str_caster.from_python(h, flags, cleanup))
            return false;
        const char* str = str_caster.operator cast_t<const char*>();
        if (!str)
            return false;
        char* endp;
        value.number = strtoul(str, &endp, 10);
        return *str && !*endp;
    }
    static handle from_cpp(numeric_string, rv_policy, handle) noexcept {
        return nullptr;
    }
};

int test_31(int i) noexcept { return i; }

NB_MODULE(test_functions_ext, m) {
    m.doc() = "function testcase";

    // Function without inputs/outputs
    m.def("test_01", []() { });

    // Simple binary function (via function pointer)
    auto test_02 = [](int up, int down) -> int { return up - down; };
    m.def("test_02", (int (*)(int, int)) test_02, "up"_a = 8, "down"_a = 1);

    // Simple binary function with capture object
    int i = 42;
    m.def("test_03", [i](int j, int k) -> int { return i + j - k; });

    // Large capture object requiring separate storage
    uint64_t k = 10, l = 11, m_ = 12, n = 13, o = 14;
    m.def("test_04", [k, l, m_, n, o]() -> int { return (int) (k + l + m_ + n + o); });

    // Overload chain with two docstrings
    m.def("test_05", [](int) -> int { return 1; }, "doc_1");
    nb::object first_overload = m.attr("test_05");
    m.def("test_05", [](float) -> int { return 2; }, "doc_2");
#if !defined(PYPY_VERSION) && !defined(Py_GIL_DISABLED)
    // Make sure we don't leak the previous member of the overload chain
    // (pypy's refcounts are bogus and will not help us with this check)
    if (first_overload.ptr()->ob_refcnt != 1) {
        throw std::runtime_error("Overload was leaked!");
    }
#endif
    first_overload.reset();

    // Test an overload chain that always repeats the same docstring
    m.def("test_05b", [](int) -> int { return 1; }, "doc_1");
    m.def("test_05b", [](float) -> int { return 2; }, "doc_1");

    // Test an overload chain with an empty docstring
    m.def("test_05c", [](int) -> int { return 1; }, "doc_1");
    m.def("test_05c", [](float) -> int { return 2; }, "");

    // Test a partially repeated docstring followed by a distinct one
    m.def("test_05d", [](int) -> int { return 1; }, "doc_1");
    m.def("test_05d", [](float) -> int { return 2; }, "doc_1");
    m.def("test_05d", [](const char *) -> int { return 3; }, "doc_2");

    /// Function raising an exception
    m.def("test_06", []() { throw std::runtime_error("oops!"); });

    /// Function taking some positional/keyword args and nb::[kw]args
    m.def("test_07", [](int, int, nb::args args, nb::kwargs kwargs) {
        return std::make_pair(args.size(), kwargs.size());
    });

    /// As above, but with nb::arg annotations
    m.def("test_07", [](int, int, nb::args args, nb::kwargs kwargs) {
        return std::make_pair(args.size(), kwargs.size());
    }, "a"_a, "b"_a, "myargs"_a, "mykwargs"_a);

    /// Function with eight arguments
    m.def("test_simple",
        [](int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7) {
            return i0 + i1 + i2 + i3 + i4 + i5 + i6 - i7;
        });

    /// Test successful/unsuccessful tuple conversion, with rich output types
    m.def("test_tuple", []() -> nb::typed<nb::tuple, std::string, int> {
        return nb::make_tuple("Hello", 123); });
    m.def("test_bad_tuple", []() -> nb::typed<nb::object, std::pair<std::string, nb::object>> {
        struct Foo{}; return nb::make_tuple("Hello", Foo()); });

    /// Incremental construction of dynamically sized tuples/lists
    m.def("test_tuple_builder", [](nb::list l) {
        nb::tuple_builder b(l.size());
        for (nb::handle h : l)
            b.put(h);
        return b.commit();
    });
    m.def("test_list_builder", [](nb::tuple t) {
        nb::list_builder b(t.size());
        for (nb::handle h : t)
            b.put(h);
        return b.commit();
    });
    m.def("test_builder_incomplete", []() {
        nb::tuple_builder b(2);
        b.put(1);
        return b.commit();
    });
    m.def("test_builder_put_fail", []() {
        struct Foo { };
        nb::list_builder b(2);
        b.put(1);
        b.put(Foo()); // Returns false, making the later commit() raise
        return b.commit();
    });
    m.def("test_builder_abandon", [](nb::handle h) {
        nb::list_builder b(3);
        b.put(h);
        b.put(h);
        throw std::runtime_error("abandoned");
    });
    m.def("test_builder_reuse", []() {
        nb::tuple_builder b(1);
        b.put(1);
        nb::tuple t = b.commit();
        return b.commit();
    });
    m.def("test_builder_checks", []() {
#if !defined(NDEBUG)
        return true;
#else
        return false;
#endif
    });

    /// Perform a Python function call from C++
    m.def("test_call_1", [](nb::typed<nb::object, std::function<int(int)>> o) {
        return o(1);
    });
    m.def("test_call_2", [](nb::typed<nb::callable, void(int, int)> o) {
        return o(1, 2);
    });

    /// Test expansion of args/kwargs-style arguments
    m.def("test_call_extra", [](nb::typed<nb::callable, void(...)> o,
                                nb::args args, nb::kwargs kwargs) {
        return o(1, 2, *args, **kwargs, "extra"_a = 5);
    });

    /// Test '*' and '**' expansion of arbitrary iterables and mappings
    m.def("test_call_star", [](nb::object f, nb::object seq) {
        return f(*seq);
    });
    m.def("test_call_dstar", [](nb::object f, nb::object map) {
        return f(**map);
    });
    m.def("test_call_star_dstar", [](nb::object f, nb::object seq, nb::object map) {
        return f(*seq, **map);
    });

    /// A keyword argument object may be reused across calls
    m.def("test_call_kwarg_lvalue", [](nb::object f) {
        nb::arg_v kw = nb::arg("x") = 42;
        return nb::make_tuple(f(kw), f(kw));
    });

    /// Method call with keyword arguments and expansions
    m.def("test_call_method_complex", [](nb::object o, nb::object seq, nb::object map) {
        return o.attr("meth")(1, *seq, "k"_a = 2, **map);
    });

    /// Attribute and keyword names passed through a reused buffer must not
    /// be confused by the interned-string cache
    m.def("test_attr_dynamic", [](nb::object o) {
        char buf[32];
        nb::list result;
        for (int i = 0; i < 3; ++i) {
            snprintf(buf, sizeof(buf), "field_%i", i);
            result.append(o.attr(buf));
        }
        for (int i = 0; i < 3; ++i) {
            snprintf(buf, sizeof(buf), "kw_%i", i);
            result.append(o.attr("collect")(nb::arg(buf) = i));
        }
        return result;
    });

    /// Raw vector calls, once with two positional arguments and once with one
    /// positional and one keyword argument. 'stack[0]' is the scratch slot
    /// that NB_VECTORCALL_ARGUMENTS_OFFSET promises to the callee.
    m.def("test_vectorcall_raw", [](nb::object f, nb::object a, nb::object b) {
        PyObject *stack[3] = { nullptr, a.ptr(), b.ptr() };
        size_t nargsf = 2 | NB_VECTORCALL_ARGUMENTS_OFFSET;

        nb::object pos = nb::steal(
            nb::detail::vectorcall(f.ptr(), stack + 1, nargsf, nullptr));

        nb::object kwnames = nb::make_tuple("x");
        nb::object kw = nb::steal(nb::detail::vectorcall(
            f.ptr(), stack + 1, 1 | NB_VECTORCALL_ARGUMENTS_OFFSET,
            kwnames.ptr()));

        return nb::make_tuple(pos, kw, NB_VECTORCALL_NARGS(nargsf));
    });

    /// The method flavor looks 'name' up on 'stack[1]', which counts towards
    /// 'nargsf'. A failure returns null with an error set and does not raise.
    m.def("test_vectorcall_raw_method", [](nb::object o, nb::object a) {
        PyObject *stack[3] = { nullptr, o.ptr(), a.ptr() };
        size_t nargsf = 2 | NB_VECTORCALL_ARGUMENTS_OFFSET;
        nb::object name = nb::cast("meth"), missing = nb::cast("nonexistent");

        nb::object r = nb::steal(nb::detail::vectorcall_method(
            name.ptr(), stack + 1, nargsf, nullptr));

        PyObject *bad = nb::detail::vectorcall_method(
            missing.ptr(), stack + 1, nargsf, nullptr);
        bool raised = !bad && PyErr_Occurred();
        Py_XDECREF(bad);
        PyErr_Clear();

        return nb::make_tuple(r, raised);
    });

    /// Calls involving null objects must raise instead of crashing
    m.def("test_call_null_base", []() { return nb::object()(1); });
    m.def("test_call_null_arg", [](nb::object f) { return f(nb::object()); });
    m.def("test_call_null_kwarg", [](nb::object f) { return f("x"_a = nb::object()); });

    /// Test list manipulation
    m.def("test_list", [](nb::list l) {
        int result = 0;
        for (size_t i = 0; i < l.size(); ++i)
            result += nb::cast<int>(l[i]);
        l[2] = 123;
        l.append(result);
    });

    /// Test tuple manipulation
    m.def("test_tuple", [](nb::typed<nb::tuple, int, nb::ellipsis> l) {
        int result = 0;
        for (size_t i = 0; i < l.size(); ++i)
            result += nb::cast<int>(l[i]);
        return result;
    });

    /// Test call_guard feature
    m.def("test_call_guard_wrapper_rvalue_ref", [](int&& i) { return i; },
          nb::call_guard<my_call_guard>());

    m.def("test_call_guard", []() {
        return call_guard_value;
    }, nb::call_guard<my_call_guard>());

    m.def("call_guard_value", []() { return call_guard_value; });

    m.def("test_release_gil", []() -> bool {
#if defined(Py_LIMITED_API)
        return false;
#else
        return PyGILState_Check();
#endif
    }, nb::call_guard<nb::gil_scoped_release>());

    // Acquire on a thread that already has a thread state
    m.def("test_acquire_gil_nested", []() -> bool {
        nb::gil_scoped_acquire g1;
        nb::gil_scoped_acquire g2;
        return g1.is_valid() && g2.is_valid() &&
               nb::cast<int>(nb::int_(3) + nb::int_(4)) == 7;
    });

    // Reattach a thread state from a thread that just gave one up
    m.def("test_reacquire_gil", []() -> bool {
        nb::gil_scoped_acquire guard;
        return guard.is_valid() && nb::cast<int>(nb::int_(3) + nb::int_(4)) == 7;
    }, nb::call_guard<nb::gil_scoped_release>());

    // Same from a thread that Python has never seen
    m.def("test_acquire_gil_foreign", []() -> bool {
        bool result = false;
        std::thread t([&] {
            nb::gil_scoped_acquire guard;
            result = guard.is_valid() &&
                     nb::cast<int>(nb::int_(3) + nb::int_(4)) == 7;
        });
        t.join();
        return result;
    }, nb::call_guard<nb::gil_scoped_release>());

    m.def("test_print", []{
        nb::print("Test 1");
        nb::print("Test 2"_s);
    });

    m.def("test_iter", [](nb::object in) {
        nb::list l;
        for (nb::handle h : in)
            l.append(h);
        return l;
    });

    m.def("test_iter_tuple", [](nb::tuple in) {
        nb::list l;
        for (nb::handle h : in)
            l.append(h);
        return l;
    });

    m.def("test_iter_list", [](nb::list in) {
        nb::list l;
        for (nb::handle h : in)
            l.append(h);
        return l;
    });

    // Overload chain with a raw docstring that has precedence
    m.def("test_08", [](int) -> int { return 1; }, "first docstring");
    m.def("test_08", [](float) -> int { return 2; },
          nb::sig("def test_08(x: typing.Annotated[float, 'foo']) -> int"),
          "another docstring");

    // Manual type check
    m.def("test_09", [](nb::type_object t) -> bool { return t.is(&PyBool_Type); });

    // nb::dict iterator
    m.def("test_10", [](nb::dict d) {
        nb::dict result;
        for (auto [k, v] : d)
            result[k] = v;
        return result;
    });

    m.def("test_10_contains", [](nb::dict d) {
        return d.contains("foo"_s);
    });

    // Test string caster
    m.def("test_12", [](const char *c) { return nb::str(c); });
    m.def("test_13", []() -> const char * { return "test"; });
    m.def("test_14", [](nb::object o) -> const char * { return nb::cast<const char *>(o); });

    // Test bytes type
    m.def("test_15",   [](nb::bytes o) -> const char * { return o.c_str(); });
    m.def("test_15_d", [](nb::bytes o) { return nb::bytes(o.data(), o.size()); });
    m.def("test_16",   [](const char *c) { return nb::bytes(c); });
    m.def("test_17",   [](nb::bytes c) { return c.size(); });
    m.def("test_18",   [](const char *c, int size) { return nb::bytes(c, (size_t) size); });

    // Test int type
    m.def("test_19", [](nb::int_ i) { return i + nb::int_(123); });
    m.def("test_20", [](nb::str s) { return nb::int_(s) + nb::int_(123); });
    m.def("test_21", [](nb::int_ i) { return (int) i; });
    m.def("test_21_f", [](nb::float_ f) { return nb::int_(f); });
    m.def("test_21_g", []() { return nb::int_(1.5); });
    m.def("test_21_h", []() { return nb::int_(1e50); });
    m.def("test_21_char",  []() { return nb::int_((char) 'a'); });
    m.def("test_21_schar", []() { return nb::int_((signed char) 'a'); });
    m.def("test_21_uchar", []() { return nb::int_((unsigned char) 'a'); });
    m.def("test_21_short", []() { return nb::int_((short) -5); });
    m.def("test_21_bool",  []() { return nb::int_(true); });

    // Test floating-point
    m.def("test_21_dnc", [](double d) { return d + 1.0; }, nb::arg().noconvert());
    m.def("test_21_fnc", [](float f) { return f + 1.0f; }, nb::arg().noconvert());

    // Test capsule wrapper
    m.def("test_22", []() -> void * { return (void*) 1; });
    m.def("test_23", []() -> void * { return nullptr; });
    m.def("test_24", [](void *p) { return (uintptr_t) p; }, "p"_a.none());

    // Test capsule with nullptr
    m.def("test_capsule_nullptr", []() {
        return nb::capsule(nullptr, [](void *) noexcept {});
    });
    m.def("test_capsule_nullptr_no_cleanup", []() {
        return nb::capsule(nullptr);
    });

    // Test slice
    m.def("test_25", [](nb::slice s) { return s; });
    m.def("test_26", []() { return nb::slice(4); });
    m.def("test_27", []() {
        nb::slice s(2, 10);
        auto tpl = s.compute(7);
        if (tpl.get<0>() != 2) return nb::slice(400);  // fail
        auto [start, stop, step, slice_length] = tpl;
        if (start != 2) return nb::slice(401);         // fail
        if (stop != 7) return nb::slice(402);          // fail
        if (step != 1) return nb::slice(403);          // fail
        if (slice_length != 5) return nb::slice(404);  // fail
        return s;
    });
    m.def("test_28", []() { return nb::slice(5, -5, -2); });

    // Test ellipsis
    m.def("test_29", [](nb::ellipsis) { return nb::ellipsis(); });

    // Traceback test
    m.def("test_30", [](nb::callable f) -> std::string {
        nb::gil_scoped_release g;
        try {
            nb::gil_scoped_acquire g2;
            f();
        } catch (const nb::python_error &e) {
            return e.what();
        }
        return "Unknown";
    });

    m.def("test_31", &test_31);
    m.def("test_32", [](int i) noexcept { return i; });

    // An identity function for every integer type that the type caster
    // handles, plus a table with the range of each of them. The Python test
    // 'test31_range' iterates over this table.
    nb::dict int_limits;
    auto bind_int = [&](const char *name, auto tag) {
        using T = decltype(tag);
        m.def(("identity_" + std::string(name)).c_str(), [](T value) { return value; });
        int_limits[name] = nb::make_tuple(std::numeric_limits<T>::min(),
                                          std::numeric_limits<T>::max());
    };

    bind_int("i8",     (int8_t) 0);
    bind_int("u8",     (uint8_t) 0);
    bind_int("i16",    (int16_t) 0);
    bind_int("u16",    (uint16_t) 0);
    bind_int("i32",    (int32_t) 0);
    bind_int("u32",    (uint32_t) 0);
    bind_int("i64",    (int64_t) 0);
    bind_int("u64",    (uint64_t) 0);
    bind_int("schar",  (signed char) 0);
    bind_int("uchar",  (unsigned char) 0);
    bind_int("short",  (short) 0);
    bind_int("ushort", (unsigned short) 0);
    bind_int("int",    (int) 0);
    bind_int("uint",   (unsigned int) 0);
    bind_int("long",   (long) 0);
    bind_int("ulong",  (unsigned long) 0);
    bind_int("llong",  (long long) 0);
    bind_int("ullong", (unsigned long long) 0);

    m.attr("int_limits") = int_limits;

    m.attr("test_33") = nb::cpp_function([](nb::object self, int y) {
        return nb::cast<int>(self.attr("x")) + y;
    }, nb::is_method());

    m.attr("test_34") = nb::cpp_function([](nb::object self, int y) {
        return nb::cast<int>(self.attr("x")) * y;
    }, nb::arg("y"), nb::is_method());

    m.def("test_35", []() {
        const char *name = "Foo";

        auto callback = [=]() {
            return "Test {}"_s.format(name);
        };

        return nb::cpp_function(callback);
    });

    m.def("test_cast_char", [](nb::handle h) {
        return nb::cast<char>(h);
    });

    m.def("test_cast_str", [](nb::handle h) {
        return nb::cast<const char *>(h);
    });

    // Two overloads where the first matches any object and internally performs
    // a 'nb::cast<char>' that fails for multi-character strings. A failing cast
    // must surface as cast_error rather than silently re-dispatching to the
    // second (string) overload.
    m.def("test_cast_redispatch", [](nb::handle h) {
        return std::string(1, nb::cast<char>(h));
    });
    m.def("test_cast_redispatch", [](const char *s) { return std::string(s); });

    m.def("test_set", []() {
        nb::set s;
        s.add("123");
        s.add(123);
        return s;
    });

    m.def("test_set_contains", [](nb::set s, nb::handle h) { return s.contains(h); });

    m.def("test_frozenset", []() {
        return nb::frozenset(nb::make_tuple("123", 123));
    });

    m.def("test_frozenset_contains", [](nb::frozenset s, nb::handle h) {
        return s.contains(h);
    });

    m.def("test_memoryview", []() { return nb::memoryview(nb::bytes("123456")); });
    m.def("test_bad_memview", []() { return nb::memoryview(nb::int_(0)); });

    m.def("test_del_list", [](nb::list l) { nb::del(l[2]); });
    m.def("test_del_dict", [](nb::dict l) { nb::del(l["a"]); });

    static int imut = 10;
    static const int iconst = 100;
    m.def("test_ptr_return", []() { return std::make_pair(&imut, &iconst); });

    // These are caught at compile time, uncomment and rebuild to verify:

    // No nb::arg annotations:
    //m.def("bad_args1", [](nb::args, int) {});

    // kw_only in wrong place (1):
    //m.def("bad_args2", [](nb::args, int) {}, nb::kw_only(), "args"_a, "i"_a);

    // kw_only in wrong place (2):
    //m.def("bad_args3", [](nb::args, int) {}, "args"_a, "i"_a, nb::kw_only());

    // kw_only in wrong place (3):
    //m.def("bad_args4", [](int, nb::kwargs) {}, "i"_a, "kwargs"_a, nb::kw_only());

    // kw_only specified twice:
    //m.def("bad_args5", [](int, int) {}, nb::kw_only(), "i"_a, nb::kw_only(), "j"_a);

    // Wrong number of nb::arg annotations. A parameter that supplies an
    // implicit annotation (here 'std::nullptr_t') does not relax the count,
    // which must be zero or one per parameter:
    //m.def("bad_args6", [](std::nullptr_t) {}, "i"_a, "j"_a);
    //m.def("bad_args7", [](std::nullptr_t, int, int) {}, "i"_a);

    // No nb::arg annotations, as in 'bad_args1'. An implicit annotation has no
    // name and cannot make the keyword-only parameter usable:
    //m.def("bad_args8", [](nb::args, std::nullptr_t) {});

    m.def("test_args_kwonly",
          [](int i, double j, nb::args args, int z) {
              return nb::make_tuple(i, j, args, z);
          }, "i"_a, "j"_a, "args"_a, "z"_a);
    m.def("test_args_kwonly_kwargs",
          [](int i, double j, nb::args args, int z, nb::kwargs kwargs) {
              return nb::make_tuple(i, j, args, z, kwargs);
          }, "i"_a, "j"_a, "args"_a, nb::kw_only(), "z"_a, "kwargs"_a);
    m.def("test_kwonly_kwargs",
          [](int i, double j, nb::kwargs kwargs) {
              return nb::make_tuple(i, j, kwargs);
          }, "i"_a, nb::kw_only(), "j"_a, "kwargs"_a);

    m.def("test_kw_only_all",
          [](int i, int j) { return nb::make_tuple(i, j); },
          nb::kw_only(), "i"_a, "j"_a);
    m.def("test_kw_only_some",
          [](int i, int j, int k) { return nb::make_tuple(i, j, k); },
          nb::arg(), nb::kw_only(), "j"_a, "k"_a);
    m.def("test_kw_only_with_defaults",
          [](int i, int j, int k, int z) { return nb::make_tuple(i, j, k, z); },
          nb::arg() = 3, "j"_a = 4, nb::kw_only(), "k"_a = 5, "z"_a);
    m.def("test_kw_only_mixed",
          [](int i, int j) { return nb::make_tuple(i, j); },
          "i"_a, nb::kw_only(), "j"_a);

    struct kw_only_methods {
        kw_only_methods(int _v) : v(_v) {}
        int v;
    };

    nb::class_<kw_only_methods>(m, "kw_only_methods")
        .def(nb::init<int>(), nb::kw_only(), "v"_a)
        .def_rw("v", &kw_only_methods::v)
        .def("method_2k",
             [](kw_only_methods&, int i, int j) { return nb::make_tuple(i, j); },
             nb::kw_only(), "i"_a = 1, "j"_a = 2)
        .def("method_1p1k",
             [](kw_only_methods&, int i, int j) { return nb::make_tuple(i, j); },
             "i"_a = 1, nb::kw_only(), "j"_a = 2);

    m.def("test_any", [](nb::any a) { return a; } );

    m.def("test_wrappers_list", []{
        nb::list l1, l2;
        l1.append(1);
        l2.append(2);
        l1.extend(l2);

        bool b = nb::len(l1) == 2 && nb::len(l2) == 1 &&
            l1[0].equal(nb::int_(1)) && l1[1].equal(nb::int_(2));

        l1.clear();
        return b && nb::len(l1) == 0;
    });

    m.def("test_wrappers_dict", []{
        nb::dict d1, d2;
        d1["a"] = 1;
        d2["b"] = 2;
        d1.update(d2);

        bool b = nb::len(d1) == 2 && nb::len(d2) == 1 &&
            d1["a"].equal(nb::int_(1)) &&
            d1["b"].equal(nb::int_(2));

        d1.clear();
        return b && nb::len(d1) == 0;
    });

    m.def("test_wrappers_set", []{
        nb::set s;
        s.add("a");
        s.add("b");

        bool b = nb::len(s) == 2 && s.contains("a") && s.contains("b");

        b &= s.discard("a");
        b &= !s.discard("q");

        b &= !s.contains("a") && s.contains("b");
        s.clear();
        b &= s.size() == 0;

        return b;
    });

    m.def("hash_it", [](nb::handle h) { return nb::hash(h); });
    m.def("isinstance_", [](nb::handle inst, nb::handle cls) {
        return nb::isinstance(inst, cls);
    });

    // Test bytearray type
    m.def("test_bytearray_new",     []() { return nb::bytearray(); });
    m.def("test_bytearray_new",     [](const char *c, int size) { return nb::bytearray(c, (size_t) size); });
    m.def("test_bytearray_copy",    [](nb::bytearray o) { return nb::bytearray(o.c_str(), o.size()); });
    m.def("test_bytearray_c_str",   [](nb::bytearray o) -> const char * { return o.c_str(); });
    m.def("test_bytearray_size",    [](nb::bytearray o) { return o.size(); });
    m.def("test_bytearray_resize",  [](nb::bytearray c, int size) { return c.resize((size_t) size);
    });

    // Test call_policy feature
    m.def("test_call_policy",
          [](const char* s, numeric_string n) -> const char* {
              if (0 == strcmp(s, "returnfail")) {
                  return "not utf8 \xff";
              }
              if (n.number > strlen(s)) {
                  throw std::runtime_error("offset too large");
              }
              return s + n.number;
          },
          nb::call_policy<example_policy>());

    m.def("call_policy_record",
          []() {
              auto ret = std::move(example_policy::calls);
              return ret;
          });

    m.def("abi_tag", [](){ return NB_PLATFORM_ABI_TAG; });

    // Test the nb::fallback type
    m.def("test_fallback_1", [](double){ return 0; });
    m.def("test_fallback_1", [](nb::handle){ return 1; });
    m.def("test_fallback_2", [](double) { return 0; });
    m.def("test_fallback_2", [](nb::fallback){ return 1; });

    m.def("test_get_dict_default", [](nb::dict l) { return l.get("key", nb::int_(123)); });
    m.def("test_get_dict_default_2", [](nb::dict l, nb::handle key) { return l.get(key, nb::int_(123)); });
    m.def("test_getitem_dict", [](nb::dict l, nb::handle key) -> nb::object { return l[key]; });

    m.def("test_accessor_inplace_attr", [](nb::object o, nb::object v) { o.attr("x") += v; });
    m.def("test_accessor_inplace_item", [](nb::object o, nb::object v) { o["x"] += v; });
}
