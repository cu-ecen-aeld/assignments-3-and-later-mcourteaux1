#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    // Open syslog
    openlog("writer", LOG_PID, LOG_USER);

    // Check for correct argument count
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments: Expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file_path> <text_string>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    const char *text_string = argv[2];

    // Open file for writing
    FILE *file = fopen(file_path, "w");
    if (!file) {
        syslog(LOG_ERR, "Failed to open file: %s", file_path);
        perror("Error opening file");
        return 1;
    }

    // Write string to file with a newline
	if (fprintf(file, "%s\n", text_string) < 0) {

        syslog(LOG_ERR, "Failed to write to file: %s", file_path);
        perror("Error writing to file");
        fclose(file);
        return 1;
    }

    // Log successful write operation
    syslog(LOG_DEBUG, "Writing '%s' to '%s'", text_string, file_path);
    
    // Close file
    fclose(file);
    closelog();
    return 0;
}
