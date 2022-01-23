
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
SectorList sectorList = SectorList();

void* smalloc(size_t size){
    if (size == 0 || size > MAX_SIZE){
        return nullptr;
    }
    MallocMetadata* tmp = sectorList.head;
    // Search for available sector.
    while (tmp){
        if (tmp->size >= size && tmp->is_free){
            tmp->is_free = false;
            tmp->real_size = size;
            return (char*)tmp + sizeof(*tmp);
        }
        tmp = tmp->next;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* new_sector = (MallocMetadata *)(sbrk(size + size_of_meta));
    if (*(int*) new_sector == -1){
        return nullptr;
    }
    new_sector->is_free = false;
    new_sector->size = size;
    new_sector->real_size = size;
    sectorList.add(new_sector);
    return ((char*)new_sector + sizeof(*new_sector));
}

void* scalloc(size_t num, size_t size) {
    if (size == 0 || (size * num) > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata *tmp = sectorList.head;
    // Search for available sector.
    size *= num;
    while (tmp) {
        if (tmp->size >= size && tmp->is_free) {
            tmp->is_free = false;
            tmp->real_size = size;
            memset((char *)tmp + sizeof(*tmp),0, size);
            return (char*)tmp + sizeof(tmp);
        }
        tmp = tmp->next;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata *new_sector = (MallocMetadata *) (sbrk(size + size_of_meta));
    if (*(int *) new_sector == -1) {
        return nullptr;
    }
    memset((char*)new_sector + size_of_meta, 0, size);
    new_sector->is_free = false;
    new_sector->size = size;
    new_sector->real_size = size;
    sectorList.add(new_sector);
    return (char*)new_sector + size_of_meta;
}

void sfree(void* p){
    if (!p){
        return;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* to_find = (MallocMetadata*)((char *)p - (char*)size_of_meta);
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        if (tmp == to_find){
            tmp->is_free = true;
            tmp->real_size = 0;
            return;
        }
        tmp = tmp->next;
    }
}

void* srealloc(void* oldp, size_t size){
    if (size == 0 || size > MAX_SIZE){
        return nullptr;
    }
    if (!oldp){
        oldp = smalloc(size);
        return oldp;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* to_find = (MallocMetadata*)((char*)oldp - (char *)size_of_meta);
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        if (tmp == to_find){
            if (tmp->size >= size){
                return oldp;
            } else {
                void* new_location = smalloc(size);
                if (!new_location){
                    return nullptr;
                }
                memcpy(new_location, oldp, tmp->real_size);
                tmp->is_free = true;
                tmp->real_size = 0;
                return new_location;
            }
        }
        tmp = tmp->next;
    }
    return nullptr;
}

size_t _num_free_blocks(){
    size_t count = 0;
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        if (tmp->is_free){
            count++;
        }
        tmp = tmp->next;
    }
    return count;
}
size_t _num_free_bytes(){
    size_t count = 0;
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        if (tmp->is_free){
            count += tmp->size;
        }
        tmp = tmp->next;
    }
    return count;
}

size_t _num_allocated_blocks(){
    size_t count = 0;
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        count++;
        tmp = tmp->next;
    }
    return count;
}

size_t _num_allocated_bytes(){
    size_t count = 0;
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        count += tmp->size;
        tmp = tmp->next;
    }
    return count;
}

size_t _num_meta_data_bytes(){
    size_t count = 0;
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        count += sizeof(*tmp);
        tmp = tmp->next;
    }
    return count;
}
size_t _size_meta_data(){
    size_t res = sizeof(MallocMetadata);
    return res;
}