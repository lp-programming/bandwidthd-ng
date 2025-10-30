module;

export module BandwidthD;
export import Types;
export import HeaderView;
export import Syslog;
export import <iostream>;
export import <pcap/pcap.h>;
export import <poll.h>;
export import <netinet/ip.h>;
export import <netinet/tcp.h>;
export import <netinet/ip6.h>;
export import <net/ethernet.h>;
export import <format>;
export import <string>;
export import <concepts>;
export import <unordered_map>;

using StatField = uint64_t Statistics::*;
export using Logger = Syslog::Logger;

namespace util {
  inline StatField GetSubProto(const net_u16 port) {
    switch(port.host()) {
    case 110:
    case 25:
    case 143:
    case 587:
      return &Statistics::mail;
    case 80:
    case 443:
      return &Statistics::http;
    case 20:
    case 21:
      return &Statistics::ftp;
    case 1044:	// Direct File Express
    case 1045:	// ''  <- Dito Marks
    case 1214:	// Grokster, Kaza, Morpheus
    case 4661:	// EDonkey 2000
    case 4662:	// ''
    case 4665:	// ''
    case 5190:	// Song Spy
    case 5500:	// Hotline Connect
    case 5501:	// ''
    case 5502:	// ''
    case 5503:	// ''
    case 6346:	// Gnutella Engine
    case 6347:	// ''
    case 6666:	// Yoink
    case 6667:	// ''
    case 7788:	// Budy Share
    case 8888:	// AudioGnome, OpenNap, Swaptor
    case 8889:	// AudioGnome, OpenNap
    case 28864: // hotComm
    case 28865: // hotComm
      return &Statistics::p2p;
    }
    return nullptr;
  }
  template <int Family, NetIntegerIP N>
  inline std::pair<bool, bool> _is_allowed(const N srcip, const N dstip, const Config& cfg) {
    bool src_ok{false};
    bool dst_ok{false};
    
    for (const auto& s : cfg.subnets) {
      src_ok |= (s.family == Family) && s.contains(srcip);
      dst_ok |= (s.family == Family) && s.contains(dstip);
      if (src_ok && dst_ok) {
        break;
      }
    }

    if (src_ok || dst_ok) {
      for (const auto& s : cfg.notsubnets) {
        src_ok &= !(s.family == Family && s.contains(srcip));
        dst_ok &= !(s.family == Family && s.contains(dstip));
        if (!(src_ok || dst_ok)) {
          break;
        }
      }
    }
    return {src_ok, dst_ok};
  }
}


using EnumFlags::FlagSet;
export enum class ModeFlag {
  IPv4Default = 1,
  IPv6Default = 2,
  IPv4 = 4,
  IPv6 = 8
};

export struct Modes : FlagSet<ModeFlag> {
  using ModeFlags = FlagSet<ModeFlag>;
  constexpr Modes(ModeFlags F): ModeFlags(F) {}

  static constexpr ModeFlags IPv4 = ModeFlags{ModeFlag::IPv4};

  static constexpr ModeFlags IPv6        = ModeFlags{ModeFlag::IPv6};
  static constexpr ModeFlags IPv4Default = ModeFlags{ModeFlag::IPv4Default};
  static constexpr ModeFlags IPv6Default = ModeFlags{ModeFlag::IPv6Default};
  static constexpr ModeFlags Both        = IPv4 | IPv6;
  static constexpr ModeFlags BothDefault = IPv4Default | IPv6Default;
  static constexpr ModeFlags Any = Both | BothDefault;
  static constexpr ModeFlags NeedsIPv4 = IPv4 | IPv4Default;
  static constexpr ModeFlags NeedsIPv6 = IPv6 | IPv6Default;


};



template<Modes m>
struct EmptyMixin {};

