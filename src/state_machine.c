#include <stdlib.h>
#include <strings.h>
#include "common.h"

oxo_sm *sm_new(int init_state,int max_state_number)
{
    int transition_size = max_state_number * max_state_number * sizeof(transition_handler_t);
    oxo_sm *sm = malloc(sizeof(oxo_sm));
    sm->current_state = init_state;
    sm->max_state_type = max_state_number;
    sm->transition = (transition_handler_t**)malloc(transition_size);
    bzero(sm->transition,transition_size);
    return sm;
}

void sm_free(oxo_sm *sm)
{
    if (sm) {
        if (sm->transition) {
            free(sm->transition);
        }
        free(sm);
    }
}

int sm_set_transition(oxo_sm *sm,int from_state,int to_state,transition_handler_t handler)
{
    if (from_state >= sm->max_state_type || to_state >= sm->max_state_type) {
        xlog("sm_set_transition: wrong state type");
        return -1;
    }
    sm->transition[from_state][to_state] = handler;
    return 0;
}

void sm_iterate(oxo_sm *sm,oxo_event *new_event)
{
    int orig_state = sm->current_state;
    int new_state = new_event->state;
    if (new_state >= sm->max_state_type) {
        xlog("sm_iterate: wrong new event type");
        return;
    }

    transition_handler_t handler = sm->transition[orig_state][new_state];
    if (handler) {
        new_state = handler(sm,new_event);
        if (new_state >= sm->max_state_type) {
            xlog("sm_iterate: wrong new event after transition");
            return;
        }
        sm->current_state = new_state;
    }
}
