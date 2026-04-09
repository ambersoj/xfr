#pragma once

#include "Component.hpp"

#define CHUNK_SIZE 1024

using json = nlohmann::ordered_json;

struct XfrRegisters
{
  // Identity Registers
  std::string component = "XFR";
  int sba = 0;
  int fsm_sba_ = 5000;

  // Control Registers:
  bool xfr_tx_open = false;
  bool xfr_tx_close = false;
  std::string xfr_tx_path = "/usr/local/mpp/tx-file.txt";
  bool xfr_tx_next = false;
  bool xfr_rx_open = false;
  bool xfr_rx_close = false;
  std::string xfr_rx_path = "/usr/local/mpp/rx-file.txt";
  int rx_seq = 0;
  std::string rx_buffer = "";
  int rx_len = 0;

  // Errors
  std::string last_error = "";
};

class Xfr : public mpp::Component<Xfr>
{
public:
    explicit Xfr(int sba);

    // MPP interface
    const char* component_name() const override { return "XFR"; }
    json serialize_registers() const;
    void apply_snapshot(const mpp::json& j);
    void on_message(const mpp::json& j);

private:
    // file handling
    int fd = 0;
    int tx_seq = 0;
    int chunk_len = 0;
    int offset = 0;
    int file_size = 0;
    bool eof = false;
    char buffer[65536]{};

    XfrRegisters    regs_;

    void read_chunk();
    void write_chunk();

};
