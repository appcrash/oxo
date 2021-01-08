#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "arpa/inet.h"
#include "sys/socket.h"
#include "sys/param.h"
#include "netinet/in.h"
#include "errno.h"

#include "common.h"
#include "proxy.h"
#include "io.h"

oxo_proxy_watcher* watcher_new(oxo_proxy *p)
{
    oxo_proxy_watcher *watcher = malloc(sizeof(oxo_proxy_watcher));
    watcher->proxy = p;
    return watcher;
}

/*
 * once proxy connection established, each of four directions(lr/rw) can
 * raise error due to all kinds of causes. the error handler just shutdown
 * bidrectional fds
 */
static void wh_on_error_handler(oxo_proxy *p,int flag)
{
    int left_flag,right_flag;
    int left_io_flag,right_io_flag;

    switch (flag) {
    case PROXY_FLAG_LEFT_READ:
    case PROXY_FLAG_RIGHT_WRITE:
        left_flag = PROXY_FLAG_LEFT_READ;
        right_flag = PROXY_FLAG_RIGHT_WRITE;
        left_io_flag = IOF_READ;
        right_io_flag = IOF_WRITE;
        break;
    case PROXY_FLAG_RIGHT_READ:
    case PROXY_FLAG_LEFT_WRITE:
        left_flag = PROXY_FLAG_LEFT_WRITE;
        right_flag = PROXY_FLAG_RIGHT_READ;
        left_io_flag = IOF_WRITE;
        right_io_flag = IOF_READ;

        proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
        proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
        break;
    default:
        D_PROXY(p, "error invalid error handler flag");
        return;
    }

    /* shutdown both directions of one flow */
    io_disable(p->left_io_data, left_io_flag);
    io_disable(p->right_io_data, right_io_flag);
    proxy_peer_shutdown(p, left_flag);
    proxy_peer_shutdown(p, right_flag);
}

void wh_left_read_handler(io_data *data)
{
    struct sockaddr_in addr;
    int s,n;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = (oxo_proxy*)data->ptr;

    if (PROXY_STATUS_LEFT_CONNECTED == p->status) {
        /* left connected in the first place
         * stop left watcher until right is connected */
        io_disable(data, IOF_READ);
        s = socket(AF_INET,SOCK_STREAM,0); /* remote peer socket */
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(p->remote_port);
        set_socket_nonblock(s);

        puts("connecting right");
        D_PROXY(p, "connect");
        /* add timeout and retry, connect would return -1(errno:EINPROGRESS) for non-block socket */
        connect(s,(struct sockaddr*)&addr,sizeof(addr));

        p->status = PROXY_STATUS_RIGHT_CONNECTING;
        io_data *id = io_new_data(data->loop, s, &wh_right_read_handler, &wh_right_write_handler,0);
        id->ptr = p;
        p->right_io_data = id;

        io_add(id,IOF_READ | IOF_WRITE);
    } else if (PROXY_STATUS_RIGHT_CONNECTING == p->status) {
        puts("error: should not come here when status is right connecting");
        D_PROXY(p, "error premature right connecting");
    } else if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        int remain = proxy_buffer_lr_remain(p);
        int sn,sent;
        if (0 == remain) {
            /* buffer is full, previous data is pending,
             * enable right write to consume them */
            io_disable(p->right_io_data, IOF_READ | IOF_WRITE);
            return;
        }

        puts("lr receiving");
        /* got data from left side, wrote them to right side asap,
         * but data cannot be sent under some heavy workload.
         * in this case put them to lr-buffer until writing ready
         * and disable left reading in the meanwhile.
         * note: use a conservative approach to read left data,
         * once recv syscall returned, app is responsible for data.
         * if we copy too much data from kernel to user space, ensure that
         * enough buffer is ready whenever send syscall cannot be satisfied
         * */
        remain = MIN(remain,PROXY_BUFFER_SIZE);
        while(1) {
            n = recv(data->fd,buff,remain,0);
            if (n < 0) {
                if (EAGAIN == errno) {
                    /* no data now */
                    return;
                } else {
                    perror("lr recv error");
                    return;
                }
            } else if (0 == n) {
                // left peer shutdown
                puts("left read shutdown, shutdown both directions");
                io_disable(p->left_io_data, IOF_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_LEFT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_WRITE);
                return;
            }

            sent = 0;
            while(n > sent) {
                sn = send(p->right_io_data->fd,&buff[sent],n - sent,0);
                if (-1 == sn) {
                    if (EAGAIN == errno) {
                        /* socket can not catch up with us
                         * put pending data into buffer */
                        proxy_buffer_lr_put(p, &buff[sent], n - sent);
                        io_enable(p->right_io_data, IOF_WRITE);
                        io_disable(p->left_io_data, IOF_READ);
                        return;
                    }
                    D_PROXY(p,"ioerror right write");
                    io_disable(p->left_io_data, IOF_READ);
                    proxy_peer_shutdown(p, PROXY_FLAG_LEFT_READ);
                    proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_WRITE);
                    return;
                }
                sent += sn;
            }
        }

    }
}

