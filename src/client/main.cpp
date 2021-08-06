#include"json.hpp"
#include<iostream>
#include<thread>
#include<string>
#include<vector>
#include<chrono>
#include<ctime>
using namespace std;
using json = nlohmann::json;

#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include"group.hpp"
#include"user.hpp"
#include"public.hpp"

//记录当前系统登陆的用户信息
User g_currentUser;
//记录当前登陆用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前登陆用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制主菜单页面程序
bool isMainMenuRunning = false;

//显示当前登陆成功用户的基本信息
void showCurrentUserData();

//接收线程
void readTaskHandler(int clientfd);
//获取系统时间
string getCurrentTime();
//主聊天页面程序
void mainMenu(int clientfd);

//聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char ** argv)
{
    if(argc < 3)
    {
        cerr<<"command invalid ! example: ./ChatClient 192.168.119.130 6000"<<endl;
        exit(-1);
    }
    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    //1、创建：创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr<<"socket create error"<<endl;
        exit(-1);
    }
    //2、填写client需要连接的server信息的IP+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    //client 和 server进行连接
    if(-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr<<"connect server error"<<endl;
        close(clientfd);
        exit(-1);
    }

    //main线程用于接收用户输入，负责发送数据
    for(;;)
    {
        //显示首页面菜单 登陆、注册、退出
        cout<<"**************"<<endl;
        cout<<"* 1.login    *"<<endl;
        cout<<"* 2.register *"<<endl;
        cout<<"* 3.quit     *"<<endl;
        cout<<"**************"<<endl;
        cout<<"choice: ";
        int choice = 0;
        cin>>choice;
        cin.get();//读掉缓冲区残留的回车
        switch(choice)
        {
            case 1://login 登陆业务
            {
                int id = 0;
                char pwd[50] = {0};
                cout<<"userid: ";
                cin>>id;
                cin.get();//读掉缓冲区残留的回车
                cout<<"userpassword: ";
                cin.getline(pwd,50);
                

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();//dump表示序列化
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr<<"send login msg error: "<<request<<endl;
                }
                else
                {
                    //cout<<"进入到第一个else"<<endl;
                    char buffer[1024] = {0};
                    int len = recv(clientfd, buffer, 1024, 0);//阻塞在这里了
                    if(len == -1)
                    {
                        cerr<<"recv login response error"<<endl;
                    }
                    else
                    {
                        //cout<<"进入到第二个else"<<endl;
                        json responsejs = json::parse(buffer);//parse表示反序列化
                        if(0 != responsejs["errno"].get<int>())//登陆失败，返回的error number != 0
                        {
                            cerr<<responsejs["errmsg"]<<endl;
                        }
                        else//登陆成功
                        {
                            //cout<<"进入到第三个else"<<endl;
                            cout<<"登陆成功"<<endl;
                            //记录当前用户的id和name
                            g_currentUser.setId(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);

                            //记录当前用户的好友列表信息
                            if(responsejs.contains("friends"))
                            {
                                //初始化
                                g_currentUserFriendList.clear();//全局变量，使用前初始化
                                vector<string>  vec = responsejs["friends"];
                                for(string &str : vec)
                                {
                                    json js = json::parse(str);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }
                            //记录当前用户的群组列表信息
                            if(responsejs.contains("groups"))
                            {
                                //初始化
                                g_currentUserGroupList.clear();//全局变量，使用前初始化
                                vector<string> vec1 = responsejs["groups"];
                                for(string &groupstr : vec1)
                                {
                                    json grpjs = json::parse(groupstr);
                                    Group group;
                                    group.setId(grpjs["id"].get<int>());
                                    group.setName(grpjs["groupname"]);
                                    group.setDesc(grpjs["groupdesc"]);
                                    
                                    vector<string> vec2 = grpjs["users"];
                                    for(string &userstr : vec2)
                                    {
                                        json js = json::parse(userstr);//这句话等同于 json js; js.parse(userstr);反序列化过程一行代码代替
                                        GroupUser user;
                                        user.setId(js["id"].get<int>());
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }
                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            //显示用户的基本信息  个人信息，好友信息，群信息和群成员
                            showCurrentUserData();

                            //显示当前用户的离线消息  个人聊天信息或者群组消息
                            if(responsejs.contains("offlinemsg"))
                            {
                                vector<string> vec = responsejs["offlinemsg"];
                                for(string &str : vec)
                                {
                                    json js = json::parse(str);
                                    //time + [id] + name + "said" + xxx
                                    if (ONE_CHAT_MSG == js["msgid"].get<int>())//个人消息
                                    {
                                        cout << js["time"].get<string>() << " [ " << js["id"] << " ] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                                    }
                                    else//群消息
                                    {
                                        cout << "群消息 [ " << js["groupid"] << " ]: " << js["time"].get<string>() << " [ " << js["id"] << " ] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                                        continue;
                                    }
                                }
                            }
                            //登陆成功后，启动接收线程负责接收数据；该线程只启动一次
                            static int readthreadnumber = 0;//只有第一次进来为0，后面每次进来都不会为0了
                            if(readthreadnumber == 0)
                            {
                                std::thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                readthreadnumber++;
                            }

                            //进入聊天主菜单页面
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        }
                    }
                }
            }
            break;
            case 2:// register 业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout<<"username: "<<endl;
                cin.getline(name, 50);// cin >> scanf遇见空格和非法字符都认为是结束输入，所以用cin.getline(),在c语言用gets，默认遇见回车在结束
                cout<<"userpassword: "<<endl;
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr<<"send reg msg error: "<<request<<endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if(-1 == len)
                    {
                        cerr<<"recv reg response error"<<endl;
                    }
                    else
                    {
                        json responsejs = json::parse(buffer);
                        if(0 != responsejs["errno"].get<int>())//注册失败，errornumber != 0
                        {
                            cerr<<name<<"is already exist, register error!"<<endl;
                        }
                        else//注册成功
                        {
                            cout<<name<<" register success, userid is: "<<responsejs["id"]<<", do not forget it!"<<endl;
                        }
                    }
                }
            }
            break;
            case 3: //quit业务
            {
                close(clientfd);
                exit(0);
            }
            default:
                cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout<<endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout<<endl;
    cout << "----------------------friend list---------------------" << endl;
    cout<<endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    cout<<endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
    cout<<endl;
}

//接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);//阻塞
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }
        //接收ChatServer转发的数据并进行反序列化，生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        //个人消息 
        if(ONE_CHAT_MSG == msgtype)
        {
            cout<<js["time"].get<string>()<<" [ "<<js["id"]<<" ] "<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
        //群消息
        else if(GROUP_CHAT_MSG == msgtype)
        {
            cout<<"群消息 [ "<<js["groupid"]<<" ]: "<<js["time"].get<string>()<<" [ "<<js["id"]<<" ] "<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
    }
}

//获取系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

void help(int fd = 0, string str = "");
//chat command handler
void chat(int, string);
//addfriend command
void addfriend(int, string);
//creategroup command
void creategroup(int, string);
//addgroup command
void addgroup(int, string);
//groupchat command
void groupchat(int, string);
//loginout command
void loginout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend","添加好友，格式addfriend:friendid"},
    {"creategroup","创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组，格式addgroup：groupid"},
    {"groupchat","群聊，格式groupchat:groupid:message"},
    {"loginout","注销，格式loginout"}};  

