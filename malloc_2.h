
#ifndef OS_4_MALLOC_2_H
#define OS_4_MALLOC_2_H
#include <unistd.h>
#include <cmath>
#include <cstring>


const int MAX_SIZE = pow(10, 8);

class MallocMetadata{
public:
    size_t size;
    size_t real_size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata(): size(0), real_size(0), is_free(false), next(nullptr), prev(nullptr){}
};

class SectorList {
public:
    MallocMetadata *head;

    SectorList() : head(nullptr) {}

    int add(MallocMetadata *m) {
        if (!m) {
            return -1;
        }
        MallocMetadata *tmp = head;
        if (head == nullptr) {
            head = m;
            return 0;
        }
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = m;
        m->prev = tmp;
        return 0;
    }
};



void* smalloc(size_t size);

void* scalloc(size_t num, size_t size);

void sfree(void* p);

void* srealloc(void* oldp, size_t size);


#endif //OS_4_MALLOC_2_H
