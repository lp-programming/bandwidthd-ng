import BandwidthD;
import Parser;
import Memory;
import Cursor;
import Postgres;
import Sqlite;
import TrafficGraph;
import Syslog;

import <print>;
import <unordered_map>;
import <fstream>;
import <chrono>;
import <vector>;
import <ranges>;
import <type_traits>;
import <functional>;

import <unistd.h>;

using namespace std::chrono_literals;
using namespace bandwidthd;

consteval bool HasSqlite() {
  return !std::is_empty_v<SqliteDB>;
}

consteval bool HasPostgres() {
  return !std::is_empty_v<PostgresDB>;
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
  int pg_sensor_id;
  int sq_sensor_id;

  ClassicSensor(Config& config, MEM& mem, auto& pg_cursor, auto& sq_cursor):
    Sensor<ClassicSensor<MEM, PG_CURSOR, SQ_CURSOR>, Modes::BothDefault>(config),
    mem(mem),
    pg_cursor(maybe_ref(pg_cursor)),
    sq_cursor(maybe_ref(sq_cursor)) {
    if constexpr (HasPostgres()) {
      if (this->pg_cursor) {
        pg_sensor_id = this->pg_cursor.value().get().GetSensorId();
      }
    }
    if constexpr (HasSqlite()) {
      if (this->sq_cursor) {
        sq_sensor_id = this->sq_cursor.value().get().GetSensorId();
      }
    }
  }

  int Run(this auto& base) {
    uint32_t count{0};
    for (;;) {
      const auto start_time = std::chrono::system_clock::now();
      auto now = std::chrono::system_clock::now();
      while (((now = std::chrono::system_clock::now()) - start_time) < base.config.interval) {
        if (base.Poll()) {
          base.Step();
        }
      }
      if (base.config.output_cdf || base.config.graph_intervals) {
        base.mem.CollateData(base.ips, base.txrx, base.config.interval);
        base.mem.CollateData(base.ips6, base.txrx6, base.config.interval);
      }

      if constexpr (HasPostgres()) {
        if (base.pg_cursor) {
          auto& p = base.pg_cursor.value().get();
          p.UpdateSensor(base.pg_sensor_id);
          p.SerializeData(base.pg_sensor_id, base.ips, base.txrx, base.config.interval, now);
          p.SerializeData(base.pg_sensor_id, base.ips6, base.txrx6, base.config.interval, now);
        }
      }
      if constexpr (HasSqlite()) {
        if (base.sq_cursor) {
          auto& s = base.sq_cursor.value().get();
          s.UpdateSensor(base.sq_sensor_id);
          s.SerializeData(base.sq_sensor_id, base.ips, base.txrx, base.config.interval, now);
          s.SerializeData(base.sq_sensor_id, base.ips6, base.txrx6, base.config.interval, now);
        }
      }

      if (! (count++ % (base.config.skip_intervals + 1))) {
        if (base.config.graph_intervals) {
          TrafficGraph grapher{{.width=1920, .height=1080, .left_margin=150, .top_margin=150, .bottom_margin=50, .right_margin=100}};
          base.mem.Graph(grapher);
        }
        if (base.config.output_cdf) {
          std::string cdf = base.mem.MakeCdf();
          std::filesystem::path log_dir { base.config.log_dir };
          if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
          }
          constexpr auto FLAGS = std::ios::binary | std::ios::out | std::ios::trunc;

          std::ofstream cdf_file{log_dir / "bandwidthd.cdf", FLAGS};
          cdf_file << cdf << "\n";
        }
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

  using PostgresCursor = std::conditional_t<
    HasPostgres(),
    Cursor<PostgresDB>,
    PostgresDB
    >;

  using SqliteCursor = std::conditional_t<
    HasSqlite(),
    Cursor<SqliteDB>,
    SqliteDB
    >;

  std::optional<PostgresCursor> pg_cursor{};
  if (!config.psql_connect_string.empty()) {
    if constexpr (HasPostgres()) {
      pg_cursor.emplace(config);
    }
    else {
      std::println("Postgres backend requested, but postgres support not built.");
      return 1;
    }
  }

  std::optional<SqliteCursor> sq_cursor{};
  if (!config.sqlite_connect_string.empty()) {
    if constexpr (HasSqlite()) {
      sq_cursor.emplace(config);
    }
    else {
      std::println("Sqlite backend requested, but sqlite support not built.");
      return 1;
    }
  }
  
  Memory memory{config};
  ClassicSensor<Memory, PostgresCursor, SqliteCursor> sensor{config, memory, pg_cursor, sq_cursor};

  if (background && fork()) {
    return 0;
  }
  return sensor.Run();
}

int main(const int argc, const char * const * const argv) {
  const std::vector<std::string> args{argv, argv + argc};
  return Main(args);
}
