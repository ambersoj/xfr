#include "Xfr.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using json = nlohmann::ordered_json;

Xfr::Xfr(int sba)
    : mpp::Component<Xfr>(sba)
{
    regs_.sba = sba;
}

json Xfr::serialize_registers() const
{
    json j;

    // Identity
    j["component"] = regs_.component;
    j["sba"]       = regs_.sba;

    // Registers
    j["xfr_tx_open"]      = regs_.xfr_tx_open;
    j["xfr_tx_path"] = regs_.xfr_tx_path;
    j["xfr_tx_next"]   = regs_.xfr_tx_next;
    j["xfr_rx_open"]   = regs_.xfr_rx_open;
    j["xfr_rx_path"]    = regs_.xfr_rx_path;
    j["rx_seq"] = regs_.rx_seq;
    j["rx_len"] = regs_.rx_len;

    // Errors
    j["last_error"] = regs_.last_error;

    return j;
}

static int safe_to_int(const json& j, const char* key, int current)
{
    if (!j.contains(key))
        return current;

    if (j[key].is_number())
        return j[key];

    if (!j[key].is_string())
        return current;

    auto s = j[key].get<std::string>();

    if (s.empty())
        return current;

    // reject non-digits
    if (!std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); }))
        return current;

    return std::stoi(s);
}

void Xfr::apply_snapshot(const json& j)
{
    // Identity
    if (j.contains("component")) regs_.component = j["component"];
    if (j.contains("sba"))   regs_.sba   = j["sba"];

    // Control Registers
    if (j.contains("xfr_tx_open"))  regs_.xfr_tx_open   = j["xfr_tx_open"];
    if (j.contains("xfr_tx_path"))  regs_.xfr_tx_path   = j["xfr_tx_path"];
    if (j.contains("xfr_tx_next"))  regs_.xfr_tx_next   = j["xfr_tx_next"];
    if (j.contains("xfr_rx_open"))  regs_.xfr_rx_open   = j["xfr_rx_open"];
    if (j.contains("xfr_rx_path"))  regs_.xfr_rx_path   = j["xfr_rx_path"];
    regs_.rx_seq = safe_to_int(j, "rx_seq", regs_.rx_seq);
    regs_.rx_len = safe_to_int(j, "rx_len", regs_.rx_len);
    if (j.contains("rx_buffer"))
        regs_.rx_buffer = j["rx_buffer"].get<std::string>();

    // Errors
    if (j.contains("last_error"))   regs_.last_error   = j["last_error"];

    if (!j.contains("verb"))
        return;

    const std::string verb = j["verb"];

    if (verb == "GET") {
        reply_json(serialize_registers());
        return;
    }
}

void Xfr::on_message(const json&)
{
    if (regs_.xfr_tx_open) {

        fd = open(regs_.xfr_tx_path.c_str(), O_RDONLY);
        if (fd < 0) {
            regs_.last_error = "open failed";
            return;
        }

        offset = 0;
        tx_seq = 0;
        eof = false;

        struct stat st{};
        if (fstat(fd, &st) == 0)
            file_size = st.st_size;
        else
            file_size = 0;

        json out;
        out["component"] = "XFR";
        out["tx_opened"] = true;

        send_json(out, regs_.fsm_sba_);

        regs_.xfr_tx_open = false;
    }
    if (regs_.xfr_rx_open) {

        fd = open(regs_.xfr_rx_path.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC,
                0644);
        if (fd < 0) {
            regs_.last_error = "open failed";
            return;
        }

        offset = 0;
        regs_.rx_seq = 0;
        eof = false;

        struct stat st{};
        if (fstat(fd, &st) == 0)
            file_size = st.st_size;
        else
            file_size = 0;

        json out;
        out["component"] = "XFR";
        out["rx_opened"] = true;

        send_json(out, regs_.fsm_sba_);

        regs_.xfr_rx_open = false;
    }
    if (regs_.xfr_tx_next && !eof) {

        read_chunk();

        if (eof) {
            json out;
            out["component"] = "XFR";
            out["tx_eof"] = true;
            send_json(out, regs_.fsm_sba_);

            regs_.xfr_tx_next = false;
            return;
        }

        json out;
        out["component"] = "XFR";
        out["seq"] = tx_seq;
        out["len"] = chunk_len;
        out["eof"] = eof;
        out["buffer"] = std::string(buffer, chunk_len);
        out["component"] = "XFR";
        out["xfr_tx_valid"] = true;
        send_json(out, regs_.fsm_sba_);

        regs_.xfr_tx_next = false;
    }
    if (!regs_.rx_buffer.empty())
    {
        write_chunk();

        if (eof)
        {
            json j;
            j["component"] = "XFR";
            j["rx_done"] = true;
            send_json(j, regs_.fsm_sba_);
        }

        regs_.rx_buffer.clear();
    }
    if (regs_.xfr_tx_close)
    {
        if (fd >= 0)
            close(fd);

        regs_.xfr_tx_close = false;
    }
    if (regs_.xfr_rx_close)
    {
        if (fd >= 0)
            close(fd);

        regs_.xfr_rx_close = false;
    }
}

void Xfr::read_chunk()
{
    if (eof) {
        chunk_len = 0;

        json out;
        out["component"] = "XFR";
        out["tx_eof"] = true;

        send_json(out, regs_.fsm_sba_);

        return;
    }

    chunk_len = read(fd, buffer, CHUNK_SIZE);

    if (chunk_len <= 0) {
        eof = true;
        chunk_len = 0;
        return;
    }

    offset += chunk_len;
    tx_seq++;

    if (offset >= file_size) {
        eof = true;

        json out;
        out["component"] = "XFR";
        out["tx_eof"] = true;

        send_json(out, regs_.fsm_sba_);
        eof = false;
    }
}

void Xfr::write_chunk()
{
    if (regs_.rx_len <= 0)
        return;

    ssize_t written =
        write(fd,
              regs_.rx_buffer.data(),
              regs_.rx_len);

    if (written != regs_.rx_len) {
        regs_.last_error = "write failed";
        return;
    }

    offset += written;
    regs_.rx_seq++;
}
