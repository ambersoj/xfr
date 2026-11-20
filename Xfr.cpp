#include "Xfr.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using json = nlohmann::ordered_json;

Xfr::Xfr(int sba)
    : mpp::UdpComponent<Xfr>(sba)
{
    sba_ = sba;
}

void Xfr::on_event(const json&)
{
    if (xfr_tx_open) {

        fd = open(xfr_tx_path.c_str(), O_RDONLY);
        if (fd < 0) {
            last_error = "open failed";
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

        send_json(out, fsm_sba_);

        xfr_tx_open = false;
    }
    if (xfr_rx_open) {

        fd = open(xfr_rx_path.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC,
                0644);
        if (fd < 0) {
            last_error = "open failed";
            return;
        }

        offset = 0;
        rx_seq = 0;
        eof = false;

        struct stat st{};
        if (fstat(fd, &st) == 0)
            file_size = st.st_size;
        else
            file_size = 0;

        json out;
        out["component"] = "XFR";
        out["rx_opened"] = true;

        send_json(out, fsm_sba_);

        xfr_rx_open = false;
    }
    if (xfr_tx_next && !eof) {

        read_chunk();

        if (eof) {
            json out;
            out["component"] = "XFR";
            out["eof"] = true;
            send_json(out, fsm_sba_);

            xfr_tx_next = false;
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
        send_json(out, fsm_sba_);

        xfr_tx_next = false;
    }
    if (!rx_buffer.empty())
    {
        write_chunk();

        if (eof)
        {
            json j;
            j["component"] = "XFR";
            j["rx_done"] = true;
            send_json(j, fsm_sba_);
        }

        json out;
        out["component"] = "XFR";
        out["xfr_rx_valid"] = true;
        send_json(out, fsm_sba_);

        rx_buffer.clear();
    }
    if (xfr_tx_close)
    {
        if (fd >= 0)
            close(fd);

        xfr_tx_close = false;
    }
    if (xfr_rx_close)
    {
        if (fd >= 0)
            close(fd);

        xfr_rx_close = false;
    }
}

void Xfr::read_chunk()
{
    if (eof) {
        chunk_len = 0;

        json out;
        out["component"] = "XFR";
        out["eof"] = true;

        send_json(out, fsm_sba_);

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
        out["eof"] = true;

        send_json(out, fsm_sba_);
    }
}

void Xfr::write_chunk()
{
    if (rx_len <= 0)
        return;

    ssize_t written =
        write(fd,
              rx_buffer.data(),
              rx_len);

    if (written != rx_len) {
        last_error = "write failed";
        return;
    }

    offset += written;
    rx_seq++;
}
