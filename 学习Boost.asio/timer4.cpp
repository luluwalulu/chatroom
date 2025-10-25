#include <iostream>
#include <boost/asio.hpp>
using namespace std;

// 这里的printer类就封装了一个完整的定时器会话，它包含了添加任务(async_wait)和执行任务(print)的完整逻辑。
// printer类创建后只需要由io启动会话即可
class printer{
public:
  printer(boost::asio::io_context& io)
    : timer_(io,boost::asio::chrono::seconds(1)),
      count_(0)
  {
    timer_.async_wait( [this](const boost::system::error_code& /*e*/) {this->print();} );
  }

  ~printer(){
    std::cout<<"finish"<<endl;
  }

  void print(){
    if(count_<5){
      std::cout<<count_<<std::endl;
      ++count_;
      
      timer_.expires_at(timer_.expiry()+boost::asio::chrono::seconds(1));
      timer_.async_wait([this](const boost::system::error_code& /*e*/) {this->print();} );
    }
  }

private:
  boost::asio::steady_timer timer_;
  int count_;
};

int main(){
  boost::asio::io_context io;
  
  printer p(io);
  io.run();
  return 0;
}