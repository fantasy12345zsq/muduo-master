#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/Connector.h"
#include "muduo/net/TcpClient.h"
#include "muduo/base/Logging.h"

#include <stdio.h>

void onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if(conn->connected())
    {
        printf("onConnection(): new connection [%s] from %s\n",
             conn->name().c_str(),
             conn->peerAddress().toIpPort().c_str());
    }
    else
    {
      printf("onConnection(): connection [%s] is down\n",
             conn->name().c_str());
    }
}
  
void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* data,
                 muduo::Timestamp)
{
    printf("onMessage(): received %zd bytes from connection [%s]\n",
           data->readableBytes(), conn->name().c_str());
}

int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    muduo::net::EventLoop loop;
    muduo::net::InetAddress seraddr("127.0.0.1", 9981);

    muduo::net::TcpClient client(&loop, seraddr,"echo");
    client.setConnectionCallback(onConnection);
    client.setMessageCallback(onMessage);
    client.connect();

    loop.loop();
    return 0;
}