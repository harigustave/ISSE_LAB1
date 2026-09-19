#include "internal/alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef RBC_TESTING
#include "alloc_control.h"

static size_t failure_countdown = SIZE_MAX;
static size_t attempt_count;
static size_t live_count;

void
rbc_alloc_test_reset(void)
{
    failure_countdown = SIZE_MAX;
    attempt_count = 0;
}

void
rbc_alloc_test_fail_after(size_t successful_calls_before_failure)
{
    failure_countdown = successful_calls_before_failure;
    attempt_count = 0;
}

size_t
rbc_alloc_test_attempt_count(void)
{
    return attempt_count;
}

size_t
rbc_alloc_test_live_count(void)
{
    return live_count;
}
#endif

void *
rbc_alloc(size_t size)
{
    void *ptr;

#ifdef RBC_TESTING
    ++attempt_count;
    if (failure_countdown == 0) {
        return NULL;
    }
#endif

    ptr = malloc(size);
    if (ptr == NULL) {
        return NULL;
    }

#ifdef RBC_TESTING
    if (failure_countdown != SIZE_MAX) {
        --failure_countdown;
    }
    ++live_count;
#endif

    return ptr;
}

void
rbc_dealloc(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

#ifdef RBC_TESTING
    assert(live_count > 0);
    --live_count;
#endif
    free(ptr);
}
