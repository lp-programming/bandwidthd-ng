import Postgres;
import Exception;
import Types;
import <sys/socket.h>;
import <map>;
import <vector>;
import <utility>;
import <pybind11/pybind11.h>;
import <pybind11/stl.h>;

using util::net_integer::parse_ipv4;
using util::net_integer::parse_ipv6;


namespace py = pybind11;

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

    
    py::register_exception<Exception::Exception>(m, "Exception::Exception", PyExc_RuntimeError);
  }
