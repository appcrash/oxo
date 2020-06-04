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
    struct oxo_proxy_watcher *left_read_watcher;
    struct oxo_proxy_watcher *left_write_watcher;
    struct oxo_proxy_watcher *right_read_watcher;
    struct oxo_proxy_watcher *right_write_watcher;
    int status;
    int socket_status;
    proxy_update_handler_t update_handler;

    // circular buffer for left->right and right->left flows
    char lr_buffer[PROXY_BUFFER_SIZE];
    unsigned int lr_head,lr_count;

    char rl_buffer[PROXY_BUFFER_SIZE];
    unsigned int rl_head,rl_count;

} oxo_proxy;

oxo_proxy* proxy_new();
void proxy_flow_update(int dir,char *data,int len);
int proxy_buffer_lr_remain(oxo_proxy *p);
int proxy_buffer_rl_remain(oxo_proxy *p);
void proxy_event_enable(oxo_proxy *p,int flag);
void proxy_event_disable(oxo_proxy *p,int flag);
void proxy_peer_shutdown(oxo_proxy *p,int flag);

int proxy_buffer_lr_get(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_lr_put(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_rl_get(oxo_proxy *p,char *data,unsigned int len);
int proxy_buffer_rl_put(oxo_proxy *p,char *data,unsigned int len);
