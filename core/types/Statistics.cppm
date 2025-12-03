export module Statistics;
import <string>;
import <cstdint>;

import net_integer;

using namespace lpprogramming::types::net_integer;

namespace bandwidthd {
  export struct Statistics {
    using StatField = uint64_t Statistics::*;
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
    static inline constexpr StatField GetSubProto(const net_u16 port) noexcept {
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

  };
}
