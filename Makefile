CC=gcc
FLAGS=-g -Wall
BIN=progBar

${BIN}: main.c
	$(CC) $(FLAGS) $^ -lcurl -o $@

run:
	./${BIN}

clean:
	rm ${BIN} *.i *.h.gch *.bc *.o *.s; rm -r **.dSYM