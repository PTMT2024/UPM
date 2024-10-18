#include "samples.h"

#include <sys/ioctl.h>
extern "C" {
#include <perfmon/pfmlib.h>


#include "../smaps/smaps.h"
}
#include <sys/resource.h>
#include <thread>
#include <chrono>

using namespace std;

extern Parameters gParams;
int num_pebs_fetch = 0;
std::mutex myMutex;
std::mutex scanProcMutex;
std::atomic<bool> should_stop_tuning(false);
std::atomic<bool> should_exit(false);

// LFU module: use LFU-based activeList and inactiveList
// Use the activeList to track hot pages in the fast memory
// Use the inactiveList to track the candidates of hot pages to be migrated to
// the fast memory

LFU* activeLFU = new LFU();

LFU* inactiveLFU = new LFU();

vector<unsigned long> hot_pages_;

vector<unsigned long> all_pages_;

vector<unsigned long> cold_pages_;

vector<unsigned long> smaps_cold_pages_;

vector<unsigned long> active_pages_;

vector<unsigned long> inactive_pages_;

deque<unsigned long> smaps_pages_deque_exclude_cbf1;

deque<unsigned long> smaps_pages_deque_exclude_cbf2;

unsigned long long int total_page_migrated;

uint32_t pebs_read_index = 0;

bool is_second_queue = false;

uint32_t smaps_read_index = 0;

unsigned long page_demotion_batch_size = 300000;

uint64_t ctrl_free_mem_KB = 128 * 1024;

void warmup(unsigned int seconds) {
  cout << "warmup for " << seconds << " seconds" << endl;
  sleep(seconds);
}

bool is_process_running(pid_t pid) {
  return (kill(pid, 0) == 0);
}

void check_workload_is_still_runing(pid_t client_pid) {
  while (1) {
    sleep(1);
    if (!is_process_running(client_pid)) {
      // process doesn't exist anymore
      cout << "Application finished, close the accept_fd." << endl;
      break;
    }
  }
}

std::vector<long> setup_perf_event_for_period_collection_for_all_cpus(
  int pid, long long config) {
  std::vector<long> cpu_fd_map(Parameters_get_pebs_nprocs(&gParams));

  struct perf_event_attr pe;

  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = config;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  for (int cpu = 0; cpu < Parameters_get_pebs_nprocs(&gParams); cpu++) {
    long fd = perf_event_open(&pe, pid, cpu, -1, 0);
    cout << "run_logic setup_perf_event_for_period_collection_for_all_cpus: "
      << cpu << endl;
    if (fd == -1) {
      perror("perf_event_open");
      exit(EXIT_FAILURE);
    }
    cpu_fd_map[cpu] = fd;
  }
  return cpu_fd_map;
}

long long get_sum_of_all_cpu_fd(std::vector<long>& cpu_fd_map) {
  long sum = 0;
  int cpu_count = Parameters_get_pebs_nprocs(&gParams);
  for (int cpu = 0; cpu < cpu_count; cpu++) {
    cout << "run_logic get_sum_of_all_cpu_fd" << cpu << endl;
    long long num;
    read(cpu_fd_map[cpu], &num, sizeof(long long));
    sum += num;
  }
  return sum;
}

void enable_all_cpu_fd(std::vector<long>& cpu_fd_map) {
  int cpu_count = Parameters_get_pebs_nprocs(&gParams);
  for (int cpu = 0; cpu < cpu_count; cpu++) {
    ioctl(cpu_fd_map[cpu], PERF_EVENT_IOC_ENABLE, 0);
  }
}

void disable_all_cpu_fd(std::vector<long>& cpu_fd_map) {
  int cpu_count = Parameters_get_pebs_nprocs(&gParams);
  for (int cpu = 0; cpu < cpu_count; cpu++) {
    ioctl(cpu_fd_map[cpu], PERF_EVENT_IOC_DISABLE, 0);
  }
}

void refresh_active_pages() {
  active_pages_.clear();
  activeLFU->list_pages_by_freq(active_pages_, false);
}

void refresh_inactive_pages() {
  inactive_pages_.clear();
  inactiveLFU->list_pages_by_freq(inactive_pages_, false);
}

void periodicallyResetOperationToActiveList(LFU* activeList) {
  // Periodically reset to all page counters to 0
  activeLFU->SetListValueToZero();
}

void display_deque(deque<unsigned long>& smaps_pages_deque_exclude_cbf) {
  cout << "################## (BEFORE) Get deque size ####################"
    << endl;
  cout << "Deque size is: " << smaps_pages_deque_exclude_cbf.size() << endl;
  cout << "################## (AFTER) Get deque size  ####################"
    << endl;
  // Displaying set elements
}

void display_vector(vector<unsigned long>& pages_) {
  vector<unsigned long>::iterator vec_itr;
  cout << "Pages vector size is: " << pages_.size() << endl;
  // Display vector elements
  // for (vec_itr = pages_.begin(); vec_itr != pages_.end(); vec_itr++) {
  //    cout << *vec_itr << " ";
  //}
  // cout << endl;
}

void counting_bloom_add_page(CountingBloom cb, unsigned long page,
  pid_t client_pid) {
  if (page > 0) {
    std::string page_id = std::to_string(page);
    std::string process_id = std::to_string(client_pid);
    std::string full_page_id_to_add = process_id + page_id;
    // cout << "################## full_page_id_to_add: ####################: "
    // << full_page_id_to_add << endl;
    int r = counting_bloom_add_string(&cb, (full_page_id_to_add).c_str());
    // cout << "##################### (BEFORE) Status of counting bloom filter:
    // " << endl; counting_bloom_stats(&cb); unsigned long test_page = page;
    // std::string test_page_id = std::to_string(test_page);
    // std::string tes_process_id = "TEST";
    // std::string full_test_page_id = tes_process_id + test_page_id;
    // int res = counting_bloom_check_string(&cb, (full_test_page_id).c_str());
    // for (unsigned int i = 0; i < cb.number_bits; i++) {
    //     cout << cb.bloom[i] << " ";
    // }
    // cout << endl;
    // cout << "##################### (AFTER) Status of counting bloom filter: "
    // << full_test_page_id << ": " << res << endl;
    if (r == 0) {
      // cout << "Successfully to add page into counting bloom filter." << endl;
    }
    else if (r == -1) {
      cout << "Failture to add page into counting bloom filter." << endl;
    }
  }
}

void print_page_access() {
   myMutex.lock(); 
  std::cout << "################################### (INACTIVE LIST) Pages Cnt "
    << "################################# " << std::endl;
  uint32_t cnt = inactiveLFU->PrintLFUCnt();
  std::cout << "total_cnt: " << cnt << std::endl;
  std::cout << "################################### (ACTIVE LIST) Pages Cnt "
    << "################################# " << std::endl;
  cnt = activeLFU->PrintLFUCnt();
  std::cout << "total_cnt: " << cnt << std::endl;
    myMutex.unlock();
}

void print_page_access_timer(Timer& t, int interval) {
  auto func_with_params = [] {
    print_page_access();
    };
  t.start(interval, func_with_params);
}

void counting_bloom_remove_page(CountingBloom cb, unsigned long page,
  pid_t client_pid) {
  if (page > 0) {
    std::string page_id = std::to_string(page);
    std::string process_id = std::to_string(client_pid);
    std::string full_page_id_to_remove = process_id + page_id;
    if (counting_bloom_check_string(&cb, (full_page_id_to_remove).c_str()) ==
      0) {
      int r =
        counting_bloom_remove_string(&cb, (full_page_id_to_remove).c_str());
      if (r == 0) {
        // cout << "Successfully to remove page from counting bloom filter." <<
        // endl;
      }
      else if (r == -1) {
        cout << "Failtur:to remove page from counting bloom filter." << endl;
      }
    }
  }
}

void process_pebs_profiling_module(PAGE_INFO pebs_pages_info_ring_buffer[],
  uint32_t read_index,
  uint32_t pebs_ring_buffer_write_index,
  pid_t client_pid, CountingBloom cb,
  LFU* inactiveList, LFU* activeList) {
  // auto start_clock = std::chrono::steady_clock::now();
  // cout << "!!!!!!!! debugging0 pebs_profiling_module, read index: " <<
  // read_index << endl; cout << "!!!!!!!! debugging0 pebs_profiling_module,
  // write index: " << pebs_ring_buffer_write_index << endl;
  while (read_index != pebs_ring_buffer_write_index) {
    unsigned long page_from_pebs =
      pebs_pages_info_ring_buffer[read_index].virtual_page;
    uint16_t event_type_id =
      pebs_pages_info_ring_buffer[read_index].event_type_id;
    // cout << "(PEBS) read_index: " << read_index << ";
    // pebs_ring_buffer_write_index: " << pebs_ring_buffer_write_index <<";
    // event_type_id: " << event_type_id << "; page_from_pebs: " <<
    // page_from_pebs << "; num_pebs_fetch: " << num_pebs_fetch << endl;
    if (page_from_pebs <= 0) {
      // // cout << "!!!!!!!! if statement in pebs_profiling_module, write
      // index"
      // // << endl; cout << "(PEBS IF STATEMENT) read_index: " << read_index <<
      // ";
      // // pebs_ring_buffer_write_index: " << pebs_ring_buffer_write_index <<";
      // // event_type_id: " << event_type_id << "; page_from_pebs: " <<
      // // page_from_pebs <<
      // // "; num_pebs_fetch: " << num_pebs_fetch << endl;
      // uint32_t next_read_index = (read_index + 1) % SIZE_OF_PEBS_RING_BUFFER;
      // unsigned long next_page_from_pebs =
      //     pebs_pages_info_ring_buffer[next_read_index].virtual_page;
      // if (page_from_pebs == 0 && next_page_from_pebs > 0) {
      //   read_index = (read_index + 1) % SIZE_OF_PEBS_RING_BUFFER;
      // }
    }
    else {
      // cout << "!!!!!!!! pebs_profiling_module" << read_index <<
      // endl;
      /*
      if (activeList->isNodePresent(page_from_pebs)) {
          activeList->Retrieve(page_from_pebs);
      } else {
          inactiveList->Retrieve(page_from_pebs);
      }
      */
      num_pebs_fetch++;
      if (num_pebs_fetch % 2000000 == 0) {
        activeList->CoolingDown();
        inactiveList->CoolingDown();
      }
      if (event_type_id == L_DRAMREAD) {
        activeList->Retrieve(page_from_pebs);
        counting_bloom_add_page(cb, page_from_pebs, client_pid);
      }
      else if (event_type_id == L_PMMREAD) {
        inactiveList->Retrieve(page_from_pebs);
        counting_bloom_add_page(cb, page_from_pebs, client_pid);
      }
      else {
        // inactiveList->Retrieve(page_from_pebs);
        counting_bloom_add_page(cb, page_from_pebs, client_pid);
      }
    }
    read_index = (read_index + 1) % SIZE_OF_PEBS_RING_BUFFER;
  }
  // auto end_clock = std::chrono::steady_clock::now();
  // auto temp_duration_time =
  // chrono::duration_cast<chrono::microseconds>(end_clock -
  // start_clock).count(); cout << "Build cbf time is: " << temp_duration_time
  // << " us." << endl;
}

