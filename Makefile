CC = clang
CFLAGS = -Wall -Werror -Wextra -pedantic -pthread

all: httpserver 

httpserver: httpserver.c
	$(CC) $(CFLAGS) -o httpserver httpserver.c

clean: 
	rm -f httpserver

