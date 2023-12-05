#include <stdio.h>
#include <stdlib.h>
#include <defs.h>
#include <mutex.h>
#include <alloc.h>
#include <string.h>
#include <mram.h>
#include <stdbool.h>
#include <stdint.h>
#include "defs.h"

#define DATA_SIZE 2 << 20
#define ERROR 16
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (b) : (a))

typedef struct segment
{
    int key;
    float slope;
    int intercept;
} segment;

__mram_noinit segment segments_list[32];
__mram_noinit int data[DATA_SIZE];
__mram_noinit int query_keys[QUERY_BATCH];
__mram_noinit int result[QUERY_BATCH];

int main()
{
    int n_segs = segments_list[0].key;
    int n_data = data[0];
    int n_query = query_keys[0];
    printf("sgements:\n");
    for (int i = 1; i <= n_segs; i++)
    {
        printf("[%d]: %d, %f, %d\n", i, segments_list[i].key, segments_list[i].slope, segments_list[i].intercept);
    }
    printf("data size:%d\n", n_data);
    for (int i = 1; i < 32; i++)
    {
        printf("[%d]: %d\n", i, data[i]);
    }

    printf("querys:%d\n", n_query);
    for (int i = 1; i < n_query; i++)
        printf("[%d]: %d\n", i, query_keys[i]);

    return 0;
}
