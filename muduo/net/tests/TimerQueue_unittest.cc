#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/Thread.h"
#include "muduo/base/Logging.h"

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int cnt = 0;
EventLoop *g_loop;

void printTid()
{
  printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char *msg)
{
  printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
  if (++cnt == 20)
  {
    g_loop->quit();
  }
}

void cancel(TimerId timer)
{
  g_loop->cancel(timer);
  printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

int main()
{
  muduo::Logger::setLogLevel(Logger::TRACE);
  printTid();
  sleep(1);
  {
    EventLoop loop;
    g_loop = &loop;

    print("main");
    printf("now %s\n", Timestamp::now().toString().c_str());

    loop.runAfter(1, std::bind(print, "once1"));

    printf("now %s\n", Timestamp::now().toString().c_str());
    loop.runAfter(2, std::bind(print, "once1.5"));

    printf("now %s\n", Timestamp::now().toString().c_str());
    loop.runAfter(3, std::bind(print, "once2.5"));

    printf("now %s\n", Timestamp::now().toString().c_str());
    loop.runAfter(4, std::bind(print, "once3.5"));
    // TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
    // loop.runAfter(4.2, std::bind(cancel, t45));
    // loop.runAfter(4.8, std::bind(cancel, t45));
    // loop.runEvery(2, std::bind(print, "every2"));
    // TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
    // loop.runAfter(9.001, std::bind(cancel, t3));
    // printf("now %s\n", Timestamp::now().toString().c_str());

    // sleep(10);
    // printf("now %s\n", Timestamp::now().toString().c_str());

    printf("loop start!\n");
    printf("now %s\n", Timestamp::now().toString().c_str());

    loop.loop();
    print("main loop exits");
  }
  sleep(1);
  {
    // EventLoopThread loopThread;
    // EventLoop* loop = loopThread.startLoop();
    // loop->runAfter(2, printTid);
    // sleep(3);
    // print("thread loop exits");
  }
}
