module;
export module Sqlite;
export import Database;
export import Types;
export import <SQLiteCpp/SQLiteCpp.h>;
import <algorithm>;
import <ranges>;
import <functional>;
import <numeric>;
import <vector>;
import static_for;
import <limits>;

template<typename DB>
struct Transaction {
  DB& db;
  SQLite::Transaction txn;
  Transaction(auto& db): db(db), txn(SQLite::Transaction{db.connection}) {}

  auto exec(const std::string& query) {
    return db.prepare(query).exec();
  }
  template<typename... Ns>
  auto exec(const std::string& query, const std::tuple<Ns...>& args) {
    return exec(db.prepare(query), args);
  }
  template<typename... Ns>
  auto exec(SQLite::Statement& query, const std::tuple<Ns...>& args) {
    query.reset();
    constexpr std::size_t N = std::tuple_size_v<std::tuple<Ns...>>;
    std::print("Running sqlite statement: ");
    static_assert(N < std::numeric_limits<int>::max(), "Too many args to exec");
    static_for<N>([&]<std::size_t n>(std::integral_constant<std::size_t, n>) {
        const auto& arg = std::get<n>(args);
        using T = std::remove_cvref_t<decltype(arg)>;

        if constexpr ((std::is_same_v<T, long long>) || (std::is_unsigned_v<T> && std::is_integral_v<T>)) {
          std::print("{}, ", static_cast<int64_t>(arg));
          query.bind(static_cast<int>(n + 1), static_cast<int64_t>(arg));
        }
        else {
          std::print("{}, ", arg);
          query.bind(static_cast<int>(n + 1), arg);
        }
      });
    std::println(";");
    return query.exec();
  }

  auto commit() {
    return txn.commit();
  }
};



export
class SqliteDB  {
public:
  template<typename... Args>
  using params = std::tuple<Args...>;
  using prepared_t = SQLite::Statement;
  SQLite::Database connection;

  SqliteDB(Config& config) : connection(config.sqlite_connect_string.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {
  }

  Transaction<SqliteDB> begin(this auto& self) {
    return Transaction<SqliteDB>{self};
  };

  constexpr SQLite::Statement prepare(this auto& self, const std::string_view query) {
    
    auto res = util::iter_replace
      (query, "$$", 
       [](std::string acc, auto&& subrange) {
         acc += "?";
         acc.append(subrange.begin(), subrange.end()); // append word
         return acc;
       }
       );
    res = util::iter_replace
      (res, "$now",
       [](std::string acc, auto&& subrange) {
         acc += "?";
         acc.append(subrange.begin(), subrange.end()); // append word
         return acc;
       }
       );
                             
    std::println("preparing statement: {}", res);
    return SQLite::Statement{self.connection, res};
  }

  
};

