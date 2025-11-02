export module net_integer;

import <bit>;
import <compare>;
import <cstdint>;
import <string_view>;
import <string>;
import <array>;
import <charconv>;
import <stdexcept>;
import <format>;
import <functional>;
import <cstring>;
import <netinet/in.h>;
import <limits>;

import uint128_t;

using std::uint16_t;
using std::uint32_t;

namespace types::net_integer {
  export
  template <typename T>
  concept NetIntegerIP = requires {
    typename T::value_type;
  } && (std::is_same_v<typename T::value_type, uint32_t> ||
        std::is_same_v<typename T::value_type, uint128_t>);
  
  template <typename T>
  concept NetIntSupported = requires {
    requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t> || std::same_as<T, uint128_t>;
  };

  template <NetIntSupported T>
  consteval bool needs_swap() {
    return std::endian::native != std::endian::big;
  }

  template <typename T>
  constexpr T bswap(T value);

  template <>
  constexpr uint16_t bswap(uint16_t val) {
    return static_cast<uint16_t>((val << 8) | (val >> 8));
  }

  template <>
  constexpr uint32_t bswap(uint32_t val) {
    return (val << 24) |
      ((val << 8) & 0x00FF0000) |
      ((val >> 8) & 0x0000FF00) |
      (val >> 24);
  }

  template <>
  constexpr uint64_t bswap(uint64_t val) {
    return  (val << 56) |
      ((val << 40) & 0x00FF000000000000ULL) |
      ((val << 24) & 0x0000FF0000000000ULL) |
      ((val << 8)  & 0x000000FF00000000ULL) |
      ((val >> 8)  & 0x00000000FF000000ULL) |
      ((val >> 24) & 0x0000000000FF0000ULL) |
      ((val >> 40) & 0x000000000000FF00ULL) |
      (val >> 56);
  }

  template <>
  constexpr uint128_t bswap(uint128_t val) {
    uint64_t hi = static_cast<uint64_t>(val >> 64);
    uint64_t lo = static_cast<uint64_t>(val);
    uint64_t swapped_hi = bswap(lo);
    uint64_t swapped_lo = bswap(hi);
    return (static_cast<uint128_t>(swapped_hi) << 64) | swapped_lo;
  }

  export template <NetIntSupported T>
  struct net_integer {
  private:
    T value{}; // stored in network byte order

    // Private constructor that takes raw network order
    constexpr net_integer(T raw, std::true_type /* tag */) : value(raw) {}

  public:
    using value_type = T;

    // Default constructor
    constexpr net_integer() noexcept = default;

    // Constructor takes a network-order value (no swapping)
    constexpr net_integer(T network_order_value) : value(network_order_value) {}

    template <typename U = T>
    constexpr explicit net_integer(const in6_addr& addr)
      requires(std::is_same_v<U, __uint128_t>)
      : value(std::bit_cast<__uint128_t>(addr)) {}

    // Copy/move
    constexpr net_integer(const net_integer&) = default;
    constexpr net_integer& operator=(const net_integer&) = default;

    // Static factory: construct from host-order value (with swapping)
    static constexpr net_integer from_host(T host_value) {
      return net_integer(needs_swap<T>() ? bswap(host_value) : host_value, std::true_type{});
    }

    // Access raw (network-order) and host-order representations
    constexpr T net() const { return value; }
    constexpr T host() const { return needs_swap<T>() ? bswap(value) : value; }

    // No implicit cast to T — explicit use of .host() or .net()

    // Comparisons
    constexpr auto operator<=>(const net_integer&) const = default;
    constexpr bool operator==(const net_integer&) const = default;

    // Bitwise operators (on host values)
    constexpr net_integer operator&(const net_integer& rhs) const {
      return from_host(this->host() & rhs.host());
    }
    constexpr net_integer operator|(const net_integer& rhs) const {
      return from_host(this->host() | rhs.host());
    }
    constexpr net_integer operator^(const net_integer& rhs) const {
      return from_host(this->host() ^ rhs.host());
    }
    constexpr net_integer operator~() const {
      return from_host(~this->host());
    }

    // Shifts (on host values)
    constexpr net_integer operator<<(int n) const {
      return from_host(this->host() << n);
    }
    constexpr net_integer operator>>(int n) const {
      return from_host(this->host() >> n);
    }

    // Hash support
    friend struct std::hash<net_integer<T>>;
  };

  // Aliases
  export using net_u16 = net_integer<uint16_t>;
  export using net_u32 = net_integer<uint32_t>;
  export using net_u128 = net_integer<uint128_t>;

