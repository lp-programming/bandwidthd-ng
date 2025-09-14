export module TrafficGraph;

export import <string>;
export import <vector>;
export import <unordered_map>;
export import <chrono>;
export import <sstream>;
export import <iomanip>;
export import <concepts>;
export import <algorithm>;
export import <format>;
export import <print>;
export import <numeric>;
export import Types;

static constexpr const char* COLOR_ICMP = "#FF0000";   // Red
static constexpr const char* COLOR_UDP = "#800000";    // Brown
static constexpr const char* COLOR_TCP = "#00FF00";    // Green
//static constexpr const char* COLOR_P2P = "#008000";    // Dark Green
static constexpr const char* COLOR_HTTP = "#0000FF";   // Blue
static constexpr const char* COLOR_MAIL = "#800080";   // Purple
static constexpr const char* COLOR_FTP = "#FF8000";    // Orange
static constexpr const char* COLOR_TOTAL = "#000000";  // Black

class AggregateData : public SummaryData {
public:
  uint32_t samples{0};

  inline void normalize() {
    if (!samples) {
      return;
    }
    total /= samples;
    icmp /= samples;
    udp /= samples;
    tcp /= samples;
    ftp /= samples;
    http /= samples;
    mail /= samples;
    p2p /= samples;
    count /= samples;
    samples = 1;
  }

  inline void operator<<(const SummaryData& rhs) {
    total += rhs.total;
    icmp += rhs.icmp;
    udp += rhs.udp;
    tcp += rhs.tcp;
    ftp += rhs.ftp;
    http += rhs.http;
    mail += rhs.mail;
    p2p += rhs.p2p;
    count += rhs.count;
    samples++;
  }

};



export
class TrafficGraph {
private:
  int width;
  int height;
  int yoffset;
  std::map<uint64_t, AggregateData> samples_by_x;

  double y_max = 1.0;
  double peak_rate = 0.0;
  double total_sum = 0.0;

public:
  TrafficGraph(int width, int height, int yoffset)
    : width(width), height(height), yoffset(yoffset) {}

  std::string render(const std::vector<SummaryData>& data_rows,
                     const std::chrono::system_clock::time_point start_time,
                     const std::chrono::system_clock::time_point end_time) {
    aggregateSamples(data_rows, start_time, end_time);
    computeMaxAndPeak();
    return buildSVG(start_time, end_time);
  }

private:

  void clear() {
    samples_by_x.clear();
    y_max = 1.0;
    peak_rate = 0.0;
    total_sum = 0.0;
  }

  void aggregateSamples(const std::vector<SummaryData>& data_rows,
                        const std::chrono::system_clock::time_point start_time,
                        const std::chrono::system_clock::time_point end_time) {
    using namespace std::chrono;

    clear();
    for (const auto& row : data_rows) {
      if (row.timestamp < start_time || row.timestamp > end_time) {
        continue;
      }

      uint64_t x = timestampToX(row.timestamp, start_time, end_time);
      samples_by_x[x] << row;
    }
  }

  void computeMaxAndPeak() {
    y_max = 0;
    peak_rate = 0;
    total_sum = 0;

    for (auto& [x, sample] : samples_by_x) {
      sample.normalize();
      if (sample.total > peak_rate) {
        peak_rate = sample.total;
      }
      total_sum += sample.total;
      if (sample.total > y_max) {
        y_max = sample.total;
      }
    }

    y_max *= 1.05; // 5% headroom
    if (y_max < 1.0) {
      y_max = 1.0;
    }
  }

  uint64_t timestampToX(const std::chrono::system_clock::time_point ts,
                   const std::chrono::system_clock::time_point start,
                   const std::chrono::system_clock::time_point end) const {
    using namespace std::chrono;
    double ratio = double(duration_cast<milliseconds>(ts - start).count()) /
      double(duration_cast<milliseconds>(end - start).count());
    ratio = std::clamp(ratio, 0.0, 1.0);
    return uint64_t(ratio * (width - 60)) + 50; // margin left=50, right=10
  }

