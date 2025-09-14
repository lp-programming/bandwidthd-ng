import TrafficGraph;

import Types;
import BandwidthD;
import Postgres;
import <vector>;
import <string>;
import <span>;
import <print>;
import <chrono>;
using namespace std::string_literals;


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
    auto res = db.GetData(start, end, relative_start, relative_end, sn, "tx", "ip");
    TrafficGraph tg(1920, 1080, 40);
    std::chrono::sys_days st = std::chrono::year{2025}/7/22;
    std::chrono::sys_days et = std::chrono::year{2025}/7/29;


    std::println("{}", tg.render(res, st, et));
  }
               
  return 0;
}

extern "C" {
  int main(int argc, char **argv) {
    return Main(argc, argv);
  }
}
