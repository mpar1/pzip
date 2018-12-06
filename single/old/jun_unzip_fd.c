#include <stdio.h>  // printf
#include <stdint.h>  // portable int types
#include <assert.h> // assert
#include <fcntl.h>  // read
#include <stdlib.h> // exit
#include <sys/types.h> // read
#include <sys/uio.h>   // ..
#include <unistd.h>    // ..


#define BYTES_PER_UNIT 5

size_t Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
void decompress(int fd);
void* Malloc(size_t size);
ssize_t Read(int fd, void* buf, int count);
void init(const char* infile, int* fd);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* infile = argv[1];
    
    int fd;

    init(infile, &fd);
    decompress(fd);

    close(fd);
}

void decompress(int fd)
{
    
    while (1)
    {
        int bytes_read = Read(fd, buf, BYTES_PER_UNIT);
        if (bytes_read == 0) break;
        if (bytes_read != BYTES_PER_UNIT)
        {
            fprintf(stderr, "ill-formed file\n");
            exit(1);
        }
        char ch;
        uint32_t count = *(buf+3);
        printf("%d,", (int) count);
        count <<= 8;
        count |= *(buf+2);
        printf("%d,", (int) count);
        count <<= 8;
        count |= *(buf+1);
        printf("%d,", (int) count);
        count <<= 8;
        count |= *(buf);
        printf("%d,", (int) count);
        ch = *(buf + 4);
        // fwrite(&ch, 1, 1, stdout);
        printf("%c\n", ch);
    }
    free(buf);
}

// size_t Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
// {
//     size_t bytes_written = fwrite(ptr, size, nmemb, stream);
//     if (ferror(fd))
//     {
//         fprintf(stderr, "write failed\n");
//         exit(1);
//     }
// }

void* Malloc(size_t size)
{
    void* ptr =  malloc(size);
    if (ptr == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    return ptr;
}

/*
 * Wrapper around read() to deal with error code.
 */
ssize_t Read(int fd, void* buf, int count)
{
    ssize_t bytes_read = read(fd, buf, count);
    if (bytes_read < 0)
    {
        fprintf(stderr, "read failed\n");
        exit(1);
    }
    return bytes_read;
}

void init(const char* infile, int* fd)
{
    // open file
    *fd = open(infile, O_RDONLY);
    if (*fd < 0) // check if open() was successful
    {
        fprintf(stderr, "Cannot open file: %s\n", infile);
        exit(1);
    }
}