
#include <string.h>
#include <unistd.h>
#include "utils.h"


#define MD5SUM "./md5sum"
#define MAXBUF 33
#define MAXREAD 1024
#define MAXPATHS 10
#define MAXPATHSIZE 50
#define ISNEWLINE(a) ((a) == '\n')
#define STDIN 0

void md5sum(int pipefd[2], char* path);

int main(int argc, char * argv[], char* envp[]){
    int myPid = getpid();
    int pipefd[2];
    char* path = NULL;
    size_t len = 0;
    size_t readBytes;

    while( (readBytes = getline(&path,&len,stdin)) != EOF){
        path[readBytes-1] = '\0'; //Change \n for \0
        if(pipe(pipefd) == ERROR) exitOnError("PIPE ERROR\n");        
        md5sum(pipefd,path);                    
        char *buf = malloc(MAXBUF);            
        read(pipefd[0],buf,MAXBUF); // -> no se si leer todo y despues cortarlo
        buf[MAXBUF-1] = '\0'; // -> no es lindo
        printf("File: %s - md5: %s - Processed by: %d\n",path,buf,myPid);
        free(buf);   
        close(pipefd[0]);
        close(pipefd[1]);
        
        
    }
    if(path != NULL)  free(path);
    exit(0);
}


//md5sum parent.c | cut -d' ' -f 1

void md5sum(int pipefd[2],char* path){
        int pid = fork();
        if(pid == CHILD){
           close(pipefd[0]);     
           close(1);
           dup(pipefd[1]);
           close(pipefd[1]);
            char * argv[] = {MD5SUM,"-z", path,NULL}; // -z makes md5sum return a null terminated string instead of a newline terminated string
            char * envp[] = {NULL};
            if(execve(MD5SUM,argv,envp) == ERROR) exitOnError("EXEC ERROR\n");
        }
        if(pid == ERROR) exitOnError("FORK ERROR\n");
}

