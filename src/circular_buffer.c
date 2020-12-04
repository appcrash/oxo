#include <string.h>
#include <sys/param.h>
#include "proxy.h"

static void _proxy_buffer_get(oxo_proxy *p,int is_lr,char **buff,
                              unsigned int *head,unsigned int *count)
{
    if (is_lr) {
        *buff = p->lr_buffer;
        *head = p->lr_head;
        *count = p->lr_count;
    } else {
        *buff = p->rl_buffer;
        *head = p->rl_head;
        *count = p->rl_count;
    }
}


// insert data into circular buffer
static unsigned int _proxy_buffer_in(oxo_proxy *p,int is_lr,char *data,unsigned int len)
{
    char *buff;
    unsigned int head,count;
    _proxy_buffer_get(p, is_lr, &buff, &head, &count);

    unsigned int copied = 0,remain = 0;
    unsigned int left = PROXY_BUFFER_SIZE - count;
    unsigned int tail = (head + count) % PROXY_BUFFER_SIZE;
    if (0 == left) {
        return 0;
    }

    if (tail >= head) {
        copied = MIN(PROXY_BUFFER_SIZE - tail,len);
        memcpy(&buff[tail],data,copied);
        if (len > copied) {
            remain = MIN(len - copied,head);
            memcpy(buff,&data[copied],remain);
            copied += remain;
        }
    } else {
        copied = MIN(head - tail,len);
        memcpy(&buff[tail],data,copied);
    }

    // update count after data in (head stands still)
    if (is_lr) {
        p->lr_count += copied;
    } else {
        p->rl_count += copied;
    }
    return copied;
}


// consume data from circular buffer
static unsigned int _proxy_buffer_out(oxo_proxy *p,int is_lr,char *data,unsigned int len)
{
    char *buff;
    unsigned int head,count;
    unsigned int copied;

    _proxy_buffer_get(p, is_lr, &buff, &head, &count);
    if (0 == count) {
        return 0;
    }

    len = MIN(len,count);
    copied = MIN(PROXY_BUFFER_SIZE - head,len);
    memcpy(data,&buff[head],copied);
    if (len > copied) {
        memcpy(&data[copied],buff,len - copied);
        copied = len;
    }

    // update head and count after data out
    head = (head + copied) % PROXY_BUFFER_SIZE;
    if (is_lr) {
        p->lr_head = head;
        p->lr_count -= copied;
    } else {
        p->rl_head = head;
        p->rl_count -= copied;
    }
    return copied;
}

int proxy_buffer_lr_remain(oxo_proxy *p)
{
    return PROXY_BUFFER_SIZE - p->lr_count;
}

int proxy_buffer_rl_remain(oxo_proxy *p)
{
    return PROXY_BUFFER_SIZE - p->rl_count;
}

int proxy_buffer_lr_get(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_out(p, 1, data, len);
}

int proxy_buffer_lr_put(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_in(p, 1, data, len);
}

int proxy_buffer_rl_get(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_out(p, 0, data, len);
}

int proxy_buffer_rl_put(oxo_proxy *p,char *data,unsigned int len)
{
    return _proxy_buffer_in(p, 0, data, len);
}
