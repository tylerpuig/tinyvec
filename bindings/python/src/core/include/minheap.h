#ifndef MINHEAP_H
#define MINHEAP_H
#include "vec_types.h"
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float *data;  // Array to store similarity scores
        int *indices; // Array to store corresponding vector indices
        int capacity; // Maximum size of heap
        int size;     // Current number of elements
    } MinHeap;

    MinHeap *createMinHeap(int capacity);
    void swap(float *a, float *b);
    void heapify(MinHeap *heap, int idx);
    void insert(MinHeap *heap, float similarity, int index);
    void freeHeap(MinHeap *heap);
    VecResult *createVecResult(MinHeap *mh, int top_k);

#ifdef __cplusplus
}
#endif

#endif