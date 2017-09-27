#ifndef _EVENTS_H_INCLUDED_
#define _EVENTS_H_INCLUDED_

#include "distenv.h"
#include "node.h"
#include "dllist.h"

extern void *event_loop(struct distenv *distenv);
extern void node_disconnect(struct dllist *list, struct node *node);

#endif
