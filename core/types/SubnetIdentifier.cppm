export module SubnetIdentifier;
import <charconv>;
import <string_view>;
import <netinet/ip.h>;
import <system_error>;

import net_integer;
import uint128_t;

using namespace lpprogramming;

using types::net_integer::net_u128;
using types::net_integer::net_u16;
using types::net_integer::net_u32;

using types::net_integer::parse_ipv4;
using types::net_integer::parse_ipv6;

using types::net_integer::NetIntegerIP;

export struct SubnetIdentifier {
  net_u128 ip;
  net_u128 mask;
  int family;

  constexpr auto operator<=>(const SubnetIdentifier& rhs) const {
    return (rhs.ip <=> ip);
  }
  constexpr bool operator==(const SubnetIdentifier& rhs) const {
    return (rhs.ip == ip && rhs.mask == mask && rhs.family == family);
  }

  inline constexpr bool contains(const net_u32 i) const {
    return family == AF_INET && contains(widen_ipv4(i));
  }
  
  inline constexpr bool contains(const net_u128 i) const {
    return (i & mask) == (ip & mask);
  }
  
  constexpr SubnetIdentifier() = default;
  constexpr SubnetIdentifier(const SubnetIdentifier&) = default;
  constexpr SubnetIdentifier(SubnetIdentifier&&) = default;
  constexpr SubnetIdentifier& operator=(const SubnetIdentifier&) = default;
  constexpr SubnetIdentifier& operator=(SubnetIdentifier&&) = default;

  constexpr SubnetIdentifier(net_u32 i, net_u32 m, int f) : ip(widen_ipv4(i)), mask(widen_ipv4(m)), family(f) {}
  constexpr SubnetIdentifier(net_u128 i, net_u128 m, int f) : ip(i), mask(m), family(f) {}

  constexpr SubnetIdentifier(const std::string_view line) : SubnetIdentifier(parse_sv(line)) {}


  constexpr static SubnetIdentifier parse_sv(std::string_view line) {
    if (line.find(':') != std::string_view::npos) {
      return parse_ipv6_sv(line);
    }
    return parse_ipv4_sv(line);
  }
  constexpr static net_u128 widen_ipv4(const net_u32 ip) {
    return net_u128::from_host(static_cast<uint128_t>(ip.host()));
  }
private:
  constexpr static net_u128 make_mask(const int bits) {
    if (bits < 0 || bits > 128) {
      throw std::invalid_argument("Invalid IPv6 CIDR mask bits");
    }
    uint128_t mask = bits == 0 ? 0 : (~uint128_t(0) << (128 - bits));
    return net_u128::from_host(mask);
  }
  constexpr static net_u32 make_mask32(const int bits) {
    if (bits < 0 || bits > 32) {
      throw std::invalid_argument("Invalid IPv4 CIDR mask bits");
    }
    uint32_t mask = bits == 0 ? 0 : ~((1u << (32 - bits)) - 1);
    return net_u32::from_host(mask);
  }
  constexpr static SubnetIdentifier parse_ipv4_sv(std::string_view input) {
    size_t slash = input.find('/');
    if (slash == std::string_view::npos) {
      size_t space = input.find(' ');
      if (space == std::string_view::npos) {
        return SubnetIdentifier(parse_ipv4(input), make_mask32(32), AF_INET);
      }
      return SubnetIdentifier(parse_ipv4(input.substr(0, space)), parse_ipv4(input.substr(space + 1)), AF_INET);
    }
    auto ip_part = input.substr(0, slash);
    auto mask_part = input.substr(slash + 1);
    int bits = 0;
    auto [ptr, ec] = std::from_chars(mask_part.data(), mask_part.data() + mask_part.size(), bits);
    if (ec != std::errc{} || bits < 0 || bits > 32) {
      throw std::invalid_argument("Invalid IPv4 mask length");
    }
    net_u32 ip = parse_ipv4(ip_part);
    net_u32 mask = make_mask32(bits);
    return SubnetIdentifier{widen_ipv4(ip), widen_ipv4(mask), AF_INET};
  }
  
  constexpr static SubnetIdentifier parse_ipv6_sv(std::string_view input) {
    size_t slash = input.find('/');
    if (slash == std::string_view::npos) {
      size_t space = input.find(' ');
      if (space == std::string_view::npos) {
        return SubnetIdentifier(parse_ipv6(input), make_mask(128), AF_INET6);
      }
      return SubnetIdentifier(parse_ipv6(input.substr(0, space)), parse_ipv6(input.substr(space + 1)), AF_INET6);
    }
    auto ip_part = input.substr(0, slash);
    auto mask_part = input.substr(slash + 1);
    int bits = 0;
    auto [ptr, ec] = std::from_chars(mask_part.data(), mask_part.data() + mask_part.size(), bits);
    if (ec != std::errc{} || bits < 0 || bits > 128) {
      throw std::invalid_argument("Invalid IPv6 mask length");
    }
    net_u128 ip = parse_ipv6(ip_part);
    net_u128 mask = make_mask(bits);
    return SubnetIdentifier{ip, mask, AF_INET6};
  }
};