template<typename BD>
class IPv6DefaultMixin {
public:
  std::unordered_map<net_u128, IPData> ips6{};
  std::unordered_map<std::pair<net_u128,net_u128>, Statistics> txrx6{};
public:  
  void ProcessIPv6(const uint16_t length, const ip6_hdr& iheader, HeaderView& view) {
    const Config& cfg = static_cast<const BD&>(*this).config;
    auto src = static_cast<net_u128>(iheader.ip6_src);
    auto dst = static_cast<net_u128>(iheader.ip6_dst);

    auto [src_allowed, dst_allowed] = util::_is_allowed<AF_INET6>(src, dst, cfg);
    bool txrx_allowed{false};
    for (const auto& sn: cfg.txrxsubnets) {
      if (sn.contains(src) || sn.contains(dst)) {
        txrx_allowed = true;
        break;
      }
    }
    
    if (!src_allowed && !dst_allowed && !txrx_allowed) {
      return;
    }
    StatField proto = nullptr;
    StatField subproto = nullptr;
    uint8_t next = iheader.ip6_ctlun.ip6_un1.ip6_un1_nxt;
    view.skip_forward(sizeof(ip6_hdr));
    while (true) {
      switch (next) {
      default:
        {
          const ip6_ext& ext = view.peek_header<ip6_ext>();
          next = ext.ip6e_nxt;
          view.skip_forward((ext.ip6e_len + 1) * 8);
          continue;
        }
      case IPPROTO_ICMPV6:
        {
          proto = &Statistics::icmp;
          break;
        }
      case IPPROTO_TCP:
        {
          proto = &Statistics::tcp;
          const auto& tcp = view.next_header<tcphdr>();

          subproto = util::GetSubProto(static_cast<net_u16>(tcp.th_sport));
          if (subproto == nullptr) {
            subproto = util::GetSubProto(static_cast<net_u16>(tcp.th_dport));
          }
          break;
        }
      case IPPROTO_UDP:
        {

          proto = &Statistics::udp;
          break;
        }
      }
      break;
    }
    if (src_allowed) {
      auto& rec = ips6[src].Sent;
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }

    if (dst_allowed) {
      auto& rec = ips6[dst].Received;
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }

    if (txrx_allowed) {
      auto& rec = txrx6[{src,dst}];
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }
  }
};

template<typename BD>
class IPv4DefaultMixin {
public:
  std::unordered_map<net_u32, IPData> ips{};
  std::unordered_map<std::pair<net_u32,net_u32>, Statistics> txrx{};
  void ProcessIPv4(const uint16_t length, const ip& iheader, HeaderView& view) {
    const Config& cfg = static_cast<const BD&>(*this).config;
    const net_u32 src = static_cast<net_u32>(iheader.ip_src.s_addr);
    const net_u32 dst = static_cast<net_u32>(iheader.ip_dst.s_addr);

    auto [src_allowed, dst_allowed] = util::_is_allowed<AF_INET>(src, dst, cfg);
    bool txrx_allowed{false};
    for (const auto& sn: cfg.txrxsubnets) {
      if (sn.contains(src) || sn.contains(dst)) {
        txrx_allowed = true;
        break;
      }
    }
    
    if (!src_allowed && !dst_allowed && !txrx_allowed) {
      return;
    }
    StatField proto = nullptr;
    StatField subproto = nullptr;
    switch (iheader.ip_p) {
    case IPPROTO_ICMP:
      proto = &Statistics::icmp;
      break;
    case IPPROTO_TCP:
      {
        proto = &Statistics::tcp;
        view.skip_forward(iheader.ip_hl * 4);
        const tcphdr& tcp = view.next_header<tcphdr>();
        
        subproto = util::GetSubProto(static_cast<net_u16>(tcp.th_sport));
        if (subproto == nullptr) {
          subproto = util::GetSubProto(static_cast<net_u16>(tcp.th_dport));
        }
      }
      break;
    case IPPROTO_UDP:
      proto = &Statistics::udp;
      break;      
    }

    if (src_allowed) {
      auto& rec = ips[src].Sent;
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }

    if (dst_allowed) {
      auto& rec = ips[dst].Received;
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }

    if (txrx_allowed) {
      auto& rec = txrx[{src,dst}];
      rec.packet_count++;
      rec.total += length;
      if (proto != nullptr) {
        rec.*proto += length;
        if (subproto != nullptr) {
          rec.*subproto += length;
        }
      }
    }
  }
};

