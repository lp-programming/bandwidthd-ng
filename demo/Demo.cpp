import Types;
import BandwidthD;
import <vector>;
import <string>;
import <span>;
import <print>;

class SimpleBD: public Sensor<SimpleBD, Modes::Both> {
public:
  void ProcessIPv6(const std::string& srcip, const std::string& dstip, const net_u128 , const net_u128 , const uint16_t length, const ip6_hdr& ) {
    std::println("Got {} bytes from {} to {}", length, srcip, dstip);
  }
  auto ProcessIPv4(const std::string& srcip, const std::string& dstip, const net_u32 , const net_u32 , const uint16_t length, const ip& ) {
    std::println("Got {} bytes from {} to {}", length, srcip, dstip);
  }
};

int Main(int argc, char **argv) {
  Config config{
  };

  const std::vector<std::string> args{argv, argv + argc};
  std::string usage = std::format("Usage: {} --dev DEV [--filter FILTER] [--promiscuous BOOL]", args.at(0));
  if (argc < 3 || !(argc % 2)) {
    std::cerr << usage << std::endl << "Wrong number of args" << std::endl;
    return 1;
  }
  
  config.dev = args.at(2);
  config.filter = "tcp";

  if (argc > 3) {
    if (args.at(3) == "--filter") {
      config.filter = args.at(4);
      if (argc > 5) {
        config.promisc = args.at(6) == "true";
      }
    }
    else {
      config.promisc = args.at(4) == "true";
    }
  }
  SimpleBD bd{config};
  for (;;) {
    if (bd.Poll()) {
      bd.Step();
    }
    
  }
  return 0;
}

extern "C" {
  int main(int argc, char **argv) {
    return Main(argc, argv);
  }
}
