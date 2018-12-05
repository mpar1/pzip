#include <stdio.h>
#include <stdint.h> // portable int types
#include <assert.h> // assert
#include <stdlib.h> // exit
#include <sys/stat.h> // file stat
#include <time.h> // for benchmarking


// #define WRITE_READABLE // write compressed data in a human readable format

/* Wrapper functions around the standard IO routines */
FILE* Fopen(const char* filename);
size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
void Fclose(FILE* stream);

void write_out(uint32_t count, unsigned char ch);

#define MAX_COUNT 0x80000000

#define STATS_FILE "my_stats.txt"

/* compress the characters from a given input file stream */
// void compress(FILE* infile);
void compress(int fd);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* filename = argv[1];
    // open the file
    // FILE* infile = Fopen(filename);
	int fd = open(filename, O_RDONLY);
	assert(fd >= 0);

	// actual work
	// compress(infile);
	compress(fd);

	// close the file
    // Fclose(infile);
	close(fd);
}

void compress(int fd)
// void compress(FILE* infile)
{

	struct stat stat_buf;

	fstat(fd, &stat_buf);
	size_t size = stat_buf.st_size;

	// void *mmap(void *addr, size_t length, int prot, int flags,
    //               int fd, off_t offset);
	char* start_addr = mmap(NULL, size, PROT_READ, MAP_POPULATE, fd, 0);


	uint32_t count = 0;
    unsigned char prev_ch = -1;
    unsigned char ch;
    size_t rc;

    time_t s,e;
    double read_time = 0.0;
    double compress_time = 0.0;
    double write_time = 0.0;

	for (size_t i = 0; i < size)
    {
        s = clock();
        // read char
        rc = Fread(&ch, sizeof(unsigned char), 1, infile);
        if (rc == 0)
        {
            write_out(count, prev_ch);
            break; // EOF
        }
        assert(rc == 1); // make sure only one char was read
        e = clock();
        read_time += (double) (e - s)/CLOCKS_PER_SEC;

        s = clock();
        // compress
        if (count <= 0) // initialize
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
            e = clock();
            compress_time += (double) (e - s)/CLOCKS_PER_SEC;
            s = clock();
            write_out(count, prev_ch);
            prev_ch = ch;
            count = 1;
            e = clock();
            write_time += (double) (e - s)/CLOCKS_PER_SEC;
        }
        e = clock();
        write_time += (double) (e - s)/CLOCKS_PER_SEC;
    }

	// int munmap(void *addr, size_t length);
	munmap(start_addr, size);

    FILE* stat = fopen(STATS_FILE, "w");
    fprintf(stat, "read takes %f sec\n", read_time);
    fprintf(stat, "compress take %f sec\n", compress_time);
    fprintf(stat, "write take %f sec\n", write_time);
    fclose(stat);
}

void write_out(uint32_t count, unsigned char ch)
{
    #ifdef WRITE_READABLE
        printf("%d%c", count, prev_ch);
    #else
        Fwrite(&count, sizeof(uint32_t), 1, stdout);
        Fwrite(&ch, sizeof(unsigned char), 1, stdout);
    #endif
}

size_t Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t rc = fwrite(ptr, size, nmemb, stream);
    return rc;
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t rc = fread(ptr, size, nmemb, stream);
    if (ferror(stream)) // check if fread() was successful
    {
        fprintf(stderr, "fread() returns an error\n");
        exit(1);
    }
    return rc;
}

FILE* Fopen(const char* filename)
{
    FILE* stream = fopen(filename, "r");
    if (stream == NULL) // check if fopen() was successful
    {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        exit(1);
    }
    return stream;
}

void Fclose(FILE* stream)
{
    int rc = fclose(stream);
    if (rc == EOF) // check if fclose() was successful
    {
        fprintf(stderr, "Cannot close file\n");
        exit(1);
    }
}
