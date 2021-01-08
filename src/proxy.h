#pragma once

struct io_data;

#define PROXY_DIR_LEFT_RIGHT 1
#define PROXY_DIR_RIGHT_LEFT 2

#define PROXY_STATUS_LEFT_CONNECTED 1
#define PROXY_STATUS_RIGHT_CONNECTING 2
#define PROXY_STATUS_RIGHT_CONNECTED 3

#define PROXY_FLAG_LEFT_READ (1 << 0)
#define PROXY_FLAG_LEFT_WRITE (1 << 1)
#define PROXY_FLAG_RIGHT_READ (1 << 2)
#define PROXY_FLAG_RIGHT_WRITE (1 << 3)

#define PROXY_BUFFER_SIZE 4096

typedef void (*proxy_update_handler_t)(int dir,char *data,int len);

struct oxo_proxy_watcher;

typedef struct oxo_proxy {
    int local_port;
    int remote_port;

    char diagnose;

    struct io_data *left_io_data;
    struct io_data *right_io_data;

    int status;
    int socket_status;
    proxy_update_handler_t update_handler;

    // circular buffer for left->right and right->left flows
    char lr_buffer[PROXY_BUFFER_SIZE];
    unsigned int lr_head,lr_count;

    char rl_buffer[PROXY_BUFFER_SIZE];
    unsigned int rl_head,rl_count;

} oxo_proxy;

typedef struct oxo_connect
{
    struct oxo_proxy *proxy;
    struct sockaddr *addr;
} oxo_connect;


oxo_proxy* proxy_new(int local_port,int remote_port);
int proxy_start(oxo_proxy *p);
void proxy_flow_update(int dir,char *data,int len);
int proxy_buffer_lr_remain(oxo_proxy *p);
int proxy_buffer_rl_remain(oxo_proxy *p);
void proxy_peer_shutdown(oxo_proxy *p,int flag);

int proxy_buffer_lr_get(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_lr_put(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_rl_get(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_rl_put(oxo_proxy *p,char *data,unsigned int len);
