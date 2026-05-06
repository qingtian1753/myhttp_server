#pragma once
#include <string>

namespace SocketUtil {
    bool setSocketNonblockNoDelay(int fd);

    std::string escapeJson(const std::string& s);
}
