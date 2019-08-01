// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/LogFile.h"
#include "muduo/base/Timestamp.h"

#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string &basename,
                           off_t rollSize,
                           int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(basename),
      rollSize_(rollSize),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      latch_(1),
      mutex_(),
      cond_(mutex_),
      currentBuffer_(new Buffer),
      nextBuffer_(new Buffer),
      buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

void AsyncLogging::append(const char *logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);

  //如果currentBuffer_剩余空间足够容纳这条日志消息，直接拷贝到currentBuffer_
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  //currentBuffer_已满
  else
  {
    //把currentBuffer_添加到buffer列表中
    //currentBuffer_置为NULL
    buffers_.push_back(std::move(currentBuffer_));

    if (nextBuffer_)
    {
      //currectBuffer_指向nextBuffer_，并将nextBuffer_置为NULL
      currentBuffer_ = std::move(nextBuffer_);
    }
    else
    {
      //前端写入速度很快，已用完两块缓冲，只好分配新的Buffer
      //currectBuffer_指向new Buffer的空间，

      //FIXME:new Buffer的临时变量什么时候释放呢？
      //Buffer是unique_ptr 不需要显示调用delete
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);

    //通知日志线程，有数据可写
    //只有当缓冲区满了之后才会将数据写入日志文件中
    cond_.notify();
  }
}

//日志线程调用的函数
void AsyncLogging::threadFunc()
{
  //running_在start函数内置为true
  assert(running_ == true);
  latch_.countDown();

  //日志对象
  LogFile output(basename_, rollSize_, false);

  //buffer指针
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();

  //将要写入文件的buffer
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      //加锁
      muduo::MutexLockGuard lock(mutex_);

      //如果buffer为空，表示没有数据需要写入文件，等待指定的时间
      //buffers_为主程序中写入的缓冲区
      if (buffers_.empty()) // unusual usage!
      {
        //如果主程序中没有写入日志
        //等待指定的时间
        cond_.waitForSeconds(flushInterval_);
      }

      //将当前currentBuffer的数据倒入buffer列表中
      buffers_.push_back(std::move(currentBuffer_));

      //将新的buffer转换成当前缓冲区
      currentBuffer_ = std::move(newBuffer1);

      //buffers_和buffersToWrite交换数据，此时buffers_所有的数据存放在buffersTowrite，
      //buffers_变为空
      buffersToWrite.swap(buffers_);

      //如果备份缓冲区为空就设置下一个缓冲区
      if (!nextBuffer_)
      {
        nextBuffer_ = std::move(newBuffer2);
      }
    }
    //临界区之外操作buffersToWrite缓冲区
    assert(!buffersToWrite.empty());

    //如果将要写入文件的buffer列表中buffer个数大于25，那么将这些数据删除
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size() - 2);
      fputs(buf, stderr);
      //添加到文件
      output.append(buf, static_cast<int>(strlen(buf)));
      //删除bufferToWrite的说有数据
      buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
    }

    //将bufferTowrite的数据写入到日志中
    for (const auto &buffer : buffersToWrite)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffer->data(), buffer->length());
    }

    //重新调整bufferToWrite的大小
    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }

    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    output.flush();
  }
  output.flush();
}
