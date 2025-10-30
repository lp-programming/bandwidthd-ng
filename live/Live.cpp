import BandwidthD;
import Memory;
import TrafficGraph;
//import <mutex>;

/*
std::mutex alloc_mutex;

size_t total_allocated = 0;
size_t total_freed = 0;

void* operator new(std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();

    {
        std::lock_guard<std::mutex> lock(alloc_mutex);
        total_allocated += size;
        printf("[alloc] %zu bytes at %p (total allocated: %zu bytes)\n", size, ptr, total_allocated);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    // Can't know size here, so no size deduction
    std::free(ptr);
    printf("[free] pointer at %p\n", ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    {
        std::lock_guard<std::mutex> lock(alloc_mutex);
        total_freed += size;
        printf("[free] %zu bytes at %p (total freed: %zu bytes)\n", size, ptr, total_freed);
    }
    std::free(ptr);
}


void* operator new[](std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    {
        std::lock_guard<std::mutex> lock(alloc_mutex);
        total_allocated += size;
        printf("[alloc[]] %zu bytes at %p (total allocated: %zu bytes)\n", size, ptr, total_allocated);
    }
    return ptr;
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
    printf("[free[]] pointer at %p\n", ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    {
        std::lock_guard<std::mutex> lock(alloc_mutex);
        total_freed += size;
        printf("[free[]] %zu bytes at %p (total freed: %zu bytes)\n", size, ptr, total_freed);
    }
    std::free(ptr);
}
*/

import <unordered_map>;
import <vector>;
import <utility>;
import <chrono>;
import <exception>;

using namespace std::chrono_literals;

namespace __cxxabiv1 {
  std::terminate_handler __terminate_handler = +[]() {
    std::abort();
  };
}


template<typename DB>
class MemorySensor: public Sensor<MemorySensor<DB>, Modes::BothDefault>  {
public:

  DB db;
  MemorySensor& base;
  MemorySensor(Config& config): Sensor<MemorySensor<DB>, Modes::BothDefault>(config), db{config}, base(*this) {
    
    
  }

  int Main() {
    uint32_t count{0};
    for (;;) {
      const auto starttime = std::chrono::system_clock::now();
      while ((std::chrono::system_clock::now() - starttime) < base.config.interval) {
        if (this->Poll()) {
          this->Step();
        }
      }
      count++;
      std::println("flushing");
      db.CollateData(base.ips, base.txrx, base.config.interval);
      base.ips.clear();
      base.txrx.clear();
      db.CollateData(base.ips, base.txrx6, base.config.interval);
      base.ips.clear();
      base.txrx6.clear();
      if (! (count++ % (base.config.skip_intervals + 1))) {
        std::println("graphing");
        TrafficGraph grapher{{.width=1920, .height=1080, .left_margin=150, .top_margin=100, .bottom_margin=50, .right_margin=50}};
        db.graph(grapher);
      }
      return 1;

    }
    return 0;
  } 
};

int Main(const uint argc, const char * const * const argv) {
  Config config{};
  config.graph_intervals = GraphIntervals{GraphInterval::Hour};
  config.htdocs_dir = "/tmp/bdcpp/htdocs";
  config.interval = 200s;
  config.syslog_prefix = "bandwidthd-live";

  const std::vector<std::string> args{argv, argv + argc};
  if (argc < 5 || !(argc % 2)) {
    std::println("Usage: {} --dev DEV [--filter FILTER] [--promiscuous BOOL] --subnet <CIDR> [--subnet <CIDR>...] [--notsubnet <CIDR>...] [--txrxsubnet <CIDR>...]", args.at(0));
    std::println("Wrong number of args");
    return 1;
  }
  
  config.dev = args.at(2);
  config.filter = "tcp";

  for (auto i = 3uz; i < argc; i += 2uz) {
    auto arg = args.at(i);
    if (arg == std::string{"--filter"}) {
      config.filter = args.at(i + 1uz);
    }
    else if (arg == std::string{"--promiscuous"}) {
      config.promisc = args.at(i + 1uz) == "true";
    }
    else if (arg == std::string{"--subnet"}) {
      auto& sn = config.subnets.emplace_back(args.at(i + 1uz));
      //            syslog(LOG_INFO, "Monitoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));

      Logger::info(std::format("Monitoring subnet {} with netmask {}",
                               util::format_ip(sn.ip, sn.family),
                               util::format_ip(sn.mask, sn.family)));
    }
    else if (arg == std::string{"--notsubnet"}) {
      auto& sn = config.notsubnets.emplace_back(args.at(i + 1uz));
      Logger::info(std::format("Ignoring subnet {} with netmask {}",
                               util::format_ip(sn.ip, sn.family),
                               util::format_ip(sn.mask, sn.family)));
    }
    else if (arg == std::string{"--txrxsubnet"}) {
      auto& sn = config.txrxsubnets.emplace_back(args.at(i + 1uz));
      Logger::info(std::format("Tracking subnet {} with netmask {}",
                               util::format_ip(sn.ip, sn.family),
                               util::format_ip(sn.mask, sn.family)));
    }
  }

  MemorySensor<Memory> sensor{config};
  
  
  return sensor.Main();
}

extern "C" {
  int main(const int argc, const char * const * const argv) {
    return Main(static_cast<const ssize_t>(argc), argv);
  }
}
