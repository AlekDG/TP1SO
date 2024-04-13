//
// Created by jo on 07/04/24.
//

#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "utils.h"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>          /* For O_* constants */



#define MAXREAD 256
#define MAX_WRITE 256
#define MAX_CHILDREN 3
#define MAXFD 16
#define READ_END 0
#define WRITE_END 1

sem_t accessToShm;
sem_t empty;
sem_t full;


int main(int argc, char  ** argv){

    if(argc != 1){
        exitOnError("Wrong number of arguments. Must specify at least one file path.\n");
    }

    //  MASTER->SLAVE PIPE CREATION AREA.
    int MasterSlavePipes [MAX_CHILDREN][2];
    //  Process A keeps the write end open and closes the read end of the pipe. It can only write to process B.
    //  I can gather all pipes in one matrix.
    //  thePipes[n] is the n-th file descriptor array.
    //  thePipes[n][0] is the read-end of pipe n.
    //  thePipes[n][1] is the write-end of pipe n.
    for(int i = 0; i < MAX_CHILDREN; i++){
        
        if(pipe(MasterSlavePipes[i]) == ERROR){                                                 
            exitOnError("PIPE ERROR");
        }
        //  The Pipeline is already created. Now we have to assign read and write -ends.
        //  I am the parent. I do not need to read from the child process using this pipe.
    }
    // M->S PIPE CREATION AREA END.
    // SLAVE->MASTER PIPE CREATION AREA.
    int slaveMasterPipes[MAX_CHILDREN][2];
    for(int i = 0; i < MAX_CHILDREN; i++){
        if(pipe(slaveMasterPipes[i]) == ERROR){
            exitOnError("Slave to Master pipe creation ERROR");
        }
        //  Esta Pipe se usa desde el Master para leer solamente. No necesito escribir.
    }
    // SLAVE->MASTER PIPE CREATION AREA END.



    printf("Starting Master\n");                                                        
    int pids[MAX_CHILDREN];                                                             //VER SI ES NECESARIO
    int pathsProcessed = 1;
    for (int i=0; i < MAX_CHILDREN; i++){ 
        pid_t pid = fork();
        if (pid == CHILD){                                                                  //AGREGAR CLOSE;DUP;ETC
            //  I am the slave and will jump to slave program.
            pids[i] = pid;
            close(MasterSlavePipes[i][WRITE_END]);
            close(slaveMasterPipes[i][READ_END]);
            close(0);
            dup(MasterSlavePipes[i][0]);
            close(MasterSlavePipes[i][0]);
            close(1);
            dup(slaveMasterPipes[i][1]);
            close(slaveMasterPipes[i][1]);
            //  This was the pipe management, slave is "receiving by stdin" and "printing by stdout".

            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            exitOnError("Could not jump to slave program. Will now exit.\n");
        }
        else if(pid < 0) {
            exitOnError("FORK ERROR\n");
        }
        else{
            //  I am the master

            printf("Activado el slave %d\n", pids[i]);
            //  I am the master. I only want to write to the slaves using the first set of pipes. I must close read-end.
            close(MasterSlavePipes[i][0]);                                                        //DENIFIR MACRO PARA WRITE-READ PIPE END
            //  I am the master. I only want to read from the slaves using the second set of pipes. I must close write-end.
            close(slaveMasterPipes[i][1]);
            //  I will now write the FILE PATHS IN ARGV TO THE SLAVES.
           /* int currentSlave = 0;
            for(int j = 1; argv[j] == NULL; j++){                                                       //ARRANCA EN 1 ->agregar verificacion argc > 1
                if(currentSlave == MAX_CHILDREN){
                    currentSlave = 0;
                }
                write(MasterSlavePipes[currentSlave][1], argv[j], MAX_WRITE);
                currentSlave++;
            }*/

           for(int j = 1; argv[pathsProcessed] != NULL && j < MAX_CHILDREN; j++){
               //   Escribo por primera vez un argumento a cada slave.
               write(MasterSlavePipes[j][1], argv[pathsProcessed], MAX_WRITE);
               pathsProcessed++;
           }
            //  For every file path in ARGV, copy it into the current slave's pipe and increment it. If I am going to go out of bounds next loop, return to 0.

        }
    }

    //  I receive the results of md5Sum.
    int resultsReceived = 0; // I want this number to eventually be ARGC - 1.

    sem_init(&accessToShm, 1, 1);

    char readBuffer[MAXREAD];

    char * sharedMem = 0;
    int shmFd;
    if((shmFd =  shm_open(sharedMem, 0666 | O_CREAT, 0)) == -1){
        exitOnError("Shared Memory Creation ERROR");
    }
    /*if(shm_open(sharedMem, O_RDWR, 0) == -1){
        exitOnError("Shared Memory R-W creation ERROR");
    }*/
    //  Se usan sem_wait y sem_post como down() y up().

    fd_set readFs;
    int FDsAvailableForReading;
    while(resultsReceived != argc -1){
        //  Receive results by pipe.

        //  Agregar el FD de lectura a readFs.
        FD_ZERO(&readFs);
       /* for(int i = 0; i < MAX_CHILDREN; i++){
            FD_SET(slaveMasterPipes[i][0], &readFs);                     //Deberiamos sacar cuando el slave muera -> si no hay errorman
        }*/

        for(int j = 0; j < MAX_CHILDREN; j++) {
            int status;
            pid_t return_pid = waitpid(pids[j], &status, WNOHANG); /* WNOHANG hace que waitpid retorne inmediatamente. */
            if (return_pid == -1) {
                exitOnError("Wait error on second+ write\n");
            } else if (return_pid == 0) {
                FD_SET(slaveMasterPipes[j][READ_END], &readFs);
            }
        }

        //  La idea seria sacar con PSelect cuantos de esos FDs estan disponibles en un determinado momento,
        //  y una vez que lo sepa, ir iterando sobre los hijos y haciendo read.
        //  La idea seria iterar solo si la mayoria esta en estado ready, osea si FDsAvailableForReading > MAX_CHILDREN/2 + 1.

        FDsAvailableForReading = select(MAXFD, &readFs, NULL, NULL, NULL);
        if(FDsAvailableForReading > 0){                               //Deberiamos leer apenas hay info -> rescatar que fd esta ready
            for(int i = 0; i < MAX_CHILDREN && FDsAvailableForReading != 0; i++){
                if(read(slaveMasterPipes[i][0], readBuffer, MAXREAD) != 0){
                    FDsAvailableForReading--;
                    resultsReceived++;

                    sem_wait(&empty);
                    sem_wait(&accessToShm);
                    write(shmFd, readBuffer, MAX_WRITE);
                    sem_post(&accessToShm);
                    sem_post(&full);

                    //  Escribir de vuelta al hijo.

                    if(pathsProcessed < argc){
                        write(MasterSlavePipes[i][WRITE_END], argv[pathsProcessed++], MAX_WRITE);
                    }
                }
            }
        }
        else if (FDsAvailableForReading == -1){
            exitOnError("Pselect error\n");
        }


        //  Write results to shared memory.
        //  Procucer-consumer problem

    }

    for(int i = 0; i < MAX_CHILDREN; i++){
        waitpid(pids[i], NULL, WUNTRACED);
        printf("Desactivado el slave %d\n", pids[i]);
    }
}
