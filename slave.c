#include <string.h>
#include <unistd.h>
#include "utils.h"

#define MD5SUM "/usr/bin/md5sum"
#define MD5_SIZE 33

void md5sum(int pipefd[PIPE_FD_ARR_SIZE], char *path);

void proccesPath(char *path, int *pipeFileDescriptors);

static int myPid;

int main(int argc, char *argv[], char *envp[]) {
    myPid = getpid();
    int pipefd[PIPE_FD_ARR_SIZE];
    char *path = NULL;
    size_t len = 0;
    size_t readBytes;
    while ((readBytes = getdelim(&path, &len, '\n', stdin)) != EOF) {
        if (readBytes == 0) exitOnError("Bad Path");
        path[readBytes - 1] = '\0';
        proccesPath(path, pipefd);
        fflush(stdin);
    }
    if (path != NULL)
        free(path);
    exit(0);
}

void proccesPath(char *path, int *pipeFileDescriptors) {
    if (pipe(pipeFileDescriptors) == ERROR) exitOnError("PIPE ERROR\n");
    md5sum(pipeFileDescriptors, path);
    char *buf = malloc(MD5_SIZE);
    if (buf == NULL) {
        exitOnError("Malloc ERROR\n");
    }
    if (read(pipeFileDescriptors[READ_END], buf, MD5_SIZE) == -1) {
        exitOnError("Read error in slave\n");
    }
    buf[MD5_SIZE - 1] = '\0';
    char writeBuf[256];
    sprintf(writeBuf, "File: %s - md5: %s - Processed by: %d\n", path, buf, myPid);
    write(1, writeBuf, strlen(writeBuf) + 1);
    free(buf);
    close(pipeFileDescriptors[READ_END]);
    close(pipeFileDescriptors[WRITE_END]);
}

void md5sum(int pipefd[PIPE_FD_ARR_SIZE], char *path) {
    int pid = fork();
    if (pid == CHILD) {
        if (close(pipefd[READ_END]) == ERROR) {
            exitOnError("Close Error\n");
        }
        if (close(WRITE_END) == ERROR) {
            exitOnError("Close Error\n");
        }
        if (dup(pipefd[WRITE_END]) == ERROR) {
            exitOnError("Dup Error\n");
        }
        if (close(pipefd[WRITE_END]) == ERROR) {
            exitOnError("Close Error\n");
        }
        char *argv[] = {MD5SUM, "-z", path, NULL};
        char *envp[] = {NULL};
        execve(MD5SUM, argv, envp);
        exitOnError("EXEC ERROR\n");
    }
    if (pid == ERROR) exitOnError("FORK ERROR\n");
}
