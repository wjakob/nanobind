NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

// Tiny self-contained tuple to avoid having to import 1000s of LOC from <tuple>
template <typename... Ts> struct nb_tuple;
template <> struct nb_tuple<> {
    template <size_t> using type = void;
};

template <typename T, typename... Ts> struct nb_tuple<T, Ts...> : nb_tuple<Ts...> {
    using Base = nb_tuple<Ts...>;

    nb_tuple() = default;
    nb_tuple(const nb_tuple &) = default;
    nb_tuple(nb_tuple &&) = default;
    nb_tuple& operator=(nb_tuple &&) = default;
    nb_tuple& operator=(const nb_tuple &) = default;

    NB_INLINE nb_tuple(const T& value, const Ts&... ts)
        : Base(ts...), value(value) { }

    NB_INLINE nb_tuple(T&& value, Ts&&... ts)
        : Base(std::move(ts)...), value(std::move(value)) { }

    template <size_t I> NB_INLINE auto& get() {
        if constexpr (I == 0)
            return value;
        else
            return Base::template get<I - 1>();
    }

    template <size_t I> NB_INLINE const auto& get() const {
        if constexpr (I == 0)
            return value;
        else
            return Base::template get<I - 1>();
    }

    template <size_t I>
    using type =
        std::conditional_t<I == 0, T, typename Base::template type<I - 1>>;

private:
    T value;
};

template <typename... Ts> nb_tuple(Ts &&...) -> nb_tuple<std::decay_t<Ts>...>;

NAMESPACE_END(NB_NAMESPACE)
NAMESPACE_END(detail)
