#include "customAlloc.h"
#include <stdio.h>


char test_string[] = "This is a very original test string lol";
char bigger_test_string[] = "This is a bigger and more original test string taht is very nice for testing that things work";
char small_test_string[] = "test string, for test purpose";

void* test(size_t size) {
    void* a = customAlloc_malloc(size);
    printf("allocated memory at: %p\n", a);
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

    customAlloc_print_memspace();

    customAlloc_free(a);
    customAlloc_free(b);
    customAlloc_free(c);
    customAlloc_free(d);
    customAlloc_free(e);

    customAlloc_print_memspace();

    char* t = test(sizeof(test_string));

    memcpy(t, test_string, sizeof(test_string));

    printf("%s\n", t);

    char* big = customAlloc_realloc(t, sizeof(bigger_test_string));

    printf("%s\n", big);

    if(strcmp(big, test_string) != 0) {
        printf("1. strings are different tf ? %s / %s", big, test_string);
    }

    memcpy(big, bigger_test_string, sizeof(bigger_test_string));

    printf("%s\n", big);

    char* s = customAlloc_realloc(big, sizeof(small_test_string));

    if(strncmp(s, bigger_test_string, sizeof(small_test_string)) != 0) {
        printf("2. strings are different tf ? %s / %s", t, bigger_test_string);
    }

    customAlloc_free(s);


    customAlloc_unmap_all();

    return 0;
}
