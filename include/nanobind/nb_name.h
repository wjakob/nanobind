NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<std::size_t size>
struct CArray {
    char name[size+1];
};

struct StringView {
    const char* data;
    std::size_t size;

    [[nodiscard]] constexpr char front() const {
        return data[0];
    }

    [[nodiscard]] constexpr char back() const {
        return data[size-1];
    }
};

constexpr NB_INLINE StringView pretty_name(StringView name, bool remove_args = false) noexcept {
    if (remove_args) {
        auto i = name.size;
        while (i > 0 && name.data[--i] != ')');

        for (std::size_t countOfParenthesis{}; ; --i) {
            if (auto v = name.data[i]; v == ')') {
                ++countOfParenthesis;
            } else if (v == '(' && --countOfParenthesis == 0) {
                break;
            }
        }
        while (i > 0 && name.data[i] == ' ') {
            --i;
        }
        name.size = i;
    } else if (name.back() == ')')
        --name.size;

    if (name.back() == '>') {
        auto i = name.size - 1;

        for (std::size_t countOfAngleBracket{}; ; --i) {
            if (auto v = name.data[i]; v == '>') {
                ++countOfAngleBracket;
            } else if (v == '<' && --countOfAngleBracket == 0) {
                break;
            }
        }
        while (i > 0 && name.data[i] == ' ') {
            --i;
        }

        name.size = i;
    }

    for (std::size_t i = name.size; i > 0; --i) {
        if (!((name.data[i - 1] >= '0' && name.data[i - 1] <= '9') ||
              (name.data[i - 1] >= 'a' && name.data[i - 1] <= 'z') ||
              (name.data[i - 1] >= 'A' && name.data[i - 1] <= 'Z') ||
              (name.data[i - 1] == '_'))) {
            name.data += i;
            name.size -= i;
            break;
        }
    }

    if (name.size > 0 && ((name.front() >= 'a' && name.front() <= 'z') ||
                          (name.front() >= 'A' && name.front() <= 'Z') ||
                          (name.front() == '_'))) {
        return name;
    }

    return {}; // Invalid name.
}

template<std::size_t ...Ix>
constexpr NB_INLINE auto to_array(StringView sv, std::index_sequence<Ix...>) {
    return CArray<sizeof...(Ix)>{sv.data[Ix]..., '\0'};
}

#if defined(__clang__) || defined(__GNUC__)
#   define NB_NAME_GETTER constexpr auto name = pretty_name({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2})
#elif defined(_MSC_VER)
#   define NB_NAME_GETTER constexpr auto name = pretty_name({__FUNCSIG__, sizeof(__FUNCSIG__) - 17}, \
        std::is_member_function_pointer_v<T> || std::is_function_v<T>)
#else
#   define NB_NAME_GETTER constexpr auto name = StringView{}
#endif

template<typename T>
constexpr NB_INLINE auto get_name_impl() noexcept {
    NB_NAME_GETTER;
    return to_array(name, std::make_index_sequence<name.size>{});
}

template<auto V>
constexpr NB_INLINE auto get_name_impl() noexcept {
    using T = std::remove_pointer_t<std::decay_t<decltype(V)>>;
    NB_NAME_GETTER;
    return to_array(name, std::make_index_sequence<name.size>{});
}


template<typename T>
constexpr inline auto get_name_v = get_name_impl<T>();

template<auto T>
constexpr inline auto get_name_var_v = get_name_impl<T>();


template<typename T>
constexpr NB_INLINE const char* get_name() noexcept {
    static_assert(get_name_v<T>.name[0] != '\0', "Cannot deduce class name. Please add it manually.");
    return get_name_v<T>.name;
}

template<auto T>
constexpr NB_INLINE const char* get_name() noexcept {
    static_assert(get_name_var_v<T>.name[0] != '\0', "Cannot deduce member/function name. Please add it manually.");
    return get_name_var_v<T>.name;
}

#undef NB_NAME_GETTER

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
