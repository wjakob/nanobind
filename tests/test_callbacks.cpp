// This is an example of using nb::call_policy to support binding an
// object that takes non-owning callbacks. Since the callbacks can't
// directly keep a Python object alive (they're trivially copyable), we
// maintain a sideband structure to manage the lifetimes.

#include <algorithm>
#include <unordered_set>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/stl/unordered_set.h>

namespace nb = nanobind;

// The callback type accepted by the object, which we assume we can't change.
// It's trivially copyable, so it can't directly keep a Python object alive.
struct callback {
    void *context;
    void (*func)(void *context, int arg);

    void operator()(int arg) const { (*func)(context, arg); }
    bool operator==(const callback& other) const {
        return context == other.context && func == other.func;
    }
};

// An object that uses these callbacks, which we want to write bindings for
class publisher {
  public:
    void subscribe(callback cb) { cbs.push_back(cb); }
    void unsubscribe(callback cb) {
        cbs.erase(std::remove(cbs.begin(), cbs.end(), cb), cbs.end());
    }
    void emit(int arg) const { for (auto cb : cbs) cb(arg); }
  private:
    std::vector<callback> cbs;
};

template <> struct nanobind::detail::type_caster<callback> {
    static void wrap_call(void *context, int arg) {
        borrow<callable>((PyObject *) context)(arg);
    }
    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        if (!isinstance<callable>(src)) return false;
        value = {(void *) src.ptr(), &wrap_call};
        return true;
    }
    static handle from_cpp(callback cb, rv_policy policy, cleanup_list*) noexcept {
        if (cb.func == &wrap_call)
            return handle((PyObject *) cb.context).inc_ref();
        if (policy == rv_policy::none)
            return handle();
        return cpp_function(cb, policy).release();
    }
    NB_TYPE_CASTER(callback, const_name("Callable[[int], None]"))
};

nb::dict cb_registry() {
    return nb::cast<nb::dict>(
            nb::module_::import_("test_callbacks_ext").attr("registry"));
}

struct callback_data {
    struct py_hash {
        size_t operator()(const nb::object& obj) const { return nb::hash(obj); }
    };
    struct py_eq {
        bool operator()(const nb::object& a, const nb::object& b) const {
            return a.equal(b);
        }
    };
    std::unordered_set<nb::object, py_hash, py_eq> subscribers;
};

callback_data& callbacks_for(nb::handle publisher) {
    auto registry = cb_registry();
    nb::weakref key(publisher, registry.attr("__delitem__"));
    if (nb::handle value = PyDict_GetItem(registry.ptr(), key.ptr())) {
        return nb::cast<callback_data&>(value);
    }
    nb::object new_data = nb::cast(callback_data{});
    registry[key] = new_data;
    return nb::cast<callback_data&>(new_data);
}

struct cb_policy_common {
    using TwoArgs = std::integral_constant<size_t, 2>;
    static void precall(PyObject **args, TwoArgs,
                        nb::detail::cleanup_list *cleanup) {
        nb::handle self = args[0], cb = args[1];
        auto& cbs = callbacks_for(self);
        auto it = cbs.subscribers.find(nb::borrow(cb));
        if (it != cbs.subscribers.end() && !it->is(cb)) {
            // A callback is already subscribed that is
            // equal-but-not-identical to the one passed in.
            // Adjust args to refer to that one, to work around
            // the fact that the C++ object does not understand py-equality.
            args[1] = it->ptr();

            // This ensures that the normalized callback won't be
            // immediately destroyed if it's removed from the registry
            // in the unsubscribe postcall hook. Such destruction could
            // result in a use-after-free if you have other postcall hooks
            // or keep_alives that try to inspect the function args.
            // It's not strictly necessary if each arg is inspected by
            // only one call policy or keep_alive.
            cleanup->append(it->inc_ref().ptr());
        }
    }
};

struct subscribe_policy : cb_policy_common {
    static void postcall(PyObject **args, TwoArgs, nb::handle) {
        nb::handle self = args[0], cb = args[1];
        callbacks_for(self).subscribers.insert(nb::borrow(cb));
    }
};

struct unsubscribe_policy : cb_policy_common {
    static void postcall(PyObject **args, TwoArgs, nb::handle) {
        nb::handle self = args[0], cb = args[1];
        callbacks_for(self).subscribers.erase(nb::borrow(cb));
    }
};

NB_MODULE(test_callbacks_ext, m) {
    m.attr("registry") = nb::dict();
    nb::class_<callback_data>(m, "callback_data")
        .def_ro("subscribers", &callback_data::subscribers);
    nb::class_<publisher>(m, "publisher", nb::is_weak_referenceable())
        .def(nb::init<>())
        .def("subscribe", &publisher::subscribe,
             nb::call_policy<subscribe_policy>())
        .def("unsubscribe", &publisher::unsubscribe,
             nb::call_policy<unsubscribe_policy>())
        .def("emit", &publisher::emit);
}
