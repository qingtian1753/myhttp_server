#include "socket_util.h"
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace SocketUtil {

bool setSocketNonblockNoDelay(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;

    int yes = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != -1;
}


//正确地将字符串转化为json字符串
std::string escapeJson(const std::string& s)
{
    std::string out;
    for (char ch : s) {
        switch (ch) {
            //如果是",那么就加入\\\",因为json字符串里不允许出现双引号，
            //所以\\转义为\，再加上\"转义为"
            case '"': out += "\\\""; break;

            /*如果是'\\',那么要返回'\\'就必须再加一个'\\'，因为一个'\\'会转义为'\'
            例如echo那段json,客户端发了\\hello,那么为了返回\\hello，需要将源字符串的
            \\转为\\\\才行 */
            case '\\': out += "\\\\"; break;

            //常见的转义字符不必多说，肯定要转义的，但是还有一些不常见，我暂时不写了，这样已经够用了
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}
}
