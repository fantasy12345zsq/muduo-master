// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  //如果线程数量大于0
  //创建EventLoopThread，插入threads_
  //EventLoopThread调用startLoop函数，将返回的Eventloop指针插入loops_向量内
  for (int i = 0; i < numThreads_; ++i)
  {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    EventLoopThread *t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    loops_.push_back(t->startLoop());
  }
  //如果线程数量为0，并且用户设置的有回调
  //直接调用用户回调
  //这个baseLoop_是用户创建的
  if (numThreads_ == 0 && cb)
  {
    cb(baseLoop_);
  }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop *loop = baseLoop_;

  //如果loops_向量为空，代表着没有创建好的loop
  //就把用户创建的baseLoop返回去
  //如果loops_不为空，在向量选一个loop返回
  if (!loops_.empty())
  {
    // round-robin
    // 这个round-robin代表轮询调度算法
    // next_初始为0，一直加到loops_向量的大小，然后再返回到0
    loop = loops_[next_];
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
}

//使用hash方法返回loop
EventLoop *EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
  baseLoop_->assertInLoopThread();
  EventLoop *loop = baseLoop_;

  if (!loops_.empty())
  {
    loop = loops_[hashCode % loops_.size()];
  }
  return loop;
}

//得到所有的loop
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  if (loops_.empty())
  {
    return std::vector<EventLoop *>(1, baseLoop_);
  }
  else
  {
    return loops_;
  }
}
