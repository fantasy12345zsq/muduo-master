#include<boost/shared_ptr.hpp>
#include<boost/weak_ptr.hpp>
#include<boost/bind.hpp>
#include<muduo/base/Thread.h>
#include<iostream>
#include<memory>

class tester 
{
    public:
      tester(int value) 
      {
         value_ = value;
         std::cout << "tester()" << std::endl;
      }
      ~tester() 
      {
        std::cout << "~tester()" << std::endl;
      }
    
      void update()
      {
          std::cout << "value = " << value_ << std::endl;
      }
    private:
      int value_;


};


std::shared_ptr<tester> sp;

void fun1(std::weak_ptr<tester> wp)
{
  // sleep(5);

  sp = wp.lock();
  std::cout << sp << " sp.use_count = " << sp.use_count() << std::endl;

  if (sp)
  {
    // 在这里可以安全的使用sp指针.
    std::cout << muduo::CurrentThread::tid() << "  use sp " << std::endl;
    sp->update();
  }
  else
  {
    std::cout << muduo::CurrentThread::tid() << "  指针已被释放!"<< std::endl;
  }
} 
void fun2(std::weak_ptr<tester> wp)
{
  sp = wp.lock();
  std::cout << sp << " sp.use_count = " << sp.use_count() << std::endl;

  sp.reset();
  std::cout << sp << " sp.use_count = " << sp.use_count() << std::endl;

  if (sp)
  {
    // 在这里可以安全的使用sp指针.
    std::cout << muduo::CurrentThread::tid() << "   use sp " << std::endl;
    sp->update();
  }
  else
  {
    std::cout << muduo::CurrentThread::tid() << "   指针已被释放!"<< std::endl;
  }
} 


int main()
{
  std::shared_ptr<tester> sp1(new tester(1));
  std::weak_ptr<tester> wp(sp1);


  // 开启两个线程，并将智能指针传入使用。
  muduo::Thread t1(boost::bind(&fun1, wp));
  muduo::Thread t2(boost::bind(&fun2, wp));

  std::cout << "sp1.use_count = " << sp1.use_count() << std::endl;

  t1.start();
  t2.start();


  t1.join();
  t2.join();
  return 0;
}