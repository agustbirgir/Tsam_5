#ifndef COMMON_H
#define COMMON_H

#include <cstddef>
#include <string>
#include <vector>


constexpr char SOH = 0x01;
constexpr char STX = 0x02;
constexpr char ETX = 0x03;

constexpr size_t MSG_LIMIT = 5000;
constexpr size_t MAX_CLIENT_BUF = 8192;

#endif
