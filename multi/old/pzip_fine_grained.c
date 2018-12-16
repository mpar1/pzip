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
#include <sys/sysinfo.h> // get_nprocs



/*********************************************************
 *                      Constants
 *********************************************************/

// constants. do not change these
#define MAX_COUNT (1 << 31) // 2^32 max count
#define UNIT_SIZE 5 // 5 bytes per compressed unit

// parameters
#define CHUNCK_SIZE (1024*1024) // 10MB chuncks
#define BYTES_PER_SLOT ((UNIT_SIZE) * (CHUNCK_SIZE))
// #define N_CPUS 3
#define SLOT_PER_CPU 1

/* output behaviors */
#define WRITE_TO_STD 0
#define OUT_FILE "./out"
// #define WRITE_TO_FILE 0 // write to a file instead of stdout


/* Slot state constants for the circular buffer */
#define SLOT_FREE 0     // default state
#define SLOT_COMPRESSED 1


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* for debugging */

#define print_flow do_nothing
#define print_pre do_nothing
#define print_write do_nothing
// #define ENABLE_PRINT_STATES 
// #define PRINT_BUFFER_ON_WRITE 0
// #define OUT_HUMAN_READABLE


/*********************************************************
 *                   Type Declarations
 *********************************************************/

typedef pthread_mutex_t lock_t;
typedef pthread_cond_t cond_t;

/* circular buffer */
typedef struct __buf_t {
    char* states;     // state can be one of {FREE, ASSIGN, COMPRESSED}
    char** data;      // actual data buffer
    cond_t* compressed; // C tells W that it finishes compressing its slot
    cond_t* freed;      // W tells C that the requested slot is free
    lock_t* locks;      // 1 lock per slot to control state and cond var
    int n_slots;       // # of slots
} buf_t;

/* args to C threads */
typedef struct __C_arg_t {
    char* file_start;
    char* file_end; // end of the input file
    buf_t* buf;      // buffer struct
    int id;          // between 0 and n_slots
    int compressors; // # of C threads
} C_arg_t;

/* args to W thread */
typedef struct __W_arg_t {
    int n_chuncks;
    buf_t* buf;
} W_arg_t;

/*********************************************************
 *                 Function Declarations
 *********************************************************/

void* C(void *arg);
void* W(void *arg);
void work(int fd);
uint64_t get_file_size(int fd);
void write_to_buf(uint32_t count, char ch, char* ptr);
void write_out(uint32_t count, char ch, FILE* out);
void exit_if(int boolean, const char* msg);
void print_states(char* states, int n_slots);
void do_nothing(const char* s, ...); // to substitute printf

char* data_base;
char* data_end;
char* base;

