module;
export module Constants;
import <chrono>;
import <net/ethernet.h>;

import net_integer;

using namespace types::net_integer;

using namespace std::chrono_literals;


export constexpr std::chrono::seconds MINUTE{60s};
export constexpr std::chrono::seconds HOUR{60s * 60};
export constexpr std::chrono::seconds CLASSIC_INTERVAL{200s};
export constexpr std::chrono::seconds DAY{60 * 60 * 24s};
export constexpr std::chrono::seconds WEEK{DAY * 7};
export constexpr std::chrono::seconds MONTH{DAY * 30};
export constexpr std::chrono::seconds YEAR{DAY * 365};


export constexpr net_u16 ETHER_VLAN{net_u16::from_host(ETHERTYPE_VLAN)};
export constexpr net_u16 ETHER_IPV4{net_u16::from_host(ETHERTYPE_IP)};
export constexpr net_u16 ETHER_IPV6{net_u16::from_host(ETHERTYPE_IPV6)};


