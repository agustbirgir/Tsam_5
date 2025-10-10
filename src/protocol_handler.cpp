#include "../include/protocol.h"
#include "../include/common.h":


#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ProtocolHandler {

std::string build_frame(const std::string &payload) {
    // total length includes the 5 framing bytes
    uint16_t total_length = (uint16_t)(5 + payload.size());
    uint16_t netlen = htons(total_length);

    std::string out;
    out.reserve(5 + payload.size());
    out.push_back((char)SOH);
    // push netlen as two bytes (big-endian)
    out.push_back((char)((netlen >> 8) & 0xFF));
    out.push_back((char)(netlen & 0xFF));
    out.push_back((char)STX);
    out += payload;
    out.push_back((char)ETX);
    return out;
}

void extract_frames_from_buffer(std::string &buffer, std::vector<std::string> &out) {
    // buffer contains arbitrary bytes. We search for frames of the structure:
    // [SOH][len_hi][len_lo][STX][payload...][ETX], where len is network-order and includes 5 framing bytes.
    size_t pos = 0;
    while (true) {
        if (buffer.size() < 1) return;
        // find SOH
        size_t soh = buffer.find((char)SOH, pos);
        if (soh == std::string::npos) {
            // no SOH: drop everything
            buffer.clear();
            return;
        }
        // ensure enough bytes for header (SOH + 2 len + STX)
        if (buffer.size() < soh + 4) {
            // incomplete header
            if (soh > 0) buffer.erase(0, soh);
            return;
        }
        // read length bytes
        uint8_t b1 = (uint8_t)buffer[soh + 1];
        uint8_t b2 = (uint8_t)buffer[soh + 2];
        uint16_t netlen = (uint16_t)((b1 << 8) | b2);
        uint16_t length = ntohs(netlen);
        // validate minimal size
        if (length < 5) {
            // malformed: skip this SOH and continue
            pos = soh + 1;
            continue;
        }
        if (buffer.size() < soh + length) {
            // wait for more
            if (soh > 0) buffer.erase(0, soh); // keep SOH at 0
            return;
        }
        // check STX and ETX
        if ((uint8_t)buffer[soh + 3] != STX || (uint8_t)buffer[soh + length - 1] != ETX) {
            // malformed: skip SOH
            pos = soh + 1;
            continue;
        }
        // payload between soh+4 .. soh+length-2 inclusive (length-5 bytes)
        size_t payload_start = soh + 4;
        size_t payload_len = length - 5;
        std::string payload;
        if (payload_len > 0) payload = buffer.substr(payload_start, payload_len);
        else payload = "";
        out.push_back(payload);
        // remove consumed bytes
        buffer.erase(0, soh + length);
        // reset pos
        pos = 0;
    }
}

} // namespace ProtocolHandler

