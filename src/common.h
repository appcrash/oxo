#include <ev.h>

struct oxo_proxy;

typedef struct oxo_proxy_watcher
{
    ev_io io;
    struct oxo_proxy *proxy;
} oxo_proxy_watcher;


void wh_left_read_handler(EV_P_ ev_io *watcher,int revents);
void wh_left_write_handler(EV_P_ ev_io *watcher,int revents);
void wh_right_read_handler(EV_P_ ev_io *watcher,int revents);
void wh_right_write_handler(EV_P_ ev_io *watcher,int revents);

oxo_proxy_watcher *watcher_new(struct oxo_proxy *p);

void set_socket_nonblock(int s);
void set_socket_reuse(int s);

int diagnose_init(int dst);
void diagnose_log(char *type,char *data);
