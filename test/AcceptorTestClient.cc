#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/Connector.h"
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/Buffer.h"

#include <stdio.h>



void newConnection(const muduo::net::TcpConnectionPtr& conn)
{
    printf("newConnection!\n");
    
    conn->send("world\n");
}
int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    muduo::net::EventLoop loop;
    muduo::net::InetAddress seraddr("127.0.0.1", 9999);

    muduo::net::TcpClient Client(&loop, seraddr,"echo");

    Client.setConnectionCallback(newConnection);
    Client.connect();

    loop.loop();
    return 0;
}