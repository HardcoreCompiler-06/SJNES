#pragma once
#include <string>
#include <vector>

// Buffer toàn cục, Mapper ghi vào, UI đọc ra
inline std::vector<std::string> gLogBuffer;

inline void LogPrint(const std::string& msg) {
    gLogBuffer.push_back(msg);
}