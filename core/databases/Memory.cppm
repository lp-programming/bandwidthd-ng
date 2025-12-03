export module Memory;
import <map>;
import <filesystem>;
import <chrono>;
import <fstream>;
import <algorithm>;
import <vector>;
import <ranges>;
import <numeric>;
import <format>;

import net_integer;
import SummaryData;
import Config;
import SubnetIdentifier;
import format_ip;
import Constants;
import string_util;

constexpr auto FLAGS = std::ios::binary | std::ios::out | std::ios::trunc;

using namespace bandwidthd;
using namespace lpprogramming;
using types::net_integer::net_u128;
using types::net_integer::net_u16;
using types::net_integer::net_u32;

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
      return p.tx;
    }
    else if constexpr (I == 1) {
      return p.rx;
    }
  }

  export template <std::size_t I>
  constexpr const SummaryData& get(const SummaryPair& p) noexcept {
    if constexpr (I == 0) {
      return p.tx;
    }
    else if constexpr (I == 1) {
      return p.rx;
    }
  }
}

using std::get;

namespace bandwidthd {
  export
  class Memory  {
    const Config& cfg;
    std::map<SubnetIdentifier, std::vector<SummaryPair>> subnets{};
    std::map<net_u32, std::vector<SummaryPair>> ipv4{};
    std::map<net_u128, std::vector<SummaryPair>> ipv6{};
    std::map<SubnetIdentifier, SummaryPair> frames{};

  public:
    Memory(const Config& c) : cfg(c) {}

