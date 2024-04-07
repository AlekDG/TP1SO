/*idea: Recibo paths por stdin -> ¿Recibo EOF? -> exit()
                                -> recibo path -> llamamos a md5sum (podriamos usar popen)
                                               -> toma el result, imprime en stdout str del estilo "path result mypid"
       ->vuelvo a consultar stdin   (con read para no hacer busy w)                           
*/
//popen()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHILD 0
#define ERROR -1
#define MD5SUM "./md5sum"
#define MAXREAD 256

void exitOnError(char* msg){
    perror(msg);
    exit(1);
}

int main(){


    int pipefd[2];
    int myPid = getpid();
    //hay que ver si esta bien hacerlo con getline -> no seria bueno que la app quede blockeada en write por esto
    char *path = NULL;
    size_t size;

    int wstatus;
    char *buf = malloc(MAXREAD);
    while(getline(&path,&size,stdin) != EOF){ //getline ejecuta read bloquante -> no hacemos busy wating
        if(pipe(pipefd) == ERROR) exitOnError("PIPE ERROR\n");
        int pid = fork();
        if(pid == CHILD){
            char* str = malloc(strlen(path)-1);
            for(int i = 0; i < (strlen(path)-1); i++){
                str[i] = path[i];
            }
            str[strlen(path)] = '\0';
            
           close(pipefd[0]);
           close(1);
           dup(pipefd[1]);
           close(pipefd[1]);
            char * argv[] = {"./md5sum", str,NULL};
            char * envp[] = {NULL};
            free(str); //   If we do not free STR the memory will leak!
            if(execve(MD5SUM,argv,envp) == ERROR) exitOnError("EXEC ERROR\n");
        }
        if(pid == ERROR) exitOnError("FORK ERROR\n");
        
        read(pipefd[0],buf,MAXREAD);
        printf("%s",buf);
    }

}