#ifndef ML_GUIDED_PAGR_MIGRATION_COMMON_H
#define ML_GUIDED_PAGR_MIGRATION_COMMON_H

// When to move from the inactive list to the active list
const int NUM_INACT = 1;

// No of pages: (192 * 1024 * 1024) / 4 = 50331648
// We need to define the bit_set_size as 50331648
#define BIT_SET_SIZE 50331648

// application cpu location
const int APP_CPU = 0;

const int FAST_NUMA_NODE = 0;

const int SLOW_NUMA_NODE = 2;

const int MAX_PEBS_FETCH_NUM = 4000;

#endif
