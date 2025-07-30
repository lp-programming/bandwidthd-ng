module;
export module Postgres;
export import Database;
export import Types;
export import <pqxx/pqxx>;




export

class PostgresDB  {
public:
  pqxx::connection connection;

  PostgresDB(Config& config) : connection(config.db_connect_string.c_str()) {
  }

  pqxx::work begin() {
    return pqxx::work{connection};
  };

  
};

