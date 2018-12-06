#include <stdio.h>
#include <stdint.h> // portable int types
#include <assert.h> // assert
#include <stdlib.h> // exit


/* Wrapper functions around the standard IO routines */
FILE* Fopen(const char* filename);
size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
void Fclose(FILE* stream);

/* decompress the characters from a given input file stream */
void decompress(FILE* infile);

int main( int argc, const char* argv[] )
{
    // take in a file name
	assert(argc == 2);
	const char* filename = argv[1];
    // open the file
    FILE* infile = Fopen(filename);
    // actual work
    decompress(infile);
    // close the file
    Fclose(infile);
}

void decompress(FILE* infile)
{
    uint32_t count;
    unsigned char ch;
    size_t rc;
    while (1)
    {
        // read count
        rc = Fread(&count, sizeof(uint32_t), 1, infile);
        if (rc == 0) break; // EOF
        assert(rc == 1);
        // read char
        rc = Fread(&ch, sizeof(unsigned char), 1, infile);
        assert(rc == 1);
        // decompress
        for (uint32_t i = 0; i < count; i++)
            printf("%c", ch);
    }
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