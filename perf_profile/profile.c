#define _GNU_SOURCE

#include "profile.h"

#include <asm/unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "../common.h"
#include "parameters.h"
#include "perfmon/pfmlib.h"

uint64_t other_pages_cnt = 0;
uint64_t total_pages_cnt = 0;
uint64_t zero_pages_cnt = 0;

pthread_t scan_thread;

// define the lock
pthread_mutex_t lock;

__u64 input_pid;

// TODO: change PEBS_NPROCS constant to a variable
static struct perf_event_mmap_page *perf_page[PEBS_NPROCS][NPBUFTYPES];
int pfd[PEBS_NPROCS][NPBUFTYPES];

PAGE_INFO pebs_pages_info_ring_buffer[SIZE_OF_PEBS_RING_BUFFER];
uint32_t pebs_ring_buffer_write_index = 0;

extern Parameters gParams;

long perf_event_open(struct perf_event_attr *pf_attr, pid_t pid, int cpu,
                     int group_fd, unsigned long flags) {
  int ret;
  ret = syscall(__NR_perf_event_open, pf_attr, pid, cpu, group_fd, flags);
  return ret;
}

int get_file_size(const char *file) {
  struct stat statbuf;
  if (stat(file, &statbuf) == 0) {
    return statbuf.st_size;
  }
  return -1;
}

#define PAGE_NUMBER 0xffffffffff000
#define OFFSET 0x0000000000fff

unsigned long cvt_page_number(unsigned long virtual_addr) {
  return virtual_addr >> 12;
}

unsigned long cvt_page_boundary(unsigned long virtual_addr) {
  return ((virtual_addr & PAGE_NUMBER) >> 12) << 12;
}

static struct perf_event_mmap_page *perf_setup(__u64 config, __u64 config1,
                                               __u64 pid, __u64 cpu,
                                               __u64 type) {
  struct perf_event_attr attr;

  memset(&attr, 0, sizeof(struct perf_event_attr));

  /*
  If type is PERF_TYPE_RAW, then a custom "raw" config value is needed.
  Most CPUs support events that are not covered by the "generalized" events.
  These are implementation defined; see your CPU manual (for example the Intel
  Volume 3B documentation or the AMD BIOS and Kernel Developer Guide). The
  libpfm4 library can be used to translate from the name in the architectural
  manuals to the raw hex value perf_event_open() expects in this field.
 */
  attr.type = PERF_TYPE_RAW;
  attr.size = sizeof(struct perf_event_attr);

  attr.config = config;
  attr.config1 = config1;

  // The perf_events interface allows two modes to express the sampling period:
  // the number of occurrences of the event (period)
  // the average rate of samples/sec (frequency)

  // attr.sample_period = SAMPLE_PERIOD;
  // set attr.freq to 1 if we use sample_freq
  if (Parameters_get_sample_frequency(&gParams) > 0) {
    attr.freq = 1;
    attr.sample_freq = Parameters_get_sample_frequency(&gParams);
  } else {
    attr.freq = 0;
    // if (type == WRITE) {
    //   attr.sample_period = Parameters_get_store_sample_period(&gParams);
    // } else {
      attr.sample_period = Parameters_get_read_sample_period(&gParams);
    // }
    
  }

  attr.sample_type =
      PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_WEIGHT | PERF_SAMPLE_ADDR;
  attr.disabled = 0;
  // attr.sample_id_all = 1;
  // attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
  // PERF_FORMAT_TOTAL_TIME_RUNNING;
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1;
  attr.exclude_callchain_kernel = 1;
  attr.exclude_callchain_user = 1;
  attr.precise_ip = 1;

  // TODO: pid = -1 means all processes, will it work?
  pfd[cpu][type] = perf_event_open(&attr, pid, cpu, -1, 0);
  if (pfd[cpu][type] == -1) {
    perror("perf_event_open");
  }
  assert(pfd[cpu][type] != -1);

  size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
  printf("[perf_setup] mmap_size is: %lu\n", mmap_size);
  // size_t page_size = sysconf(_SC_PAGESIZE);
  struct perf_event_mmap_page *p = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, pfd[cpu][type], 0);
  if (p == MAP_FAILED) {
    perror("mmap");
  }
  assert(p != MAP_FAILED);
  return p;
}

