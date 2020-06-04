#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include <ev.h>

#include "common.h"
#include "proxy.h"

int setnonblock(int fd) {
    int flags;
    flags = fcntl(fd,F_GETFD);
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFD, flags);
}


void client_cb(EV_P_ ev_io *watcher, int revents) {
    char buffer[1024];
    ssize_t n_read;

    bzero(buffer,1024);
    if (revents & EV_READ) {
        puts("reading ...");
        n_read = recv(watcher->fd,buffer,1024,0);
        if (n_read < 0) {
            perror("read error");
        } else if (n_read == 0) {
            ev_io_stop(EV_A_ watcher);
            free(watcher);
        } else {
            puts(buffer);
        }
    }

    if (revents & EV_WRITE) {
        puts("writing ready");

    }
}

void accept_cb(EV_P_ ev_io *watcher, int revents) {
    struct sockaddr_in addr;
    socklen_t addr_len;
    int s;
    oxo_proxy *p = proxy_new();


    if (revents & EV_ERROR) {
        perror("error happened");
        return;
    }
    if (revents & EV_READ) {
        puts("accepting...");
        s = accept(watcher->fd,(struct sockaddr*)&addr,&addr_len);

        if (s < 0) {
            perror("accept error");
            return;
        }
        setnonblock(s);

        p->status = PROXY_STATUS_LEFT_CONNECTED;
        p->left_read_watcher = watcher_new(p);
        p->left_write_watcher = watcher_new(p);
        p->right_read_watcher = watcher_new(p);
        p->right_write_watcher = watcher_new(p);
        ev_io_init((ev_io*)p->left_read_watcher, wh_left_read_handler, s,EV_READ);
        ev_io_init((ev_io*)p->left_write_watcher, wh_left_write_handler, s, EV_WRITE);
        proxy_event_enable(p, PROXY_FLAG_LEFT_READ);
    }
}



int main() {
    int s,option;
    struct sockaddr_in addr;
    ev_io watcher_accept;
    struct ev_loop *loop = EV_DEFAULT;
    s = socket(AF_INET,SOCK_STREAM,0);

    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);

    if (setsockopt(s, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), (char*)&option, sizeof(option)) < 0) {
        perror("setsockopt failed");
        close(s);
        return -1;
    }
    if (bind(s,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
        perror("bind failed");
        close(s);
        return -1;
    }

    if(listen(s,-1) == -1) {
        perror("listen failed");
        close(s);
        return -1;
    }

    ev_io_init(&watcher_accept, accept_cb, s, EV_READ | EV_WRITE);
    ev_io_start(loop, &watcher_accept);

    ev_loop(loop,0);

    puts("ev_loop done");
    return 0;
}
