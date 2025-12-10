export module Cursor;
import <array>;
import <string_view>;
import <chrono>;
import <vector>;
import <string>;
import <format>;

import BandwidthD;
import SummaryData;
import format_ip;
import parse_date;
import uint128_t;
import Config;

namespace bandwidthd {
  export 
  template<typename CONN>
  class Cursor : public CONN {
    CONN& connection;
    const Config& config;

    CONN::prepared_t bd_tx_insert = connection.prepare("INSERT INTO bd_tx_log values ($$, $$, $$, $now, $$, $$, $$, $$, $$, $$, $$, $$, $$, $$);");
    CONN::prepared_t bd_rx_insert = connection.prepare("INSERT INTO bd_rx_log values ($$, $$, $$, $now, $$, $$, $$, $$, $$, $$, $$, $$, $$, $$);");
    CONN::prepared_t bd_tx_rx_insert = connection.prepare("INSERT INTO bd_tx_rx_log values ($$, $$, $$, $$, $now, $$, $$, $$, $$, $$, $$, $$, $$, $$, $$);");
    CONN::prepared_t get_sensor = connection.prepare("SELECT * FROM sensors WHERE sensor_name = $$ AND interface = $$");
    CONN::prepared_t new_sensor = connection.prepare("INSERT INTO sensors (sensor_name, interface) VALUES ($$, $$);");
    CONN::prepared_t update_sensor = connection.prepare("UPDATE sensors SET description = $$, management_url = $$, last_connection = now(), build = $$, uptime = $$*interval '1 second' where sensor_id = $$;");

    void validate() {
      auto txn = connection.begin();
      txn.exec("CREATE TABLE IF NOT EXISTS sensors ( sensor_id serial PRIMARY KEY, sensor_name varchar, location int, build int default 0, uptime interval, reboots int default 0, interface varchar, description varchar, management_url varchar, last_connection timestamptz );");
      txn.exec("CREATE TABLE IF NOT EXISTS bd_rx_log (sensor_id int, mac varchar(20), ip inet, timestamp timestamptz, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_rx_log_sensor_id_ip_timestamp_idx on bd_rx_log (sensor_id, ip, timestamp); create index if not exists bd_rx_log_sensor_id_timestamp_idx on bd_rx_log(sensor_id, timestamp);");
      txn.exec("CREATE TABLE IF NOT EXISTS bd_tx_log (sensor_id int, mac varchar(20), ip inet, timestamp timestamptz, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_tx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index if not exists bd_tx_log_sensor_id_timestamp_idx on bd_tx_log(sensor_id, timestamp);");

      txn.exec("CREATE TABLE IF NOT EXISTS bd_rx_total_log (sensor_id int, mac varchar(20), ip inet, timestamp timestamptz DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); CREATE index IF NOT EXISTS bd_rx_total_log_sensor_id_timestamp_ip_idx on bd_rx_total_log (sensor_id, timestamp);");
      txn.exec("CREATE TABLE IF NOT EXISTS bd_tx_total_log (sensor_id int, mac varchar(20), ip inet, timestamp timestamptz  DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); CREATE index IF NOT EXISTS bd_tx_total_log_sensor_id_timestamp_ip_idx on bd_tx_total_log (sensor_id, timestamp);");

      txn.exec("CREATE TABLE IF NOT EXISTS links (id1 int, id2 int, plot boolean default TRUE, last_update timestamptz);");

      

      txn.exec("CREATE TABLE IF NOT EXISTS bd_tx_rx_log (sensor_id int, mac varchar(20), srcip inet, dstip inet, timestamp timestamptz, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index if not exists bd_tx_rx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index if not exists bd_tx_rx_log_sensor_id_timestamp_idx on bd_tx_rx_log(sensor_id, timestamp);");
      txn.commit();

    
    }

  public:
    Cursor(const Config& config): CONN(config), connection(*this), config(config) {
      validate();
    }

