#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

class Logger {
public:
    // Append timestamped message to stdout and file
    static void init(const std::string &filename = "server_log.txt");
    static void log(const std::string &msg);
};

#endif // LOGGER_HPP

