#include <sys/select.h>
#include <string.h>
#include <sys/wait.h>
#include "utils.h"
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAXREAD 256
#define MAX_WRITE 256
#define MAX_CHILDREN 3
#define MAXFD 16
#define RWCREAT (0666 | O_CREAT)
#define FINISHED 0
#define TERMINATED -1
#define OUTPUT_FILE "./outputFile"
#define SHM_SIZE 1024

sem_t * accessToShm;

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

    if ((accessToShm = sem_open("/accesToShm", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
        exitOnError("Sem Open Error\n");
    }

    char readBuffer[MAXREAD];

    char * sharedMem;
    int shmID;
    key_t key;
    if ((key = ftok(".", 'R')) == -1) {
        exitOnError("FTOK ERROR\n");
    }

    if ((shmID = shmget(key, SHM_SIZE, RWCREAT)) == ERROR) {
        exitOnError("shmget ERROR\n");
    }

    if ((sharedMem = shmat(shmID, NULL, 0)) == (char *) ERROR) {
        exitOnError("Shmat ERROR\n");
    }

    fd_set readFs;
    int FDsAvailableForReading;

    //  Create the output file.
    int outputFileDescriptor = open(OUTPUT_FILE, O_CREAT);
    if(outputFileDescriptor == ERROR){
        exitOnError("Output file creation error.\n");
    }

    while(resultsReceived != argc -1){

        FD_ZERO(&readFs);

        for(int j = 0; j < MAX_CHILDREN; j++) {
            int status;
          /*  pid_t return_pid = waitpid(pids[j], &status, WNOHANG);
            if (return_pid == ERROR) {
                exitOnError("Wait error on second+ write\n");
            } else if (return_pid == 0) {
                FD_SET(slaveToMasterPipes[j][READ_END], &readFs);
            }*/
            if(pids[j] != TERMINATED){
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


                    sem_wait(accessToShm);
                    write(shmID, readBuffer, MAX_WRITE);
                    sem_post(accessToShm);
                    //  Write a un archivo
                    write(outputFileDescriptor, readBuffer, MAX_WRITE);
                    //  Escribir de vuelta al hijo.

                    if (pathsProcessed < argc) {
                        write(masterToSlavePipes[i][WRITE_END], argv[pathsProcessed++], MAX_WRITE);
                    }
                }
                else if (readCheck == FINISHED){
                    pids[i] = TERMINATED;
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

    if(close(outputFileDescriptor) == ERROR){
        exitOnError("Error on closing the output file\n");
    }
    for(int i = 0; i < MAX_CHILDREN; i++){
        if(waitpid(pids[i], NULL, WUNTRACED) == -1){
            exitOnError("Final wait error\n");
        }
        printf("Desactivado el slave %d\n", pids[i]);
    }
}
