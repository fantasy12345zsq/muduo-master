#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/Connector.h"
#include "muduo/base/Logging.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    muduo::net::EventLoop loop;
    muduo::net::InetAddress seraddr("192.168.1.83", 9999);

    muduo::net::Connector Conn(&loop, seraddr);
    Conn.start();

    loop.loop();
    return 0;
}