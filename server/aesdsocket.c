#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PORT 9000
#define BACKLOG 10
#define DATA_FILE "/var/tmp/aesdsocketdata"

int sockfd = -1;
int client_fd = -1;

void cleanup() {
    if (client_fd != -1) close(client_fd);
    if (sockfd != -1) close(sockfd);
    remove(DATA_FILE);
    syslog(LOG_INFO, "Caught signal, exiting");
    closelog();
    exit(0);
}

void signal_handler(int sig) {
    cleanup();
}

void run_server(int daemon_mode) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        cleanup();
    }

    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        cleanup();
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

        if (setsid() < 0) exit(EXIT_FAILURE);

        // Redirect stdio
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    while (1) {
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        FILE *fp = fopen(DATA_FILE, "a+");
        if (!fp) {
            syslog(LOG_ERR, "fopen() failed: %s", strerror(errno));
            close(client_fd);
            continue;
        }

        char buffer[1024];
        ssize_t bytes_read;
        size_t total = 0;
        char *packet = NULL;

        while ((bytes_read = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
            packet = realloc(packet, total + bytes_read + 1);
            memcpy(packet + total, buffer, bytes_read);
            total += bytes_read;
            packet[total] = '\0';

            if (strchr(packet, '\n')) break;
        }

        if (packet) {
            fwrite(packet, 1, total, fp);
            fflush(fp);
            fseek(fp, 0, SEEK_SET);

            while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                send(client_fd, buffer, bytes_read, 0);
            }

            free(packet);
        }

        fclose(fp);
        close(client_fd);
        client_fd = -1;
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int daemon_mode = 0;
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    run_server(daemon_mode);

    return 0;
}

