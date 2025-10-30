#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string(){
  std::time_t now=std::time(0);
  return std::ctime(&now);
}

int main(){
  try{
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context,tcp::endpoint(tcp::v4(),13));
    // 1.tcp::v4()意思是使用IPV4并监听本机所有IP地址
    // 2.组合起来意思就是请创建一个‘总机’，将它绑定 (bind) 到本机所有 IPv4 地址的 13 端口上，并开始监听 (listen)。
    // 3.endpoint仍然是包含IPV4地址和端口号
    for(;;){
      tcp::socket socket(io_context);
      // 4.main线程会阻塞咋i这里直至连接建立，然后所有信息都会被填充到这个空的socket对象中
      acceptor.accept(socket); 
      std::string message=make_daytime_string();

      boost::system::error_code ignored_error;
      // 5.write是一个阻塞调用，直至所有数据都被发送给客户端
      boost::asio::write(socket,boost::asio::buffer(message),ignored_error);
    } // 6.无限循环直至下一个连接接入
  }
  catch(std::exception& e){
    std::cerr<<e.what()<<std::endl;
  }
  return 0;
}