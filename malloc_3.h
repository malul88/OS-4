#ifndef OS_4_MALLOC_3_H
#define OS_4_MALLOC_3_H
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
const int MAX_SIZE = pow(10, 8);
const int KiB = 1024;
const int MAX_SECTOR_SIZE = 128*KiB;


class MallocMetadata{
        public:
        size_t size;
        size_t real_size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
        MallocMetadata* Hist_next;
        MallocMetadata* Hist_prev;
        MallocMetadata(): size(0), real_size(0), is_free(false), next(nullptr), prev(nullptr), Hist_next(nullptr), Hist_prev(
        nullptr){}
};

class vm_area{
public:
    size_t size;
    void* addr;
    vm_area* next;

    vm_area(size_t size, void *addr):size(size), addr(addr), next(nullptr){}
};

class SectorList {
public:
    MallocMetadata *head;
    vm_area* mm_head;
    SectorList() : head(nullptr), mm_head(nullptr) {}

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
    int addRegion(vm_area* addr){
        if (!addr) {
            return -1;
        }
        vm_area *tmp = mm_head;
        if (mm_head == nullptr) {
            mm_head = addr;
            return 0;
        }
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = addr;
        return 0;
    }

    vm_area *searchRegion(void *p) {
        vm_area* tmp = mm_head;
        while (tmp){
            if (tmp->addr == p){
                return tmp;
            }
            tmp = tmp->next;
        }
        return nullptr;
    }

    void removeRegion(vm_area *p) {
        vm_area* tmp = mm_head;
        vm_area* tmp_prev = nullptr;
        if (p == mm_head){
            mm_head = mm_head->next;
            return;
        }
        while (tmp){
            if (tmp->addr == p){
                if (tmp_prev){
                    tmp_prev->next = tmp->next;
                }
            }
            tmp_prev = tmp;
            tmp = tmp->next;
        }
        return;
    }
};

void* smalloc(size_t size);

void* scalloc(size_t num, size_t size);

void sfree(void* p);

void* srealloc(void* oldp, size_t size);


#endif //OS_4_MALLOC_3_H