template <typename BD, Modes M>
using IPv4Mixin = std::conditional_t<
  M & Modes::IPv4Default,
  IPv4DefaultMixin<BD>,
  EmptyMixin<Modes::IPv4>
  >;

template <typename BD, Modes M>
using IPv6Mixin = std::conditional_t<
  M & Modes::IPv6Default,
  IPv6DefaultMixin<BD>,
  EmptyMixin<Modes::IPv6>
  >;

template <typename T>
concept HasIPv4Processor = requires {
    { &T::ProcessIPv4 } -> std::same_as<
      void (T::*)(const uint16_t, const ip&, HeaderView&)
    >;
};


template <typename T>
concept HasIPv6Processor = requires {
    { &T::ProcessIPv6 } -> std::same_as<
      void (T::*)(const uint16_t, const ip6_hdr&, HeaderView&)
    >;
};



template <typename Derived>
concept HasOwnIPv4Processor =
requires {
  requires &Derived::ProcessIPv4 != &IPv4DefaultMixin<Derived>::ProcessIPv4;
};


template <typename Derived>
concept HasOwnIPv6Processor =
requires {
  requires &Derived::ProcessIPv6 != &IPv6DefaultMixin<Derived>::ProcessIPv6;
};


template <typename BD, Modes M>
concept SaneMode = (M & Modes::Any)
  && (!(M & Modes::IPv4) || HasIPv4Processor<BD>)
  && (!(M & Modes::IPv6) || HasIPv6Processor<BD>)
  && (!(M & Modes::IPv4 && M & Modes::IPv4Default))
  && (!(M & Modes::IPv6 && M & Modes::IPv6Default))
  ;



template <typename T, Modes M>
requires SaneMode<T, M>
struct SanityChecker {
  constexpr static bool Valid() {
    if constexpr (M & Modes::IPv4Default) {
      static_assert(!HasOwnIPv4Processor<T>, "Cannot supply ProcessIPv4 in derived class when selecting IPv4Default");
    }
    if constexpr (M & Modes::IPv6Default) {
      static_assert(!HasOwnIPv6Processor<T>, "Cannot supply ProcessIPv6 in derived class when selecting IPv6Default");
    }
    return true;
  }
};



export
template <typename BD, Modes M = {Modes::IPv4Default}>
class Sensor : public IPv4Mixin<BD,M>, public IPv6Mixin<BD,M> {
public:
  Config config;
  bpf_program fcode;
  pcap_t* pc = nullptr;
  int selectable_fd{-1};
  std::string errbuf = std::string(PCAP_ERRBUF_SIZE, 0);
  size_t IP_Offset{0};
  BD& self;
  Sensor(Config& config): IPv4Mixin<BD,M>(), IPv6Mixin<BD,M>(), config(config), self(static_cast<BD&>(*this)) {
    static_assert(SanityChecker<BD,M>::Valid(), "Missing method or inconsistent mode selection");
    Logger::init(config.syslog_prefix, Syslog::Option::PID, Syslog::Facility::DAEMON);
    Logger::info(std::format("Opening {}", config.dev));
    pcap_init(PCAP_CHAR_ENC_UTF_8, nullptr);
    pc = pcap_create(config.dev.c_str(), errbuf.data());
    pcap_set_promisc(pc, config.promisc);
    pcap_set_snaplen(pc, config.snaplen);
    pcap_setnonblock(pc, true, errbuf.data());
    if (pcap_activate(pc)) {
      auto e = pcap_geterr(pc);
      Logger::err(errbuf);
      throw std::runtime_error{
        e
      };
    }
    if (pcap_compile(pc, &fcode, config.filter.c_str(), 1, 0) < 0) {
      auto e = pcap_geterr(pc);
      Logger::err(errbuf);
      throw std::runtime_error{
        e
      };
    }
    if (pcap_setfilter(pc, &fcode) < 0) {
      auto e = pcap_geterr(pc);
      Logger::err(errbuf);
      Logger::err("Malformed libpcap filter string");

      throw std::runtime_error{
        e
      };
    }
    auto DataLink = pcap_datalink(pc);
    switch (DataLink) {
    default:
      Logger::info("Unkown datalink type, defaulting to ethernet");
    case DLT_EN10MB:
      Logger::info("Packet Encoding: Ethernet");
      IP_Offset = 14; //IP_Offset = sizeof(struct ether_header);
      break;
#ifdef DLT_LINUX_SLL 
    case DLT_LINUX_SLL:
      IP_Offset = 16;
      break;
#endif
#ifdef DLT_RAW
    case DLT_RAW:
      IP_Offset = 0;
      break;
#endif
    case DLT_IEEE802:
      Logger::info("Untested packet encoding: Token Ring");
      IP_Offset = 22;
      break;
    }
    selectable_fd = pcap_get_selectable_fd(pc);
  }

