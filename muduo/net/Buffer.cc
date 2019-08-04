// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "muduo/net/Buffer.h"

#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

//readv为分散读，来自读操作的输入数据被分散到多个应用缓冲区
ssize_t Buffer::readFd(int fd, int *savedErrno)
{
  // saved an ioctl()/FIONREAD call to tell how much to read
  //栈上的空间
  char extrabuf[65536];
  struct iovec vec[2];
  //可写空间大小
  const size_t writable = writableBytes();
  //第一块缓冲区
  vec[0].iov_base = begin() + writerIndex_;
  vec[0].iov_len = writable;
  //第二块缓冲区
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
  //可写空间小于65536, iovec就有两个
  //可写空间大于65536，iovec只有一个
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = sockets::readv(fd, vec, iovcnt);
  if (n < 0)
  {
    *savedErrno = errno;
  }
  else if (implicit_cast<size_t>(n) <= writable)
  {
    //读到的n小于可写的空间
    //直接改变writerIndex
    writerIndex_ += n;
  }
  else
  {
    //当前可写空间不够容纳，因而将数据接收到了第二块缓冲区extrabuf，将其append至buffer
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable);
  }
  // if (n == writable + sizeof extrabuf)
  // {
  //   goto line_30;
  // }
  return n;
}
