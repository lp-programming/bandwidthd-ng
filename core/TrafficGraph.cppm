export module TrafficGraph;
import static_for;
import SummaryData;
import string_util;

import uint128_t;
import <string>;
import <vector>;
import <unordered_map>;
import <chrono>;
import <sstream>;
import <iomanip>;
import <concepts>;
import <algorithm>;
import <format>;
import <print>;
import <numeric>;
import <array>;

static constexpr const char* COLOR_ICMP = "#FF0000";   // Red
static constexpr const char* COLOR_UDP = "#800000";    // Brown
static constexpr const char* COLOR_TCP = "#00FF00";    // Green
static constexpr const char* COLOR_P2P = "#008000";    // Dark Green
static constexpr const char* COLOR_HTTP = "#0000FF";   // Blue
static constexpr const char* COLOR_MAIL = "#800080";   // Purple
static constexpr const char* COLOR_FTP = "#FF8000";    // Orange
//static constexpr const char* COLOR_TOTAL = "#F0F0F0";  // GREY

static constexpr const char* PATH_START = "<path fill='{}' opacity='0.7' stroke-width='0' d='";

namespace bandwidthd {

  struct GraphConfig {
    const int width;
    const int height;
    const int left_margin;
    const int top_margin;
    const int bottom_margin;
    const int right_margin;
  
    const double y_padding = 1.05;
  };

  namespace {
    struct Layer {
      const std::string_view name;
      const std::string_view color;
      uint64_t SummaryData::* field;
    };
    constexpr std::array<Layer, 7> layers{
      Layer{ "icmp", COLOR_ICMP, &SummaryData::icmp },
      Layer{ "udp",  COLOR_UDP,  &SummaryData::udp  },
      Layer{ "tcp",  COLOR_TCP,  &SummaryData::tcp  },
      Layer{ "ftp",  COLOR_FTP,  &SummaryData::ftp  },
      Layer{ "http", COLOR_HTTP, &SummaryData::http },
      Layer{ "mail", COLOR_MAIL, &SummaryData::mail },
      Layer{ "p2p",  COLOR_P2P,  &SummaryData::p2p  },
    };

  }


  template <typename DATA, std::size_t N>
  constexpr const auto& get_unit(const DATA& d) {
    if constexpr (requires { get<N>(d); }) {
      return get<N>(d);
    } else {
      static_assert(N == 0, "Non-tuple data supports only index 0");
      return d;
    }
  }

  template <typename DATA, std::size_t N>
  constexpr auto& get_unit(DATA& d) {
    if constexpr (requires { get<N>(d); }) {
      return get<N>(d);
    } else {
      static_assert(N == 0, "Non-tuple data supports only index 0");
      return d;
    }
  }

  template <typename DATA>
  consteval std::size_t data_size_v() {
    if constexpr (requires { std::tuple_size<DATA>(); }) {
      return std::tuple_size_v<DATA>;
    }
    else {
      return 1;
    }
  }


  export
  class TrafficGraph {
    const GraphConfig cfg;
    double graph_height;
  public:
    TrafficGraph(const GraphConfig& c) : cfg(c), graph_height(cfg.height - cfg.top_margin - cfg.bottom_margin) {}
    template<typename DATA>
    auto render(std::vector<DATA>& data_rows,
                const std::chrono::system_clock::time_point start_time,
                const std::chrono::system_clock::time_point end_time) {
      using namespace std::chrono;

      const auto row_count = data_rows.size();
      constexpr std::size_t N = data_size_v<DATA>();

      std::array<std::vector<std::string>, N> svgs{};
      std::array<SummaryData, N> peak{};
      std::array<SummaryData, N> total{};

      SummaryData peak_peak{};

      for (DATA& row : data_rows) {
        static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
            auto& t = total[n];
            auto& p = peak[n];
            auto& r = get_unit<DATA,n>(row);
            if (r.timestamp < start_time || r.timestamp > end_time) {
              return;
            }
            std::println("total: {}", r.total);
            t += r;
            r.normalize();
            std::println("rate: {}", r.total);
            std::println("peak: {}", p.total);
            p << r;
            std::println("new peak: {}", p.total);
            peak_peak << r;
          });
      }

