#include "../include/common.h"
#include "../include/logger.h"
#include "../include/network.h"
#include "../include/protocol.h"

#include <sys/select.h>
#include <unistd.h>
#include <algorithm>
#include <chrono> 
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

// Connection struct holds info about each connected socket
struct ConnInfo {
    int sock;
    enum Type { UNKNOWN = 0, CLIENT = 1, SERVERPEER = 2 } type;
    std::string peer_addr;
    std::string peer_group;
    std::string recvbuf;
    ConnInfo() : sock(-1), type(UNKNOWN) {}
};

// --- Global State ---
static std::map<int, ConnInfo> conns;
static std::map<std::string, std::vector<std::string>> msgs_for_group;
static unsigned short g_listen_port = 0;
static std::string g_group_id = "A5_117"; // Your group ID

// --- Forward Declarations ---
static void handle_payload(int sock, const std::string& payload, bool is_framed);
static void forward_frame_to_peers(int origin_sock, const std::string& frame);

// Helper function to build the SERVERS response string
static std::string build_SERVERS_response() {
    std::ostringstream ss;
    ss << "SERVERS";
    // Assuming 0.0.0.0 is not helpful, let's just send our group and port.
    // Other servers will know our IP from the socket connection itself.
    ss << "," << g_group_id << ",0.0.0.0," << g_listen_port;

    for (auto const& [sock, ci] : conns) {
        if (ci.type != ConnInfo::SERVERPEER || ci.peer_group.empty()) continue;
        std::string ip = ci.peer_addr;
        std::string ip_only = ip;
        std::string port = "0";
        auto p = ip.find(':');
        if (p != std::string::npos) {
            ip_only = ip.substr(0, p);
            port = ip.substr(p + 1);
        }
        ss << ";" << ci.peer_group << "," << ip_only << "," << port;
    }
    return ss.str();
}

