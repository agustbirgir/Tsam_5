#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>

constexpr char SOH = 0x01;
constexpr char STX = 0x02;
constexpr char ETX = 0x03;
