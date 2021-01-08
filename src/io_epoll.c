#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stddef.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include "io.h"

io_data *io_new_data(io_loop *loop,int fd,io_handler_t on_read,
                     io_handler_t on_write,io_handler_t on_error)
{
    io_data *data = (io_data*)malloc(sizeof(io_data));
    data->loop = loop;
    data->fd = fd;
    data->flag = 0;
    data->ptr = 0;
    data->on_read = on_read;
    data->on_write = on_write;
    data->on_error = on_error;
    return data;
}

io_loop *io_new_loop()
{
    io_loop *loop = (io_loop*)malloc(sizeof(io_loop));
    loop->fd = epoll_create1(0);
    return loop;
}

static int _io_flag(int flag)
{
    int osflag = 0;
    if (flag & IOF_READ) {
        osflag |= EPOLLIN;
    }
    if (flag & IOF_WRITE) {
        osflag |= EPOLLOUT;
    }
    if (flag & IOF_ERROR) {
        osflag |= EPOLLERR;
    }
    return osflag;
}

int io_add(io_data *data,int flag)
{
    struct epoll_event ev;
    ev.events = _io_flag(flag);
    ev.data.ptr = data;
    if (-1 == epoll_ctl(data->loop->fd, EPOLL_CTL_ADD, data->fd, &ev)) {
        perror("io_add error");
        return -1;
    }
    data->flag = flag;
    return 0;
}

int io_del(io_data *data)
{
    if (-1 == epoll_ctl(data->loop->fd, EPOLL_CTL_DEL, data->fd, NULL)) {
        perror("io_del error");
        return -1;
    }
    return 0;
}

int io_enable(io_data *data,int io_flag)
{
    int newflag = data->flag | io_flag;
    struct epoll_event ev;

    ev.events = _io_flag(newflag);
    ev.data.ptr = data;
    if (-1 == epoll_ctl(data->loop->fd, EPOLL_CTL_MOD, data->fd, &ev)) {
        perror("io_enable error");
        return -1;
    }
    data->flag = newflag;
    return 0;
}

int io_disable(io_data *data,int io_flag)
{
    int newflag = data->flag & ~io_flag;
    struct epoll_event ev;

    ev.events = _io_flag(newflag);
    ev.data.ptr = data;
    if (-1 == epoll_ctl(data->loop->fd, EPOLL_CTL_MOD, data->fd, &ev)) {
        perror("io_enable error");
        return -1;
    }
    data->flag = newflag;
    return 0;
}

void io_loop_start(io_loop *loop)
{
    int i,r;
    const int max_event = 64;
    struct epoll_event evs[max_event];

    while (1) {
        r = epoll_wait(loop->fd, evs, max_event, -1);
        if (-1 == r) {
            perror("epoll_wait error, exit ev loop");
            return;
        }
        for (i = 0; i < r; ++i) {
            struct epoll_event *ev = &evs[i];
            io_data *data = ev->data.ptr;
            if (ev->events & EPOLLIN) {
                data->on_read(data);
            }
            if (ev->events & EPOLLOUT) {
                data->on_write(data);
            }
            if (ev->events & EPOLLERR) {
                data->on_error(data);
            }
        }

    }
}


static void _timer_timeout_callback(io_data *data)
{
    io_timer *timer = (io_timer*)data;
    if (timer->handler) {
        timer->handler(timer);
    }
}

io_timer *io_new_timer(io_loop *loop,timer_handler_t handler)
{
    io_timer *timer = malloc(sizeof(io_timer));
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    timer->io_data.fd = fd;
    timer->io_data.loop = loop;
    timer->io_data.flag = 0;
    timer->io_data.on_read = _timer_timeout_callback; /* forwarder */
    timer->handler = handler;                         /* real handler */
    return timer;
}

void io_timer_start(io_timer *timer,long timeout_ms)
{
    struct itimerspec spec;

    bzero(&spec,sizeof(spec));

    long second = timeout_ms / 1000L;
    long nanosecond = (timeout_ms % 1000) * 1000L;
    spec.it_value.tv_sec = second;
    spec.it_value.tv_nsec = nanosecond;

    io_add((io_data*)timer,IOF_READ);
    timerfd_settime(timer->io_data.fd, 0, &spec, NULL);
}

void io_timer_stop(io_timer *timer)
{
    io_del((io_data*)timer);
}
