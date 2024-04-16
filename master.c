// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

#include <fcntl.h>           // For O_* constants


#define STDIN 0
#define STDOUT 1

#define PIPE_BUFF 512
#define SLAVE_HASH_OUTPUT 106   // Counts it as a string.
#define NUMBER_OF_SLAVES_FORMULA(a) (1 + ((a) - 1) / MAX_TASKS )
#define MAX_TASKS 4

// Data structure useful for creating a group of pipes used by the main process.
typedef struct pipeMatrix{
    int **pipes;
} pipeMatrix;

// Gcc doesn't find the propotype of this function, we don't know why, so we write it in here.
//  There are lots of "May point to deallocated memory" warnings. We have chosen to ignore them, since the memory will never be deallocated in the parent process.
void setlinebuf(FILE *stream);

void createPipes(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int i);
void createSlave(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int i, int slaves_amount);
void distributeInitialArgs(int initialPaths, int argCount, int argc, char * argv[], int slavesAmount, char * buffer, pipeMatrix masterToSlavePipes);
int readFromSlavesAndWriteToSharedMemory(int argc, int numberOfSlaves, pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, char * buffer);
void writeRemainingPaths(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int numberOfSlaves, int arg_count, int argc, char * aux_buffer, char * argv[]);

int main(int argc, char *argv[]) {
    pipeMatrix masterToSlavePipes;
    pipeMatrix slaveToMasterPipes;

    int status;
    char auxBuffer[PIPE_BUFF] = {0};
    // The program path/name doesn't count, so we start to counting one after.
    int argIterator = 1;
    /*
    **  Slaves are created in a log2 order of input, it reduces drastically
    **  the amount of processes created for each group of files.
    */

    int numberOfSlaves = NUMBER_OF_SLAVES_FORMULA(argc);
    int numberOfInitialPaths = numberOfSlaves / 2;


    if(numberOfSlaves == 0){
        numberOfSlaves++;
        numberOfInitialPaths++;
    }

    masterToSlavePipes.pipes = malloc(numberOfSlaves * sizeof(int *));
    slaveToMasterPipes.pipes = malloc(numberOfSlaves * sizeof(int *));

    if(masterToSlavePipes.pipes == NULL || slaveToMasterPipes.pipes == NULL){
        exitOnError("Error on matrix creation\n");
    }


    // We now create all the pipes that we're going to need.

    for(int i = 0; i < numberOfSlaves ; i++){
        createPipes(masterToSlavePipes, slaveToMasterPipes, i);
    }

    pid_t pid;
    for (int i = 0; i < numberOfSlaves ; i++) {
        if((pid = fork()) == ERROR){
            exitOnError("Fork error\n");
        }

        if(pid == CHILD){
            createSlave(masterToSlavePipes, slaveToMasterPipes, i, numberOfSlaves);
            // If reached this line, is because it was an error.
            exitOnError("Could not execve to child\n");
        }
    }

    if(masterToSlavePipes.pipes == NULL || slaveToMasterPipes.pipes == NULL){
        exitOnError("One of the pipe sets is null!!\n");
    }

    // We need to close the rest of the pipes in the parent code before forking of doing anything else.

    for(int i = 0; i < numberOfSlaves ; i++){
        if(close(masterToSlavePipes.pipes[i][READ_END]) == ERROR || close(slaveToMasterPipes.pipes[i][WRITE_END]) == ERROR){
            exitOnError("close error on master\n");
        }
    }


    // Now we have to distribute evenly all the initial paths that the slaves have to process.

    distributeInitialArgs(numberOfInitialPaths, argIterator, argc, argv, numberOfSlaves, auxBuffer, masterToSlavePipes);


    // Now we can create a subprocess of the main program that can read all the data given by the slaves.

    if((pid = fork()) == ERROR){
        exitOnError("Second fork error\n");
    }


    if(pid == CHILD){
        if(readFromSlavesAndWriteToSharedMemory(argc, numberOfSlaves, masterToSlavePipes, slaveToMasterPipes, auxBuffer) == EXIT_FAILURE){
            exitOnError("Read from slaves and write to shm error\n");
        }
    }
    /*
    **  If we are the father of all processes, we still need to keep writing the remaining paths evenly
    **  on the slaves' pipes. But first, we need to get rid of the pipes that we don't need.
    */
    writeRemainingPaths(masterToSlavePipes, slaveToMasterPipes, numberOfSlaves, argIterator, argc, auxBuffer, argv);
    // No arguments left, now we need to close the pipes and free the heap
    for(int i = 0; i < numberOfSlaves ; i++){
        if(close(masterToSlavePipes.pipes[i][1]) == ERROR){
            exitOnError("close error\n");
        }
        free(masterToSlavePipes.pipes[i]);
    }
    free(masterToSlavePipes.pipes);
    // Waits until ALL child processes finishes their tasks
    while(waitpid(-1, &status, 0) > 0);
    if(status != 0){
        exitOnError("status on some child is wrong\n");
    }
    return EXIT_SUCCESS;
}

