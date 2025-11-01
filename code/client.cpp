#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <unistd.h>
#include <string>

using boost::asio::ip::tcp;
// cin和cout(所有标准iostream对象)共享一把锁
// 为了避免在等待输入数据（等待cin时），无法接受数据（无法cout）的死锁问题
// 在这里我们异步地管理键盘输入（键盘输入完成才触发事件，而不是一直阻塞在这里）


// Reader的作用就是异步地读取数据并将其打印到控制台
class Reader{
public:
  Reader(boost::asio::io_context& io_context,tcp::socket& socket):
  io_context_(io_context),socket_(socket) {
    boost::asio::async_read(socket_,boost::asio::buffer(buffer_),[&](const boost::system::error_code& error,std::size_t bytes_transferred){
      if(!error) reader_handler(bytes_transferred);
      else std::cerr<<"读取失败"<<std::endl;
    });
  }

private:
  void reader_handler(std::size_t bytes_transferred){
    boost::asio::async_read(socket_,boost::asio::buffer(buffer_),[&](const boost::system::error_code& error,std::size_t bytes_transferred){
      if(!error) reader_handler(bytes_transferred);
      else std::cerr<<"读取失败"<<std::endl;
    });
  }

private:
  boost::asio::io_context& io_context_;
  tcp::socket& socket_;
  std::array<char,128> buffer_{};
};

// Sender的作用是每隔一段固定的时间异步地发送"Hello"（其阻塞点为时间）
class Sender{
public:
  Sender(boost::asio::io_context& io,tcp::socket& socket)
    : timer_(io,boost::asio::chrono::seconds(1)),socket_(socket),
      count_(0)
  {
    timer_.async_wait( [this](const boost::system::error_code& /*e*/) {this->send();} );
  }

private:
  void send(){
    if(count_<5){
      boost::asio::async_write(socket_,boost::asio::buffer(buffer_),[this](const boost::system::error_code& error){
        if(!error) sender_handler();
        else std::cerr<<"读取错误"<<std::endl;
      });

      ++count_;      
      timer_.expires_at(timer_.expiry()+boost::asio::chrono::seconds(1));
      timer_.async_wait([this](const boost::system::error_code& /*e*/) {this->send();} );
    }
  }

  void sender_handler(){

  }

private:
  boost::asio::steady_timer timer_;
  int count_;
  tcp::socket& socket_;
  std::string buffer_{"Client1: Hello\n"};
};

// client是主动发起连接的一方
class Client{
public:
  Client(boost::asio::io_context& io_context):
    io_context_(io_context),resolver_(io_context),socket_(io_context)
  {
    start_connect();
  }
private:
  void start_connect(){
    tcp::resolver::results_type endpoints=resolver_.resolve("localhost","8080");
    boost::asio::async_connect(socket_,endpoints,[this](const boost::system::error_code& error){
      if(!error) handler();
      else std::cerr<<"连接失败"<<std::endl;
    });
  }

  void handler(){
    // 一旦连接建立，就应该开启async_write和async_read
    // 其中async_write应该被std::cin阻塞，一旦从键盘上读取到数据，就启动async_write发送消息
    // 而async_read不应该被std::cin阻塞
    Sender(io_context_,socket_);
    Reader(io_context_,socket_);
  }

private:
  boost::asio::io_context& io_context_;
  tcp::resolver resolver_;
  tcp::socket socket_;
};