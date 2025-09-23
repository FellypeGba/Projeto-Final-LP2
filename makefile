CC = gcc
CFLAGS = -Wall -pthread -Iinclude

all: main

main: src/main.c src/tslog.c
	$(CC) $(CFLAGS) src/main.c src/tslog.c -o main

clean:
	rm -f main *.o logs.txt