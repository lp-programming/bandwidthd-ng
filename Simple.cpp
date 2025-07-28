import Postgres;
import <map>;
import <vector>;
import <utility>;

template<typename CURSOR>
class DatabaseWritingSensor: public Sensor<DatabaseWritingSensor<CURSOR>, Modes::BothDefault>  {
public:

  CURSOR& cursor;
  DatabaseWritingSensor& base;
  DatabaseWritingSensor(Config& config, CURSOR& cursor): Sensor<DatabaseWritingSensor<CURSOR>, Modes::IPv4Default | Modes::IPv6Default>(config), cursor(cursor), base(*this) {
    
    
  }

  int Main() {
    for (;;) {
      const auto starttime = std::chrono::system_clock::now();
      while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - starttime).count() < static_cast<long long>(base.config.interval)) {
        if (this->Poll()) {
          this->Step();
        }
      }
      std::println("flushing");
      cursor.SerializeData(base.ips, base.txrx, base.config.interval);
      base.ips.clear();
      base.txrx.clear();
      cursor.SerializeData(base.ips, base.txrx6, base.config.interval);
      base.ips.clear();
      base.txrx6.clear();
    }
    return 0;
  } 
};

int Main(int argc, char **argv) {
  Exception::set_terminate();
  Config config{};
  config.interval = 10;

  const std::vector<std::string> args{argv, argv + argc};
  std::string usage = std::format("Usage: {} --dev DEV [--filter FILTER] [--promiscuous BOOL] --subnet <CIDR> [--subnet <CIDR>...] [--notsubnet <CIDR>...] [--txrxsubnet <CIDR>...]", args.at(0));
  if (argc < 5 || !(argc % 2)) {
    std::cerr << usage << std::endl << "Wrong number of args" << std::endl;
    return 1;
  }
  
  config.dev = args.at(2);
  config.filter = "tcp";

  for (auto i = 3; i < argc; i += 2) {
    auto arg = args.at(i);
    if (arg == std::string{"--filter"}) {
      config.filter = args.at(i + 1);
    }
    else if (arg == std::string{"--promiscuous"}) {
      config.promisc = args.at(i + 1) == "true";
    }
    else if (arg == std::string{"--subnet"}) {
      config.subnets.emplace_back(args.at(i + 1));
    }
    else if (arg == std::string{"--notsubnet"}) {
      config.notsubnets.emplace_back(args.at(i + 1));
    }
    else if (arg == std::string{"--txrxsubnet"}) {
      config.txrxsubnets.emplace_back(args.at(i + 1));
    }
  }


  Cursor<PostgresDB> db{config};
  DatabaseWritingSensor<Cursor<PostgresDB>> sensor{config, db};
  
  
  return sensor.Main();
}

extern "C" {
  int main(int argc, char **argv) {
    return Main(argc, argv);
  }
}
