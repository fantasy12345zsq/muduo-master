// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include "muduo/base/BlockingQueue.h"
#include "muduo/base/BoundedBlockingQueue.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"
#include "muduo/base/LogStream.h"

#include <atomic>
#include <vector>

namespace muduo
{

class AsyncLogging : noncopyable
{
public:
  AsyncLogging(const string &basename,
               off_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  void append(const char *logline, int len);

  //开始启动异步日志
  void start()
  {
    running_ = true;

    //在构造函数中latch_的值为1
    //线程运行之后将latch_的值减为0
    thread_.start();

    //必须等到latch_变为0才能从start函数中返回，表明初始化已经完成
    latch_.wait();
  }

  //停止异步日志
  void stop() NO_THREAD_SAFETY_ANALYSIS
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

private:
  //线程调用的函数，主要用于周期性的flush数据到日志文件中
  void threadFunc();

  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector; //buffer队列
  typedef BufferVector::value_type BufferPtr;

  const int flushInterval_;     //日志线程等待的时间长度
  std::atomic<bool> running_;   //是否正在运行
  const string basename_;       //日志名字
  const off_t rollSize_;        //预留的日志大小
  muduo::Thread thread_;        //执行该异步日志记录器的线程
  muduo::CountDownLatch latch_; //倒数计数，用于指示什么时候日志记录器才能开始正常工作
  muduo::MutexLock mutex_;
  muduo::Condition cond_ GUARDED_BY(mutex_);

  //当前缓冲
  BufferPtr currentBuffer_ GUARDED_BY(mutex_);
  //预备缓冲
  BufferPtr nextBuffer_ GUARDED_BY(mutex_);
  //待写入文件的已填满的缓冲
  BufferVector buffers_ GUARDED_BY(mutex_);
};

} // namespace muduo

#endif // MUDUO_BASE_ASYNCLOGGING_H
