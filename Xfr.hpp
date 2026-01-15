#pragma once

#include "Belief.hpp"
#include "Component.hpp"

#include <map>
#include <vector>
#include <string>
#include <netinet/in.h>

using json = nlohmann::ordered_json;

struct XfrRegisters
{
  std::string component = "XFR";
  int sba = 4004;

  // identity / intent
  std::string mode = "idle";            // idle | send | recv
  std::string file_path = "";
  uint64_t file_size = 0;
  std::string peer_id = "";
  
  // chunking
  int chunk_size = 512;
  uint64_t offset = 0;
  uint32_t chunk_index = 0;
  bool eof = false;
  std::string chunk_payload = "";

  // progress
  bool send_done = false;
  bool recv_done =false;

  // control
  bool advance = false;

  // errors
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
    void legacy_apply_snapshot(const mpp::json& j);
    void on_message(const mpp::json& j);
    void publish_snapshot() {}
    void Xfr::on_tick();

private:
    XfrRegisters    regs_;

};
