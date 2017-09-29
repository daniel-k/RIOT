#include <stddef.h>
#include "sytare.h"

/*
 * Everything here is defined empty and weak in order to make RIOT compile. But
 * when linking against Sytare the right symbols will be chosen from Sytare.
 */

__attribute__((weak))
size_t syt_os_sp;

__attribute__((weak))
size_t syt_usr_sp;

// syscall checkpoint pointer
__attribute__((weak))
size_t syt_syscall_ptr;

__attribute__((weak))
void drv_save_all(void)
{
}

__attribute__((weak))
int drv_register(drv_save_func_t save, drv_restore_func_t restore, size_t size)
{
    (void) save;
    (void) restore;
    (void) size;

    return 0;
}

__attribute__((weak))
const void* syt_drv_get_ctx_last(int handle)
{
    (void) handle;
    return NULL;
}

__attribute__((weak))
void* syt_drv_get_ctx_next(int handle)
{
    (void) handle;
    return NULL;
}

__attribute__((weak))
void drv_mark_clean(syt_dev_ctx_changes_t* ctx_changes)
{
    (void) ctx_changes;
}


__attribute__((weak))
void drv_mark_dirty(syt_dev_ctx_changes_t* ctx_changes)
{
    (void) ctx_changes;
}

__attribute__((weak))
void drv_dirty_range(syt_dev_ctx_changes_t* ctx_changes, void* addr, size_t length)
{
    (void) ctx_changes;
    (void) addr;
    (void) length;
}


__attribute__((weak))
void drv_save(syt_dev_ctx_changes_t* ctx_changes, void* ctx_to, void* ctx_from, size_t ctx_size)
{
    (void) ctx_changes;
    (void) ctx_to;
    (void) ctx_from;
    (void) ctx_size;
}
