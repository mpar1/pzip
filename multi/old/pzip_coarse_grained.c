/*********************************************************
 * Your Name:    Junrui Liu
 * Partner Name: Shihan Zhao
 *********************************************************/

#include <stdio.h>
#include <stdint.h> // portable int types
#include <fcntl.h> // open
#include <unistd.h> // close
#include <assert.h> // assert
#include <stdlib.h> // exit
#include <sys/stat.h> // file stat
#include <sys/mman.h> // mmap
#include <pthread.h> // pthreads
#include <math.h>   // ceil
#include <sys/mman.h> // madvise 
// #include <sys/sysinfo.h> // get_nprocs


// constants. do not change these
#define MAX_COUNT (0x80000000) // 2^32 max count
#define UNIT_SIZE 5 // 5 bytes per compressed unit

// parameters
#define CHUNCK_SIZE (2) // 1MB chuncks
#define N_SLOTS (3)
#define BYTES_PER_SLOT ((UNIT_SIZE) * (CHUNCK_SIZE))
#define N_CPUS 3

// output
// #define PRINT_BUFFER_ON_WRITE 0
// #define OUT_HUMAN_READABLE
#define OUT_FILE "./zip_out.z"
// #define WRITE_TO_STD 0
#define WRITE_TO_FILE 0 // write to a file instead of stdout
/* SLOTs for the circular buffer */
#define SLOT_FREE 0     // default state
#define SLOT_ASSIGNED 1
#define SLOT_COMPRESSED 2


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define print_flow printf

typedef pthread_mutex_t lock_t;
typedef pthread_cond_t cond_t;

typedef struct __buf_t {
    char* states;     // state can be {FREE, ASSIGN, COMPRESSED}
    char* data;      // actual data buffer. initialized once and never modified
    cond_t* compressed; // C tells W that it finishes its slot
    cond_t* freed;      // W tells C that the requested slot is free
    lock_t lock;  // one state lock per buffer slot
} buf_t;

typedef struct __C_arg_t {
    char* input_ptr;
    char* input_end;
    int id;
    buf_t* buf;
} C_arg_t;

typedef struct __W_arg_t {
    int last_chunck;
    buf_t* buf;
} W_arg_t;

void work(int fd);
uint64_t get_file_size(int fd);
void write_to_buf(uint32_t count, char ch, char* ptr);
void write_out(uint32_t count, char ch, FILE* out);
void exit_if(int boolean, const char* msg);
void* C(void *arg);
void* W(void *arg);

void do_nothing(const char* s, ...);
void print_states(char* states);

int main(int argc, const char* argv[])
{
    // take in a file name
	exit_if(argc != 2, "please specify one and only one input file");
	const char* filename = argv[1];
    
    // open the file
	int fd = open(filename, O_RDONLY);
	exit_if(fd < 0, "cannot open input file");

	// actual work
	work(fd);

	// close the file
    int rc = close(fd);
    exit_if(rc != 0, "cannot close input file");
}

void* C(void *arg)
{
    
    C_arg_t* a = (C_arg_t *) arg;
    char* input_ptr = a->input_ptr;
    char* input_end = a->input_end;
    int id = a->id;
    int nth_chunck = id;
    buf_t* buf = a->buf;
    char* slot_ptr, *slot_end, *chunck_end;
    
    
    while (input_ptr < input_end)
    {
        int slot = nth_chunck % N_SLOTS; // current buffer slot
        // print_flow("%d  now reading %d-th chunck. %p : %p : %p\n", id, nth_chunck, input_ptr, input_ptr + CHUNCK_SIZE, input_end);
        // print_flow("%d  slot: %d\n", id, slot);
        // wait until this buffer slot is free
        pthread_mutex_lock ( &buf->lock );
        printf("before: ");
        print_states(buf->states);
        print_flow("%d  begin. chunck: %d, slot: %d. wait to be free -> ", id, nth_chunck, slot);
        while ( buf->states[slot] != SLOT_FREE )
        {
            pthread_cond_wait ( &buf->freed[slot], &(buf->lock) );
        }
        print_flow("free -> ");
        buf->states[slot] = SLOT_ASSIGNED; // mark this slot ASSIGNED

        print_flow("assigned. Begin compressing\n");
        printf("after: ");
        print_states(buf->states);
        pthread_mutex_unlock ( &(buf->lock) );

        // slot is free. begin compressing
        slot_ptr = buf->data + BYTES_PER_SLOT * slot; // current pos in slot
        slot_end = slot_ptr + BYTES_PER_SLOT;
        chunck_end = MIN(input_ptr + CHUNCK_SIZE, input_end);
        

        // compress a chunck of input data
        madvise(input_ptr, CHUNCK_SIZE, MADV_SEQUENTIAL);
        char prev_ch = -1;
        uint32_t count = 0;

        while ( input_ptr < chunck_end )
        {
            char ch = *input_ptr; // read char
            input_ptr++;

            // compress
            if ( count == 0 ) // initialize
            {
                prev_ch = ch;
                count = 1;
            }
            else if ( count < MAX_COUNT && ch == prev_ch ) // combo
            {   count += 1;   }
            else // counter full, or new char
            {
                write_to_buf (count, prev_ch, slot_ptr);
                slot_ptr += UNIT_SIZE;
                prev_ch = ch;
                count = 1;
            }
        }
        if ( count > 0 ) // last compressed unit hasn't been freed out
        {
            write_to_buf (count, prev_ch, slot_ptr);
            slot_ptr += UNIT_SIZE;
        }
        // if slot is not full, write a (0,0) pair signaling EOF
        if ( slot_ptr < slot_end )
        {   write_to_buf (0, 0, slot_ptr);   }

        // print_flow("%d  chunck done: %p\n", id, input_ptr);
        input_ptr += CHUNCK_SIZE * (N_CPUS - 2);
        // print_flow("%d  next chunck: %p\n", id, input_ptr);
        nth_chunck += (N_CPUS - 1);

        pthread_mutex_lock ( &(buf->lock) );
        printf("before: ");
        print_states(buf->states);
        buf->states[slot] = SLOT_COMPRESSED; // mark this slot COMPRESSED

        print_flow("%d  done.  chunck: %d, slot: %d. signals for compression\n", id, nth_chunck, slot);
        printf("after: ");
        print_states(buf->states);
        pthread_mutex_unlock ( &(buf->lock) ); // TODO: eliminate this unlock;
        pthread_cond_signal ( &buf->compressed[slot]);
    }
    print_flow("%d  return.\n", id);
    return 0;
}

