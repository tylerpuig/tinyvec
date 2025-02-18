#include "greatest.h"
// #include "vec_types.h"
#include "../src/core/include/minheap.h"

// Test fixtures
typedef struct
{
    float similarity;
    int index;
} TestVec;

// Test data
static TestVec test_vectors[] = {
    {0.9f, 0}, // High similarity
    {0.8f, 1},
    {0.95f, 2}, // Highest similarity
    {0.3f, 3},  // Low similarity
    {0.6f, 4},
    {0.75f, 5},
    {0.1f, 6}, // Lowest similarity
    {0.85f, 7},
    {0.4f, 8},
    {0.5f, 9}};

static const int NUM_TEST_VECTORS = sizeof(test_vectors) / sizeof(test_vectors[0]);

// Test creating and destroying heap
TEST test_heap_creation(void)
{
    MinHeap *heap = createMinHeap(5);
    ASSERT(heap != NULL);
    ASSERT(heap->capacity == 5);
    ASSERT(heap->size == 0);
    ASSERT(heap->data != NULL);
    ASSERT(heap->indices != NULL);

    freeHeap(heap);
    PASS();
}

// Test inserting elements
TEST test_heap_insertion(void)
{
    MinHeap *heap = createMinHeap(3);

    // Insert three elements
    insert(heap, test_vectors[0].similarity, test_vectors[0].index);
    insert(heap, test_vectors[1].similarity, test_vectors[1].index);
    insert(heap, test_vectors[2].similarity, test_vectors[2].index);

    ASSERT(heap->size == 3);
    // Minimum should be at root
    ASSERT(heap->data[0] <= heap->data[1]);
    ASSERT(heap->data[0] <= heap->data[2]);

    freeHeap(heap);
    PASS();
}

// Test heap property
TEST test_heap_property(void)
{
    MinHeap *heap = createMinHeap(5);

    // Insert multiple elements
    for (int i = 0; i < 5; i++)
    {
        insert(heap, test_vectors[i].similarity, test_vectors[i].index);
    }

    // Verify min-heap property
    for (int i = 0; i < heap->size; i++)
    {
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < heap->size)
        {
            ASSERT(heap->data[i] <= heap->data[left]);
        }
        if (right < heap->size)
        {
            ASSERT(heap->data[i] <= heap->data[right]);
        }
    }

    freeHeap(heap);
    PASS();
}

// Test top-k results
TEST test_top_k_results(void)
{
    MinHeap *heap = createMinHeap(5);
    const int k = 3;

    // Insert elements
    for (int i = 0; i < NUM_TEST_VECTORS; i++)
    {
        insert(heap, test_vectors[i].similarity, test_vectors[i].index);
    }

    VecResult *results = createVecResult(heap, k);
    ASSERT(results != NULL);

    // Verify results are sorted in descending order
    for (int i = 0; i < k - 1; i++)
    {
        ASSERT(results[i].similarity >= results[i + 1].similarity);
    }

    // Verify we got the highest similarities
    ASSERT_IN_RANGE(results[0].similarity, 0.9f, 0.95f); // Should be one of our highest values

    free(results);
    freeHeap(heap);
    PASS();
}

// Test heap capacity handling
TEST test_heap_capacity(void)
{
    MinHeap *heap = createMinHeap(3);

    // Insert more elements than capacity
    for (int i = 0; i < NUM_TEST_VECTORS; i++)
    {
        insert(heap, test_vectors[i].similarity, test_vectors[i].index);
    }

    // Size should not exceed capacity
    ASSERT(heap->size == 3);

    // Verify we kept the highest similarities
    VecResult *results = createVecResult(heap, 3);
    ASSERT(results != NULL);

    // Top results should be our highest similarities
    ASSERT_IN_RANGE(results[0].similarity, 0.9f, 0.95f);

    free(results);
    freeHeap(heap);
    PASS();
}

SUITE(minheap_suite)
{
    RUN_TEST(test_heap_creation);
    RUN_TEST(test_heap_insertion);
    RUN_TEST(test_heap_property);
    RUN_TEST(test_top_k_results);
    RUN_TEST(test_heap_capacity);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(minheap_suite);
    GREATEST_MAIN_END();
}
