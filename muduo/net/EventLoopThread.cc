// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

//构造函数，指定子线程(IO线程)执行的函数
//IO线程指的是运行loop()的线程
//初始化mutex--->pthread_mutex_init()
//初始化cond---->pthread_cond_init()
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const string &name)
    : loop_(NULL),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(mutex_),
      callback_(cb)
{
}

//析构函数
EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  //如果loop_不为空，代表着子线程的loop正在运行着
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    //使子线程退出loop循环
    loop_->quit();
    //等待子线程的退出
    thread_.join();
  }
}

//创建一个IO线程
EventLoop *EventLoopThread::startLoop()
{
  assert(!thread_.started());
  //在start函数内部才创建线程
  //该函数返回代表着子线程一定创建成功
  //子线程开始执行EventLoopThread::threadFunc()函数
  //这里的子线程就代表IO线程
  thread_.start();

  EventLoop *loop = NULL;
  {
    //加锁
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      //如果loop_为空，该wait(),实际上是在执行pthread_cond_wait()函数，
      //代表主线程阻塞在此，但是会释放互斥锁
      cond_.wait();
    }
    loop = loop_;
  }

  //至此，IO线程已经有了，loop_也有了，IO线程就可以循环
  //返回loop的指针
  return loop;
}

//IO线程运行的函数，在这里定义EventLoop
//启动loop()函数
void EventLoopThread::threadFunc()
{
  //执行EventLoop构造函数，创建wakeupFd套接字，并注册读事件，
  //设置回调为EventLoop::handleRead()
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  //主线程在startLoop中阻塞在wait()函数，释放了互斥锁，
  //子线程加锁成功
  //此时loop_指向一个EventLoop
  //notify()函数实际为pthread_cond_signal函数发信号给主线程，主线程会从pthread_cond_wait返回，
  //返回时，会重新加锁
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  //子线程器loop循环
  loop.loop();
  //assert(exiting_);
  MutexLockGuard lock(mutex_);
  loop_ = NULL;
}
