#include <functional>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>

class printer
{
public:
  printer(boost::asio::io_context& io)
    : strand_(boost::asio::make_strand(io)),
      timer1_(io, boost::asio::chrono::seconds(1)),
      timer2_(io, boost::asio::chrono::seconds(1)),
      count_(0)
  {
    timer1_.async_wait(boost::asio::bind_executor(strand_,
    std::bind(&printer::print1, this)));

    timer2_.async_wait(boost::asio::bind_executor(strand_,
    std::bind(&printer::print2, this)));
  }

  ~printer()
  {
    std::cout << "Final count is " << count_ << std::endl;
  }
    void print1()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 1: " << count_ << std::endl;
      ++count_;

      timer1_.expires_at(timer1_.expiry() + boost::asio::chrono::seconds(1));

      timer1_.async_wait(boost::asio::bind_executor(strand_,
            std::bind(&printer::print1, this)));
    }
  }

  void print2()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 2: " << count_ << std::endl;
      ++count_;

      timer2_.expires_at(timer2_.expiry() + boost::asio::chrono::seconds(1));

      timer2_.async_wait(boost::asio::bind_executor(strand_,
            std::bind(&printer::print2, this)));
    }
  }

private:
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  boost::asio::steady_timer timer1_;
  boost::asio::steady_timer timer2_;
  int count_;
};

// 在该例中我们额外创建一个线程，一个是主线程，一个是std::thread t。
// 且它们都执行了io.run()。在printer中，print1和print2每次都是同时开始同时结束。
// 此时，两个线程可能会争抢访问print函数（io_context中的回调函数只能被一个线程调用，之后这个任务从任务清单上消失）
// 假设主线程访问print1，t访问print2，就会出现数据竞争。
// 当多个线程并发地访问同一块内存，并且至少有一个访问是写入操作时，就会发生数据竞争。

// 同步器：这里同步器保证了无论有多少线程在调用io.run()，凡是被提交到同一个strand上的回调，永远不会被并发执行。
// 即使任务清单上有100个回调（都被提交到strand），并且有100个线程试图访问回调。同一时刻也只有一个线程能够访问一个回调。

// boost::asio::bind_executor(),包含两个参数，执行策略（在这里就是strand对象）和回调函数

int main()
{
  boost::asio::io_context io;
  printer p(io);
  std::thread t([&]{ io.run(); });
  io.run();
  t.join(); // 告诉主线程直到t线程执行完毕，才往下走

  return 0;
}