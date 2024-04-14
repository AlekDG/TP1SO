#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


#define CHILD 0
#define ERROR -1
#define STDIN 0
#define PIPE_FD_ARR_SIZE 2
#define READ_END 0
#define WRITE_END 1

void exitOnError(char* msg);


#endif