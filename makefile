CC = gcc
CFLAGS = -Wall -pthread -Iinclude

all: server client

SERVER_SRCS = src/server.c src/tslog.c src/chat_server.c src/threadsafe_queue.c

SERVER_SRCS = src/server.c src/tslog.c src/chat_server.c src/threadsafe_queue.c src/net.c

server: $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o server


client: src/client.c src/tslog.c src/net.c
	$(CC) $(CFLAGS) src/client.c src/tslog.c src/net.c -o client

clean:
	rm -f server client main *.o *.txt