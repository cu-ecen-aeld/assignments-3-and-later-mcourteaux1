#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/queue.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define TIMESTAMP_INTERVAL 10

typedef struct thread_entry {
    pthread_t thread_id;
    int client_fd;
    SLIST_ENTRY(thread_entry) entries;
} thread_entry_t;

SLIST_HEAD(thread_list_head, thread_entry) active_threads = SLIST_HEAD_INITIALIZER(active_threads);
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t exit_requested = 0;
timer_t timerid;

void handle_signal(int sig) {
    exit_requested = 1;
}

void write_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[128];
    strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tm_info);

    pthread_mutex_lock(&file_mutex);
    int fd = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd != -1) {
        write(fd, timestamp, strlen(timestamp));
        close(fd);
    }
    pthread_mutex_unlock(&file_mutex);
}

void timer_thread_handler(union sigval val) {
    write_timestamp();
}

void* handle_connection(void* arg) {
    thread_entry_t* entry = (thread_entry_t*)arg;
    char buffer[4096];
    ssize_t bytes_read;
    size_t total = 0;
    memset(buffer, 0, sizeof(buffer));

    while ((bytes_read = recv(entry->client_fd, buffer + total, sizeof(buffer) - total, 0)) > 0) {
        total += bytes_read;
        if (memchr(buffer, '\n', total)) break;
    }

    pthread_mutex_lock(&file_mutex);
    int fd = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd != -1) {
        write(fd, buffer, total);
        close(fd);
    }

    fd = open(FILE_PATH, O_RDONLY);
    if (fd != -1) {
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            send(entry->client_fd, buffer, bytes_read, 0);
        }
        close(fd);
    }
    pthread_mutex_unlock(&file_mutex);

    close(entry->client_fd);
    free(entry);
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    struct sigevent sev;
    struct itimerspec its;
    int timer_started = 0;

    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Conditional daemonization
    if (getppid() != 1) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(0);
        if (setsid() < 0) exit(EXIT_FAILURE);
    }

    // Truncate the file on start
    int trunc_fd = open(FILE_PATH, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (trunc_fd != -1) close(trunc_fd);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) < 0) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (!exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (exit_requested) break;
            continue;
        }

        if (!timer_started) {
            memset(&sev, 0, sizeof(sev));
            sev.sigev_notify = SIGEV_THREAD;
            sev.sigev_notify_function = timer_thread_handler;
            if (timer_create(CLOCK_REALTIME, &sev, &timerid) == 0) {
                its.it_value.tv_sec = TIMESTAMP_INTERVAL;
                its.it_value.tv_nsec = 0;
                its.it_interval.tv_sec = TIMESTAMP_INTERVAL;
                its.it_interval.tv_nsec = 0;
                timer_settime(timerid, 0, &its, NULL);
                timer_started = 1;
            }
        }

        thread_entry_t* entry = malloc(sizeof(thread_entry_t));
        if (!entry) {
            close(client_fd);
            continue;
        }

        entry->client_fd = client_fd;
        pthread_create(&entry->thread_id, NULL, handle_connection, entry);
        SLIST_INSERT_HEAD(&active_threads, entry, entries);
    }

    if (timer_started) timer_delete(timerid);
    close(sockfd);

    thread_entry_t* entry;
    while (!SLIST_EMPTY(&active_threads)) {
        entry = SLIST_FIRST(&active_threads);
        pthread_join(entry->thread_id, NULL);
        SLIST_REMOVE_HEAD(&active_threads, entries);
        free(entry);
    }

    pthread_mutex_destroy(&file_mutex);
    remove(FILE_PATH);
    return 0;
}
