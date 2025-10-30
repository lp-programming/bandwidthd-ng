module;
export module static_for;
import <cstddef>;
import <utility>;

export
template <typename F, std::size_t... Is>
constexpr void static_for(std::index_sequence<Is...>, F&& f) {
    (f(std::integral_constant<std::size_t, Is>{}), ...);
}

export
template <std::size_t N, typename F>
constexpr void static_for(F&& f) {
    static_for(std::make_index_sequence<N>{}, std::forward<F>(f));
}
