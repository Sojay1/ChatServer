#include"chatserver.hpp"
#include<functional>
#include<string>
#include"json.hpp"
#include"chatservice.hpp"
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

//初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
           const InetAddress &listenAddr,
           const string &nameArg)
           : _server(loop, listenAddr, nameArg),_loop(loop)
{
    //注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this, _1));

    //注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    //线程数量
    _server.setThreadNum(4);
}

//启动服务
void ChatServer::start()
{
    _server.start();
}


//上报链接相关信息的回调函数  链接监听
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    //客户端连接断开  用户下线
    if(!conn->connected())
    {
        //客户端异常关闭的话，接收一个conn
        ChatService::instance()->clientCloseException(conn);
        //出现异常直接断开连接
        conn->shutdown();
    }
}
//上报读写事件相关信息的回调函数  读写事件监听
void ChatServer::onMessage(const TcpConnectionPtr &conn,
               Buffer *buffer,
               Timestamp time)
{
    string buf = buffer->retrieveAllAsString();//把buffer里面的数据通过retrieveAllAsString()函数放到string里面
    json js = json::parse(buf);//数据的反序列化
    //目的就是 ： 完全解耦 网络模块的代码和业务模块的代码
    //通过js里面读出的 比如["msgid"]  获取一个业务处理器 handler 回调handler-> conn js time 
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());//获取服务的唯一实例，从实例中获取消息id，并获取了消息id中对应的事件处理器
    //回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}