#ifndef HEADER_CUSTOM_ALLOC_H
#define HEADER_CUSTOM_ALLOC_H

#include <stdlib.h>
#include <string.h>

void* customAlloc_malloc(size_t size);
void customAlloc_free(void* p);
void customAlloc_unmap_all();

// reallocating with new_size <= old_size is garanty to return same ptr
// if p is NULL, allocate memory for requested size
// if size is 0 deallocate the pointer and return NULL
void* customAlloc_realloc(void* p, size_t new_size);


void customAlloc_print_memspace();

#endif