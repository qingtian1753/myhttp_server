#include "src/http_server.h"
#include "src/Log.h"
#include <cstdlib>

int main(int argc, char* argv[])
{
    int port = 8080;
    if(argc != 6)
    {
        LOG_ERROR("usage<host><databaseUserName><password><databaseName><listen port>!");
        return 1;
    }
    try{
        port = std::stoi(argv[5]);
    } catch(std::invalid_argument& ia){
        LOG_ERROR("listen port input error!");
        return 1;
    }
    HttpServer server(port, "./www");
    if (!server.init(argv[1],argv[2],argv[3],argv[4],3306)) {
        LOG_ERROR("http server init failed");
        return 1;
    }
    
    server.run();
    return 0;
}
