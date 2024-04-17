#include <sys/select.h>
#include <string.h>
#include "utils.h"

#include <sys/mman.h>
#include <sys/stat.h>


#define MAX_READ 256
#define MAX_WRITE 256
#define TERMINATED (-1)
#define OUTPUT_FILE "outputFile.txt"
#define SHM_SIZE 1024
#define MAX_TASKS 4
#define NUMBER_OF_SLAVES_FORMULA(a) (1 + ((a) - 1) / MAX_TASKS )


int createFdSet(fd_set *readFs, int numberOfSlaves, int *pids, int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);

void writeOnSHM(memADT mem,char* readBuffer,int len);

void createSlave(int i, int masterToSlavePipes[][PIPE_FD_ARR_SIZE], int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);

int createOutputFile();

void
createPipes(int masterToSlavePipes[][PIPE_FD_ARR_SIZE], int slaveToMasterPipes[][PIPE_FD_ARR_SIZE], int numberOfSlaves);

void createSlaves(int pids[], int numberOfSlaves, int masterToSlavePipes[][PIPE_FD_ARR_SIZE],
                  int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]);

void createSemaphore();

sem_t *shmSem;
    sem_t* critcalRegion ;
    sem_t* empty ;
    sem_t* full ;


int main(int argc, char **argv) {
    sem_unlink(CRITICAL_REGION_SEM);
    sem_unlink(EMPTY_SEM);
    sem_unlink(FULL_SEM);
    if (argc < 2) {
        exitOnError("Wrong number of arguments. Must specify at least one file path.\n");
    }


    memADT shm = createSharedMemory();
    int outputFile = createOutputFile();
    printf("%s\n",getMemoryID(shm));
   // shmSem = getMemorySem(shm);
  critcalRegion = sem_open(CRITICAL_REGION_SEM,O_CREAT,0666,1);
  empty = sem_open(EMPTY_SEM,O_CREAT,0666,50);
  full = sem_open(FULL_SEM,O_CREAT,0666,0);


    sleep(3);

    //createSemaphore(); //ELIMINAR CON TODO LO DE SEM QUE NO SEA LA IMPL DE ALEJO

    int numberOfSlaves = NUMBER_OF_SLAVES_FORMULA(argc);
    int masterToSlavePipes[numberOfSlaves][PIPE_FD_ARR_SIZE];
    int slaveToMasterPipes[numberOfSlaves][PIPE_FD_ARR_SIZE];
    createPipes(masterToSlavePipes, slaveToMasterPipes, numberOfSlaves);
    int pids[numberOfSlaves];
    createSlaves(pids, numberOfSlaves, masterToSlavePipes, slaveToMasterPipes);
    char pathBuf[MAX_WRITE];


    int path = 1;
    for (int slave = 0; slave < numberOfSlaves; slave++) {
        sprintf(pathBuf, "%s\n", argv[path++]);
        write(masterToSlavePipes[slave][WRITE_END], pathBuf, strlen(pathBuf));
    }
    int pathsSends = numberOfSlaves;
    int pathsProccesed = 0;
    fd_set readFs;
    int pathsToProcess = argc - 1;
    while (pathsSends < pathsToProcess) {
        int maxFd = createFdSet(&readFs, numberOfSlaves, pids, slaveToMasterPipes);
        select(maxFd, &readFs, NULL, NULL, NULL);
        for (int slave = 0; slave < numberOfSlaves; slave++) {
            if (FD_ISSET(slaveToMasterPipes[slave][READ_END], &readFs)) {
                char readBuffer[MAX_READ];
                readBuffer[read(slaveToMasterPipes[slave][READ_END], readBuffer, MAX_READ)] = '\0';
                int readLen =  strlen(readBuffer);
                write(outputFile, readBuffer,readLen);
                writeOnSHM(shm,readBuffer,readLen);
                sprintf(pathBuf, "%s\n", argv[path++]);
                pathsSends++;
                write(masterToSlavePipes[slave][WRITE_END], pathBuf, strlen(pathBuf));
                pathsProccesed++;
            }
        }
    }
    int slave = 0;
    while (slave < numberOfSlaves) {

        close(masterToSlavePipes[slave++][WRITE_END]);
    }
//me quedo esperando a leer lo que queda
    int finishedSlaves = 0;
    while (finishedSlaves < numberOfSlaves) {
        int maxFd = createFdSet(&readFs, numberOfSlaves, pids, slaveToMasterPipes);
        select(maxFd, &readFs, NULL, NULL, NULL);
        for (int slave = 0; slave < numberOfSlaves; slave++) {
            if (FD_ISSET(slaveToMasterPipes[slave][READ_END], &readFs)) {
                char readBuffer[MAX_READ];

                readBuffer[read(slaveToMasterPipes[slave][READ_END], readBuffer, MAX_READ)] = '\0';
                int readLen =strlen(readBuffer);
                write(outputFile, readBuffer, readLen);
                writeOnSHM(shm,readBuffer,readLen);
                pathsProccesed++;
                finishedSlaves++;
                pids[slave] = -1;
                close(slaveToMasterPipes[slave++][READ_END]);
            }
        }
    }

    write(outputFile, "\0", 1);
    close(outputFile);
    munmap(shm, SHM_SIZE);
   // sem_close(accessToShm);

    char buf[] = {STOP,'\n','\0'};

   writeOnSHM(shm,buf,3);
    unlinkMemory(shm);
    sem_close(critcalRegion);
    sem_close(empty);
    sem_close(full);
    sem_unlink(CRITICAL_REGION_SEM);
    sem_unlink(EMPTY_SEM);
    sem_unlink(FULL_SEM);

    exit(0);
}


