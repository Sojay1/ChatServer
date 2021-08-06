#include"chatserver.hpp"
#include"chatservice.hpp"
#include<iostream>
#include<signal.h>
using namespace std;

//处理服务器ctrl + c结束后，重置user状态
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr<<"command invalid ! example: ./ChatServer 192.168.119.130 6000"<<endl;
        exit(-1);
    }
    //解析通过命令参数传递的IP和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);


    signal(SIGINT, resetHandler);
    
    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop, addr, "ChatServer");
    server.start();
    loop.loop();
    return 0;

}