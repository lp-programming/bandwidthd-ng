module;
export module EnumFlags;
import <initializer_list>;
import <type_traits>;


namespace EnumFlags {
  export template<typename F>
  class FlagSet {
    static_assert(std::is_enum_v<F>, "FlagSet requires enum type");
    using underlying = std::underlying_type_t<F>;
    //typedef typename std::underlying_type<F>::type underlying;
    public:
    F flags;
    constexpr FlagSet(const FlagSet<F> &other): flags(other.flags) {
      
    }

    constexpr FlagSet& operator=(const FlagSet& other) noexcept = default;
    constexpr FlagSet& operator=(FlagSet&& other) noexcept = default;

    
    constexpr FlagSet(F f) : flags(f) {}
    constexpr FlagSet() : flags(static_cast<F>(0)) {}
    constexpr inline FlagSet<F> const operator|(F other) const {
      
      return FlagSet<F>(static_cast<F>(static_cast<underlying>(flags) | static_cast<underlying>(other)));
    }
    constexpr inline FlagSet<F> operator&(F other) const {
      return FlagSet<F>(static_cast<F>(static_cast<underlying>(flags) & static_cast<underlying>(other)));
    }
    constexpr inline FlagSet<F> operator|(const FlagSet<F> other) const {
      return (*this) | other.flags;
    }
    constexpr inline bool contains(const F other) const {
      return static_cast<bool>(static_cast<underlying>(flags) & static_cast<underlying>(other));
    }
    constexpr inline bool contains(const FlagSet<F>& other) const {
      return static_cast<bool>(static_cast<underlying>(flags) & static_cast<underlying>(other.flags));
    }
    constexpr inline FlagSet<F> operator&(FlagSet<F> other) const {
      return (*this) & other.flags;
    }

    constexpr inline bool operator*() const {
      return static_cast<bool>(*this);
    }

    constexpr inline operator bool() const {
      return static_cast<bool>(this->flags);
    }
  };

} // namespace EnumFlags
