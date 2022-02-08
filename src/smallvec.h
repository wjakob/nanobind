#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(nanobind)
NAMESPACE_BEGIN(detail)

/* Simple vector class for default/copy/move-constructible objects.
   Storage remains on the stack when the vectors is smaller than the 'Small'
   template argument. */
template <typename T, size_t Small = 6> struct smallvec {
    smallvec() = default;

    ~smallvec() {
        if (m_capacity != Small)
            delete[] m_data;
    }

    smallvec(const smallvec &v) {
        m_size = v.m_size;

        if (m_size > Small) {
            m_data = new T[m_size];
            m_capacity = m_size;
        }

        for (size_t i = 0; i < m_size; ++i)
            m_data[i] = v.m_data[i];
    }

    smallvec(smallvec &&v) noexcept {
        m_size = v.m_size;

        if (m_size > Small) {
            m_data = v.m_data;
            m_capacity = v.m_capacity;
            v.m_data = v.m_temp;
        } else {
            for (size_t i = 0; i < m_size; ++i)
                m_data[i] = std::move(v.m_data[i]);
        }

        v.m_size = 0;
        v.m_capacity = Small;
    }

    void clear() {
        m_size = 0;
    }

    void push_back(const T &value) {
        if (m_size >= m_capacity)
            expand();
        m_data[m_size++] = value;
    }

    void push_back(T &&value) {
        if (m_size >= m_capacity)
            expand();
        m_data[m_size++] = std::move(value);
    }

    size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    T *data() { return m_data; }
    const T *data() const { return m_data; }

    void expand() {
        size_t capacity_new = m_capacity * 2;
        T *data_new = new T[capacity_new];
        for (size_t i = 0; i < m_size; ++i)
            data_new[i] = std::move(m_data[i]);
        m_data = data_new;
        m_capacity = capacity_new;
    }

    T &operator[](size_t i) { return m_data[i]; }
    const T &operator[](size_t i) const { return m_data[i]; }

protected:
    uint32_t m_size = 0;
    uint32_t m_capacity = Small;
    T *m_data = m_temp;
    T m_temp[Small];
};

NAMESPACE_END(detail)
NAMESPACE_END(nanobind)
