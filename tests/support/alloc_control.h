#ifndef RBC_ALLOC_CONTROL_H
#define RBC_ALLOC_CONTROL_H

#include <stddef.h>

void
rbc_alloc_test_reset(void);

void
rbc_alloc_test_fail_after(size_t successful_calls_before_failure);

size_t
rbc_alloc_test_attempt_count(void);

size_t
rbc_alloc_test_live_count(void);

#endif
