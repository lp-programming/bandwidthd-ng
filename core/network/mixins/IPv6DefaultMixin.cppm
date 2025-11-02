module;
export module IPv6DefaultMixin;
import net_integer;
import HeaderView;
import Statistics;
import IPData;
import Config;

import <netinet/tcp.h>;
import <netinet/ip6.h>;
import <net/ethernet.h>;
import <unordered_map>;

using namespace types::net_integer;

namespace bandwidthd {
  export
  template<typename BD>
  class IPv6DefaultMixin {
      using StatField = Statistics::StatField;
  public:
    std::unordered_map<net_u128, IPData> ips6{};
    std::unordered_map<std::pair<net_u128,net_u128>, Statistics> txrx6{};
  public:  
    void ProcessIPv6(this auto& self, const uint16_t length, const ip6_hdr& iheader, HeaderView& view) {
      auto src = static_cast<net_u128>(iheader.ip6_src);
      auto dst = static_cast<net_u128>(iheader.ip6_dst);

      auto [src_allowed, dst_allowed] = self._is_allowed(src, dst);
      bool txrx_allowed{false};
      for (const auto& sn: self.config.txrxsubnets) {
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

            subproto = Statistics::GetSubProto(static_cast<net_u16>(tcp.th_sport));
            if (subproto == nullptr) {
              subproto = Statistics::GetSubProto(static_cast<net_u16>(tcp.th_dport));
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
        auto& rec = self.ips6[src].Sent;
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
        auto& rec = self.ips6[dst].Received;
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
        auto& rec = self.txrx6[{src,dst}];
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
}
