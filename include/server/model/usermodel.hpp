#ifndef USERMODEL_H
#define USERMODEL_H

#include"user.hpp"

//User表的数据操作类
class UserModel
{
public:
    //User表的增加方法
    bool insert(User &user);

    //根据用户号码，查询用户信息
    User query(int id);//返回一个指针，如果没有查到就可以返回一个空指针

    //更新用户的状态信息
    bool updateState(User user);

    //重置用户的状体信息 
    void resetState();
};

#endif