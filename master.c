#include <sys/select.h>
#include <string.h>
#include <sys/wait.h>
#include "utils.h"
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#define MAXREAD 256
#define MAX_WRITE 256
#define FINISHED 0
#define TERMINATED -1
#define OUTPUT_FILE "outputFile.txt"
#define SHM_SIZE 1024
#define MAX_TASKS 4
#define SHM_NAME "/sharedMemory"	
#define SHM_MODE 0600
#define NUMBER_OF_SLAVES_FORMULA(a) (1 + ((a) - 1) / MAX_TASKS )



int createFdSet(fd_set* readFs, int numberOfSlaves,int* pids,int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);
void writeOnSHM(memADT mem);
void createSlave(int i,int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);
int createOutputFile();
void createPipes(int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE],int numberOfSlaves);
void createSlaves(int pids[],int numberOfSlaves,int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);
void createSemaphore();

sem_t * accessToShm;



int main(int argc, char  ** argv){

    if(argc < 2){
        exitOnError("Wrong number of arguments. Must specify at least one file path.\n");
    }


    memADT shm = createSharedMemory();
    int outputFile = createOutputFile();
    puts(SHM_NAME);
    //sleep(2);
    createSemaphore();

    int numberOfSlaves = NUMBER_OF_SLAVES_FORMULA(argc);
    int masterToSlavePipes [numberOfSlaves][PIPE_FD_ARR_SIZE];
    int slaveToMasterPipes[numberOfSlaves][PIPE_FD_ARR_SIZE];
    createPipes(masterToSlavePipes,slaveToMasterPipes,numberOfSlaves);
    int pids[numberOfSlaves];    
    createSlaves(pids,numberOfSlaves,masterToSlavePipes,slaveToMasterPipes);
  char pathBuf[MAX_WRITE];
    
   
    int path = 1;
    for(int slave = 0 ; slave < numberOfSlaves; slave++){
        sprintf(pathBuf,"%s\n",argv[path++]);
        printf("%s",pathBuf);
        write(masterToSlavePipes[slave][WRITE_END],pathBuf,strlen(pathBuf));
    }
    int pathsSends = numberOfSlaves;
    int pathsProccesed = 0;
    fd_set readFs;
    int pathsToProcess=argc-1;
    while(pathsSends < pathsToProcess){
        int maxFd = createFdSet(&readFs,numberOfSlaves,pids,slaveToMasterPipes);
        printf("ANTES DE SELECT\n");
	select(maxFd, &readFs, NULL, NULL, NULL);
	printf("despues DE SELECT\n");
        for(int slave = 0; slave < numberOfSlaves; slave++){
            if(FD_ISSET(slaveToMasterPipes[slave][READ_END],&readFs)){
                char readBuffer[MAXREAD];
                				
                readBuffer[read(slaveToMasterPipes[slave][READ_END],readBuffer,MAXREAD)] = '\0';
                write(outputFile,readBuffer,strlen(readBuffer));
                			printf("%s\n",readBuffer);
                //writeOnSHM(shm);
                sprintf(pathBuf,"%s\n",argv[path++]);
                pathsSends++;
                write(masterToSlavePipes[slave][WRITE_END],pathBuf,strlen(pathBuf));
                pathsProccesed++;
            }
        }
    }
    int slave = 0;
    while(slave < numberOfSlaves){
   
        close(masterToSlavePipes[slave++][WRITE_END]);
    }
//me quedo esperando a leer lo que queda
    slave = 0;
    while(slave < numberOfSlaves){
        close(slaveToMasterPipes[slave++][READ_END]);
    }
    
    int finishedSlaves = 0;
    while(finishedSlaves < numberOfSlaves){
        int maxFd = createFdSet(&readFs,numberOfSlaves,pids,slaveToMasterPipes);
        printf("ANTES DE SELECTultimos\n");
	select(maxFd, &readFs, NULL, NULL, NULL);
	printf("despues DE SELECultimosT\n");
        for(int slave = 0; slave < numberOfSlaves; slave++){
            if(FD_ISSET(slaveToMasterPipes[slave][READ_END],&readFs)){
                char readBuffer[MAXREAD];
                				
                readBuffer[read(slaveToMasterPipes[slave][READ_END],readBuffer,MAXREAD)] = '\0';
                write(outputFile,readBuffer,strlen(readBuffer));
                			printf("%s\n",readBuffer);
                //writeOnSHM(shm);
                pathsProccesed++;
                finishedSlaves++;
                pids[slave] = -1;
            }
        }
    }

	write(outputFile,"\0",1);
    close(outputFile);                             
    munmap(shm, SHM_SIZE);
    unlink(SHM_NAME);

    exit(0);
}

	


int createFdSet(fd_set* readFs, int numberOfSlaves,int* pids,int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]){
        int maxFd = -1;
        FD_ZERO(readFs);
        for(int j = 0; j < numberOfSlaves; j++) {
            if(pids[j] != TERMINATED){
                FD_SET(slaveToMasterPipes[j][READ_END], readFs);
            }
            if(slaveToMasterPipes[j][READ_END] > maxFd) {
                maxFd = slaveToMasterPipes[j][READ_END];
            }
        }
        return maxFd + 1;
}

void writeOnSHM(memADT mem){
    sem_wait(accessToShm);
    write(STDOUT_FILENO, getMemoryID(mem), strlen(getMemoryID(mem)));
    sem_post(accessToShm);

}

void createSlave(int i,int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]){
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
    char *evnv[] = { NULL};
    execve(args[0], args, evnv);
    exitOnError("Could not exec slave program. Will now exit.\n");
}

int createOutputFile(){
    int file = open(OUTPUT_FILE, O_CREAT | O_RDWR, S_IRWXU );
    if(file == ERROR){
        exitOnError("Output file creation error.\n");
    }
    return file;
}

 void createPipes(int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE],int numberOfSlaves){
    for(int i = 0; i < numberOfSlaves; i++){
        if(pipe(masterToSlavePipes[i]) == ERROR){
            exitOnError("PIPE ERROR");
        }
    }
    
    for(int i = 0; i < numberOfSlaves; i++){
        if(pipe(slaveToMasterPipes[i]) == ERROR){
            exitOnError("Slave to Master pipe creation ERROR");
        }
    }
 }
void createSlaves(int pids[],int numberOfSlaves,int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]){
    for (int i=0; i < numberOfSlaves; i++){
        int pid = fork();
        if (pid == CHILD){
            createSlave(i,masterToSlavePipes,slaveToMasterPipes);
        }
        else if(pid == ERROR) {
            exitOnError("FORK ERROR\n");
        }

        printf("HIJO %d\n",i);    //DEBUG

        pids[i] = pid;
        if(close(masterToSlavePipes[i][READ_END]) == ERROR){
            exitOnError("Close Error\n");
        }
        if(close(slaveToMasterPipes[i][WRITE_END]) == ERROR){
            exitOnError("Close Error\n");
        } 

    } 
}
void createSemaphore(){
    accessToShm = sem_open("sem.accesToShm", O_CREAT ,S_IRWXU,1);
    if(accessToShm == SEM_FAILED){
        exitOnError("Sem Open Error\n");
    }
