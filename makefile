CC = gcc
CFLAGS = -Wall -pthread -Iinclude

all: server client

server: src/server.c src/tslog.c
	$(CC) $(CFLAGS) src/server.c src/tslog.c -o server

client: src/client.c src/tslog.c
	$(CC) $(CFLAGS) src/client.c src/tslog.c -o client

clean:
	rm -f server client main *.o *.txt