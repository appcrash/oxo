#pragma once

#include <stdio.h>
#include <stddef.h>
#include <sys/param.h>
#include "io.h"

/* #ifndef MIN */
/* #define MIN(x,y) ((x < y) ? x : y) */
/* #endif */

/* #ifndef MAX */
/* #define MAX(x,y) ((x > y) ? x : y) */
/* #endif */



struct oxo_proxy;

typedef struct oxo_proxy_watcher
{
    io_data io;
    struct oxo_proxy *proxy;
} oxo_proxy_watcher;

#define OXO_PROXY(p_io) ((oxo_proxy_watcher*)p_io)->proxy

/* simple logger */
void xlog(const char *str);


/* circular buffer */
typedef struct cbuf
{
    char *data;
    int head,count,size;
} cbuf;
cbuf *cbuf_new(int size);
void cbuf_free(cbuf *buf);
int cbuf_in(cbuf *buf,char *data_in,int len);
int cbuf_out(cbuf *buf,char *data_out,int len);

/* single linked list */
typedef struct list_node
{
    struct list_node *next;
} list_node;
#define list_container_of(node,type,member) \
    ((type *)((char*)node - offsetof(type,member)))
list_node *list_new();
void list_free(list_node *node);
list_node *list_next(list_node *node);
void list_insert_after(list_node *prev,list_node *node);
void list_delete_after(list_node *node); /* delete node after this one */

/* state machine */
struct oxo_sm;
typedef struct oxo_event
{
    int state;
    void *data;
} oxo_event;
typedef int (*transition_handler_t)(struct oxo_sm *sm,struct oxo_event *new_event);

typedef struct oxo_sm
{
    transition_handler_t **transition; /* transition table (N x N) */
    int max_state_type; /* equals to N */
    int current_state;
} oxo_sm;
oxo_sm *sm_new(int init_state,int max_state_number);
void sm_free(oxo_sm *sm);
int sm_set_transition(oxo_sm *sm,int from_state,int to_state,transition_handler_t handler);
void sm_iterate(oxo_sm *sm,oxo_event *new_event);


void wh_left_read_handler(io_data *data);
void wh_left_write_handler(io_data *data);
void wh_right_read_handler(io_data *data);
void wh_right_write_handler(io_data *data);

oxo_proxy_watcher *watcher_new(struct oxo_proxy *p);

void set_socket_nonblock(int s);
void set_socket_reuse(int s);

int diagnose_init(int dst);
void diagnose_log(char *type,char *data);
#define D_PROXY(proxy,msg) {                    \
        if (proxy->diagnose) {                  \
            diagnose_log("proxy",msg);          \
        }                                       \
    }


/* callback return non-zero means repeat needed otherwise oneshot */
typedef int (*timer_callback_t)(void*);
typedef struct oxo_timer
{
    timer_callback_t cb;
    void *data;
} oxo_timer;
#define OXO_TIMER_CB(p_timer) ((oxo_timer*)p_timer)->cb

oxo_timer *timer_new(void *data);
void timer_destroy(oxo_timer *timer);
void timer_start(oxo_timer *timer,double after,timer_callback_t cb);
