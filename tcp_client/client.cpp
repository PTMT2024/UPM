#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include "../common.h"

using namespace std;

char* parse_args(int argc, char* argv[], char* args) {
    // Check if there are enough arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <enable|update|disable> [-d read_sample_period] [-h hot_threshold] [-m page_migration_interval] [-p workload_pid]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // The second argument should be the command
    char* command = argv[1];

    // Define the options string for getopt
    const char* options = "d:h:m:p:";

    // Variables to store the option values
    char* d_arg = NULL;
    char* h_arg = NULL;
    char* m_arg = NULL;
    char* p_arg = NULL;

    // Parse the options
    int opt;
    // optind = 2;  // start from the third argument
    while ((opt = getopt(argc, argv, options)) != -1) {
        switch (opt) {
        case 'd':
            d_arg = optarg;
            break;
        case 'h':
            h_arg = optarg;
            break;
        case 'm':
            m_arg = optarg;
            break;
        case 'p':
            p_arg = optarg;
            break;           
        default:
            fprintf(stderr, "Usage: %s <enable|update|disable> [-d read_sample_period] [-h hot_threshold] [-m page_migration_interval] [-p workload_pid]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Start with the command in the args string
    sprintf(args, "%s", command);

    // If d_arg is not NULL, append -d and d_arg to the args string
    if (d_arg != NULL) {
        sprintf(args + strlen(args), " -d %s", d_arg);
    }

    // If h_arg is not NULL, append -h and h_arg to the args string
    if (h_arg != NULL) {
        sprintf(args + strlen(args), " -h %s", h_arg);
    }

    // If m_arg is not NULL, append -m and m_arg to the args string
    if (m_arg != NULL) {
        sprintf(args + strlen(args), " -m %s", m_arg);
    }

    // If p_arg is not NULL, append -p and p_arg to the args string
    if (p_arg != NULL) {
        sprintf(args + strlen(args), " -p %s", p_arg);
    }
    return args;
}

int main(int argc, char* argv[]) {
    char args[1024];

    parse_args(argc, argv, args);
    printf("get args: %s\n", args);

    struct sockaddr_in s_addr;
    int len = sizeof(struct sockaddr_in);
    int client_fd;

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("socket error");
        return 0;
    }

    s_addr.sin_family = AF_INET;

    s_addr.sin_port = htons(10086);

    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int connect_ret = connect(client_fd, (struct sockaddr*)&s_addr, len);
    printf("\n connect_ret %d\n", connect_ret);
    if (connect_ret < 0) {
        perror("connect error");
        close(client_fd);
        return 0;
    }

    ssize_t num_bytes_sent = send(client_fd, args, strlen(args), 0);
    if (num_bytes_sent < 0) {
        perror("send error");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // //seconds
    // int nNetTimeout = 2  * 1000;
    // //set timeout for recv
    // setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &nNetTimeout, sizeof(int));
    // while (1) {
    //     bzero(buffer, sizeof(buffer));
    //     if ((num = recv(client_fd, buffer, sizeof(buffer), 0)) == -1) {
    //         perror("recv");
    //         exit(1);
    //     } else if (num > 0) {
    //         int len, bytes_sent;
    //         buffer[num] = '\0';
    //         printf("recv: %s\n", buffer);
    //     } else {
    //         //num=0 -> connection lost
    //         printf("socket end!\n");
    //         break;
    //     }

    // }

    close(client_fd);
    return 0;
}
