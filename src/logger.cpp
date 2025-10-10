#include "../include/logger.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

static std::string g_logfile = "server_log.txt";
static std::mutex g_log_mtx;

void Logger::init(const std::string &filename) {
    std::lock_guard<std::mutex> lk(g_log_mtx);
    g_logfile = filename;
    // create/truncate file once
    std::ofstream ofs(g_logfile, std::ios::app);
    (void)ofs;
}

void Logger::log(const std::string &msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmbuf;
#if defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif
    char timebuf[64];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tmbuf);

    std::ostringstream ss;
    ss << "[" << timebuf << "] " << msg << "\n";
    std::string out = ss.str();

    // stdout
    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        std::cout << out;
        std::ofstream ofs(g_logfile, std::ios::app);
        if (ofs) ofs << out;
    }
}

