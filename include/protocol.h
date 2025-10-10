#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>

namespace ProtocolHandler {
    // Build framed message according to spec: <SOH><length(2)><STX><payload><ETX>
    std::string build_frame(const std::string &payload);

    // Extract complete payloads from a binary buffer. Found payloads appended to out.
    // The buffer is modified (consumed).
    void extract_frames_from_buffer(std::string &buffer, std::vector<std::string> &out);
}

#endif // PROTOCOL_H
