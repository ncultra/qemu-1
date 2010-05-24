/*
 * event notifier support
 *
 * Copyright Red Hat, Inc. 2010
 *
 * Authors:
 *  Michael S. Tsirkin <mst@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_EVENT_NOTIFIER_H
#define QEMU_EVENT_NOTIFIER_H

#include "qemu-common.h"

struct EventNotifier {
    int rfd;
    int wfd;
};

typedef void EventNotifierHandler(EventNotifier *);

int event_notifier_init(EventNotifier *, int active);
void event_notifier_cleanup(EventNotifier *);
int event_notifier_get_fd(EventNotifier *);
int event_notifier_set(EventNotifier *);
int event_notifier_test_and_clear(EventNotifier *);
int event_notifier_set_handler(EventNotifier *, EventNotifierHandler *);

#endif
