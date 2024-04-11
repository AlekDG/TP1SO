//
// Created by jo on 07/04/24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "utils.h"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#define CHILD 0
#define ERROR -1
#define MD5SUM "./md5sum"
#define MAXREAD 256
#define MAX_WRITE 256
#define MAX_CHILDREN 3

int main(int argc, char  ** argv){
    //  MASTER->SLAVE PIPE CREATION AREA.
    int MasterSlavePipes [MAX_CHILDREN][2];
    int MSpipeCheck;
    //  Process A keeps the write end open and closes the read end of the pipe. It can only write to process B.
    //  I can gather all pipes in one matrix.
    //  thePipes[n] is the n-th file descriptor array.
    //  thePipes[n][0] is the read-end of pipe n.
    //  thePipes[n][1] is the write-end of pipe n.
    for(int i = 0; i < MAX_CHILDREN; i++){
        MSpipeCheck = pipe(MasterSlavePipes[i]);
        if(MSpipeCheck == -1){
            exitOnError("PIPE ERROR");
        }
        //  The Pipeline is already created. Now we have to assign read and write -ends.
        //  I am the parent. I do not need to read from the child process using this pipe.
    }
    // M->S PIPE CREATION AREA END.
    // SLAVE->MASTER PIPE CREATION AREA.
    int slaveMasterPipes[MAX_CHILDREN][2];
    int SMPipeCheck;
    for(int i = 0; i < MAX_CHILDREN; i++){
        SMPipeCheck = pipe(slaveMasterPipes[i]);
        if(SMPipeCheck == -1){
            exitOnError("Slave to Master pipe creation ERROR");
        }
        //  Esta Pipe se usa desde el Master para leer solamente. No necesito escribir.
    }
    // SLAVE->MASTER PIPE CREATION AREA END.


    //  MESSAGE CREATION AREA - FOR DEBUGGING ONLY.
    char * msg1 = "We the People\n";
    char * msg2 = "The First Law of Robotics states that a robot cannot harm a human being or through inaction, allow a human being to come to harm\n";
    char * msg3 = "The previous message was very long.\n";
    char * messages[3] = {msg1, msg2, msg3};
    //  MESSAGE CREATION AREA END.

    printf("Starting Master\n");
    int pids[MAX_CHILDREN];
    for (int i=0; i < MAX_CHILDREN; i++){
        pid_t pid = fork();
        if (pid == CHILD){
            //  I am the slave and will jump to slave program.
            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            perror("execve");   //si no hay error esto no se llega a ejecutar, ya hizo el salto.
            exit(1);
        }
        else if(pid < 0) {
            exitOnError("FORK ERROR\n");
        }
        else{
            //  I am the master
            pids[i] = pid;
            printf("Activado el slave %d\n", pids[i]);
            //  I will now write the debug messages to the slaves.
            for(int j = 0; j < MAX_CHILDREN; j++){
                write(MasterSlavePipes[i][1], messages[i], MAX_WRITE);
            }
            //  I will now write the FILE PATHS IN ARGV TO THE SLAVES.
            int currentSlave = 0;
            for(int j = 0; argv[j] == NULL; j++){
                if(currentSlave == MAX_CHILDREN){
                    currentSlave = 0;
                }
                write(MasterSlavePipes[currentSlave][1], argv[j], MAX_WRITE);
                currentSlave++;
            }
            //  For every file path in ARGV, copy it into the current slave's pipe and increment it. If I am going to go out of bounds next loop, return to 0.

        }
    }

    //  I receive the results of md5Sum.
    int resultsReceived = 0; // I want this number to eventually be ARGC - 1.

    sem_t accessToView;
    sem_init(&accessToView, 1, 1);  //  This semaphore allows access to the shared memory.

    sem_t finishedSemaphores[MAX_CHILDREN];
    for(int i = 0; i < MAX_CHILDREN; i++){
        sem_init(&finishedSemaphores[i], 1, 0);
        //  Every slave will, when it has finished writing in the pipe, set its semaphore to 1.
        //  Then, the master will read only from the children that have the semaphore in 1.
    }

    char readBuffer[MAXREAD];

    char * sharedMem;
    if(shm_open(sharedMem, O_CREAT, 0) == -1){
        exitOnError("Shared Memory Creation ERROR");
    }
    if(shm_open(sharedMem, O_RDWR, 0) == -1){
        exitOnError("Shared Memory R-W creation ERROR");
    }
    //  Se usan sem_wait y sem_post como down() y up().
    while(resultsReceived != argc -1){
        //  Receive results by pipe.
        for(int i = 0; i < MAX_CHILDREN; i++){
            sem_wait(&finishedSemaphores[i]);  //  Si no termino, esperarlo.
            read(slaveMasterPipes[i][0], readBuffer, MAXREAD);
        }
        resultsReceived++;
        //  Write results to shared memory.
    }

    for(int i = 0; i < MAX_CHILDREN; i++){
        waitpid(pids[i], NULL, WUNTRACED);
        printf("Desactivado el slave %d\n", pids[i]);
    }
}