  ~Sensor() {
    pcap_close(pc);
  }




  auto Poll() {
    pollfd pfd {
      selectable_fd,
      POLLIN,
      0x00
    };
    return poll(&pfd, 1, 1000);
  }

  auto IPV4Received(const pcap_pkthdr &, const ip& iheader, HeaderView& view ) {
    self.ProcessIPv4(static_cast<net_u16>(iheader.ip_len).host(), iheader, view);
  }

  auto IPV6Received(const pcap_pkthdr &, const ip6_hdr& iheader, HeaderView& view ) {
    self.ProcessIPv6(static_cast<net_u16>(iheader.ip6_ctlun.ip6_un1.ip6_un1_plen).host(), iheader, view);
  }

  auto PacketCallback(const pcap_pkthdr &header, const std::string_view packet) {
    HeaderView view{packet};
    const auto& vlanhdr = view.peek_header<VlanHeader>();
    if (vlanhdr.ether_type[0] == 0x81 && vlanhdr.ether_type[1] == 0x00) {
      view.skip_forward(4);
    }

    view.skip_forward(IP_Offset);
    
    
    
    if constexpr (M & Modes::NeedsIPv4) {
      const auto& iheader = view.peek_header<ip>();
      if (iheader.ip_v == 4) {
        IPV4Received(header, iheader, view);
        return;
      }
    }
    if constexpr (M & Modes::NeedsIPv6) {
      const auto& i6header = view.peek_header<ip6_hdr>();
      if (i6header.ip6_ctlun.ip6_un2_vfc>>4 == 6) {
        IPV6Received(header, i6header, view);
        return;
      }
    }
  }

  static std::string hex_encode(const auto m, const auto bytes) {
    std::string r{};
    for (size_t i = 0; i < m; ++i) {
      r+=std::format("{:02x}", bytes[i]);
    }
    return r;
  }
  auto Step() {
    auto r = pcap_dispatch
      (
       pc, -1,
       [](u_char *user, const pcap_pkthdr *header, const u_char *bytes)
       {
         const std::string_view packet{reinterpret_cast<const char*>(bytes), header->caplen};
         if (header->caplen < 28) {
           std::println("discarding small packet fragment: {} bytes", header->caplen);
           std::println("hex: {}", hex_encode(header->caplen, bytes));
           return;
         }
         BD& self = *reinterpret_cast<BD*>(user);
         try {
           return self.PacketCallback(*header, packet);
         }
         catch(std::out_of_range& e) {
           std::println("got packet of {} bytes, which contained malformed headers", header->caplen);
           std::println("hex: {}", hex_encode(header->caplen, bytes));
         
         }
       }, reinterpret_cast<u_char*>(this)
       );
    return r;
  }



};