void createPipes(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int i){
    masterToSlavePipes.pipes[i] = malloc(2 * sizeof(int));
    slaveToMasterPipes.pipes[i] = malloc(2 * sizeof(int));

    if(masterToSlavePipes.pipes[i] == NULL || slaveToMasterPipes.pipes[i] == NULL){
        exitOnError("Error on matrix creation\n");
    }

    if(pipe(masterToSlavePipes.pipes[i]) == ERROR || pipe(slaveToMasterPipes.pipes[i]) == ERROR){
        exitOnError("Error on initial pipe creation\n");
    }
}

void createSlave(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int i, int slaves_amount){
    int aux;

    if(masterToSlavePipes.pipes == NULL || slaveToMasterPipes.pipes == NULL){
        exitOnError("One of the pipe sets is null!\n");
    }

    if((aux = dup2(masterToSlavePipes.pipes[i][0], STDIN)) == ERROR || aux != STDIN){     // r-end of pipe
        exitOnError("Dup 2 error on m->s pipes\n");
    }

    if((aux = dup2(slaveToMasterPipes.pipes[i][1], STDOUT)) == ERROR || aux != STDOUT){  // w-end of pipe
        exitOnError("Dup 2 error on s->m pipes\n");
    }

    /*
    **  No need for the pipes variables to exist in the child process.
    **  We can't have memory leaks anywhere, so we're freeing everything that we are not using
    **  and closing every fd that we are not using.
    */

    for(int j = 0; j < slaves_amount ;j++){
        if(close(masterToSlavePipes.pipes[j][0]) == ERROR || close(masterToSlavePipes.pipes[j][1]) == ERROR
           || close(slaveToMasterPipes.pipes[j][0]) == ERROR || close(slaveToMasterPipes.pipes[j][1])){
            exitOnError("Close error on child\n");
        }
        free(masterToSlavePipes.pipes[j]);
        free(slaveToMasterPipes.pipes[j]);
    }
    free(masterToSlavePipes.pipes);
    free(slaveToMasterPipes.pipes);

    char *slaveEnv[] = {NULL};
    char *const slaveArgs[] = {"./slave", NULL};

    execve("./slave", slaveArgs, slaveEnv);
}

void distributeInitialArgs(int initialPaths, int argCount, int argc, char * argv[], int slavesAmount, char * buffer, pipeMatrix masterToSlavePipes){
    for(int i = 0; i < initialPaths && argCount < argc ; i++){
        for(int j = 0; j < slavesAmount && argCount < argc; j++, argCount++){
            int k = 0;
            while(k < PIPE_BUFF && argv[argCount][k] != '\0'){
                buffer[k] = argv[argCount][k];
                k++;
            }
            if(k < PIPE_BUFF - 1){
                buffer[k++] = '\n';
                buffer[k] = '\0';
            } else{
                buffer[k] = '\0';
            }

            write(masterToSlavePipes.pipes[j][1], buffer, k);
        }
    }
}