/*********************************************************
 *                          Main
 *********************************************************/

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
    /* unpack the args */
    C_arg_t a = *(C_arg_t *) arg;
    char* file_start = a.file_start;
    char* file_end = a.file_end;
    int id = a.id;
    int compressors = a.compressors;
    buf_t* buf = a.buf;
    int n_slots = buf->n_slots;

    int nth_chunck = a.id;
    char* input_ptr = file_start + id * CHUNCK_SIZE;
    while (input_ptr < file_end)
    {
        int slot = nth_chunck % n_slots; // current buffer slot
        // print_flow("%d  now reading %d-th chunck. %p : %p : %p\n", id, nth_chunck, input_ptr, input_ptr + CHUNCK_SIZE, input_end);
        // print_flow("%d  slot: %d\n", id, slot);
        
        /* wait until this buffer slot is free */
        pthread_mutex_lock ( &buf->locks[slot] );
        print_pre("before ");
        print_states(buf->states, n_slots);
        print_flow("%d  (%d, %d). wait  W.\n", id, nth_chunck, slot);
        while ( buf->states[slot] != SLOT_FREE )
        {
            pthread_cond_wait ( &buf->freed[slot], &(buf->locks[slot]) );
        }
        print_flow("%d  (%d, %d). begin C.\n", id, nth_chunck, slot);

        print_pre("after ");
        print_states(buf->states, n_slots);
        pthread_mutex_unlock ( &(buf->locks[slot]) );

        /* slot is free. begin compressing */
        char* slot_ptr = buf->data[slot]; // current pos in slot
        char* slot_end = slot_ptr + BYTES_PER_SLOT;
        char* chunck_end = MIN(input_ptr + CHUNCK_SIZE, file_end);
        print_flow("id:%d, nth_chunck: %d, input_ptr: %lu, slot_ptr: %lu/%lu\n",
        id, nth_chunck, (input_ptr - file_start), slot_ptr-data_base, data_end-data_base);

        /* compress a chunck of input data */
        madvise(input_ptr, CHUNCK_SIZE, MADV_SEQUENTIAL);
        char prev_ch = -1;
        uint32_t count = 0;

        while ( input_ptr < chunck_end )
        {
            char ch = *input_ptr; // read char
            input_ptr++;
            if ( count == 0 ) // initialize
            {
                prev_ch = ch;
                count = 1;
            }
            else if ( count < MAX_COUNT && ch == prev_ch ) // combo
            {   count += 1;   }
            else // counter full, or a different char
            {
                exit_if(slot_ptr >= data_end, "slot out of bound");
                
                write_to_buf (count, prev_ch, slot_ptr);
                slot_ptr += UNIT_SIZE;
                prev_ch = ch;
                count = 1;
            }
        }
        if ( count > 0 ) // last compressed unit hasn't been written to buf
        {
            write_to_buf (count, prev_ch, slot_ptr);
            slot_ptr += UNIT_SIZE;
        }
        // if slot is not full, write a (0,0) pair signaling EOF
        if ( slot_ptr < slot_end )
        {   write_to_buf (0, 0, slot_ptr);   }

        /* tell W that job is done */
        pthread_mutex_lock ( &(buf->locks[slot]) );
        print_pre("before ");
        print_states(buf->states, n_slots);
        buf->states[slot] = SLOT_COMPRESSED; // mark this slot COMPRESSED

        print_flow("%d  (%d, %d). done  C.\n", id, nth_chunck, slot);
        print_pre("after ");
        print_states(buf->states, n_slots);
        pthread_mutex_unlock ( &(buf->locks[slot]) ); // TODO: eliminate this unlock;
        pthread_cond_signal ( &buf->compressed[slot]);

        /* go to the next chunck */
        input_ptr += CHUNCK_SIZE * (compressors - 1);
        nth_chunck += compressors; 
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
    int n_chuncks = a.n_chuncks;
    buf_t* buf = a.buf;
    int n_slots = buf->n_slots;

    /* uncompress */
    int nth_chunck = 0;
    char last_ch = 0; // buffer a unit, in case combos onto the next chunck
    uint32_t last_count = 0;

    while ( nth_chunck < n_chuncks )
    {
        int slot = nth_chunck % n_slots;

        /* wait for C to compress this chunck */
        print_flow("-  <%d, %d>. wait  C.\n", nth_chunck, slot);
        pthread_mutex_lock ( &buf->locks[slot] );
        while ( buf->states[slot] != SLOT_COMPRESSED )
        {
            pthread_cond_wait ( &buf->compressed[slot], &buf->locks[slot] );
        }
        print_flow("-  <%d, %d>. begin W.\n", nth_chunck, slot);
        pthread_mutex_unlock ( &buf->locks[slot] );

        /* good to go */
        char* slot_ptr = buf->data[slot]; // current pos in slot
        char* slot_end = slot_ptr + BYTES_PER_SLOT;
        madvise(slot_ptr, BYTES_PER_SLOT, MADV_SEQUENTIAL);

        while ( slot_ptr < slot_end )
        {
            /* read the count and the ch */
            uint32_t count = *(uint32_t *) slot_ptr;
            slot_ptr += sizeof(uint32_t);
            char ch = *slot_ptr;
            slot_ptr++;
            if ( last_count == 0 ) // initialize
            {
                print_write("initialize\n");
                last_count = count;
                last_ch = ch;
            }
            else if (count == 0) // EOF marked by C, can break early
            {
                print_write("EOF\n");
                break;
            }
            else if (ch != last_ch) // write out if no longer on combo
            {
                print_write("no combo: %d%c\n", last_count, last_ch);
                write_out(last_count, last_ch, out);
                last_count = count;
                last_ch = ch;
            }
            else // combo continues!
            {
                uint64_t temp_count = last_count + count;
                if (temp_count > MAX_COUNT) // overflow
                {
                    print_write("combo but overflow\n");
                    write_out(MAX_COUNT, last_ch, out);
                    last_count = temp_count - MAX_COUNT;
                }
                else // combo and no overflow!
                {   
                    last_count = last_count + count;
                    print_write("combo: %d\n", last_count);
                }
            }
        }

        /* tell C that this slot is free */
        pthread_mutex_lock ( &buf->locks[slot] );
        buf->states[slot] = SLOT_FREE; // mark this slot COMPRESSED
        pthread_mutex_unlock ( &buf->locks[slot] );
        pthread_cond_signal ( &buf->freed[slot] );
        print_flow("-  <%d, %d>. done  W.\n", nth_chunck, slot);

        nth_chunck++; // move on to next chunck (and slot)
    }
    
    if ( last_count != 0 )
    {   write_out(last_count, last_ch, out); } // write out the very last unit
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

    double n_chuncks = ceil (file_size * 1.0 / CHUNCK_SIZE);
    print_flow("file size %lu, chunck size: %d, div: %d\n", file_size, CHUNCK_SIZE, n_chuncks);

    /* create a circular buffer */
    buf_t buf;

    int cpu_cores; // scale # of slots according to # of cpu cores
    cpu_cores = 
    #ifdef N_CPUS // if we have specified the parameter, use that
        N_CPUS;
    #else
        get_nprocs();
    #endif

    int n_slots = (int) ((cpu_cores-1) * SLOT_PER_CPU); // 1 core reserved for W
    buf.n_slots = n_slots;
    
    buf.data = malloc(n_slots * sizeof(char*)); // slot ptrs
    for (int i = 0; i < n_slots; i++)
    {   
        buf.data[i] = malloc(BYTES_PER_SLOT); // slot memory
        exit_if(buf.data[i] == NULL, "cannot allocate buffer slots");
    }
    
    buf.states = calloc(n_slots, sizeof(char)); // each slot has a state
        exit_if(buf.states == NULL, "cannot allocate states buffer");
    buf.locks = malloc(n_slots * sizeof(lock_t));
        exit_if(buf.locks == NULL, "cannot allocate locks");
    buf.compressed = malloc(n_slots * sizeof(cond_t));
        exit_if(buf.compressed == NULL, "cannot allocate compressed (cond var) array");
    buf.freed = malloc(n_slots * sizeof(cond_t));
        exit_if(buf.freed == NULL, "cannot allocate freed (cond var) array");

    for (int i = 0; i < n_slots; i++) // initialize cond vars
    {
        buf.locks[i] = (lock_t) PTHREAD_MUTEX_INITIALIZER;
        buf.compressed[i] = (cond_t) PTHREAD_COND_INITIALIZER;
        buf.freed[i] = (cond_t) PTHREAD_COND_INITIALIZER;
    }

    // DEBUG start
    base = MIN((char*) buf.data, (char*) buf.states);
    base = MIN(base, (char*) buf.locks);
    base = MIN(base, (char*) buf.compressed);
    base = MIN(base, (char*) buf.freed);
    data_base = buf.data[0];
    data_end = buf.data[0];

    print_flow("data: %lu\tstates: %lu\n", (char*)buf.data-base, (char*) buf.states-base);
    print_flow("locks: %lu\tcompressed: %lu\tfreed: %lu\n", (char*) buf.locks-base, (char*) buf.compressed-base, (char*) buf.freed-base);
    
    for (int i = 0; i < n_slots; i++)
    {
        data_base = MIN(data_base, buf.data[i]);
        data_end = MAX(data_end, buf.data[i]);
    }
    data_end += BYTES_PER_SLOT;
    for (int i = 0; i < n_slots; i++)
    {
        print_flow("data[%d]: %lu\n", i, buf.data[i]-data_base);
    }
    print_flow("data ends at: %lu\n", data_end-data_base);
    // DEBUG end

    /* create compressor threads */
    int compressors = cpu_cores - 1; // since W occupies a cpu core
    pthread_t c_threads[compressors];
    C_arg_t cargs[compressors];
    for (int i = 0; i < compressors; i++)
    {
        /* args for C */
        cargs[i].file_start = start_addr;
        cargs[i].file_end = start_addr + file_size;
        cargs[i].id = i;
        cargs[i].compressors = compressors;
        cargs[i].buf = &buf;
    
        rc = pthread_create(&c_threads[i], NULL, C, &cargs[i]);
        exit_if(rc != 0, "cannot create compressor thread");
    }

    /* create a W thread on standby */
    pthread_t w;

    W_arg_t wargs; // args to W
    wargs.buf = &buf;
    wargs.n_chuncks = (int) n_chuncks;
    rc = pthread_create(&w, NULL, W, &wargs);
    exit_if(rc != 0, "cannot create writer thread");


    /* wait for the W thread to finish */
    pthread_join(w, (void **) &rc); // this implies that the C threads have finished
    // for (int i = 0; i < compressors; i++)
    // {
    //     pthread_join(c_threads[i], (void **) &rc);
    //     print_flow("Compressor %d done\n", i);
    // }

    /* cleaning */
    rc = munmap(start_addr, file_size); // un-mmap the file
        exit_if(rc != 0, "cannot un-mmap input file");
        print_flow("munmap'ed\n");
    for (int i = 0; i < n_slots; i++)
    {   free(buf.data[i]);  }
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
        printf("%d", count);
        printf("%c", ch);
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
    if (boolean) {
        fprintf(stderr, "%s", msg);
        exit(1);
    }
}

void print_states(char* states, int n_slots)
{
#ifdef ENABLE_PRINT_STATES
    printf("states:[");
    for (int i = 0; i < n_slots; i++)
    {
        switch (states[i])
        {
            case SLOT_COMPRESSED: printf("c, "); break;
            case SLOT_FREE: printf("f, "); break;
            default: exit_if(1, "no such state");
        }
        
    }
    printf("]\n");
#endif
}

void do_nothing(const char* s, ...)
{}