NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

constexpr NB_INLINE std::string_view pretty_name(std::string_view name) noexcept {
    if (name.back() == ')')
        name.remove_suffix(1);

    for (std::size_t i = name.size(); i > 0; --i) {
        if (!((name[i - 1] >= '0' && name[i - 1] <= '9') ||
              (name[i - 1] >= 'a' && name[i - 1] <= 'z') ||
              (name[i - 1] >= 'A' && name[i - 1] <= 'Z') ||
              (name[i - 1] == '_'))) {
            name.remove_prefix(i);
            break;
        }
    }

    if (!name.empty() && ((name.front() >= 'a' && name.front() <= 'z') ||
                          (name.front() >= 'A' && name.front() <= 'Z') ||
                          (name.front() == '_'))) {
        return name;
    }

    return {}; // Invalid name.
}

template<std::size_t size>
struct CArray {
    char name[size+1];
};

template<std::size_t ...Ix>
constexpr NB_INLINE auto to_array(std::string_view sv, std::index_sequence<Ix...>) {
    return CArray<sizeof...(Ix)>{sv[Ix]..., '\0'};
}

#if defined(__clang__) || defined(__GNUC__)
#   define NB_NAME_GETTER constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2})
#elif defined(_MSC_VER)
#   define NB_NAME_GETTER constexpr auto name = pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17})
#else
#   define NB_NAME_GETTER constexpr auto name = string_view{}
#endif

template<typename T>
constexpr NB_INLINE auto get_name_impl() noexcept {
    NB_NAME_GETTER;
    static_assert(name.size() > 0, "Cannot deduce class name. Please use implicit name.");
    return to_array(name, std::make_index_sequence<name.size()>{});
}

template<auto T>
constexpr NB_INLINE auto get_name_impl() noexcept {
    NB_NAME_GETTER;
    static_assert(name.size() > 0, "Cannot deduce member/function name. Please use implicit name.");
    return to_array(name, std::make_index_sequence<name.size()>{});
}


template<typename T>
constexpr inline auto get_name_v = get_name_impl<T>();

template<auto T>
constexpr inline auto get_name_var_v = get_name_impl<T>();


template<typename T>
constexpr NB_INLINE const char* get_name() {
    return get_name_v<T>.name;
}

template<auto T>
constexpr NB_INLINE const char* get_name() {
    return get_name_var_v<T>.name;
}

#undef NB_NAME_GETTER

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
