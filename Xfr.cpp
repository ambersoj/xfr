#include "Xfr.hpp"

using json = nlohmann::ordered_json;

Xfr::Xfr(int sba)
    : mpp::Component<Xfr>(sba)
{
    regs_.sba = sba;
}

json Xfr::serialize_registers() const
{
    json j;

    j["component"] = regs_.component;
    j["sba"]       = regs_.sba;

    j["mode"]      = regs_.mode;
    j["file_path"] = regs_.file_path;
    j["peer_id"]   = regs_.peer_id;

    j["chunk_size"]    = regs_.chunk_size;

    j["chunk_payload"]  = regs_.chunk_payload;

    j["send_done"] = regs_.send_done;
    j["recv_done"] = regs_.recv_done;

    j["advance"]   = regs_.advance;

    j["last_error"] = regs_.last_error;

    return j;
}

void Xfr::apply_snapshot(const json& j)
{
    if (j.value("tick", false)) {
        on_tick();
        return;
    }
    // --- Intent-aware path ---
    if (j.contains("verb")) {
        const std::string verb = j["verb"];

        if (verb == "GET") {
            reply_json(serialize_registers());
            return;
        }
        if (verb == "PUT") {
            if (j.value("resource","") == "xfr" && j.contains("body")) {
                const auto& body = j["body"];

            }
            return;
        }
    }
    // --- Legacy path (unchanged) ---
    legacy_apply_snapshot(j);
}

void Xfr::legacy_apply_snapshot(const json& j)
{
    if (j.contains("mode")) regs_.mode = j["mode"];
    if (j.value("advance", false)) regs_.advance = true;
    if (j.contains("mode") && j["mode"] == "send") {
        regs_.offset = 0;
        regs_.chunk_index = 0;
        regs_.eof = false;
        regs_.send_done = false;
    }
}

void Xfr::on_message(const json&)
{
}

void Xfr::on_tick()
{
    if (!regs_.advance)
        return;

    regs_.offset += regs_.chunk_size;
    regs_.chunk_index++;

    if (regs_.offset >= regs_.file_size)
        regs_.eof = true;

    regs_.chunk_payload =
        "CHUNK#" + std::to_string(regs_.chunk_index);

    regs_.advance = false;

    if (regs_.eof) {
        commit("XFR.eof", true);
    } else {
        commit("XFR.chunk_ready", true);
    }
}
