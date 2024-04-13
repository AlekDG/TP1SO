#include "utils.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

//#define SHM_SIZE 1024

int main(int argc, char *argv[]) {
    key_t key = ftok("/tmp", 'A');
    int shmid;
    char *info;
    shmid = atoi(argv[1]);
    // Creamos el array de shared memory. Meter en el master
/*    if ((shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT)) == -1) {
        exitOnError("No se pudo alocar SHM");
    }
*/    // Aniadimos los datos al array
    if ((info = (char *)shmat(shmid, NULL, 0)) == (char *) -1) {
        exitOnError("No se pudo utilizar SHM");
    }
    // Hacemos print de la informacion
    int printAmount=atoi(argv[2]);
    for(int i=0;i<printAmount;i++) {
        printf(info);
    }
    //Desacoplamos el array
    shmdt(info);
    return 0;
}