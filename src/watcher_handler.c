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
    int f1,f2;

    switch (flag) {
    case PROXY_FLAG_LEFT_READ:
    case PROXY_FLAG_RIGHT_WRITE:
        f1 = PROXY_FLAG_LEFT_READ;
        f2 = PROXY_FLAG_RIGHT_WRITE;
        break;
    case PROXY_FLAG_RIGHT_READ:
    case PROXY_FLAG_LEFT_WRITE:
        f1 = PROXY_FLAG_RIGHT_READ;
        f2 = PROXY_FLAG_LEFT_WRITE;

        proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
        proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
        break;
    default:
        D_PROXY(p, "error invalid error handler flag");
        return;
    }

    /* shutdown both directions of one flow */
    proxy_event_disable(p, f1);
    proxy_event_disable(p, f2);
    proxy_peer_shutdown(p, f1);
    proxy_peer_shutdown(p, f2);
}

void wh_left_read_handler(EV_P_ ev_io *watcher,int revents)
{
    struct sockaddr_in addr;
    int s,n;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = OXO_PROXY(watcher);

    if (revents & EV_ERROR) {
        wh_on_error_handler(p, PROXY_FLAG_LEFT_READ);
        D_PROXY(p, "ioerror left read");
        return;
    }

    if (PROXY_STATUS_LEFT_CONNECTED == p->status) {
        /* left connected in the first place
         * stop left watcher until right is connected */
        proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
        s = socket(AF_INET,SOCK_STREAM,0);
        ev_io_init((ev_io*)p->right_read_watcher,wh_right_read_handler,s,EV_READ);
        ev_io_init((ev_io*)p->right_write_watcher,wh_right_write_handler,s,EV_WRITE);
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(p->remote_port);
        set_socket_nonblock(s);
        p->right_socket = s;

        puts("connecting right");
        D_PROXY(p, "connect");
        /* add timeout and retry, connect would return -1(errno:EINPROGRESS) for non-block socket */
        connect(s,(struct sockaddr*)&addr,sizeof(addr));

        p->status = PROXY_STATUS_RIGHT_CONNECTING;
        proxy_event_enable(p,PROXY_FLAG_RIGHT_READ);
        proxy_event_enable(p,PROXY_FLAG_RIGHT_WRITE);
    } else if (PROXY_STATUS_RIGHT_CONNECTING == p->status) {
        puts("error: should not come here when status is right connecting");
        D_PROXY(p, "error premature right connecting");
    } else if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        int remain = proxy_buffer_lr_remain(p);
        int sn,sent;
        if (0 == remain) {
            /* buffer is full, previous data is pending,
             * enable right write to consume them */
            proxy_event_enable(p, PROXY_FLAG_RIGHT_WRITE);
            proxy_event_disable(p,PROXY_FLAG_LEFT_READ);
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
            n = recv(watcher->fd,buff,remain,0);
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
                proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_LEFT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_WRITE);
                return;
            }

            sent = 0;
            while(n > sent) {
                sn = send(p->right_socket,&buff[sent],n - sent,0);
                if (-1 == sn) {
                    if (EAGAIN == errno) {
                        /* socket can not catch up with us
                         * put pending data into buffer */
                        proxy_buffer_lr_put(p, &buff[sent], n - sent);
                        proxy_event_enable(p, PROXY_FLAG_RIGHT_WRITE);
                        proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
                        return;
                    }
                    D_PROXY(p,"ioerror right write");
                    proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
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
void wh_left_write_handler(EV_P_ ev_io *watcher,int revents)
{
    oxo_proxy *p = OXO_PROXY(watcher);
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

    if (revents & EV_ERROR) {
        wh_on_error_handler(p, PROXY_FLAG_LEFT_WRITE);
        D_PROXY(p, "ioerror left write");
        return;
    }

    if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        puts("lr write ready");
        pending = proxy_buffer_rl_get(p, buff, PROXY_BUFFER_SIZE);
        if (0 == pending) {
            puts("rl write with 0 pending bytes");
            proxy_event_disable(p, PROXY_FLAG_LEFT_WRITE);
            return;
        }
        while (pending > 0) {
            n = send(watcher->fd,&buff[i],pending - i,0);
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
        proxy_event_disable(p, PROXY_FLAG_LEFT_WRITE);
        proxy_event_enable(p,PROXY_FLAG_RIGHT_READ);
    }

}

/*
 * similar with left read handler, read from one side and streams
 * it into the other side
 */
void wh_right_read_handler(EV_P_ ev_io *watcher,int revents)
{
    int n,remain,sn,sent;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = OXO_PROXY(watcher);

    if (revents & EV_ERROR) {
        wh_on_error_handler(p, PROXY_FLAG_RIGHT_READ);
        D_PROXY(p, "ioerror right read");
        return;
    }

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        // notify left to read data in
        puts("notify left to read data");
        proxy_event_enable(p, PROXY_FLAG_LEFT_READ);
        p->status = PROXY_STATUS_RIGHT_CONNECTED;
    } else if (p->status == PROXY_STATUS_RIGHT_CONNECTED) {
        remain = proxy_buffer_rl_remain(p);
        if (remain == 0) {
            /* buffer is full, enable left write to consume them */
            proxy_event_enable(p, PROXY_FLAG_LEFT_WRITE);
            proxy_event_disable(p, PROXY_FLAG_RIGHT_READ);
            return;
        }

        puts("recv from right");
        remain = MIN(remain,PROXY_BUFFER_SIZE);
        while(1) {
            n = recv(watcher->fd,buff,remain,0);
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
                proxy_event_disable(p, PROXY_FLAG_RIGHT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
                proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
                return;
            }

            sent = 0;
            while (n > sent) {
                sn = send(p->left_socket,&buff[sent],n - sent,0);
                if (-1 == sn) {
                    if (EAGAIN == errno) {
                        proxy_buffer_rl_put(p, &buff[sent], n - sent);
                        proxy_event_enable(p, PROXY_FLAG_LEFT_WRITE);
                        proxy_event_disable(p, PROXY_FLAG_RIGHT_READ);
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
void wh_right_write_handler(EV_P_ ev_io *watcher,int revents)
{
    oxo_proxy *p = OXO_PROXY(watcher);
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

    if (revents & EV_ERROR) {
        wh_on_error_handler(p, PROXY_FLAG_RIGHT_WRITE);
        D_PROXY(p, "ioerror right write");
        return;
    }

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        /* notify left to read data in */
        puts("notify left to read data");
        proxy_event_disable(p, PROXY_FLAG_RIGHT_WRITE);
        proxy_event_enable(p, PROXY_FLAG_LEFT_READ);
        p->status = PROXY_STATUS_RIGHT_CONNECTED;
    } else if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        pending = proxy_buffer_lr_get(p, buff, PROXY_BUFFER_SIZE);
        if (pending == 0) {
            puts("lr write with pending 0");
            proxy_event_disable(p, PROXY_FLAG_RIGHT_WRITE);
            return;
        }
        while (pending > 0) {
            printf("lr send with pending %d\n",pending);
            n = send(watcher->fd,&buff[i],pending - i,0);
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

        proxy_event_disable(p, PROXY_FLAG_RIGHT_WRITE);
    }
}
