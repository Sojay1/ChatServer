#ifndef USER_H
#define USER_H
#include<string>
using namespace std;

//User表的ORM类
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }
    void setId(int id){this->id = id;}
    void setName(string name){this->name = name;}
    void setPwd(string pwd){this->password = pwd;}
    void setState(string state){this->state = state;}

    //这些get方法是不需要参数的
    int getId(){return this->id;}
    string getName(){return this->name = name;}
    string getPwd(){return this->password = password;}
    string getState(){return this->state = state;}
protected:
    int id;
    string name;
    string password;
    string state;
};

#endif