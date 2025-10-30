import Postgres;
import BandwidthD;
import Parser;
import Memory;
import TrafficGraph;
import <vector>;
import <string>;
import <span>;
import <print>;
import <chrono>;
import <string_view>;
import <sstream>;
import <exception>;
import <stdexcept>;
import <unordered_map>;
import <vector>;
import <utility>;
import <chrono>;
import <exception>;
import <optional>;
import <unistd.h>;


using namespace std::string_literals;

std::chrono::time_point<std::chrono::system_clock>
parse_datetime(const std::string_view input) {
  using namespace std::chrono;
  int y;
  unsigned int m, d;
  int H = 0, M = 0, S = 0;
  char sep1, sep2, sep3, sep4, sep5;

  std::istringstream ss{input};

  // Parse date part YYYY-MM-DD
  ss >> y >> sep1 >> m >> sep2 >> d;

  if (ss.fail() || sep1 != '-' || sep2 != '-') {
    throw std::runtime_error("Bad date format");
  }

  // Check if there's time part
  if (ss.peek() == EOF) {
    // Only date, no time
    auto ymd = std::chrono::year{y}/std::chrono::month{m}/std::chrono::day{d};
    if (!ymd.ok()) {
      throw std::runtime_error{"Invalid date"};
    }
    return sys_days{ymd};
  }

  // Expect space or 'T' separator before time
  ss >> sep3;
  if (ss.fail() || (sep3 != ' ' && sep3 != 'T')) {
    throw std::runtime_error{"Bad datetime format"};
  }

  // Parse time part HH:MM:SS
  ss >> H >> sep4 >> M >> sep5 >> S;

  if (ss.fail() || sep4 != ':' || sep5 != ':') {
    throw std::runtime_error{"Bad time format"};
  }

  // Validate date and time
  using namespace std::chrono;
  auto ymd = year{y}/month{m}/day{d};
  if (!ymd.ok()) {
    throw std::runtime_error{"Bad date format"};
  }

  if (H < 0 || H > 23 || M < 0 || M > 59 || S < 0 || S > 59) {
    throw std::runtime_error{"Bad time format"};
  }

  sys_days date{ymd};
  sys_seconds time_point = date + hours{H} + minutes{M} + seconds{S};

  return std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>{time_point};
}




int Main(int argc, char **argv) {
  Config config{};
  size_t s_argc = static_cast<size_t>(argc);

  const std::vector<std::string> args{argv, argv + argc};
  if (argc < 5 || !(argc % 2)) {
    std::println("Usage: {} --start <START> --end <END> [--subnet <SUBNET> ..] ", args.at(0));
    return 1;
  }

  bool relative_start{false};
  bool relative_end{false};
  std::string start{};
  std::string end{};
  std::vector<std::string> subnets{};
  
  for (size_t i = 1; i < s_argc; i += 2 ) {
    std::string_view arg = args[i];
    if (arg == "--start") {
      start = args.at(i + 1);
      if (start.starts_with("-")) {
        relative_start = true;
        start = start.substr(1);
      }
    }
    if (arg == "--end"s) {
      end = args.at(i + 1);
      if (end.starts_with("-")) {
        relative_end = true;
        end = end.substr(1);
      }
    }
    if (arg == "--subnet"s) {
      auto& sn = subnets.emplace_back(args.at(i + 1));
      std::println("Graphing subnet {}", sn);

    }
  }

  Cursor<PostgresDB> db{config};
  for (auto& sn: subnets) {
    std::vector<SummaryData> res = db.GetData(start, end, relative_start, relative_end, sn, "rx", "ip");
    TrafficGraph tg{{.width=1920, .height=1080, .left_margin=150, .top_margin=100, .bottom_margin=50, .right_margin=50}};
    auto st = parse_datetime(start);
    auto et = parse_datetime(end);


    std::println("{}", tg.render(res, st, et));
  }
               
  return 0;
}

extern "C" {
  int main(int argc, char **argv) {
    return Main(argc, argv);
  }
}
