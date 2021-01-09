#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#include "io.h"

io_data *io_new_data(io_loop *loop,int fd,io_handler_t on_read,
                     io_handler_t on_write,io_handler_t on_error)
{
    io_data *data = (io_data*)malloc(sizeof(io_data));
    bzero(data,sizeof(io_data));
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
    loop->fd = kqueue();
    loop->new_timer_id = 1;
    return loop;
}


static int _io_kevent_do(io_loop *loop,int fd,short filter,u_short flags,u_int fflags,int64_t data,void *udata)
{
    struct kevent event;
    EV_SET(&event, fd, filter, flags, fflags, data, udata);
    return kevent(loop->fd,&event,1,NULL,0,NULL);
}

int io_add(io_data *data,int flag)
{
    if (flag & IOF_READ) {
        _io_kevent_do(data->loop, data->fd, EVFILT_READ, EV_ADD, 0, 0, data);
    }
    if (flag & IOF_WRITE) {
        _io_kevent_do(data->loop, data->fd, EVFILT_WRITE, EV_ADD, 0, 0, data);
    }
    return 0;
}


int io_del(io_data *data)
{
    int flag = data->flag;
    if (flag & IOF_READ) {
        _io_kevent_do(data->loop, data->fd, EVFILT_READ, EV_DELETE, 0, 0, data);
    }
    if (flag & IOF_WRITE) {
        _io_kevent_do(data->loop, data->fd, EVFILT_WRITE, EV_DELETE, 0, 0, data);
    }
    return 0;
}


int io_enable(io_data *data,int io_flag)
{
    int newflag = data->flag | io_flag;
    if (io_flag & IOF_READ) {
        _io_kevent_do(data->loop, data->fd, EVFILT_READ, EV_ENABLE, 0, 0, data);
    }
    if (io_flag & IOF_WRITE) {
        _io_kevent_do(data->loop, data->fd, EVFILT_WRITE, EV_ENABLE, 0, 0, data);
    }

    data->flag = newflag;
    return 0;
}


int io_disable(io_data *data,int io_flag)
{
    int newflag = data->flag & ~io_flag;
    if (io_flag & IOF_READ) {
        _io_kevent_do(data->loop, data->fd, EVFILT_READ, EV_DISABLE, 0, 0, data);
    }
    if (io_flag & IOF_WRITE) {
        _io_kevent_do(data->loop, data->fd, EVFILT_WRITE, EV_DISABLE, 0, 0, data);
    }

    data->flag = newflag;
    return 0;
}


static int _io_kevent_handler(struct kevent *event)
{
    io_data *data = event->udata;

    if (event->flags & EV_ERROR) {
        return -1;
    }

    if (event->filter == EVFILT_READ && data->on_read) {
        data->on_read(data);
    }
    if (event->filter == EVFILT_WRITE && data->on_write) {
        data->on_write(data);
    }
    if (event->filter == EVFILT_TIMER) {
        io_timer *timer = (io_timer*)data;
        if (timer->handler) {
            timer->handler(timer);
        }
    }

    return 0;
}



void io_loop_start(io_loop *loop)
{
    int i,r;
    const int max_event = 64;
    struct kevent events[max_event];

    while (1) {
        r = kevent(loop->fd,NULL,0,events,max_event,NULL);

        if (-1 == r) {
            perror("kevent poll error, exit io loop");
            return;
        }
        for (i = 0; i < r; ++i) {
            if(_io_kevent_handler(&events[i]) < 0) {
                perror("kevent error event, exit io loop");
                return;
            }

        }

    }
}


io_timer *io_new_timer(io_loop *loop,timer_handler_t handler)
{
    io_timer *timer = malloc(sizeof(io_timer));

    bzero(timer,sizeof(io_timer));
    timer->io_data.loop = loop;
    timer->handler = handler;                         /* real handler */
    return timer;
}

void io_timer_start(io_timer *timer,long timeout_ms)
{
    io_loop *loop = timer->io_data.loop;
    int id = loop->new_timer_id++; /* self-increasing id for each new timer */
    _io_kevent_do(loop, id, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_MSECONDS, timeout_ms, (io_data*)timer);
}

void io_timer_stop(io_timer *timer)
{
    /* unused, as EV_ONESHOT will remove timer from kqueue once timer expired and being read */
    (void)timer;
}
