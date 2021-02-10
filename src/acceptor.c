#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "io.h"
#include "common.h"
#include "proxy.h"


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

    return 0;
}