// Helper to add a new connection to our list
static void add_new_socket_to_master(int sock, fd_set* master, int* maxfd, const std::string& peer_addr) {
    FD_SET(sock, master);
    if (sock > *maxfd) *maxfd = sock;
    ConnInfo ci;
    ci.sock = sock;
    ci.type = ConnInfo::UNKNOWN; // We don't know the type until they send a command
    ci.peer_addr = peer_addr;
    conns[sock] = ci;
    Logger::log("Registered new connection from " + peer_addr + " on sock " + std::to_string(sock));
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <listen_port> <group_id> [peer1_ip:port] [peer2_ip:port] ...\n", argv[0]);
        return 1;
    }
    g_listen_port = (unsigned short)atoi(argv[1]);
    g_group_id = argv[2];

    Logger::init("server_log.txt");
    Logger::log("Starting server for group: " + g_group_id + " on port " + std::to_string(g_listen_port));

    int listenfd = NetworkManager::create_listen_socket(g_listen_port);
    if (listenfd < 0) {
        Logger::log("Fatal: Failed to create listen socket");
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listenfd, &master);
    int maxfd = listenfd;

    // Connect to peers from command line
    for (int i = 3; i < argc; ++i) {
        std::string peer_str = argv[i];
        auto colon_pos = peer_str.find(':');
        if (colon_pos == std::string::npos) {
            Logger::log("Skipping invalid peer address: " + peer_str);
            continue;
        }
        std::string host = peer_str.substr(0, colon_pos);
        unsigned short port = (unsigned short)atoi(peer_str.substr(colon_pos + 1).c_str());

        Logger::log("Attempting to connect to peer " + host + ":" + std::to_string(port));
        int peer_sock = NetworkManager::connect_to(host, port);
        if (peer_sock >= 0) {
            add_new_socket_to_master(peer_sock, &master, &maxfd, peer_str);
            // We are a server, so we identify with HELO right away
            conns[peer_sock].type = ConnInfo::SERVERPEER;
            std::string helo_payload = "HELO," + g_group_id;
            std::string frame = ProtocolHandler::build_frame(helo_payload);
            NetworkManager::send_all(peer_sock, frame);
            Logger::log("Successfully connected to peer and sent HELO.");
        } else {
            Logger::log("Failed to connect to peer " + peer_str);
        }
    }
    
    // Timer for KEEPALIVE
    auto last_keepalive_time = std::chrono::steady_clock::now();

    while (true) {
        fd_set readfs = master;
        
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int sel = select(maxfd + 1, &readfs, nullptr, nullptr, &timeout);
        if (sel < 0) {
            if (errno == EINTR) continue;
            Logger::log("select() error, exiting");
            break;
        }
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_keepalive_time).count() >= 60) {
            Logger::log("Sending KEEPALIVE to peers.");
            for(auto const& [sock, ci] : conns) {
                if (ci.type == ConnInfo::SERVERPEER) {
                    int msg_count = 0;
                    if (!ci.peer_group.empty() && msgs_for_group.count(ci.peer_group)) {
                        msg_count = msgs_for_group.at(ci.peer_group).size();
                    }
                    std::string payload = "KEEPALIVE," + std::to_string(msg_count);
                    std::string frame = ProtocolHandler::build_frame(payload);
                    NetworkManager::send_all(sock, frame);
                }
            }
            last_keepalive_time = now;
        }

        if (FD_ISSET(listenfd, &readfs)) {
            std::string peer_ip;
            int c = NetworkManager::accept_nonblocking(listenfd, &peer_ip);
            if (c >= 0) {
                add_new_socket_to_master(c, &master, &maxfd, peer_ip);
            }
        }

        std::vector<int> to_erase;
        for (auto& pair : conns) {
            int s = pair.first;
            ConnInfo& ci = pair.second;

            if (FD_ISSET(s, &readfs)) {
                std::vector<char> received_data;
                ssize_t r = NetworkManager::receive(s, received_data);

                if (r <= 0) {
                    if (r == 0) Logger::log("Connection closed by peer: " + ci.peer_addr);
                    else Logger::log(std::string("recv error on sock ") + std::to_string(s) + ": " + strerror(errno));
                    to_erase.push_back(s);
                    continue;
                }

                ci.recvbuf.append(received_data.begin(), received_data.end());

                // --- FIXED RECEIVE LOGIC ---
                // First, try to extract standard framed messages (for server-to-server)
                std::vector<std::string> framed_payloads;
                ProtocolHandler::extract_frames_from_buffer(ci.recvbuf, framed_payloads);
                for (const auto& pl : framed_payloads) {
                    handle_payload(s, pl, true);
                }

                // Now, check for simple, newline-terminated commands (for our simple client)
                size_t newline_pos;
                while ((newline_pos = ci.recvbuf.find('\n')) != std::string::npos) {
                    std::string client_payload = ci.recvbuf.substr(0, newline_pos);
                    if (!client_payload.empty() && client_payload.back() == '\r') {
                        client_payload.pop_back();
                    }
                    
                    if (!client_payload.empty()) {
                        handle_payload(s, client_payload, false);
                    }
                    ci.recvbuf.erase(0, newline_pos + 1);
                }
            }
        }

        for (int s : to_erase) {
            close(s);
            FD_CLR(s, &master);
            conns.erase(s);
        }
    }

    for (auto const& [sock, conn_info] : conns) close(sock);
    close(listenfd);
    return 0;
}


