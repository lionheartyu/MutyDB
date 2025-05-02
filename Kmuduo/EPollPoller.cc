#include "EPollPoller.h"
#include "logger.h"
#include "Channel.h"
#include "Timestamp.h"
#include <errno.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include<strings.h>

// channel 未添加到poller中
const int kNew = -1; // channel中的成员index_ = -1
// channel 已添加到poller中
const int kAdded = 1;
// channel 从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上用LOG_DEBUG更为合适
    LOG_INFO("func=%s , fd total count %ld\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    // static_cast<int>(...)==>把 events_.size() 的返回值从 size_t 类型转换为 int 类型。

    int saveErrno = errno;

    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannel(numEvents, activeChannels);
        if (numEvents = events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {

        LOG_DEBUG("%dtimeout\n", timeoutMs);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPOllPOller::poll()err!");
        }
    }
    return now;
}
// channel update remove ==> eventloop updatechannel removechannel ==>poller updatechannel removechannel
/*
    EventLoop
    ChannelList   Poller
                  channelMap <fd,channel*>
*/
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s fd=%d event=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), channel->index());
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        else
        {
            // index = kDeleted
        }
        channel->setindex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel 以及在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->setindex(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel)
{

    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s fd=%d \n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setindex(kDeleted);
}

// 填写活跃的链接
void EPollPoller::fillActiveChannel(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);//EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}

// 更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    // memset(&event, 0, sizeof(event));
    bzero(&event,sizeof(event));
    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl_del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl_add/mod fatal:%d\n", errno);
        }
    }
}