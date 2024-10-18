#include "smaps.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <numaif.h>

#define MAX_NUMA_NODE                   4

char *(field_name_short[]) = {
    "size", "kps",  "mmu",  "rss",   "pss",   "pssd",  "shrc",  "shrd",
    "pric", "prid", "ref",  "anon",  "lzf",   "anonp", "shmap", "fimap",
    "shhu", "prhu", "swap", "swpss", "lockd", "thp",   "pkey",
};

char *(field_name[]) = {
    "Size:",
    "KernelPageSize:",
    "MMUPageSize:",
    "Rss:",
    "Pss:",
    "Pss_Dirty:",
    "Shared_Clean:",
    "Shared_Dirty:",
    "Private_Clean:",
    "Private_Dirty:",
    "Referenced:",
    "Anonymous:",
    "LazyFree:",
    "AnonHugePages:",
    "ShmemPmdMapped:",
    "FilePmdMapped:",
    "Shared_Hugetlb:",
    "Private_Hugetlb:",
    "Swap:",
    "SwapPss:",
    "Locked:",
    "THPeligible:",
    "ProtectionKey:",
};

unsigned long smaps_pages_ring_buffer[SIZE_OF_SMAPS_RING_BUFFER];
uint32_t smaps_ring_buffer_write_index = 0;
char linebuf[BUFSIZ];

#define RET_SYNTAX_ERROR(x)                    \
  do {                                         \
    fprintf(stderr, "Syntax error!\n%s\n", x); \
    smaps_free(&head_seg);                     \
    break;                                     \
  } while (0)

void smaps_free(type_seg **smaps_head) {
  type_seg *head = *smaps_head;
  while (head != NULL) {
    type_seg *tmp = head;
    head = head->next;
    free(tmp);
  }
  *smaps_head = NULL;
}

int policy;
unsigned long nodemask = 1; // NUMA node 0
unsigned long maxnode = sizeof(nodemask) * 8;


int getNode(int client_pid, uint64_t address)
{
    void* addr = (void*)address;
    int status = -1;
    int ret = move_pages(client_pid, 1, &addr, NULL, &status, MPOL_MF_MOVE);
    if(ret)
    {
        fprintf(stderr,
         "move_pages(%d, 1, &addr, NULL, &status, MPOL_MF_MOVE) failed: ", client_pid);
    }
    if(status < 0 || status >= MAX_NUMA_NODE)
        status = 0xf;
    return status;
}

type_seg *smaps(int client_pid) {
  int i, j;
  FILE *file;
  type_seg *head_seg = NULL;
  char filename[64];
  char start_va[20], end_va[20];
  char start_page[20], end_page[20];
  unsigned long lu_start_page, lu_end_page;
  struct timeval start, end;
  int valid_line = 0;

  sprintf(filename, "/proc/%d/smaps", client_pid);


  file = fopen(filename, "r");
  if (!file) {
    perror(filename);
    return NULL;
  }

  gettimeofday(&start, NULL);
  while (1) {
    if (!fgets(linebuf, sizeof(linebuf), file)) {
      if (ferror(file)) {
        perror(filename);
        smaps_free(&head_seg);
        return NULL;
      } else {
        break;
      }
    }

    type_seg preg = {0};
    if (sscanf(linebuf, "%s%s%s%s%s%s", preg.range, preg.perm, preg.offset,
               preg.device, preg.inode, preg.path) < 5) {
      continue;
    }

    if (sscanf(preg.range, "%[0-9a-f]-%[0-9a-f]", start_va, end_va) < 2) {
      continue;
    }

    sscanf(start_va, "%s\n", start_page);
    sscanf(end_va, "%s\n", end_page);
    sscanf(start_page, "%lx\n", &lu_start_page);
    sscanf(end_page, "%lx\n", &lu_end_page);

    if (lu_end_page - lu_start_page <= 1024 * 1024 * 1024) {
      continue;
    }

    valid_line++;

    // printf("smaps_debug start_page: %s, end_page: %s\n", start_page,
    // end_page);

    // printf("smaps_debug lu_start_page: %lu, lu_end_page: %lu, diff: %lu\n",
    //        lu_start_page, lu_end_page, lu_end_page - lu_start_page);
    for (unsigned long page_number = lu_start_page; page_number < lu_end_page;
         page_number += (1 << 12)) {
      // if ((smaps_ring_buffer_write_index + 1) % SIZE_OF_SMAPS_RING_BUFFER ==
      //     smaps_read_index) {
      //   // printf("The smaps_ring_buffer is full!\n");
      //   // break;
      // } else {
        // if (get_mempolicy(&policy, &nodemask, maxnode, (void *)page_number, MPOL_F_ADDR) != -1) {
        //     if (nodemask & 1) {
        //         // printf("Page is in NUMA node 0\n");
        //         unsigned long page_boundary = page_number >> 12;
        //         smaps_ring_buffer_write_index =
        //             (smaps_ring_buffer_write_index + 1) % SIZE_OF_SMAPS_RING_BUFFER;
        //         smaps_pages_ring_buffer[smaps_ring_buffer_write_index] = page_boundary;
        //         // printf("smaps ring buffer page number: %lu\n",
        //         // smaps_pages_ring_buffer[smaps_ring_buffer_write_index]);
        //     } else {
        //         // printf("Page is not in NUMA node 0\n");
        //     }
        // } else {
        //     // perror("get_mempolicy");
        // }
        // int status[1] = {-1}; // status array for the result
        // void *pages[1]; // array of pointers to the pages
        // pages[0] = (void *)page_number;
        int status = getNode(client_pid, page_number);
        if (status == 0) {
                // printf("Page is in NUMA node 0\n");
                unsigned long page_boundary = page_number >> 12;
                smaps_ring_buffer_write_index = (smaps_ring_buffer_write_index + 1) % SIZE_OF_SMAPS_RING_BUFFER;
                smaps_pages_ring_buffer[smaps_ring_buffer_write_index] = page_boundary;
                // printf("smaps ring buffer page number: %lu\n", smaps_pages_ring_buffer[smaps_ring_buffer_write_index]);
        }
      // }
    }
  }
  gettimeofday(&end, NULL);
  uint64_t scan_smaps_duration =
      (end.tv_sec - start.tv_sec) * 1000000L + end.tv_usec - start.tv_usec;
  printf("smaps_debug Scan smaps duration is: %ld microseconds\n",
         scan_smaps_duration);
  printf("end time: %ld, valid_line: %d\n", end.tv_sec, valid_line);

  fclose(file);

  return head_seg;
}
