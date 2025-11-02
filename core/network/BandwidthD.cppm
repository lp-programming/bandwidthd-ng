module;

export module BandwidthD;
export import HeaderView;
export import Syslog;
export import net_integer;
export import Statistics;
export import Config;
export import Parser;
export import EnumFlags;
export import IPData;
export import Constants;
export import VlanHeader;
import IPv4DefaultMixin;
import IPv6DefaultMixin;


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
export import <vector>;

export using namespace types::net_integer;


export using Logger = Syslog::Logger;

namespace bandwidthd {

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
    pcap_t* pc = nullptr;
    int selectable_fd{-1};
    int DataLink{};
    std::string errbuf = std::string(PCAP_ERRBUF_SIZE, 0);
    bpf_program fcode;
  public:
    Config config;
    Sensor(Config& config): IPv4Mixin<BD,M>(), IPv6Mixin<BD,M>(), config(config) {
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
      DataLink = pcap_datalink(pc);
      switch (DataLink) {
      default:
        Logger::info("Unkown datalink type, defaulting to ethernet");
      case DLT_EN10MB:
        Logger::info("Packet Encoding: Ethernet");
        break;
#ifdef DLT_LINUX_SLL 
      case DLT_LINUX_SLL:
        break;
#endif
#ifdef DLT_RAW
      case DLT_RAW:
        break;
#endif
      case DLT_IEEE802:
        Logger::info("Untested packet encoding: Token Ring");
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

    auto IPV4Received(this auto& self, const pcap_pkthdr &, const ip& iheader, HeaderView& view ) {
      static_cast<BD&>(self).ProcessIPv4(static_cast<net_u16>(iheader.ip_len).host(), iheader, view);
    }

    auto IPV6Received(this auto& self, const pcap_pkthdr &, const ip6_hdr& iheader, HeaderView& view ) {
      static_cast<BD&>(self).ProcessIPv6(static_cast<net_u16>(iheader.ip6_ctlun.ip6_un1.ip6_un1_plen).host(), iheader, view);
    }

    auto PacketCallback(const pcap_pkthdr &header, const std::string_view packet) {
      HeaderView view{packet};
      const auto& ethhdr = view.next_header<ether_header>();

      net_u16 ether_type{ethhdr.ether_type};

      while (ether_type == ETHER_VLAN) {
        const auto& vlanhdr = view.next_header<VlanHeader>();
        ether_type = net_u16{vlanhdr.encapsulated_proto};
      }

      if (ether_type == ETHER_IPV4) {
        if constexpr (M & Modes::NeedsIPv4) {
          const auto& iheader = view.next_header<ip>();
          IPV4Received(header, iheader, view);
          return;
        }
      }
      else if (ether_type == ETHER_IPV6) {
        if constexpr (M & Modes::NeedsIPv6) {
          const auto& i6header = view.peek_header<ip6_hdr>();
          if (i6header.ip6_ctlun.ip6_un2_vfc>>4 == 6) {
            IPV6Received(header, i6header, view);
            return;
          }
        }
      }
    }

    static std::string hex_encode(const auto m, const auto bytes) {
      std::string r{};
      for (size_t i = 0; i < m; ++i) {
        r += std::format("{:02x}", bytes[i]);
      }
      return r;
    }
    int Step() {
      int r = pcap_dispatch
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
    template<NetIntegerIP N>
    [[nodiscard]]
    inline std::pair<bool, bool> _is_allowed(this const auto& self, const N srcip, const N dstip) noexcept{
      bool src_ok{false};
      bool dst_ok{false};
    
      for (const auto& s : self.config.subnets) {
        src_ok |= s.contains(srcip);
        dst_ok |= s.contains(dstip);
        if (src_ok && dst_ok) {
          break;
        }
      }

      if (src_ok || dst_ok) {
        for (const auto& s : self.config.notsubnets) {
          src_ok &= !(s.contains(srcip));
          dst_ok &= !(s.contains(dstip));
          if (!(src_ok || dst_ok)) {
            break;
          }
        }
      }
      return {src_ok, dst_ok};
    }

  };
}
