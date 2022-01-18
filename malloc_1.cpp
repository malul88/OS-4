#include <iostream>
#include <unistd.h>
#include <cmath>
const int MAX_SIZE = pow(10, 8);

void *smalloc(size_t size){
    if (size > MAX_SIZE || size == 0){
        return nullptr;
    }
    void* res = sbrk(size);
    if (*(int*)res == -1){
        return nullptr;
    }
    return res;
}