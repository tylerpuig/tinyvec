int compare_ints(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

int binary_search_int(int *array, int size, int value)
{
    int left = 0;
    int right = size - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        if (array[mid] == value)
            return mid;

        if (array[mid] < value)
            left = mid + 1;
        else
            right = mid - 1;
    }

    return -1;
}