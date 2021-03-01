#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"
#include "io.h"

#define CMD_CONNECT 0x01
#define ADDRTYPE_IPV4 0x01
#define ADDRTYPE_DOMAIN 0x03
#define ADDRTYPE_IPV6 0x04


typedef struct sock_state
{
    char buff[16];
    int init_count;
} sock_state;

typedef struct
{
    uint8_t  version;
    uint8_t method_num;
} header;

typedef struct
{
    uint8_t  version;
    uint8_t  cmd;
    uint8_t  reserved;
    uint8_t  address_type;
} request;

typedef struct
{
    uint8_t version;
    uint8_t resp;
    uint8_t reserved;
    uint8_t address_type;
} response;


static void socks_on_data_error(io_data *data)
{
    xlog("sock_on_data_error");
    io_del(data);
    close(data->fd);
}

static void socks_on_data_read_remote_binding(io_data *data)
{}


static void socks_on_data_read_request(io_data *data)
{
    int n;
    uint8_t len;
    uint16_t port;
    int s = data->fd;
    request req;
    char buff[256];

    n = recv(s,&req,4,0);
    if (n < 4 || req.version != 0x05) {
        goto err;
    }
    if (req.cmd != CMD_CONNECT || req.address_type == ADDRTYPE_IPV6) {
        /* only connect cmd supported */
        goto err;
    }

    /* only support ipv4/domain address type */
    switch (req.address_type) {
    case ADDRTYPE_IPV4:
        len = 4;
        break;
    case ADDRTYPE_DOMAIN:
        n = recv(s,&len,1,0);   /* first octet is the domain length */
        if (n < 0) {
            goto err;
        }
        break;
    default:
        goto err;
    }
    n = recv(s,buff,len,0);
    if (n < 0) {
        goto err;
    }
    n = recv(s,&port,2,0);
    if (n < 0) {
        goto err;
    }
    port = ntohs(port);

    /* TODO: now destination address and port are got,
     * send them to remote and get the binding address
     */

    data->on_read = &socks_on_data_read_remote_binding;
    return;
err:
    socks_on_data_error(data);
}


static void socks_on_data_read_header(io_data *data)
{
    int n;
    int s = data->fd;
    header h;
    char buff[256];
    //sock_state *state = (sock_state*)data->ptr;

    n = recv(s,&h,2,0);         /* get the header */
    if (n < 0 || h.version != 0x05 || h.method_num == 0) {
        goto err;
    }
    n = recv(s,buff,h.method_num,0); /* consume methods bytes */
    if (n < 0) {
        goto err;
    }
    n = send(s,"\x05\x00",2,0); /* send no-auth reply */
    if (n < 0) {
        goto err;
    }

    data->on_read = &socks_on_data_read_request;
    return;

err:
    socks_on_data_error(data);
}

static void socks_on_data_write(io_data *data)
{}

void socks_on_new_connection(io_loop *loop,int socket)
{
    io_data *id = io_new_data(loop,socket,&socks_on_data_read_header,&socks_on_data_write,&socks_on_data_error);
    sock_state *state = (sock_state*)malloc(sizeof(sock_state));
    bzero(state,sizeof(sock_state));

    id->ptr = state;
    io_add(id, IOF_READ);
}
