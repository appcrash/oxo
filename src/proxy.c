#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ev.h>
#include "io.h"

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



void proxy_accpet_cb(io_data *data)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int s;
    oxo_proxy *p = (oxo_proxy*)data->ptr;

    puts("accepting...");
    s = accept(data->fd,(struct sockaddr*)&addr,&addr_len);

    if (s < 0) {
        perror("accept error");
        return;
    }
    D_PROXY(p, "accept");

    set_socket_nonblock(s);

    p->status = PROXY_STATUS_LEFT_CONNECTED;
    io_data *id = io_new_data(data->loop, s,&wh_left_read_handler,&wh_left_write_handler,0);
    id->ptr = p;
    p->left_io_data = id;

    io_add(id,IOF_READ);        /* only enable read */
}


int proxy_start(oxo_proxy *p)
{
    int s;
    struct sockaddr_in addr;

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

    io_loop *loop = io_new_loop();
    io_data *id = io_new_data(loop,s,&proxy_accpet_cb,0,0);
    id->ptr = p;

    if (io_add(id,IOF_READ) == -1) {
        perror("io_add listen fd error");
        close(s);
        return -1;
    }
    io_loop_start(loop);

    puts("event loop done");
    return 0;
}


void proxy_peer_shutdown(oxo_proxy *p,int flag)
{
    int lfd = p->left_io_data->fd;
    int rfd = p->right_io_data->fd;

    switch(flag) {
    case PROXY_FLAG_LEFT_READ:
        shutdown(lfd,SHUT_RD);
        goto check_left;
    case PROXY_FLAG_LEFT_WRITE:
        shutdown(lfd,SHUT_WR);
        goto check_left;
    case PROXY_FLAG_RIGHT_READ:
        shutdown(rfd,SHUT_RD);
        goto check_right;
    case PROXY_FLAG_RIGHT_WRITE:
        shutdown(rfd,SHUT_WR);
        goto check_right;
    }

check_left:
    if (!(p->socket_status & (PROXY_FLAG_LEFT_READ | PROXY_FLAG_LEFT_WRITE))) {
        puts("close left fd");
        io_del(p->left_io_data);
        close(lfd);
    }
    return;

check_right:
    if (!(p->socket_status & (PROXY_FLAG_RIGHT_READ | PROXY_FLAG_RIGHT_WRITE))) {
        puts("close right fd");
        io_del(p->right_io_data);
        close(rfd);
    }

}
