#include "EventLoopThread.h"

//整个源文件的作用就是 把一个新线程和一个新loop 一一对应
//thread_ ==>调用回调threadFfunc==>thread_.start()开启线程


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startloop()
{
    thread_.start(); // 启动底层新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop=loop_;
    }
    return loop;
}

// 下面这个方法是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    // “One loop per thread”（每个线程一个事件循环）
    EventLoop loop; // 创建了一个独立的eventloop 和 上面的线程是一一对应的 one loop per thread模型
    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();//EventLoop loop => Poller.poll

    std::unique_lock<std::mutex> lock(mutex_);
    loop_=nullptr;
}