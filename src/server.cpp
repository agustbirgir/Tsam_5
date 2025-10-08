// src/server.cpp
// Server entrypoint (refactored). Uses NetworkManager, ProtocolHandler, Logger.

#include "common.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>   // strerror
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct ConnInfo {
    int sock;
    enum Type { UNKNOWN=0, CLIENT=1, SERVERPEER=2 } type;
    std::string peer_addr;   // ip:port as observed
    std::string peer_group;  // group string learned from HELO
    std::string recvbuf;     // accumulated bytes
    ConnInfo(): sock(-1), type(UNKNOWN) {}
};

static std::map<int, ConnInfo> conns;
static std::map<std::string, std::vector<std::string>> msgs_for_group;
static unsigned short g_listen_port = 0;
static std::string g_group_id = "A117 1";

static std::string get_peer_ipport(int sock) {
    auto it = conns.find(sock);
    if (it != conns.end()) return it->second.peer_addr;
    return "unknown";
}

static std::string build_SERVERS_response(const ConnInfo &request_conn) {
    std::ostringstream ss;
    ss << "SERVERS";
    // include remote (the observed peer) first
    std::string peerip = request_conn.peer_addr;
    std::string peerip_only = peerip;
    std::string peerport = "0";
    auto pos = peerip.find(':');
    if (pos != std::string::npos) {
        peerip_only = peerip.substr(0, pos);
        peerport = peerip.substr(pos+1);
    }
    ss << "," << (request_conn.peer_group.empty()? "unknown" : request_conn.peer_group)
       << "," << peerip_only << "," << peerport;

    // include this server (advertise 0.0.0.0 and our listen port)
    ss << ";" << g_group_id << ",0.0.0.0," << g_listen_port;

    // include other server peers
    for (auto &kv : conns) {
        const ConnInfo &ci = kv.second;
        if (ci.type != ConnInfo::SERVERPEER) continue;
        if (ci.sock == request_conn.sock) continue;
        std::string ip = ci.peer_addr;
        std::string ip_only = ip;
        std::string port = "0";
        auto p = ip.find(':');
        if (p != std::string::npos) { ip_only = ip.substr(0,p); port = ip.substr(p+1); }
        ss << ";" << (ci.peer_group.empty()? "unknown" : ci.peer_group) << "," << ip_only << "," << port;
    }
    return ss.str();
}

static void handle_server_payload(int sock, const std::string &payload) {
    Logger::log("Server payload received on sock " + std::to_string(sock) + ": " + payload);
    // split by comma (but message content may contain commas -> reconstruct later)
    std::vector<std::string> tokens;
    {
        std::istringstream ss(payload);
        std::string tok;
        while (std::getline(ss, tok, ',')) tokens.push_back(tok);
    }
    if (tokens.empty()) return;
    std::string verb = tokens[0];
    if (verb == "HELO") {
        std::string from = (tokens.size()>=2 ? tokens[1] : "unknown");
        conns[sock].peer_group = from;
        std::string resp = build_SERVERS_response(conns[sock]);
        std::string frame = ProtocolHandler::build_frame(resp);
        NetworkManager::send_all(sock, frame);
        Logger::log("Replied SERVERS to " + conns[sock].peer_addr + " (" + from + ")");
    } else if (verb == "SENDMSG") {
        // SENDMSG,<TO>,<FROM>,<content...>
        if (tokens.size() >= 4) {
            std::string to = tokens[1];
            std::string from = tokens[2];
            size_t pos = payload.find(tokens[3]);
            std::string content = (pos==std::string::npos? std::string() : payload.substr(pos));
            if (content.size() > MSG_LIMIT) content.resize(MSG_LIMIT);
            std::string stored = from + "|" + content;
            msgs_for_group[to].push_back(stored);
            Logger::log("Stored message for " + to + " from " + from + " (via peer SENDMSG)");
        } else {
            Logger::log("Malformed SENDMSG from peer");
        }
    } else if (verb == "GETMSGS") {
        if (tokens.size() >= 2) {
            std::string which = tokens[1];
            size_t n = msgs_for_group[which].size();
            std::ostringstream ss; ss << "STATUSRESP," << g_group_id << "," << n;
            std::string frame = ProtocolHandler::build_frame(ss.str());
            NetworkManager::send_all(sock, frame);
        }
    } else if (verb == "STATUSREQ") {
        std::ostringstream ss;
        ss << "STATUSRESP";
        for (auto &kv : msgs_for_group) {
            ss << "," << kv.first << "," << kv.second.size();
        }
        std::string frame = ProtocolHandler::build_frame(ss.str());
        NetworkManager::send_all(sock, frame);
    } else if (verb == "KEEPALIVE") {
        Logger::log("KEEPALIVE from peer: " + payload);
    } else {
        Logger::log("Unknown server command: " + payload);
    }
}

static void forward_sendmsg_to_peers(const std::string &payload) {
    std::string frame = ProtocolHandler::build_frame(payload);
    for (auto &kv : conns) {
        ConnInfo &ci = kv.second;
        if (ci.type != ConnInfo::SERVERPEER) continue;
        ssize_t s = NetworkManager::send_all(ci.sock, frame);
        if (s <= 0) {
            Logger::log("Forward to peer failed: " + ci.peer_addr);
        } else {
            Logger::log("Forwarded SENDMSG to " + ci.peer_addr);
        }
    }
}

