# ChatServer
可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端源码  基于muduo/redis/mysql/nginx实现



#编译方式 cd build rm -rf * cmake .. make

#本项目附了一个自动编译脚本autobuild.sh,下载后可直接运行autobuild进行编译

#需要nginx的tcp负载均衡

#需要启动redis服务，作为消息中间件
