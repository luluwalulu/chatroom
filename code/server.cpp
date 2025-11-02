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
    std::cerr<<"开始等待消息"<<std::endl;
    boost::asio::async_read_until(socket_,buffer_,'\n',[self=shared_from_this()](const boost::system::error_code& error,
    std::size_t bytes_transferred){
      if(!error){
        std::cerr<<"tcp_connection开始工作"<<std::endl;
        self->handler(bytes_transferred);
      } 
      else
    {
        // 【“失败”】
        // 检查是哪种“失败”

        if (error == boost::asio::error::eof) // 是这种！！！！！！！！
        {
            // “正常”失败：客户端主动挂断了
            std::cout << "客户端已正常断开连接。" << std::endl;
        }
        else if (error == boost::asio::error::operation_aborted)
        {
            // “正常”失败：我们自己关闭了服务器
            std::cout << "操作被我们自己取消 (服务器关闭中)。" << std::endl;
        }
        else if (error == boost::asio::error::connection_reset)
        {
            // “异常”失败：客户端崩溃了
            std::cerr << "客户端连接被重置 (崩溃)。" << std::endl;
        }
        else
        {
            // “异常”失败：其他所有网络错误
            std::cerr << "读取错误: " << error.message() << std::endl;
        }

        // 无论哪种“失败”，这个会话 (tcp_connection) 都应该结束了。
        // 我们 *不* 再提交下一个读任务。
        // 这个回调函数返回后，(如果用了 shared_ptr)
        // 这个会话对象就会被自动销毁。
    }
    });
  }

private:
  void handler(std::size_t bytes_transferred){
    // 转发buffer_中的消息，然后再次递归地调用start函数
    std::istream is(&buffer_);
    std::string line;
    std::getline(is,line);
    if(line[0]=='L'){
      std::cout << "收到消息：L加客户名" << std::endl;
      forward_table_[line[1]]=shared_from_this();
    }
    else{
      if(forward_table_.find(line[1])!=forward_table_.end()){
        boost::asio::async_write(forward_table_.find(line[1])->second->socket(),boost::asio::buffer(line+'\n'),[this](const boost::system::error_code& error,std::size_t bytes_transferred){
          std::cerr<<"消息已经被成功发送"<<std::endl;
        });
      }
      else{
        std::cerr<<"server尚未与用户"<<line[1]<<"建立连接"<<std::endl;
      }
    }
    this->start();
  }

private:
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  boost::asio::streambuf buffer_{};
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
        std::cout << "server启动tcp_connection" << std::endl;
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
  std::cout << "run之前" << std::endl;
  io_context.run();
  return 0;
}

