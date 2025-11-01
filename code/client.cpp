#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <unistd.h>
#include <string>

using boost::asio::ip::tcp;
// cin和cout(所有标准iostream对象)共享一把锁
// 为了避免在等待输入数据（等待cin时），无法接受数据（无法cout）的死锁问题
// 在这里我们异步地管理键盘输入（键盘输入完成才触发事件，而不是一直阻塞在这里）


// Receiver的作用就是异步地读取数据并将其打印到控制台
class Receiver{
public:
  Receiver(boost::asio::io_context& io_context,tcp::socket& socket):
  io_context_(io_context),socket_(socket) {
    boost::asio::async_read(socket_,boost::asio::buffer(buffer_),[&](const boost::system::error_code& error,std::size_t bytes_transferred){
      if(!error) receiver_handler(bytes_transferred);
      else std::cerr<<"读取失败"<<std::endl;
    });
  }

private:
  void receiver_handler(std::size_t bytes_transferred){
    boost::asio::async_read(socket_,boost::asio::buffer(buffer_),[&](const boost::system::error_code& error,std::size_t bytes_transferred){
      if(!error) receiver_handler(bytes_transferred);
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
  Sender(boost::asio::io_context& io,tcp::socket& socket,std::string client_name)
    : timer_(io,boost::asio::chrono::seconds(1)),socket_(socket),
      count_(0),buffer_{},client_name_(client_name)
  {
    if(client_name_.size()!=1) std::cerr<<"名字长度必须为1！"<<std::endl;
    timer_.async_wait( [this](const boost::system::error_code& /*e*/) {this->send();} );
  }

private:
  void send(){
    if(count_<5){
      // 隐含的问题，一旦client1到服务器的连接建立就发送信息，此时client2和服务器的连接可能尚未建立，所以引入下面的初始身份验证。同时，为了server的转发表能够顺利建立客户名和socket之间的映射
      // 所以定义一个登录协议，连接建立时的第一条消息发送"LOGIN ALICE"
      // 之后的消息发送"SENDTOTOM  MESSAGE"
      // 前六个字母是行为，后五个字母是客户名，再后面是消息

      if(count_==0){
        buffer_.assign("L"+client_name_);
      }
      else{
        buffer_.assign("S"+client_name_+"你好你好，我系"+client_name_);
      }
      
      boost::asio::async_write(socket_,boost::asio::buffer(buffer_),[this](const boost::system::error_code& error,std::size_t bytes_transferred){
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
  std::string buffer_;
  std::string client_name_;
};

// client是主动发起连接的一方
class Client{
public:
  Client(boost::asio::io_context& io_context,std::string client_name):
    io_context_(io_context),resolver_(io_context),socket_(io_context),client_name_(client_name)
  {
    start_connect();
  }
private:
  void start_connect(){
    // Client一旦被建立，就会和server建立连接
    tcp::resolver::results_type endpoints=resolver_.resolve("localhost","8080");
    boost::asio::async_connect(socket_,endpoints,[this](const boost::system::error_code& error,const tcp::endpoint& endpoint){
      if(!error) handler();
      else std::cerr<<"连接失败"<<std::endl;
    });
  }

  void handler(){
    // 一旦连接建立，就应该开启async_write和async_read
    // 其中async_write应该被std::cin阻塞，一旦从键盘上读取到数据，就启动async_write发送消息
    // 而async_read不应该被std::cin阻塞
    Sender(io_context_,socket_,client_name_);
    Receiver(io_context_,socket_);
  }

private:
  boost::asio::io_context& io_context_;
  tcp::resolver resolver_;
  tcp::socket socket_;
  std::string client_name_; // 名字长度必须为1
};

int main(int argc,char* argv[]){
  try{
    if(argc!=2){
      std::cerr<<"argc!=2"<<std::endl;
      return 1;
    }
    boost::asio::io_context io_context;
    Client client(io_context,argv[1]);
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
  }
  return 0;
}