#ifndef _SMAPS_H
#define _SMAPS_H
#include <stdint.h>
#include <inttypes.h>

typedef enum {
    size = 0,
    kps,
    mmu,
    rss,
    pss,
    pssd,
    shrc,
    shrd,
    pric,
    prid,
    ref,
    anon,
    lzf,
    anonp,
    shmap,
    fimap,
    shhu,
    prhu,
    swap,
    swpss,
    lockd,
    thp,
    pkey,
    FIELD_LEN,
} field_type;

typedef struct segtype {
    char range[40];
    char perm[5];
    char offset[9];
    char device[6];
    char inode[9];
    char path[256];
    int  fields[FIELD_LEN];
    struct segtype *next;
} type_seg;

// #define SIZE_OF_SMAPS_RING_BUFFER 500000000
#define SIZE_OF_SMAPS_RING_BUFFER 50000000
extern unsigned long smaps_pages_ring_buffer[SIZE_OF_SMAPS_RING_BUFFER];
extern uint32_t smaps_read_index;
extern uint32_t smaps_ring_buffer_write_index;

type_seg* smaps(int pid);
void smaps_free(type_seg **smaps_head);

#endif //_SMAPS_H
