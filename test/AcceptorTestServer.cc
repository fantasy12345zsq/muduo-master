#include "muduo/net/Acceptor.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/base/Logging.h"

void newConnection(int sockfd, const muduo::net::InetAddress &peerAddr)
{
    printf("newConnection(): accepted a new connection from %s\n",
           peerAddr.toIpPort().c_str());
    ssize_t n = 0;
    n = ::write(sockfd, "Hello world!\n", 13);

    printf("write %ld bytes\n", n);

    muduo::net::sockets::close(sockfd);
}
int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    printf("main(): pid = %d\n", getpid());

    muduo::net::InetAddress listenAddr(9999);
    muduo::net::EventLoop loop;

    muduo::net::Acceptor acceptor(&loop, listenAddr, true);
    acceptor.setNewConnectionCallback(newConnection);
    acceptor.listen();

    loop.loop();
    return 0;
}