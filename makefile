COMPILER = gcc
CFLAGS = -Wall
DEBUG_COMPILER = -Wall -pedantic -std=c99  -fsanitize=address
SEM_AND_SHM_FLAGS = -pthread -lrt

all: utils.o master slave view
	

utils.o: utils.c
	$(COMPILER) -c utils.c $(CFLAGS) -o utils.o

master: utils.o master.c
	$(COMPILER)  master.c  utils.o $(CFLAGS) $(SEM_AND_SHM_FLAGS) -o master

view: utils.o view.c
	$(COMPILER)  view.c utils.o $(CFLAGS) $(SEM_AND_SHM_FLAGS) -o view

slave:  utils.o slave.c
	$(COMPILER)  slave.c utils.o $(CFLAGS) -o slave

debug: CFLAGS=$(DEBUG_COMPILER)
debug: all

clean:
	rm -r  utils.o slave view master