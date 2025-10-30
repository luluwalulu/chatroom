#include <array>
#include <iostream>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

// A synchronous TCP daytime client（client体现在是主动请求连接的一方）
int main(int argc,char* argv[]){
  try{
    if(argc!=2){
      std::cerr<<"argc!=2"<<std::endl;
      return 1;
    }

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints=resolver.resolve(argv[1],"daytime");
    // 1.endpoints是一个包含一系列IP地址的列表。一个主机名可能对应多个IP地址（比如一个主机名可能有一个IPV4地址和一个IPV6地址）
    // 2.给resolve函数传递的参数是域名和所需服务（daytime对应端口号13）

    tcp::socket socket(io_context);
    // 函数会在这里阻塞，connect函数会遍历endpoints列表，直至找到可以连接的IP地址
    boost::asio::connect(socket,endpoints);

    for(;;){
      std::array<char,128> buf;
      boost::system::error_code error;

      // 又一个阻塞点，只要服务器发来了数据，马上返回
      size_t len=socket.read_some(boost::asio::buffer(buf),error);
      if(error==boost::asio::error::eof) break;
      else if(error) throw boost::system::system_error(error);

      std::cout.write(buf.data(),len);
    }
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
  }
  return 0;
}