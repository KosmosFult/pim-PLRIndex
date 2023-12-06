#include <stdio.h>
#include <stdlib.h>
#include <defs.h>
#include <alloc.h>
#include <string.h>
#include <mram.h>
#include <stdbool.h>
#include <stdint.h>
#include "defs.h"
#include "seqread.h"

#define DATA_SIZE 2 << 20
#define ERROR 16
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (b) : (a))
#define MAX_SEGS 16

// #define FIRST_SEG ((segment*)((void*)segments_list + sizeof(segment))) 为什么不行
#define UPPER_8_ALIGNED(n) (((n) + 7) & ~7)

#define PGM_SUB_EPS(x,epsilon) ((x) <= (epsilon) ? 0 : ((x) - (epsilon)))
#define PGM_ADD_EPS(x,epsilon,size) ((x) + (epsilon) + 2 >= (size) ? (size) : (x) + (epsilon) + 2)

typedef struct segment
{
    int key;
    float slope;
    int intercept;
} segment;

typedef struct ApproxPos
{
    uint32_t pos;
    uint32_t lo;
    uint32_t hi;
} ApproxPos;

__mram_noinit segment segments_list[32];
__mram_noinit int data[DATA_SIZE];
__mram_noinit int query_keys[QUERY_BATCH];
__mram_noinit int result[QUERY_BATCH];

__host uint32_t segs_len;
__host uint32_t data_len;
__host uint32_t lkey_i;


void print_info()
{
    int n_query = query_keys[0];
    printf("sgements:\n");
    for (int i = 1; i <= segs_len; i++)
    {
        printf("[%d]: %d, %f, %d\n", i, segments_list[i].key, segments_list[i].slope, segments_list[i].intercept);
    }
    printf("data size:%d\n", data_len);
    for (int i = 1; i < 32; i++)
    {
        printf("[%d]: %d\n", i, data[i]);
    }

    printf("querys:%d\n", n_query);
    for (int i = 1; i <= n_query; i++)
        printf("[%d]: %d\n", i, query_keys[i]);
}

int init_data()
{
    segs_len = segments_list[0].key;
    lkey_i = segments_list[0].intercept;
    data_len = data[0];
    printf("segs len: %d\ndata len:%d\nlkey_i:%d\n", segs_len, data_len, lkey_i);
    if(segs_len > MAX_SEGS)
    {
        printf("segs len:%d > max segs len\n", segs_len);
        return -1;
    }
    return 0;
}


ApproxPos get_appos(int k)
{
    // Bcause the segments are in mram, it's not sure what implementation
    // is the most efficient
    // 好像是有local cache，不知道直接访问mram开销如何

    __dma_aligned segment seg_buffer[MAX_SEGS];
    
    mram_read(segments_list, seg_buffer, UPPER_8_ALIGNED(sizeof(segment)*(segs_len+1)));
    
    // 后面再换二分
    
    // 因为i=0就会报错，因此换个逻辑
    // for(i = 1; i <= segs_len && seg_buffer[i].key<= k; i++);
    int i;
    for(i = 1; i <= segs_len && seg_buffer[i].key<= k; i++);

    i--;
    // int a = 0;
    if (i <= 0)
    {
        ApproxPos appos = {UINT32_MAX, 0, 0};
        return appos;
    }

    int cal_pos = seg_buffer[i].slope * k + seg_buffer[i].intercept;
    cal_pos -= lkey_i;
    int lo = PGM_SUB_EPS(cal_pos, EPSILON);
    int hi = PGM_ADD_EPS(cal_pos, EPSILON, data_len);
    ApproxPos appos = {cal_pos, lo, hi};
    // ApproxPos appos = {0,0,0};

    return appos;
}




/**
 * @brief Search for the max data key kd, kd <= k
 * @param k key for search
 * @return The max data key
 */
int search(int k)
{
    ApproxPos appos = get_appos(k);

    if(appos.pos == UINT32_MAX)
        return INT32_MIN;

    // 这里用seq read api

    /* Cache for the sequential reader */
    seqreader_buffer_t local_cache = seqread_alloc();
    /* The reader */
    seqreader_t sr;
    /* The pointer where we will access the cached data */
    uint8_t *current_char = seqread_init(local_cache, data+appos.lo, &sr);

    int tdata, pdata = INT32_MIN;
    for (int data_read = 0; data_read <= appos.hi - appos.lo; data_read++)
    {
        tdata = *((int*)current_char);
        
        if(tdata == k)
            return k;
        else if (tdata > k)
            return pdata;
        
        current_char = seqread_get(current_char, sizeof(int), &sr);
        pdata = tdata;
    }

    return pdata;
}



int query_handler()
{
    __dma_aligned int query_buffer[QUERY_BATCH];
    __dma_aligned int result_buffer[QUERY_BATCH];
    
    mram_read(query_keys, query_buffer, QUERY_BATCH * sizeof(int));
    for(int i = 1; i <= query_buffer[0]; i++)
    {
        result_buffer[i] = search(query_buffer[i]);
        printf("q[%d]: (%d,%d)\n", i, query_keys[i], result_buffer[i]);
    }

    mram_write(result_buffer, result, QUERY_BATCH * sizeof(int));



    return 0;
}

int main()
{
    init_data();
    // print_info();
    query_handler();
    return 0;
}
