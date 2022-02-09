#include <nanobind/nanobind.h>
#include "buffer.h"

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

Buffer buf(128);

static const char *error_fetch(PyObject **type, PyObject **value,
                               PyObject **trace) noexcept {
    PyErr_Fetch(type, value, trace);

    buf.clear();
    if (*type) {
        object name = handle(*type).attr("__name__");
        buf.put_dstr(borrow<str>(name).c_str());
        buf.put(": ");
    }

    if (*value)
        buf.put_dstr(str(*value).c_str());

    if (*trace) {
        PyTracebackObject *to = (PyTracebackObject *) *trace;

        // Get the deepest trace possible
        while (to->tb_next)
            to = to->tb_next;

        PyFrameObject *frame = to->tb_frame;
        buf.put("\n\nAt:\n");
        while (frame) {
#if PY_VERSION_HEX >= 0x03090000
            PyCodeObject *f_code = PyFrame_GetCode(frame);
#else
            PyCodeObject *f_code = frame->f_code;
            Py_INCREF(f_code);
#endif
            buf.put_dstr(borrow<str>(f_code->co_filename).c_str());
            buf.put('(');
            buf.put_uint32(PyFrame_GetLineNumber(frame));
            buf.put("): ");
            buf.put_dstr(borrow<str>(f_code->co_name).c_str());
            buf.put('\n');

            frame = frame->f_back;
            Py_DECREF(f_code);
        }
    }

    return buf.get();
}

NAMESPACE_END(detail)

python_error::python_error()
    : std::runtime_error(detail::error_fetch(
          &m_type.m_ptr, &m_value.m_ptr, &m_trace.m_ptr)) { }

python_error::python_error(const python_error &e) : std::runtime_error(e) {
    m_type = e.m_type;
    m_value = e.m_value;
    m_trace = e.m_trace;
}

python_error::python_error(python_error &&e)
    : std::runtime_error(std::move(e)) {
    m_type = std::move(e.m_type);
    m_value = std::move(e.m_value);
    m_trace = std::move(e.m_trace);
}

python_error::~python_error() { }

void python_error::restore() {
    PyErr_Restore(m_type.release().ptr(), m_value.release().ptr(),
                  m_trace.release().ptr());
}

NAMESPACE_END(nanobind)
