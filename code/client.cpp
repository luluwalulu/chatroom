#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <unistd.h>
#include <string>
#include <memory>

using boost::asio::ip::tcp;

// Receiver的作用就是异步地读取数据并将其打印到控制台
class Receiver
  :public std::enable_shared_from_this<Receiver>
{
public:
  Receiver(boost::asio::io_context& io_context,tcp::socket& socket):
  io_context_(io_context),socket_(socket) {
  }

  void start(){
    boost::asio::async_read_until(socket_,buffer_,'\n',[&,self=shared_from_this()](const boost::system::error_code& error,std::size_t bytes_transferred){
      // 回调函数体
      std::istream is(&buffer_);
      std::string line;
      std::getline(is,line);
      if(!error){
        std::cout<<"收到消息："<<line<<std::endl;
        self->start();
      }
      else std::cerr<<"读取失败"<<std::endl;
    }); // async_read语句结束
  } // start函数结束

private:
  boost::asio::io_context& io_context_;
  tcp::socket& socket_;
  boost::asio::streambuf buffer_;
};

// Sender的作用是每隔一段固定的时间异步地发送"Hello"（其阻塞点为时间）
class Sender
:public std::enable_shared_from_this<Sender>
{
public:
  Sender(boost::asio::io_context& io,tcp::socket& socket,std::string client_name)
    : input(io,::dup(STDIN_FILENO)),socket_(socket),
      count_(0),client_name_(client_name)
  {
    if(client_name_.size()!=1){
      std::cerr<<"名字长度必须为1！"<<std::endl;
    } 
  }

  void start(){
    boost::asio::async_read_until(input,input_buffer_,'\n',[self=shared_from_this()](const boost::system::error_code& error,std::size_t len){
      if(!error){
        self->send(len);
      }
      else{
        std::cerr<<"从键盘读取消息发生错误"<<std::endl;
      }
    });
  }
private:
  void send(std::size_t len){
    std::istream is(&input_buffer_);
    std::string line;
    std::getline(is,line);
    
    boost::asio::async_write(socket_,boost::asio::buffer(line+'\n'),[self=shared_from_this()](const boost::system::error_code& error,std::size_t len){
      if(!error){
        self->start();
      }
      else{
        std::cerr<<"发送消息失败"<<std::endl;
      }
    });
  }

  void write_handler(){
    // std::cerr<<"结束send函数，此时count_="<<count_-1<<std::endl;
  }

private:
  // 将标准输入包装成Asio的I/O对象
  boost::asio::posix::stream_descriptor input;
  // 为键盘输入准备一个“智能”的、可增长的缓冲区
  boost::asio::streambuf input_buffer_{};
  int count_;
  tcp::socket& socket_;
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
    // 由于类不能在构造函数中创建shared_from_this指针，所以需要start函数
    std::shared_ptr<Receiver> receiver(new Receiver(io_context_,socket_));
    receiver->start();
    std::shared_ptr<Sender> sender(new Sender(io_context_,socket_,client_name_));
    sender->start();
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
    io_context.run();
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
  }
  return 0;
}
// 为了避免在程序中处理IP地址相关问题，我们在server的转发表中维护的是客户名和对应socket之间的映射。由于消息到达时，客户端智能由socket查找对应IP地址，而无法对客户名进行处理
// 所以我们引入协议：发送的消息以如果是“L+客户名+'\n'”，则server需要将相应信息存储到表中。如果是“S+客户名+消息+'\n'”，则需将消息转发到对应客户上去
// 客户名只允许有一个字母，发送的消息的末尾必须有'\n'

// 异步地执行任务要避免的一点是，由于类不会阻塞在回调函数那里，所以类可能会被直接销毁。就比如说这里，在Client的handler函数中，
// Sender类和Receiver类如果不用shared_ptr和shared_from_this处理好的话就会出问题
// 所以我们用shared_ptr创建sender和receiver
// 总结一下这里的sender和receiver：
  // 1.用shared_ptr创建。
  // 2.继承自enabled_shared_from_this类
  // 3.在构造函数中禁止使用shared_from_this()函数，构造函数结束后，基类中的弱指针才指向了正确的位置
  // 4.所以不能依靠类自身的构造函数来完成启动，在类外通过client类来完成启动start()函数
  // 5.start函数的回调函数需要再次调用start函数

// 已经实现client定时发送消息，server发送回声消息的效果
// 现在想要实现client a和client b之间通信的效果
// 需要做出以下修改：
  // 1.对于client，sender类中async_write()不再是定时由steady_timer来进行触发，而是由一个异步的std::cin来实现
  // 注：cin和cout(所有标准iostream对象)共享一把锁
  // 为了避免在等待输入数据（等待cin时），无法接受数据（无法cout）的死锁问题
  // 在这里我们异步地管理键盘输入（键盘输入完成才触发事件，而不是一直阻塞在这里）
  // 2.当client发送消息时，等待键盘输入然后async_write，再次进入下一次等待键盘输入