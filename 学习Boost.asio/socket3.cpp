#include <ctime>
#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>

// 这是一个能够处理多个客户端请求的，异步的，并发的服务器

using boost::asio::ip::tcp;

std::string make_daytime_string()
{
    using namespace std;
    time_t now = time(0);
    return ctime(&now);
}

// 作用：处理一个客户端，然后自动消失
class tcp_connection
  : public std::enable_shared_from_this<tcp_connection>
  // 对于继承自std::enable_shared_from_this<T>的类，当该对象是由shared_ptr创建时，它的基类部分会用一个指针p记录shared_ptr的控制块。
  // 当使用shared_from_this时，可以直接在类的内部创建一个共享控制块p的shared_ptr
  // 对于继承自std::enable_shared_from_this<T>的对象应该始终由一个shared_ptr（或者说唯一控制块）来进行管理，因为基类指向的控制块始终是最初的shared_ptr的控制块
  // 使用std::enable_shared_from_this<T>，我们使得类本身可以控制管理自己的shared_ptr是否销毁。shared_ptr是否销毁不再完全取决于类外。
{
public:
    typedef std::shared_ptr<tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context) // 静态函数
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start() // 会话的启动器
    {
        message_ = make_daytime_string();

        // 所有数据均已被发送时调用回调函数
        // 同样不会阻塞在这里，所以为了保证start函数结束后，tcp_server类中的new_connection指针（shared_ptr）不会被立刻销毁，所以需要enable_shared_from_this
        boost::asio::async_write(socket_, boost::asio::buffer(message_),
            
        // shared_from_this()函数会安全地创建一个新的shared_ptr并且和new_connection共享一个计数
            [self = shared_from_this()](const boost::system::error_code& error,
                                      size_t /*bytes_transferred*/)
            {
                self->handle_write(error);
            });
    }

private:
    tcp_connection(boost::asio::io_context& io_context)
      : socket_(io_context)
    {
    }

    void handle_write(const boost::system::error_code& /*error*/)
    {
    }

    tcp::socket socket_;
    std::string message_;
};

// 唯一的工作就是接收连接，然后把工作转包出去
class tcp_server
{
public:
    tcp_server(boost::asio::io_context& io_context)
      : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 13))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        tcp_connection::pointer new_connection = tcp_connection::create(io_context_); // pointer就是shared_ptr
        // 这是一个异步等待，它告诉io_context等待一个客户端连接，当它到达时，把连接填充到new_connection的socket当中。
        // 当然，线程不会在这里阻塞
        // io_context管理的会话包括socket和acceptor_，由于acceptor的异步接受的工作尚未完成，此时线程会阻塞在io.run()那里
        acceptor_.async_accept(new_connection->socket(),
            
            [this, new_connection](const boost::system::error_code& error)
            {
                this->handle_accept(new_connection, error);
            });
    }

    void handle_accept(tcp_connection::pointer new_connection,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            // 告知tcp_connection对象开始执行其任务
            new_connection->start();
        }
        // 递归然后向io_context提供下一个异步接收任务
        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};


int main()
{
  try
  {
    boost::asio::io_context io_context; // 创建管家
    tcp_server server(io_context); // 创建总机
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

// const boost::system::error_code& 这个参数到底是什么
// 在同步变成中，我可以直接通过返回值来检查错误；但在异步编程中，async_dosomething并不阻塞线程
// 我们通过const boost::system::error_code& /*error*/在未来得知异步任务执行的结果
// 这个变量本身由io_context调用回调函数时提供，所以本身是const变量，我们只需要检查它就OK了
// 如果const boost::system::error_code& error值为false，即!error，则执行成功。反之，执行失败。

// 如果回调函数的第一个参数不是error_code，Asio也会尝试适配它。但我们并不应该这么去做。