void *pebs_scan_thread() {
  cpu_set_t cpuset;
  pthread_t thread;

  thread = pthread_self();
  CPU_ZERO(&cpuset);
  // Add CPU cpu to set.
  CPU_SET(Parameters_get_scanning_thread_cpu(&gParams), &cpuset);
  int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0) {
    perror("pebs_scan_thread pthread_setaffinity_np");
    assert(0);
  }

  /* pebs buffer size to store sample record(in KB), -1 means exclude the
   * metadata*/
  // size_t pebs_buffer_size = sysconf(_SC_PAGESIZE) * (PERF_PAGES - 1);
  /* Pebs sample size (in Bytes), here is 48 bytes*/
  // size_t sample_size = sizeof(struct perf_sample);
  // int num_smaples_per_buffer = pebs_buffer_size * 1024 / sample_size;
  // int max_num_samples = num_smaples_per_buffer * PEBS_NPROCS * NPBUFTYPES;

  // pid_t   tid = gettid();
  pid_t tid = syscall(SYS_gettid);
  printf("tid=%d\n", tid);

  // char file_name[64];
  // snprintf(file_name, sizeof(file_name), "profiling_results_%d.txt", tid);

  // FILE *f = fopen(file_name, "w");
  // if (f == NULL) {
  //   printf("Error opening file!\n");
  //   exit(1);
  // }

  unsigned long total_samples = 0;
  unsigned long total_throttle_cnt = 0;
  unsigned long total_unthrottle_cnt = 0;
  unsigned long total_event_sample_cnt[3] = {0, 0, 0};

  for (;;) {
    uint64_t throttle_cnt = 0;
    uint64_t unthrottle_cnt = 0;
    int samples_cnt = 0;
    int num_empty_pebs_buffer = 0;
    // timer
    sleep(Parameters_get_profiling_interval(&gParams));
    int i, j;
    // The number of pebs buffers which does not contain pebs samples
    // pthread_mutex_lock(&lock);
    for (i = 0; i < PEBS_NPROCS; i++) {
      for (j = 0; j < NPBUFTYPES; j++) {
        struct perf_event_mmap_page *p = perf_page[i][j];
        __sync_synchronize();
        char *pbuf = (char *)p + p->data_offset;

        if (p->data_head == p->data_tail) {
          num_empty_pebs_buffer++;
          continue;
        }

        struct perf_event_header *ph =
            (void *)(pbuf + (p->data_tail % p->data_size));
        struct perf_sample *ps;

        // if (i == 0 && j == 0) {
        // printf("The data_head is: %lu\n", p->data_head);
        // printf("The data_tail is: %lu\n", p->data_tail);
        // }
        // Traverse the buffer, check for new samples.
        while (p->data_head != p->data_tail) {
          ph = (void *)(pbuf + (p->data_tail % p->data_size));
          switch (ph->type) {
            case PERF_RECORD_SAMPLE:
              if ((p->data_size - (p->data_tail % p->data_size)) < ph->size) {
                printf("The unread data size %lu is not enough!\n", (p->data_size - (p->data_tail % p->data_size)));
                break;
              }
              ps = (struct perf_sample *)ph;
              assert(ps != NULL);
              //Since perf collected data for all pids, here we need to check if it is input_pid.
              if (ps->addr != 0 && ps->pid == input_pid) {
                unsigned long page_boundary = cvt_page_number((void *)ps->addr);
                // fprintf(f, "%p %lu\n", ps->addr, page_boundary);
                // if (ps->pid == input_pid) {
                //     printf("****** (PAGES PID) The pebs page's pid: %d\n",
                //     ps->pid);
                // }
                // printf("###### (PAGES ADDRESSES) The address is: %p\n",
                // ps->addr); printf("###### (PAGES BOUNDARY) The pebs page
                // number:  %lu\n", page_boundary);
                //  Check if buffer is full
                if ((pebs_ring_buffer_write_index + 1) %
                        SIZE_OF_PEBS_RING_BUFFER ==
                    pebs_read_index) {
                  printf("The pebs ring buffer is full!\n");
                } else {
                  // Increase write index position
                  pebs_ring_buffer_write_index =
                      (pebs_ring_buffer_write_index + 1) %
                      SIZE_OF_PEBS_RING_BUFFER;
                  // printf("###### (PEBS PROFILING)The pebs page number:%lu\n",
                  // page_boundary);
                  /* Add page boundary into the ring buffer */
                  pebs_pages_info_ring_buffer[pebs_ring_buffer_write_index]
                      .event_type_id = j;
                  pebs_pages_info_ring_buffer[pebs_ring_buffer_write_index]
                      .virtual_page = page_boundary;
                }

                samples_cnt++;
                total_event_sample_cnt[j] = total_event_sample_cnt[j] + 1;
              }
              break;
            case PERF_RECORD_THROTTLE:
              throttle_cnt++;
              break;
            case PERF_RECORD_UNTHROTTLE:
              unthrottle_cnt++;
              break;
            default:
              break;
          }  // End of the switch
          p->data_tail += ph->size;
        }  // End of while loop
        __sync_synchronize();
        // printf("profiling interval (s): %d\n",
        //        Parameters_get_profiling_interval(&gParams));
        // printf("samples_cnt per second is: %.1f\n", (double)samples_cnt
        // / Parameters_get_profiling_interval(&gParams)); printf("num of
        // throttle cnt per second is: %.1f\n", (double)throttle_cnt /
        // Parameters_get_profiling_interval(&gParams)); printf("num of
        // unthrottle cnt per second is: %.1f\n", (double)unthrottle_cnt /
        // Parameters_get_profiling_interval(&gParams));

      }  // End of loop NPBUFTYPES
    }    // End of loop PEBS_NPROCS
         // pthread_mutex_unlock(&lock);
    total_samples += samples_cnt;
    total_throttle_cnt += throttle_cnt;
    total_unthrottle_cnt += unthrottle_cnt;
    printf("The total number of samples is: %ul\n", total_samples);
    printf("The total number of throttle cnt is: %ul\n", total_throttle_cnt);
    printf("The total number of unthrottle cnt is: %ul\n",
           total_unthrottle_cnt);
    printf("L_DRAMREAD: %ul, L_PMMREAD: %ul,  WRITE: %ul\n",
           total_event_sample_cnt[0], total_event_sample_cnt[1],
           total_event_sample_cnt[2]);
    // printf("Pebs profiling, read index is: %d\n", read_index);
    // printf("Pebs profiling, write index is: %d\n", write_index);
  }  // End of infinity loop
  // close(f);
  return NULL;
}

