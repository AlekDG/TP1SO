COMPILER = gcc
CFLAGS = -Wall
DEBUG_COMPILER = -Wall -pedantic -std=c99  -fsanitize=address

all: utils.o master.o slave.o view.o
	

utils.o: utils.c
	$(COMPILER) -c utils.c $(CFLAGS) -o utils.o

master.o: master.c
	$(COMPILER) -c master.c $(CFLAGS) -o master.o

view.o: view.c
	$(COMPILER) -c view.c $(CFLAGS) -o view.o

slave.o: slave.c
	$(COMPILER) -c slave.c $(CFLAGS) -o slave.o

debug: CFLAGS=$(DEBUG_COMPILER)
debug: all

clean:
	rm -r $(OUTPUT_FILE) *.o 