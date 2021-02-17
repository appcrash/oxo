#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "common.h"

void set_socket_nonblock(int s)
{
    int flags;
    flags = fcntl(s,F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(s,F_SETFL, flags);
}

void set_socket_reuse(int s)
{
    int option = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0) {
        perror("setsockopt reuse failed");
    }
}

/* circular buffer */
cbuf *cbuf_new(int size)
{
    cbuf *buf;
    char *data = malloc(size);
    if (!data) {
        return NULL;
    }
    buf = malloc(sizeof(cbuf));
    if (!buf) {
        free(data);
        return NULL;
    }
    buf->size = size;
    buf->head = 0;
    buf->count = 0;
    buf->data = data;

    return buf;
}

void cbuf_free(cbuf *buf)
{
    if (!buf) {
        return;
    }
    if (buf->data){
        free(buf->data);
    }
    free(buf);
}

int cbuf_in(cbuf *buf,char *data,int len)
{
    int head = buf->head;
    int count = buf->count;
    int copied = 0,remain = 0;
    int left = buf->size - count;
    int tail = (head + count) % buf->size;

    if (0 == left) {
        return 0;
    }
    if (tail >= head) {
        copied = MIN(buf->size - tail,len);
        memcpy(&buf->data[tail],data,copied);
        if (len > copied) {
            remain = MIN(len - copied,head);
            memcpy(buf->data,&data[copied],remain);
            copied += remain;
        }
    } else {
        copied = MIN(head - tail,len);
        memcpy(&buf->data[tail],data,copied);
    }
    buf->count += copied;

    return copied;
}

int cbuf_out(cbuf *buf,char *data_out,int len)
{
    int head = buf->head;
    int count = buf->count;
    int copied;

    if (0 == count) {
        return 0;
    }
    len = MIN(len,count);
    copied = MIN(buf->size - head,len);
    memcpy(data_out,&buf->data[head],copied);
    if (len > copied) {
        memcpy(&data_out[copied],buf->data,len - copied);
        copied = len;
    }
    /* update head and count after data out */
    buf->head = (head + copied) % buf->size;
    buf->count -= copied;

    return copied;
}

/* single linked list */
list_node *list_new()
{
    list_node *node = malloc(sizeof(list_node));
    node->next = NULL;
    return node;
}

void list_free(list_node *node)
{
    if (node) {
        free(node);
    }
}


list_node *list_next(list_node *node)
{
    return node->next;
}

void list_insert_after(list_node *prev,list_node *node)
{
    list_node *n = prev->next;
    if (!n) {
        prev->next = node;
    } else {
        node->next = n;
        prev->next = node;
    }
}

void list_delete_after(list_node *node)
{
    list_node *next = node->next;
    if (next) {
        node->next = next->next;
        list_free(next);
    }
}
