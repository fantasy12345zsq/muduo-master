// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>

namespace muduo
{
namespace net
{
namespace detail
{

//生成一个定时器对象，返回与之关联的文件描述符
//接受两个参数，一个是clocid，填写CLOCK_REALTIME或者CLOCK_MOMOTONIC
//CLOCK_REALTIME为系统实时时间，随系统实时时间改变而改变，即从UTC1970-1-1 0:0:0开始计时
//中间时刻如果系统时间被用户改成其他，则对应的时间改变
//CLOCK_MONOTONIC:从系统启动的这一刻开始，不受系统时间被用户改变的影响
//第二个参数TFD_NONBLOCK为设置非阻塞
//TFD_CLOEXEC表示当程序执行exec函数时本fd将被系统自动关闭，表示不传递
int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
  int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
  // printf("when.time = %ld,now.time =%ld,microseconds = %ld\n",
  //        when.microSecondsSinceEpoch(),
  //        Timestamp::now().microSecondsSinceEpoch(),
  //        microseconds);
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  printf("seconds = %ld\n", ts.tv_sec);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

//timerfd_settime()此函数用于设置新的超时时间，并开始计时
//timefd为timerfd_create函数返回的文件句柄
//flags：参数flags为1表示设置的是绝对时间，为0表示相对时间
//
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  memZero(&newValue, sizeof newValue);
  memZero(&oldValue, sizeof oldValue);
  //计算时间差
  newValue.it_value = howMuchTimeFromNow(expiration);
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

} // namespace detail
} // namespace net
} // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
  //设置回调函数
  timerfdChannel_.setReadCallback(
      std::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  //注册读事件
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (const Entry &timer : timers_)
  {
    delete timer.second;
  }
}
//添加定时器
TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,
                             double interval)
{
  //timer保存时间戳、回调函数、
  Timer *timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(
      std::bind(&TimerQueue::addTimerInLoop, this, timer));
  printf("timer->sequence() = %ld\n", timer->sequence());
  return TimerId(timer, timer->sequence());
}

//取消定时器
void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
  loop_->assertInLoopThread();
  //插入定时器
  bool earliestChanged = insert(timer);

  if (earliestChanged)
  {
    printf("earliestChanged!\n");
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end())
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1);
    (void)n;
    delete it->first; // FIXME: no delete please
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_)
  {
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

//定时到了之后回调的函数
void TimerQueue::handleRead()
{
  printf("now %s\n", Timestamp::now().toString().c_str());

  printf("TimerQueue::handleRead()\n");
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  //由于muduo设置为默认水平触发，
  //如果不读套接字，会一直触发读事件
  readTimerfd(timerfd_, now);

  //定时到了之后要删除相应的定时器
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  //回调用户设置的回调函数
  //表明时间到了之后该做什么
  for (const Entry &it : expired)
  {
    it.second->run();
  }
  callingExpiredTimers_ = false;

  //重置的主要目的在于，Eventloop存在runEvery函数，意思为每过多长时间要执行一次用户的函数
  //所以定时器到时之后要添加新的定时器
  reset(expired, now);
}

//删除定时器
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
  //end在于找到一个比要删除的定时器时间戳大的一个下界
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);
  printf("end->seond = %p\n", end->second);
  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  for (const Entry &it : expired)
  {
    ActiveTimer timer(it.second, it.second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1);
    (void)n;
  }

  assert(timers_.size() == activeTimers_.size());

  std::vector<TimerQueue::Entry>::iterator iter = expired.begin();
  printf("expired!\n");
  while (iter != expired.end())
  {
    printf("timestamp = %s,time * = %p\n", iter->first.toString().data(), iter->second);
    iter++;
  }
  return expired;
}

void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
  Timestamp nextExpire;

  for (const Entry &it : expired)
  {
    ActiveTimer timer(it.second, it.second->sequence());

    //Eventloop中runEvery设置的定时器会进入这个if，再次插入Timer
    if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      printf("it.second = %p\n", it.second);
      it.second->restart(now);
      insert(it.second);
    }
    else
    {
      // FIXME move to a free list
      delete it.second; // FIXME: no delete please
    }
  }

  if (!timers_.empty())
  {
    nextExpire = timers_.begin()->second->expiration();
  }
  printf("nextExpire = %s\n", nextExpire.toString());
  if (nextExpire.valid())
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}

//插入定时器操作
bool TimerQueue::insert(Timer *timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  bool earliestChanged = false;
  //获取定时器的时间戳
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin();

  //当要插入的定时器为第一个，或者要插入的定时器的时间戳小于set集合中的第一个定时器的时间戳
  //需要重置一下定时器
  if (it == timers_.end() || when < it->first)
  {
    earliestChanged = true;
  }
  //插入timers_集合中
  {
    std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
    assert(result.second);
    (void)result;
  }
  //插入activeTimers_集合中
  {
    std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second);
    (void)result;
  }

  //保证两个集合的大小一样，因为存的都是定时器
  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}
