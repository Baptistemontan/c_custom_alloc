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
#define PADDING_SIZE (3 * sizeof(size_t)) // header/footer + end/start marker + next bloc addr

#define CLEAR(p) (SET_VALUE(p, 0))

#define BASE_ALLOC_SIZE (1 << 16) // 2^16 might be a big minimum for each bloc but meh whatever


static void* base_memory_space = NULL;

static size_t GET_VALUE(size_t* p) {
    return *p;
}

static void SET_VALUE(size_t* p, size_t value) {
    *p = value;
}

static void SET_SIZE(size_t* p, size_t size) {
    SET_VALUE(p, (size | IS_USED(p)));
}

static size_t normalize_size(size_t size) {
    // make size a multiple of 8
    if(size & 0b111) {
        size &= ~0b111; 
        size += 8;
    }

    size += 2 * sizeof(size_t); // for header and footer

    return size;
}


static void init_bloc(void* mem, size_t size) {
    size_t bloc_size = size - PADDING_SIZE;
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
        min_size += PADDING_SIZE;
    } else {
        min_size = BASE_ALLOC_SIZE;
    }

    void* m = mmap(0, min_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(m == MAP_FAILED) {
        return NULL;
    } else {
        init_bloc(m, min_size);
        return m;
    }
}


static void* realloc_memory_space(void* prev_bloc_end_marker, size_t min_size) {

    void* m = alloc_memory_space(min_size);

    if(m == NULL) {
        return NULL;
    }

    SET_VALUE(prev_bloc_end_marker + sizeof(size_t), (size_t)m);

    return m;
}

static void alloc_bloc(void* header, size_t size) {
    size_t prevSize = SIZE(header);
    size_t newSize = prevSize - size;
    // we need the space for header and footer, bloc can't be 0 sized
    // so if the remaining space is smaller than that just alloc the whole bloc
    if(newSize >= 2 * sizeof(size_t)) { 
        SET_SIZE(FOOTER(header), newSize); // set old footer to new free size
        CLEAR(HEADER(FOOTER(header))); // clear new header
        SET_SIZE(HEADER(FOOTER(header)), newSize); // set new header to new free size
        SET_SIZE(header, size); // set old header to new used size
        CLEAR(FOOTER(header)); // clear new footer
        SET_SIZE(FOOTER(header), size); // set new footer to new used size
    }
    SET_USED(header);
    SET_USED(FOOTER(header));
}

void* search_empty_bloc(size_t size) {
    // I'm lazy so first fit strategy
    void* p = base_memory_space + sizeof(size_t);

    search_loop:

    while (!IS_END(p) && (IS_USED(p) || SIZE(p) < size)) {
        p = NEXT(p);
    }

    if (IS_END(p)) {
        void* next_block = NEXT_BLOC(p);
        if(next_block == NULL) {
            // allocate more space
            next_block = realloc_memory_space(p, size);
            if(next_block == NULL) {
                // reallocation failed
                return NULL;
            }
        }
        p = next_block + sizeof(size_t);
        

        goto search_loop; // yeah goto bad blah blah blah IDGAF
    }
    return p;
}


void* customAlloc_malloc(size_t size) {
    if(base_memory_space == NULL) {
        void* m = alloc_memory_space(normalize_size(size));
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

    //first alloc good size
    size = normalize_size(size);
    
    void* p = search_empty_bloc(size);
    
    alloc_bloc(p, size);
    return p + sizeof(size_t); // return pointer to data
}

inline static void merge(void* low, void* high) {
    if (!IS_END(high) && !IS_USED(high) && !IS_USED(low)) {
        size_t total_size = SIZE(low) + SIZE(high);
        SET_SIZE(low, total_size);
        SET_SIZE(FOOTER(low), total_size);
    }
}

static void merge_with_next(void* header) {
    void* next = NEXT(header);
    merge(header, next);
}

static void merge_with_prev(void* header) {
    if(!IS_START(header - sizeof(size_t))) {
        void* prev = PREV(header);
        merge(prev, header);
    }   
}

static void dealloc_bloc(void* header) {
    SET_FREE(header);
    SET_FREE(FOOTER(header));
    // ! Merge with next first so header stay valid
    merge_with_next(header);
    merge_with_prev(header);
}

void customAlloc_free(void* p) {
    if (p == NULL) {
        return;
    }
    void* header = p - sizeof(size_t);
    if (!IS_USED(header)) {
        fprintf(stderr, "Attempt to free unallocated memory\n");
        exit(1);
    }
    dealloc_bloc(header);
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
        unmap_bloc(&base_memory_space);
    }
}

