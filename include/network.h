#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <string>

namespace NetworkManager {
    // Create, bind and listen on port. Returns listening socket FD or -1 on error.
    int create_listen_socket(unsigned short port, int backlog = 10);

    // Set socket non-blocking
    bool set_nonblocking(int fd);

    // Accept (non-blocking) a connection on listenfd. Returns client fd or -1/no connection.
    // If success and peer info is desired, supply &peer_ipport which will receive "ip:port".
    int accept_nonblocking(int listenfd, std::string *peer_ipport = nullptr);

    // Connect to remote host:port, returns socket FD or -1
    int connect_to(const std::string &host, unsigned short port);

    // Send all bytes; returns bytes sent or -1
    ssize_t send_all(int sockfd, const std::string &data);
}

#endif // NETWORKMANAGER_HPP

