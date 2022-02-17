NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <bool... Bs> struct index_1;
template <bool... Bs> struct index_n;

template <> struct index_1<> { constexpr static size_t value = 0; };
template <> struct index_n<> { constexpr static size_t value = 0; };

template <bool B, bool... Bs> struct index_1<B, Bs...> {
    constexpr static size_t value_rec = index_1<Bs...>::value;
    constexpr static size_t value = B ? 0 : (value_rec + 1);
};

template <bool B, bool... Bs> struct index_n<B, Bs...> {
    constexpr static size_t value_rec = index_n<Bs...>::value;
    constexpr static size_t value =
        (value_rec < sizeof...(Bs) || !B) ? (value_rec + 1) : 0;
};

template <bool... Bs> constexpr size_t index_1_v = index_1<Bs...>::value;
template <bool... Bs> constexpr size_t index_n_v = index_n<Bs...>::value;

/// Helper template to strip away type modifiers
template <typename T> struct intrinsic_type                       { using type = T; };
template <typename T> struct intrinsic_type<const T>              { using type = typename intrinsic_type<T>::type; };
template <typename T> struct intrinsic_type<T*>                   { using type = typename intrinsic_type<T>::type; };
template <typename T> struct intrinsic_type<T&>                   { using type = typename intrinsic_type<T>::type; };
template <typename T> struct intrinsic_type<T&&>                  { using type = typename intrinsic_type<T>::type; };
template <typename T, size_t N> struct intrinsic_type<const T[N]> { using type = typename intrinsic_type<T>::type; };
template <typename T, size_t N> struct intrinsic_type<T[N]>       { using type = typename intrinsic_type<T>::type; };
template <typename T> using intrinsic_t = typename intrinsic_type<T>::type;

template <typename T>
constexpr bool is_std_char_v =
    std::is_same_v<T, char>
#if defined(NB_HAS_U8STRING)
    std::is_same<CharT, char8_t>, /* std::u8string */
#endif
    || std::is_same_v<T, char16_t> ||
    std::is_same_v<T, char32_t> || std::is_same_v<T, wchar_t>;

template <bool V> using enable_if_t = std::enable_if_t<V, int>;

/// Check if a function is a lambda function
template <typename T>
constexpr bool is_lambda_v = !std::is_function_v<T> && !std::is_pointer_v<T> &&
                             !std::is_member_pointer_v<T>;

/// Inspect the signature of a method call
template <typename T> struct analyze_method { };
template <typename Cls, typename Ret, typename... Args>
struct analyze_method<Ret (Cls::*)(Args...)> {
    using func = Ret(Args...);
    static constexpr size_t argc = sizeof...(Args);
};

template <typename Cls, typename Ret, typename... Args>
struct analyze_method<Ret (Cls::*)(Args...) const> {
    using func = Ret(Args...);
    static constexpr size_t argc = sizeof...(Args);
};

template <typename T>
using forward_t = std::conditional_t<std::is_lvalue_reference_v<T>, T, T &&>;

template <typename...> inline constexpr bool false_v = false;

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