static void realloc_shrink(void* header, size_t new_size) {
    size_t bloc_size = SIZE(header);
    void* next_header = NEXT(header);
    size_t diff = bloc_size - new_size;
    // need to do some checks to see if we have enough place for the header/footer
    if((diff < 2 * sizeof(size_t)) && (IS_END(next_header) || IS_USED(next_header))) {
        // no place :(
        // don't do anything
    } else if(!IS_END(next_header) && !IS_USED(next_header)) {
        // if next is not used, we need to merge it
        size_t next_new_size = SIZE(next_header) + diff;
        void* footer = FOOTER(next_header);

        SET_SIZE(footer, next_new_size);
        next_header = HEADER(footer);
        CLEAR(next_header);
        SET_SIZE(next_header, next_new_size);
        
        // now just set new bloc size
        SET_SIZE(header, new_size);
        CLEAR(FOOTER(header));
        SET_SIZE(FOOTER(header), new_size);        
    } else {
        // so we go place for header and footer
        void* footer = FOOTER(header);
        // create new unallocated bloc
        SET_SIZE(footer, diff);
        CLEAR(HEADER(footer));
        SET_SIZE(HEADER(footer), diff);
        SET_FREE(footer);
        // shrink bloc
        SET_SIZE(header, new_size);
        CLEAR(FOOTER(header));
        SET_SIZE(FOOTER(header), new_size);
        SET_USED(FOOTER(header));
        // alright we good, normally (?)
    }
}

static void* realloc_grow(void* header, size_t new_size) {
    size_t bloc_size = SIZE(header);
    void* next_header = NEXT(header);
    size_t diff = new_size - bloc_size;
    if(!IS_END(next_header) && !IS_USED(next_header) && (SIZE(next_header) >= diff)) {
        // PERFECT ! best case possible, we can just grow and no copy !
        // now check if we cut the bloc, or take it entirely

        // we take it entirely if there is no place for header/footer in left space
        size_t next_bloc_size = SIZE(next_header);
        // left just space for header/footer make it kinda useless but meh whatever
        if(next_bloc_size < 2 * sizeof(size_t)) { 
            // just take whole bloc
            void* footer = FOOTER(next_header);
            SET_SIZE(footer, new_size);
            SET_USED(footer);
            SET_SIZE(header, new_size);
            // so eazy :)
            
        } else {
            // we got space for header/footer
            void* next_footer = FOOTER(next_header);
            SET_SIZE(next_footer, diff);
            void* new_next_header = HEADER(next_footer);
            CLEAR(new_next_header);
            SET_SIZE(new_next_header, diff);
            // set size for bloc
            SET_SIZE(header, new_size);
            void* footer = FOOTER(header);
            CLEAR(footer);
            SET_SIZE(footer, new_size);
            SET_USED(footer);
        }
        return header + sizeof(size_t);
    } else {
        // sadly, we need to reallocate...

        // search for a bloc that can hold that much
        void* m = search_empty_bloc(new_size);
        if(m == NULL) {
            // allocation failed
            return NULL;
        }
        alloc_bloc(m, new_size);
        // copy everyhting into new bloc
        memcpy(m + sizeof(size_t), header + sizeof(size_t), bloc_size - (2 * sizeof(size_t)));
        // dealloc old bloc
        dealloc_bloc(header);

        return m + sizeof(size_t);
    }
}

void* customAlloc_realloc(void* p, size_t new_size) {
    if(p == NULL) {
        // if p is null, could return NULL or throw error, but lets just alloc
        return customAlloc_malloc(new_size);
    } else if(new_size == 0) {
        // deallocate
        customAlloc_free(p);
        return NULL;
    }

    // grab bloc header
    void* header = p - sizeof(size_t);


    if (!IS_USED(header)) {
        fprintf(stderr, "Attempted to realloc unused memory\n");
        exit(1);
    }

    // get bloc size
    size_t bloc_size = SIZE(header);

    new_size = normalize_size(new_size);

    
    if(new_size < bloc_size) {
        // shrink it
        realloc_shrink(header, new_size);
        // shrinking always give back same ptr
        return p;
    } else if(new_size > bloc_size) {
        // grow it
        return realloc_grow(header, new_size);
    } else {
        // same size, don't do anything
        return p;
    }
}   

void print_bloc(void* header) {
    size_t size = SIZE(header);
    printf("\tBloc start at %p with size %lu, used: %lu\n", header, size, IS_USED(header));
}

void* print_page(void* page_start) {
    size_t page_size = SIZE(page_start);
    printf("--- START OF PAGE ---\n");
    printf("Page starting at %p with size %lu\n", page_start, page_size);
    void* b = page_start + sizeof(size_t);
    while(!IS_END(b)) {
        print_bloc(b);
        b = NEXT(b);
    }
    printf("---  END OF PAGE  ---\n");
    return NEXT_BLOC(b);
}




void customAlloc_print_memspace() {
    void* m = base_memory_space;
    printf("------ START OF MEMORY SPACE ------\n");
    while(m != NULL) {
         m = print_page(m);
    }
    printf("------  END OF MEMORY SPACE  ------\n");
}