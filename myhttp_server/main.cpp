#include "src/http_server.h"
#include "src/Log.h"
#include <cstdlib>

int main(int argc, char* argv[])
{
    int port = 8080;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }

    HttpServer server(port, "./www");
    if (!server.init()) {
        LOG_ERROR("http server init failed");
        return 1;
    }

    LOG_INFO("http server started on port=" + std::to_string(port));
    server.run();
    return 0;
}
