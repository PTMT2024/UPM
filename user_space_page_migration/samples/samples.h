#ifndef SAMPLES_SAMPLES_H
#define SAMPLES_SAMPLES_H

#include <arpa/inet.h>
#include <assert.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <numa.h>
#include <numaif.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../common.h"
#include "LFU.h"
#include "algorithm.h"
// #include "cpucounters.h"
#include "parameters.h"
#include "timer.hpp"
extern "C" {
#include "../counting_bloom/counting_bloom.h"
#include "../perf_profile/lib/debug.h"
#include "../perf_profile/profile.h"
}

void send_info_to_tcp_client(pid_t client_pid);
void tuning_logic(std::ofstream& csv_file, pid_t client_pid, int page_replace_type, int only_sampling);

void run_logic(std::ofstream& csv_file, pid_t client_pid, int only_sampling);

void start_collecting_sliced_period(int warmup_seconds, pid_t client_pid,
                                    int accept_fd, int page_replace_type,
                                    unsigned long initial_used_fast_mem_Kb,
                                    int pages_count, int pebs_nprocs,
                                    int collect_period,
                                    std::string result_file, int mem_quota_KB);
long setup_perf_event_for_period_collection(int pid, long long config);
void start_tuning(int warmup_seconds, pid_t client_pid, int page_replace_type, int only_sampling);
void disable_tuning();
bool should_terminate();
void check_workload_is_still_runing(pid_t client_pid);
void fetch_page_timer(Timer& t, int interval, pid_t client_pid,
                      CountingBloom& cb);
void fetch_page (pid_t client_pid, CountingBloom& cb);
void scan_proc (pid_t client_pid, CountingBloom& cb);
void scan_proc_timer(Timer& t, int interval, pid_t client_pid, CountingBloom& cb);
void migrate_page(pid_t client_pid,
                        CountingBloom& cb);
void migrate_page_timer(Timer& t, int interval, pid_t client_pid,
                        unsigned long mem_quota_Kb,
                        unsigned long initial_used_fast_mem_Kb,
                        int hot_page_threshold, CountingBloom& cb);
void send_info_to_tcp_client_timer(Timer& t, int interval, pid_t client_pid,
                                   int accept_fd, Timer& t1, Timer& t2,
                                   Timer& t3);
// void save_result_to_csv(uint64_t elapsedMicros,
//                         pcm::SocketCounterState beforeState,
//                         pcm::SocketCounterState afterState,
//                         std::ofstream& csv_file, unsigned long mem_quota_Kb,
//                         long long countInstructions, long long countCycles,
//                         double processCpuTimeUsed, double totalCpuTime,
//                         double cpu_usage);
unsigned long long get_process_cpu_time(pid_t pid);
unsigned long long get_total_cpu_time();
void update_perf_sample_period(uint64_t sample_period);
#endif  // SAMPLES_SAMPLES_H
