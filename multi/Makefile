CC = gcc
CFLAGS = -g -Wall -Og -Werror -Wno-unused-value

pzip: pzip.c
	$(CC) $(CFLAGS) -pthread -o pzip pzip.c

clean:
	$(RM) pzip *.o 