void process_smaps_scanning_module(unsigned long smaps_pages_ring_buffer[],
  uint32_t read_index,
  uint32_t smaps_ring_buffer_write_index,
  pid_t client_pid, CountingBloom cb,
  deque<unsigned long>& smaps_pages_deque) {
  cout << "[Movepage] process_smaps_scanning_module smaps_read_index: "
    << read_index
    << "; smaps_ring_buffer_write_index: " << smaps_ring_buffer_write_index
    << endl;
  int num_smaps_fetch = 0;
  int cb_hit_count = 0;
  // cout << "##################### (BEFORE) smaps counting_bloom_stats
  // #####################: " << read_index << endl; counting_bloom_stats(&cb);
  // cout << "##################### (AFTER)  smaps counting_bloom_stats
  // #####################: " << smaps_ring_buffer_write_index << endl;
  // pages_set.clear();
  while (read_index != smaps_ring_buffer_write_index &&
    num_smaps_fetch < SIZE_OF_SMAPS_RING_BUFFER) {
    unsigned long page_from_smaps = smaps_pages_ring_buffer[read_index];
    num_smaps_fetch++;

    if (page_from_smaps <= 0) {
      cout << "(SMAPS) invalid_page_from_smaps is: " << page_from_smaps
        << "; smaps_read_index: " << read_index
        << "; smaps_ring_buffer_write_index: "
        << smaps_ring_buffer_write_index
        << "; num_smaps_fetch: " << num_smaps_fetch << endl;
      uint32_t next_read_index = (read_index + 1) % SIZE_OF_SMAPS_RING_BUFFER;
      unsigned long next_page_from_smaps =
        smaps_pages_ring_buffer[next_read_index];
      if (page_from_smaps == 0 && next_page_from_smaps > 0) {
        read_index = (read_index + 1) % SIZE_OF_SMAPS_RING_BUFFER;
      }
      continue;
    }
    else {
      // Increase the smaps read index
      read_index = (read_index + 1) % SIZE_OF_SMAPS_RING_BUFFER;
      std::string smaps_page_id = std::to_string(page_from_smaps);
      std::string process_id = std::to_string(client_pid);
      std::string full_smaps_page_id = process_id + smaps_page_id;
      // cout << "##################### (BEFORE) full_smaps_page_id: " <<
      // full_smaps_page_id << endl; counting_bloom_stats(&cb); cout <<
      // "##################### (BEFORE) full_smaps_page_id: " <<
      // full_smaps_page_id << endl; cout << "#####################
      // full_smaps_page_id: " << full_smaps_page_id << " VS " <<
      // (full_smaps_page_id).c_str() << endl; int res =
      // counting_bloom_check_string(&cb, (full_smaps_page_id).c_str()); cout <<
      // "Return value of counting bloom filter is:" << res << endl;
      if (counting_bloom_check_string(&cb, (full_smaps_page_id).c_str()) ==
        COUNTING_BLOOM_SUCCESS) {
        cb_hit_count++;
        // cout << full_smaps_page_id << " is in the counting bloom a maximum of
        // " << counting_bloom_get_max_insertions(&cb,
        // (full_smaps_page_id).c_str()) << " times!" << endl;
        continue;
      }
      else {
        // cout << "##################### (BEFORE) smaps page set insert
        // #####################" << endl;
        smaps_pages_deque.push_back(page_from_smaps);
        // cout << "##################### (AFTER)  smaps page set insert
        // #####################" << endl;
      }
    }
  }
  cout << "[Movepage] process_smaps_scanning_module num_smaps_fetch: "
    << num_smaps_fetch << ", cb_hit_count: " << cb_hit_count << endl;
}

void get_smaps_cold_pages(deque<unsigned long>& pages_deque,
  vector<unsigned long>& cold_pages,
  unsigned long num_demotion_pages) {
  if (num_demotion_pages <= 0) {
    return;
  }
  if (pages_deque.size() == 0) {
    return;
  }
  if (pages_deque.size() < num_demotion_pages) {
    num_demotion_pages = pages_deque.size();
  }
  // auto gen = std::mt19937{std::random_device{}()};
  unsigned long sampled_cold_page;

  for (int i = 0; i < num_demotion_pages; i++) {
    // std::sample(pages_set.begin(), pages_set.end(), &sampled_cold_page, 1,
    // gen);
    sampled_cold_page = pages_deque.front();
    cold_pages.push_back(sampled_cold_page);
    pages_deque.pop_front();
  }
}

int do_migrate_pages(vector<unsigned long>& pages_to_move, bool is_hot,
  pid_t client_pid, CountingBloom cb, LFU* inactiveLFU,
  LFU* activeLFU) {
  unsigned long long int page_count = pages_to_move.size();

  if (page_count <= 0) {
    return -1;
  }
  auto initialization_start_clock = std::chrono::steady_clock::now();
  int dest[page_count];
  int status[page_count];
  unsigned long addr[page_count];
  void* address[page_count];
  unsigned int i;
  unsigned long fast_node_migration_failure_count = 0;
  unsigned long slow_node_migration_failure_count = 0;
  float success_ratio;
  unsigned long success_movement_count = 0;
  // void * source;
  // source = (void *) &address[0];
  for (i = 0; i < page_count; i++) {
    address[i] = (void*)(pages_to_move[i] << 12);
    status[i] = -123;
  }

  if (is_hot) {
    // Slow to fast, destination numa node is 0
    // cout << "############ The destination numa node is: #############" <<
    // endl;
    memset(dest, FAST_NUMA_NODE, page_count * sizeof(int));

    // for (i = 0; i < page_count; i++) {
    //     cout << dest[i] << " ";
    // }
    // cout << endl;
  }
  else {
    // Fast to slow, destination numa node is SLOW_NUMA_NODE
    // cout << "############ The destination numa node is: #############" <<
    // endl;
    for (i = 0; i < page_count; i++) {
      dest[i] = SLOW_NUMA_NODE;
      // cout << dest[i] << " ";
    }
    // cout << endl;
  }

  auto move_page_start_clock = std::chrono::steady_clock::now();
  auto migration_param_initial_time =
    chrono::duration_cast<chrono::microseconds>(move_page_start_clock -
      initialization_start_clock)
    .count();
  // When status is set to NULL, no migration occurs
  long move_page_err =
    move_pages(client_pid, page_count, address, dest, status, MPOL_MF_MOVE);
  /*
  cout << "############ The status after move_pages: ############" << endl;
  for (i = 0; i < page_count; i++) {
          cout << status[i] << " ";
  }
  cout << endl;
  */
  auto move_page_end_clock = std::chrono::steady_clock::now();
  auto move_page_time = chrono::duration_cast<chrono::microseconds>(
    move_page_end_clock - move_page_start_clock)
    .count();
  // Based on the error.h, status will return -16 if the page is busy
  // -EBUSY: the page is currently busy and cannot be moved.

  std::unordered_map<int, int> hot_page_error_count;
  std::unordered_map<int, int> cold_page_error_count;

  for (i = 0; i < page_count; i++) {
    if (is_hot && status[i] != FAST_NUMA_NODE) {
      // inactiveLFU->Retrieve(pages_to_move[i]);
      fast_node_migration_failure_count++;
      hot_page_error_count[status[i]]++;
    }
    else if (!is_hot && status[i] != SLOW_NUMA_NODE) {
      // activeLFU->Retrieve(pages_to_move[i]);
      slow_node_migration_failure_count++;
      cold_page_error_count[status[i]]++;

      // TODO Update counting bloom filter?
    }
  }
  auto status_check_end_clock = std::chrono::steady_clock::now();
  auto status_check_time = chrono::duration_cast<chrono::microseconds>(
    status_check_end_clock - move_page_end_clock)
    .count();

  // Add the page into the cb or remove the page out of the cb
  // This is based on the outcome status of the move_pages API
  for (i = 0; i < page_count; i++) {
    if (is_hot && status[i] == FAST_NUMA_NODE) {
      int freq = inactiveLFU->Evict(pages_to_move[i]);
      activeLFU->Set(pages_to_move[i], freq);
      // activeLFU->Retrieve(pages_to_move[i]);
      // counting_bloom_add_page(cb, pages_to_move[i], client_pid);
    }
    else if (!is_hot && status[i] == SLOW_NUMA_NODE) {
      int freq = activeLFU->Evict(pages_to_move[i]);
      inactiveLFU->Retrieve(pages_to_move[i]);
      // counting_bloom_add_page(cb, pages_to_move[i], client_pid);
    }
  }
  auto cbf_update_end_clock = std::chrono::steady_clock::now();
  auto cbf_update_time = chrono::duration_cast<chrono::microseconds>(
    cbf_update_end_clock - status_check_end_clock)
    .count();

  if (move_page_err == 0) {
    auto move_page_time1 = chrono::duration_cast<chrono::microseconds>(
      move_page_end_clock - move_page_start_clock)
      .count();
    cout << "Move Page Time: " << move_page_time << " us." << endl;
  }
  else {
    cout << "[Movepage] move_page_err: " << move_page_err
      << " ,errno: " << errno << endl;
    switch (errno) {
    case E2BIG:
      cout << "Error E2BIG: Too many pages to move." << endl;
      break;
    case EACCES:
      cout << "Error EACCES: One of the target nodes is not allowed by the "
        "current cpuset."
        << endl;
      break;
    case EFAULT:
      cout << "Error EFAULT: Parameter array could not be accessed." << endl;
      break;
    case EINVAL:
      cout << "Error EINVAL: Flags other than MPOL_MF_MOVE and "
        "MPOL_MF_MOVE_ALL was specified or an attempt was made to "
        "migrate pages of a kernel thread."
        << endl;
      break;
    case ENODEV:
      cout << "Error ENODEV: One of the target nodes is not online." << endl;
      break;
    case ENOENT:
      cout << "Error ENOENT: No pages were found that require moving. All "
        "pages are either already on the target node, not present, had "
        "an invalid address or could not be moved because they were "
        "mapped by multiple processes."
        << endl;
      break;
    case EPERM:
      cout << "Error EPERM: The caller specified MPOL_MF_MOVE_ALL without "
        "sufficient privileges (CAP_SYS_NICE). Or, the caller "
        "attempted to move pages of a process belonging to another "
        "user but did not have privilege to do so (CAP_SYS_NICE)."
        << endl;
      break;
    case ESRCH:
      cout << "Error ESRCH: Process does not exist." << endl;
      break;
    }  // End switch
  }    // End if statement for move_pages error checking

  // if (is_hot) {
  //   for (unsigned long& hot_page : pages_to_move) {
  //     // cout << "################# (HOT BEFORE) activeList/inactiveList evict and
  //     // retrive ################" << endl;
  //     if (inactiveLFU->isNodePresent(hot_page)) {
  //       inactiveLFU->Evict(hot_page);
  //     }
  //     // inactiveLFU->Evict(hot_page);
  //     activeLFU->Retrieve(hot_page);
  //     // cout << "################# Update the activeList and inactiveList for
  //     HOT
  //     // pages ################" << endl;
  //   }
  // } else {
  //   for (unsigned long& cold_page : pages_to_move) {
  //     // cout << "############### 1.Update the activeList and inactiveList for
  //     COLD
  //     // pages ###############" << endl;
  //     if (activeLFU->isNodePresent(cold_page)) {
  //       activeLFU->Evict(cold_page);
  //     }
  //     // activeLFU->Evict(cold_page);
  //     // cout << "############### 2.Update the activeList and inactiveList for
  //     COLD
  //     // pages ###############" << endl;

  //     // TODO: this may be a bug
  //     inactiveLFU->Retrieve(cold_page);
  //     // cout << "############### 3.Update the activeList and inactiveList for
  //     COLD
  //     // pages ###############" << endl;
  //   }
  // }
  auto list_update_end_clock = std::chrono::steady_clock::now();
  auto list_update_time = chrono::duration_cast<chrono::microseconds>(
    list_update_end_clock - cbf_update_end_clock)
    .count();
  cout << ">>>>>>>>>>>> Param initilize time:   "
    << migration_param_initial_time << " us." << endl;
  cout << ">>>>>>>>>>>> The move_pages time:    " << move_page_time << " us."
    << endl;
  cout << ">>>>>>>>>>>> The status check time:  " << status_check_time << " us."
    << endl;
  cout << ">>>>>>>>>>>> The CBF update time:    " << cbf_update_time << " us."
    << endl;
  cout << ">>>>>>>>>>>> Activelist update time: " << list_update_time << " us."
    << endl;

  if (is_hot) {
    success_movement_count = page_count - fast_node_migration_failure_count;
  }
  else {
    success_movement_count = page_count - slow_node_migration_failure_count;
  }
  success_ratio =
    round((float)success_movement_count / page_count * 10000) / 100;
  cout << ">>>>>>>>>>>> [Movepage] success ratio: " << success_ratio << "% "
    << "is_hot: " << is_hot << endl;
  cout << ">>>>>>>>>>>> [Movepage] Success movement count: "
    << success_movement_count << " is_hot: " << is_hot << endl;
  cout << ">>>>>>>>>>>> [Movepage] Total movement count:   " << page_count
    << " is_hot: " << is_hot << endl;
  // print err count

  cout << "[Movepage] Hot page error count:" << endl;
  for (const auto& pair : hot_page_error_count) {
    cout << "[Movepage] Hot Page Error type " << pair.first << ": "
      << pair.second << " occurrences" << endl;
  }
  cout << "[Movepage] Cold page error count:" << endl;
  for (const auto& pair : cold_page_error_count) {
    cout << "[Movepage] Cold Page Error type " << pair.first << ": "
      << pair.second << " occurrences" << endl;
  }
  return 0;
}

