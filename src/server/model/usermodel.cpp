#include"usermodel.hpp"
#include"db.h"
#include<iostream>
using namespace std;

//User表的增加方法
bool UserModel::insert(User &user)
{
    //组装sql语句，再发送相应的sql语句

    //1、组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')", 
        user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());//这里取到状态默认是offline，然后get方法取到的是string，要转成char*类型,用c_str转
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            //获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));//mysql_insert_id这是一个全局方法，返回id， 传入的参数是MYSQL的对象，mysql.getConnection()返回的恰好就是_conn,MySQL对象
            return true;
        }
    }
    return false;
}

//根据用户号码，查询用户信息
User UserModel::query(int id)
{
    //1、组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d",id); 
        
    MySQL mysql;
    if(mysql.connect())
    {
       MYSQL_RES *res = mysql.query(sql);//query的返回值是一个MYSQL_RES类型，是一个指针
       if(res != nullptr)
       {
           MYSQL_ROW row = mysql_fetch_row(res);//row是首行，char*类型
           if(row != nullptr)
           {
               User user;
               user.setId(atoi(row[0]));//atoi是把char*类型转换为int类型
               user.setName(row[1]);
               user.setPwd(row[2]);
               user.setState(row[3]);
               mysql_free_result(res);//上述返回的指针，这里需要释放，不然会内存泄漏
               return user;
           }
       }
    }
}
//{"msgid":1,"name":"zhangsan","password":"123456"}

//更新用户的状态信息
bool UserModel::updateState(User user)
{
    //1、组装sql语句
    char sql[1024] = {0};
    
    sprintf(sql, "update User set state = '%s' where id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

//重置用户的状体信息
void UserModel::resetState()
{
    char sql[1024] = "update User set state = 'offline' where state = 'online'";

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}