static void handle_client_command(int sock, const std::string &line) {
    Logger::log("Client command from " + get_peer_ipport(sock) + ": " + line);
    std::vector<std::string> parts;
    {
        std::istringstream ss(line);
        std::string t;
        while (std::getline(ss, t, ',')) parts.push_back(t);
    }
    if (parts.empty()) return;
    std::string verb = parts[0];
    if (verb == "SENDMSG") {
        if (parts.size() >= 3) {
            std::string to = parts[1];
            size_t pos = line.find(parts[2]);
            std::string content = (pos==std::string::npos? std::string() : line.substr(pos));
            if (content.size() > MSG_LIMIT) { content.resize(MSG_LIMIT); Logger::log("Truncated message to MSG_LIMIT"); }
            std::string stored = g_group_id + "|" + content;
            msgs_for_group[to].push_back(stored);
            std::ostringstream pl;
            pl << "SENDMSG," << to << "," << g_group_id << "," << content;
            forward_sendmsg_to_peers(pl.str());
            NetworkManager::send_all(sock, std::string("OK,SENDMSG stored and forwarded\n"));
        } else {
            NetworkManager::send_all(sock, std::string("ERR,Usage: SENDMSG,GROUPID,<message>\n"));
        }
    } else if (verb == "GETMSG") {
        auto it = msgs_for_group.find(g_group_id);
        if (it != msgs_for_group.end() && !it->second.empty()) {
            std::string entry = it->second.front();
            it->second.erase(it->second.begin());
            size_t p = entry.find('|');
            std::string from = (p==std::string::npos? "unknown" : entry.substr(0,p));
            std::string content = (p==std::string::npos? entry : entry.substr(p+1));
            std::ostringstream out;
            out << "MSG," << from << "," << content << "\n";
            NetworkManager::send_all(sock, out.str());
            Logger::log("Delivered 1 message to local client from " + from);
        } else {
            NetworkManager::send_all(sock, std::string("NO_MSG\n"));
        }
    } else if (verb == "LISTSERVERS") {
        std::ostringstream out;
        out << "SERVERS_LIST";
        for (auto &kv : conns) {
            ConnInfo &ci = kv.second;
            if (ci.type == ConnInfo::SERVERPEER) {
                out << "," << (ci.peer_group.empty()? "unknown" : ci.peer_group) << ":" << ci.peer_addr;
            }
        }
        out << "\n";
        NetworkManager::send_all(sock, out.str());
    } else {
        NetworkManager::send_all(sock, std::string("ERR,unknown command\n"));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <listen_port> [group_id]\n", argv[0]);
        return 1;
    }
    g_listen_port = (unsigned short)atoi(argv[1]);
    if (argc >= 3) g_group_id = argv[2];

    Logger::init("server_log.txt");
    Logger::log("Starting server for group: " + g_group_id + " on port " + std::to_string(g_listen_port));

    int listenfd = NetworkManager::create_listen_socket(g_listen_port);
    if (listenfd < 0) {
        Logger::log("Failed to create listen socket");
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listenfd, &master);
    int maxfd = listenfd;

    while (true) {
        fd_set readfs = master;
        int sel = select(maxfd+1, &readfs, nullptr, nullptr, nullptr);
        if (sel < 0) {
            if (errno == EINTR) continue;
            Logger::log("select() error, exiting");
            break;
        }
        // accept
        if (FD_ISSET(listenfd, &readfs)) {
            std::string peer_ip;
            int c = NetworkManager::accept_nonblocking(listenfd, &peer_ip);
            if (c >= 0) {
                ConnInfo ci;
                ci.sock = c;
                ci.type = ConnInfo::UNKNOWN;
                ci.peer_addr = peer_ip;
                conns[c] = ci;
                FD_SET(c, &master);
                if (c > maxfd) maxfd = c;
                Logger::log("Accepted connection from " + peer_ip + " sock=" + std::to_string(c));
            }
        }
        // iterate conns
        std::vector<int> to_erase;
        for (auto it = conns.begin(); it != conns.end(); ++it) {
            int s = it->first;
            ConnInfo &ci = it->second;
            if (!FD_ISSET(s, &readfs)) continue;
            char buf[4096];
            ssize_t r = recv(s, buf, sizeof(buf), 0);
            if (r == 0) {
                Logger::log("Connection closed by peer: " + ci.peer_addr);
                close(s);
                FD_CLR(s, &master);
                to_erase.push_back(s);
                continue;
            } else if (r < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                Logger::log(std::string("recv error: ") + strerror(errno));
                close(s);
                FD_CLR(s, &master);
                to_erase.push_back(s);
                continue;
            }
            ci.recvbuf.append(buf, buf + r);

            if (ci.type == ConnInfo::UNKNOWN) {
                if (!ci.recvbuf.empty() && (uint8_t)ci.recvbuf[0] == SOH) {
                    ci.type = ConnInfo::SERVERPEER;
                    Logger::log("Marked as SERVERPEER: " + ci.peer_addr);
                } else {
                    ci.type = ConnInfo::CLIENT;
                    Logger::log("Marked as CLIENT: " + ci.peer_addr);
                }
            }

            if (ci.type == ConnInfo::SERVERPEER) {
                std::vector<std::string> payloads;
                ProtocolHandler::extract_frames_from_buffer(ci.recvbuf, payloads);
                for (auto &pl : payloads) handle_server_payload(s, pl);
            } else if (ci.type == ConnInfo::CLIENT) {
                size_t pos;
                while ((pos = ci.recvbuf.find('\n')) != std::string::npos) {
                    std::string line = ci.recvbuf.substr(0, pos);
                    ci.recvbuf.erase(0, pos+1);
                    handle_client_command(s, line);
                }
                if (ci.recvbuf.size() > MAX_CLIENT_BUF) {
                    ci.recvbuf.clear();
                    Logger::log("Cleared oversized client buffer for " + ci.peer_addr);
                }
            }
        }
        for (int s : to_erase) conns.erase(s);
    }

    for (auto &kv : conns) close(kv.first);
    close(listenfd);
    return 0;
}

