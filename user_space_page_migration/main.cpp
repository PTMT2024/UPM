#include "../common.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include <signal.h>
#include <cstring>
#include <iostream>

#include "cmdline.h"
#include "samples/samples.h"

extern "C" {
#include "parameters.h"
}


using namespace std;
extern Parameters gParams;
int socket_fd;

const char* PID_PIPEPATH = "/tmp/user_space_page_migration_fifo";

void parse_client_msg(char* buffer, std::map<std::string, std::string>& args) {
  char* token = strtok(buffer, " ");

  // The first token is the command
  std::string command = token;
  args["command"] = command;
  token = strtok(NULL, " ");
  while (token != NULL) {
    if (token[0] == '-') {
      // This token is a key. The next token is its value.
      std::string key = token;
      token = strtok(NULL, " ");
      if (token != NULL) {
        std::string value = token;
        args[key] = value;
      }
    }
    token = strtok(NULL, " ");
  }

  cout << "Parsed client message: " << endl;
  for (const auto& arg : args) {
    std::cout << arg.first << ": " << arg.second << std::endl;
  }
}

void listen_client_request(char* buffer, size_t bufferSize) {
  struct sockaddr_in c_addr;
  socklen_t client_len = sizeof(struct sockaddr_in);
  // Accept a client connection
  int accept_fd = accept(socket_fd, (struct sockaddr*)&c_addr, &client_len);
  if (accept_fd < 0) {
    perror("accept error");
    exit(EXIT_FAILURE);
  }

  // Receive the data
  ssize_t num_bytes_received = recv(accept_fd, buffer, bufferSize, 0);
  if (num_bytes_received < 0) {
    perror("recv error");
    exit(EXIT_FAILURE);
  }

  // Null-terminate the received data
  buffer[num_bytes_received] = '\0';
}

void create_socket() {
  struct sockaddr_in s_addr;
  int len = sizeof(struct sockaddr_in);
  socklen_t client_len = sizeof(struct sockaddr_in);

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    perror("socket error");
    exit(EXIT_FAILURE);
  }

  // Add it to skip TCP TIME_WAIT status
  // Set SO_LINGER option on socket
  struct linger linger_opt = { 1, 0 };  // linger active, timeout 0
  setsockopt(socket_fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

  // AF_INET -> IPv4 address, AF_INET6 -> IPv6 address
  s_addr.sin_family = AF_INET;
  // set port
  s_addr.sin_port = htons(10086);
  // INADDR_ANY -> generally means local machine
  s_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_fd, (struct sockaddr*)&s_addr, len) < 0) {
    perror("bind error");
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(socket_fd, 10) < 0) {
    perror("listen error");
    close(socket_fd);
    exit(EXIT_FAILURE);
  }
}

void exec_shell_cmd(char* cmd)
{
  FILE* fp;
  char bp[500];
  fp = popen(cmd, "r");
  while (fgets(bp, sizeof(bp), fp) != NULL) {
    printf("%s", bp);
  }
  pclose(fp);
}

