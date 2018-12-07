- How to parallelize the compression. Of course, the central challenge of this project is to parallelize the compression process. Think about what can be done in parallel, and what must be done serially by a single thread, and design your parallel zip as appropriate.
<<<<<<< HEAD
    - The main thread **M** divides the input file into fixed-size chuncks, and dispatch those chuncks sequentially to the Compressor threads (denoted by **C**), which run in parallel. The compressed data are stored in reusable buffers.
    - When a **C** thread finishes compressing its chuncks, it signals the Writer thread (denoted by **W**).
    - The **W** thread writes the buffered data (if any) to stdout. When a write completes, the **W** thread notifies the **M** thread that a buffer is now free so that **M** can dispatch the next chunck to a new **C** thread. Asuuming that the **W** thread is always slower than the **C** threads, the writer will only be idle when the very first chunck is being processed by a **C** thread.

- One interesting issue that the “best” implementations will handle is this: what happens if one thread runs more slowly than another? Does the compression give more work to faster threads?
=======
    - The main thread (**M**) dispatches chuncks of the input file sequentially to the Compressor threads (**C**), which run in parallel. The compressed data are stored in a reusable circular buffer.
    - When a **C** thread finishes compressing its chuncks, it signals the Writer thread (**W**) that this portion of the buffer can be written to stdout.
    - The **W** thread writes the buffered data (if any) to stdout. When a write completes, the **W** thread notifies the **M** thread that a buffer is now free so that **M** can dispatch the next chunck to a new **C** thread.

- One interesting issue that the “best” implementations will handle is this: what happens if one thread runs more slowly than another? Does the compression give more work to faster threads?
    - We have tried approaches such as dividing the chuncks into 
>>>>>>> modified design doc

- How to determine how many threads to create. On Linux, this means using interfaces like get_nprocs() and get_nprocs_conf(); read the man pages for more details. Then, create threads to match the number of CPU resources available.
    - Since the bottleneck is writing out compressed data, we create 1 **W** thread and N-1 **C** threads, where N is the number of CPU cores (assuming all cores are available).


- How to efficiently perform each piece of work. While parallelization will yield speed up, each thread’s efficiency in performing the compression is also of critical importance. Thus, making the core compression loop as CPU efficient as possible is needed for high performance.
    - Keep the inner loop as compact as possible. (Look at the assembly with Og flag after done.)
    - Let the compiler do loop unrolling?

- How to access the input file efficiently. On Linux, there are many ways to read from a file, including C standard library calls like fread() and raw system calls like read(). One particularly efficient way is to use memory-mapped files, available via mmap(). By mapping the input file into the address space, you can then access bytes of the input file via pointers and do so quite efficiently.
    - The **M** thread calls mmap() to memory-map the entire file to some memory location (with parameters PROT_READ and MAP_SHARED). When it dispatches a chunck to a **C** thread, it tells **C** what the starting address and the chunck size are.
    - The **C** thread calls madvise(MADV_SEQUENTIAL) to tell the OS that subsequent reads will be strictly sequential. Then it reads the input file as if it accesses the memory.