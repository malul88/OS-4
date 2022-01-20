#include <unistd.h>
#include <cmath>
#include <algorithm>
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

SectorList sectorList = SectorList();

MallocMetadata* _findWilderness(){
    MallocMetadata* tmp = sectorList.head;
    if (!tmp){
        return nullptr;
    }
    while (tmp->next){
        tmp = tmp->next;
    }
    if (tmp->is_free){
        return tmp;
    } else {
        return nullptr;
    }
}
class Hist {
public:
    MallocMetadata *histograma[128] = {nullptr};

    int sizeToIdx(size_t size) {
        return size / KiB;
    }

    int add(MallocMetadata *m, int idx) {
        if (!m || idx < 0 || idx > 128) {
            return -1;
        }
        MallocMetadata *tmp = histograma[idx];
        size_t size = m->size;
        if (!tmp) {
            histograma[idx] = m;
            m->Hist_next = nullptr;
            m->Hist_prev = nullptr;
            return 0;
        }
        while (tmp->Hist_next) {
            if (tmp->Hist_next->size > size) {
                MallocMetadata *next = tmp->Hist_next;
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
        return 0;
    }

    MallocMetadata *searchForFreeBlock(size_t size) {
        int idx = sizeToIdx(size);
        MallocMetadata *tmp;
        for (int i = idx; i < 128; ++i) {
            tmp = histograma[idx];
            while (tmp) {
                if (tmp->size >= size) {
                    return tmp;
                }
                tmp = tmp->next;
            }
        }
        tmp = _findWilderness();
        if (tmp){
            void* res = sbrk(size - tmp->size);
            if (*(int*)res == -1){
                return nullptr;
            } else {
                pullSector(tmp);
                tmp->size = size;
                tmp->real_size = size;
                return tmp;
            }
        }
        return nullptr;
    }

    void *split(MallocMetadata *m, size_t size) {
        // Pulling out the sector from the hist.
        pullSector(m);

        // Splitting the sector.
        MallocMetadata* new_sector = (MallocMetadata*)((char *)m + sizeof(*m) + size);
        new_sector->is_free = true;
        new_sector->size = m->size - size - (2 * (sizeof(*m)));
        new_sector->real_size = 0;
        new_sector->next = m->next;
        new_sector->prev = m;
        m->next = new_sector;
        m->is_free = false;
        m->real_size = size;
        m->size = size;
        m->Hist_next = nullptr;
        m->Hist_prev = nullptr;

        //Adding the new sector to hist.
        add(new_sector, sizeToIdx(new_sector->size));

        return m + sizeof(*m);
    }

    void *pullSector(MallocMetadata *m) {
        int idx = sizeToIdx(m->size);
        if (histograma[idx] == m){
            histograma[idx] = nullptr;
        }
        if (m->Hist_prev){
            m->Hist_prev->Hist_next = m->Hist_next;
        }
        if (m->Hist_next){
            m->Hist_next->Hist_prev = m->Hist_prev;
        }
        m->Hist_next = nullptr;
        m->Hist_prev = nullptr;
        return (char *)m + sizeof(*m);
    }

    MallocMetadata *last() {
        MallocMetadata* tmp = sectorList.head;
        if (!tmp){
            return nullptr;
        }
        while (tmp->next){
            tmp = tmp->next;
        }
        return tmp;
    }
};


void *newMappedRegion(size_t size);

void* _srealloc(MallocMetadata *pMetadata, size_t size);

Hist hist = Hist();

void* smalloc(size_t size){
    if (size == 0 || size > MAX_SIZE){
        return nullptr;
    }
    if (size >= MAX_SECTOR_SIZE){
        return newMappedRegion(size);
    }
    // Search for available sector.
    MallocMetadata* tmp = hist.searchForFreeBlock(size);
    if (tmp){
        if (tmp->size > size + 128 + sizeof(*tmp)){
            return hist.split(tmp, size);
        } else {
            tmp->is_free = false;
            tmp->real_size = size;
            return hist.pullSector(tmp);
        }
    // Create new Sector.
    } else {
        size_t size_of_meta = sizeof(MallocMetadata);
        MallocMetadata* new_sector = (MallocMetadata *)(sbrk(size + size_of_meta));
        if (*(int*) new_sector == -1){
            return nullptr;
        }
        new_sector->is_free = false;
        new_sector->size = size;
        new_sector->real_size = size;
        sectorList.add(new_sector);
        return (char *)new_sector + size_of_meta;
    }
}

void *newMappedRegion(size_t size) {
    size_t size_of_vm = sizeof(vm_area);
    vm_area* new_area = (vm_area*)mmap(nullptr,size + size_of_vm, PROT_WRITE|PROT_READ, MAP_ANONYMOUS| MAP_PRIVATE, -1,0);
    if (new_area == MAP_FAILED){
        return nullptr;
    }
    new_area->size = size;
    new_area->addr = (char *)new_area + size_of_vm;
    sectorList.addRegion(new_area);
    return new_area->addr;
}

void* scalloc(size_t num, size_t size) {
    if (size == 0 || (size * num) > MAX_SIZE) {
        return nullptr;
    }
    size *= num;
    int idx = hist.sizeToIdx(size);
    MallocMetadata *tmp;
    void *res;
    // Search for available sector.
    tmp = hist.searchForFreeBlock(size); // Including wilderness
    if (tmp){
        if (tmp->size > size + 128 + sizeof(*tmp)){
            res = hist.split(tmp, size);
            memset(res, 0, size);
            return res;
        } else {
            tmp->is_free = false;
            tmp->real_size = size;
            res = hist.pullSector(tmp);
            memset(res, 0, size);
            return res;
        }
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata *new_sector = (MallocMetadata *) (sbrk(size + size_of_meta));
    if (*(int *) new_sector == -1) {
        return nullptr;
    }
    memset((char *)new_sector + size_of_meta, 0, size);
    new_sector->is_free = false;
    new_sector->size = size;
    new_sector->real_size = size;
    sectorList.add(new_sector);
    hist.add(new_sector, hist.sizeToIdx(new_sector->size));
    return (char *)new_sector + size_of_meta;
}

void sfree(void* p) {
    if (!p) {
        return;
    }
    bool has_found = false;
    vm_area *vm = sectorList.searchRegion(p);
    if (vm) {  // If it's an area region.
        sectorList.removeRegion(vm);
        int res = munmap(vm, vm->size + sizeof(*vm));
        if (res != 0) {
            std::cout << "free failed: cant unmap" << std::endl;
        }
        return;
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata *to_find = (MallocMetadata *) ((char *) p - size_of_meta);
    MallocMetadata *tmp = sectorList.head;
    while (tmp) {
        if (tmp == to_find) {
            has_found = true;
            break;
        }
        tmp = tmp->next;
    }
    if (!has_found) {
        return;
    }
    if (to_find->next && to_find->next->is_free && to_find->prev && to_find->prev->is_free) { // Both adj's are free
        MallocMetadata *first = to_find->prev;
        MallocMetadata *last = to_find->next;
        first->next = last->next;
        if (last->next) {
            last->next->prev = first;
        }
        first->size += to_find->size + last->size + (2 * size_of_meta);
        first->real_size = 0;
        first->is_free = true;
        hist.pullSector(first);
        hist.pullSector(last);
        hist.pullSector(to_find);
        hist.add(first, hist.sizeToIdx(first->size));
    } else if (to_find->next && to_find->next->is_free) {  // Only right adj is free
        MallocMetadata *last = to_find->next;
        to_find->next = last->next;
        if (last->next) {
            last->next->prev = to_find;
        }
        to_find->size += last->size + size_of_meta;
        to_find->real_size = 0;
        to_find->is_free = true;
        hist.pullSector(last);
        hist.pullSector(to_find);
        hist.add(to_find, hist.sizeToIdx(to_find->size));
    } else if (to_find->prev && to_find->prev->is_free) {  // Only left adj is free
        MallocMetadata *first = to_find->prev;
        first->next = to_find->next;
        if (to_find->next) {
            to_find->next->prev = first;
        }
        first->size += to_find->size + size_of_meta;
        first->real_size = 0;
        first->is_free = true;
        hist.pullSector(first);
        hist.pullSector(to_find);
        hist.add(first, hist.sizeToIdx(first->size));
    } else {                                            // No adj is free
        to_find->real_size = 0;
        to_find->is_free = true;
        hist.add(to_find, hist.sizeToIdx(to_find->size));
    }
}

void* srealloc(void* oldp, size_t size){
    if (size == 0 || size > MAX_SIZE){
        return nullptr;
    }
    if (!oldp){
        return smalloc(size);
    }
    size_t size_of_meta = sizeof(MallocMetadata);
    MallocMetadata* to_find = (MallocMetadata*)((char *)oldp - size_of_meta);
    MallocMetadata* tmp = sectorList.head;
    while (tmp){
        if (tmp == to_find){
            return _srealloc(tmp, size);
        }
        tmp = tmp->next;
    }
    std::cout << " srealloc error: Not found" << std::endl;
    return nullptr;
}

void* _srealloc(MallocMetadata *m, size_t size) {
    if (!m) {
        return nullptr;
    }
    MallocMetadata *before = m->prev;
    MallocMetadata *after = m->next;
    if (m->size >= size) {
        return (char *) m + sizeof(*m);
    } else if (before && before->is_free && (m->size + before->size) >= size) {
        before->next = m->next;
        if (before->next) {
            before->next->prev = before;
        }
        before->is_free = false;
        hist.pullSector(before);
        before->size += m->size + sizeof(*m);
        before->real_size = size + sizeof(*m);
        memcpy((char *) before + sizeof(*before), (char *) m + sizeof(*m), m->real_size);
        if (before->size > before->real_size + 128 + sizeof(*before)) {
            hist.split(before, before->real_size);
        }
        return (char *) before + sizeof(*before);
    } else if (after && after->is_free && (m->size + after->size) >= size) {
        hist.pullSector(after);
        m->next = after->next;
        if (m->next) {
            m->next->prev = m;
        }
        m->size += after->size + sizeof(*m);
        m->real_size = size + sizeof(*m);
        if (m->size > m->real_size + 128 + sizeof(*m)) {
            hist.split(m, m->real_size);
        }
        return (char *) m + sizeof(*m);
    } else if (after && after->is_free && before && before->is_free && (m->size + before->size + after->size) >= size) {
        hist.pullSector(before);
        hist.pullSector(after);
        before->next = after->next;
        if (before->next) {
            before->next->prev = before;
        }
        before->is_free = false;
        memcpy((char *) before + sizeof(*before), (char *) m + sizeof(*m), m->real_size);
        before->size += m->size + after->size + 2 * sizeof(*m);
        before->real_size = size + (2 * sizeof(*m));
        if (before->size > before->real_size + 128 + sizeof(*before)) {
            hist.split(before, before->real_size);
        }
        return (char *) before + sizeof(*before);
    } else if (MallocMetadata *res = hist.searchForFreeBlock(size)) { // Including wilderness
        memcpy((char *) res + sizeof(*res), (char *) m + sizeof(*m), m->real_size);
        res->is_free = false;
        res->real_size = size;
        sfree((char *) m + sizeof(*m));
        hist.pullSector(res);
        if (res->size > res->real_size + 128 + sizeof(*res)) {
            hist.split(res, res->real_size);
        }
        return (char *) res + sizeof(*res);
    } else if (MallocMetadata *ress = hist.last()) {
        sbrk(size - ress->size);
        ress->size = size;
        ress->real_size = size;
        return (char *) ress + sizeof(*ress);
    } else {
        void *new_location = smalloc(size);
        if (!new_location) {
            return nullptr;
        }
        memcpy(new_location, (char *) m + sizeof(*m), m->real_size);
        m->is_free = true;
        m->real_size = 0;
        sfree((char *) m + sizeof(*m));
        return new_location;
    }
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
    vm_area* temp = sectorList.mm_head;
    while (temp){
        count++;
        temp = temp->next;
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
    vm_area* temp = sectorList.mm_head;
    while (temp){
        count += temp->size;
        temp = temp->next;
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
    vm_area* temp = sectorList.mm_head;
    while (temp){
        count += sizeof(*temp);
        temp = temp->next;
    }
    return count;
}
size_t _size_meta_data(){
    size_t res = sizeof(MallocMetadata);
    return res;
}


