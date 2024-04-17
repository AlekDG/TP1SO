#include "utils.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <strings.h>
#include <sys/mman.h>

#define PERMS O_CREAT|O_RDWR
#define MAX_SHM_SIZE 1024

//Prototipos de funciones auxiliares
int _openMem(char *id, int oflag, mode_t mode);

int _trunMem(int fd);

void _randomID(char *buffer);

void _unlinkMem(char *id);

void* _mapMem(int fd);


void exitOnError(char *msg) {
    perror(msg);
    exit(1);
}

//API para control de memoria compartida de alto nivel

memADT createSharedMemory(void) {
    memADT mem = malloc(sizeof(memStruct));
    char id[ID_SIZE];
    _randomID(id);

    int fd = _openMem(id, PERMS, S_IRUSR | 0600);

    if (fd == -1){
        return NULL;}

    if (_trunMem(fd) == -1) {
        _unlinkMem(id);
        return NULL;
    }

    mem->map =  _mapMem(fd);
 
    if (mem == NULL) {
        _unlinkMem(id);
        return NULL;
    }
    strcpy(mem->fileID, id);
    mem->flag = 0;
    return mem;
}

memADT openExistingMemory(char *id) {
    memADT mem = malloc(sizeof(memStruct));
    int fd = _openMem(id, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return NULL;
    if (mem == NULL) {
        _unlinkMem(id);
        return NULL;
    }
    mem->map = _mapMem(fd);
    return mem;
}

void unlinkMemory(memADT m) {
    _unlinkMem(m->fileID);
}

void setFlag(memADT m, int val) {
    m->flag = val;
}

int getFlag(memADT m) {
    return m->flag;
}

sem_t *getMemorySem(memADT m) {
    return m->critcalRegion;
}

char *getMemoryMap(memADT m) {
    return m->map;
}

char *getMemoryID(memADT m) {
    return m->fileID;
}

//Funciones Auxiliares. Principalmente wrappers con programacion defensiva 
int _openMem(char *id, int oflag, mode_t mode) {
    if (strlen(id) > ID_SIZE) {
        return -1;
    }
    char aux[ID_SIZE + 1];
    aux[0] = '/';
    strcpy(aux + 1, id);
    int fd = shm_open(aux, oflag, mode);
    if (fd == -1)
        return -1;
    return fd;
}

int _trunMem(int fd) {
    if (ftruncate(fd, MAX_SHM_SIZE) == -1)
        return -1;
    return 0;
}

void _unlinkMem(char *id) {
    char aux[ID_SIZE + 1];
    aux[0] = '/';
    strcpy(aux + 1, id);
    shm_unlink(id);
}

void* _mapMem(int fd) {
    void* aux = mmap(NULL, MAX_SHM_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (aux == MAP_FAILED)
        return NULL;
    return aux;
}

//Cada instacia de memoria se identifica con un nombre unico
void _randomID(char *buffer) {
    char set[] = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm0123456789";
    int setSize = strlen(set);
    int indx;
    int i = 0;
    for (; i < ID_SIZE - 1; i++) {
        indx = rand() % setSize;
        buffer[i] = set[indx];
    }
    buffer[i] = '\0';
}