void do_migrate_pages_batch(vector<unsigned long>& pages_to_move, bool is_hot,
  pid_t client_pid, CountingBloom cb,
  LFU* inactiveLFU, LFU* activeLFU) {
  for (int i = 0; i < pages_to_move.size(); i += 100000) {
    vector<unsigned long> pages_to_move_batch;
    for (int j = i; j < i + 100000; j++) {
      if (j < pages_to_move.size()) {
        pages_to_move_batch.push_back(pages_to_move[j]);
      }
      else {
        break;
      }
    }
    cout << "[Movepage] pages_to_move_batch size: "
      << pages_to_move_batch.size() << ", is_hot:" << is_hot << endl;
    do_migrate_pages(pages_to_move_batch, is_hot, client_pid, cb, inactiveLFU,
      activeLFU);
  }
}

void tuning_logic(std::ofstream& csv_file, pid_t client_pid,
  int page_replace_type, int only_sampling) {
  enum Page_Replacement_Type { LFU = 1, LRU = 2, ML = 3, MIX = 4 };

  Algorithm* alg = new Algorithm(page_replace_type);
  cout << "type is: " << alg->get_type() << endl;

  cout << "mem_quota is (Kb): " << Parameters_get_mem_quota_KB(&gParams) << endl;
  cout << "sample_freq is: " << Parameters_get_sample_frequency(&gParams) << endl;
  cout << "read_sample_period is: " << Parameters_get_read_sample_period(&gParams)
    << endl;
  cout << "store_sample_period is: "
    << Parameters_get_store_sample_period(&gParams) << endl;
  cout << "profiling_interval is: " << Parameters_get_profiling_interval(&gParams)
    << endl;
  cout << "proc_scan_interval is: " << Parameters_get_proc_scan_interval(&gParams)
    << endl;
  cout << "page_fetch_interval is: "
    << Parameters_get_page_fetch_interval(&gParams) << endl;
  cout << "hot_page_threshold is: " << Parameters_get_hot_page_threshold(&gParams)
    << endl;
  cout << "page_migration_interval is: "
    << Parameters_get_page_migration_interval(&gParams) << endl;
  cout << "pebs_nprocs is: " << Parameters_get_pebs_nprocs(&gParams) << endl;

  switch (alg->get_type()) {
  case LFU:
    cout << "initialize the activeList/inactiveList of LFU." << endl;
    alg->set_activeLFU();
    alg->set_inactiveLFU();
    activeLFU = alg->get_activeLFU();
    inactiveLFU = alg->get_inactiveLFU();
    run_logic(csv_file, client_pid, only_sampling);
    break;
  case LRU:
    cout << "initialize the activeList/inactiveList of LRU." << endl;
    break;
  case ML:
    cout << "initialize the activeList/inactiveList of Machine Learning model."
      << endl;
    break;
  case MIX:
    cout << "initialize the activeList/inactiveList of mix LRU and LFU model."
      << endl;
    break;
  default:
    cout << "Input page replacement algorithm is invalid." << endl;
    break;
  }
}


void run_logic(std::ofstream& csv_file, pid_t client_pid, int only_sampling) {
  // cpu_set_t cpuset;
  // CPU_ZERO(&cpuset);
  // CPU_SET(APP_CPU, &cpuset);
  // int s = sched_setaffinity(client_pid, sizeof(cpu_set_t), &cpuset);
  // if (s != 0) {
  //   perror("could not set CPU affinity");
  //   assert(0);
  // }
  // std::mutex myMutex;
  // bool is_second_queue = false;

  CountingBloom cb;
  counting_bloom_init(&cb, 3000000, 0.01);
  smaps(client_pid);
  process_smaps_scanning_module(smaps_pages_ring_buffer, smaps_read_index,
                                smaps_ring_buffer_write_index, client_pid,
                                cb, smaps_pages_deque_exclude_cbf1);
  // display_deque(smaps_pages_deque_exclude_cbf1);

  // Timer t1;
  // Timer t2;
  // Timer t3;
  // Timer t4;

  std::thread t1([&]() {
    while (!should_stop_tuning) {
      fetch_page(client_pid, cb);
      cout << "page_fetch_interval: "
        << Parameters_get_page_fetch_interval(&gParams) << endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(Parameters_get_page_fetch_interval(&gParams) * 1000));
    }
    });
  std::thread t2([&]() {
    if (only_sampling) {
      cerr << "only_sampling is true, skip page migration" << endl;
      return;
    }
    while (!should_stop_tuning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(Parameters_get_page_migration_interval(&gParams) * 1000));
      migrate_page(client_pid, cb);
      cout << "page_migration_interval: "
        << Parameters_get_page_migration_interval(&gParams) << endl;
    }
    });
  std::thread t3([&]() {
    while (!should_stop_tuning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20 * 1000));
      scan_proc(client_pid, cb);
    }
    });
  std::thread t4([&]() {
    while (!should_stop_tuning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
      // print_page_access();
    }
    });

  // fetch_page_timer(t1, page_fetch_interval * 1000, client_pid, cb);
  // scan_proc_timer(t2, proc_scan_interval * 1000, client_pid, cb);
  // migrate_page_timer(t4, page_migration_interval * 1000, client_pid,
  //   mem_quota_Kb, initial_used_fast_mem_Kb, hot_page_threshold,
  //   cb);
  // print_page_access(t4, 5 * 1000);
  // t4.start(RESET_INTERVAL * 30000, [] {
  //         myMutex.lock();
  //         periodicallyResetOperationToActiveList(activeLFU);
  //         myMutex.unlock();
  // });
  // auto fd1 = setup_perf_event_for_period_collection(client_pid,
  //                                                   PERF_COUNT_HW_INSTRUCTIONS);
  // auto fd2 = setup_perf_event_for_period_collection(client_pid,
  //                                                   PERF_COUNT_HW_CPU_CYCLES);
  // // auto fds1 = setup_perf_event_for_period_collection_for_all_cpus(
  // //     client_pid, PERF_COUNT_HW_INSTRUCTIONS);
  // // auto fds2 = setup_perf_event_for_period_collection_for_all_cpus(
  // //     client_pid, PERF_COUNT_HW_CPU_CYCLES);
  // // unsigned long long start_process_time =
  // get_process_cpu_time(client_pid); unsigned long long start_total_time =
  // get_total_cpu_time();
  // // Enable the events
  // ioctl(fd1, PERF_EVENT_IOC_ENABLE, 0);
  // ioctl(fd2, PERF_EVENT_IOC_ENABLE, 0);
  // // enable_all_cpu_fd(fds1);
  // // enable_all_cpu_fd(fds2);

  // // Initialize PCM
  // pcm::PCM* pcm = pcm::PCM::getInstance();
  // pcm::PCM::ErrorCode status = pcm->program();
  // if (status != pcm::PCM::Success) {
  //   std::cerr << "Failed to initialize PCM: " << status << std::endl;
  //   return;
  // }
  //  // Only measure the memory BW of socket 0.
  // int socket = 0;
  // pcm::SocketCounterState beforeState = pcm->getSocketCounterState(socket);
  // uint64_t elapsedMicros = pcm->getTickCount(1000000);

  check_workload_is_still_runing(client_pid);
  cout << ">>>>>>>>>>>> [run_logic] workload finished execution <<<<<<<<<<<"
    << std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count()
    << endl;

  disable_tuning();
  t1.join();
  t2.join();
  t3.join();
  t4.join();




  // t1.stop();
  // t2.stop();
  // t4.stop();
  // t4.stop();


  // // unsigned long long end_process_time = get_process_cpu_time(client_pid);
  // unsigned long long end_total_time = get_total_cpu_time();
  // pcm::SocketCounterState afterState = pcm->getSocketCounterState(socket);
  // elapsedMicros = pcm->getTickCount(1000000) - elapsedMicros;

  // // uint64_t elapsedMicros = collect_period * 1000000;

  // // double processCpuTimeUsed = end_process_time - start_process_time;
  // double processCpuTimeUsed = 0;
  // double totalCpuTime = end_total_time - start_total_time;
  // double cpu_usage = processCpuTimeUsed / totalCpuTime;
  // // Disable the events
  // ioctl(fd1, PERF_EVENT_IOC_DISABLE, 0);
  // ioctl(fd2, PERF_EVENT_IOC_DISABLE, 0);
  // // disable_all_cpu_fd(fds1);
  // // disable_all_cpu_fd(fds2);

  // // read value from fd
  // long selfInstructions = 0;
  // long selfCycles = 0;
  // read(fd1, &selfInstructions, sizeof(long long));
  // read(fd2, &selfCycles, sizeof(long long));

  // cout << ">>>>>>>>>>>> [run_logic] read events finished <<<<<<<<<<<"
  //      << std::chrono::duration_cast<std::chrono::seconds>(
  //             std::chrono::system_clock::now().time_since_epoch())
  //             .count()
  //      << endl;

  // // cout << ">>>>>>>>>>>> [run_logic] get_sum_of_all_cpu_fd(fds1)
  // <<<<<<<<<<<"
  // //      << std::chrono::duration_cast<std::chrono::seconds>(
  // //             std::chrono::system_clock::now().time_since_epoch())
  // //             .count()
  // //      << endl;
  // // long long processInstructions = get_sum_of_all_cpu_fd(fds1);
  // // cout << ">>>>>>>>>>>> [run_logic] get_sum_of_all_cpu_fd(fds2)
  // <<<<<<<<<<<"
  // //      << std::chrono::duration_cast<std::chrono::seconds>(
  // //             std::chrono::system_clock::now().time_since_epoch())
  // //             .count()
  // //      << endl;
  // // long long processCycles = get_sum_of_all_cpu_fd(fds2);

  // // cout << ">>>>>>>>>>>> [run_logic] get_sum_of_all_cpu_fd finished
  // // <<<<<<<<<<<"
  // //      << std::chrono::duration_cast<std::chrono::seconds>(
  // //             std::chrono::system_clock::now().time_since_epoch())
  // //             .count()
  // //      << endl;

  // save_result_to_csv(elapsedMicros, beforeState, afterState, csv_file,
  //                    mem_quota_Kb, selfInstructions, selfCycles,
  //                    processCpuTimeUsed, totalCpuTime, cpu_usage);

  // cout << ">>>>>>>>>>>> [run_logic] save_result_to_csv finished <<<<<<<<<<<"
  //      << std::chrono::duration_cast<std::chrono::seconds>(
  //             std::chrono::system_clock::now().time_since_epoch())
  //             .count()
  //      << endl;
  // std::set<unsigned long> mySet;
  // for (int i = 0; i < pebs_ring_buffer_write_index; i++) {
  //   mySet.insert(pebs_pages_info_ring_buffer[i].virtual_page);
  // }
  // cout << "Movepage uniq page size:" << mySet.size() << endl;
}

