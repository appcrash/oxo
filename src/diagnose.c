#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>

#include "common.h"

static int dskt = 0;
static struct sockaddr_in daddr;

/* send diagnose msg by udp
 * each msg is separated by space:
 * TYPE DATA1 DATA2 ...
 * |TYPE|
 * |init| sent once diagnose module initialized
 **/

int diagnose_init(int dst)
{
    const char init[] = "init";

    dskt = socket(AF_INET, SOCK_DGRAM, 0);
    if (dskt < 0) {
        perror("initialize diagnose socket failed");
        return -1;
    }
    set_socket_nonblock(dskt);
    //set_socket_reuse(dskt);

    bzero(&daddr,sizeof(daddr));
    daddr.sin_family = AF_INET;
    daddr.sin_port = htons(dst);
    daddr.sin_addr.s_addr = INADDR_ANY;

    if (sendto(dskt,init,strlen(init),0,
               (const struct sockaddr*)&daddr,sizeof(daddr)) < 0) {
        perror("diagnose init msg sent failed");
        return -1;
    }

    return 0;
}

void diagnose_log(char *type,char *data)
{
    int len;
    char buff[1024];
    len = snprintf(buff, 1024, "%s %s", type,data);

    /* TODO: use libev to handle EAGAIN */
    if(sendto(dskt,buff,len,0,
              (const struct sockaddr*)&daddr,sizeof(daddr)) < 0) {
        perror("diagnose log sent failed");
    }
}
