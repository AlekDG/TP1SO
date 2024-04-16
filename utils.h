#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <bits/semaphore.h>


#define CHILD 0
#define ERROR -1
#define STDIN 0
#define PIPE_FD_ARR_SIZE 2
#define READ_END 0
#define WRITE_END 1
#define ID_SIZE 256
#define MEM_SIZE 1024

#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>


#define CHILD 0
#define ERROR -1
#define STDIN 0
#define PIPE_FD_ARR_SIZE 2
#define READ_END 0
#define WRITE_END 1

#include <stdlib.h>

#define END_TOKEN '$'
#define END_STR "$"

// Data structure provided for the user to hold all the information needed about this shared memory.
typedef struct shared_memory_CDT *shared_memory_ADT;

//  Creates the shared memory, using the size required as a parameter.
shared_memory_ADT initialize_shared_memory(size_t shm_size);

//  Opens the shared memory previously created, it will fail if we try to open without having one created.
shared_memory_ADT open_shared_memory(size_t shm_size);

// It writes the given string to the shared memory.
void write_to_shared_memory(shared_memory_ADT shm, char* buffer, size_t size);

/*
**  It reads the shared memory and writes the result on the buffer, it returns the number of
**  characters read, or EOF if we reached the end of file.
*/
size_t read_shared_memory(shared_memory_ADT shm, char *buffer, size_t size);

// Closes, but not removes the shared memory object from the process.
void close_shared_memory(shared_memory_ADT shm);

// Closes and removes the shared memory of all processes, should be called after all the processes closed the shm before.
void unlink_shared_memory_resources(shared_memory_ADT shm);

void exitOnError(char* msg);


#endif

void exitOnError(char* msg);
typedef struct memoryStruct{
    char fileID[ID_SIZE];
    int flag;
    sem_t sem;
    char map[MEM_SIZE];
} memStruct;
typedef memStruct* memADT;

void exitOnError(char* msg);
memADT createSharedMemory(void);
memADT openExistingMemory(char*id);
void unlinkMemory(memADT m);
void setFlag(memADT m,int val);
int getFlag(memADT m);
sem_t* getMemorySem(memADT m);
char* getMemoryMap(memADT m);
char* getMemoryID(memADT m);

#endif