void pebs_init(pid_t pid) {
  printf("pebs_init: started pid %d\n", pid);

  input_pid = pid;

  // Initialize pfm library (required before we can use it)

  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    errx(1, "Cannot initialize library: %s", pfm_strerror(ret));
  }

  struct perf_event_attr attr;

  memset(&attr, 0, sizeof(attr));

  ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM",
                                    PFM_PLMH, &attr, NULL, NULL);

  if (ret != PFM_SUCCESS) {
    err(1, " cannot get encoding %s", pfm_strerror(ret));
  } else {
    printf("success event 1\n");
  }
  assert(ret == PFM_SUCCESS);
  __u64 event1 = attr.config;

  // ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.REMOTE_DRAM",
  //                                   PFM_PLMH, &attr, NULL, NULL);
  ret = pfm_get_perf_event_encoding("MEM_LOAD_RETIRED.LOCAL_PMM", PFM_PLMH,
                                    &attr, NULL, NULL);
  if (ret != PFM_SUCCESS) {
    err(1, " cannot get encoding %s", pfm_strerror(ret));
  } else {
    printf("success event 2\n");
  }
  assert(ret == PFM_SUCCESS);

  __u64 event2 = attr.config;
  /*
  ret = pfm_get_perf_event_encoding("MEM_LOAD_RETIRED.LOCAL_PMM", PFM_PLMH,
  &attr, NULL, NULL);

  if (ret != PFM_SUCCESS) {
      err(1, " cannot get encoding %s", pfm_strerror(ret));
  } else {
      printf("success event 3\n");
  }
  __u64 event3 = attr.config;

  ret = pfm_get_perf_event_encoding("MEM_LOAD_RETIRED.REMOTE_PMM", PFM_PLMH,
  &attr, NULL, NULL);

  if (ret != PFM_SUCCESS) {
      err(1, " cannot get encoding %s", pfm_strerror(ret));
  } else {
      printf("success event 4\n");
  }
  __u64 event4 = attr.config;
  */

  ret = pfm_get_perf_event_encoding("MEM_INST_RETIRED.ALL_STORES", PFM_PLMH,
                                    &attr, NULL, NULL);
  if (ret != PFM_SUCCESS) {
    err(1, " cannot get encoding %s", pfm_strerror(ret));
  } else {
    printf("success event 3\n");
  }
  __u64 event3 = attr.config;

  // printf("events number are %x, %x, %x, %x, %x\n", event1, event2, event3,
  // event4, event5); printf("events number are %x\n", event1);
  printf("events number are %x, %x, %x\n", event1, event2, event3);
  int i;
  for (i = 0; i < PEBS_NPROCS; i++) {
    perf_page[i][L_DRAMREAD] =
        perf_setup(event1, 0, -1, i, L_DRAMREAD);  // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM
    perf_page[i][L_PMMREAD] =
        perf_setup(event2, 0, -1, i, L_PMMREAD);  // MEM_LOAD_L3_MISS_RETIRED.REMOTE_DRAM
    // perf_page[i][READ] = perf_setup(event3, 0, pid, i, READ);       //
    // MEM_LOAD_RETIRED.ALL_LOADS perf_page[i][R_NVMREAD] =
    // perf_setup(event4,
    // 0, pid, i, R_NVMREAD);       // MEM_LOAD_RETIRED.REMOTE_PMM
    // perf_page[i][WRITE] = perf_setup(
    //     event3, 0, -1, i, WRITE);  // MEM_INST_RETIRED.ALL_STORES
  }
  // for (i = 0; i < PEBS_NPROCS; i++) {
  //   perf_page[i][L_DRAMREAD] = perf_setup(
  //       event1, 0, input_pid, i, L_DRAMREAD);  // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM

  //   perf_page[i][L_PMMREAD] = perf_setup(
  //       event2, 0, input_pid, i, L_PMMREAD);  // MEM_LOAD_L3_MISS_RETIRED.REMOTE_DRAM

  //   perf_page[i][WRITE] =
  //       perf_setup(event3, 0, input_pid, i, WRITE);  // MEM_INST_RETIRED.ALL_STORES
  // }
  int r = pthread_create(&scan_thread, NULL, pebs_scan_thread, NULL);
  assert(r == 0);
  printf("pebs_init: finished\n");
}

