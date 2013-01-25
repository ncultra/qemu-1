#include "qemu-common.h"
#include "qemu/main-loop.h"

bool qemu_mutex_lock_iothread(void)
{
    return false;
}

void qemu_mutex_unlock_iothread(void)
{
}