  constexpr uint8_t hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') {
      return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
      return static_cast<uint8_t>(10 + (c - 'a'));
    }
    if (c >= 'A' && c <= 'F') {
      return static_cast<uint8_t>(10 + (c - 'A'));
    }
    throw std::invalid_argument(std::format("Invalid hex digit: {}", c));
  }

  constexpr uint16_t parse_hextet(std::string_view sv) {
    if (sv.empty() || sv.size() > 4) {
      throw std::invalid_argument("Invalid hextet length");
    }
    uint16_t val = 0;
    for (char c : sv) {
      val = (val << 4) + hex_char_to_val(c);
    }
    return val;
  }


  // IPv4 string literal
  export constexpr net_u32 parse_ipv4(std::string_view s) {
    std::array<uint8_t, 4> bytes{};
    size_t start = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      auto end = s.find('.', start);
      if (end == std::string_view::npos && i < 3)
        throw std::invalid_argument("Invalid IPv4 format");
      std::string_view part = s.substr(start, end - start);
      int val = 0;
      auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), val);
      if (ec != std::errc() || val < 0 || val > 255)
        throw std::invalid_argument("Invalid IPv4 byte");
      bytes[i] = static_cast<uint8_t>(val);
      start = end + 1;
    }

    // Build value in **host order** — LSB = last byte
    uint32_t host_order_val =
      static_cast<uint32_t>(bytes[3]) << 0  |
      static_cast<uint32_t>(bytes[2]) << 8  |
      static_cast<uint32_t>(bytes[1]) << 16 |
      static_cast<uint32_t>(bytes[0]) << 24;

    return net_u32::from_host(host_order_val);
  }

  export constexpr net_u128 parse_ipv6(std::string_view input) {
    std::array<uint16_t, 8> segments{};
    size_t seg_index = 0;
    std::optional<size_t> insert_zeros_at;

    while (!input.empty()) {
      // Handle "::"
      if (input.starts_with("::")) {
        if (insert_zeros_at) {
          throw std::invalid_argument("Multiple '::' in IPv6 address");
        }
        insert_zeros_at = seg_index;
        input.remove_prefix(2);
        if (input.empty()) {
          break; // trailing "::"
        }
        continue;
      }

      // Find next ':' or end of input
      size_t next_colon = input.find(':');
      std::string_view hextet;

      if (next_colon == std::string_view::npos) {
        hextet = input;
        input = {};
      }
      else {
        hextet = input.substr(0, next_colon);
        input.remove_prefix(next_colon + 1);
      }

      if (seg_index >= 8) {
        throw std::invalid_argument("Too many segments in IPv6 address");
      }

      segments[seg_index++] = parse_hextet(hextet);
    }

    // Fill missing segments if "::" was used
    if (insert_zeros_at) {
      const size_t zeros_to_insert = 8 - seg_index;
      for (size_t i = seg_index; i-- > *insert_zeros_at; )
        segments[i + zeros_to_insert] = segments[i];
      for (size_t i = 0; i < zeros_to_insert; ++i)
        segments[*insert_zeros_at + i] = 0;
    }
    else if (seg_index != 8) {
      throw std::invalid_argument("Incorrect number of IPv6 segments");
    }
  

    // Combine into a 128-bit value
    __uint128_t value = 0;
    for (size_t i = 0; i < 8; ++i)
      value = (value << 16) | segments[i];

    return net_u128::from_host(value);

  }
}
namespace types::net_integer::literal {
  // Numeric literal
  export consteval net_u128 operator""_n(unsigned long long v) {
    return net_u128(static_cast<uint128_t>(v));
  }

  export consteval net_u32 operator""_ipv4(const char* s, size_t n) {
    return parse_ipv4({s, n});
  }

  export consteval net_u128 operator""_ipv6(const char* s, size_t n) {
    return parse_ipv6({s, n});
  }
}

using types::net_integer::NetIntSupported;
using types::net_integer::net_integer;

export
namespace std {

  // Hash support
  template <NetIntSupported T>
  struct hash<net_integer<T>> {

    constexpr hash<net_integer<T>>() noexcept = default;
    
    size_t operator()(const net_integer<T>& k) const noexcept {
      return hash<T>{}(k.net());
    }
  };

  template <NetIntSupported T>
  struct hash<pair<net_integer<T>, net_integer<T>>> {

    constexpr hash<net_integer<T>,net_integer<T>>() noexcept = default;
    
    size_t operator()(const pair<net_integer<T>, net_integer<T>>& k) const noexcept {
      return hash<T>{}((k.first.net()&std::numeric_limits<int32_t>::max()) << 32 + k.second.net()&std::numeric_limits<int32_t>::max() );
    }
  };

}
