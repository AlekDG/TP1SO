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
void writeOnSHM(void* shm,char* readBuffer,int shmID);
void createSlave(int i,int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);
void* createSharedMemory(int* shmID);
int createOutputFile();
void createPipes(int masterToSlavePipes[][PIPE_FD_ARR_SIZE],int slaveToMasterPipes[][PIPE_FD_ARR_SIZE],int numberOfSlaves);


sem_t * accessToShm;



int main(int argc, char  ** argv){

    if(argc < 2){
        exitOnError("Wrong number of arguments. Must specify at least one file path.\n");
    }


    int shmID;
    void * shm = createSharedMemory(&shmID);
    int outputFile = createOutputFile();
    puts(SHM_NAME);
    //sleep(2);

    if ((accessToShm = sem_open("sem.accesToShm", O_CREAT ,S_IRWXU,1)) == SEM_FAILED) {
        exitOnError("Sem Open Error\n");
    }

    //  Decido cuantos numberOfSlaves crear.
    int numberOfSlaves = NUMBER_OF_SLAVES_FORMULA(argc);
    int masterToSlavePipes [numberOfSlaves][PIPE_FD_ARR_SIZE];
    int slaveToMasterPipes[numberOfSlaves][PIPE_FD_ARR_SIZE];
    createPipes(masterToSlavePipes,slaveToMasterPipes,numberOfSlaves);
        printf("PIPES CREADOS Y LO ANTERIOR TAMBIEN\n");                                               
    int pids[numberOfSlaves];
    
    for (int i=0; i < numberOfSlaves; i++){
        int pid = fork();
        if (pid == CHILD){
            createSlave(i,masterToSlavePipes,slaveToMasterPipes);
        }
        else if(pid == ERROR) {
            exitOnError("FORK ERROR\n");
        }
        printf("HIJO %d\n",i);   
        pids[i] = pid;
        if(close(masterToSlavePipes[i][READ_END]) == ERROR){
            exitOnError("Close Error\n");
        }
        if(close(slaveToMasterPipes[i][WRITE_END]) == ERROR){
            exitOnError("Close Error\n");
        }
        printf("TEMINO CREAR HIJO ALGO\n");   

    } 
    int pathsProcessed = 1;
    int j = 1 ;
    printf("WRITEE:\n");   
    for(; argv[pathsProcessed] != NULL && j < numberOfSlaves; j++){                // TENEMOS QUE PASARLE UNA CANT X NO TODO
       if(write(masterToSlavePipes[j][WRITE_END], argv[pathsProcessed], MAX_WRITE) == ERROR){
           exitOnError("Failed to write to numberOfSlaves for the first time\n");
       }
        printf("PASE WRITE\n");   
       pathsProcessed++;
       
    }
    printf("%s",argv[j]);

    int resultsReceived = 0;

  

    char readBuffer[MAXREAD];
    fd_set readFs;
    int FDsAvailableForReading;

    while(resultsReceived != argc -1){
        printf("WHILE\n");   
        int maxFd = createFdSet(&readFs,numberOfSlaves,pids,slaveToMasterPipes);
printf("CREELOS FDSET \n");   
        ssize_t readCheck;
        FDsAvailableForReading = select(maxFd, &readFs, NULL, NULL, NULL);

        if(FDsAvailableForReading > 0){
            for(int i = 0; i < numberOfSlaves && FDsAvailableForReading != 0; i++){
                if((readCheck = read(slaveToMasterPipes[i][READ_END], readBuffer, MAXREAD)) > 0) {
                    FDsAvailableForReading--;
                    resultsReceived++;

                    writeOnSHM(shm,readBuffer,shmID);
                    write(outputFile, readBuffer, MAX_WRITE);                           

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

    close(outputFile);                             
    munmap(shm, SHM_SIZE);
    unlink(SHM_NAME);
    close(shmID);

    for(int i = 0; i < numberOfSlaves; i++){
        if(waitpid(pids[i], NULL, WUNTRACED) == -1){
            exitOnError("Final wait error\n");
        }
        printf("Desactivado el slave %d\n", pids[i]);
    }
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
        return maxFd;
}

void writeOnSHM(void* shm,char* readBuffer,int shmID){
    sem_wait(accessToShm);
    shm += write(shmID, readBuffer, MAX_WRITE);
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
        exitOnError("Close Error\n");}
    
    char *args[] = {"./slave", NULL};
    execve(args[0], args, NULL);
    exitOnError("Could not exec slave program. Will now exit.\n");
}

void* createSharedMemory(int* shmID){
    *shmID =  shm_open(SHM_NAME, O_CREAT | O_RDWR, SHM_MODE);
    if(*shmID == ERROR){
        exitOnError("Shm Open Error\n");
    }
    if(ftruncate(*shmID, SHM_SIZE) == ERROR){
        exitOnError("Ftruncate error\n");
    }
    void * shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *shmID, 0);
    if( shm == (void *) ERROR){
        exitOnError("mmap error\n");
    }
    return shm;
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