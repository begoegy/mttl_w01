/* OFFLINE EXPERIMENT. Bounds sbrk; does not prove newlib thread safety. */
#include <stdint.h>
#include <stddef.h>
#include <sys/errno.h>
extern unsigned char __mttl_newlib_start[], __mttl_newlib_limit[];
static uintptr_t position;
void *_sbrk(int increment) {
    uint32_t ipsr, mask;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    if (ipsr) { errno = ENOMEM; return (void *)-1; }
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(mask) :: "memory");
    uintptr_t start = (uintptr_t)__mttl_newlib_start;
    uintptr_t limit = (uintptr_t)__mttl_newlib_limit;
    uintptr_t old = position ? position : start;
    uintptr_t amount = increment < 0 ? (uintptr_t)(-(int64_t)increment) : (uintptr_t)increment;
    int invalid = old < start || old > limit;
    if (!invalid) invalid = increment < 0 ? amount > old - start : amount > limit - old;
    if (!invalid) position = increment < 0 ? old - amount : old + amount;
    __asm volatile("msr primask, %0" :: "r"(mask) : "memory");
    if (invalid) { errno = ENOMEM; return (void *)-1; }
    return (void *)old;
}
