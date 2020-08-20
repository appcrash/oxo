#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ev.h>

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
    if (diagnose_port <= 0) {
        puts("invalid diagnose port argument");
        return -1;
    }
    return 0;
}


int main(int argc,char **argv) {
    oxo_proxy *p;

    if (parse_args(argc, argv) < 0) {
        exit(-1);
    }
    p = proxy_new(local_port, remote_port);
    if (diagnose_port > 0) {
        p->diagnose_port = diagnose_port;
    }

    proxy_start(p);
    return 0;
}