    int GetSensorId() {
      {
        auto txn = connection.begin();
        for (const auto& row: connection.query(txn, get_sensor, typename CONN::params{config.sensor_name, config.dev})) {
          return row["sensor_id"].template as<int>();
        }
        txn.exec(new_sensor, typename CONN::params{config.sensor_name, config.dev});
        txn.commit();
      }
      return GetSensorId();
    }

    void UpdateSensor(int sensor_id) {
      auto txn = connection.begin();
      txn.exec(update_sensor, typename CONN::params{config.description, config.management_url, 1, 1, sensor_id});
      txn.commit();
    }

    void SerializeData(this auto& self, int sensor_id, const auto& IPs, const auto& both, const std::chrono::seconds duration, const auto now) {
      auto d = duration.count();
      auto txn = self.connection.begin();
      std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(now));
      for (const auto& entry : IPs) {
        const auto &ip = entry.first;
        const auto &data = entry.second;
        const auto &sent = data.Sent;
        const auto &recv = data.Received;
        const auto &label = data.label;
        const std::string strip = util::format_ip(ip);

        txn.exec(self.bd_tx_insert, typename CONN::params{sensor_id, label, strip, timestamp, d, sent.packet_count,
                                                          sent.total, sent.icmp, sent.udp, sent.tcp, sent.ftp,
                                                          sent.http, sent.mail, sent.p2p});
        txn.exec(self.bd_rx_insert, typename CONN::params{sensor_id, label, strip, timestamp, d, recv.packet_count,
                                                          recv.total, recv.icmp, recv.udp, recv.tcp, recv.ftp,
                                                          recv.http, recv.mail, recv.p2p});
      }



      for (auto entry : both) {
        auto srcip = entry.first.first;
        auto dstip = entry.first.second;
        auto data = entry.second;
        const std::string strsrcip = util::format_ip(srcip);
        const std::string strdstip = util::format_ip(dstip);
            
        txn.exec(self.bd_tx_rx_insert, typename CONN::params{sensor_id, data.label, strsrcip, strdstip, timestamp,
                                                             d, data.packet_count, data.total, data.icmp, data.udp,
                                                             data.tcp, data.ftp, data.http, data.mail, data.p2p});
      }

      txn.commit();
    }

    std::vector<SummaryData> GetData(const std::string_view start,
                                     const std::string_view end,
                                     bool relative_start,
                                     bool relative_end,
                                     const std::string_view sn,
                                     const std::string_view table,
                                     const std::string_view field) {
      std::vector<SummaryData> data{};
      auto txn = connection.begin();
      std::string query = std::format("select timestamp, sample_duration, sum(total) as total, sum(packet_count) as "
                                      "packet_count, sum(icmp) as icmp, sum(udp) as udp, sum(tcp) as tcp, sum(ftp) as "
                                      "ftp, sum(http) as http, sum(mail) as mail, sum(p2p) as p2p from bd_{}_log where "
                                      "{} << $1 and timestamp > ", table, field);
      if (relative_start) {
        query += "now() - $2::interval and timestamp < ";
      }
      else {
        query += "$2 and timestamp < ";
      }
      if (relative_end) {
        query += "now() - $3::interval ";
      }
      else {
        query += "$3 ";
      }
      query += "group by timestamp, sample_duration order by timestamp ;";
      for (const auto& row: txn.exec(query, {sn, start, end})) {
        SummaryData& d = data.emplace_back();
        
        d.timestamp = util::parse_timestamp(std::string{row["timestamp"].view()});

        auto sd = row["sample_duration"].template as<int64_t>();
        d.sample_duration = std::chrono::seconds(sd);
        d.net = sn;
        d.total = row[2].template as<uint64_t>();
        d.icmp = row["icmp"].template as<uint64_t>();
        d.udp = row["udp"].template as<uint64_t>();
        d.tcp = row["tcp"].template as<uint64_t>();
        d.ftp = row["ftp"].template as<uint64_t>();
        d.http = row["http"].template as<uint64_t>();
        d.mail = row["mail"].template as<uint64_t>();
        d.p2p = row["p2p"].template as<uint64_t>();
        d.count = row["packet_count"].template as<uint64_t>();
      }
      return data;
    }


  
  };

}
