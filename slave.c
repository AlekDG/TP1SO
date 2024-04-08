
#include <string.h>
#include <unistd.h>
#include "utils.h"


#define MD5SUM "./md5sum"
#define MAXBUF 33
#define MAXREAD 1024
#define MAXPATHS 10
#define MAXPATHSIZE 50
#define ISNEWLINE(a) ((a) == '\n')

int getPath(char** files);
int parse(char** files, char* buf,size_t size);
void md5sum(int pipefd[2], char* path);

int main(int argc, char * argv[], char* envp[]){
    int myPid = getpid();
    int pipefd[2];
    char ** paths = malloc(sizeof(char*)*MAXPATHS); // matriz donde vamos a cargar los paths
    int pathCount;

    while((pathCount = getPath(paths)) != EOF){
        int i = 0;
        while(i < pathCount){
            if(pipe(pipefd) == ERROR) exitOnError("PIPE ERROR\n");        
            md5sum(pipefd,paths[i]);                    
            char *buf = malloc(MAXBUF);            
            read(pipefd[0],buf,MAXBUF); // -> no se si leer todo y despues cortarlo
            buf[MAXBUF-1] = '\0'; // -> no es lindo
            printf("File: %s - md5: %s - Processed by: %d\n",paths[i],buf,myPid);
            free(buf);
            free(paths[i]); //fue alocado en parse()    
            close(pipefd[0]);
            close(pipefd[1]);
            i++;
        }
    }

    free(paths);

    exit(0);
}




void md5sum(int pipefd[2],char* path){
        int pid = fork();
        if(pid == CHILD){
           close(pipefd[0]);     
           close(1);
           dup(pipefd[1]);
           close(pipefd[1]);
            char * argv[] = {MD5SUM,"-z", path,NULL};
            char * envp[] = {NULL};
            if(execve(MD5SUM,argv,envp) == ERROR) exitOnError("EXEC ERROR\n");
        }
        if(pid == ERROR) exitOnError("FORK ERROR\n");
        //AGREGAR MANEJO DE ERRORES DE MD5SUM 
}

//Guarda en files los paths de los archivos como strings y devuelve la cantidad de archivos
int parse(char** files, char* buf,size_t size){
    char* path = NULL;
    int pathCount = 0;
    for(size_t i = 0 , j = 0; i < size && j < MAXPATHSIZE; i++, j++){
        if(path == NULL){
            path = malloc(MAXPATHSIZE);
        }
        if(ISNEWLINE(buf[i])){
            path[j] = '\0';
            files[pathCount++] = path;
            path = NULL;
            j = 0; //MUY feo
            continue;  //feo
        }

        path[j] = buf[i];

    }
    free(buf); //deberia liberarlo en getPath creo
    return pathCount;
}

//lee de stdin y parsea
int getPath(char** files){
    char* readBuf = calloc(MAXREAD,sizeof(char));
    size_t size = read(STDIN,readBuf,MAXREAD);
    if( size == 0){
        free(readBuf);
        return EOF;
    }
    return parse(files,readBuf,size);

}