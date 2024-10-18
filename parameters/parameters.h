#ifndef PARAMETERS_PARAMETERS_H
#define PARAMETERS_PARAMETERS_H

#ifdef __cplusplus
extern "C" {
#endif



typedef struct {
  int buffer_size;
  int sample_frequency;
  int profiling_interval;
  int pebs_nprocs;
  int scanning_thread_cpu;
  int pages_count;
  int hot_page_threshold;
  int reset_interval;
  int ml_input;
  int ml_prediction;
  int page_migration_interval;
  int page_fetch_interval;
  int proc_scan_interval;
  unsigned long mem_quota_Kb;
  int read_sample_period;
  int store_sample_period;
  int collect_period;
  int warmup_seconds;
} Parameters;

void Parameters_init(Parameters* self, unsigned long mem_quota_Kb,
                     int sample_freq, int profiling_interval,
                     int page_fetch_interval, int hot_page_threshold,
                     int pebs_nprocs, int proc_scan_interval,
                     int page_migration_interval, int read_sample_period, int store_sample_period,
                     int warmup_seconds);

void Parameters_generate_random(Parameters* self, int pages_count,
                                int pebs_nprocs, int collect_period,
                                int warmup_seconds, int mem_quota_KB);

void Parameters_set_store_sample_period(Parameters* params, int store_sample_period);

int Parameters_get_store_sample_period(Parameters* params);


void Parameters_set_warmup_seconds(Parameters* params, int warmup_seconds);

int Parameters_get_warmup_seconds(Parameters* params);

void Parameters_set_buffer_size(Parameters* params, int count);

int Parameters_get_buffer_size(Parameters* params);

void Parameters_set_sample_frequency(Parameters* params, int sample_freq);

int Parameters_get_sample_frequency(Parameters* params);

void Parameters_set_read_sample_period(Parameters* params, int sample_period);

int Parameters_get_read_sample_period(Parameters* params);

void Parameters_set_page_fetch_interval(Parameters* params,
                                        int page_fetch_interval);

int Parameters_get_page_fetch_interval(Parameters* params);

void Parameters_set_profiling_interval(Parameters* params,
                                       int profiling_interval);

int Parameters_get_profiling_interval(Parameters* params);

void Parameters_set_pebs_nprocs(Parameters* params, int pebs_nprocs);

int Parameters_get_pebs_nprocs(Parameters* params);

void Parameters_set_scanning_thread_cpu(Parameters* params, int pebs_nprocs);

int Parameters_get_scanning_thread_cpu(Parameters* params);

void Parameters_set_hot_page_threshold(Parameters* params,
                                       int hot_page_threshold);

int Parameters_get_hot_page_threshold(Parameters* params);

void Parameters_set_reset_interval(Parameters* params, int profiling_interval);

int Parameters_get_reset_interval(Parameters* params);


void Parameters_set_page_migration_interval(Parameters* params,
                                            int profiling_interval);

int Parameters_get_page_migration_interval(Parameters* params);

int Parameters_get_proc_scan_interval(Parameters* params);

void Parameters_set_proc_scan_interval(Parameters* params,
                                       int proc_scan_interval);

void Parameters_set_mem_quota_KB(Parameters* params,
                                 unsigned long mem_quota_Kb);

int Parameters_get_mem_quota_KB(Parameters* params);

void Parameters_set_collect_period(Parameters* params, int collect_period);

int Parameters_get_collect_period(Parameters* params);

#ifdef __cplusplus
}
#endif

#endif  // PARAMETERS_PARAMETERS_H
