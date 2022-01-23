
#ifndef OS_4_MALLOC_2_H
#define OS_4_MALLOC_2_H
#include <unistd.h>
#include <cmath>
#include <cstring>






void* smalloc(size_t size);

void* scalloc(size_t num, size_t size);

void sfree(void* p);

void* srealloc(void* oldp, size_t size);


#endif //OS_4_MALLOC_2_H
