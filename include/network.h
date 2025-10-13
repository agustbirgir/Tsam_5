#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <vector>

namespace NetworkManager {
    int create_listen_socket(unsigned short port, int backlog = 10);

    bool set_nonblocking(int fd);

    int accept_nonblocking(int listenfd, std::string *peer_ipport = nullptr);

    int connect_to(const std::string &host, unsigned short port);

    ssize_t send_all(int sockfd, const std::string &data);

    ssize_t receive(int sockfd, std::vector<char>& buffer);
}
#endif