void pebs_enable() {
  int i, j;
  for (i = 0; i < PEBS_NPROCS; i++) {
    for (j = 0; j < NPBUFTYPES; j++) {
      if (ioctl(pfd[i][j], PERF_EVENT_IOC_ENABLE, 0) == -1) {
        printf("Error ioctl for pfd!\n");
        exit(1);
      }
    }
  }
  printf("pebs_enable: finished\n");
}

void pebs_shutdown() {
  void *ret = NULL;
  // cancel the scan thread
  int p_cancel = pthread_cancel(scan_thread);
  assert(0 == p_cancel);

  int i, j;
  for (i = 0; i < PEBS_NPROCS; i++) {
    for (j = 0; j < NPBUFTYPES; j++) {
      if (ioctl(pfd[i][j], PERF_EVENT_IOC_DISABLE, 0) == -1) {
        printf("Error ioctl for pfd!\n");
        exit(1);
      }
      if (munmap(perf_page[i][j], sysconf(_SC_PAGESIZE) * PERF_PAGES) == -1) {
        printf("Error munmap perf_page!\n");
        exit(1);
      }
    }
  }
  // memset(pebs_pages_info_ring_buffer, 0, sizeof(pebs_pages_info_ring_buffer));
  // pebs_ring_buffer_write_index = 0;

  int p_join = pthread_join(scan_thread, &ret);
  assert(0 == p_join);
  printf("pebs profiling is shutdown now\n");
}


void pebs_update_period(uint64_t value)
{
  int cpu, event;
  for (cpu = 0; cpu < PEBS_NPROCS; cpu++) {
    for (event = 0; event < NPBUFTYPES; event++) {
      if (!pfd[cpu][event]) {
        continue;
      }
      int ret = ioctl(pfd[cpu][event], PERF_EVENT_IOC_PERIOD, &value);
      if (ret == -1) {
        printf("update sample period failed\n");
        exit(1);
      }
      printf("update sample period success, value: %lu\n", value);
      // ret = perf_event_period(pfd[cpu][event], value);
      // if (ret == -EINVAL) {
      //   printk("failed to update sample period");
      // }
    }
  }
}
