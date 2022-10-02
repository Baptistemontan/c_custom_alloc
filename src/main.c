#include "customAlloc.h"
#include <stdio.h>

void* test(size_t size) {
    void* a = customAlloc_malloc(size);
    printf("%p\n", a);
    return a;
}

int main(int argc, char const *argv[])
{
    void* a = test(0x100);
    void* b = test(1 << 19);
    void* c = test(0x100);
    void* d = test(1 << 19);
    void* e = test(0x100);

    customAlloc_free(a);
    customAlloc_free(b);
    customAlloc_free(c);
    customAlloc_free(d);
    customAlloc_free(e);

    printf("-------------------------------------\n");
    printf("               NEXT\n");
    printf("-------------------------------------\n");

    a = test(0x100);
    b = test(1 << 19);
    c = test(0x100);
    d = test(1 << 19);
    e = test(0x100);

    customAlloc_free(a);
    customAlloc_free(b);
    customAlloc_free(c);
    customAlloc_free(d);
    customAlloc_free(e);

    customAlloc_unmap_all();

    return 0;
}
