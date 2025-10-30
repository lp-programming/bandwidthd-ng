module;
export module Memory;
export import Database;
export import Types;
export import net_integer;
import <map>;
import <filesystem>;
import <chrono>;
import <fstream>;
import <algorithm>;

constexpr auto FLAGS = std::ios::binary | std::ios::out | std::ios::trunc;

export using util::net_integer::net_u128;
export using util::net_integer::net_u16;
export using util::net_integer::net_u32;

struct SummaryPair {
  SummaryData tx{};
  SummaryData rx{};
};

namespace std {
  export template<> struct tuple_size<SummaryPair> : std::integral_constant<size_t, 2> {};
  export template<> struct tuple_element<0, SummaryPair> { using type = SummaryData; };
  export template<> struct tuple_element<1, SummaryPair> { using type = SummaryData; };

  export template <std::size_t I>
  constexpr SummaryData& get(SummaryPair& p) noexcept {
    if constexpr (I == 0) {
      std::println("geting tx");
      return p.tx;
    }
    else if constexpr (I == 1) {
      std::println("geting rx");
      return p.rx;
    }
  }
  
  export template <std::size_t I>
  constexpr const SummaryData& get(const SummaryPair& p) noexcept {
    if constexpr (I == 0) {
      std::println("geting tx");
      return p.tx;
    }
    else if constexpr (I == 1) {
      std::println("geting rx");
      return p.rx;
    }
  }
}

using std::get;

export
class Memory  {
  const Config& cfg;
  std::map<SubnetIdentifier, std::vector<SummaryPair>> subnets{};
  std::map<net_u32, std::vector<SummaryPair>> ipv4{};
  std::map<net_u128, std::vector<SummaryPair>> ipv6{};
  std::map<SubnetIdentifier, SummaryPair> frames{};
  
public:
  Memory(const Config& c) : cfg(c) {}

  void graph(auto& grapher) {
    const auto now = std::chrono::system_clock::now();
    for (auto& [sn, sd] : subnets) {
      auto svgname = std::format("{}_{}.svg", util::format_ip(sn.ip, sn.family), util::format_ip(sn.mask, sn.family));
      std::println("considering printing to {}", svgname);
      uint64_t total{0};
      for (const auto& sp: sd) {
        total += sp.rx.total + sp.tx.total;
        if (total >= cfg.graph_cutoff) {
          break;
        }
      }
      if (total < cfg.graph_cutoff) {
        std::println("nope, not printing because {} is less than {}", total, cfg.graph_cutoff);
        continue;
      }
      std::vector<SummaryPair> sd_copy = sd;
      graph(grapher, now, svgname, sd_copy);
    }
    return;
    for (auto& [ip, sd] : ipv4) {
      auto svgname = std::format("{}.svg", util::format_ipv4(ip));
      std::println("considering printing to {}", svgname);
      uint64_t total{0};
      for (auto& sp: sd) {
        total += sp.rx.total + sp.tx.total;
        if (total >= cfg.graph_cutoff) {
          break;
        }
      }
      if (total < cfg.graph_cutoff) {
        std::println("nope, not printing because {} is less than {}", total, cfg.graph_cutoff);
        continue;
      }
      std::vector<SummaryPair> sd_copy = sd;
      graph(grapher, now, svgname, sd_copy);
    }

    for (auto& [ip, sd] : ipv6) {
      auto svgname = std::format("{}.svg", util::format_ipv6(ip));
      std::println("considering printing to {}", svgname);
      uint64_t total{0};
      for (auto& sp: sd) {
        total += sp.rx.total + sp.tx.total;
        if (total >= cfg.graph_cutoff) {
          break;
        }
      }
      if (total < cfg.graph_cutoff) {
        std::println("nope, not printing because {} is less than {}", total, cfg.graph_cutoff);
        continue;
      }
      std::vector<SummaryPair> sd_copy = sd;
      graph(grapher, now, svgname, sd_copy);
    }
  }
private:
  void graph(auto& grapher, auto& now, auto svgname, std::vector<SummaryPair>& sp) {
    std::filesystem::path htdocs { cfg.htdocs_dir };
    const auto hour = now - std::chrono::hours{1};
    const auto day = now - std::chrono::days{1};
    const auto week = now - std::chrono::weeks{1};
    const auto month = now - std::chrono::months{1};
    const auto year = now - std::chrono::years{1};
      
    std::string svg;

    struct GraphSpec {
      GraphInterval flag;
      std::string_view folder;
      decltype(hour) period;
    };
    const std::array graph_specs {
      GraphSpec{GraphInterval::Classic,  "classic",  now - CLASSIC_INTERVAL},
      GraphSpec{GraphInterval::Hour,  "hourly",  hour},
      GraphSpec{GraphInterval::Day,   "daily",   day},
      GraphSpec{GraphInterval::Week,  "weekly",  week},
      GraphSpec{GraphInterval::Month, "monthly", month},
      GraphSpec{GraphInterval::Year,  "yearly",  year},
    };
    for (const auto& [flag, folder, period] : graph_specs) {
      if (!(cfg.graph_intervals & flag)) {
        continue;
      }

      auto tx_dir = (htdocs / "graphs" / folder / "tx");
      auto rx_dir = (htdocs / "graphs" / folder / "rx");
      if (!std::filesystem::exists(tx_dir)) {
        std::filesystem::create_directories(tx_dir);
      }
      if (!std::filesystem::exists(rx_dir)) {
        std::filesystem::create_directories(rx_dir);
      }

      

      std::ofstream tx_op{htdocs / "graphs" / folder / "tx" / svgname, FLAGS};
      std::ofstream rx_op{htdocs / "graphs" / folder / "rx" / svgname, FLAGS};
      auto [tx_svg, rx_svg] = grapher.render(sp, period, now);
      tx_op << tx_svg;
      rx_op << rx_svg;
      
    }
     
  }

