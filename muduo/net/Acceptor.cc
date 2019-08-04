// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Acceptor.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

//构造函数
//createNonblockingOrDie函数创建socket套接字
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  //设置地址和端口重用
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reuseport);
  //实际为bind函数
  acceptSocket_.bindAddress(listenAddr);
  //设置回调
  acceptChannel_.setReadCallback(
      std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen()
{
  //保证只在IO线程执行
  loop_->assertInLoopThread();
  listenning_ = true;
  //listen函数
  acceptSocket_.listen();
  //注册读事件
  acceptChannel_.enableReading();
}

//有新连接来时的回调函数
void Acceptor::handleRead()
{
  //同样保证只在IO线程执行
  loop_->assertInLoopThread();
  InetAddress peerAddr;
  //FIXME:loop until no more
  //新连接来时，调用accept函数
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    //调用accept之后的回调函数
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr);
    }
    else
    {
      //如果accept之后没有回调，就关闭套接字
      sockets::close(connfd);
    }
  }
  else
  {
    //accept出错
    //FIXME:出错后是在干嘛呢？？？？？？
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}
