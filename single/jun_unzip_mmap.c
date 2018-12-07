#include <stdio.h>
#include <stdint.h> // portable int types
#include <fcntl.h> // open
#include <unistd.h> // close
#include <assert.h> // assert
#include <stdlib.h> // exit
#include <sys/stat.h> // file stat
#include <sys/mman.h> // mmap


// #define WRITE_TO_FILE 0
#define OUT_FILE "./unzip_out.txt"


/* decompress the characters from a given input file stream */
void decompress(int fd);
uint64_t get_file_size(int fd);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* filename = argv[1];

    // open the file
    int fd = open(filename, O_RDONLY);
	assert(fd >= 0);

    // actual work
    decompress(fd);

    // close the file
    close(fd);
}

void decompress(int fd)
{
    int rc = -1; // for return codes

    FILE* out;
    #ifdef WRITE_TO_FILE
        out = fopen(OUT_FILE, "w");
        assert(out != NULL);
    #else
        out = stdout;
    #endif

    uint64_t size = get_file_size(fd);
    assert (size % 5 == 0); // compressed file must be well-formed
    
    char* start_addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    assert(start_addr != MAP_FAILED);

    uint32_t count;
    unsigned char ch;
    char* ptr = start_addr;
    while (ptr < start_addr + size)
    {
        count = *ptr + (*(ptr+1) << 8) + (*(ptr+2) << 16) + 
                       (*(ptr+3) << 24);
        ch = *(ptr+4);
        ptr += 5;
    
        for (uint32_t i = 0; i < count; i++) // decompress
            fprintf(out, "%c", ch);
    }

	rc = munmap(start_addr, size);
    assert(rc == 0);

    #ifdef WRITE_TO_FILE
        rc = fclose(out);
        assert(rc == 0);
    #endif
}

uint64_t get_file_size(int fd)
{
	struct stat stat_buf;
    fstat(fd, &stat_buf);
    return stat_buf.st_size;
}