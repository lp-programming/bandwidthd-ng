import Postgres;
import Sqlite;
import BandwidthD;
import Parser;
import Memory;
import TrafficGraph;
import Cursor;

import <unordered_map>;
import <vector>;
import <utility>;
import <chrono>;
import <exception>;
import <optional>;
import <unistd.h>;
import <ranges>;
import <functional>;
import <type_traits>;

using namespace std::chrono_literals;
using namespace bandwidthd;

consteval bool HasSqlite() {
  return !std::is_empty_v<SqliteDB>;
}


template<typename MEM, typename PG_CURSOR, typename SQ_CURSOR>
class ClassicSensor: public Sensor<ClassicSensor<MEM, PG_CURSOR, SQ_CURSOR>, Modes::BothDefault>  {
  template<typename T>
  constexpr auto maybe_ref(T& r) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::optional<typename T::value_type>>) {
      if (r) {
        return std::optional<std::reference_wrapper<typename T::value_type>>{*r};
      }
      else {
        return std::optional<std::reference_wrapper<typename T::value_type>>{std::nullopt};
      }
    }
    else {
      return std::optional<std::reference_wrapper<T>>{r};
    }
  }
  
public:
  MEM& mem;
  std::optional<std::reference_wrapper<PG_CURSOR>> pg_cursor;
  std::optional<std::reference_wrapper<SQ_CURSOR>> sq_cursor;

  ClassicSensor(Config& config, MEM& mem, auto& pg_cursor, auto& sq_cursor): Sensor<ClassicSensor<MEM, PG_CURSOR, SQ_CURSOR>, Modes::IPv4Default | Modes::IPv6Default>(config), mem(mem), pg_cursor(maybe_ref(pg_cursor)), sq_cursor(maybe_ref(sq_cursor)) {}

  int Main(this auto& base) {
    uint32_t count{0};
    std::println("interval: {}\ncdf: {}", base.config.interval, base.config.output_cdf);
    for (;;) {
      const auto starttime = std::chrono::system_clock::now();
      auto now = std::chrono::system_clock::now();
      while (((now = std::chrono::system_clock::now()) - starttime) < base.config.interval) {
        if (base.Poll()) {
          base.Step();
        }
      }
      if (base.config.output_cdf || base.config.graph_intervals) {
        base.mem.CollateData(base.ips, base.txrx, base.config.interval);
        base.mem.CollateData(base.ips6, base.txrx6, base.config.interval);
      }
      if (base.pg_cursor) {
        std::println("sending to psql");
                          
        base.pg_cursor.value().get().SerializeData(base.ips, base.txrx, base.config.interval, now);
        base.pg_cursor.value().get().SerializeData(base.ips6, base.txrx6, base.config.interval, now);
      }
      if constexpr (HasSqlite()) {
        if (base.sq_cursor) {
          std::println("sending to sqlite");
                          
          base.sq_cursor.value().get().SerializeData(base.ips, base.txrx, base.config.interval, now);
          base.sq_cursor.value().get().SerializeData(base.ips6, base.txrx6, base.config.interval, now);
        }
      }

      if (! (count++ % (base.config.skip_intervals + 1))) {
        std::println("graphing or cdfings");
        if (base.config.graph_intervals) {
          TrafficGraph grapher{{.width=1920, .height=1080, .left_margin=150, .top_margin=150, .bottom_margin=50, .right_margin=100}};
          base.mem.graph(grapher);
        }
        if (base.config.output_cdf) {
          std::string cdf = base.mem.make_cdf();
          std::println("{}", cdf);
        }
      }
      else {
        std::println("skipping");
      }

      base.ips.clear();
      base.ips6.clear();
      base.txrx.clear();
      base.txrx6.clear();
    }
    return 0;
  }
};

int Main(const std::vector<std::string>& args) {
  Config config{};
  config.interval = 200s;
  config.syslog_prefix = "bandwidthd";
  config.filter = "tcp";
  config.graph_individual_ips = true;
  bool background = true;

  bool cfg_file = false;
  for (const auto& arg : args | std::views::drop(1)) {
    if (cfg_file) {
      Parser parser{arg};
      parser.parse(config);
      cfg_file = false;
      continue;
    }
    if (arg == "-D") {
      background = false;
      continue;
    }
    if (arg == "-c") {
      cfg_file = true;
      continue;
    }
    if ((arg == "-h") || (arg == "--help")) {
        std::println("Usage: {} [-c conf] [-D]", args[0]);
        return 1;
    }
    std::println("Improper argument: {}", arg);
    return 1;
  }
  
  if (config.psql_connect_string.empty() && config.sqlite_connect_string.empty() && 
      (!config.output_cdf && !config.graph_intervals)) {
    std::println("Not sending to database, graphing, or writing cdfs, nothing to do!");
    return 1;
  }

  std::optional<Cursor<PostgresDB>> pg_cursor{};
  if (!config.psql_connect_string.empty()) {
    pg_cursor.emplace(config);
  }

  using SqliteCursor = std::conditional_t<
    HasSqlite(),
    Cursor<SqliteDB>,
    SqliteDB
    >;


  std::optional<SqliteCursor> sq_cursor{};
  if constexpr (HasSqlite()) {
    if (!config.sqlite_connect_string.empty()) {
      sq_cursor.emplace(config);
    }
  }
  
  Memory memory{config};
  ClassicSensor<Memory, Cursor<PostgresDB>, SqliteCursor> sensor{config, memory, pg_cursor, sq_cursor};
  if (background && fork()) {
    return 0;
  }
  return sensor.Main();
}

extern "C" {
  int main(const int argc, const char * const * const argv) {
    const std::vector<std::string> args{argv, argv + argc};
    return Main(args);
  }
}
