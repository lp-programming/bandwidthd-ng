module;

export module util;
import <string>;
import <vector>;
import <numeric>;
import <sstream>;
import <format>; 
export import <sys/types.h>;
export import <stdint.h>;
export import <sys/socket.h>;

export import net_integer;


export typedef __uint128_t uint128_t ;


export
namespace util {
  using net_integer::net_u128;
  using net_integer::net_u32;

  inline std::vector<std::string> string_split(const std::string_view& s, const char sep) {
    std::vector<std::string> out{};
    
    std::istringstream stream(s);

    std::string token;

    while (std::getline(stream, token, sep)) {
        out.push_back(token);
    }
    return out;
  }
  inline std::string string_join(const std::string& sep, const std::vector<std::string> &out) {
    return std::accumulate(
        std::next(out.begin()),
        out.end(),
        out[0],
        [sep](std::string a, std::string b) {
          return a + sep + b;
        });
  }

  inline std::string format_ipv4(const net_u32 nip) {
    uint32_t ip = nip.host();
    return std::format("{}.{}.{}.{}",
                       (ip >> 24) & 0xFF,
                       (ip >> 16) & 0xFF,
                       (ip >> 8) & 0xFF,
                       ip & 0xFF);
  }

  inline std::string format_ipv4(const net_u128 nip) {
    uint32_t ip = nip.host();
    return std::format("{}.{}.{}.{}",
                       (ip >> 24) & 0xFF,
                       (ip >> 16) & 0xFF,
                       (ip >> 8) & 0xFF,
                       ip & 0xFF);
  }


  inline std::string format_ipv6(const net_u128 nip) {
    auto srcip = nip.host();
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

  inline std::string format_ip(const net_u128 ip, const int family) {
    if (family == AF_INET) {
      return format_ipv4(ip);
    }
    else {
      return format_ipv6(ip);
    }
  }
  inline std::string format_ip(const net_u32 ip) {
    return format_ipv4(ip);
  }
  inline std::string format_ip(const net_u128 ip) {
    return format_ipv6(ip);
  }
         
  template<typename I>
  inline std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) { // Not particularly clean, but efficiently uses SIMD
    static const char *digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
      rc[i] = digits[(w >> j) & 0x0f];
    return rc;
  }

} // namespace util
