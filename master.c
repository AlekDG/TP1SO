#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "utils.h"
#include <sys/mman.h>
#include <fcntl.h>

#define MAXREAD 256
#define MAX_WRITE 256
#define MAX_CHILDREN 3
#define MAXFD 16

sem_t accessToShm;
sem_t empty;
sem_t full;

int main(int argc, char  ** argv){

    if(argc != 1){
        exitOnError("Wrong number of arguments. Must specify at least one file path.\n");
    }

    int masterToSlavePipes [MAX_CHILDREN][PIPE_FD_ARR_SIZE];
    for(int i = 0; i < MAX_CHILDREN; i++){
        if(pipe(masterToSlavePipes[i]) == ERROR){
            exitOnError("PIPE ERROR");
        }
    }

    int slaveToMasterPipes[MAX_CHILDREN][PIPE_FD_ARR_SIZE];
    for(int i = 0; i < MAX_CHILDREN; i++){
        if(pipe(slaveToMasterPipes[i]) == ERROR){
            exitOnError("Slave to Master pipe creation ERROR");
        }
    }




    printf("Starting Master\n");                                                        
    int pids[MAX_CHILDREN];
    int pathsProcessed = 1;
    for (int i=0; i < MAX_CHILDREN; i++){ 
        pid_t pid = fork();
        if (pid == CHILD){
            pids[i] = pid;

            if(close(masterToSlavePipes[i][WRITE_END]) == ERROR){
                exitOnError("Close Error\n");
            }
            if(close(slaveToMasterPipes[i][READ_END]) == ERROR){
                exitOnError("Close Error\n");
            }
            if(close(READ_END) == ERROR){
                exitOnError("Close Error\n");
            }
            if(dup(masterToSlavePipes[i][READ_END]) == ERROR){
                exitOnError("Dup Error\n");
            }
            if(close(masterToSlavePipes[i][READ_END]) == ERROR){
                exitOnError("Close Error\n");
            }
            if(close(WRITE_END) == ERROR){
                exitOnError("Close Error\n");
            }
            if(dup(slaveToMasterPipes[i][WRITE_END]) == ERROR){
                exitOnError("Dup Error\n");
            }
            if(close(slaveToMasterPipes[i][WRITE_END]) == ERROR){
                exitOnError("Close Error\n");
            }

            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            exitOnError("Could not jump to slave program. Will now exit.\n");
        }
        else if(pid < 0) {
            exitOnError("FORK ERROR\n");
        }
        else{
            printf("Activado el slave %d\n", pids[i]);
            if(close(masterToSlavePipes[i][READ_END]) == ERROR){
                exitOnError("Close Error\n");
            }
            if(close(slaveToMasterPipes[i][WRITE_END]) == ERROR){
                exitOnError("Close Error\n");
            }

           for(int j = 1; argv[pathsProcessed] != NULL && j < MAX_CHILDREN; j++){

               if(write(masterToSlavePipes[j][WRITE_END], argv[pathsProcessed], MAX_WRITE) == ERROR){
                   exitOnError("Failed to write to slaves for the first time\n");
               }
               pathsProcessed++;
           }
        }
    }


    int resultsReceived = 0;

    sem_init(&accessToShm, 1, 1);
    accessToShm = *sem_open("accessToShm", O_CREAT);
    sem_init(&empty, 1, 1);
    empty = *sem_open("empty", O_CREAT);
    sem_init(&full, 1, 1);
    full = *sem_open("full", O_CREAT);
    char readBuffer[MAXREAD];

    char * sharedMem = 0;
    int shmFd;
    if((shmFd =  shm_open(sharedMem, 0666 | O_CREAT, 0)) == ERROR){
        exitOnError("Shared Memory Creation ERROR");
    }

    fd_set readFs;
    int FDsAvailableForReading;
    while(resultsReceived != argc -1){

        FD_ZERO(&readFs);

        for(int j = 0; j < MAX_CHILDREN; j++) {
            int status;
            pid_t return_pid = waitpid(pids[j], &status, WNOHANG);
            if (return_pid == -1) {
                exitOnError("Wait error on second+ write\n");
            } else if (return_pid == 0) {
                FD_SET(slaveToMasterPipes[j][READ_END], &readFs);
            }
        }

        int readCheck;
        FDsAvailableForReading = select(MAXFD, &readFs, NULL, NULL, NULL);
        if(FDsAvailableForReading > 0){
            for(int i = 0; i < MAX_CHILDREN && FDsAvailableForReading != 0; i++){
                if((readCheck = read(slaveToMasterPipes[i][READ_END], readBuffer, MAXREAD)) > 0) {
                    FDsAvailableForReading--;
                    resultsReceived++;

                    sem_wait(&empty);
                    sem_wait(&accessToShm);
                    write(shmFd, readBuffer, MAX_WRITE);
                    sem_post(&accessToShm);
                    sem_post(&full);

                    //  Escribir de vuelta al hijo.

                    if (pathsProcessed < argc) {
                        write(masterToSlavePipes[i][WRITE_END], argv[pathsProcessed++], MAX_WRITE);
                    }
                }
                else if (readCheck == 0){
                    ;
                }
                else{
                    exitOnError("Failed to read from children\n");
                }
            }
        }
        else if (FDsAvailableForReading == ERROR){
            exitOnError("Pselect error\n");
        }
    }

    for(int i = 0; i < MAX_CHILDREN; i++){
        if(waitpid(pids[i], NULL, WUNTRACED) == -1){
            exitOnError("Final wait error\n");
        }
        printf("Desactivado el slave %d\n", pids[i]);
    }
}
