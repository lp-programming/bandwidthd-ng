module;
export module Types;

import <sys/types.h>;
import <string>;
import <errno.h>;
import <sys/time.h>;
import <stdint.h>;
import <chrono>;
export import <vector>;
import <map>;
import <sys/socket.h>;
import <charconv>;
import <print>;

export import util;

export import EnumFlags;
export import net_integer;
export import uint128_t;

export using EnumFlags::FlagSet;

export using util::net_integer::net_u128;
export using util::net_integer::net_u16;
export using util::net_integer::net_u32;

export using util::net_integer::parse_ipv4;
export using util::net_integer::parse_ipv6;

export using util::net_integer::NetIntegerIP;

using namespace std::chrono_literals;

export constexpr std::chrono::seconds MINUTE{60s};
export constexpr std::chrono::seconds HOUR{60s * 60};
export constexpr std::chrono::seconds CLASSIC_INTERVAL{200s};
export constexpr std::chrono::seconds DAY{60 * 60 * 24s};
export constexpr std::chrono::seconds WEEK{DAY * 7};
export constexpr std::chrono::seconds MONTH{DAY * 30};
export constexpr std::chrono::seconds YEAR{DAY * 365};


export enum class GraphInterval {
  None = 0,
  Classic = 1,
  Minute = 2, 
  Hour = 4, 
  Day = 8,
  Week = 16,
  Month = 32,
  Year = 64
};

export struct GraphIntervals : FlagSet<GraphInterval> {
  using Intervals = FlagSet<GraphInterval>;
  constexpr GraphIntervals(Intervals F): Intervals(F) {}
};

export constexpr GraphIntervals operator|(const GraphInterval l, const GraphInterval r) {
  return GraphIntervals{static_cast<GraphInterval>(static_cast<uint32_t>(l) | static_cast<uint32_t>(r))};
}


export class SubnetIdentifier {
public:
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

export class Config {
public:
  std::string dev{};
  std::string filter{"tcp"};
  bool promisc{true};

  uint8_t tag;

  int snaplen{65535};

  GraphIntervals graph_intervals{GraphInterval::None};

  uint32_t skip_intervals{0};
  uint64_t graph_cutoff{0};
  bool graph_individual_ips{true};

  bool output_cdf{false};
  bool recover_cdf{false};

  std::chrono::seconds interval{30s};
  
  std::chrono::seconds meta_refresh{MINUTE};
  
  std::string sqlite_connect_string{};
  std::string psql_connect_string{};
  std::string sensor_name{};
  std::string log_dir{};
  std::string htdocs_dir{};
  std::string description{};
  std::string management_url{};
  
  std::vector<SubnetIdentifier> subnets{};
  std::vector<SubnetIdentifier> notsubnets{};
  std::vector<SubnetIdentifier> txrxsubnets{};

  std::string syslog_prefix{};
};

export class Statistics {
public:
  uint64_t packet_count;
  uint64_t total;
  uint64_t icmp;
  uint64_t udp;
  uint64_t tcp;
  uint64_t ftp;
  uint64_t http;
  uint64_t mail;
  uint64_t p2p;
  std::string label{};
};

export class IPData {
public:
  std::string label{};
  net_u128 ip;
  Statistics Sent;
  Statistics Received;
};

export 
class VlanHeader {
public:
  u_int8_t  ether_dhost[6];  /* destination eth addr */
  u_int8_t  ether_shost[6];  /* source ether addr  */
  u_int8_t  ether_type[2];   /* packet type ID field */
  u_int8_t  vlan_tag[2];     /* vlan tag information */
};

export class SummaryData {
public:
  std::chrono::system_clock::time_point timestamp;
  std::chrono::seconds sample_duration;
  std::string net;
  uint64_t total{0};
  uint64_t icmp{0};
  uint64_t udp{0};
  uint64_t tcp{0};
  uint64_t ftp{0};
  uint64_t http{0};
  uint64_t mail{0};
  uint64_t p2p{0};
  uint64_t count{0};

  inline SummaryData& operator+=(const SummaryData& rhs) {
    total += rhs.total;
    icmp  += rhs.icmp;
    udp   += rhs.udp;
    tcp   += rhs.tcp;
    ftp   += rhs.ftp;
    http  += rhs.http;
    mail  += rhs.mail;
    p2p   += rhs.p2p;
    count += rhs.count;
    return *this;
  }
  inline SummaryData& operator<<(const SummaryData& rhs) {
    total  = std::max(total, rhs.total);
    icmp   = std::max(icmp , rhs.icmp );
    udp    = std::max(udp  , rhs.udp  );
    tcp    = std::max(tcp  , rhs.tcp  );
    ftp    = std::max(ftp  , rhs.ftp  );
    http   = std::max(http , rhs.http );
    mail   = std::max(mail , rhs.mail );
    p2p    = std::max(p2p  , rhs.p2p  );
    count  = std::max(count, rhs.count);
    return *this;
  }

  inline SummaryData& operator+=(const Statistics& rhs) {
    total += rhs.total;
    icmp  += rhs.icmp;
    udp   += rhs.udp;
    tcp   += rhs.tcp;
    ftp   += rhs.ftp;
    http  += rhs.http;
    mail  += rhs.mail;
    p2p   += rhs.p2p;
    count += rhs.packet_count;
    return *this;
  }


  void normalize() {
    auto sd = static_cast<uint64_t>(sample_duration.count());
    total /= sd;
    icmp /= sd;
    udp /= sd;
    tcp /= sd;
    ftp /= sd;
    http /= sd;
    mail /= sd;
    p2p /= sd;
    count /= sd;
  }

  void clear() {
    total = 0;
    icmp = 0;
    udp = 0;
    tcp = 0;
    ftp = 0;
    http = 0;
    mail = 0;
    p2p = 0;
    count = 0;
  }
};

