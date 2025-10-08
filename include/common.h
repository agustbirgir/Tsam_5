#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>

constexpr uint8_t SOH = 0x01;
constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;

constexpr size_t MSG_LIMIT = 5000;
constexpr size_t MAX_CLIENT_BUF = 8192;

#endif // COMMON_HPP

