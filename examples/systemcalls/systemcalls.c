#include "systemcalls.h"
#include <stdlib.h>     // system(), exit()
#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // waitpid(), WIFEXITED(), WEXITSTATUS()
#include <unistd.h>     // fork(), execv(), dup2(), close()
#include <fcntl.h>      // open(), O_WRONLY, O_CREAT, O_TRUNC
#include <stdio.h>      // perror()

bool do_system(const char *cmd)
{
    if (cmd == NULL) {
        perror("NULL command");
        return false;
    }

    int status = system(cmd);
    if (status == -1) {
        perror("system() failed");
        return false;
    }
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];

    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execv(command[0], command);  // Ensure absolute path is passed
        perror("execv failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int wstatus;
        waitpid(pid, &wstatus, 0);
        va_end(args);
        return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
    } else {
        perror("fork() failed");
        va_end(args);
        return false;
    }
}

bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];

    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open() failed");
        va_end(args);
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);  // Redirect stdout
        close(fd);
        execv(command[0], command);
        perror("execv failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        close(fd);
        int wstatus;
        waitpid(pid, &wstatus, 0);
        va_end(args);
        return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
    } else {
        perror("fork() failed");
        close(fd);
        va_end(args);
        return false;
    }
}

