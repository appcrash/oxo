#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>

void set_socket_nonblock(int s)
{
    int flags;
    flags = fcntl(s,F_GETFD);
    flags |= O_NONBLOCK;
    fcntl(s,F_SETFD, flags);
}

void set_socket_reuse(int s)
{
    int option;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0) {
        perror("setsockopt reuse failed");
    }
}
