#include <stdio.h>
#include <stdint.h> // portable int types
#include <fcntl.h> // open
#include <unistd.h> // close
#include <assert.h> // assert
#include <stdlib.h> // exit
#include <sys/stat.h> // file stat
#include <sys/mman.h> // mmap


#define MAX_COUNT 0x80000000

#define OUT_FILE "./zip_out.z"
#define WRITE_TO_FILE 0

void compress(int fd);
void write_out(uint32_t count, unsigned char ch, FILE* out);
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
	compress(fd);

	// close the file
    int rc = close(fd);
    assert(rc == 0);
}

uint64_t get_file_size(int fd)
{
	struct stat stat_buf;
    fstat(fd, &stat_buf);
    return stat_buf.st_size;
}

void compress(int fd)
// void compress(FILE* infile)
{
    int rc = -1; // for return codes

    FILE* out;
    #ifdef WRITE_TO_FILE
        out = fopen(OUT_FILE, "w");
        assert(out != NULL);
    #else
        out = stdout;
    #endif

    // mmap the file
    uint64_t size = get_file_size(fd);
	char* start_addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    assert(start_addr != MAP_FAILED);

    unsigned char prev_ch = -1;
    unsigned char ch;
    uint32_t count = 0;
    char* ptr = start_addr;

	while (ptr < start_addr + size)
    {
        // read char
        ch = *ptr;
        ptr++;

        // compress
        if (count == 0) // initialize
        {
            prev_ch = ch;
            count = 1;
        }
        else if (count < MAX_COUNT && ch == prev_ch) // combo
        {
            count += 1;
        }
        else // counter full, or new char
        {
            write_out(count, prev_ch, out);
            prev_ch = ch;
            count = 1;
        }
    }

    if (count > 0) // last compressed unit hasn't been written out
        write_out(count, prev_ch, out);

	rc = munmap(start_addr, size); // un-mmap the file
    assert(rc == 0);

    #ifdef WRITE_TO_FILE
        rc = fclose(out);
        assert(rc == 0);
    #endif
}

void write_out(uint32_t count, unsigned char ch, FILE* out)
{
    fwrite(&count, sizeof(uint32_t), 1, out);
    fwrite(&ch, sizeof(unsigned char), 1, out);
}