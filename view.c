#include "utils.h"
#include <unistd.h>


#define BUFF_SIZE 128

int getMemoryAddress(char* pname, char* buffer);
void writeOutput(memADT mem);
int connectToApp(char* buffer);

    sem_t* critcalRegion ;
    sem_t* empty ;
    sem_t* full ;

int main(int argc, char* argv[]){
    char* appOutput=NULL;
    size_t length=0;
    if(argc==2){
        appOutput = argv[1];
        printf("arg %s\n",appOutput);
    } else {
        ssize_t len = getline(&appOutput,&length,stdin);
        if(appOutput[len-1]=='\n')
            appOutput[len-1]='\0';
        printf("%s",appOutput);
    }

    memADT sharedMem = openExistingMemory(appOutput);
    critcalRegion = sem_open(CRITICAL_REGION_SEM,O_RDWR);
    empty = sem_open(EMPTY_SEM,O_RDWR);
    full = sem_open(FULL_SEM,O_RDWR);
    puts("PASO shm");
    if(sharedMem == NULL){
        puts("PASO null");
        exitOnError("Failed to open memory");
    }
    writeOutput(sharedMem);
    free(sharedMem);
    sem_close(critcalRegion);
    sem_close(empty);
    sem_close(full);
    exit(0);
}

void writeOutput(memADT mem){
    char buffer[1000] = {0};
    char* memMap = getMemoryMap(mem);
    char* mapPtr = memMap;
    int st = 1;

    while(st){
     
        int val;
        sem_getvalue(full,&val);
        printf("full %d\n",val);
        sem_wait(full);
        sem_getvalue(critcalRegion,&val);
        printf("cr %d\n",val);
        
        sem_wait(critcalRegion);
        int i = 0;
        for(; mapPtr[i]!='\n' && mapPtr[i]!='\0';i++){
            buffer[i] = mapPtr[i];
        }
        buffer[i] = '\0';
        if(*mapPtr != STOP) mapPtr++;
        if(buffer[0] != STOP){
            sem_post(critcalRegion);
            printf("%s\n",buffer);
        }else{  
            st = 0 ; 
            sem_post(critcalRegion);
        }
        sem_post(empty);
        mapPtr+=strlen(buffer);
    }
}