    void Graph(auto& grapher) {
      const auto now = std::chrono::system_clock::now();
      for (auto& [sn, sd] : subnets) {
        auto svgname = std::format("{}_{}.svg", util::format_ip(sn.ip, sn.family), util::format_ip(sn.mask, sn.family));
        uint64_t total{0};
        for (const auto& sp: sd) {
          total += sp.rx.total + sp.tx.total;
          if (total >= cfg.graph_cutoff) {
            break;
          }
        }
        if (total < cfg.graph_cutoff) {
          continue;
        }
        std::vector<SummaryPair> sd_copy = sd;
        graph(grapher, now, svgname, sd_copy);
      }
      if (!cfg.graph_individual_ips) {
        return;
      }
      for (auto& [ip, sd] : ipv4) {
        auto svgname = std::format("{}.svg", util::format_ipv4(ip));
        uint64_t total{0};
        for (auto& sp: sd) {
          total += sp.rx.total + sp.tx.total;
          if (total >= cfg.graph_cutoff) {
            break;
          }
        }
        if (total < cfg.graph_cutoff) {
          continue;
        }
        std::vector<SummaryPair> sd_copy = sd;
        graph(grapher, now, svgname, sd_copy);
      }

      for (auto& [ip, sd] : ipv6) {
        auto svgname = std::format("{}.svg", util::format_ipv6(ip));
        uint64_t total{0};
        for (auto& sp: sd) {
          total += sp.rx.total + sp.tx.total;
          if (total >= cfg.graph_cutoff) {
            break;
          }
        }
        if (total < cfg.graph_cutoff) {
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
        auto [tx_svg, rx_svg] = grapher.Render(sp, period, now);
        tx_op << tx_svg;
        rx_op << rx_svg;
      }
    }

    constexpr inline auto get_cutoff(const auto now) const noexcept {
      if (cfg.graph_intervals & GraphInterval::Year) {
        return now - YEAR;
      }
      else if (cfg.graph_intervals & GraphInterval::Month) {
        return now - MONTH;
      }
      else if (cfg.graph_intervals & GraphInterval::Week) {
        return now - WEEK;
      }
      else if (cfg.graph_intervals & GraphInterval::Day) {
        return now - DAY;
      }
      else if (cfg.graph_intervals & GraphInterval::Hour) {
        return now - MINUTE ;
      }
      else if (cfg.graph_intervals & GraphInterval::Classic) {
        return now - CLASSIC_INTERVAL;
      }
      return now - YEAR;
    }

    void cleanup(const auto now) {
      std::chrono::time_point oldest = get_cutoff(now);

      for (auto& [sni, sp] : subnets) {
        cleanup(sp, now, oldest);
      }
      for (auto& [ip4, sp] : ipv4) {
        cleanup(sp, now, oldest);
      }
      for (auto& [ip6, sp] : ipv6) {
        cleanup(sp, now, oldest);
      }
    }

    void cleanup(auto& sp, const auto now, const auto oldest) {
      using namespace std::chrono;
      struct AggregationLevel {
        const hours min_age;
        const hours duration;
      };

      constexpr std::array aggregation_levels{
        AggregationLevel{
          .min_age=duration_cast<hours>(months{1}),
          .duration=duration_cast<hours>(days{1})
        },
        AggregationLevel{
          .min_age=duration_cast<hours>(weeks{1}),
          .duration=duration_cast<hours>(hours{1})
        }

      };

      if (sp.size() == 0) {
        return;
      }

      auto writer = sp.begin();

      // Find the first element we are keeping, then compute how many we are dropping.
      auto reader = std::ranges::find_if(sp, [&](auto const& x){
        return x.tx.timestamp >= oldest;
      });
      if (reader == sp.end()) {
        sp.clear();
        return;
      }

      // We don't use ~static_for~ here, so that we can break out more easily.
      constexpr auto N = aggregation_levels.size();
      for (std::size_t n{0}; n < N; ++n) {
        // Step 1: find the entries [reader, not_aggregated) and move them to our writer.
        // lower_bound might be faster to find the first daily entry, but it'll be slower
        // to find the first hourly entry. For simplicity, and since the worst-case number of entries is likely 300,
        // this uses linear search.
        auto not_aggregated = std::ranges::find_if(reader, sp.end(), [&](const auto& l) {
          return l.tx.sample_duration < aggregation_levels[n].duration;
        });
        // Everything in that interval gets moved to the current writer, which gets updated.
        writer = std::ranges::move(reader, not_aggregated, writer).out;
        reader = not_aggregated;
        if (reader == sp.end()) {
          // all the remaining entries are already done, and we've compacted everything, time to erase everything
          // after our writer.
          sp.erase(writer, sp.end());
          return;
        }

        // Step 2: Verify the first not-compacted entry is old enough to compact. This means its new /ending-time/ is
        // older than our min-age.
        auto new_sample_end = now - aggregation_levels[n].min_age;
        auto& to_aggregate = *not_aggregated;
        if (to_aggregate.tx.timestamp + aggregation_levels[n].duration > new_sample_end) {
          // It's too new. Leave the reader and writer where they are and move on to the next aggregation level.
          continue;
        }
        // Step 3: It's old enough to compact, so collect everything in its new duration. It might be better to ensure
        // each target is fully inside this interval, but that could create overlapping intervals. So instead we absorb
        // everything that /starts/ inside this interval. In practice, that shouldn't smudge anything left, since the
        // default interval durations line out nicely.
        auto end_of_includes = std::ranges::find_if(reader, sp.end(), [&](const auto& l) {
          return l.tx.timestamp > new_sample_end;
        });
        reader = end_of_includes;
        // Step 4: Pick our output location, derefernce it and bump the write pointer. It's okay even if this is the
        // /only/ entry, since writer won't advance again in that case. Also, we don't write to the target until later.
        auto& target_data = *(writer++);

        // Step 5: left-fold everything in the target interval. This uses a temporary just to avoid needing to check or
        // clear the target.
        target_data = std::ranges::fold_left(not_aggregated, end_of_includes, SummaryPair{},
                                             [](const auto& l, const auto& r) {
                                               return SummaryPair{l.tx + r.tx, l.rx + r.rx};
                                             });
        // Step 6: Set the timestamp and duration to the new values.
        target_data.tx.timestamp = (*not_aggregated).tx.timestamp;
        target_data.tx.sample_duration = aggregation_levels[n].duration;
        target_data.rx.timestamp = (*not_aggregated).tx.timestamp;
        target_data.rx.sample_duration = aggregation_levels[n].duration;

        // Step 7: Check if we have hit the end of the input data.
        // Otherwise we loop again or finalize.
        if (reader == sp.end()) {
          sp.erase(writer, sp.end());
          return;
        }
      }
      // Step 8: At this point, all aggregation is done. If we haven't returned early, it means there may still be data
      // after the reader, so we move it, clean up, and return
      if (reader != writer) {
        // we actually moved something
        writer = std::ranges::move(reader, sp.end(), writer).out;
        sp.erase(writer, sp.end());
      }

    }

    std::vector<SummaryPair>& get_pair(const net_u32 ip) {
      return ipv4[ip];
    }
    std::vector<SummaryPair>& get_pair(const net_u128 ip) {
      return ipv6[ip];
    }
  public:

    std::string MakeCdf() {
      std::vector<std::string> frags{};
      for (auto& [sni, sp] : subnets) {
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
      for (auto& [ip, sp] : ipv4) {
        for (SummaryPair& entry : sp) {
          auto timestamp = duration_cast<std::chrono::seconds>(entry.tx.timestamp.time_since_epoch()).count();
          auto duration = duration_cast<std::chrono::seconds>(entry.tx.sample_duration).count();

          frags.push_back(std::format
                          ("{},{},{}",
                           util::format_ipv4(ip),
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
      for (auto& [ip, sp] : ipv6) {
        for (SummaryPair& entry : sp) {
          auto timestamp = duration_cast<std::chrono::seconds>(entry.tx.timestamp.time_since_epoch()).count();
          auto duration = duration_cast<std::chrono::seconds>(entry.tx.sample_duration).count();

          frags.push_back(std::format
                          ("{},{},{}",
                           util::format_ipv6(ip),
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

      return util::string_concat(frags);

    }

    void CollateData(const auto& IPs, const auto&, const std::chrono::seconds duration) {
      const auto now = std::chrono::system_clock::now();

      cleanup(now);

      frames.clear();

      for (const auto& [ip, ipData] : IPs) {
        std::vector<SummaryPair>& sps = get_pair(ip);
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
          if (sn.contains(ip)) {
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
}
