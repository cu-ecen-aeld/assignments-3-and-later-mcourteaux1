#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <arpa/inet.h>

#define SERVER_PORT 9000
#define MAX_CONNECTION_QUEUE 5
#define BUFFER_LEN 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

// Global file descriptors for server and client sockets
int server_socket_fd = -1;
int client_socket_fd = -1;

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *        Cleans up sockets, removes the file, logs messages, and exits.
 */
void signal_handler(int signal_number) {
    if (signal_number == SIGINT) {
        printf("SIGINT received\n");
    } else if (signal_number == SIGTERM) {
        printf("SIGTERM received\n");
    }

    syslog(LOG_INFO, "Caught termination signal, exiting...");
    syslog(LOG_INFO, "Closed connection on port %d", SERVER_PORT);
    printf("Closed connection from port %d\n", SERVER_PORT);

    if (client_socket_fd != -1) close(client_socket_fd);
    if (server_socket_fd != -1) close(server_socket_fd);

    remove(DATA_FILE);  // Clean up the data file
    closelog();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int data_file_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    char io_buffer[BUFFER_LEN];
    ssize_t recv_bytes, read_bytes;

    int run_as_daemon = 0;
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        run_as_daemon = 1;
    }

    // Register signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create a TCP socket
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // Set server address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    server_addr.sin_port = htons(SERVER_PORT);

    // Allow address reuse
    int reuse_opt = 1;
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(reuse_opt)) == -1) {
        perror("setsockopt failed");
        close(server_socket_fd);
        return EXIT_FAILURE;
    }

    // Bind the socket to the port
    if (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket_fd);
        return EXIT_FAILURE;
    }

    // If daemon mode is requested, fork off and redirect stdio
    if (run_as_daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Parent process exits
            exit(EXIT_SUCCESS);
        }

        // Child becomes session leader
        if (setsid() == -1) {
            perror("setsid failed");
            exit(EXIT_FAILURE);
        }

        // Redirect stdin, stdout, stderr to /dev/null
        int devnull_fd = open("/dev/null", O_RDWR);
        if (devnull_fd != -1) {
            dup2(devnull_fd, STDIN_FILENO);
            dup2(devnull_fd, STDOUT_FILENO);
            dup2(devnull_fd, STDERR_FILENO);
            if (devnull_fd > 2) close(devnull_fd);
        }
    }

    // Listen for incoming connections
    if (listen(server_socket_fd, MAX_CONNECTION_QUEUE) == -1) {
        perror("Listen failed");
        close(server_socket_fd);
        return EXIT_FAILURE;
    }

    printf("Server is listening on port %d...\n", SERVER_PORT);

    // Main server loop
    while (1) {
        // Accept new connection
        client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_socket_fd == -1) {
            perror("Accept failed");
            continue;
        }

        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip_str);

        // Open the file to append received data
        data_file_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (data_file_fd == -1) {
            perror("File open for append failed");
            close(client_socket_fd);
            continue;
        }

        // Receive and write data until newline is found
        while ((recv_bytes = recv(client_socket_fd, io_buffer, BUFFER_LEN - 1, 0)) > 0) {
            io_buffer[recv_bytes] = '\0';  // Null-terminate buffer

            if (write(data_file_fd, io_buffer, recv_bytes) == -1) {
                perror("Write to file failed");
                break;
            }

            if (strchr(io_buffer, '\n') != NULL) {
                break;  // Stop reading after a complete line
            }
        }

        if (recv_bytes == -1) {
            perror("Receive failed");
        }

        close(data_file_fd);  // Close after writing

        // Open file again for reading back contents
        data_file_fd = open(DATA_FILE, O_RDONLY);
        if (data_file_fd == -1) {
            perror("File open for read failed");
            close(client_socket_fd);
            continue;
        }

        // Send file contents to the client
        while ((read_bytes = read(data_file_fd, io_buffer, BUFFER_LEN)) > 0) {
            if (send(client_socket_fd, io_buffer, read_bytes, 0) == -1) {
                perror("Send failed");
                break;
            }
        }

        close(data_file_fd);
        close(client_socket_fd);
        printf("Closed connection with client\n");
    }

    // Cleanup (unreachable in this version)
    close(server_socket_fd);
    closelog();
    return 0;
}