bool should_terminate() {
  return should_exit;
}


void disable_tuning() {
  should_stop_tuning = true;
  cout << ">>>>>>>>>>>> [disable_tuning] should_stop_tuning <<<<<<<<<<<"
    << std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count()
    << endl;
}

void exit_proc() {
  should_exit = true;
  cout << ">>>>>>>>>>>> [exit_proc] should_exit <<<<<<<<<<<"
    << std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count()
    << endl;
}

void start_tuning(int warmup_seconds, pid_t client_pid, int page_replace_type, int only_sampling) {
  cout << "start_tuning pid: " << client_pid << endl;
  // open or create a csv file to store the result data
  ofstream csv_file;
  // csv_file.open(result_file, ios::out | ios::app);
  // if (csv_file.tellp() == 0) {
  //   csv_file <<
  //   "warmup(s),mem_quota(GB),sample_freq,read_sample_period,store_"
  //               "sample_period,"
  //               "profiling_interval(s),"
  //               "proc_scan_interval(s),page_fetch_interval(s),hotpage_"
  //               "threshold,page_migration_interval(s),elapsedSeconds,"
  //               "Instructions,Cycles,IPC,IPUS, "
  //               "WrittenToPMM(MB/s),ReadFromPMM(MB/s),WrittenToDRAMMC(MB/"
  //               "s),ReadFromDRAMMC(MB/s),L3CacheMisses, L2CacheMisses, "
  //               "L3CacheMisses(/us),L2CacheMisses(/us),"
  //               "L3CacheHitRatio,L2CacheHitRatio,LLCReadMissLatency(ns),"
  //               "process_cpu_time(jiffies),total_cpu_tim(jiffies),cpu_usage"
  //            << endl;
  // }

  warmup(warmup_seconds);
  pebs_init(client_pid);
  pebs_enable();
  tuning_logic(csv_file, client_pid, page_replace_type, only_sampling);
  pebs_shutdown();
  cout << ">>>>>>>>>>>> [run_logic] pebs_shutdown <<<<<<<<<<<" << endl;
  exit_proc();
}

// void start_collecting_sliced_period(int warmup_seconds, pid_t client_pid,
//   int accept_fd, int page_replace_type,
//   int pages_count, int pebs_nprocs,
//   int collect_period, string result_file,
//   int mem_quota_KB) {
//   warmup(warmup_seconds);
//   // open or create a csv file to store the result data
//   ofstream csv_file;
//   csv_file.open(result_file, ios::out | ios::app);
//   if (csv_file.tellp() == 0) {
//     csv_file << "warmup(s),mem_quota(GB),sample_freq,read_sample_period,store_"
//       "sample_period,"
//       "profiling_interval(s),"
//       "proc_scan_interval(s),page_fetch_interval(s),hotpage_"
//       "threshold,page_migration_interval(s),threads,elapsedSeconds,"
//       "Instructions,Cycles,IPC,IPUS, "
//       "WrittenToPMM(MB/s),ReadFromPMM(MB/s),WrittenToDRAMMC(MB/"
//       "s),ReadFromDRAMMC(MB/s),L3CacheMisses, L2CacheMisses, "
//       "L3CacheMisses(/us),L2CacheMisses(/us),"
//       "L3CacheHitRatio,L2CacheHitRatio,LLCReadMissLatency(ns),"
//       "process_cpu_time(jiffies),total_cpu_tim(jiffies),cpu_usage"
//       << endl;
//   }

//   // Initialize PCM
//   pcm::PCM* pcm = pcm::PCM::getInstance();
//   pcm::PCM::ErrorCode status = pcm->program();
//   if (status != pcm::PCM::Success) {
//     std::cerr << "Failed to initialize PCM: " << status << std::endl;
//     return;
//   }

//   while (1) {
//     Parameters_generate_random(&gParams, pages_count, pebs_nprocs,
//       collect_period, warmup_seconds, mem_quota_KB);
//     collect_one_period(csv_file, pcm, client_pid, accept_fd, &gParams,
//       page_replace_type, initial_used_fast_mem_Kb);
//   }
// }

void scan_proc(pid_t client_pid, CountingBloom& cb) {
    scanProcMutex.lock();
    auto start_clock = std::chrono::steady_clock::now();
    cout << "************ [scan_proc] SCANNING SMAPS" << endl;
    cout << "************ [scan_proc] (BEFORE SCANNING) read index is: " << smaps_read_index
      << "; write index is: " << smaps_ring_buffer_write_index << endl;
    if (smaps_pages_deque_exclude_cbf1.size() <= page_demotion_batch_size) {
      smaps(client_pid);
      process_smaps_scanning_module(smaps_pages_ring_buffer, smaps_read_index,
        smaps_ring_buffer_write_index, client_pid,
        cb, smaps_pages_deque_exclude_cbf1);
      smaps_read_index = smaps_ring_buffer_write_index;
    }
    else if (smaps_pages_deque_exclude_cbf2.size() <=
      page_demotion_batch_size) {
      smaps(client_pid);
      process_smaps_scanning_module(smaps_pages_ring_buffer, smaps_read_index,
        smaps_ring_buffer_write_index, client_pid,
        cb, smaps_pages_deque_exclude_cbf2);
      smaps_read_index = smaps_ring_buffer_write_index;
    }
    cout << "************ [scan_proc] Queue1 size: "
      << smaps_pages_deque_exclude_cbf1.size()
      << "; Queue2 size: " << smaps_pages_deque_exclude_cbf2.size() << endl;
    cout << "************ [scan_proc] (AFTER SCANNING) read index is: " << smaps_read_index
      << "; write index is: " << smaps_ring_buffer_write_index << endl;
    auto end_clock = std::chrono::steady_clock::now();
    auto temp_duration_time =
      chrono::duration_cast<chrono::microseconds>(end_clock - start_clock)
      .count();
    cout << "************ [scan_proc] duration: "
      << temp_duration_time << " us." << endl;
    cout << endl;
    scanProcMutex.unlock();
}

void fetch_page(pid_t client_pid, CountingBloom& cb) {
  // mutex
  // cout << "1.pebs_read_index is: " << pebs_read_index <<
  // "; 2.pebs_ring_buffer_write_index is: " <<
  // pebs_ring_buffer_write_index << endl;
  myMutex.lock();
  process_pebs_profiling_module(pebs_pages_info_ring_buffer, pebs_read_index,
    pebs_ring_buffer_write_index, client_pid, cb,
    inactiveLFU, activeLFU);
  pebs_read_index = pebs_ring_buffer_write_index;
  myMutex.unlock();
  // cout << endl;
  // cout << "################################### (ACTIVE LIST) Pages
  // ################################# " << endl; activeLFU->PrintLFU();
  // cout << endl;
  // cout << "################################### (INACTIVE LIST) Pages
  // ################################# " << endl;
  // inactiveLFU->PrintLFU();
  // cout << "################################### (BEFORE)
  // Pebscounting_bloom_stats ################################# " <<
  // endl; counting_bloom_stats(cb); cout <<
  // "################################### (AFTER)
  // Pebscounting_bloom_stats ################################# " <<
  // endl; cout << "3.pebs_read_index is: " << pebs_read_index
  // <<"; 4.pebs_ring_buffer_write_index is: " <<
  // pebs_ring_buffer_write_index << endl;
}

