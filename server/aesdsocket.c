#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"

volatile sig_atomic_t exit_requested = 0;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_signal(int sig) {
    exit_requested = 1;
}

typedef struct thread_data {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr client_addr;
    socklen_t client_addr_len;

    SLIST_ENTRY(thread_data) entries;
} thread_data_t;

SLIST_HEAD(thread_list_head, thread_data);
struct thread_list_head active_threads;

void* handle_connection(void* arg) {
    thread_data_t* td = (thread_data_t*)arg;
    char buffer[1024];
    ssize_t rcv_len;

    FILE* fp = fopen(FILE_PATH, "a+");
    if (!fp) {
        perror("fopen");
        close(td->client_fd);
        free(td);
        return NULL;
    }

    while ((rcv_len = recv(td->client_fd, buffer, sizeof(buffer), 0)) > 0) {
        pthread_mutex_lock(&file_mutex);
        fwrite(buffer, 1, rcv_len, fp);
        fflush(fp);
        pthread_mutex_unlock(&file_mutex);
    }

    fclose(fp);
    close(td->client_fd);
    return NULL;
}

void* timestamp_thread_func(void* arg) {
    while (!exit_requested) {
        sleep(10);

        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);

        char timestamp[100];
        strftime(timestamp, sizeof(timestamp), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", tm_info);

        pthread_mutex_lock(&file_mutex);
        FILE* fp = fopen(FILE_PATH, "a");
        if (fp) {
            fputs(timestamp, fp);
            fclose(fp);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    SLIST_INIT(&active_threads);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    pthread_t timer_thread;
    if (pthread_create(&timer_thread, NULL, timestamp_thread_func, NULL) != 0) {
        perror("pthread_create (timestamp)");
        close(server_fd);
        return 1;
    }

    while (!exit_requested) {
        thread_data_t* td = malloc(sizeof(thread_data_t));
        if (!td) {
            perror("malloc");
            continue;
        }

        td->client_addr_len = sizeof(td->client_addr);
        td->client_fd = accept(server_fd, (struct sockaddr*)&td->client_addr, &td->client_addr_len);

        if (td->client_fd == -1) {
            free(td);
            if (exit_requested) break;
            perror("accept");
            continue;
        }

        if (pthread_create(&td->thread_id, NULL, handle_connection, td) != 0) {
            perror("pthread_create");
            close(td->client_fd);
            free(td);
            continue;
        }

        SLIST_INSERT_HEAD(&active_threads, td, entries);
    }

    // Shutdown cleanup
    close(server_fd);
    pthread_join(timer_thread, NULL);

    thread_data_t* td;
    while (!SLIST_EMPTY(&active_threads)) {
        td = SLIST_FIRST(&active_threads);
        pthread_join(td->thread_id, NULL);
        SLIST_REMOVE_HEAD(&active_threads, entries);
        free(td);
    }

    pthread_mutex_destroy(&file_mutex);
    remove(FILE_PATH);

    return 0;
}