      static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
          svgs[n].reserve(76 + row_count * 4);
        });

      double y_max{peak_peak.total * cfg.y_padding};
      if (y_max < 1.0) {
        std::println("has no sent?");
        y_max = 1;
      }

      static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
          write_header(svgs[n], start_time, end_time, y_max);
        });

      drawStackedLines(data_rows, svgs, start_time, end_time, y_max);

      static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
          drawLabels(svgs[n], end_time - start_time, peak[n], total[n]);
          svgs[n].push_back("</svg>\n");
        });

      if constexpr (N == 1) {
        return util::string_join("", svgs[0]);
      }
      else {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
          return std::tuple<decltype(util::string_join("", svgs[Is]))...>{ util::string_join("", svgs[Is])... };
        }(std::make_index_sequence<N>{});

      }
    }

    void write_header(std::vector<std::string>& svg, const auto& start_time, const auto& end_time, const auto y_max) const {
      svg.push_back(std::format("<svg xmlns='http://www.w3.org/2000/svg' width='{}' height='{}' style='background:#fff; font-family: Arial, sans-serif;'>\n", cfg.width, cfg.height));

      svg.emplace_back("<rect width='100%' height='100%' fill='white'/>\n");

      drawAxes(svg, start_time, end_time, y_max);
    }

    void drawAxes(std::vector<std::string>& svg,
                  const std::chrono::system_clock::time_point start_time,
                  const std::chrono::system_clock::time_point end_time,
                  const auto& y_max) const {
    
      // Draw Y axis line
      svg.push_back(std::format("<line x1='{}' y1='{}' x2='{}' y2='{}' stroke='black' stroke-width='1'/>\n",
                                cfg.left_margin,
                                cfg.top_margin,
                                cfg.left_margin,
                                cfg.height - cfg.bottom_margin));

      // Draw X axis line
      svg.push_back(std::format("<line x1='{}' y1='{}' x2='{}' y2='{}' stroke='black' stroke-width='1'/>\n",
                                cfg.left_margin,
                                cfg.height - cfg.bottom_margin,
                                cfg.width - cfg.right_margin,
                                cfg.height - cfg.bottom_margin));

      // Y axis ticks and labels (5 ticks)
      for (int i = 0; i < 5; i++) {
        double y = i / 5.0 * (cfg.height - cfg.top_margin - cfg.bottom_margin) + cfg.top_margin;
        svg.push_back(std::format("<line x1='{}' y1='{:.2f}' x2='{}' y2='{:.2f}' stroke='black' stroke-width='2' />\n",
                                  cfg.left_margin - 10,
                                  y,
                                  cfg.left_margin,
                                  y));
                                

        double val = y_max * (5 - i) / 5.0;
        svg.push_back(std::format("<text x='{}' y='{:.2f}' font-size='20' text-anchor='end'>{}bps</text>\n",
                                  cfg.left_margin - 15,
                                  y + 4,
                                  formatTraffic(val)));                                
      }

      // X axis daily ticks
      auto current_day = alignToDayStart(start_time);

      while (current_day <= end_time) {
        double x = timestampToX(current_day, start_time, end_time);
        svg.push_back(std::format("<line x1='{:.2f}' y1='{}' x2='{:.2f}' y2='{}' stroke='black' stroke-width='2' />\n",
                                  x,
                                  cfg.height - cfg.bottom_margin,
                                  x,
                                  cfg.height - cfg.bottom_margin + 5));

        svg.push_back(std::format("<text x='{:.2f}' y='{}' font-size='20' text-anchor='middle'>{}</text>\n",
                                  x,
                                  cfg.height - cfg.bottom_margin + 30,
                                  formatDate(current_day)));

        current_day += std::chrono::hours(24);
      }
    }

    double timestampToX(const auto& ts, const auto& start_time, const auto& end_time) const noexcept {
      const double w = cfg.width - cfg.left_margin - cfg.right_margin;
      const double elapsed = (ts - start_time).count();
      const double total = (end_time - start_time).count();
      return elapsed / total * w + cfg.left_margin;
    }
    template<typename DATA>
    void drawStackedLines(const std::vector<DATA>& data_rows, auto& svgs, const auto& start_time, const auto& end_time, const auto y_max) {

      constexpr std::size_t N = data_size_v<DATA>();

    

      std::array<std::array<std::vector<std::string>, layers.size()>, N> paths{};

      for (std::size_t i = 0; i < layers.size(); ++i) {
        static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
            paths[n][i].reserve(data_rows.size() * 6);
            paths[n][i].push_back(std::format(PATH_START, layers[i].color));
          });
      }
      std::array<double, N> y0s{};

      for (const DATA& d : data_rows) {
        y0s.fill(cfg.height - cfg.bottom_margin);

        auto& first = get_unit<DATA, 0>(d);
        const double x = timestampToX(first.timestamp, start_time, end_time + first.sample_duration);
        const double x1 = timestampToX(first.timestamp + first.sample_duration, start_time, end_time + first.sample_duration);

        static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
            const auto& sd = get_unit<DATA, n>(d);
            if (sd.timestamp < start_time || sd.timestamp > end_time) {
              return;
            }
            static_for<layers.size()>([&]<std::size_t i>(std::integral_constant<std::size_t, i>) {

                uint64_t val = sd.*(layers[i].field);
                if constexpr (layers[i].name == "tcp") {
                  val -= (sd.ftp + sd.http + sd.p2p + sd.mail);
                }
                double y = y0s[n]
                  - (val / y_max) * (graph_height - cfg.top_margin);
                drawBox(paths[n][i], x, x1, y0s[n], y);
                svgs[n].push_back(std::format("<text x='{}' y='{}' font-size='20' fill='black'>{} {}Bps</text>\n",
                                              x,
                                              y+50*i+50,
                                              layers[i].name,
                                              formatTraffic(val)
                                              ));
                std::println("setting y to {} for {} {} {}, {}, {}, {}", y, val, y_max, y0s[n], i, layers[i].name, n);
                std::println("{}", &(sd.*(layers[i].field)) - &sd.total);
                y0s[n] = y;
              });
          });

    

      }
      for (std::size_t i = 0; i < layers.size(); i++) {
        static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
            paths[n][i].emplace_back(" Z' />\n");
          });
      }
      static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
          for (std::size_t i = 0; i < layers.size(); i++) {
            svgs[n].push_back(util::string_join("", paths[n][i]));
          }
        });
  
    }

    void drawBox(std::vector<std::string>& svgpath, double x0, double x1, double y0, double y1) {
      svgpath.push_back("\n");
      moveTo(svgpath, x0, y1);
      lineTo(svgpath, x1, y1);
      lineTo(svgpath, x1, y0);
      lineTo(svgpath, x0, y0);
      lineTo(svgpath, x0, y1);
      svgpath.push_back("\n");
    }

    void moveTo(std::vector<std::string>& svgpath, double x, double y) {
      svgpath.push_back(std::format("M {:.2f} {:.2f} ", x, y));
    }
    void lineTo(std::vector<std::string>& svgpath, double x, double y) {
      svgpath.push_back(std::format("L {:.2f} {:.2f} ", x, y));
    }

    void drawLabels(std::vector<std::string>& svg, const auto& duration, const auto& peak, const auto& total) const {
      uint128_t s = static_cast<uint128_t>(duration_cast<std::chrono::seconds>(duration).count());
      std::println("duration: {}", s);
      svg.push_back(std::format("<text x='10' y='15' font-size='20' fill='black'>Peak: {}Bps, Total: {}B, Avg: {}Bps</text>\n",
                                formatTraffic(peak.total),
                                formatTraffic(total.total),
                                formatTraffic(total.total / s)));

      static_for<layers.size()>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
          svg.push_back(std::format("<text x='{}' y='{}' font-size='20' fill='black'>{}</text>\n", cfg.width - 50, 30 + 20*n, layers[n].name));
          svg.push_back(std::format("<rect x='{}' y='{}' width='50' height='20' fill='{}' />\n", cfg.width - 100, 15 + 20*n, layers[n].color));

        });
    }

    // Utility: Align time point to 00:00 of that day
    static std::chrono::system_clock::time_point alignToDayStart(std::chrono::system_clock::time_point t) {
      using namespace std::chrono;
      auto dp = floor<days>(t);
      return system_clock::time_point{dp};
    }

    static std::string formatDate(std::chrono::system_clock::time_point tp) {
      using namespace std::chrono;

      // Convert to time_point truncated to days
      auto dp = floor<days>(tp);

      // Convert to year_month_day
      year_month_day ymd = dp;

      // Format as YYYY-MM-DD
      return std::format("{:%F}", ymd); // %F = ISO 8601 (YYYY-MM-DD)
    }
    std::string formatTraffic(double value) const {
      constexpr const char* suffix[] = {"", "Ki", "Mi", "Gi", "Ti"};
      int idx = 0;
      while (value >= 1024.0 && idx < 4) {
        value /= 1024.0;
        ++idx;
      }
      return std::format("{:.2f}{}", value, suffix[idx]);
    }
  };
}
