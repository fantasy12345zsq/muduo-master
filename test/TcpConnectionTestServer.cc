#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Acceptor.h"
#include "muduo/base/Logging.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/Buffer.h"

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
 
    

int main(int argc,char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    printf("main = (): pid = %d\n",getpid());

    muduo::net::InetAddress listenaddr(9981);
    muduo::net::EventLoop loop;

    muduo::net::TcpServer server(&loop,listenaddr,"echo",muduo::net::TcpServer::kReusePort);

    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);

    printf("server.start()!\n");
    server.start();

    loop.loop();

    return 0;
}