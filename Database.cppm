module;
export module Database;
export import BandwidthD;
export import Types;
export typedef __uint128_t uint128_t ;

export 
template<typename CONN>
class Cursor : public CONN {
public:
  int sensor_id{};
  CONN& connection;
  
  Cursor(Config& config): CONN(config), connection(*this){
    validate();
  }

  void validate() {
    auto txn = connection.begin();
    txn.exec("CREATE TABLE IF NOT EXISTS sensors ( sensor_id INTEGER PRIMARY KEY, sensor_name varchar, location int, build int default 0, uptime int, reboots int default 0, interface varchar, description varchar, management_url varchar, last_connection int );");
    txn.exec("CREATE TABLE IF NOT EXISTS bd_rx_log (sensor_id int, label varchar, ip inet, timestamp timestamp, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_rx_log_sensor_id_ip_timestamp_idx on bd_rx_log (sensor_id, ip, timestamp); create index if not exists bd_rx_log_sensor_id_timestamp_idx on bd_rx_log(sensor_id, timestamp);");
    txn.exec("CREATE TABLE IF NOT EXISTS bd_tx_log (sensor_id int, label varchar, ip inet, timestamp timestamp, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_tx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index if not exists bd_tx_log_sensor_id_timestamp_idx on bd_tx_log(sensor_id, timestamp);");

    txn.exec("CREATE TABLE IF NOT EXISTS bd_tx_rx_log (sensor_id int, label varchar, srcip inet, dstip inet, timestamp timestamp, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_tx_rx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index if not exists bd_tx_rx_log_sensor_id_timestamp_idx on bd_tx_rx_log(sensor_id, timestamp);");
    txn.commit();

    
  }

  void SerializeData(const auto& IPs, const auto& both, const int duration) {
    auto txn = connection.begin();
    for (const auto& entry : IPs) {
      const auto &ip = entry.first;
      const auto &data = entry.second;
      const auto &sent = data.Sent;
      const auto &recv = data.Received;
      const auto &label = data.label;
      const std::string strip = util::format_ip(ip);
      txn.exec("INSERT INTO bd_tx_log values ($1, $2, $3, now(), $4, $5, $6, $7, $8, $9, $10, $11, $12, $13);", {sensor_id, label, strip, duration, sent.packet_count, sent.total, sent.icmp, sent.udp, sent.tcp, sent.ftp, sent.http, sent.mail, sent.p2p});
      txn.exec("INSERT INTO bd_rx_log values ($1, $2, $3, now(), $4, $5, $6, $7, $8, $9, $10, $11, $12, $13);", {sensor_id, label, strip, duration, recv.packet_count, recv.total, recv.icmp, recv.udp, recv.tcp, recv.ftp, recv.http, recv.mail, recv.p2p});
    }



    for (auto entry : both) {
      auto srcip = entry.first.first;
      auto dstip = entry.first.second;
      auto data = entry.second;
      const std::string strsrcip = util::format_ip(srcip);
      const std::string strdstip = util::format_ip(dstip);
            
      txn.exec("INSERT INTO bd_tx_rx_log values ($1, $2, $3, $4, now(), $5, $6, $7, $8, $9, $10, $11, $12, $13, $14);", {sensor_id, data.label, strsrcip, strdstip, duration, data.packet_count, data.total, data.icmp, data.udp, data.tcp, data.ftp, data.http, data.mail, data.p2p});
    }

    txn.commit();
  }
};

