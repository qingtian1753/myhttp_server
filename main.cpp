#include "src/http_server.h"
#include "src/Log.h"
#include <cstdlib>

int main(int argc, char* argv[])
{
    int port = 8080;
    if(argc != 5)
    {
        LOG_ERROR("usage<port><databaseUserName><password><databaseName>!");
        return 1;
    }
    try{
        port = std::atoi(argv[1]);
    }catch(const std::invalid_argument& ia){
            LOG_ERROR("port is invaild_argument");
            return 1;
        };

    HttpServer server(port, "./www");
    if (!server.init("127.0.0.1","qingtian@localhost","hukai823","myserver",3306)) {
        LOG_ERROR("http server init failed");
        return 1;
    }
    
    LOG_INFO("http server started on port=" + std::to_string(port));
    server.run();
    return 0;
}
