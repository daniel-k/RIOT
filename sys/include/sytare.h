#ifndef SYTARE_H
#define SYTARE_H

#include <stddef.h>
#include <msp430.h>

#ifdef __cplusplus
extern "C" {
#endif

extern size_t syt_os_sp;
extern size_t syt_usr_sp;
extern size_t syt_syscall_ptr;


inline __attribute__((always_inline))
static void syt_run_os_stack(void)
{
    // save USR stack pointer and change stack pointer USR -> SYS
    __asm__ __volatile__(   "mov r1, %0 \n\t"
                            "mov %1, r1\n\t"
                            : "=m" (syt_usr_sp)
                            : "m" (syt_os_sp)
                            );
}

inline __attribute__((always_inline))
static void syt_run_usr_stack(void)
{
    // save USR stack pointer and change stack pointer SYS -> USR
    __asm__ __volatile__(   "mov r1, %0 \n\t"
                            "mov %1, r1\n\t"
                            : "=m" (syt_os_sp)
                            : "m" (syt_usr_sp)
                            );
}

static inline __attribute__((always_inline))
void syt_syscall_enter(void)
{
    // save processor state
    __asm__ __volatile__("push r2 \n\t"
                         : // no outputs
                         : // no inputs
                         );
    // then ensure disabling interrupts for OS operations
    _disable_interrupts();

    //save GPRs (R[4-15])
    __asm__ __volatile__("pushm #12, r15 \n\t"
                         : // no outputs
                         : // no inputs
                         );

    syt_run_os_stack();


    __asm__ __volatile__("pushm #4, r15 \n\t"
                         : // no outputs
                         : // no inputs
                         );

    void drv_save_all(void);
    drv_save_all();

    __asm__ __volatile__("popm #4, r15 \n\t"
                         : // no outputs
                         : // no inputs
                         );


    // save PC in a fram variable
    __asm__ __volatile__("mov %0, r10 \n\t"
                         "mov r0, @r10 \n\t"
                         :
                         : "m"(syt_syscall_ptr)
                         );

    // re-enable interrupts for driver function call
    _enable_interrupts();
}


static inline __attribute__((always_inline))
void syt_syscall_exit(void)
{
    // at driver return disable again interrupts for os operations
    _disable_interrupts();

    syt_run_usr_stack();

    // clear the syscall reference
    __asm__ __volatile__("mov %0, r10 \n\t"
                         "mov #0, @r10 \n\t"
                         :
                         : "m"(syt_syscall_ptr)
                         );


    // repopulate callee saved registers
    // clear the stack (#8 = 4 regs not restored)
    // restore SR
    __asm__ __volatile__("popm #8, r11 \n\t"
                         "add #8, r1 \n\t"
                         "pop r2 \n\t"
                         : // no outputs
                         : // no inputs
                         );
}



/// Function pointer for driver save function
typedef void (*drv_save_func_t)(int handle);

/// Function pointer for driver restore function
typedef void (*drv_restore_func_t)(int handle);


/**
 * @brief Register driver to the kernel for persistency
 *
 * Note: Has to be a macro because the ctx_size must be known at compile time,
 *       it cannot be a function argument (even inlined).
 *
 * @param save      Pointer to save function or NULL if not required
 * @param restore   Pointer to restore function or NULL if not required
 * @param ctx_size  Compile-time constant size of device context
 *
 * @return          Handle that identifies driver vis-a-vis the kernel
 */
#define syt_drv_register(save, restore, ctx_size) ({ \
    /* allocate context in checkpoint, therefore make it static */ \
    static __attribute__((section(".dev_ctx"))) \
    uint8_t __dev_ctx[ctx_size]; \
    \
    /* Prevent the compiler from optimizing the context away */ \
    __asm__ __volatile__("" :: "r" (&__dev_ctx)); \
    \
    /* Wrap internal register function to allocate context in checkpoint. Since \
     * we're in a macro, this will expand and allocate memory for every individual \
     * call by each driver. */ \
    int drv_register(drv_save_func_t save, drv_restore_func_t restore, size_t size); \
    drv_register(save, restore, ctx_size); \
})


/**
 * @brief Get pointer to the device context corresponding in "last" checkpoint
 *
 * @param handle    Handle provided by kernel
 * @return          Pointer to memory region in checkpoint reserved for context
 */
const void* syt_drv_get_ctx_last(int handle);


/**
 * @brief Get pointer to the device context corresponding in "next" checkpoint
 *
 * @param handle    Handle provided by kernel
 * @return          Pointer to memory region in checkpoint reserved for context
 */
void* syt_drv_get_ctx_next(int handle);


// Driver helpers ------------------------------------------------------------//
// These functions may help implementing partial dirtiness of device contexts to
// optimize save and restoration time. Each driver can choose to implement this.

/// Holds information about dirtiness, has to be passed to all related functions
typedef struct {
    void*  addr;    ///< address of dirty region (NULL if clean, SIZE_MAX if all dirty)
    size_t length;  ///< length of dirty region
} syt_dev_ctx_changes_t;


/**
 * @brief Mark context clean, so drv_save() won't do anything
 *
 * @param ctx_changes   Pointer to dirtiness information structure
 */
void drv_mark_clean(syt_dev_ctx_changes_t* ctx_changes);


/**
 * @brief Mark context dirty, so drv_save() will copy the whole context
 *
 * @param ctx_changes   Pointer to dirtiness information structure
 */
void drv_mark_dirty(syt_dev_ctx_changes_t* ctx_changes);


/**
 * @brief Mark context partially dirty, so drv_save() only copies dirty parts
 *
 * If multiple regions are marked dirty they will be merged into one region that
 * contains both, possibly including non-dirty parts.
 *
 * @param ctx_changes   Pointer to dirtiness information structure
 * @param addr
 * @param length
 */
void drv_dirty_range(syt_dev_ctx_changes_t* ctx_changes, void* addr, size_t length);


/**
 * @brief Copy dirty parts from one context to another if neccessary
 *
 * If @p ctx_from is dirty as indicated by @p ctx_changes, copy dirty parts to
 * @p ctx_to.
 *
 * @param ctx_changes   Pointer to dirtiness information structure
 * @param ctx_to
 * @param ctx_from
 * @param ctx_size
 */
void drv_save(syt_dev_ctx_changes_t* ctx_changes, void* ctx_to, void* ctx_from, size_t ctx_size);


#ifdef __cplusplus
}
#endif
/** @} */
#endif /* SYTARE_H */