/*
 * this function is called when previous write failed(EAGAIN)
 * and write is ready right now, so purge buffer keeping pending
 * data left by last failed write then disable write event.
 */
void wh_left_write_handler(io_data *data)
{
    oxo_proxy *p = data->ptr;
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

    if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        puts("lr write ready");
        pending = proxy_buffer_rl_get(p, buff, PROXY_BUFFER_SIZE);
        if (0 == pending) {
            puts("rl write with 0 pending bytes");
            io_disable(p->left_io_data, IOF_WRITE);
            return;
        }
        while (pending > 0) {
            n = send(data->fd,&buff[i],pending - i,0);
            if (n < 0) {
                if (EAGAIN == errno) {
                    /* TODO: put the data back */
                } else {
                    wh_on_error_handler(p, PROXY_FLAG_LEFT_WRITE);
                }
                return;
            }
            i += n;
            pending -= n;
        }

        // flushed all pending buffer to socket buffer, disable EV_WRITE
        io_disable(p->left_io_data, IOF_WRITE);
        io_enable(p->right_io_data, IOF_READ);
    }

}

/*
 * similar with left read handler, read from one side and streams
 * it into the other side
 */
void wh_right_read_handler(io_data *data)
{
    int n,remain,sn,sent;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = data->ptr;

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        // notify left to read data in
        puts("notify left to read data");
        io_enable(p->left_io_data, IOF_READ);
        p->status = PROXY_STATUS_RIGHT_CONNECTED;
    } else if (p->status == PROXY_STATUS_RIGHT_CONNECTED) {
        remain = proxy_buffer_rl_remain(p);
        if (remain == 0) {
            /* buffer is full, enable left write to consume them */
            io_enable(p->left_io_data, IOF_WRITE);
            io_disable(p->right_io_data, IOF_READ);
            return;
        }

        puts("recv from right");
        remain = MIN(remain,PROXY_BUFFER_SIZE);
        while(1) {
            n = recv(data->fd,buff,remain,0);
            if (n < 0) {
                if (EAGAIN == errno) {
                    return;
                } else {
                    perror("rl recv error");
                    return;
                }
            } else if (0 == n) {
                // right peer shutdown
                puts("right read shutdown, shutdown both directions");
                io_disable(p->right_io_data, IOF_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
                return;
            }

            sent = 0;
            while (n > sent) {
                sn = send(p->left_io_data->fd,&buff[sent],n - sent,0);
                if (-1 == sn) {
                    if (EAGAIN == errno) {
                        proxy_buffer_rl_put(p, &buff[sent], n - sent);
                        io_enable(p->left_io_data, IOF_WRITE);
                        io_disable(p->right_io_data, IOF_READ);
                        return;
                    }
                    D_PROXY(p, "ioerror left write");
                    wh_on_error_handler(p, PROXY_FLAG_LEFT_WRITE);
                    return;
                }
                sent += sn;
            }
        }
    }

}

/*
 * similar with left write, purge pending data to right side
 */
void wh_right_write_handler(io_data *data)
{
    oxo_proxy *p = data->ptr;
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        /* notify left to read data in */
        puts("notify left to read data");
        io_disable(p->right_io_data, IOF_WRITE);
        io_enable(p->left_io_data, IOF_READ);
        p->status = PROXY_STATUS_RIGHT_CONNECTED;
    } else if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        pending = proxy_buffer_lr_get(p, buff, PROXY_BUFFER_SIZE);
        if (pending == 0) {
            puts("lr write with pending 0");
            io_disable(p->right_io_data, IOF_WRITE);
            return;
        }
        while (pending > 0) {
            printf("lr send with pending %d\n",pending);
            n = send(data->fd,&buff[i],pending - i,0);
            if (n < 0) {
                if (EAGAIN == n) {
                    puts("lr write with eagain");
                } else {
                    wh_on_error_handler(p, PROXY_FLAG_RIGHT_WRITE);
                }
                return;
            }
            i += n;
            pending -= n;
        }

        io_disable(p->right_io_data, IOF_WRITE);
    }
}