int readFromSlavesAndWriteToSharedMemory(int argc, int numberOfSlaves, pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, char * buffer){
    //  View will start showing the workers' results after receiving the output given by this process.

    size_t shmSize = (argc - 1) * SLAVE_HASH_OUTPUT + 1024;
    shared_memory_ADT shared_mem = initialize_shared_memory(shmSize);
    printf("%d\n", (int) shmSize);
    if(fflush(stdout) == EOF){
        exitOnError("fflush error\n");
    }

    int outputFileFd;
    if((outputFileFd = open("output.txt", O_WRONLY | O_CREAT)) == ERROR){
        exitOnError("open error");
    }


    // We don't need the writing pipes if we are in this process.

    for(int i = 0; i < numberOfSlaves ; i++){
        if(close(masterToSlavePipes.pipes[i][1]) == ERROR){
            exitOnError("Close error\n");
        }
        free(masterToSlavePipes.pipes[i]);
    }
    free(masterToSlavePipes.pipes);

    /*
    **  Now we need to make all the synchronization process for reading all the outputs from the slaves
    **  is blocked waiting for the childs to be on ready state.
    */

    fd_set readFds;
    int numberOfFdsAvailable, maxFd;
    maxFd = 0;

    /*
    **  We need to create stream variables for the pipes that we can use for reading all the results
    **  without any risk of reading more than we should.
    */

    FILE **pipeStreams;
    if((pipeStreams = malloc(numberOfSlaves * sizeof(FILE *))) == NULL){
        exitOnError("Pipe stream malloc error\n");
    }

    for(int i = 0; i < numberOfSlaves ; i++){
        maxFd = slaveToMasterPipes.pipes[i][0] > maxFd ? slaveToMasterPipes.pipes[i][0] : maxFd;
        if((pipeStreams[i] = fdopen(slaveToMasterPipes.pipes[i][0], "r")) == NULL){
            exitOnError("fdopen error\n");
        }

        /*
        **  The worker leaves a new line after every result, that is useful for us to
        **  separate each result correctly.
        */

        setlinebuf(pipeStreams[i]);
    }

    maxFd++;


    char *bufPtr;
    int *closedPipes = calloc(numberOfSlaves, sizeof(int));
    if(closedPipes == NULL){
        exitOnError("Calloc error\n");
    }

    int numberOfClosedPipes = 0;


    // We need to process the rest of the programs arguments.

    while(numberOfClosedPipes < numberOfSlaves){

        // Initialize the File Descriptor set for the use of select.

        FD_ZERO(&readFds);
        for(int i = 0; i < numberOfSlaves ; i++){
            if(closedPipes[i] != 1)
                FD_SET(slaveToMasterPipes.pipes[i][0], &readFds);
        }

        if((numberOfFdsAvailable = select(maxFd, &readFds, NULL, NULL, NULL)) == ERROR){
            exitOnError("Select error\n");
        }

        // For every result that is ready, we can raed it and write it on the shared memory.

        for(int i = 0; i < numberOfSlaves && numberOfFdsAvailable > 0 ; i++){
            if(closedPipes[i] != 1 && FD_ISSET(slaveToMasterPipes.pipes[i][0], &readFds)){
                bufPtr = fgets(buffer, PIPE_BUFF, pipeStreams[i]);

                // EOF reached on that pipe

                if(bufPtr == NULL){
                    if(close(slaveToMasterPipes.pipes[i][0]) == ERROR){
                        exitOnError("Close error on second-to-last area\n");
                    }
                    free(slaveToMasterPipes.pipes[i]);
                    closedPipes[i] = 1;
                    numberOfClosedPipes++;
                } else{
                    int length = strlen(buffer);
                    write_to_shared_memory(shared_mem, buffer, length);
                    write(outputFileFd, buffer, length);
                }

                numberOfFdsAvailable--;
            }
        }
    }

    // Every pipe is closed, now we need to free the remaining memory spaces and finish this process
    if(fcloseall() == EOF){
        exitOnError("fcloseall error\n");
    }
    free(slaveToMasterPipes.pipes);
    free(closedPipes);
    free(pipeStreams);

    // We notify to the shared memory that we finished writing in it

    strcpy(buffer, END_STR);
    write_to_shared_memory(shared_mem, buffer, strlen(buffer));

    sleep(4);

    unlink_shared_memory_resources(shared_mem);
    if(close(outputFileFd) == ERROR){
        exitOnError("close error\n");
    }

    return EXIT_SUCCESS;
}

void writeRemainingPaths(pipeMatrix masterToSlavePipes, pipeMatrix slaveToMasterPipes, int numberOfSlaves, int arg_count, int argc, char * aux_buffer, char * argv[]){
    for(int i = 0; i < numberOfSlaves ; i++){
        if(close(slaveToMasterPipes.pipes[i][0]) == ERROR){
            exitOnError("close errror\n");
        }
        free(slaveToMasterPipes.pipes[i]);
    }
    free(slaveToMasterPipes.pipes);

    fd_set writeFd;
    int numberOfAvailableFDs, maxFd;

    maxFd = 0;

    for(int i = 0; i < numberOfSlaves ; i++){
        maxFd = masterToSlavePipes.pipes[i][1] > maxFd ? masterToSlavePipes.pipes[i][1] : maxFd;
    }

    maxFd++;

    // We need to process the rest of the programs arguments.

    while(arg_count < argc){

        // Initialize the File Descriptor set for the use of select.
        FD_ZERO(&writeFd);

        for(int i = 0; i < numberOfSlaves ; i++){
            FD_SET(masterToSlavePipes.pipes[i][1], &writeFd);
        }

        if((numberOfAvailableFDs = select(maxFd, NULL, &writeFd, NULL, NULL)) == ERROR){
            exitOnError("select error\n");
        }

        // For every process that is ready, we can write on it the next file that needs to be processed.

        for(int i = 0; i < numberOfSlaves && arg_count < argc ; i++){
            if(FD_ISSET(masterToSlavePipes.pipes[i][1], &writeFd)){
                int k = 0;
                while(k < PIPE_BUFF && argv[arg_count][k] != '\0'){
                    aux_buffer[k] = argv[arg_count][k];
                    k++;
                }
                if(k < PIPE_BUFF - 1){
                    aux_buffer[k++] = '\n';
                    aux_buffer[k] = '\0';
                } else{
                    aux_buffer[k] = '\0';
                }
                write(masterToSlavePipes.pipes[i][1], aux_buffer, k);

                arg_count++;
            }
        }
    }
}
