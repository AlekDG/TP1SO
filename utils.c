#include "utils.h"
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>


//Shared memory
#include <sys/mman.h>
#include <sys/stat.h>        // For mode constants
#include <fcntl.h>           // For O_* constants
#include <semaphore.h>
#include "SHARED_MEM.h"

#define SPLIT_TOKEN '\n'
#define ERROR -1

/*
**  Used to motinor how many hashes can be read by the read processes. The writer will increment the
**  semaphore value every time it writes a new result, and it will decrease every time the reader tries
**  to read the results. It will block the reader process until the writer writes something new.
*/
#define SHM__READ_SEM "/SO-READ-SEM"

//  Name of the shared memory that we are going to use
#define SHM_NAME "/SO-SHARED_MEMORY"

// Data structure used to store all the info a process would need to use the shared memory.
typedef struct shared_memory_CDT{
    char *shm_ptr;
    void *shm_initial_ptr;
    sem_t *hashes_unread;
    size_t shm_size;
} shared_memory_CDT;


shared_memory_ADT initialize_shared_memory(size_t shm_size) {
    shared_memory_ADT shm;
    if((shm = malloc(sizeof(shared_memory_CDT))) == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    shm->shm_size = shm_size;
    shm->hashes_unread = sem_open(SHM__READ_SEM, O_RDWR | O_CREAT, 0666, 0);


    //  A new shared memory object initially has zero length--the size of the object.

    int shm_fd;
    if ((shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666)) == ERROR) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }


    //  Cause the regular file named by path or referenced by fd to be truncated to a size of precisely length bytes.

    if (ftruncate(shm_fd, shm_size) == ERROR) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }


    // Now we map the shared memory on the virtual memory of the caller process.

    void *shm_ptr;
    if ((shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    shm->shm_initial_ptr = shm_ptr;
    shm->shm_ptr = (char *)shm_ptr;

    return shm;
}

shared_memory_ADT open_shared_memory(size_t shm_size) {
    if(shm_size < 1)
        return NULL;

    shared_memory_ADT shm;
    if((shm = malloc(sizeof(shared_memory_CDT))) == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == ERROR) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }


    shm->shm_size = shm_size;
    if((shm->hashes_unread = sem_open(SHM__READ_SEM, O_RDWR)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }


    //  Now we map the shared memory on the virtual memory of the caller process

    void *shm_ptr;
    if ((shm_ptr = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0)) == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    shm->shm_initial_ptr = shm_ptr;
    shm->shm_ptr = (char *)shm_ptr;

    return shm;
}

void write_to_shared_memory(shared_memory_ADT shm, char* buffer, size_t size) {
    char *shm_ptr = shm->shm_ptr;

    strncpy(shm_ptr, buffer, size);
    shm->shm_ptr = shm_ptr + size;

    if(sem_post(shm->hashes_unread) == ERROR){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

}

size_t read_shared_memory(shared_memory_ADT shm, char *buffer, size_t size){
    size_t to_return = 0;

    if(sem_wait(shm->hashes_unread) == ERROR){
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }

    char *shm_ptr = shm->shm_ptr;

    while(to_return < size && shm_ptr[to_return] != SPLIT_TOKEN && shm_ptr[to_return] != END_TOKEN){
        buffer[to_return] = shm_ptr[to_return];
        to_return++;
    }
    if(shm_ptr[to_return] == END_TOKEN){
        return EOF;
    }


    shm->shm_ptr = to_return < size ? shm_ptr + to_return + 1 : shm_ptr + to_return;

    buffer[to_return++] = SPLIT_TOKEN;

    return to_return < size ? to_return + 1 : to_return;
}

void close_shared_memory(shared_memory_ADT shm){
    if(sem_close(shm->hashes_unread) == ERROR){
        perror("sem_close");
        exit(EXIT_FAILURE);
    }
    if(munmap(shm->shm_initial_ptr, shm->shm_size)==ERROR){
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    free(shm);
}

void unlink_shared_memory_resources(shared_memory_ADT shm) {
    if(sem_close(shm->hashes_unread) == ERROR){
        perror("sem_close");
        exit(EXIT_FAILURE);
    }
    if(sem_unlink(SHM__READ_SEM) == ERROR){
        perror("sem_unlink");
        exit(EXIT_FAILURE);
    }

    if(munmap(shm->shm_initial_ptr, shm->shm_size)==ERROR){
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    if(shm_unlink(SHM_NAME)==ERROR){
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }
    free(shm);
}


void exitOnError(char* msg){
    perror(msg);
    exit(1);
}
