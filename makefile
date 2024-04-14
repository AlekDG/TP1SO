COMPILER = gcc
CFLAGS = -Wall
DEBUG_COMPILER = -Wall -pedantic -std=c99  -fsanitize=address

all: utils.o master slave view
	

utils.o: utils.c
	$(COMPILER) -c utils.c $(CFLAGS) -o utils.o

master: utils.o master.c
	$(COMPILER)  master.c  utils.o $(CFLAGS) -o master

view: utils.o view.c
	$(COMPILER)  view.c utils.o $(CFLAGS) -o view

slave:  utils.o slave.c
	$(COMPILER)  slave.c utils.o $(CFLAGS) -o slave

debug: CFLAGS=$(DEBUG_COMPILER)
debug: all

clean:
	rm -r  utils.o slave view master