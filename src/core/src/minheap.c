#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Optional for debugging
#include "vec_types.h"

// MinHeap structure
typedef struct
{
    float *data;  // Array to store similarity scores
    int *indices; // Array to store corresponding vector indices
    int capacity; // Maximum size of heap
    int size;     // Current number of elements
} MinHeap;

// Create and initialize heap
MinHeap *createMinHeap(int capacity)
{
    MinHeap *heap = (MinHeap *)malloc(sizeof(MinHeap));
    heap->data = (float *)malloc(capacity * sizeof(float)); // Similarity scores
    heap->indices = (int *)malloc(capacity * sizeof(int));  // Indices of vectors
    heap->capacity = capacity;
    heap->size = 0;
    return heap;
}

// Helper function to swap float values
void swap(float *a, float *b)
{
    float temp = *a;
    *a = *b;
    *b = temp;
}

// Helper function to swap int values
void swapInt(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

int compVecResult(const void *a, const void *b)
{
    const VecResult *va = (const VecResult *)a;
    const VecResult *vb = (const VecResult *)b;

    // Sort in descending order of similarity
    if (va->similarity < vb->similarity)
        return 1;
    if (va->similarity > vb->similarity)
        return -1;
    return 0;
}

VecResult *createVecResult(MinHeap *mh, int top_k)
{
    if (mh->size < top_k)
    {
        // Handle cases where heap has fewer elements
        top_k = mh->size;
    }

    // Allocate memory for the result
    VecResult *vr = malloc(sizeof(VecResult) * top_k);
    if (vr == NULL)
    {
        // printf("Memory allocation for VecResult failed\n");
        return NULL;
    }

    // Copy the top-k elements
    for (int i = 0; i < top_k; i++)
    {
        vr[i] = (VecResult){mh->data[i], mh->indices[i]};
        // printf("Copied: similarity=%f, index=%d\n", mh->data[i], mh->indices[i]);
    }

    // Sort the results
    qsort(vr, top_k, sizeof(VecResult), compVecResult);

    return vr;
}

// Heapify function to maintain the heap property
void heapify(MinHeap *heap, int idx)
{
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < heap->size && heap->data[left] < heap->data[smallest])
    {
        smallest = left;
    }
    if (right < heap->size && heap->data[right] < heap->data[smallest])
    {
        smallest = right;
    }

    if (smallest != idx)
    {
        // Swap elements and recurse
        swap(&heap->data[idx], &heap->data[smallest]);
        swapInt(&heap->indices[idx], &heap->indices[smallest]);
        heapify(heap, smallest);
    }
}

// Insert a similarity and its index into the heap
void insert(MinHeap *heap, float similarity, int index)
{
    if (heap->size < heap->capacity)
    {
        // Add new element when heap isn't full
        heap->data[heap->size] = similarity;
        heap->indices[heap->size] = index;
        heap->size++;

        // Fix the min-heap property
        int current = heap->size - 1;
        while (current > 0)
        {
            int parent = (current - 1) / 2;
            if (heap->data[parent] > heap->data[current])
            {
                swap(&heap->data[parent], &heap->data[current]);
                swapInt(&heap->indices[parent], &heap->indices[current]);
                current = parent;
            }
            else
            {
                break;
            }
        }
    }
    else if (similarity > heap->data[0])
    {
        // Replace root if new similarity is larger
        heap->data[0] = similarity;
        heap->indices[0] = index;
        heapify(heap, 0);
    }
}

// Free heap memory
void freeHeap(MinHeap *heap)
{
    free(heap->data);
    free(heap->indices);
    free(heap);
}

// Debugging function (optional)
void printHeap(MinHeap *heap)
{
    printf("Heap contents:\n");
    for (int i = 0; i < heap->size; i++)
    {
        printf("Similarity: %f, Index: %d\n", heap->data[i], heap->indices[i]);
    }
}