  std::vector<SummaryPair>& getPair(const net_u32 ip) {
    return ipv4[ip];
  }
  std::vector<SummaryPair>& getPair(const net_u128 ip) {
    return ipv6[ip];
  }

  /*  void Cleanup(auto now) {
    std::chrono::time_point target = now - YEAR;
    if (cfg.graph_intervals & GraphInterval::Year) {
      target = now - YEAR;
    }
    else if (cfg.graph_intervals & GraphInterval::Month) {
      target = now - MONTH;
    }
    else if (cfg.graph_intervals & GraphInterval::Week) {
      target = now - WEEK;
    }
    else if (cfg.graph_intervals & GraphInterval::Day) {
      target = now - DAY;
    }
    else if (cfg.graph_intervals & GraphInterval::Hour) {
      target = now - MINUTE ;
    }
    else if (cfg.graph_intervals & GraphInterval::Classic) {
      target = now - CLASSIC_INTERVAL;
    }

    for (auto& [sni, sp] : subnets) {
      std::erase_if(sp.tx, [target](const auto& r){return r.timestamp < target;});
      std::erase_if(sp.rx, [target](const auto& r){return r.timestamp < target;});

    }
    for (auto& [ip4, sp] : ipv4) {
      std::erase_if(sp.tx, [target](const auto& r){return r.timestamp < target;});
      std::erase_if(sp.rx, [target](const auto& r){return r.timestamp < target;});

    }
    for (auto& [ip6, sp] : ipv6) {
      std::erase_if(sp.tx, [target](const auto& r){return r.timestamp < target;});
      std::erase_if(sp.rx, [target](const auto& r){return r.timestamp < target;});

    }

      
    }*/
public:

  std::string make_cdf() {
    std::vector<std::string> frags{};
    for (auto& [sni, sp] : subnets) {
      std::println("subnet: {}, rows: {}", util::format_ip(sni.ip), sp.size());
      for (SummaryPair& entry : sp) {
        auto timestamp = duration_cast<std::chrono::seconds>(entry.tx.timestamp.time_since_epoch()).count();
        auto duration = duration_cast<std::chrono::seconds>(entry.tx.sample_duration).count();

        frags.push_back(std::format
                        ("{},{},{},{}",
                         util::format_ip(sni.ip, sni.family),
                         util::format_ip(sni.mask, sni.family),
                         timestamp,
                         duration));
        frags.push_back(std::format
                        (",{},{},{},{},{},{},{},{},{}",
                         entry.tx.total,
                         entry.tx.icmp,
                         entry.tx.udp,
                         entry.tx.tcp,
                         entry.tx.ftp,
                         entry.tx.http,
                         entry.tx.mail,
                         entry.tx.p2p,
                         entry.tx.count));
        frags.push_back(std::format
                        (",{},{},{},{},{},{},{},{},{}\n",
                         entry.tx.total,
                         entry.tx.icmp,
                         entry.tx.udp,
                         entry.tx.tcp,
                         entry.tx.ftp,
                         entry.tx.http,
                         entry.tx.mail,
                         entry.tx.p2p,
                         entry.tx.count));
      }
    }
    return util::string_join("", frags);

  }
  
  void CollateData(const auto& IPs, const auto&, const std::chrono::seconds duration) {
    const auto now = std::chrono::system_clock::now();

    //    Cleanup(now);
    
    frames.clear();

    for (const auto& sn : cfg.subnets) {
      std::println("still listening on subnet {}/{}", util::format_ip(sn.ip, sn.family), util::format_ip(sn.mask, sn.family));
    }

    std::println("IPs: {}", IPs.size());

    for (const auto& [ip, ipData] : IPs) {
      std::vector<SummaryPair>& sps = getPair(ip);
      SummaryPair& pair = sps.emplace_back();
      SummaryData& txf = pair.tx;
      SummaryData& rxf = pair.rx;
      txf += ipData.Sent;
      rxf += ipData.Received;
      txf.sample_duration = duration;
      rxf.sample_duration = duration;
      txf.timestamp = now;
      rxf.timestamp = now;
      for (const auto& sn : cfg.subnets) {
        std::println("{} vs {}/{}", util::format_ip(ipData.ip), util::format_ip(sn.ip), util::format_ip(sn.mask));
        if (sn.contains(ip)) {
          std::println("in a sn");
          SummaryPair& sp = frames[sn];
          sp.tx += ipData.Sent;
          sp.rx += ipData.Received;
        }
      }
    }
    for (auto& [sn, tx_rx]: frames) {
      auto& [tx, rx] = tx_rx;
      auto& sp = subnets[sn].emplace_back(SummaryPair{});
      
      tx.timestamp = now;
      tx.sample_duration = duration;
      rx.timestamp = now;
      rx.sample_duration = duration;
      sp.tx = tx;
      sp.rx = rx;
    }
  }
};
