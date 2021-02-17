#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "proxy.h"

static int local_port = 0;
static int remote_port = 0;
static int diagnose_port = 0; // the port to receive diagnose udp msg


int parse_args(int argc,char **argv)
{
    int c;

    while ((c = getopt(argc,argv,"d:l:r:")) != -1) {
        switch(c) {
        case 'l':
            local_port = atoi(optarg);
            break;
        case 'r':
            remote_port = atoi(optarg);
            break;
        case 'd':
            diagnose_port = atoi(optarg);
            break;
        default:
            return -1;
        }
    }

    if (local_port <= 0) {
        puts("invalid local port argument");
        return -1;
    }
    if (remote_port <= 0) {
        puts("invalid remote port argument");
        return -1;
    }
    if (diagnose_port < 0) {
        puts("invalid diagnose port argument");
        return -1;
    }
    return 0;
}

static void on_new_connection(io_loop *loop,int socket,void *data)
{
    oxo_proxy *p = (oxo_proxy*)data;

    p->status = PROXY_STATUS_LEFT_CONNECTED;
    io_data *id = io_new_data(loop, socket,&wh_left_read_handler,&wh_left_write_handler,0);
    id->ptr = p;
    p->left_io_data = id;

    io_add(id,IOF_READ);        /* only enable read */
}


int main(int argc,char **argv) {
    oxo_accpt *accpt;

    if (parse_args(argc, argv) < 0) {
        exit(-1);
    }
    if (diagnose_port > 0) {
        if(diagnose_init(diagnose_port) < 0) {
            exit(-1);
        }
    }
    accpt = accpt_new();
    accpt->loop = io_new_loop();
    accpt->local_port = local_port;
    accpt->on_new_connection = on_new_connection;
    accpt_start(accpt);
    io_loop_start(accpt->loop);


    /* p = proxy_new(local_port, remote_port); */
    /* p->diagnose = (diagnose_port > 0) ? 1 : 0; */
    /* proxy_start(p); */
    return 0;
}
