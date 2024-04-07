//
// Created by jo on 07/04/24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define CHILD 0
#define ERROR -1
#define MD5SUM "./md5sum"
#define MAXREAD 256
#define MAX_WRITE 256
#define MAX_CHILDREN 3

int main(){
    //  MASTER->SLAVE PIPE CREATION AREA.
    int thePipes [MAX_CHILDREN][2];
    int pipeCheck;
    //  Process A keeps the write end open and closes the read end of the pipe. It can only write to process B.
    //  I can gather all pipes in one matrix.
    //  thePipes[n] is the n-th file descriptor array.
    //  thePipes[n][0] is the read-end of pipe n.
    //  thePipes[n][1] is the write-end of pipe n.
    for(int i = 0; i < MAX_CHILDREN; i++){
        pipeCheck = pipe(thePipes[i]);
        if(pipeCheck == -1){
            perror("Pipe ERROR\n");
            exit(1);
        }
        //  The Pipeline is already created. Now we have to assign read and write -ends.
        //  I am the parent. I do not need to read from the child process using this pipe.
        close(thePipes[i][0]);
    }
    // M->S PIPE CREATION AREA END.


    //  MESSAGE CREATION AREA - FOR DEBUGGING ONLY.
    char * msg1 = "We the People\n";
    char * msg2 = "The First Law of Robotics states that a robot cannot harm a human being or through inaction, allow a human being to come to harm\n";
    char * msg3 = "The previous message was very long.\n";
    char * messages[3] = {msg1, msg2, msg3};
    //  MESSAGE CREATION AREA END.

    printf("Starting Master\n");
    int pids[MAX_CHILDREN];
    for (int i=0; i < MAX_CHILDREN; i++){
        pid_t pid = fork();
        if (pid == CHILD){
            //  I am the slave and will jump to slave program.
            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            perror("execve");   //si no hay error esto no se llega a ejecutar, ya hizo el salto.
            exit(1);
        }
        else if(pid < 0) {
            perror("Fork error");
            exit(1);
        }
        else{
            //  I am the master
            pids[i] = pid;
            printf("Activado el slave %d\n", pids[i]);
            //  I will now write to the slaves.
            for(int j = 0; j < MAX_CHILDREN; j++){
                write(thePipes[i][1], messages[i], MAX_WRITE);
            }
        }
    }

    for(int i = 0; i < MAX_CHILDREN; i++){
        waitpid(pids[i], NULL, WUNTRACED);
        printf("Desactivado el slave %d\n", pids[i]);
    }
}