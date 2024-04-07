#include "utils.h"


void exitOnError(char* msg){
    perror(msg);
    exit(1);
}