//注册系统支持的客户端命令处理
unordered_map<string,function<void(int, string)>> commandHandlerMap ={
    {"help", help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout", loginout}};

//主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;//存储命令
        int idx = commandbuf.find(":");
        if(idx == -1)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0,idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr<<"invalid input command"<<endl;
            continue;
        }
        //调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

//command list
void help(int, string)
{
    cout<<"show command list"<<endl;
    for(auto &p : commandMap)
    {
        cout<<p.first<<" : "<<p.second<<endl;
    }
    cout<<endl;
}

//addfriend command
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send addfriend msg error ->"<<buffer<<endl;
    }
}

//chat command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":");//friend
    if(idx == -1)
    {
        cerr<<"chat command invalid"<<endl;
        return ;
    }
    int friendid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send chat msg error -> "<<buffer<<endl;
    }
}

//creategroup command  groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr<<"creategroup command invalid"<<endl;
        return ;
    }
    string groupname = str.substr(0,idx);//substr的第二个参数是长度，不是下标，第一个才是下标
    string groupdesc = str.substr(idx + 1,str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send creatgroup msg error -> "<<buffer<<endl;
    }
}

//addgroup command  
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();
    
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send addgroup msg error ->"<<buffer<<endl;
    }
}

//groupchat command   groupid:message
void groupchat(int clientfd, string str) 
{
    int idx = str.find(":");//friend
    if(idx == -1)
    {
        cerr<<"groupchat command invalid"<<endl;
        return ;
    }
    int groupid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send groupchat msg error -> "<<buffer<<endl;
    }
}
//loginout command
void loginout(int clientfd, string)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr<<"send loginout msg error ->"<<buffer<<endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}