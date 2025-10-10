#include "../include/common.h"
#include "../include/network.h"


#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <string>

// Helper definitions
namespace NetworkManager {

int create_listen_socket(unsigned short port, int backlog) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    if (listen(s, backlog) < 0) {
        close(s);
        return -1;
    }
    set_nonblocking(s);
    return s;
}

bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    return true;
}

int accept_nonblocking(int listenfd, std::string *peer_ipport) {
    sockaddr_storage sa;
    socklen_t sl = sizeof(sa);
    int c = accept(listenfd, (sockaddr*)&sa, &sl);
    if (c < 0) return -1;
    set_nonblocking(c);
    if (peer_ipport) {
        char host[NI_MAXHOST], serv[NI_MAXSERV];
        if (getnameinfo((sockaddr*)&sa, sl, host, sizeof(host), serv, sizeof(serv),
                        NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            *peer_ipport = std::string(host) + ":" + std::string(serv);
        } else {
            *peer_ipport = "unknown";
        }
    }
    return c;
}

int connect_to(const std::string &host, unsigned short port) {
    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%u", port);
    int err = getaddrinfo(host.c_str(), portstr, &hints, &res);
    if (err != 0) return -1;
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { freeaddrinfo(res); return -1; }
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        close(s);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return s;
}

ssize_t send_all(int sockfd, const std::string &data) {
    size_t total = 0;
    const char *buf = data.data();
    size_t len = data.size();
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t receive(int sockfd, std::vector<char>& buffer) {
    char temp_buf[4096];
    ssize_t bytes_read = recv(sockfd, temp_buf, sizeof(temp_buf), 0);

    if (bytes_read > 0) {
        buffer.insert(buffer.end(), temp_buf, temp_buf + bytes_read);
    }

    return bytes_read;
}

}

