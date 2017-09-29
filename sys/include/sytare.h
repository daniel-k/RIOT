#ifndef SYTARE_H
#define SYTARE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((weak))
size_t syt_os_sp;

__attribute__((weak))
size_t syt_usr_sp;

// syscall checkpoint pointer
__attribute__((weak))
size_t syt_syscall_ptr;

__attribute__((weak))
void drv_save_all(void) {}


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

#ifdef __cplusplus
}
#endif
/** @} */
#endif /* SYTARE_H */
