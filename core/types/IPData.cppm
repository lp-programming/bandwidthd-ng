module;
export module IPData;

import net_integer;
import Statistics;

import <string>;

using types::net_integer::net_u128;

namespace bandwidthd {

  export class IPData {
  public:
    std::string label{};
    net_u128 ip;
    Statistics Sent;
    Statistics Received;
  };
}
