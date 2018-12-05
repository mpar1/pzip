CC = gcc
CFLAGS = -g -Wall -Og -Werror -Wno-unused-value

all: my_zip my_unzip pzip

my_zip: my_zip.c
	$(CC) $(CFLAGS) -o my_zip my_zip.c

my_unzip: my_unzip.c
	$(CC) $(CFLAGS) -o my_unzip my_unzip.c

pzip: pzip.c
	$(CC) $(CFLAGS) -pthread -o pzip pzip.c

clean:
	$(RM) my_zip my_unzip pzip *.o 

submit:
	cp my_zip.c my_unzip.c pzip.c pzip-submit
	submit334 pzip-submit pzip-submit
