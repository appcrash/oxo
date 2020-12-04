#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

oxo_proxy* proxy_new(int local_port,int remote_port) {
    oxo_proxy* p = malloc(sizeof(oxo_proxy));
    bzero(p,sizeof(oxo_proxy));
    p->local_port = local_port;
    p->remote_port = remote_port;
    p->diagnose = 0;
    p->status = 0;
    p->update_handler = proxy_flow_update;

    return p;
}



void proxy_accpet_cb(EV_P_ ev_io *watcher, int revents) {
    struct sockaddr_in addr;
    socklen_t addr_len;
    int s;
    oxo_proxy *p = OXO_PROXY(watcher);

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
        D_PROXY(p, "accept");

        set_socket_nonblock(s);

        p->status = PROXY_STATUS_LEFT_CONNECTED;
        p->left_socket = s;
        p->left_read_watcher = watcher_new(p);
        p->left_write_watcher = watcher_new(p);
        p->right_read_watcher = watcher_new(p);
        p->right_write_watcher = watcher_new(p);
        ev_io_init((ev_io*)p->left_read_watcher, wh_left_read_handler, s,EV_READ);
        ev_io_init((ev_io*)p->left_write_watcher, wh_left_write_handler, s, EV_WRITE);
        proxy_event_enable(p, PROXY_FLAG_LEFT_READ);
    }
}


int proxy_start(oxo_proxy *p)
{
    int s;
    struct sockaddr_in addr;
    oxo_proxy_watcher watcher_accept;
    struct ev_loop *loop = EV_DEFAULT;

    s = socket(AF_INET,SOCK_STREAM,0);
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(p->local_port);

    set_socket_reuse(s);
    if (bind(s,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
        perror("proxy_start bind failed");
        close(s);
        return -1;
    }

    if(listen(s,-1) == -1) {
        perror("listen failed");
        close(s);
        return -1;
    }

    watcher_accept.proxy = p;
    ev_io_init((ev_io*)&watcher_accept, proxy_accpet_cb, s, EV_READ | EV_WRITE);
    ev_io_start(loop, (ev_io*)&watcher_accept);

    ev_loop(loop,0);

    puts("ev_loop done");
    return 0;
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
