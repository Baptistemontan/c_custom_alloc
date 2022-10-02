#include "customAlloc.h"
#include <stdio.h>
#include <sys/mman.h>

#define IS_USED(p) (GET_VALUE(p) & 1) // is the bloc currently used
#define SIZE(p) (GET_VALUE(p) & ~0b11) // get size of block
#define NEXT(p) ((p) + SIZE(p)) // get header of next bloc from header of current header
#define PREV(p) ((p) - SIZE(p - sizeof(size_t))) // get header of previous bloc from current header
#define FOOTER(p) (NEXT(p) - sizeof(size_t)) // get footer from header
#define HEADER(p) (p - SIZE(p) + sizeof(size_t)) // get header from footer
#define IS_END(p) (GET_VALUE(p) & 0b10) // is end marker
#define IS_START(p) (GET_VALUE(p) & 0b11) // is start marker

#define SET_USED(p) (SET_VALUE(p, GET_VALUE(p) | 0b1)) // set bloc as used
#define SET_FREE(p) (SET_VALUE(p, GET_VALUE(p) & ~0b1)) // set bloc as unused
#define SET_END(p) (SET_VALUE(p, 0b10)) // set end marker
#define NEXT_BLOC(p) (*(void**)(p + sizeof(size_t)))
#define PADDING_SIZE (5 * sizeof(size_t)) // header/footer + end/start marker + next bloc addr

#define CLEAR(p) (SET_VALUE(p, 0))

#define BASE_ALLOC_SIZE (1 << 16) // 2^16 might be a big minimum for each bloc but meh whatever


static void* base_memory_space = NULL;

size_t GET_VALUE(size_t* p) {
    return *p;
}

void SET_VALUE(size_t* p, size_t value) {
    *p = value;
}

void SET_SIZE(size_t* p, size_t size) {
    SET_VALUE(p, (size | IS_USED(p)));
}


static void init_bloc(void* mem, size_t size) {
    size_t bloc_size = size - PADDING_SIZE + 2 * sizeof(size_t); // still keep size of header/footer
    SET_VALUE(mem, size | 0b11);
    mem += sizeof(size_t);
    // init header
    CLEAR(mem);
    SET_SIZE(mem, bloc_size);
    // init footer
    CLEAR(FOOTER(mem));
    SET_SIZE(FOOTER(mem), bloc_size);
    // end marker
    SET_END(NEXT(mem));
    SET_VALUE(NEXT(mem) + sizeof(size_t), (size_t)NULL);
}

static void* alloc_memory_space(size_t min_size) {

    if(BASE_ALLOC_SIZE < (min_size + PADDING_SIZE)) {
        // make min_size multiple of 8
        min_size &= ~0b111;
        // add some space for padding
        // + add 8 in case troncanetion made it smaller
        min_size += PADDING_SIZE + 8;
    } else {
        min_size = BASE_ALLOC_SIZE;
    }

    fprintf(stderr, "allocating memory bloc of size = %lu\n", min_size);

    void* m = mmap(0, min_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(m == MAP_FAILED) {
        fprintf(stderr, "failed to allocate memory bloc\n");
        return NULL;
    } else {
        fprintf(stderr, "succesfully allocated memory bloc at %p\n", m);
        init_bloc(m, min_size);
        return m;
    }
}


static void* realloc_memory_space(void* prev_bloc_end_marker, size_t min_size) { 
    fprintf(stderr, "reallocating memory\n");

    void* m = alloc_memory_space(min_size);

    if(m == NULL) {
        return NULL;
    }

    SET_VALUE(prev_bloc_end_marker + sizeof(size_t), (size_t)m);

    return m;
}



void* customAlloc_malloc(size_t size) {
    if(base_memory_space == NULL) {
        void* m = alloc_memory_space(size);
        if(m == NULL) {
            return NULL;
        }
        base_memory_space = m;
    }
    if (size == 0) { 
        // so if size is 0, 2 ways, return NULL or valid pointer
        // I'm lazy so first option 
        // (could be end marker but don't really want user to overwrite that)
        return NULL;
    }
    size_t original_size = size;
    //first alloc good size

    // make size a multiple of 8
    if(size & 0b111) {
        size &= ~0b111; 
        size += 8;
    }

    size += 2 * sizeof(size_t); // for header and footer
    // I'm lazy so first fit strategy
    void* p = base_memory_space + sizeof(size_t);
    fprintf(stderr, "start: %p, looking for: %lu\n", base_memory_space, size);

    search_loop:
    // fprintf(stderr, "looking for space: %p, size: %lu (needed: %lu), used: %lu\n", p, SIZE(p), size, IS_USED(p));
    while (!IS_END(p) && (IS_USED(p) || SIZE(p) < size)) {
        fprintf(stderr, "looking for space: %p, size: %lu (needed: %lu), used: %lu\n", p, SIZE(p), size, IS_USED(p));
        p = NEXT(p);
    }

    if (IS_END(p)) {
        void* next_block = NEXT_BLOC(p);
        fprintf(stderr, "next block %p\n", next_block);
        if(next_block == NULL) {
            fprintf(stderr, "realloc\n");
            // allocate more space
            next_block = realloc_memory_space(p, original_size);
            fprintf(stderr, "reallocated %p\n", next_block);
            if(next_block == NULL) {
                // reallocation failed
                return NULL;
            }
        }
        p = next_block + sizeof(size_t);
        

        goto search_loop; // yeah goto bad blah blah blah IDGAF
    }
    fprintf(stderr, "found space: %p\n", p);
    size_t prevSize = SIZE(p);
    size_t newSize = prevSize - size;
    if(newSize >= 3 * sizeof(size_t)) {
        SET_SIZE(FOOTER(p), newSize); // set old footer to new free size
        CLEAR(HEADER(FOOTER(p))); // clear new header
        SET_SIZE(HEADER(FOOTER(p)), newSize); // set new header to new free size
        SET_SIZE(p, size); // set old header to new used size
        CLEAR(FOOTER(p)); // clear new footer
        SET_SIZE(FOOTER(p), size); // set new footer to new used size
    }
    SET_USED(p);
    SET_USED(FOOTER(p));
    return p + sizeof(size_t); // return pointer to data
}

void customAlloc_free(void* p) {
    if (p == NULL) {
        return;
    }
    p -= sizeof(size_t);
    if (!IS_USED(p)) {
        fprintf(stderr, "Attempt to free unallocated memory\n");
        exit(1);
    }
    SET_FREE(p);
    SET_FREE(FOOTER(p));
    void* next = NEXT(p);
    void* prev = PREV(p);
    if (!IS_END(next) && !IS_USED(next)) { 
        // merge with next
        SET_SIZE(p, SIZE(p) + SIZE(next));
        SET_SIZE(FOOTER(p), SIZE(p));
    }
    if (!IS_START(p - sizeof(size_t)) && !IS_USED(prev)) {
        // merge with prev
        SET_SIZE(prev, SIZE(prev) + SIZE(p));
        SET_SIZE(FOOTER(prev), SIZE(prev));
    }
}

static void unmap_bloc(void** bloc) {
    void* bloc_start = *bloc;
    size_t bloc_size = (*(size_t*)bloc_start) & ~0b11;
    void* next_bloc = *(void**)(bloc_start + bloc_size - sizeof(size_t));

    munmap(bloc_start, bloc_size);

    *bloc = next_bloc; 
}

void customAlloc_unmap_all() {
    while(base_memory_space != NULL) {
        fprintf(stderr, "unmapping %p\n", base_memory_space);
        unmap_bloc(&base_memory_space);
    }
}