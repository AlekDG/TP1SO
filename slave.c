
#include <string.h>
#include <unistd.h>
#include "utils.h"


#define MD5SUM "./md5sum"
#define MAXBUF 33
#define STDIN 0

void md5sum(int pipefd[PIPE_FD_ARR_SIZE], char* path);
void proccesPath(char* path,int* pipeFileDescriptors);
static int myPid;

int main(int argc, char * argv[], char* envp[]){
    myPid = getpid();
    int pipefd[PIPE_FD_ARR_SIZE];
    char* path = NULL;
    size_t len = 0;
    size_t readBytes;
    while( (readBytes = getdelim(&path,&len,'\n',stdin)) != EOF){
        if(readBytes == 0) exitOnError("Bad Path");
        path[readBytes-1] = '\0';
        proccesPath(path,pipefd);
    }
    if(path != NULL)  free(path);
    exit(0);
}

void proccesPath(char* path,int* pipeFileDescriptors){
    if(pipe(pipeFileDescriptors) == ERROR) exitOnError("PIPE ERROR\n");        
    md5sum(pipeFileDescriptors,path);                    
    char *buf = malloc(MAXBUF);
    if(buf == NULL){
        exitOnError("Malloc ERROR\n");
    }
    if(read(pipeFileDescriptors[READ_END],buf,MAXBUF) == -1){
        exitOnError("Read error in slave\n");
    }
    buf[MAXBUF-1] = '\0';
    printf("File: %s - md5: %s - Processed by: %d\n",path,buf,myPid);
    free(buf);   
    close(pipeFileDescriptors[READ_END]);
    close(pipeFileDescriptors[WRITE_END]);
}

void md5sum(int pipefd[PIPE_FD_ARR_SIZE],char* path){
        int pid = fork();
        if(pid == CHILD){
           if(close(pipefd[READ_END]) == ERROR){
               exitOnError("Close Error\n");
           }
           if(close(WRITE_END) == ERROR){
               exitOnError("Close Error\n");
           }
           if(dup(pipefd[WRITE_END]) == ERROR){
               exitOnError("Dup Error\n");
           }
           if(close(pipefd[WRITE_END]) == ERROR){
               exitOnError("Close Error\n");
           }
            char * argv[] = {MD5SUM,"-z", path,NULL};
            char * envp[] = {NULL};
            execve(MD5SUM,argv,envp);
            exitOnError("EXEC ERROR\n");  
        }
        if(pid == ERROR) exitOnError("FORK ERROR\n");
}

