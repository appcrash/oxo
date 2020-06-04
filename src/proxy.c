#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ev.h>


#include "common.h"
#include "proxy.h"

void proxy_flow_update(int dir,char *data,int len) {
    if (PROXY_DIR_RIGHT_LEFT == dir) {

    } else if (PROXY_DIR_LEFT_RIGHT == dir) {

    } else {

    }
}

oxo_proxy* proxy_new() {
    oxo_proxy* p = malloc(sizeof(oxo_proxy));
    bzero(p,sizeof(oxo_proxy));
    p->status = 0;
    p->update_handler = proxy_flow_update;

    return p;
}


static void _proxy_buffer_get(oxo_proxy *p,int is_lr,char **buff,
                              unsigned int *head,unsigned int *count)
{
    if (is_lr) {
        *buff = p->lr_buffer;
        *head = p->lr_head;
        *count = p->lr_count;
    } else {
        *buff = p->rl_buffer;
        *head = p->rl_head;
        *count = p->rl_count;
    }
}


// insert data into circular buffer
static unsigned int _proxy_buffer_in(oxo_proxy *p,int is_lr,char *data,unsigned int len)
{
    char *buff;
    unsigned int head,count;
    _proxy_buffer_get(p, is_lr, &buff, &head, &count);

    unsigned int copied = 0,remain = 0;
    unsigned int left = PROXY_BUFFER_SIZE - count;
    unsigned int tail = (head + count) % PROXY_BUFFER_SIZE;
    if (0 == left) {
        return 0;
    }

    if (tail >= head) {
        copied = MIN(PROXY_BUFFER_SIZE - tail,len);
        memcpy(&buff[tail],data,copied);
        if (len > copied) {
            remain = MIN(len - copied,head);
            memcpy(buff,&data[copied],remain);
            copied += remain;
        }
    } else {
        copied = MIN(head - tail,len);
        memcpy(&buff[tail],data,copied);
    }

    // update count after data in (head stands still)
    if (is_lr) {
        p->lr_count += copied;
    } else {
        p->rl_count += copied;
    }
    return copied;
}


// consume data from circular buffer
static unsigned int _proxy_buffer_out(oxo_proxy *p,int is_lr,char *data,unsigned int len)
{
    char *buff;
    unsigned int head,count;
    unsigned int copied;

    _proxy_buffer_get(p, is_lr, &buff, &head, &count);
    if (0 == count) {
        return 0;
    }

    len = MIN(len,count);
    copied = MIN(PROXY_BUFFER_SIZE - head,len);
    memcpy(data,&buff[head],copied);
    if (len > copied) {
        memcpy(&data[copied],buff,len - copied);
        copied = len;
    }

    // update head and count after data out
    head = (head + copied) % PROXY_BUFFER_SIZE;
    if (is_lr) {
        p->lr_head = head;
        p->lr_count -= copied;
    } else {
        p->rl_head = head;
        p->rl_count -= copied;
    }
    return copied;
}

int proxy_buffer_lr_remain(oxo_proxy *p)
{
    return PROXY_BUFFER_SIZE - p->lr_count;
}

int proxy_buffer_rl_remain(oxo_proxy *p)
{
    return PROXY_BUFFER_SIZE - p->rl_count;
}

int proxy_buffer_lr_get(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_out(p, 1, data, len);
}

int proxy_buffer_lr_put(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_in(p, 1, data, len);
}

int proxy_buffer_rl_get(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_out(p, 0, data, len);
}

int proxy_buffer_rl_put(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_in(p, 0, data, len);
}

void proxy_event_enable(oxo_proxy *p,int flag)
{
    struct ev_loop *loop = EV_DEFAULT;
    ev_io *io;

    switch(flag) {
    case PROXY_FLAG_LEFT_READ:
        io = (ev_io*)p->left_read_watcher;
        break;
    case PROXY_FLAG_LEFT_WRITE:
        io = (ev_io*)p->left_write_watcher;
        break;
    case PROXY_FLAG_RIGHT_READ:
        io = (ev_io*)p->right_read_watcher;
        break;
    case PROXY_FLAG_RIGHT_WRITE:
        io = (ev_io*)p->right_write_watcher;
        break;
    }

    ev_io_start(loop, io);
    p->socket_status |= flag;
}

void proxy_event_disable(oxo_proxy *p,int flag)
{
    struct ev_loop *loop = EV_DEFAULT;
    ev_io *io;

    switch(flag) {
    case PROXY_FLAG_LEFT_READ:
        io = (ev_io*)p->left_read_watcher;
        break;
    case PROXY_FLAG_LEFT_WRITE:
        io = (ev_io*)p->left_write_watcher;
        break;
    case PROXY_FLAG_RIGHT_READ:
        io = (ev_io*)p->right_read_watcher;
        break;
    case PROXY_FLAG_RIGHT_WRITE:
        io = (ev_io*)p->right_write_watcher;
        break;
    }

    ev_io_stop(loop, io);
    p->socket_status &= ~flag;
}

void proxy_peer_shutdown(oxo_proxy *p,int flag)
{
    switch(flag) {
    case PROXY_FLAG_LEFT_READ:
        shutdown(((ev_io*)p->left_read_watcher)->fd,SHUT_RD);
        goto check_left;
    case PROXY_FLAG_LEFT_WRITE:
        shutdown(((ev_io*)p->left_write_watcher)->fd,SHUT_WR);
        goto check_left;
    case PROXY_FLAG_RIGHT_READ:
        shutdown(((ev_io*)p->right_read_watcher)->fd,SHUT_RD);
        goto check_right;
    case PROXY_FLAG_RIGHT_WRITE:
        shutdown(((ev_io*)p->right_write_watcher)->fd,SHUT_WR);
        goto check_right;
    }

check_left:
    if (!(p->socket_status & (PROXY_FLAG_LEFT_READ | PROXY_FLAG_LEFT_WRITE))) {
        puts("close left fd");
        close(((ev_io*)p->left_read_watcher)->fd);
    }
    return;

check_right:
    if (!(p->socket_status & (PROXY_FLAG_RIGHT_READ | PROXY_FLAG_RIGHT_WRITE))) {
        puts("close right fd");
        close(((ev_io*)p->right_read_watcher)->fd);
    }

}