// Signal handler for SIGTERM
void sigterm_handler(int signum)
{
    close(socket_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
  signal(SIGTERM, sigterm_handler);
  pid_t pid = getpid();
  printf("The current process ID is %d\n", pid);

  char cmd[256];
  sprintf(cmd, "choom --adjust -1000 -p %d", (int)pid);
  exec_shell_cmd(cmd);

  create_socket();

  long long free_fast_mem_byte, initial_used_fast_mem_byte;
  unsigned long free_fast_mem_Mb, total_fast_mem_Mb, initial_used_fast_mem_Mb;
  unsigned long free_fast_mem_Gb, total_fast_mem_Gb, initial_used_fast_mem_Gb;
  unsigned long long total_fast_mem_byte =
    numa_node_size64(FAST_NUMA_NODE, &free_fast_mem_byte);

  if (total_fast_mem_byte < 0) {
    perror(
      "Function numa_node_size64 error, can not get total fast memmory size");
    assert(total_fast_mem_byte >= 0);
  }

  free_fast_mem_Mb = floor(free_fast_mem_byte / 1024 / 1024);
  total_fast_mem_Mb = floor(total_fast_mem_byte / 1024 / 1024);
  initial_used_fast_mem_Mb = total_fast_mem_Mb - free_fast_mem_Mb;
  free_fast_mem_Gb = floor(free_fast_mem_byte / 1024 / 1024 / 1024);
  total_fast_mem_Gb = floor(total_fast_mem_byte / 1024 / 1024 / 1024);
  initial_used_fast_mem_byte = total_fast_mem_byte - free_fast_mem_byte;
  initial_used_fast_mem_Gb = total_fast_mem_Gb - free_fast_mem_Gb;
  unsigned long initial_used_fast_mem_Kb =
    ceil(initial_used_fast_mem_byte / 1024);

  cout << "Free fast memory size: " << free_fast_mem_byte
    << " Byte = " << free_fast_mem_Mb << " Mb = " << free_fast_mem_Gb
    << " Gb" << endl;
  cout << "Total fast memory size: " << total_fast_mem_byte
    << " Byte = " << total_fast_mem_Mb << " Mb = " << total_fast_mem_Gb
    << " Gb" << endl;

  cout << "Initial used memory before running is: " << initial_used_fast_mem_Gb
    << " Gb "
    << "= " << initial_used_fast_mem_Kb << " Kb" << endl;
  cmdline::parser cmd_parser;
  cmd_parser.add<int>("type", 't',
    "page replace type,{lfu:1; lru:2; ml:3; mix:4}", false, 1,
    cmdline::oneof<int>(1, 2, 3, 4));
  cmd_parser.add<unsigned long>("mem_quota", 'q',
    "the user input fast memory quota", false, 0,
    cmdline::range(0, 102400));
  cmd_parser.add<int>("target_pid", 'p', "target workload pid", false, 0,
    cmdline::range(0, 10000000));
  cmd_parser.add<int>("warmup_seconds", 'w', "warmup seconds", false, 0,
    cmdline::range(0, 1200));
  cmd_parser.add<int>("sample_freq", 'r', "pebs sample rate (samples/second)",
    false, 0, cmdline::range(0, 10000000));
  cmd_parser.add<int>("read_sample_period", 'd', "pebs sample period", false,
    1000, cmdline::range(0, 100000));
  cmd_parser.add<int>("store_sample_period", 'S', "pebs sample period", false,
    1000, cmdline::range(0, 10000000));
  cmd_parser.add<int>("profiling_interval", 'i', "pebs profiling interval",
    false, 1, cmdline::range(1, 100000));
  cmd_parser.add<int>("proc_scan_interval", 's',
    "proc/[pid]/smaps scan interval", false, 1,
    cmdline::range(0, 100000));
  cmd_parser.add<int>("page_fetch_interval", 'f',
    "interval for fetching pages from ring buffer", false, 1,
    cmdline::range(1, 100000));
  cmd_parser.add<int>("threshold", 'h', "threshold for hot pages", false, 2,
    cmdline::range(2, 10));
  cmd_parser.add<int>("only_sampling", 'o', "only sampling", false, 0,
    cmdline::range(0, 1));
  cmd_parser.add<int>("page_migration_interval", 'm',
    "interval for page migration", false, 1,
    cmdline::range(1, 100000));
  cmd_parser.add<int>("nprocs", 'n', "number of processors for pebs", false, 24,
    cmdline::range(1, 96));

  cmd_parser.add<string>("result_file", 'F', "result_file", false,
    "result.csv");

  cmd_parser.parse_check(argc, argv);

  int page_replace_type = cmd_parser.get<int>("type");
  unsigned long mem_quota = cmd_parser.get<unsigned long>("mem_quota");
  int sample_freq = cmd_parser.get<int>("sample_freq");
  int read_sample_period = cmd_parser.get<int>("read_sample_period");
  int store_sample_period = cmd_parser.get<int>("store_sample_period");
  int profiling_interval = cmd_parser.get<int>("profiling_interval");
  int proc_scan_interval = cmd_parser.get<int>("proc_scan_interval");
  int page_fetch_interval = cmd_parser.get<int>("page_fetch_interval");
  int hot_page_threshold = cmd_parser.get<int>("threshold");
  int page_migration_interval = cmd_parser.get<int>("page_migration_interval");
  int pebs_nprocs = cmd_parser.get<int>("nprocs");
  int warmup_seconds = cmd_parser.get<int>("warmup_seconds");
  string result_file = cmd_parser.get<string>("result_file");
  unsigned long mem_quota_Kb = mem_quota * 1024;
  // pid_t client_pid = cmd_parser.get<int>("target_pid");
  int only_sampling = cmd_parser.get<int>("only_sampling");

  bool is_tuning = false;
  std::thread t([&]() {
    while (true) {
      char msg[1024];
      // it will block
      listen_client_request(msg, sizeof(msg) - 1);
      cout << "Received client message: " << msg << endl;
      if (msg[0] != '\0') {
        std::map<std::string, std::string> args;
        parse_client_msg(msg, args);
        if (auto it = args.find("-d"); it != args.end()) {
          read_sample_period = std::stoi(it->second);
        }
        if (auto it = args.find("-h"); it != args.end()) {
          hot_page_threshold = std::stoi(it->second);
        }
        if (auto it = args.find("-m"); it != args.end()) {
          page_migration_interval = std::stoi(it->second);
        }
        if (args["command"] == "enable") {
          if (is_tuning) {
            cout << "Tuning is already enabled" << endl;
            continue;
          }
          cout << "Enable tuning" << endl;
          is_tuning = true;
          pid_t client_pid = 0;
          if (auto it = args.find("-p"); it != args.end()) {
            client_pid = std::stoi(it->second);
          } else {
            cerr << "No target pid specified" << endl;
            exit(EXIT_FAILURE); 
          }
          Parameters_init(&gParams, mem_quota_Kb, sample_freq, profiling_interval,
            page_fetch_interval, hot_page_threshold,
            pebs_nprocs, proc_scan_interval, page_migration_interval,
            read_sample_period, store_sample_period,
            warmup_seconds);

          std::thread t([&]() {
            start_tuning(warmup_seconds, client_pid, page_replace_type, only_sampling);
            });
          t.detach();
        }
        else if (args["command"] == "update") {
          cout << "Update tuning parameters hot_page_threshold: " << hot_page_threshold
            << " page_migration_interval: " << page_migration_interval << endl;
          // update_perf_sample_period(read_sample_period);
          Parameters_init(&gParams, mem_quota_Kb, sample_freq, profiling_interval,
            page_fetch_interval, hot_page_threshold,
            pebs_nprocs, proc_scan_interval, page_migration_interval,
            read_sample_period, store_sample_period,
            warmup_seconds);
        }
        else if (args["command"] == "disable") {
          cout << "Disable tuning" << endl;
          disable_tuning();
          break;
        }
      }
    }
    });
  t.detach();

  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (should_terminate()) {
      break;
    }
  }

  close(socket_fd);
  return EXIT_SUCCESS;
}
// exit(EXIT_SUCCESS);
// } else if (pid > 0) {
//   close(accept_fd);
//   waitpid(pid, nullptr, 0);  // wait child process to finish
// }
// }
