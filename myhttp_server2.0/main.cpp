#include "src/main_reactor.h"
#include "src/eventloop.h"
#include "src/Log.h"
#include <cstdlib>

int main(int argc, char* argv[])
{
    Log::setLevel(LogLevel::warn);
    int port = 8080;
    int reactor_size = 5;
    if(argc!=7)
    {
        LOG_ERROR("usage<host><reactor_size><username><password><databasename><port>");
        return 1;
    }
    
    try{    
        reactor_size = std::stoi(argv[2]);
        port = std::stoi(argv[6]);
    }catch(std::invalid_argument& ia){
        LOG_ERROR("port or reactor_size is invalid!");
        return 1;
    }
    MainReactor server(port,reactor_size,argv[1],argv[3],argv[4],argv[5]);
    LOG_INFO("http server started on port=" + std::to_string(port));
    server.run();
    return 0;
}
