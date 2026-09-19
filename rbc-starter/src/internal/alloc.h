#ifndef RBC_INTERNAL_ALLOC_H
#define RBC_INTERNAL_ALLOC_H

#include <stddef.h>

void *rbc_alloc(size_t size);
void rbc_dealloc(void *ptr);

#endif
