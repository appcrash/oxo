#include <stdlib.h>
#include "common.h"
#include "proxy.h"




static void _proxy_timer_cb(EV_P_ ev_timer *w,int revents)
{
    oxo_timer *timer = (oxo_timer*)w;
    timer_callback_t cb = timer->cb;
    if (!cb(timer->data)) {
        /* non-zero means repeat needed,
         * zero means oneshot timer callback finished */
        ev_timer_stop(EV_A, w);
        timer_destroy(timer);
    }
}

oxo_timer *timer_new(void *data)
{
    oxo_timer *t = malloc(sizeof(oxo_timer));
    t->data = data;
    return t;
}

void timer_destroy(oxo_timer *timer)
{
    if (!timer->data) {
        free(timer->data);
    }
    free(timer);
}

void timer_oneshot(oxo_timer *timer,double after,timer_callback_t cb)
{
    timer->cb = cb;
    ev_timer_init((ev_timer*)timer, _proxy_timer_cb, after, after);
    ev_timer_start(EV_DEFAULT, (ev_timer*)timer);
}
