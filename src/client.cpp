
#include "../include/logger.h"
#include "../include/network.h"


#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <string>
#include <thread>



static void logprint(const std::string &s) {
    Logger::log(s);
}

void listener(int sock) {
    char buf[4096];
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf)-1, 0);
        if (r <= 0) {
            logprint("Disconnected from server.");
            exit(0);
        }
        buf[r] = '\0';
        std::string s(buf);
        logprint(std::string("RECV: ") + s);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port>\n";
        return 1;
    }
    Logger::init("client_log.txt");
    std::string host = argv[1];
    unsigned short port = (unsigned short)atoi(argv[2]);

    int s = NetworkManager::connect_to(host, port);
    if (s < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        return 1;
    }
    logprint("Connected to server " + host + ":" + std::to_string(port));
    std::thread t(listener, s);
    t.detach();

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line.back() != '\n') line.push_back('\n');
        ssize_t sent = NetworkManager::send_all(s, line);
        if (sent <= 0) {
            logprint("Failed to send to server");
            break;
        }
        logprint(std::string("SENT: ") + line);
    }
    close(s);
    return 0;
}

