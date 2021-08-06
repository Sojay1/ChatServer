#include"redis.hpp"
#include<iostream>
using namespace std;

Redis::Redis()
    : _publish_context(nullptr), _subscribe_context(nullptr)//构造函数，把两个成员变量初始化置空
{
}

Redis::~Redis()
{
    if(_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if(_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

//连接redis服务器
bool Redis::connect()
{
    //负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1",6379);
    if(_publish_context == nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
    }
    //负责subscribe订阅消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1",6379);
    if(_subscribe_context == nullptr)
    {
        cerr<<"connect redis failed!"<<endl;
    }

    //在单独的线程中，监听通道上的事件，有消息给业务层进行上报，因为subscribe是阻塞状态的
    thread t([&]() {
        observer_channel_message();
    });
    t.detach();
    cout<<"connect redis-server success"<<endl;
    return true;
}

//向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if(reply == nullptr)
    {
        cerr<<"publish command failed"<<endl;
        return false;
    }

    freeReplyObject(reply);
    return true;

    //redisCommand就是redisAppendCommand，redisBufferWrite， redisGetReply这三个函数的调用，就是把命令写到本地缓存，从本地缓存把命令发送到redisServer,再以阻塞的方式等待响应 

}

//向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    //SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只能做订阅通道，不接收通道消息
    //通道消息的接收专门在observer_channel_message函数中的独立线程中运行
    //只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占资源
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
        {
            cerr<<"subscribe command failed!"<<endl;
            return false;
        }
    int done = 0;
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr<<"subscribe command failed!"<<endl;
            return false;
        }
    }
    return true;
}

//向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
        {
            cerr<<"unsubscribe command failed!"<<endl;
            return false;
        }
    int done = 0;
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr<<"unsubscribe command failed!"<<endl;
            return false;
        }
    }
    return true;
}

//在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        //订阅收到的消息是一个带三元素的数组redis发送消息返回三个东西，分别是命令(publish/subscribe)对应的element[0]，channel  element[1]， message  element[2]
        if(reply != nullptr & reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            //给业务层上报通道上发生的消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr<<">>>>>>>>>>>>>>observer_channel_message quit<<<<<<<<<<<<<<<<"<<endl;
}

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}