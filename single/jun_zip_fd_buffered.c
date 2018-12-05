#include <stdio.h>  // printf
#include <stdint.h>  // portable int types
#include <assert.h> // assert
#include <fcntl.h>  // read
#include <stdlib.h> // exit
#include <sys/types.h> // read
#include <sys/uio.h>   // ..
#include <unistd.h>    // ..
#include <time.h> // for benchmarking

#define STATS_FILE "my_stats_buffer.txt"

// #define PRINT_DATA 0 // print all characters read
// #define BUFFER_OUTPUT 0
// #define WRITE_READABLE // write compressed data in a human readable format
#define IN_BUF_SIZE (0x1000000) // max buffer size is 2^24B = 16MB
#define OUT_BUF_SIZE IN_BUF_SIZE



#define TRUE 1
#define FALSE 0
typedef char boolean_t;

typedef uint64_t in_buffer_t; // support buffer size up to max of unsigned long
typedef uint32_t count_t; // can compress up to 4 bytes worth of occurrences (2^32 - 1 occurrences)
#define MAX_COMBO 0xffffffff // max combo = 2^32 - 1

#ifdef BUFFER_OUTPUT
typedef uint64_t out_buffer_t;
#endif

typedef struct __continue_point_t
{
    count_t cnt; // previous count
    char chr;     // previous char
    boolean_t valid;         // valid bit (but smallest type is byte in C)
} continue_point_t;

void init(const char* infile, int* fd, char** buf, char** chars, count_t** counts);
void clean(int fd, char* buf, char* chars, count_t* counts);
in_buffer_t read_to_buf(int fd, char** buf);
continue_point_t compress(continue_point_t previous,
                          char* buf,    int bytes_read,
                          char** chars, count_t** counts);
void write_combo(int cnt, int chr);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* infile = argv[1];
    
    int fd;
    char* buf; // data buffer
    char* chars; // characters
    count_t* counts; // occurences of each char in chars

    init(infile, &fd, &buf, &chars, &counts);

    continue_point_t leftover;
    leftover.valid = FALSE;

    time_t s,e;
    double read_time = 0.0;
    double compress_time = 0.0;
    double write_time = 0.0;

    while (TRUE)
    {
        s = clock();
        int bytes_read = read_to_buf(fd, &buf);
        e = clock();
        read_time += (double) (e - s)/CLOCKS_PER_SEC;
        if (bytes_read == 0)
        {
            break;
        }
        s = clock();
        leftover = compress(leftover, buf, bytes_read, &chars, &counts);
        e = clock();
        compress_time += (double) (e - s)/CLOCKS_PER_SEC;

    }
    
    // write leftover compressed data
    if (leftover.valid == TRUE)
    {
        s = clock();
        write_combo(leftover.cnt, leftover.chr);
        e = clock();
        write_time += (double) (e - s)/CLOCKS_PER_SEC;
    }
    
    #ifdef WRITE_READABLE
        printf("\n");
    #endif

    clean(fd, buf, chars, counts);
        FILE* stat = fopen(STATS_FILE, "w");
    fprintf(stat, "read takes %f sec\n", read_time);
    fprintf(stat, "compress take %f sec\n", compress_time);
    fprintf(stat, "write take %f sec\n", write_time);
    fclose(stat);
}

void write_combo(int cnt, int chr)
{
    #ifdef WRITE_READABLE
        printf("%d", cnt);
    #else
        fwrite(&cnt, 4, 1, stdout);
    #endif

    fwrite(&chr, 1, 1, stdout);
}

continue_point_t compress(continue_point_t leftover, char* buf, int bytes_read, char** chars, count_t** counts)
{
#ifndef BUFFER_OUTPUT
    char combo_char;
    count_t combo_count;
    // continue from previous result if it is valid
    combo_char =  leftover.valid == TRUE? leftover.chr: -1;
    combo_count = leftover.valid == TRUE? leftover.cnt: 0;
    // start actual compression
    for (int i = 0; i < bytes_read; i++)
    {
        // if this is not a continuation,
        // then initialze char on combo to be the 1st char on buffer
        if (i == 0 && !leftover.valid)
        {
            combo_char = buf[i];
            combo_count++;
        }
        else // we are on combo!
        {
            if (buf[i] == combo_char && combo_count <= MAX_COMBO) // combo!
                combo_count++;
            else // start a new combo
            {
                write_combo(combo_count, combo_char); // previous combo ends
                combo_char = buf[i];
                combo_count = 1;
            }
        }
    }
    continue_point_t current;
    current.cnt = combo_count;
    current.chr = combo_char;
    current.valid = TRUE;
    return current;
#else
// TODO: output buffer
#endif
}

void init(const char* infile, int* fd, char** buf, char** chars, count_t** counts)
{
    // open file
    *fd = open(infile, O_RDONLY);
    if (*fd < 0) // check if open() was successful
    {
        fprintf(stderr, "Cannot open file: %s\n", infile);
        exit(1);
    }

    // allocate a char buffer to hold input data
    // use malloc instead of array to support huge buffers.
    *buf = malloc(sizeof(char)*IN_BUF_SIZE);
    if (*buf == NULL) // check if malloc() was successful
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    // create output buffers
    #ifdef BUFFER_OUTPUT
    // allocate a char array to store char appeared
    *chars = malloc(sizeof(char)*OUT_BUF_SIZE);
    if (*chars == NULL) // check if malloc() was successful
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    // allocate a int array to store counts of characters appeared
    *counts = malloc(sizeof(count_t)*OUT_BUF_SIZE);
    if (*counts == NULL) // check if malloc() was successful
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    #endif
}

void clean(int fd, char* buf, char* chars, count_t* counts)
{
    close(fd);
    free(buf);
    #ifdef BUFFER_OUTPUT
        free(chars);
        free(counts);
    #endif
}

in_buffer_t read_to_buf(int fd, char** buf)
{
    unsigned long count = 0;
    in_buffer_t bytes_read;
    // read data into buffer
    bytes_read = read(fd, *buf, IN_BUF_SIZE);
    if (bytes_read < 0)    // error in read
    {
        fprintf(stderr, "read failed\n");
        exit(1);
    }
    else if (bytes_read > 0)
    {
        count += bytes_read;
    #ifdef PRINT_DATA
        for (in_buffer_t j = 0; j < IN_BUF_SIZE; j++)
        {
            printf("%c", *buf[j]);
        }
    #endif
    }
    // printf("Number of bytes read: %lu\n", count);
    return bytes_read;
}