// --- Main Command Handler ---
static void handle_payload(int sock, const std::string& payload, bool is_framed) {
    Logger::log("Payload from sock " + std::to_string(sock) + " (framed: " + (is_framed ? "yes" : "no") + "): " + payload);
    std::vector<std::string> tokens;
    std::istringstream ss(payload);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tokens.push_back(tok);
    }
    if (tokens.empty()) return;

    std::string command = tokens[0];
    ConnInfo& ci = conns.at(sock);

    if (command == "HELO") {
        ci.type = ConnInfo::SERVERPEER;
        ci.peer_group = (tokens.size() >= 2 ? tokens[1] : "unknown");
        Logger::log("Peer " + ci.peer_group + " said HELO from " + ci.peer_addr);
        std::string resp = build_SERVERS_response();
        NetworkManager::send_all(sock, ProtocolHandler::build_frame(resp));
    }
    else if (command == "SERVERS") {
        Logger::log("Received SERVERS list from peer: " + payload);
    }
    else if (command == "KEEPALIVE") {
         Logger::log("Received KEEPALIVE from " + ci.peer_group);
    }
    else if (command == "SENDMSG") {
        // --- THIS IS THE CORRECTED LOGIC ---

        std::string to_group, from_group, content, full_payload;

        // Case 1: Message from a client (not framed)
        if (!is_framed && tokens.size() >= 2) {
            if (ci.type == ConnInfo::UNKNOWN) ci.type = ConnInfo::CLIENT;
            
            to_group = tokens[1];
            from_group = g_group_id; // The server's own group is the sender

            // Reconstruct message content from the payload
            auto first_comma = payload.find(',');
            auto second_comma = payload.find(',', first_comma + 1);
            content = payload.substr(second_comma + 1);

            full_payload = "SENDMSG," + to_group + "," + from_group + "," + content;
            Logger::log("Received SENDMSG from client. Full command: " + full_payload);
        }
        // Case 2: Message from another server (framed)
        else if (is_framed && tokens.size() >= 4) {
            to_group = tokens[1];
            from_group = tokens[2];
            
            auto first_comma = payload.find(',');
            auto second_comma = payload.find(',', first_comma + 1);
            auto third_comma = payload.find(',', second_comma + 1);
            content = payload.substr(third_comma + 1);
            full_payload = payload; // Already in the correct format
        } else {
            Logger::log("Malformed SENDMSG command: " + payload);
            return;
        }

        // Now, process the unified, full_payload
        if (content.size() > MSG_LIMIT) content.resize(MSG_LIMIT);

        if (to_group == g_group_id) {
            msgs_for_group[to_group].push_back(from_group + "|" + content);
            Logger::log("Stored message for my group (" + to_group + ") from " + from_group);
        } else {
            Logger::log("Forwarding message for " + to_group + " from " + from_group);
            std::string forward_frame = ProtocolHandler::build_frame(full_payload);
            forward_frame_to_peers(sock, forward_frame);
        }
    }
    else if (command == "STATUSREQ") {
        if (ci.type == ConnInfo::UNKNOWN) { ci.type = ConnInfo::CLIENT; }
        std::ostringstream resp_ss;
        resp_ss << "STATUSRESP";
        for (auto const& [group, msg_list] : msgs_for_group) {
            if (!msg_list.empty()) {
                resp_ss << "," << group << "," << msg_list.size();
            }
        }
        std::string response_str = resp_ss.str();
        Logger::log("Responding to STATUSREQ with: " + response_str);

        if (ci.type == ConnInfo::CLIENT) {
             NetworkManager::send_all(sock, response_str + "\n");
        } else {
             NetworkManager::send_all(sock, ProtocolHandler::build_frame(response_str));
        }
    }
    else if (command == "GETMSGS") {
        if(tokens.size() >= 2) {
            std::string requested_group = tokens[1];
            Logger::log("Peer " + ci.peer_group + " is requesting messages for group " + requested_group);
            auto it = msgs_for_group.find(requested_group);
            if (it != msgs_for_group.end()) {
                for(const auto& msg_entry : it->second) {
                    auto pipe_pos = msg_entry.find('|');
                    std::string from_group = msg_entry.substr(0, pipe_pos);
                    std::string content = msg_entry.substr(pipe_pos + 1);
                    std::string msg_payload = "SENDMSG," + requested_group + "," + from_group + "," + content;
                    NetworkManager::send_all(sock, ProtocolHandler::build_frame(msg_payload));
                }
                it->second.clear();
            }
        }
    }
    else if (command == "GETMSG") {
        ci.type = ConnInfo::CLIENT;
        std::string response_payload;
        auto it = msgs_for_group.find(g_group_id);
        if (it != msgs_for_group.end() && !it->second.empty()) {
            std::string entry = it->second.front();
            it->second.erase(it->second.begin());
            
            auto pipe_pos = entry.find('|');
            std::string from_group = entry.substr(0, pipe_pos);
            std::string content = entry.substr(pipe_pos + 1);
            response_payload = "MSG," + from_group + "," + content;
            Logger::log("Delivering message to client from " + from_group);
        } else {
            response_payload = "NO_MSG";
        }
        NetworkManager::send_all(sock, response_payload + "\n");
    } else if (command == "LISTSERVERS") {
        ci.type = ConnInfo::CLIENT;
        std::string response = build_SERVERS_response();
        NetworkManager::send_all(sock, response + "\n");
    } else {
        Logger::log("Unknown command received: " + payload);
    }
}

static void forward_frame_to_peers(int origin_sock, const std::string& frame) {
    for (auto const& [peer_sock, ci] : conns) {
        if (ci.type == ConnInfo::SERVERPEER && peer_sock != origin_sock) {
            NetworkManager::send_all(peer_sock, frame);
        }
    }
}
