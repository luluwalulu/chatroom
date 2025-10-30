#include <iostream>
#include <thread>
using namespace std;
void thread_1()
{
    while(1)
    {
        cout<<"子线程1111"<<endl;
    }
}
void thread_2(int x)
{
    while(1)
    {
        cout<<"子线程2222"<<endl;
    }
}
void detached_worker(){
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout<<"detached worker over"<<endl;
}

int main()
{
    // 1.线程一旦被创立，就马上开始执行
    thread t(detached_worker);

    // 2.线程需要指定以何种方式等待线程结束。t.join()会使主线程在此阻塞，直到线程执行完毕
    // t.detach()会让主线程在结束时直接粗暴地终止该线程，即使线程未执行完毕
    t.detach();          
    cout<<"主线程"<<endl;
    // 你可以看到，在这里线程永远无法执行完毕。
    return 0;
}
