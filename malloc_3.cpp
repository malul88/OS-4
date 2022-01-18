#include <unistd.h>
#include <cmath>
#include <algorithm>
#include <cstring>


const int MAX_SIZE = pow(10, 8);

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

class Hist{
    MallocMetadata* histograma[128];
    Hist(): histograma({nullptr});
    int sizeToIdx(size_t size){
         return size/1024;
    }
    int add(MallocMetadata* m, idx){
        MallocMetadata* tmp = histograma[idx];
        size_t size = m->size;
        if (!tmp){
            histograma[idx] = m;
            return 0;
        }
        while (tmp->Hist_next){
            if (tmp->Hist_next->size > size){
                MallocMetadata* next = tmp->Hist_next;
                tmp->Hist_next = m;
                m->Hist_prev = tmp;
                m->Hist_next = next;
                next->Hist_prev = m;
                return 0;
            }
            tmp = tmp->Hist_next;
        }
        tmp->Hist_next = m;
        m->Hist_prev = tmp;
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
            return tmp + sizeof(*tmp);
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
    return new_sector + size_of_meta;
}

void* scalloc(size_t num, size_t size) {
    if (size == 0 || (size * num) > MAX_SIZE) {
        return nullptr;
    }
    MallocMetadata *tmp = sectorList.head;
    // Search for available sector.
    while (tmp) {
        if (tmp->size >= size && tmp->is_free) {
            tmp->is_free = false;
            tmp->real_size = size;
            std::fill(tmp + sizeof(*tmp), tmp + sizeof(*tmp) + size + 1, 0);
            return tmp + sizeof(tmp);
        }
        tmp = tmp->next;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata *new_sector = (MallocMetadata *) (sbrk(size + size_of_meta));
    if (*(int *) new_sector == -1) {
        return nullptr;
    }
    std::fill(new_sector + size_of_meta, new_sector + size_of_meta + size + 1, 0);
    new_sector->is_free = false;
    new_sector->size = size;
    new_sector->real_size = size;
    sectorList.add(new_sector);
    return new_sector + size_of_meta;
}

void sfree(void* p){
    if (!p){
        return;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* to_find = (MallocMetadata*)p - size_of_meta;
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
    if (size == 0 || size > MAX_SIZE || !oldp){
        return nullptr;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* to_find = (MallocMetadata*)oldp - size_of_meta;
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
    MallocMetadata* tmp = sectorList.head;
    return sizeof(*tmp);
}
