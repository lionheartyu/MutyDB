#include <mymuduo/TcpServer.h>
#include <string>
#include <mymuduo/logger.h>
#include <functional>
#include"kvstore.h"
#include <iostream>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name),
          loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量  loopthread
        server_.setThreadNum(3);
    }
    
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("conn up:%s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("conn down:%s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        struct conn_item item={0};
		strcpy(item.rbuffer,msg.c_str());
        kvstore_requese(&item);
        conn->send(item.wbuffer);
    }

    EventLoop *loop_;
    TcpServer server_;
};

int kmuduo_enrty()
{
    EventLoop loop;
    InetAddress addr(8080, "192.168.217.148");
    EchoServer echoServer(&loop, addr, "myserver-01"); // accpertor non-blocking listenfd create bind
    echoServer.start();                                // listen  loopthread  listenfd => accpetorchannle => mainloop
    loop.loop();                                       // 启动mainloop的底层poller
    return 0;
}