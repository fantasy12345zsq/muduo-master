#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/Logging.h"
#include <stdio.h>

void runInThread()
{
    printf("runInThread() : pid = %d,tid = %d\n",
           getpid(), muduo::CurrentThread::tid());
    printf("runInThread now = %s\n", muduo::Timestamp::now().toString().data());
}

int main(int argc, char *argv[])
{
    muduo::Logger::setLogLevel(muduo::Logger::TRACE);

    printf("main() : pid = %d, tid = %d\n",
           getpid(), muduo::CurrentThread::tid());
    muduo::net::EventLoopThread loopThread;
    printf("loopThread.startLoop()!\n");
    muduo::net::EventLoop *loop = loopThread.startLoop();
    printf("loop->runInloop()!\n");
    loop->runInLoop(runInThread);
    sleep(1);
    printf("main now = %s\n", muduo::Timestamp::now().toString().data());
    loop->runAfter(2, runInThread);
    sleep(3);
    loop->quit();

    printf("exit main!\n");
    return 0;
}