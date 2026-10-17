#ifndef MTTL_DEADLINE_H
#define MTTL_DEADLINE_H
#include <stdint.h>
/* Budgets are below 2^31 ms; unsigned subtraction tolerates tick wrap. */
static inline uint32_t mttl_remaining(uint32_t start,uint32_t now,uint32_t budget){
    uint32_t elapsed=now-start;
    return elapsed>=budget?0:budget-elapsed;
}
#endif
