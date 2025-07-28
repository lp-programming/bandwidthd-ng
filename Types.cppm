module;
export module Types;
export import Exception;
export import EnumFlags;
export import util;
export import <sys/types.h>;
export import <string>;
export import <errno.h>;
export import <sys/time.h>;
export import <stdint.h>;
export import <chrono>;
export import <vector>;
export import <map>;
export import <sys/socket.h>;
export import <charconv>;

export import net_integer;

export using uint128_t = __uint128_t;
export using util::net_integer::net_u128;
export using util::net_integer::net_u16;
export using util::net_integer::net_u32;

export using util::net_integer::parse_ipv4;
export using util::net_integer::parse_ipv6;

export using util::net_integer::NetIntegerIP;

constexpr uint128_t MAX_IPV6{static_cast<uint128_t>(__int128_t{-1})};

export constexpr uint32_t MINUTE{60};
export constexpr uint32_t DAY{60*60*24};
export constexpr uint32_t WEEK{DAY*7};
export constexpr uint32_t MONTH{DAY*30};
export constexpr uint32_t YEAR{DAY*365};

export class SubnetIdentifier {
public:
  net_u128 ip;
  net_u128 mask;
  int family;

  inline constexpr bool contains(const net_u32 i) const {
    return contains(widen_ipv4(i));
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

  constexpr SubnetIdentifier(std::string_view cidr) : SubnetIdentifier(parse_cidr(cidr)) {}

  constexpr static SubnetIdentifier parse_cidr(std::string_view cidr) {
    if (cidr.find(':') != std::string_view::npos) {
      return parse_ipv6_cidr(cidr);
    }
    return parse_ipv4_cidr(cidr);
  }
  constexpr static net_u128 widen_ipv4(const net_u32 ip) {
    return net_u128::from_host(static_cast<__uint128_t>(ip.host()));
  }
private:
  constexpr static net_u128 make_mask(const int bits) {
    if (bits < 0 || bits > 128) {
      throw std::invalid_argument("Invalid IPv6 CIDR mask bits");
    }
    uint128_t mask = bits == 0 ? 0 : (~__uint128_t(0) << (128 - bits));
    return net_u128::from_host(mask);
  }

  constexpr static net_u32 make_mask32(const int bits) {
    if (bits < 0 || bits > 32) {
      throw std::invalid_argument("Invalid IPv4 CIDR mask bits");
    }
    uint32_t mask = bits == 0 ? 0 : ~((1u << (32 - bits)) - 1);
    return net_u32::from_host(mask);
  }
  
  constexpr static SubnetIdentifier parse_ipv4_cidr(std::string_view input) {
    size_t slash = input.find('/');
    if (slash == std::string_view::npos) {
      return SubnetIdentifier(parse_ipv4(input), make_mask32(32), AF_INET);
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
  
  constexpr static SubnetIdentifier parse_ipv6_cidr(std::string_view input) {
    size_t slash = input.find('/');
    if (slash == std::string_view::npos) {
      return SubnetIdentifier(parse_ipv6(input), make_mask(128), AF_INET6);
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

  double range;
  uint8_t tag;

  int snaplen{65535};

  bool graph{false};
  uint32_t skip_intervals{0};
  uint64_t graph_cutoff{0};
  uint64_t interval{200};
  
  uint32_t meta_refresh{MINUTE};

  std::string db_connect_string{};
  std::string sensor_name{};
  std::string log_dir{};
  std::string htdocs_dir{};
  std::string description{};
  std::string management_url{};

  std::vector<SubnetIdentifier> subnets{};
  std::vector<SubnetIdentifier> notsubnets{};
  std::vector<SubnetIdentifier> txrxsubnets{};
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
  std::string label;
};

export class IPData {
public:
  std::string label;
  net_u128 ip;
  Statistics Sent;
  Statistics Received;
};


export class SummaryData {
public:
  net_u128 IP;
  bool Graphed;
  uint128_t Total;
  uint128_t TotalSent;
  uint128_t TotalReceived;
  uint128_t ICMP;
  uint128_t UDP;
  uint128_t TCP;
  uint128_t FTP;
  uint128_t HTTP;
  uint128_t MAIL;
  uint128_t P2P;
};

export 
class VlanHeader {
public:
  u_int8_t  ether_dhost[6];  /* destination eth addr */
  u_int8_t  ether_shost[6];  /* source ether addr  */
  u_int8_t  ether_type[2];   /* packet type ID field */
  u_int8_t  vlan_tag[2];     /* vlan tag information */
};

