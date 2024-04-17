#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>


#define CHILD 0
#define ERROR (-1)
#define PIPE_FD_ARR_SIZE 2
#define READ_END 0
#define WRITE_END 1
#define ID_SIZE 6
#define MEM_SIZE 1024
#define CRITICAL_REGION_SEM "/critical"
#define EMPTY_SEM "/empty"
#define FULL_SEM "/full"
#define STOP '='

void exitOnError(char *msg);

typedef struct memoryStruct {
    char fileID[ID_SIZE];
    int flag;
    sem_t* critcalRegion;
    sem_t* empty;
    sem_t* full;
    char* map;
} memStruct;
typedef memStruct *memADT;

void exitOnError(char *msg);

memADT createSharedMemory(void);

memADT openExistingMemory(char *id);


void writeToSHM(memADT m,char* data);

void unlinkMemory(memADT m);

void setFlag(memADT m, int val);

int getFlag(memADT m);

sem_t *getMemorySem(memADT m);

char *getMemoryMap(memADT m);

char *getMemoryID(memADT m);

#endif
