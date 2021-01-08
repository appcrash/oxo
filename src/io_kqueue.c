#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stddef.h>
#include "io.h"

io_data *io_new_data(io_loop *loop,int fd,io_handler_t on_read,
                     io_handler_t on_write,io_handler_t on_error)
{
}

io_loop *io_new_loop()
{
}

static int _io_flag(int flag)
{
}

int io_add(io_data *data,int flag)
{
}

int io_del(io_data *data)
{
}

int io_enable(io_data *data,int io_flag)
{
}

int io_disable(io_data *data,int io_flag)
{
}

void io_loop_start(io_loop *loop)
{
}


static void _timer_timeout_callback(io_data *data)
{
}

io_timer *io_new_timer(io_loop *loop,timer_handler_t handler)
{
}

void io_timer_start(io_timer *timer,long timeout_ms)
{
}

void io_timer_stop(io_timer *timer)
{
}
