NAMESPACE_BEGIN(NB_NAMESPACE)

struct gil_scoped_acquire {
public:
    gil_scoped_acquire() : state (PyGILState_Ensure()) { }
    ~gil_scoped_acquire() { PyGILState_Release(state); }

private:
    const PyGILState_STATE state;
};

NAMESPACE_END(NB_NAMESPACE)