void fetch_page_timer(Timer& t, int interval, pid_t client_pid,
  CountingBloom& cb) {
  auto func_with_params = [client_pid, cb] {
    // mutex
    // cout << "1.pebs_read_index is: " << pebs_read_index <<
    // "; 2.pebs_ring_buffer_write_index is: " <<
    // pebs_ring_buffer_write_index << endl;
    myMutex.lock();
    process_pebs_profiling_module(pebs_pages_info_ring_buffer, pebs_read_index,
      pebs_ring_buffer_write_index, client_pid, cb,
      inactiveLFU, activeLFU);
    pebs_read_index = pebs_ring_buffer_write_index;
    myMutex.unlock();
    // cout << endl;
    // cout << "################################### (ACTIVE LIST) Pages
    // ################################# " << endl; activeLFU->PrintLFU();
    // cout << endl;
    // cout << "################################### (INACTIVE LIST) Pages
    // ################################# " << endl;
    // inactiveLFU->PrintLFU();
    // cout << "################################### (BEFORE)
    // Pebscounting_bloom_stats ################################# " <<
    // endl; counting_bloom_stats(cb); cout <<
    // "################################### (AFTER)
    // Pebscounting_bloom_stats ################################# " <<
    // endl; cout << "3.pebs_read_index is: " << pebs_read_index
    // <<"; 4.pebs_ring_buffer_write_index is: " <<
    // pebs_ring_buffer_write_index << endl;
    };
  t.start(interval, func_with_params);
}

void scan_proc_timer(Timer& t, int interval, pid_t client_pid,
  CountingBloom& cb) {
  auto func_with_params = [client_pid, cb] {
    auto start_clock = std::chrono::steady_clock::now();
    cout << "************ SCANNING SMAPS" << endl;
    cout << "************ (BEFORE SCANNING) read index is: " << smaps_read_index
      << "; write index is: " << smaps_ring_buffer_write_index << endl;
    if (smaps_pages_deque_exclude_cbf1.size() <= page_demotion_batch_size) {
      smaps(client_pid);
      process_smaps_scanning_module(smaps_pages_ring_buffer, smaps_read_index,
        smaps_ring_buffer_write_index, client_pid,
        cb, smaps_pages_deque_exclude_cbf1);
      smaps_read_index = smaps_ring_buffer_write_index;
    }
    else if (smaps_pages_deque_exclude_cbf2.size() <=
      page_demotion_batch_size) {
      smaps(client_pid);
      process_smaps_scanning_module(smaps_pages_ring_buffer, smaps_read_index,
        smaps_ring_buffer_write_index, client_pid,
        cb, smaps_pages_deque_exclude_cbf2);
      smaps_read_index = smaps_ring_buffer_write_index;
    }
    cout << "************ Queue1 size: "
      << smaps_pages_deque_exclude_cbf1.size()
      << "; Queue2 size: " << smaps_pages_deque_exclude_cbf2.size() << endl;
    cout << "************ (AFTER SCANNING) read index is: " << smaps_read_index
      << "; write index is: " << smaps_ring_buffer_write_index << endl;
    auto end_clock = std::chrono::steady_clock::now();
    auto temp_duration_time =
      chrono::duration_cast<chrono::microseconds>(end_clock - start_clock)
      .count();
    cout << "************ The time of building deque from smaps is: "
      << temp_duration_time << " us." << endl;
    cout << endl;
    };

  t.start(interval, func_with_params);
}

unsigned long long get_total_cpu_time() {
  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) {
    perror("Failed to open /proc/stat");
    exit(EXIT_FAILURE);
  }

  unsigned long long user, nice, system, idle;
  if (fscanf(fp, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle) !=
    4) {
    perror("Failed to read /proc/stat");
    fclose(fp);
    exit(EXIT_FAILURE);
  }

  fclose(fp);
  return user + nice + system + idle;
}

unsigned long long get_process_cpu_time(pid_t pid) {
  char path[50];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);

  FILE* fp = fopen(path, "r");
  if (!fp) {
    perror("Failed to open stat /proc/pid/stat");
    exit(EXIT_FAILURE);
  }

  unsigned long long utime, stime;
  if (fscanf(fp,
    "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
    &utime, &stime) != 2) {
    perror("Failed to read /proc/pid/stat");
    fclose(fp);
    exit(EXIT_FAILURE);
  }

  fclose(fp);
  return utime + stime;
};

void send_info_to_tcp_client(pid_t client_pid) {
  while (1) {
    sleep(1);
    struct stat sts;
    string stat_filename = "/proc/" + std::to_string(client_pid);
    if (stat(stat_filename.c_str(), &sts) == -1) {
      // process doesn't exist anymore
      cout << "Application finished, close the accept_fd." << endl;
      break;
    }
  }
}

void send_info_to_tcp_client_timer(Timer& t, int interval, pid_t client_pid,
  int accept_fd, Timer& t1, Timer& t2,
  Timer& t4) {
  // every 2 seconds, check the application process status and send to client
  auto func_with_params = [client_pid, accept_fd, &t1, &t2, &t4, &t] {
    struct stat sts;
    string stat_filename = "/proc/" + std::to_string(client_pid);
    if (stat(stat_filename.c_str(), &sts) == -1) {
      // process doesn't exist anymore
      cout << "Application finished, close the accept_fd." << endl;
      close(accept_fd);
      t1.stop();
      t2.stop();
      t4.stop();
      t.stop();
    }
    // send messeage back to client
    string cc = "hot page count: " + std::to_string(hot_pages_.size());
    if (send(accept_fd, cc.data(), cc.size(), 0) == -1) {
      cout << "Close the accept_fd." << endl;
      close(accept_fd);
      t1.stop();
      t2.stop();
      t4.stop();
      t.stop();
    }
    };
  t.start(interval, func_with_params);
}

long long get_left_mem_quota_Kb() {
  long long page_size = sysconf(_SC_PAGESIZE) / 1024;
  long long current_free_fast_mem_byte = 0;
  long long current_free_fast_mem_Kb = 0;
  long long total_fast_mem_Kb = 0;
  // unsigned long free_fast_mem_Gb, total_fast_mem_Gb;
  long long total_fast_mem_byte =
    numa_node_size64(FAST_NUMA_NODE, &current_free_fast_mem_byte);
  if (total_fast_mem_byte < 0) {
    perror(
      "Function numa_node_size64 error, can not get total fast "
      "memmory size");
    assert(0 <= total_fast_mem_byte);
  }
  current_free_fast_mem_Kb = floor(current_free_fast_mem_byte / 1024);
  total_fast_mem_Kb = floor(total_fast_mem_byte / 1024);
  // long long used_fast_mem_Kb =
  //   total_fast_mem_Kb - current_free_fast_mem_Kb - initial_used_fast_mem_Kb;

  // long long left_mem_quota_Kb = mem_quota_Kb - used_fast_mem_Kb;
  long long left_mem_quota_Kb = current_free_fast_mem_Kb;
  return left_mem_quota_Kb;
}

