module;
export module Postgres;
import string_util;
import Config;
import <format>;
import <print>;
export import <pqxx/pqxx>;




export
class PostgresDB  {
public:
  using params = pqxx::params;
  using prepared_t = std::string;
  
  pqxx::connection connection;

  PostgresDB(Config& config) : connection(config.psql_connect_string.c_str()) {
  }

  pqxx::work begin() {
    return pqxx::work{connection};
  };
  constexpr auto prepare(const std::string_view query) {
    int ctr = 1;
    auto res = util::iter_replace
      (query, "$$",
       [&](std::string acc, auto&& subrange) {
         acc += std::format("${}", ctr++);
         acc += util::iter_replace
           (std::string{subrange.begin(), subrange.end()},
            "$now",
            [&](std::string acc2, auto&& subrange2) {
              acc2 += std::format("${}::timestamp", ctr++);
              acc2.append(subrange2.begin(), subrange2.end());
              return acc2;
            });
         
         return acc;
       });
       

    std::println("preparing psql statement: {}", res);
    return res;
  }
  
};

