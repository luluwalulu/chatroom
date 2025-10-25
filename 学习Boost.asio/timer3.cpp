#include <iostream>
#include <boost/asio.hpp>

// 讲解重复定时器以及如何给未来的完成处理器传递参数

int main(){
  boost::asio::io_context io;
  int count=0;
  boost::asio::steady_timer t(io,boost::asio::chrono::seconds(1));

  std::function<void(const boost::system::error_code&)> handler;

  // 捕获了t和count的引用
  handler=[&](const boost::system::error_code& /*e*/){
    if(count<5){
      std::cout<<count<<std::endl;
      count++;

      // 定时器每次触发之后需要重新“给闹钟上弦”
      // 如果写成t->expires_after(boost::asio::chrono::seconds(1))；
      // 那么意思是从现在开始，再过一秒到期。但是这可能会扩大时间误差
      // t->expiry()获取的是定时器上一次计划获取的到期时间，这个时间不受延迟影响。所以能很好地避免误差积累的问题。
      t.expires_at(t.expiry()+boost::asio::chrono::seconds(1));
      t.async_wait(handler);
    }
  };

  t.async_wait(handler);
  io.run();
  std::cout<<"Final count is "<<count<<std::endl;

  return 0;
}