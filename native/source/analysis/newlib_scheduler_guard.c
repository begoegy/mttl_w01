/* Offline experiment: serialize external newlib entry calls on one Cortex-M3.
 * Coverage of archive-internal calls and real-time latency remain unverified. */
#include <stddef.h>
#include <stdint.h>
struct _reent;
extern void vTaskSuspendAll(void);
extern int xTaskResumeAll(void);
extern void *__real__malloc_r(struct _reent *, size_t);
extern void __real__free_r(struct _reent *, void *);
extern void *__real__realloc_r(struct _reent *, void *, size_t);
extern void *__real__calloc_r(struct _reent *, size_t, size_t);
static int in_interrupt(void) {
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr != 0;
}
void *__wrap__malloc_r(struct _reent *r, size_t n) {
    if (in_interrupt()) return NULL;
    vTaskSuspendAll();
    void *p = __real__malloc_r(r, n);
    xTaskResumeAll(); return p;
}
void __wrap__free_r(struct _reent *r, void *p) {
    if (in_interrupt()) return;
    vTaskSuspendAll(); __real__free_r(r, p); xTaskResumeAll();
}
void *__wrap__realloc_r(struct _reent *r, void *p, size_t n) {
    if (in_interrupt()) return NULL;
    vTaskSuspendAll();
    void *q = __real__realloc_r(r, p, n);
    xTaskResumeAll(); return q;
}
void *__wrap__calloc_r(struct _reent *r, size_t n, size_t size) {
    if (in_interrupt() || (size && n > (size_t)-1 / size)) return NULL;
    vTaskSuspendAll();
    void *p = __real__calloc_r(r, n, size);
    xTaskResumeAll(); return p;
}