void migrate_page(pid_t client_pid,
  CountingBloom& cb) {
  myMutex.lock();
  int hot_page_threshold = Parameters_get_hot_page_threshold(&gParams);
  cout << "==========>> [Movepage] Hot page threshold is: " << hot_page_threshold << endl;
  auto t3_start_clock = std::chrono::steady_clock::now();
  long long page_size = sysconf(_SC_PAGESIZE) / 1024;

  inactiveLFU->get_hot_pages(hot_pages_, hot_page_threshold, activeLFU);
  // hot_pages_.push_back(23093438091);
  // hot_pages_.push_back(23093438092);
  // hot_pages_.push_back(23093438093);
  // hot_pages_.push_back(23093438094);
  // hot_pages_.push_back(23093438095);
  auto get_hot_page_from_inactiveLFU_end_clock =
    std::chrono::steady_clock::now();
  auto get_hot_page_time =
    chrono::duration_cast<chrono::microseconds>(
      get_hot_page_from_inactiveLFU_end_clock - t3_start_clock)
    .count();
  cout << "==========>> Number of hot pages to be promoted found by "
    "Pebs: "
    << hot_pages_.size() << endl;
  cout << "---------->> Time of getting hot page from inactiveLFU is: "
    << get_hot_page_time << " us." << endl;
  // display_vector(hot_pages_);
  // cout << "Pages vector size is: " << pages_.size() << en
  //  Migrate pages in the activeList from slow memory to fast memory
  long num_hot_pages = hot_pages_.size();
  assert(num_hot_pages >= 0);

  long long left_mem_quota_Kb =
    get_left_mem_quota_Kb();
  long max_migrated_pages =
    floor((left_mem_quota_Kb - ctrl_free_mem_KB) / page_size);

  cout << "############ [Movepage] CURRENT MEM UASAGE INFO" << endl;
  cout << "------------ [Movepage] Number of hot pages:    " << num_hot_pages
    << endl;
  cout << "------------ [Movepage] Left fast memory quota: "
    << left_mem_quota_Kb << " Kb = " << left_mem_quota_Kb / 1024
    << " Mb = " << left_mem_quota_Kb / 1024 / 1024 << " Gb" << endl;
  cout << "------------ [Movepage] Max num pages migrate:  "
    << max_migrated_pages << endl;

  if ((left_mem_quota_Kb > ctrl_free_mem_KB) && num_hot_pages > 0 &&
    num_hot_pages <= max_migrated_pages) {
    // Fast memory has enough space, direcctly move hot pages to fast
    // memory
    cout << "############ (BRANCH 1) FAST MEM HAS ENOUGH SPACE" << endl;
    auto branch1_start_clock = std::chrono::steady_clock::now();

    auto branch1_begins = std::chrono::system_clock::now();
    std::time_t branch1_now_time_t1 =
      std::chrono::system_clock::to_time_t(branch1_begins);
    auto branch1_now_microsec1 =
      std::chrono::time_point_cast<std::chrono::microseconds>(
        branch1_begins);
    auto branch1_fractional_microsec1 =
      branch1_now_microsec1.time_since_epoch() % std::chrono::seconds(1);
    std::tm* branch1_now_tm1 = std::localtime(&branch1_now_time_t1);
    std::cout << "==========>> Before branch1 page migration: "
      << std::put_time(branch1_now_tm1, "%Y-%m-%d %H:%M:%S") << '.'
      << std::setw(6) << std::setfill('0')
      << branch1_fractional_microsec1.count() << std::endl;
    do_migrate_pages_batch(hot_pages_, true, client_pid, cb, inactiveLFU,
      activeLFU);
    auto branch1_end_clock = std::chrono::steady_clock::now();

    auto branch1_ends = std::chrono::system_clock::now();
    std::time_t branch1_now_time_t2 =
      std::chrono::system_clock::to_time_t(branch1_ends);
    auto branch1_now_microsec2 =
      std::chrono::time_point_cast<std::chrono::microseconds>(branch1_ends);
    auto branch1_fractional_microsec2 =
      branch1_now_microsec2.time_since_epoch() % std::chrono::seconds(1);
    std::tm* branch1_now_tm2 = std::localtime(&branch1_now_time_t2);
    std::cout << "==========>> After branch1 page migration:  "
      << std::put_time(branch1_now_tm2, "%Y-%m-%d %H:%M:%S") << '.'
      << std::setw(6) << std::setfill('0')
      << branch1_fractional_microsec2.count() << std::endl;
    auto get_branch1_time = chrono::duration_cast<chrono::microseconds>(
      branch1_end_clock - branch1_start_clock)
      .count();
    cout << "---------->> Branch1 time is:        " << get_branch1_time
      << " us." << endl;
  }
  else if (num_hot_pages > 0) {
    unsigned long page_demotion_num = num_hot_pages;
    // if (left_mem_quota_Kb > ctrl_free_mem_KB) {
    //   page_demotion_num =
    //     num_hot_pages - (left_mem_quota_Kb - ctrl_free_mem_KB) / page_size;
    // }
    // else {
    //   page_demotion_num =
    //     num_hot_pages + (ctrl_free_mem_KB - left_mem_quota_Kb) / page_size;
    // }

    // unsigned long page_demotion_num = page_demotion_batch_size;
    // unsigned long page_demotion_batch_size = num_hot_pages -
    // max_migrated_pages; unsigned long page_demotion_batch_size =
    // 300000;

    // activeLFU->get_cold_pages(cold_pages_, left_num_demotion_pages);
    //  5.Migrate the left cold pages from the fast memory to slow
    //  memory
    // do_migrate_pages_batch(cold_pages_, false, client_pid, cb, inactiveLFU,
    // activeLFU); cout << "(FINISHED STEP 5). Migrate left cold pages
    // from the fast memory to slow memory (left_num_demotion_pages >
    // 0)" << endl;

    cout << "############ (BRANCH 2) Fast MEM DOES NOT HAVE ENOUGH SPACE"
      << endl;
    // 1.Get cold pages from the smaps in fast memory first
    cout << "(FINISHED STEP 1). Get cold pages from the smaps in fast "
      "memory first"
      << endl;
     auto get_smaps_page_start_clock = std::chrono::steady_clock::now();
      cout << "############ (BRANCH 2) Fast MEM DOES NOT HAVE ENOUGH SPACE"
        << endl;
      // 1.Get cold pages from the smaps in fast memory first
      cout << "(FINISHED STEP 1). Get cold pages from the smaps in fast "
        "memory first"
        << endl;
      if (smaps_pages_deque_exclude_cbf1.size() <= page_demotion_num &&
          smaps_pages_deque_exclude_cbf2.size() > 0) {
        is_second_queue = true;
      }
      if (is_second_queue) {
        cout << "############ [Movepage](QUEUE2) Smaps pages size:"
             << smaps_pages_deque_exclude_cbf2.size() << endl;
        get_smaps_cold_pages(smaps_pages_deque_exclude_cbf2,
        smaps_cold_pages_, page_demotion_num);
        cout << "==========>> [Movepage] (QUEUE2) Size of fetched cold pages from smaps: "
             << smaps_cold_pages_.size() << endl;
      }
      if (!is_second_queue) {
        cout << "############ [Movepage](QUEUE1) Smaps pages size:"
             << smaps_pages_deque_exclude_cbf1.size() << endl;
        get_smaps_cold_pages(smaps_pages_deque_exclude_cbf1,
        smaps_cold_pages_,page_demotion_num);
        cout << "==========>> [Movepage] (QUEUE1) Size of fetched cold pages from smaps: "
             << smaps_cold_pages_.size() << endl;
      }
      auto get_smaps_page_end_clock = std::chrono::steady_clock::now();
      auto get_smaps_page_time =
          chrono::duration_cast<chrono::microseconds>(
              get_smaps_page_end_clock - get_smaps_page_start_clock)
              .count();
      cout << "---------->> Get a batch of cold pages from sample, the "
              "time is: "
           << get_smaps_page_time << " us." << endl;

      // // 2.Migrate cold hidden pages in smaps from fast memory to slow
      // // memory
      auto demote_smaps_page_start_clock = std::chrono::steady_clock::now();

      // auto demotion_begins = std::chrono::system_clock::now();
      // std::time_t now_time_t1 =
      //     std::chrono::system_clock::to_time_t(demotion_begins);
      // auto now_microsec1 =
      //     std::chrono::time_point_cast<std::chrono::microseconds>(
      //         demotion_begins);
      // auto fractional_microsec1 =
      //     now_microsec1.time_since_epoch() % std::chrono::seconds(1);
      // std::tm* now_tm1 = std::localtime(&now_time_t1);
      // std::cout << "==========>> Before page demotion from smaps timestamp: "
      //           << std::put_time(now_tm1, "%Y-%m-%d %H:%M:%S") << '.'
      //           << std::setw(6) << std::setfill('0')
      //           << fractional_microsec1.count() << std::endl;

      if (smaps_cold_pages_.size() > 0) {
        do_migrate_pages_batch(smaps_cold_pages_, false, client_pid, cb,
                             inactiveLFU, activeLFU);
      }
      auto demote_smaps_page_end_clock = std::chrono::steady_clock::now();

      // auto demotion_ends = std::chrono::system_clock::now();
      // std::time_t now_time_t2 =
      //     std::chrono::system_clock::to_time_t(demotion_ends);
      // auto now_microsec2 =
      //     std::chrono::time_point_cast<std::chrono::microseconds>(
      //         demotion_ends);
      // auto fractional_microsec2 =
      //     now_microsec2.time_since_epoch() % std::chrono::seconds(1);
      // std::tm* now_tm2 = std::localtime(&now_time_t2);
      // std::cout << "==========>> After page demotion from smaps timestamp:  "
      //           << std::put_time(now_tm2, "%Y-%m-%d %H:%M:%S") << '.'
      //           << std::setw(6) << std::setfill('0')
      //           << fractional_microsec2.count() << std::endl;
      // // auto epoch_duration_microsec =
      // //
      // std::chrono::duration_cast<std::chrono::microseconds>(demote_smaps_page_start_clock.time_since_epoch());
      auto demote_smaps_page_time =
          chrono::duration_cast<chrono::microseconds>(
              demote_smaps_page_end_clock - demote_smaps_page_start_clock)
              .count();
      cout << "(FINISHED STEP 2). Migrate cold hidden pages in smaps "
              "from fast memory to slow memory."
           << endl;
      cout << "---------->> Page demotion time: " << demote_smaps_page_time
           << " us." << endl;
      unsigned long left_num_demotion_pages = page_demotion_num;
      if (smaps_cold_pages_.size() < page_demotion_num) {
        left_num_demotion_pages = page_demotion_num -
        smaps_cold_pages_.size();
      } else {
        left_num_demotion_pages = 0;
      }
      if (smaps_cold_pages_.size() > 0) {
        smaps_cold_pages_.clear();
      }
      // display_vector(smaps_cold_pages_);

    cout << "(FINISHED STEP 3). Get number of left pages to be demoted "
      "from fast memory in activeLFU: "
      << left_num_demotion_pages << endl;
    if (left_num_demotion_pages > 0) {
      // 4.Get left cold pages from the activeList in fast memory
      activeLFU->get_cold_pages(cold_pages_, left_num_demotion_pages,
        hot_page_threshold);
      cout << "(FINISHED STEP 4). Get left cold pages from activeList "
        "in fast memory (left_num_demotion_pages > 0)"
        << endl;
      // 5.Migrate the left cold pages from the fast memory to slow
      // memory
      do_migrate_pages_batch(cold_pages_, false, client_pid, cb, inactiveLFU,
        activeLFU);
      cout << "(FINISHED STEP 5). Migrate left cold pages from the "
        "fast memory to slow memory (left_num_demotion_pages > 0)"
        << "Size of fetched cold pages from LRU: " << cold_pages_.size()
        << endl;
    }
    // long long new_left_mem_quota_Kb =
    //   get_left_mem_quota_Kb();
    // long max_migrated_pages =
    //   floor((new_left_mem_quota_Kb - ctrl_free_mem_KB) / page_size);
    // long max_migrated_pages = cold_pages_.size();
    // if (max_migrated_pages > 0) {
      // if (max_migrated_pages < hot_pages_.size()) {
      //   cout << "############ [Movepage] Max num pages migrate:  "
      //     << max_migrated_pages
      //     << " is less than hot_page_num: " << num_hot_pages << " is_hot: 1"
      //     << endl;
      //   hot_pages_.resize(max_migrated_pages);
      // }

      // 6.Migrate hot pages from the slow memory to fast memory
      auto promotion_page_start_clock = std::chrono::steady_clock::now();

      auto promotion_begins = std::chrono::system_clock::now();
      std::time_t now_time_t3 =
        std::chrono::system_clock::to_time_t(promotion_begins);
      auto now_microsec3 =
        std::chrono::time_point_cast<std::chrono::microseconds>(
          promotion_begins);
      auto fractional_microsec3 =
        now_microsec3.time_since_epoch() % std::chrono::seconds(1);
      std::tm* now_tm3 = std::localtime(&now_time_t3);
      std::cout << "==========>> Before page promotion timestamp: "
        << std::put_time(now_tm3, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0')
        << fractional_microsec3.count() << std::endl;

      do_migrate_pages_batch(hot_pages_, true, client_pid, cb, inactiveLFU,
        activeLFU);
      auto promotion_page_end_clock = std::chrono::steady_clock::now();

      auto promotion_ends = std::chrono::system_clock::now();
      std::time_t now_time_t4 =
        std::chrono::system_clock::to_time_t(promotion_ends);
      auto now_microsec4 =
        std::chrono::time_point_cast<std::chrono::microseconds>(
          promotion_ends);
      auto fractional_microsec4 =
        now_microsec4.time_since_epoch() % std::chrono::seconds(1);
      std::tm* now_tm4 = std::localtime(&now_time_t4);
      std::cout << "==========>> After page promotion timestamp:  "
        << std::put_time(now_tm4, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0')
        << fractional_microsec4.count() << std::endl;

      auto promotion_page_time =
        chrono::duration_cast<chrono::microseconds>(
          promotion_page_end_clock - promotion_page_start_clock)
        .count();
      auto branch2_time =
        chrono::duration_cast<chrono::microseconds>(
          promotion_page_end_clock - get_smaps_page_start_clock)
        .count();
      cout << "(FINISHED STEP 6). Migrate hot pages from the slow memory "
        "to fast memory"
        << endl;
      cout << "---------->> The branch2 total time: "
        << branch2_time
        //  << " us. [ 1.page fetching: " << get_smaps_page_time
        //  << " us; 2.page demotion: " << demote_smaps_page_time
        << " us; 3.page promotion: " << promotion_page_time << " us. ]"
        << endl;
    // }
  }

  auto t3_end_clock = std::chrono::steady_clock::now();
  auto t3_total_time = chrono::duration_cast<chrono::microseconds>(
    t3_end_clock - t3_start_clock)
    .count();
  cout << "---------->> Get hot pages from LFU: " << get_hot_page_time
    << " us." << endl;
  cout << "---------->> Thread t4 total time:   " << t3_total_time << " us."
    << endl;
  cout << endl;
  myMutex.unlock();
}

void migrate_page_timer(Timer& t, int interval, pid_t client_pid,
  unsigned long mem_quota_Kb,
  unsigned long initial_used_fast_mem_Kb,
  int hot_page_threshold, CountingBloom& cb) {
  auto func_with_params = [client_pid, mem_quota_Kb, initial_used_fast_mem_Kb,
    hot_page_threshold, cb] {
    // mutex
    myMutex.lock();
    auto t3_start_clock = std::chrono::steady_clock::now();
    long long page_size = sysconf(_SC_PAGESIZE) / 1024;

    inactiveLFU->get_hot_pages(hot_pages_, hot_page_threshold, activeLFU);
    // hot_pages_.push_back(23093438091);
    // hot_pages_.push_back(23093438092);
    // hot_pages_.push_back(23093438093);
    // hot_pages_.push_back(23093438094);
    // hot_pages_.push_back(23093438095);
    auto get_hot_page_from_inactiveLFU_end_clock =
      std::chrono::steady_clock::now();
    auto get_hot_page_time =
      chrono::duration_cast<chrono::microseconds>(
        get_hot_page_from_inactiveLFU_end_clock - t3_start_clock)
      .count();
    cout << "==========>> Number of hot pages to be promoted found by "
      "Pebs: "
      << hot_pages_.size() << endl;
    cout << "---------->> Time of getting hot page from inactiveLFU is: "
      << get_hot_page_time << " us." << endl;
    // display_vector(hot_pages_);
    // cout << "Pages vector size is: " << pages_.size() << en
    //  Migrate pages in the activeList from slow memory to fast memory
    long num_hot_pages = hot_pages_.size();
    assert(num_hot_pages >= 0);

    long long left_mem_quota_Kb =
      get_left_mem_quota_Kb();
    long max_migrated_pages =
      floor((left_mem_quota_Kb - ctrl_free_mem_KB) / page_size);

    cout << "############ [Movepage] CURRENT MEM UASAGE INFO" << endl;
    cout << "------------ [Movepage] Number of hot pages:    " << num_hot_pages
      << endl;
    cout << "------------ [Movepage] Left fast memory quota: "
      << left_mem_quota_Kb << " Kb = " << left_mem_quota_Kb / 1024
      << " Mb = " << left_mem_quota_Kb / 1024 / 1024 << " Gb" << endl;
    cout << "------------ [Movepage] Max num pages migrate:  "
      << max_migrated_pages << endl;

    if ((left_mem_quota_Kb > ctrl_free_mem_KB) && num_hot_pages > 0 &&
      num_hot_pages <= max_migrated_pages) {
      // Fast memory has enough space, direcctly move hot pages to fast
      // memory
      cout << "############ (BRANCH 1) FAST MEM HAS ENOUGH SPACE" << endl;
      auto branch1_start_clock = std::chrono::steady_clock::now();

      auto branch1_begins = std::chrono::system_clock::now();
      std::time_t branch1_now_time_t1 =
        std::chrono::system_clock::to_time_t(branch1_begins);
      auto branch1_now_microsec1 =
        std::chrono::time_point_cast<std::chrono::microseconds>(
          branch1_begins);
      auto branch1_fractional_microsec1 =
        branch1_now_microsec1.time_since_epoch() % std::chrono::seconds(1);
      std::tm* branch1_now_tm1 = std::localtime(&branch1_now_time_t1);
      std::cout << "==========>> Before branch1 page migration: "
        << std::put_time(branch1_now_tm1, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0')
        << branch1_fractional_microsec1.count() << std::endl;
      do_migrate_pages_batch(hot_pages_, true, client_pid, cb, inactiveLFU,
        activeLFU);
      auto branch1_end_clock = std::chrono::steady_clock::now();

      auto branch1_ends = std::chrono::system_clock::now();
      std::time_t branch1_now_time_t2 =
        std::chrono::system_clock::to_time_t(branch1_ends);
      auto branch1_now_microsec2 =
        std::chrono::time_point_cast<std::chrono::microseconds>(branch1_ends);
      auto branch1_fractional_microsec2 =
        branch1_now_microsec2.time_since_epoch() % std::chrono::seconds(1);
      std::tm* branch1_now_tm2 = std::localtime(&branch1_now_time_t2);
      std::cout << "==========>> After branch1 page migration:  "
        << std::put_time(branch1_now_tm2, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0')
        << branch1_fractional_microsec2.count() << std::endl;
      auto get_branch1_time = chrono::duration_cast<chrono::microseconds>(
        branch1_end_clock - branch1_start_clock)
        .count();
      cout << "---------->> Branch1 time is:        " << get_branch1_time
        << " us." << endl;
    }
    else if (num_hot_pages > 0) {
      unsigned long page_demotion_num = 0;
      if (left_mem_quota_Kb > ctrl_free_mem_KB) {
        page_demotion_num =
          num_hot_pages - (left_mem_quota_Kb - ctrl_free_mem_KB) / page_size;
      }
      else {
        page_demotion_num =
          num_hot_pages + (ctrl_free_mem_KB - left_mem_quota_Kb) / page_size;
      }

      // unsigned long page_demotion_num = page_demotion_batch_size;
      // unsigned long page_demotion_batch_size = num_hot_pages -
      // max_migrated_pages; unsigned long page_demotion_batch_size =
      // 300000;

      // activeLFU->get_cold_pages(cold_pages_, left_num_demotion_pages);
      //  5.Migrate the left cold pages from the fast memory to slow
      //  memory
      // do_migrate_pages_batch(cold_pages_, false, client_pid, cb, inactiveLFU,
      // activeLFU); cout << "(FINISHED STEP 5). Migrate left cold pages
      // from the fast memory to slow memory (left_num_demotion_pages >
      // 0)" << endl;

      auto get_smaps_page_start_clock = std::chrono::steady_clock::now();
      cout << "############ (BRANCH 2) Fast MEM DOES NOT HAVE ENOUGH SPACE"
        << endl;
      // 1.Get cold pages from the smaps in fast memory first
      cout << "(FINISHED STEP 1). Get cold pages from the smaps in fast "
        "memory first"
        << endl;
      // if (smaps_pages_deque_exclude_cbf1.size() <= page_demotion_num &&
      //     smaps_pages_deque_exclude_cbf2.size() > 0) {
      //   is_second_queue = true;
      // }
      // if (smaps_pages_deque_exclude_cbf2.size() <= page_demotion_num &&
      //     smaps_pages_deque_exclude_cbf1.size() > 0) {
      //   is_second_queue = false;
      // }

      // if (is_second_queue) {
      //   cout << "############ [Movepage](QUEUE2) Smaps pages size:"
      //        << smaps_pages_deque_exclude_cbf2.size() << endl;
      //   get_smaps_cold_pages(smaps_pages_deque_exclude_cbf2,
      //   smaps_cold_pages_,
      //                        page_demotion_num);
      //   cout << "==========>> [Movepage] (QUEUE2) Size of fetched cold pages
      //   "
      //           "from smaps: "
      //        << smaps_cold_pages_.size() << endl;
      // }

      // if (!is_second_queue) {
      //   cout << "############ [Movepage](QUEUE1) Smaps pages size:"
      //        << smaps_pages_deque_exclude_cbf1.size() << endl;
      //   get_smaps_cold_pages(smaps_pages_deque_exclude_cbf1,
      //   smaps_cold_pages_,
      //                        page_demotion_num);
      //   cout << "==========>> [Movepage] (QUEUE1) Size of fetched cold pages
      //   "
      //           "from smaps: "
      //        << smaps_cold_pages_.size() << endl;
      // }
      // auto get_smaps_page_end_clock = std::chrono::steady_clock::now();
      // auto get_smaps_page_time =
      //     chrono::duration_cast<chrono::microseconds>(
      //         get_smaps_page_end_clock - get_smaps_page_start_clock)
      //         .count();
      // cout << "---------->> Get a batch of cold pages from sample, the "
      //         "time is: "
      //      << get_smaps_page_time << " us." << endl;

      // // 2.Migrate cold hidden pages in smaps from fast memory to slow
      // // memory
      // auto demote_smaps_page_start_clock = std::chrono::steady_clock::now();

      // auto demotion_begins = std::chrono::system_clock::now();
      // std::time_t now_time_t1 =
      //     std::chrono::system_clock::to_time_t(demotion_begins);
      // auto now_microsec1 =
      //     std::chrono::time_point_cast<std::chrono::microseconds>(
      //         demotion_begins);
      // auto fractional_microsec1 =
      //     now_microsec1.time_since_epoch() % std::chrono::seconds(1);
      // std::tm* now_tm1 = std::localtime(&now_time_t1);
      // std::cout << "==========>> Before page demotion from smaps timestamp: "
      //           << std::put_time(now_tm1, "%Y-%m-%d %H:%M:%S") << '.'
      //           << std::setw(6) << std::setfill('0')
      //           << fractional_microsec1.count() << std::endl;

      // do_migrate_pages_batch(smaps_cold_pages_, false, client_pid, cb,
      //                        inactiveLFU, activeLFU);
      // auto demote_smaps_page_end_clock = std::chrono::steady_clock::now();

      // auto demotion_ends = std::chrono::system_clock::now();
      // std::time_t now_time_t2 =
      //     std::chrono::system_clock::to_time_t(demotion_ends);
      // auto now_microsec2 =
      //     std::chrono::time_point_cast<std::chrono::microseconds>(
      //         demotion_ends);
      // auto fractional_microsec2 =
      //     now_microsec2.time_since_epoch() % std::chrono::seconds(1);
      // std::tm* now_tm2 = std::localtime(&now_time_t2);
      // std::cout << "==========>> After page demotion from smaps timestamp:  "
      //           << std::put_time(now_tm2, "%Y-%m-%d %H:%M:%S") << '.'
      //           << std::setw(6) << std::setfill('0')
      //           << fractional_microsec2.count() << std::endl;
      // // auto epoch_duration_microsec =
      // //
      // std::chrono::duration_cast<std::chrono::microseconds>(demote_smaps_page_start_clock.time_since_epoch());
      // auto demote_smaps_page_time =
      //     chrono::duration_cast<chrono::microseconds>(
      //         demote_smaps_page_end_clock - demote_smaps_page_start_clock)
      //         .count();
      // cout << "(FINISHED STEP 2). Migrate cold hidden pages in smaps "
      //         "from fast memory to slow memory."
      //      << endl;
      // cout << "---------->> Page demotion time: " << demote_smaps_page_time
      //      << " us." << endl;
      // unsigned long left_num_demotion_pages = 0;
      // if (smaps_cold_pages_.size() < page_demotion_num) {
      //   left_num_demotion_pages = page_demotion_num -
      //   smaps_cold_pages_.size();
      // }
      // if (smaps_cold_pages_.size() > 0) {
      //   smaps_cold_pages_.clear();
      // }
      // display_vector(smaps_cold_pages_);
      //  3.Get number of left pages to be demoted from fast memory in
      //  activeLFU
      unsigned long left_num_demotion_pages = page_demotion_num;
      cout << "(FINISHED STEP 3). Get number of left pages to be demoted "
        "from fast memory in activeLFU: "
        << left_num_demotion_pages << endl;
      if (left_num_demotion_pages > 0) {
        // 4.Get left cold pages from the activeList in fast memory
        activeLFU->get_cold_pages(cold_pages_, left_num_demotion_pages,
          hot_page_threshold);
        cout << "(FINISHED STEP 4). Get left cold pages from activeList "
          "in fast memory (left_num_demotion_pages > 0)"
          << endl;
        // 5.Migrate the left cold pages from the fast memory to slow
        // memory
        do_migrate_pages_batch(cold_pages_, false, client_pid, cb, inactiveLFU,
          activeLFU);
        cout << "(FINISHED STEP 5). Migrate left cold pages from the "
          "fast memory to slow memory (left_num_demotion_pages > 0)"
          << "Size of fetched cold pages from LRU: " << cold_pages_.size()
          << endl;
      }
      // long long new_left_mem_quota_Kb =
      //   get_left_mem_quota_Kb();
      // long max_migrated_pages =
      //   floor((new_left_mem_quota_Kb - ctrl_free_mem_KB) / page_size);
      long max_migrated_pages = cold_pages_.size();
      if (max_migrated_pages > 0) {
        if (max_migrated_pages < hot_pages_.size()) {
          cout << "############ [Movepage] Max num pages migrate:  "
            << max_migrated_pages
            << " is less than hot_page_num: " << num_hot_pages << " is_hot: 1"
            << endl;
          hot_pages_.resize(max_migrated_pages);
        }

        // 6.Migrate hot pages from the slow memory to fast memory
        auto promotion_page_start_clock = std::chrono::steady_clock::now();

        auto promotion_begins = std::chrono::system_clock::now();
        std::time_t now_time_t3 =
          std::chrono::system_clock::to_time_t(promotion_begins);
        auto now_microsec3 =
          std::chrono::time_point_cast<std::chrono::microseconds>(
            promotion_begins);
        auto fractional_microsec3 =
          now_microsec3.time_since_epoch() % std::chrono::seconds(1);
        std::tm* now_tm3 = std::localtime(&now_time_t3);
        std::cout << "==========>> Before page promotion timestamp: "
          << std::put_time(now_tm3, "%Y-%m-%d %H:%M:%S") << '.'
          << std::setw(6) << std::setfill('0')
          << fractional_microsec3.count() << std::endl;

        do_migrate_pages_batch(hot_pages_, true, client_pid, cb, inactiveLFU,
          activeLFU);
        auto promotion_page_end_clock = std::chrono::steady_clock::now();

        auto promotion_ends = std::chrono::system_clock::now();
        std::time_t now_time_t4 =
          std::chrono::system_clock::to_time_t(promotion_ends);
        auto now_microsec4 =
          std::chrono::time_point_cast<std::chrono::microseconds>(
            promotion_ends);
        auto fractional_microsec4 =
          now_microsec4.time_since_epoch() % std::chrono::seconds(1);
        std::tm* now_tm4 = std::localtime(&now_time_t4);
        std::cout << "==========>> After page promotion timestamp:  "
          << std::put_time(now_tm4, "%Y-%m-%d %H:%M:%S") << '.'
          << std::setw(6) << std::setfill('0')
          << fractional_microsec4.count() << std::endl;

        auto promotion_page_time =
          chrono::duration_cast<chrono::microseconds>(
            promotion_page_end_clock - promotion_page_start_clock)
          .count();
        auto branch2_time =
          chrono::duration_cast<chrono::microseconds>(
            promotion_page_end_clock - get_smaps_page_start_clock)
          .count();
        cout << "(FINISHED STEP 6). Migrate hot pages from the slow memory "
          "to fast memory"
          << endl;
        cout << "---------->> The branch2 total time: "
          << branch2_time
          //  << " us. [ 1.page fetching: " << get_smaps_page_time
          //  << " us; 2.page demotion: " << demote_smaps_page_time
          << " us; 3.page promotion: " << promotion_page_time << " us. ]"
          << endl;
      }
    }

    auto t3_end_clock = std::chrono::steady_clock::now();
    auto t3_total_time = chrono::duration_cast<chrono::microseconds>(
      t3_end_clock - t3_start_clock)
      .count();
    cout << "---------->> Get hot pages from LFU: " << get_hot_page_time
      << " us." << endl;
    cout << "---------->> Thread t4 total time:   " << t3_total_time << " us."
      << endl;
    cout << endl;
    myMutex.unlock();
    };
  t.start(interval, func_with_params);
}

// void save_result_to_csv(uint64_t elapsedMicros,
//   pcm::SocketCounterState beforeState,
//   pcm::SocketCounterState afterState, ofstream& csv_file,
//   unsigned long mem_quota_Kb, long long selfInstructions,
//   long long selfCycles, double processCpuTimeUsed,
//   double totalCpuTime, double cpu_usage) {
//   double elapsedSeconds = elapsedMicros / 1000000.0;
//   pcm::uint64 bytesWrittenToPMM =
//     pcm::getBytesWrittenToPMM(beforeState, afterState);
//   pcm::uint64 bytesReadFromPMM =
//     pcm::getBytesReadFromPMM(beforeState, afterState);
//   pcm::uint64 bytesReadFromMC =
//     pcm::getBytesReadFromMC(beforeState, afterState);
//   pcm::uint64 bytesWrittenToMC =
//     pcm::getBytesWrittenToMC(beforeState, afterState);
//   // pcm::uint64 instructions =
//   //     pcm::getInstructionsRetired(beforeState, afterState);
//   // pcm::uint64 cycles = pcm::getCycles(beforeState, afterState);
//   // double ipc = pcm::getIPC(beforeState, afterState);
//   double ipc = (double)selfInstructions / selfCycles;
//   double ipus = (double)selfInstructions / elapsedMicros;
//   double writtenToPmmMBPerSec = (double)bytesWrittenToPMM / elapsedMicros;
//   double readFromPmmMBPerSec = (double)bytesReadFromPMM / elapsedMicros;
//   double writtenToDramMCMBPerSec = (double)bytesWrittenToMC / elapsedMicros;
//   double readFromDramMCMBPerSec = (double)bytesReadFromMC / elapsedMicros;
//   pcm::uint64 l3CacheMisses = pcm::getL3CacheMisses(beforeState, afterState);
//   pcm::uint64 l2CacheMisses = pcm::getL2CacheMisses(beforeState, afterState);
//   double l3CacheHitRatio = pcm::getL3CacheHitRatio(beforeState, afterState);
//   double l2CacheHitRatio = pcm::getL2CacheHitRatio(beforeState, afterState);
//   double llcReadMissLatency =
//     pcm::getLLCReadMissLatency(beforeState, afterState);
//   double l3CacheMissesPerUS = (double)l3CacheMisses / elapsedMicros;
//   double l2CacheMissesPerUS = (double)l2CacheMisses / elapsedMicros;

//   // write to csv file
//   csv_file << Parameters_get_warmup_seconds(&gParams) << ","
//     << mem_quota_Kb / 1024.0 / 1024.0 << ","
//     << Parameters_get_sample_frequency(&gParams) << ","
//     << Parameters_get_read_sample_period(&gParams) << ","
//     << Parameters_get_store_sample_period(&gParams) << ","
//     << Parameters_get_profiling_interval(&gParams) << ","
//     << Parameters_get_proc_scan_interval(&gParams) << ","
//     << Parameters_get_page_fetch_interval(&gParams) << ","
//     << Parameters_get_hot_page_threshold(&gParams) << ","
//     << Parameters_get_page_migration_interval(&gParams) << ","
//     << elapsedSeconds << "," << selfInstructions << "," << selfCycles
//     << "," << ipc << "," << ipus << "," << writtenToPmmMBPerSec << ","
//     << readFromPmmMBPerSec << "," << writtenToDramMCMBPerSec << ","
//     << readFromDramMCMBPerSec << "," << l3CacheMisses << ","
//     << l2CacheMisses << "," << l3CacheMissesPerUS << ","
//     << l2CacheMissesPerUS << "," << l3CacheHitRatio << ","
//     << l2CacheHitRatio << "," << llcReadMissLatency << ","
//     << processCpuTimeUsed << "," << totalCpuTime << "," << cpu_usage
//     << endl;
// }

// start perf event for period collection
long setup_perf_event_for_period_collection(int pid, long long config) {
  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(struct perf_event_attr));

  // int ret = pfm_get_perf_event_encoding(eventName,
  //                                   PFM_PLMH, &pe, NULL, NULL);

  // if (ret != PFM_SUCCESS) {
  //   std::string errStr;
  //   errStr.append("cannot get encoding for event ");
  //   errStr.append(eventName);
  //   errStr.append(pfm_strerror(ret));
  //   err(errStr.c_str());
  // } else {
  //   printf("success event 1\n");
  // }

  // pe.type = PERF_TYPE_RAW;
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = config;
  // pe.sample_type =
  //     PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_WEIGHT |
  //     PERF_SAMPLE_ADDR;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;
  // pe.exclude_callchain_kernel = 1;
  // pe.exclude_callchain_user = 1;
  // pe.precise_ip = 1;
  long fd = perf_event_open(&pe, pid, -1, -1, 0);
  if (fd == -1) {
    perror("perf_event_open");
    exit(EXIT_FAILURE);
  }
  return fd;
}

void update_perf_sample_period(uint64_t sample_period) {
  pebs_update_period(sample_period);
}