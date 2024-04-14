
#include <string.h>
#include <unistd.h>
#include "utils.h"


#define MD5SUM "./md5sum"
#define MAXBUF 33
#define STDIN 0

void md5sum(int pipefd[PIPE_FD_ARR_SIZE], char* path);
void proccesPath(char* path,int* pipeFileDescriptors);
static int myPid; //estara bien esto??

int main(int argc, char * argv[], char* envp[]){
    myPid = getpid();
    int ReadPipefd[PIPE_FD_ARR_SIZE];
    char* path = NULL;
    size_t len = 0;
    size_t readBytes;

    while( (readBytes = getline(&path,&len,stdin)) != EOF){
        if(readBytes == 0) exitOnError("Bad Path"); // esta ok?
        path[readBytes-1] = '\0'; //Change \n for \0
        proccesPath(path,ReadPipefd);
    }
    if(path != NULL)  free(path);

    //  El Slave tambien debe enviar por OTRO Pipe el resultado de MD5Sum.
    //  Las pipes para eso estan creadas en Master.
    exit(0);
}

void proccesPath(char* path,int* pipeFileDescriptors){
    if(pipe(pipeFileDescriptors) == ERROR) exitOnError("PIPE ERROR\n");        
    md5sum(pipeFileDescriptors,path);                    
    char *buf = malloc(MAXBUF);            
    read(pipeFileDescriptors[READ_END],buf,MAXBUF); // -> no se si leer todo y despues cortarlo
    buf[MAXBUF-1] = '\0'; // -> no es lindo
    printf("File: %s - md5: %s - Processed by: %d\n",path,buf,myPid); //deberia ser aparte para separar back/front
    free(buf);   
    close(pipeFileDescriptors[READ_END]);
    close(pipeFileDescriptors[WRITE_END]);
}



//md5sum parent.c | cut -d' ' -f 1

void md5sum(int pipefd[PIPE_FD_ARR_SIZE],char* path){
        int pid = fork();
        if(pid == CHILD){
           close(pipefd[READ_END]);
           close(WRITE_END);
           dup(pipefd[WRITE_END]);
           close(pipefd[WRITE_END]);
            char * argv[] = {MD5SUM,"-z", path,NULL}; // -z makes md5sum return a null terminated string instead of a newline terminated string
            char * envp[] = {NULL};
            execve(MD5SUM,argv,envp);
            exitOnError("EXEC ERROR\n");  
        }
        if(pid == ERROR) exitOnError("FORK ERROR\n");
}