  int scaleY(double value) const {
    return height - int((value / y_max) * (height - yoffset)) - yoffset;
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

  std::string buildSVG(std::chrono::system_clock::time_point start_time,
                       std::chrono::system_clock::time_point end_time) {

    std::vector<std::string> svg{};
    svg.reserve(76 + samples_by_x.size() * 4);
    svg.push_back(std::format("<svg xmlns='http://www.w3.org/2000/svg' width='{}' height='{}' style='background:#fff; font-family: Arial, sans-serif;'>\n", width, height));

    svg.emplace_back("<rect width='100%' height='100%' fill='white'/>\n");

    drawAxes(svg, start_time, end_time);
    drawStackedLines(svg);
    drawLabels(svg);

    svg.push_back("</svg>\n");
    return std::accumulate(svg.begin(), svg.end(), std::string{});
  }

  void drawAxes(std::vector<std::string>& svg,
                const std::chrono::system_clock::time_point start_time,
                const std::chrono::system_clock::time_point end_time) const {
    int left_margin = 50;
    int bottom_margin = yoffset;

    // Draw Y axis line
    svg.push_back(std::format("<line x1='{}' y1='0' x2='{}' y2='{}' stroke='black' stroke-width='1'/>\n",
                              left_margin,
                              left_margin,
                              height - bottom_margin));

    // Draw X axis line
    svg.push_back(std::format("<line x1='{}' y1='{}' x2='{}' y2='{}' stroke='black' stroke-width='1'/>\n",
                              left_margin,
                              height - bottom_margin,
                              width - 10,
                              height - bottom_margin));

    // Y axis ticks and labels (5 ticks)
    for (int i = 0; i <= 5; ++i) {
      int y = scaleY(y_max * i / 5.0);
      svg.push_back(std::format("<line x1='{}' y1='{}' x2='{}' y2='{}' stroke='black'/>\n",
                                left_margin - 5,
                                y,
                                left_margin,
                                y));
                                

      double val = y_max * i / 5.0;
      svg.push_back(std::format("<text x='{}' y='{}' font-size='10' text-anchor='end'>{}</text>\n",
                                left_margin - 10,
                                y + 4,
                                formatTraffic(val)));                                
    }

    // X axis daily ticks
    auto current_day = alignToDayStart(start_time);

    while (current_day <= end_time) {
      int x = timestampToX(current_day, start_time, end_time);
      svg.push_back(std::format("<line x1='{}' y1='{}' x2='{}' y2='{}' stroke='black'/>\n",
                                x,
                                height - bottom_margin,
                                x,
                                height - bottom_margin + 5));

      svg.push_back(std::format("<text x='{}' y='{}' font-size='10' text-anchor='middle'>{}</text>\n",
                                x,
                                height - bottom_margin + 20,
                                formatDate(current_day)));

      current_day += std::chrono::hours(24);
    }
  }

  void drawStackedLines(std::vector<std::string>& svg) {

    // Compute stack heights in pixels for each protocol
    size_t n = samples_by_x.size();
    int base{height - yoffset};

    std::vector<uint64_t> xs(n),y_icmp(n), y_udp(n), y_tcp(n), y_p2p(n), y_http(n), y_mail(n), y_ftp(n);
    size_t max_x{0};

    for (const auto& [i, sample]: samples_by_x) {
      xs.push_back(i);
      if (i > max_x) {
        max_x = i;
      }
      y_icmp[i] = uint64_t(base) - uint64_t((sample.icmp / y_max) * (height - yoffset));
      y_udp[i]  = y_icmp[i] - uint64_t((sample.udp / y_max) * (height - yoffset));
      uint64_t tcp_p2p_y = y_udp[i] - uint64_t(((sample.tcp + sample.p2p) / y_max) * (height - yoffset));
      y_tcp[i] = tcp_p2p_y;
      y_p2p[i] = tcp_p2p_y;
      y_http[i] = y_tcp[i] - uint64_t((sample.http / y_max) * (height - yoffset));
      y_mail[i] = y_http[i] - uint64_t((sample.mail / y_max) * (height - yoffset));
      y_ftp[i] = y_mail[i] - uint64_t((sample.ftp / y_max) * (height - yoffset));
    }

    std::vector<uint64_t> base_vec(n);

    auto polygonPath = [&](const std::vector<uint64_t>& top, const std::vector<uint64_t>& bottom, auto color) {
      svg.push_back(std::format("<path fill='{}' opacity='0.7' stroke='black' d='", color));
      svg.push_back( std::format("M {} {}", xs[0], top[0]));

      for (size_t i = 1; i < max_x; ++i) {
        svg.push_back(std::format(" L {} {}", xs[i], top[i]));
      }

      for (size_t i = max_x; i-- > 0;) {
        svg.push_back(std::format(" L {} {}", xs[i], bottom[i]));
      }
      svg.push_back(" Z' />\n");
    };

    polygonPath(y_icmp, base_vec, COLOR_ICMP);
    polygonPath(y_udp, y_icmp, COLOR_UDP);
    polygonPath(y_tcp, y_udp, COLOR_TCP);
    polygonPath(y_http, y_tcp, COLOR_HTTP);
    polygonPath(y_mail, y_http, COLOR_MAIL);
    polygonPath(y_ftp, y_mail, COLOR_FTP);


    // Draw total traffic line on top
    svg.push_back(std::format("<polyline fill='none' stroke='{}' stroke-width='1' points='", COLOR_TOTAL));
    for (size_t i = 0; i < n; ++i) {
      int y = scaleY(samples_by_x[xs[i]].total);
      svg.push_back(std::format("{},{} ", xs[i], y));
    }
    svg.push_back("' />\n");
  }

  void drawLabels(std::vector<std::string>& svg) const {
    svg.push_back(std::format("<text x='10' y='15' font-size='12' fill='black'>Peak: {}Bps, Avg: {}Bps</text>\n",
                              formatTraffic(peak_rate),
                              formatTraffic(total_sum / (samples_by_x.size() ? samples_by_x.size() : 1))));
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

};
