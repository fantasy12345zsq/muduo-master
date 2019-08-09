#include "muduo/net/Acceptor.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/base/Logging.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Buffer.h"

void newConnection(muduo::net::TcpConnectionPtr conn)
{
    printf("newConnection(): accepted a new connection \n");


}
void onMessage(const muduo::net::TcpConnectionPtr& conn, 
              muduo::net::Buffer* buf,muduo::Timestamp)
{
    printf("onMessage(): received %zd bytes from connection [%s]\n",
           buf->readableBytes(), conn->name().c_str());
}
int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    printf("main(): pid = %d\n", getpid());

    muduo::net::InetAddress listenAddr(9999);
    muduo::net::EventLoop loop;

    muduo::net::TcpServer server(&loop, listenAddr, "ECHO",muduo::net::TcpServer::kReusePort);
    server.setConnectionCallback(newConnection);
    server.setMessageCallback(onMessage);
    server.start();

    loop.loop();
    return 0;
}