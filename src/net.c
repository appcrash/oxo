#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "io.h"
#include "common.h"
#include "proxy.h"

/* acceptor */
static void accept_handler(io_data *data)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int s;
    oxo_accpt *accpt = (oxo_accpt*)data->ptr;

    puts("accepting...");
    s = accept(data->fd,(struct sockaddr*)&addr,&addr_len);

    if (s < 0) {
        perror("accept error");
        return;
    }

    set_socket_nonblock(s);
    accpt->on_new_connection(accpt->loop,s,accpt->handler_data);
}

oxo_accpt *accpt_new(io_loop *loop)
{
    oxo_accpt *accpt = (oxo_accpt*)malloc(sizeof(oxo_accpt));
    accpt->loop = loop;
    return accpt;
}


int accpt_start(oxo_accpt *accpt)
{
    int s;
    struct sockaddr_in addr;

    s = socket(AF_INET,SOCK_STREAM,0);
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(accpt->local_port);

    set_socket_reuse(s);
    if (bind(s,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
        perror("accpt_start bind failed");
        close(s);
        return -1;
    }

    if(listen(s,-1) == -1) {
        perror("listen failed");
        close(s);
        return -1;
    }

    io_loop * loop = accpt->loop;
    io_data *id = io_new_data(loop, s, &accept_handler, 0, 0);
    id->ptr = accpt;
    io_add(id, IOF_READ | IOF_WRITE);

    return 0;
}

/* connector */
oxo_connector *connector_new(io_loop *loop,connector_success_handler_t on_success,connector_timeout_handler_t on_timeout)
{
    oxo_connector *conn = malloc(sizeof(oxo_connector));
    conn->loop = loop;
    conn->on_success = on_success;
    conn->on_timeout = on_timeout;
    conn->io_skdata = NULL;
    conn->timer = NULL;
    return conn;
}

void connector_free(oxo_connector *conn)
{
    if (conn) {
        free(conn);
    }
}

static void connector_on_connected(io_data *data)
{
    oxo_connector *conn = (oxo_connector*)data->ptr;
    io_data *id = conn->io_skdata;

    /* disable write or the handler would be called again and again */
    io_disable(id, IOF_WRITE);
    conn->on_success(conn);
}

static void connector_on_timeout(io_timer *timer)
{
    oxo_connector *conn = (oxo_connector*)timer->io_data.ptr;
    int s = conn->socket;
    io_data *id = conn->io_skdata;
    int value;
    socklen_t len;

    if(getsockopt(s, SOL_SOCKET, SO_ERROR, &value, &len) < 0) {
        xlog("connector_on_timeout: getsockopt error");
    }
    if (value != 0) {
        printf("connect timeout with sock error %d",value);
    }

    /* clean up */
    io_del(id);
    close(s);
}

/* start connecting to remote */
void connector_start(oxo_connector *conn,char *ip,int port,long timeout_ms)
{
    struct sockaddr_in addr;
    int s;
    io_data *id = conn->io_skdata;
    io_timer *timeout_timer = conn->timer;

    s = socket(AF_INET,SOCK_STREAM,0);
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    set_socket_nonblock(s);
    conn->socket = s;

    xlog("connector: start connecting to remote");
    connect(s,(struct sockaddr*)&addr,sizeof(addr));

    if (!id) {
        id = io_new_data(conn->loop,s,0,&connector_on_connected,0);
        id->ptr = conn;
        conn->io_skdata = id;
    }
    io_add(id,IOF_WRITE);

    if (!timeout_timer) {
        timeout_timer = io_new_timer(conn->loop, &connector_on_timeout);
        timeout_timer->io_data.ptr = conn;
        conn->timer = timeout_timer;
    }
    io_timer_start(timeout_timer, timeout_ms);
}
