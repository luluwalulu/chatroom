#include <thread>
#include <mutex>
#include <iostream>
using namespace std;
// 本文件中试图探讨数据竞争问题

int global_val=0;
std::mutex mtx;

void thread1(){
  mtx.lock();
  for(int i=0;i<10000000;i++){
    global_val++;
  }
  mtx.unlock();
}
void thread2(){
  mtx.lock();
  for(int i=0;i<10000000;i++){
    global_val++;
  }
  mtx.unlock();
}

int main(){
  thread t1(thread1);
  thread t2(thread2);

  t1.join();
  t2.join();

  cout<<global_val;

  return 0;
}

// 1.完全不上锁：没有打印2000000，而是1784055，体现了数据竞争的问题
// 2.直接在函数开始和末尾上锁和开锁，打印了2000000
  // mutex变量的生命周期必须长于所有可能访问它的线程的生命周期
  // 加入在lock()和unlock()之间return或抛出了一个异常，unlock函数就不会被调用，导致了死锁
  // 为了解决上面的问题，引入std::lock guard变量，guard变量在创建时std::lock guard(mtx)，自动对mtx上锁，当它被销毁时，自动对mtx解锁
  // 结论：永远优先使用std::lock guard或std::unique lock
// 3.结合mutex理解strand。strand相比mutex区别在于它是非阻塞的。
  // 当线程1拿到了strand之后，线程2如果想要执行strand任务，它同样会遭遇失败。但是线程2并不会阻塞在这里，它会先记住这个任务，然后执行下一个非strand任务
// 4.理解线程池：线程池其实就是一个以上的线程共同执行同一个任务队列
  // 在timer5.cpp中，其实我们已经实现了一个线程池。io_context(任务列表)和t线程和主线程，它们就构成了一个线程池。
