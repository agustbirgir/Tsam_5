#ifndef COMMON_H
#define COMMON_H

#include <cstddef>

// Other commonly used C++ standard library headers
#include <string>
#include <vector>


// ASCII control characters for the protocol as per the assignment
constexpr char SOH = 0x01;
constexpr char STX = 0x02;
constexpr char ETX = 0x03;

// Message and buffer size limits
constexpr size_t MSG_LIMIT = 5000;
constexpr size_t MAX_CLIENT_BUF = 8192;

#endif // COMMON_H
