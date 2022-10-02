#ifndef HEADER_CUSTOM_ALLOC_H
#define HEADER_CUSTOM_ALLOC_H

#include <stdlib.h>
#include <string.h>

void* customAlloc_malloc(size_t size);
void customAlloc_free(void* p);
void customAlloc_unmap_all();

#endif