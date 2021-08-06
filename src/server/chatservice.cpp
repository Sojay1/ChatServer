#include"chatservice.hpp"
#include"public.hpp"
#include<muduo/base/Logging.h>
#include<string>
#include<vector>
using namespace muduo;
using namespace std;
#include<iostream>


//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler回调操作   构造函数 就是初始化成员变量，即_msgHandlerMap
ChatService::ChatService()
{
    //网络模块和业务模块的核心   解耦核心操作
    //基本业务
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1,_2,_3)});//绑定方法
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    //群组业务
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    //CREATE_GROUP_MSG, //创建群组
    //ADD_GROUP_MSG,    //加入群组
    //GROUP_CHAT_MSG,   //群聊天
    
    //连接redis服务器
    if(_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    //把online状态的用户，设置成offline
    _userModel.resetState();
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);//这里不用中括号查询，如果用中括号查询，当查询元素不在里面，会添加一个元素进去，这是副作用
    if(it == _msgHandlerMap.end())
    {
        //返回一个默认的处理器  空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid: "<<msgid<<" can not find Handler";//Muduo库不用写endl，自带endl
        };
    }
    else
    {
        //找到了
        return _msgHandlerMap[msgid];
    } 
}

//目前的需求点就是将业务模块和数据模块拆分开
//处理登陆业务   ORM框架   对象关系映射的框架 解决了痛点：业务层操作的都是对象  DAO数据层，这里才会有数据库的操作
//登陆业务  id password 
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //LOG_INFO<<"do login service!!!";
    //登陆的时候键入id和password
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd)//表示这个人真的在，且密码也正确
    {
        if(user.getState() == "online")
        {
            //该用户已经成功登陆，无法重复登陆
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; //该账号已经登陆
            response["errmsg"] = "this account is using, input anothor";
            conn->send(response.dump());
        }
        else
        {
            //登陆成功，记录用户的连接信息
            {
                lock_guard<mutex> _mguard(_connMutex); //上锁，保证线程安全
                _userConnMap.insert({id, conn});
            }//加个大括号即可保证出了大括号线程自动解锁

            //id用户登陆成功后，向redis订阅channel(id)
            _redis.subscribe(id);
            

            //登陆成功 更新用户状态信息，state offline -> state online
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK; //0
            response["errno"] = 0;             //登陆成功
            response["id"] = user.getId();
            response["name"] = user.getName();

            //查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;//json是可以直接和容器进行序列操作的，可以直接将容器赋给json
                //读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            //查询该用户的好友信息并返回
            vector<User> userVec = _friendmodel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user : userVec)
                {
                    //我们希望发回去的字段就是id, name, state，所以用get取相应的字段，并存入到json中，然后再压入容器 ，最后给response返回
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            //查询用户的群组信息
            vector<Group> groupuserVec = _groupmodel.queryGroups(id);
            if(!groupuserVec.empty())
            {
                vector<string> groupV;
                for(Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();

                    //获取群里面的用户
                    vector<string> userV;
                    for(GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        } 
    }
    else
    {
        //用户不存在，登陆失败
        //或者用户存在，账号或者密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;//登陆失败
        response["errmsg"] = "id or password is  invalid";
        conn->send(response.dump());
    }
}

//处理注册业务  name password 
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //LOG_INFO<<"do reg service!!!";
    // 业务操作的都是数据对象
    //注册的时候需要键入name和password
    string name = js["name"];
    string pwd =  js["password"];//注意在json里面的字段，有的不能自定义，比如password不能写成pwd

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;//注册成功
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;//注册失败
        conn->send(response.dump());
    }
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        //操作connectionMap需要保证线程安全
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(it);//找到了，删掉
        }
    }

    //用户注销，相当于下线，在redis中取消订阅
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid, "", "", "offline");//用user的构造函数更新就可以了
    _userModel.updateState(user);
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> _mguard(_connMutex); //上锁，保证线程安全
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)//上了线，map表中肯定存在的
        {
            if (it->second == conn)
            {
                //从map表中删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //更新用户的状态信息  online offline
    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    {
        //操作map表，需要保证线程安全
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);//在表中查找是否存在
        if(it != _userConnMap.end())
        {
            //toid在线，转发消息
            it->second->send(js.dump());//服务器主动推送消息，给toid用户
            return;
        }
    }
    //查询toid是否在线，可能在其他的服务器登陆着
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return ;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

//添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //存储好友信息
    _friendmodel.insert(userid, friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    //存储新创建的群组信息
    Group group(-1, name, desc);
    if(_groupmodel.createGroup(group))
    {
        //存储群组创建人信息
        _groupmodel.addGroup(userid, group.getId(), "creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupmodel.addGroup(userid, groupid, "normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupmodel.queryGroupUsers(userid, groupid);//查询群组用户的id
    
    lock_guard<mutex> lock(_connMutex);//保证for里面操作map的线程安全
    for(int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end())
        {
            //转发消息
            it->second->send(js.dump());
        }
        else{
            //查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                //存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
            
        }
    }
}

//从redis消息队列中获取订阅的消息  userid即redis的通道号 msg就是发送的信息内容
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    //json js = json::parse(msg.c_str());
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(msg);
        return ;
    }

    //存储用户的离线信息
    _offlineMsgModel.insert(userid, msg);

}

//{"msgid":1,"id":1,"password":"123456"}  登录
//{"msgid":1,"id":2,"password":"123456"}
//{"msgid":3,"name":"lisi","password":"123456"}  注册
//{"msgid":5,"id":1,"from":"zhangsan","to":2,"msg":"hello!"}   // 发消息 zhangsan发给lisi
//{"msgid":5,"id":2,"from":"lisi","to":1,"msg":"挺好的!"}
//{"msgid":5,"id":1,"from":"zhangsan","to":2,"msg":"hello lisi!"} 

//{"msgid":6, "id":1, "friendid":2} 添加好友