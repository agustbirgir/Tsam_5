#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>

namespace ProtocolHandler {
    std::string build_frame(const std::string &payload);

    void extract_frames_from_buffer(std::string &buffer, std::vector<std::string> &out);
}

#endif