int createFdSet(fd_set *readFs, int numberOfSlaves, int *pids, int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]) {
    int maxFd = -1;
    FD_ZERO(readFs);
    for (int j = 0; j < numberOfSlaves; j++) {
        if (pids[j] != TERMINATED) {
            FD_SET(slaveToMasterPipes[j][READ_END], readFs);
        }
        if (slaveToMasterPipes[j][READ_END] > maxFd) {
            maxFd = slaveToMasterPipes[j][READ_END];
        }
    }
    return maxFd + 1;
}

void writeOnSHM(memADT mem,char* readBuffer, int len) {//agregue el readBuffer

    sem_wait(empty);
   sem_wait(critcalRegion);
  // printf("IMPRIMO %s\n",readBuffer);
    strcpy(mem->map,readBuffer);
   sem_post(critcalRegion);
   sem_post(full);
    mem->map += len;

}

void createSlave(int i, int masterToSlavePipes[][PIPE_FD_ARR_SIZE], int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]) {
    if (close(masterToSlavePipes[i][WRITE_END]) == ERROR) {
        exitOnError("Close Error\n");
    }
    if (close(slaveToMasterPipes[i][READ_END]) == ERROR) {
        exitOnError("Close Error\n");
    }
    if (close(READ_END) == ERROR) {
        exitOnError("Close Error\n");
    }
    if (dup(masterToSlavePipes[i][READ_END]) == ERROR) {
        exitOnError("Dup Error\n");
    }
    if (close(masterToSlavePipes[i][READ_END]) == ERROR) {
        exitOnError("Close Error\n");
    }
    if (close(WRITE_END) == ERROR) {
        exitOnError("Close Error\n");
    }
    if (dup(slaveToMasterPipes[i][WRITE_END]) == ERROR) {
        exitOnError("Dup Error\n");
    }
    if (close(slaveToMasterPipes[i][WRITE_END]) == ERROR) {
        exitOnError("Close Error\n");
    }

    char *args[] = {"./slave", NULL};
    char *evnv[] = {NULL};
    execve(args[0], args, evnv);
    exitOnError("Could not exec slave program. Will now exit.\n");
}

int createOutputFile() {
    int file = open(OUTPUT_FILE, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
    if (file == ERROR) {
        exitOnError("Output file creation error.\n");
    }
    return file;
}

void createPipes(int masterToSlavePipes[][PIPE_FD_ARR_SIZE], int slaveToMasterPipes[][PIPE_FD_ARR_SIZE],
                 int numberOfSlaves) {
    for (int i = 0; i < numberOfSlaves; i++) {
        if (pipe(masterToSlavePipes[i]) == ERROR) {
            exitOnError("PIPE ERROR");
        }
    }

    for (int i = 0; i < numberOfSlaves; i++) {
        if (pipe(slaveToMasterPipes[i]) == ERROR) {
            exitOnError("Slave to Master pipe creation ERROR");
        }
    }
}

void createSlaves(int pids[], int numberOfSlaves, int masterToSlavePipes[][PIPE_FD_ARR_SIZE],
                  int slaveToMasterPipes[][PIPE_FD_ARR_SIZE]) {
    for (int i = 0; i < numberOfSlaves; i++) {
        int pid = fork();
        if (pid == CHILD) {
            createSlave(i, masterToSlavePipes, slaveToMasterPipes);
        } else if (pid == ERROR) {
            exitOnError("FORK ERROR\n");
        }

        pids[i] = pid;
        if (close(masterToSlavePipes[i][READ_END]) == ERROR) {
            exitOnError("Close Error\n");
        }
        if (close(slaveToMasterPipes[i][WRITE_END]) == ERROR) {
            exitOnError("Close Error\n");
        }

    }
}

/*void createSemaphore() {
    accessToShm = sem_open("sem.accesToShm", O_CREAT, S_IRWXU, 1);
    if (accessToShm == SEM_FAILED) {
        exitOnError("Sem Open Error\n");
    }
}*/