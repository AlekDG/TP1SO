#include "utils.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
//#define SHM_SIZE 1024

extern sem_t accessToShm;
extern sem_t empty;
extern sem_t full;

int main(int argc, char *argv[]) {
    key_t key = ftok("/tmp", 'A');
    char* shmid;
    if(argv[1]!=NULL)
        shmid = argv[1];
    else
        shmid = read(stdin);
    // Creamos el array de shared memory. Meter en el master
/*    if ((shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT)) == -1) {
        exitOnError("No se pudo alocar SHM");
    }
*/    // Aniadimos los datos al array
    int fd=shm_open(shmid);
    //sem_wait(&full);
    //sem_wait(&accessToShm);
    if ((info = (char *)shmat(fd, NULL, 0)) == (char *) -1) {
        exitOnError("No se pudo utilizar SHM");
    }
    //sem_post(&accessToShm);
    //sem_post(&empty);

    // Hacemos print de la informacion
    
    int printAmount=atoi(argv[2]);
    for(int i=0;i<printAmount;i++) {
        printf("%s\n", info);
    }
    //Desacoplamos el array
    shmdt(info);
    return 0;
}
