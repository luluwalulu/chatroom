#include <boost/asio.hpp>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <array>

using boost::asio::ip::tcp;

// 管理socket，
class tcp_connection
  :public std::enable_shared_from_this<tcp_connection>
{
public:
  using pointer=std::shared_ptr<tcp_connection>;

  static pointer create(boost::asio::io_context& io_context,std::unordered_map<char,pointer>& forward_table){
    return pointer(new tcp_connection(io_context,forward_table));
  }

  tcp_connection(boost::asio::io_context& io_context,std::unordered_map<char,pointer>& forward_table):
    io_context_(io_context),socket_(io_context_),forward_table_(forward_table){

    }

  tcp::socket& socket(){
    return socket_;
  }

  // async_accept的回调函数需要调用该函数，然后该连接就会自动地接受和发送消息到对应目标
  void start(){
    // 收到信息：检查目的地，转发信息
    boost::asio::async_read(socket_,boost::asio::buffer(buffer_),[self=shared_from_this()](const boost::system::error_code& error,
    std::size_t bytes_transferred){
      if(!error) self->handler(bytes_transferred);
      else std::cerr<<"server异步读取消息失败"<<std::endl;
    });
  }

private:
  void handler(std::size_t bytes_transferred){
    // 转发buffer_中的消息，然后再次递归地调用start函数
    if(buffer_[0]=='L'){
      forward_table_[buffer_[1]]=shared_from_this();
    }
    else{
      if(forward_table_.find(buffer_[1])!=forward_table_.end()){
        std::string temp=std::string(buffer_.begin()+2,buffer_.begin()+bytes_transferred);
        boost::asio::async_write(forward_table_.find(buffer_[1])->second->socket(),boost::asio::buffer(temp),[](const boost::system::error_code& error,std::size_t bytes_transferred){

        });
      }
      else{
        std::cerr<<"server尚未与用户"<<buffer_[1]<<"建立连接"<<std::endl;
      }
    }
    start();
  }

private:
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  std::array<char,128> buffer_;
  std::unordered_map<char,pointer>& forward_table_;
};

// 
class tcp_server{
public:
  tcp_server(boost::asio::io_context& io_context)
    :io_context_(io_context),acceptor_(io_context,tcp::endpoint(tcp::v4(),8080)){
      start_accept();
    }
private:
  void start_accept(){
    // 创立new_connection管理socket，通过async_accept来获取socket，最终将socket和对应用户名填入转发表中
    tcp_connection::pointer new_connection=tcp_connection::create(io_context_,forward_table_);
    acceptor_.async_accept(new_connection->socket(),
    [=](const boost::system::error_code& error){
      if(!error){
        new_connection->start();
        // 让对应的socket启动并开始工作，然后继续接受之后的连接请求
        start_accept();
      }
      else{
        std::cerr<<"server接受连接请求发生错误"<<std::endl;
      }
    });
  }
private:
  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  std::unordered_map<char,tcp_connection::pointer> forward_table_;
};

int main(){
  boost::asio::io_context io_context;
  tcp_server server(io_context);
  io_context.run();
  return 0;
}

