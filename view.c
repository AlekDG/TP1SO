#include "utils.h"
#include <unistd.h>

#define BUFF_SIZE 128

int getMemoryAddress(char* pname, char* buffer);
void writeOutput(memADT mem);
int connctToApp(char* buffer);

int main(int argc, char* argv[]){
    char* appOutput;
    if(argc==2){
        appOutput = argv[1];
    } else {
        char aux[BUFF_SIZE];
        appOutput=aux;
        if(connectToApp(appOutput)==-1){
            exitOnError("View could not connect to Master");
        } 
    }

    memADT sharedMem = openExistingMemory(appOutput);
    if(sharedMem == NULL){
        exitOnError("Failed to open memory");
    }
    writeOutput(sharedMem);
    unlinkMemory(sharedMem);
    return 0;
}

int connectToApp(char* buffer){ //TODO: Repasar la funcion de isatty
        if(!isatty(STDIN_FILENO)){
            read(STDIN_FILENO,buffer,BUFF_SIZE);
            return 0;
        } else {
            return -1;
        }
    }

void writeOutput(memADT mem){
    char buffer[BUFF_SIZE];
    char* memMap = getMemoryMap(mem);
    char* mapPtr = memMap;
    sem_t* appSem = getMemorySem(mem);
    int semValue;

    while(1){
        if(getFlag(mem)==1){
            if(sem_getvalue(appSem,&semValue)==1){
                exitOnError("Error de Semaforo");
            }

            if(semValue == 0){
                return;
            }
        }
        sem_wait(appSem);
        strcpy(buffer,mapPtr);
        printf("%s",buffer);
        mapPtr+=strlen(buffer)+1;
    }
}
