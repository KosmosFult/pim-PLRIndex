#include <stdio.h>
#include <stdlib.h>
#include <defs.h>
#include <mutex.h>
#include <alloc.h>
#include <string.h>
#include <mram.h>
#include <stdbool.h>
#include <stdint.h>

#define DATA_SIZE 1024
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
__mram int result[8];


int main()
{
    for (int i = 0; i < 32; i++)
    {
        printf("[%d]: %d, %f, %d\n", i, segments_list[i].key, segments_list[i].slope, segments_list[i].intercept);
    }

    return 0;
}
