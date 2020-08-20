#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "arpa/inet.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "errno.h"

#include "common.h"
#include "proxy.h"


#define IO_PROXY(p_io) ((oxo_proxy_watcher*)p_io)->proxy


oxo_proxy_watcher* watcher_new(oxo_proxy *p)
{
    oxo_proxy_watcher *watcher = malloc(sizeof(oxo_proxy_watcher));
    watcher->proxy = p;
    return watcher;
}


void wh_left_read_handler(EV_P_ ev_io *watcher,int revents)
{
    struct sockaddr_in addr;
    int s,n;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = IO_PROXY(watcher);

    if (PROXY_STATUS_LEFT_CONNECTED == p->status) {
        // left connected in the first place
        s = socket(AF_INET,SOCK_STREAM,0);
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(p->remote_port);
        puts("connecting right");
        if (connect(s,(struct sockaddr*)&addr,sizeof(addr)) != 0) {
            perror("connect to remote failed");
            close(s);
            return;
        }

        ev_io_init((ev_io*)p->right_read_watcher,wh_right_read_handler,s,EV_READ);
        ev_io_init((ev_io*)p->right_write_watcher,wh_right_write_handler,s,EV_WRITE);
        p->status = PROXY_STATUS_RIGHT_CONNECTING;
        proxy_event_enable(p,PROXY_FLAG_RIGHT_READ);
        proxy_event_enable(p,PROXY_FLAG_RIGHT_WRITE);
        // stop left watcher until right is connected
        proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
    } else if (PROXY_STATUS_RIGHT_CONNECTING == p->status) {
        puts("error: should not come here when status is right connecting");
    } else if (PROXY_STATUS_RIGHT_CONNECTED == p->status) {
        int remain = proxy_buffer_lr_remain(p);
        if (0 == remain) {
            return;
        }

        puts("lr receiving");
        n = recv(watcher->fd,buff,remain,0);
        if (n < 0) {
            perror("lr recv error");
            return;
        } else if (0 == n) {
            // left peer shutdown
            puts("left read shutdown, shutdown both directions");
            proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
            proxy_peer_shutdown(p, PROXY_FLAG_LEFT_READ);
            proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_WRITE);
            return;
        }

        printf("put left data(%d) to buffer\n",n);
        proxy_buffer_lr_put(p, buff, n);
        proxy_event_enable(p, PROXY_FLAG_RIGHT_WRITE);
    }
}

void wh_left_write_handler(EV_P_ ev_io *watcher,int revents)
{
    oxo_proxy *p = IO_PROXY(watcher);
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

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
                if (EAGAIN == n) {
                    puts("lr write with eagain");
                } else {
                    perror("lr write with error,shutdown both directions");
                    proxy_event_disable(p, PROXY_FLAG_LEFT_WRITE);
                    proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
                    proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
                }
                return;
            }
            i += n;
            pending -= n;
        }

        // flushed all pending buffer to socket buffer, disable EV_WRITE
        proxy_event_disable(p, PROXY_FLAG_LEFT_WRITE);
    }

}


void wh_right_read_handler(EV_P_ ev_io *watcher,int revents)
{
    int n,remain;
    char buff[PROXY_BUFFER_SIZE];
    oxo_proxy *p = IO_PROXY(watcher);

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        // notify left to read data in
        puts("notify left to read data");
        proxy_event_enable(p, PROXY_FLAG_LEFT_READ);
        p->status = PROXY_STATUS_RIGHT_CONNECTED;
    } else if (p->status == PROXY_STATUS_RIGHT_CONNECTED) {
        remain = proxy_buffer_rl_remain(p);
        if (remain == 0) {
            return;
        }

        puts("recv from right");
        n = recv(watcher->fd,buff,remain,0);
        if (n < 0) {
            perror("rl recv error");
        } else if (0 == n) {
            // peer shutdown
            puts("right read shutdown, shutdown both directions");
            proxy_event_disable(p, PROXY_FLAG_RIGHT_READ);
            proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_READ);
            proxy_peer_shutdown(p, PROXY_FLAG_LEFT_WRITE);
        } else {
            puts("put right data to buffer");
            proxy_buffer_rl_put(p, buff, n);
            proxy_event_enable(p, PROXY_FLAG_LEFT_WRITE);
        }

    }


}

void wh_right_write_handler(EV_P_ ev_io *watcher,int revents)
{
    oxo_proxy *p = IO_PROXY(watcher);
    int pending,n,i = 0;
    char buff[PROXY_BUFFER_SIZE];

    if (p->status == PROXY_STATUS_RIGHT_CONNECTING) {
        // notify left to read data in
        puts("notify left to read data");
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
                    perror("lr write with error, shutdown both directions");
                    proxy_event_disable(p, PROXY_FLAG_RIGHT_WRITE);
                    proxy_event_disable(p, PROXY_FLAG_LEFT_READ);
                    proxy_peer_shutdown(p, PROXY_FLAG_RIGHT_WRITE);
                    proxy_peer_shutdown(p, PROXY_FLAG_LEFT_READ);
                }
                return;
            }
            i += n;
            pending -= n;
        }

        proxy_event_disable(p, PROXY_FLAG_RIGHT_WRITE);
    }
}
