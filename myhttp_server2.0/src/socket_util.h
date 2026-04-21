#pragma once
#include <termios.h>

namespace SocketUtil {
    bool setSocketNonblockNoDelay(int fd);
}
