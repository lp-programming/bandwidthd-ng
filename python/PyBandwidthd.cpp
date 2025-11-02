import BandwidthD;
import format_ip;
import SubnetIdentifier;
import TrafficGraph;
import SummaryData;

import <sys/socket.h>;
import <map>;
import <vector>;
import <utility>;
import <pybind11/pybind11.h>;
import <pybind11/stl.h>;
import <pybind11/chrono.h>;
import <pybind11/functional.h>;

using types::net_integer::parse_ipv4;
using types::net_integer::parse_ipv6;


namespace py = pybind11;

using namespace bandwidthd;

class PythonSensor: public Sensor<PythonSensor, Modes::BothDefault> {
public:
  PythonSensor(Config& config): Sensor<PythonSensor, Modes::BothDefault>(config) {
    
  }


  void Finalize(py::object ipcb, py::object txrxcb) {
    for (auto entry: this->ips) {
      ipcb(util::format_ip(entry.first), entry.second);
    }
    for (auto entry: this->txrx) {
      txrxcb(util::format_ip(entry.first.first), util::format_ip(entry.first.second), entry.second);
    }
    for (auto entry: this->ips6) {
      ipcb(util::format_ip(entry.first), entry.second);
    }
    for (auto entry: this->txrx6) {
      txrxcb(util::format_ip(entry.first.first), util::format_ip(entry.first.second), entry.second);
    }
    this->txrx.clear();
    this->ips.clear();
    this->txrx6.clear();
    this->ips6.clear();
  }
  
};



PYBIND11_MODULE(PyBandwidthD, m) {
    m.doc() = "Bandwidthd"; // optional module docstring
    py::class_<SubnetIdentifier>(m, "Subnet")
      .def(py::init<>())
      .def(py::init<std::string_view>())
      .def_property("ip",
                    [](const SubnetIdentifier& self){
                      if (self.family == AF_INET) {
                        return util::format_ipv4(self.ip);
                      }
                      return util::format_ipv6(self.ip);
                    },
                    [](SubnetIdentifier& self, std::string_view value){
                      if (self.family == AF_INET) {
                        self.ip = self.widen_ipv4(parse_ipv4(value));
                      }
                      else {
                        self.ip = parse_ipv6(value);
                      }
                    },
                    py::return_value_policy::copy)
      .def_property("mask",
                    [](const SubnetIdentifier& self){
                      if (self.family == AF_INET) {
                        return util::format_ipv4(self.mask);
                      }
                      return util::format_ipv6(self.mask);
                    },
                    [](SubnetIdentifier& self, std::string_view value){
                      if (self.family == AF_INET) {
                        self.mask = self.widen_ipv4(parse_ipv4(value));
                      }
                      else {
                        self.mask = parse_ipv6(value);
                      }
                    },
                    py::return_value_policy::copy)
      .def_readwrite("family", &SubnetIdentifier::family)
      ;
    py::class_<Config>(m, "Config")
      .def(py::init<>())
      .def_readwrite("dev", &Config::dev)
      .def_readwrite("filter", &Config::filter)
      .def_readwrite("promisc", &Config::promisc)
      .def_readwrite("tag", &Config::tag)
      .def_readwrite("snaplen", &Config::snaplen)
      .def_readwrite("interval", &Config::interval)
      .def_readwrite("subnets", &Config::subnets)
      .def_readwrite("notsubnets", &Config::notsubnets)
      .def_readwrite("txrxsubnets", &Config::txrxsubnets)
      ;
    py::class_<PythonSensor>(m, "Sensor")
      .def(py::init<Config&>())
      .def("Poll", &PythonSensor::Poll)
      .def("Step", &PythonSensor::Step)
      .def("Finalize", &PythonSensor::Finalize)
      ;
    py::class_<Statistics>(m, "Statistics")
      .def_readwrite("packet_count", &Statistics::packet_count)
      .def_readwrite("total", &Statistics::total)
      .def_readwrite("icmp", &Statistics::icmp)
      .def_readwrite("udp", &Statistics::udp)
      .def_readwrite("tcp", &Statistics::tcp)
      .def_readwrite("ftp", &Statistics::ftp)
      .def_readwrite("http", &Statistics::http)
      .def_readwrite("mail", &Statistics::mail)
      .def_readwrite("p2p", &Statistics::p2p)
      .def_readwrite("label", &Statistics::label)
      ;

    py::class_<IPData>(m, "IPData")
      .def_readwrite("label", &IPData::label)
      .def_readwrite("sent", &IPData::Sent)
      .def_readwrite("recv", &IPData::Received)
      ;

    py::class_<SummaryData>(m, "SummaryData")
      .def(py::init<>())
      .def_readwrite("timestamp", &SummaryData::timestamp)
      .def_readwrite("sample_duration", &SummaryData::sample_duration)
      .def_readwrite("net", &SummaryData::net)
      .def_readwrite("total", &SummaryData::total)
      .def_readwrite("icmp", &SummaryData::icmp)
      .def_readwrite("udp", &SummaryData::udp)
      .def_readwrite("tcp", &SummaryData::tcp)
      .def_readwrite("ftp", &SummaryData::ftp)
      .def_readwrite("http", &SummaryData::http)
      .def_readwrite("mail", &SummaryData::mail)
      .def_readwrite("p2p", &SummaryData::p2p)
      .def_readwrite("count", &SummaryData::count)
      .def("__iadd__", [](SummaryData &a, const SummaryData &b) -> SummaryData& {
        a += b;
        return a;
      })
      .def("__add__", [](const SummaryData &a, const SummaryData &b) {
        SummaryData result = a;
        result += b;
        return result;
      })
      .def("__lshift__", &SummaryData::operator<<)
      ;


    py::class_<TrafficGraph>(m, "TrafficGraph")
      .def("render", &TrafficGraph::render<SummaryData>)
      .def(py::init([](int width = 1920,
                       int height = 1080,
                       int left_margin = 150,
                       int top_margin = 150,
                       int bottom_margin = 50,
                       int right_margin = 100) {
        return TrafficGraph{{width, height, left_margin, top_margin, bottom_margin, right_margin}};
      }),
           py::arg("width") = 1920,
           py::arg("height") = 1080,
           py::arg("left_margin") = 150,
           py::arg("top_margin") = 150,
           py::arg("bottom_margin") = 50,
           py::arg("right_margin") = 100);
      ;

    
  }
