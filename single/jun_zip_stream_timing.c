#include <stdio.h>
#include <stdint.h> // portable int types
#include <assert.h> // assert
#include <stdlib.h> // exit
#include <time.h> // for benchmarking


// #define WRITE_READABLE // write compressed data in a human readable format

/* Wrapper functions around the standard IO routines */
FILE* Fopen(const char* filename);
size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
void Fclose(FILE* stream);

void write_out(uint32_t count, unsigned char ch, FILE* out);

#define MAX_COUNT 0x80000000


#define TIMING_FILE "./stats/stream_out.txt"
#define OUT_FILE "./stream_out"
#define WRITE_TO_FILE 0
// #define ENABLE_TIMING 0

/* compress the characters from a given input file stream */
void compress(FILE* infile);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* filename = argv[1];
    // open the file
    FILE* infile = Fopen(filename);
    // actual work
    compress(infile);
    // close the file
    Fclose(infile);
}

void compress(FILE* infile)
{
    #ifdef ENABLE_TIMING
        FILE* timing = fopen(TIMING_FILE, "w");
    #endif

    #ifdef WRITE_TO_FILE
        FILE* out = fopen(OUT_FILE, "w");
    #endif

    uint32_t count = 0;
    unsigned char prev_ch = -1;
    unsigned char ch;
    size_t rc;

    #ifdef ENABLE_TIMING
        time_t s,e;
        double read_time = 0.0;
        double compress_time = 0.0;
        double write_time = 0.0;
    #endif

    while (1)
    {
        #ifdef ENABLE_TIMING
            s = clock();
        #endif

        // read char
        rc = Fread(&ch, sizeof(unsigned char), 1, infile);
        if (rc == 0)
        {
            write_out(count, prev_ch, out);
            break; // EOF
        }

        assert(rc == 1); // make sure only one char was read

        #ifdef ENABLE_TIMING
        e = clock();
        read_time += (double) (e - s)/CLOCKS_PER_SEC;
        #endif
        
        #ifdef ENABLE_TIMING
            s = clock();
        #endif
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
            #ifdef ENABLE_TIMING
            e = clock();
            compress_time += (double) (e - s)/CLOCKS_PER_SEC;
            s = clock();
            #endif
            write_out(count, prev_ch, out);
            prev_ch = ch;
            count = 1;
            #ifdef ENABLE_TIMING
            e = clock();
            write_time += (double) (e - s)/CLOCKS_PER_SEC;
            #endif
        }
        #ifdef ENABLE_TIMING
        e = clock();
        write_time += (double) (e - s)/CLOCKS_PER_SEC;
        #endif
    }

    #ifdef ENABLE_TIMING
        fprintf(timing, "read takes %f sec\n", read_time);
        fprintf(timing, "compress take %f sec\n", compress_time);
        fprintf(timing, "write take %f sec\n", write_time);
        fclose(timing);
    #endif

    #ifdef WRITE_TO_FILE
        fclose(out);
    #endif
}

void write_out(uint32_t count, unsigned char ch, FILE* out)
{
    #ifdef WRITE_TO_FILE
        Fwrite(&count, sizeof(uint32_t), 1, out);
        Fwrite(&ch, sizeof(unsigned char), 1, out);
    #else
        Fwrite(&count, sizeof(uint32_t), 1, stdout);
        Fwrite(&ch, sizeof(unsigned char), 1, stdout);
        // printf("%d%c", count, prev_ch);
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