void* W(void *arg)
{
    print_flow("-\n");
    FILE* out;
    #ifdef WRITE_TO_FILE
        out = fopen(OUT_FILE, "w");
        assert(out != NULL);
    #else
        out = stdout;
    #endif
    
    /* unpack the args */
    W_arg_t a = *(W_arg_t *) arg;
    int last_chunck = a.last_chunck;
    buf_t* buf = a.buf;

    /* uncompress */
    int nth_chunck = 0;
    char last_ch = 0;
    uint32_t last_count = 0;
    while ( nth_chunck < last_chunck ) // TODO: <= ?
    {
        int slot = nth_chunck % N_SLOTS;

        pthread_mutex_lock ( &buf->lock );
        print_flow("-  begin. chunck: %d/%d, slot: %d. wait to be compressed -> ", nth_chunck, last_chunck, slot);
        while ( buf->states[slot] != SLOT_COMPRESSED )
        {
            pthread_cond_wait ( &buf->compressed[slot], &buf->lock );
        }
        print_flow("compressed. Begin writing\n");
        pthread_mutex_unlock ( &buf->lock );

        
        char* slot_ptr = buf->data + BYTES_PER_SLOT * slot; // current pos in slot
        char* slot_end = slot_ptr + BYTES_PER_SLOT;
        // printf("\nslot #%d\n", slot);
        madvise(slot_ptr, BYTES_PER_SLOT, MADV_SEQUENTIAL);
        while ( slot_ptr < slot_end )
        {
            uint32_t count = *(uint32_t *) slot_ptr;
            slot_ptr += sizeof(uint32_t);
            char ch = *slot_ptr;
            slot_ptr++;
            if ( last_count == 0 ) // initialize
            {
                // printf("initialize\n");
                last_count = count;
                last_ch = ch;
            }
            else if (count == 0) // EOF
            {
                // printf("EOF\n");
                break;
            }
            // write out the continuation if no longer combo 
            else if (ch != last_ch)
            {
                // printf("no combo: %d%c\n", last_count, last_ch);
                write_out(last_count, last_ch, out);
                last_count = count;
                last_ch = ch;
            }
            // combo continues!
            else
            {
                uint64_t temp_count = last_count + count;
                if (temp_count > MAX_COUNT) // overflow
                {
                    // printf("combo but overflow\n");
                    write_out(MAX_COUNT, last_ch, out);
                    last_count = temp_count - MAX_COUNT;
                }
                else
                {   
                    
                    last_count = last_count + count;
                    // printf("combo: %d\n", last_count);
                }
            }
        }

        // print_flow("-  done writing chunck %d\n", nth_chunck);
        nth_chunck++;

        pthread_mutex_lock ( &buf->lock );
        buf->states[slot] = SLOT_FREE; // mark this slot COMPRESSED
        pthread_cond_signal ( &buf->freed[slot] );
        print_flow("-  done.  chunck: %d/%d, slot: %d. marked free and signaled\n", nth_chunck, last_chunck, slot);
        pthread_mutex_unlock ( &buf->lock );
    }
    
    if ( last_count != 0 )
    {   write_out(last_count, last_ch, out); } // write out the last unit
    print_flow("-  return.\n");
    
    #ifdef WRITE_TO_FILE
        int rc = fclose(out);
        exit_if(rc != 0, "cannot close output fd");
    #endif
    return 0;
}

