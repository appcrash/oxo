#pragma once

struct io_data;
struct io_timer;
typedef void (*io_handler_t)(struct io_data *data);
typedef void (*timer_handler_t)(struct io_timer *timer);


#define IOF_READ (1 << 0)
#define IOF_WRITE (1 << 1)
#define IOF_ERROR (1 << 2)

typedef struct io_loop
{
    int fd;                     /* loop fd */
#if defined(__FreeBSD__) || defined(__MACH__)
    int new_timer_id;           /* identifier for new timer, self-increasing */
#endif
} io_loop;

typedef struct io_data
{
    io_loop *loop;
    int fd;                     /* io fd, init/changed by add/del/enable/disable */
    int flag;                   /* read/write/error watcher */

    void *ptr;                  /* custom data ptr */
    io_handler_t on_read;
    io_handler_t on_write;
    io_handler_t on_error;
} io_data;

typedef struct io_timer
{
    io_data io_data;
    timer_handler_t handler;
} io_timer;


io_loop *io_new_loop();
io_data *io_new_data(io_loop *loop,int fd,io_handler_t on_read,io_handler_t on_write,io_handler_t on_error);


int io_add(io_data *data,int flag);
int io_del(io_data *data);
int io_enable(io_data *data,int io_flag);
int io_disable(io_data *data,int io_flag);
void io_loop_start(io_loop *loop); /* enter loop, wouldn't return until all done */

io_timer *io_new_timer(io_loop *loop,timer_handler_t handler);
void io_timer_start(io_timer *timer,long timeout_ms);
void io_timer_stop(io_timer *timer);

typedef void (*accpt_handler_t)(struct io_loop *loop,int new_socket,void *handler_data);
typedef struct oxo_accpt
{
    int local_port;
    void *handler_data;
    accpt_handler_t on_new_connection;

    struct io_loop *loop;
} oxo_accpt;
oxo_accpt *accpt_new(struct io_loop *loop);
int accpt_start(oxo_accpt *accpt);

struct oxo_connector;
typedef void (*connector_success_handler_t)(struct oxo_connector*);
typedef void (*connector_timeout_handler_t)(struct oxo_connector*);
typedef struct oxo_connector
{
    connector_success_handler_t on_success;
    connector_timeout_handler_t on_timeout;

    int socket;
    struct io_loop *loop;
    io_data *io_skdata;
    io_timer *timer;
} oxo_connector;
oxo_connector *connector_new(struct io_loop *loop,connector_success_handler_t on_success,connector_timeout_handler_t on_timeout);
void connector_free(oxo_connector *conn);
void connector_start(oxo_connector *conn,char *ip,int port,long timeout_ms);
