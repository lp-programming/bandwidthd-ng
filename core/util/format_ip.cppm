module;
export module format_ip;

import <format>;
import <sys/types.h>;
import <stdint.h>;
import <sys/socket.h>;

import net_integer;
import uint128_t;

using util::net_integer::NetIntegerIP;

namespace util {
  using net_integer::net_u128;
  using net_integer::net_u32;

  export
  inline std::string format_ipv4(const NetIntegerIP auto nip) {
    uint32_t ip = static_cast<uint32_t>(nip.host());
    return std::format("{}.{}.{}.{}",
                       (ip >> 24) & 0xFF,
                       (ip >> 16) & 0xFF,
                       (ip >> 8) & 0xFF,
                       ip & 0xFF);
  }

  export
  inline std::string format_ipv6(const net_u128 nip) {
    uint128_t srcip = nip.host();
    return std::format("{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}",
                       srcip >> (128 - 16),
                       srcip >> (128 - 16 * 2) & 0xffff,
                       srcip >> (128 - 16 * 3) & 0xffff,
                       srcip >> (128 - 16 * 4) & 0xffff,
                       srcip >> (128 - 16 * 5) & 0xffff,
                       srcip >> (128 - 16 * 6) & 0xffff,
                       srcip >> (128 - 16 * 7) & 0xffff,
                       srcip & 0xffff
                       );
  }

  export
  inline std::string format_ip(const net_u128 ip, const int family) {
    if (family == AF_INET) {
      return format_ipv4(ip);
    }
    else {
      return format_ipv6(ip);
    }
  }

  export
  inline std::string format_ip(const net_u32 ip) {
    return format_ipv4(ip);
  }

  export
  inline std::string format_ip(const net_u128 ip) {
    return format_ipv6(ip);
  }
}