void work(int fd)
{
    int rc = -1; // for return codes

    /* mmap the file */
    uint64_t file_size = get_file_size(fd);
	char* start_addr = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    assert(start_addr != MAP_FAILED);

    /* create a circular buffer */
    buf_t buf;
    
    buf.data = malloc(BYTES_PER_SLOT * N_SLOTS); // allocate data buffer
    exit_if(buf.data == NULL, "cannot allocate data buffer");
    
    // buf.states[N_SLOTS];
    buf.states = calloc(N_SLOTS, sizeof(char)); // each buffer slot has a state
    // buf.states = malloc(N_SLOTS * sizeof(char)); // each buffer slot has a state
    for (int i = 0; i < N_SLOTS; i++)
    {   printf("%d is %d\n", i, (int) *(buf.states + i)); }
    exit_if(buf.states == NULL, "cannot allocate states buffer");
    
    buf.lock = (lock_t) PTHREAD_MUTEX_INITIALIZER;
    exit_if(buf.data == NULL, "cannot allocate lock");
    buf.compressed = malloc(N_SLOTS * sizeof(cond_t));
    buf.freed = malloc(N_SLOTS * sizeof(cond_t));
    exit_if(buf.data == NULL, "cannot allocate cond vars array");
    for (int i = 0; i < N_SLOTS; i++) // initialize cond vars
    {
        buf.compressed[i] = (cond_t) PTHREAD_COND_INITIALIZER;
        buf.freed[i] = (cond_t) PTHREAD_COND_INITIALIZER;
        // pthread_cond_signal(&buf.compressed[i]);
        // pthread_cond_signal(&buf.freed[i]);
    }
    // int free_cpu = get_nprocs(); // # of CPUs
    int free_cpu = N_CPUS;

    pthread_t w;
    /* create a W thread on standby */
    W_arg_t wargs;
    wargs.buf = &buf;
    double num_chuncks = ceil (file_size * 1.0 / CHUNCK_SIZE);
    printf("file size %lu, chunck size: %d, div: %f\n", file_size, CHUNCK_SIZE, num_chuncks);
    wargs.last_chunck = (int)( num_chuncks );
    printf("div: %d\n", wargs.last_chunck);
    
    rc = pthread_create(&w, NULL, W, &wargs);
    exit_if(rc != 0, "cannot create writer thread");

    int n_compressors = free_cpu - 1; // W occupies a cpu

    /* create (N-1) C threads, where N = # of CPUs */
    pthread_t compressors[n_compressors];
    C_arg_t cargs[n_compressors];
    for (int i = 0; i < n_compressors; i++)
    {
        /* args for C */
        cargs[i].input_ptr = start_addr + i * CHUNCK_SIZE;
        cargs[i].input_end = start_addr + file_size;
        cargs[i].id = i;
        cargs[i].buf = &buf;

        rc = pthread_create(&compressors[i], NULL, C, &cargs[i]);
        exit_if(rc != 0, "cannot create compressor thread");
    }
    /* wait for the W thread to finish */
    pthread_join(w, (void **) &rc); // this implies that the C threads have finished
    for (int i = 0; i < n_compressors; i++)
    {
        
        pthread_join(compressors[i], (void **) &rc);
        print_flow("Compressor %d done\n", i);
    }
    rc = munmap(start_addr, file_size); // un-mmap the file
    exit_if(rc != 0, "cannot un-mmap input file");
    print_flow("munmap'ed\n");
    /* cleaning */
    free(buf.data);
    print_flow("data freed\n");
    free(buf.compressed);
    print_flow("compressed freed\n");
    free(buf.freed);
    print_flow("freed freed\n");
    free(buf.states);
    print_flow("states freed\n");
}

uint64_t get_file_size(int fd)
{
	struct stat stat_buf;
    fstat(fd, &stat_buf);
    return stat_buf.st_size;
}

void write_to_buf(uint32_t count, char ch, char* ptr)
{
    #ifdef PRINT_BUFFER_ON_WRITE
        printf("%d", count);
        printf("%c", ch);
    #endif
        *(uint32_t *) ptr = count;
        ptr += sizeof(uint32_t);
        *ptr = ch;
}

void write_out(uint32_t count, char ch, FILE* out)
{
    #ifdef OUT_HUMAN_READABLE
        printf("> %d", count);
        printf("%c\n", ch);
    #endif
    #ifdef WRITE_TO_STD
        fwrite(&count, sizeof(uint32_t), 1, stdout);
        fwrite(&ch, sizeof(char), 1, stdout);
    #endif
    #ifdef WRITE_TO_FILE
        fwrite(&count, sizeof(uint32_t), 1, out);
        fwrite(&ch, sizeof(char), 1, out);
    #endif
}

void exit_if(int boolean, const char* msg)
{
    if (boolean)
    {
        fprintf(stderr, "%s", msg);
        exit(1);
    }
}

void do_nothing(const char* s, ...)
{

}

void print_states(char* states)
{
    printf("[");
    for (int i = 0; i < N_SLOTS; i++)
    {
        printf("%d:%d, ", i, states[i]);
    }
    printf("]\n");
}