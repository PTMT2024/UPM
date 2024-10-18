#include "parameters.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

Parameters gParams;
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void Parameters_init(Parameters* self, unsigned long mem_quota_Kb,
  int sample_freq, int profiling_interval,
  int page_fetch_interval, int hot_page_threshold,
  int pebs_nprocs, int proc_scan_interval,
  int page_migration_interval, int read_sample_period,
  int store_sample_period,
  int warmup_seconds) {
  pthread_rwlock_wrlock(&rwlock);
  self->mem_quota_Kb = mem_quota_Kb;
  self->sample_frequency = sample_freq;
  self->profiling_interval = profiling_interval;
  self->page_fetch_interval = page_fetch_interval;
  self->hot_page_threshold = hot_page_threshold;
  self->proc_scan_interval = proc_scan_interval;
  self->page_migration_interval = page_migration_interval;
  self->read_sample_period = read_sample_period;
  self->store_sample_period = store_sample_period;
  self->pebs_nprocs = pebs_nprocs;
  self->warmup_seconds = warmup_seconds;
  pthread_rwlock_unlock(&rwlock);
}


void Parameters_set_warmup_seconds(Parameters* self, int warmup_seconds) {
  pthread_rwlock_wrlock(&rwlock);
  self->warmup_seconds = warmup_seconds;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_warmup_seconds(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int warmup_seconds = self->warmup_seconds;
  pthread_rwlock_unlock(&rwlock);
  return warmup_seconds;
}

void Parameters_set_collect_period(Parameters* self, int collect_period) {
  pthread_rwlock_wrlock(&rwlock);
  self->collect_period = collect_period;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_collect_period(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int collect_period = self->collect_period;
  pthread_rwlock_unlock(&rwlock);
  return collect_period;
}

void Parameters_set_buffer_size(Parameters* self, int count) {
  pthread_rwlock_wrlock(&rwlock);
  self->buffer_size = (1 + (1 << count));
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_buffer_size(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int buffer_size = self->buffer_size;
  pthread_rwlock_unlock(&rwlock);
  return buffer_size;
}

void Parameters_set_sample_frequency(Parameters* self, int sample_freq) {
  pthread_rwlock_wrlock(&rwlock);
  self->sample_frequency = sample_freq;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_sample_frequency(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int sample_frequency = self->sample_frequency;
  pthread_rwlock_unlock(&rwlock);
  return sample_frequency;
}

void Parameters_set_read_sample_period(Parameters* self,
  int read_sample_period) {
  pthread_rwlock_wrlock(&rwlock);
  self->read_sample_period = read_sample_period;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_read_sample_period(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int read_sample_period = self->read_sample_period;
  pthread_rwlock_unlock(&rwlock);
  return read_sample_period;
}

void Parameters_set_store_sample_period(Parameters* self,
  int store_sample_period) {
  pthread_rwlock_wrlock(&rwlock);
  self->store_sample_period = store_sample_period;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_store_sample_period(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int store_sample_period = self->store_sample_period;
  pthread_rwlock_unlock(&rwlock);
  return store_sample_period;

}

void Parameters_set_page_fetch_interval(Parameters* self,
  int page_fetch_interval) {
  pthread_rwlock_wrlock(&rwlock);
  self->page_fetch_interval = page_fetch_interval;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_page_fetch_interval(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int page_fetch_interval = self->page_fetch_interval;
  pthread_rwlock_unlock(&rwlock);
  return page_fetch_interval;
}

void Parameters_set_profiling_interval(Parameters* self,
  int profiling_interval) {
  pthread_rwlock_wrlock(&rwlock);
  self->profiling_interval = profiling_interval;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_profiling_interval(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int profiling_interval = self->profiling_interval;
  pthread_rwlock_unlock(&rwlock);
  return profiling_interval;
}

void Parameters_set_pebs_nprocs(Parameters* self, int pebs_nprocs) {
  pthread_rwlock_wrlock(&rwlock);
  self->pebs_nprocs = pebs_nprocs;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_pebs_nprocs(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int pebs_nprocs = self->pebs_nprocs;
  pthread_rwlock_unlock(&rwlock);
  return pebs_nprocs;
}

void Parameters_set_scanning_thread_cpu(Parameters* self, int pebs_nprocs) {
  pthread_rwlock_wrlock(&rwlock);
  self->scanning_thread_cpu = pebs_nprocs - 1;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_scanning_thread_cpu(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int scanning_thread_cpu = self->scanning_thread_cpu;
  pthread_rwlock_unlock(&rwlock);
  return scanning_thread_cpu;
}

void Parameters_set_hot_page_threshold(Parameters* self,
  int hot_page_threshold) {
  pthread_rwlock_wrlock(&rwlock);
  self->hot_page_threshold = hot_page_threshold;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_hot_page_threshold(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int hot_page_threshold = self->hot_page_threshold;
  pthread_rwlock_unlock(&rwlock);
  return hot_page_threshold;
}

void Parameters_set_reset_interval(Parameters* self, int profiling_interval) {
  pthread_rwlock_wrlock(&rwlock);
  self->reset_interval = 2 * profiling_interval;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_reset_interval(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int reset_interval = self->reset_interval;
  pthread_rwlock_unlock(&rwlock);
  return reset_interval;
}

void Parameters_set_page_migration_interval(Parameters* self,
  int page_migration_interval) {
  pthread_rwlock_wrlock(&rwlock);
  self->page_migration_interval = page_migration_interval;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_page_migration_interval(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int page_migration_interval = self->page_migration_interval;
  pthread_rwlock_unlock(&rwlock);
  return page_migration_interval;
}

int Parameters_get_proc_scan_interval(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int proc_scan_interval = self->proc_scan_interval;
  pthread_rwlock_unlock(&rwlock);
  return proc_scan_interval;
}

void Parameters_set_proc_scan_interval(Parameters* self,
  int proc_scan_interval) {
  pthread_rwlock_wrlock(&rwlock);
  self->proc_scan_interval = proc_scan_interval;
  pthread_rwlock_unlock(&rwlock);
}

void Parameters_set_mem_quota_KB(Parameters* self,
  unsigned long mem_quota_Kb) {
  pthread_rwlock_wrlock(&rwlock);
  self->mem_quota_Kb = mem_quota_Kb;
  pthread_rwlock_unlock(&rwlock);
}

int Parameters_get_mem_quota_KB(Parameters* self) {
  pthread_rwlock_rdlock(&rwlock);
  int mem_quota_Kb = self->mem_quota_Kb;
  pthread_rwlock_unlock(&rwlock);
  return mem_quota_Kb;
}

void Parameters_generate_random(Parameters* self, int pages_count,
  int pebs_nprocs, int collect_period,
  int warmup_seconds, int mem_quota_KB) {
  int sample_rate = 0;
  int sample_period = 0;

  srand(time(NULL));
  // sample_rate = (rand() % 10 + 1) * 100;                  // (100 - 1000,
  // step 100) read_sample_period = (rand() % 19 + 2) * 2000;  // (4000 - 40000,
  // step 2000)
  int read_sample_period = 1000;
  int store_sample_period = 1000;
  int profiling_interval = (rand() % 10 + 1) * 1;   // (1s -> 10s , step 1s)
  int proc_scan_interval = (rand() % 6 + 1) * 10;   // (10s -> 60s, step 10s)
  int page_fetch_interval = (rand() % 10 + 1) * 1;  // (1s - 10s, step 1s)
  // int hotpage_threshold = rand() % 1 + 1;         // (1,2,3)
  int hotpage_threshold = 1;
  int page_migration_interval = (rand() % 5 + 1) * 4;  // (4s - 20s, step 4s)

  // // sample_rate = 200;                  // (200 - 1000, step 100)
  // // read_sample_period = (rand() % 10 + 1) * 10000;  // (10000 - 100000,
  // step 10000) read_sample_period = 50000; int profiling_interval = 1; // (1s
  // -> 10s , step 1s) int proc_scan_interval = 30;        // (30s -> 90s, step
  // 30s) int page_fetch_interval = 1;      // (1s - 30s, step 1s) int
  // hotpage_threshold = 1;         // (1,2,3,4) int page_migration_interval =
  // 5;  // (5s - 60s, step 5s)

  Parameters_init(&gParams, mem_quota_KB, sample_rate, profiling_interval,
    page_fetch_interval, hotpage_threshold,
    pebs_nprocs, proc_scan_interval, page_migration_interval,
    read_sample_period, store_sample_period,
    warmup_seconds);
}
