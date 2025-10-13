#include "../include/protocol.h"
#include "../include/common.h"


#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ProtocolHandler {

std::string build_frame(const std::string &payload) {
    uint16_t total_length = (uint16_t)(5 + payload.size());
    uint16_t netlen = htons(total_length);

    std::string out;
    out.reserve(5 + payload.size());
    out.push_back((char)SOH);
    out.push_back((char)((netlen >> 8) & 0xFF));
    out.push_back((char)(netlen & 0xFF));
    out.push_back((char)STX);
    out += payload;
    out.push_back((char)ETX);
    return out;
}

void extract_frames_from_buffer(std::string &buffer, std::vector<std::string> &out) {
    size_t pos = 0;
    while (true) {
        if (buffer.size() < 1) return;
        
        size_t soh = buffer.find((char)SOH, pos);
        if (soh == std::string::npos) {
            return;
        }
        
        if (buffer.size() < soh + 4) {
            if (soh > 0) buffer.erase(0, soh);
            return;
        }

        uint8_t b1 = (uint8_t)buffer[soh + 1];
        uint8_t b2 = (uint8_t)buffer[soh + 2];
        uint16_t netlen = (uint16_t)((b1 << 8) | b2);
        uint16_t length = ntohs(netlen);

        if (length < 5) {
            pos = soh + 1;
            continue;
        }
        
        if (buffer.size() < soh + length) {
            if (soh > 0) buffer.erase(0, soh);
            return;
        }
        
        if ((uint8_t)buffer[soh + 3] != STX || (uint8_t)buffer[soh + length - 1] != ETX) {
            pos = soh + 1;
            continue;
        }

        size_t payload_start = soh + 4;
        size_t payload_len = length - 5;
        std::string payload = (payload_len > 0) ? buffer.substr(payload_start, payload_len) : "";
        out.push_back(payload);
        
        buffer.erase(0, soh + length);
        pos = 0;
    }
}

} 
