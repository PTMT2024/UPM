#ifndef PEBS_H
#define PEBS_H

#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include "../common.h"
#include "parameters.h"

// TODO, is it an appropriate value?
#define PERF_PAGES	(1 + (1 << 10))	// Has to be == 1+2^n
//#define SAMPLE_PERIOD 200

#define PEBS_NPROCS 24

struct perf_sample {
    struct perf_event_header header;
    uint64_t ip;
    uint32_t pid, tid;    /* if PERF_SAMPLE_TID */
    uint64_t addr;        /* if PERF_SAMPLE_ADDR */
    uint64_t weight;      /* if PERF_SAMPLE_WEIGHT */
    uint64_t phys_addr;   /* if PERF_SAMPLE_PHYS_ADDR */
    //uint64_t time;        /* if PERF_SAMPLE_TIME set */
   // uint64_t time_enabled;
    //uint64_t time_running;
    //__u64 id;          /* if PERF_SAMPLE_IDENTIFIER set */
    /* __u64 data_src;    /\* if PERF_SAMPLE_DATA_SRC *\/ */
};

enum pbuftype {
    L_DRAMREAD = 0,
    L_PMMREAD = 1,
    //READ = 2,
    //R_NVMREAD = 3,  
    // WRITE = 2,
    NPBUFTYPES
};

typedef struct page_info {
    uint16_t event_type_id;
    unsigned long virtual_page;
} PAGE_INFO;

#define SIZE_OF_PEBS_RING_BUFFER 20000000

extern PAGE_INFO pebs_pages_info_ring_buffer[SIZE_OF_PEBS_RING_BUFFER];
extern uint32_t pebs_read_index;
extern uint32_t pebs_ring_buffer_write_index;
extern Parameters gParams;

long perf_event_open(struct perf_event_attr *pf_attr, pid_t pid, int cpu,
                            int group_fd, unsigned long flags);

void pebs_init(pid_t pid);

void pebs_shutdown(void);

void pebs_enable(void);

void pebs_update_period(uint64_t value);

#endif 
