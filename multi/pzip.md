# pzip ideas

This is just my random thoughts on this lab. The first two approaches certainly won't work at all; I'm thinking about the third one (N+2).

## 2 threads: 1 compressor, 1 writer

Pros:
- fwrite() is a blocking call. It takes time to write data onto the internal buffer.
- Simple implementation.
    - Sequential read: when a unit is full, pass it to writer thread (how? through buffer? defeats the purpose!), possibly through a concurrent queue.
    - Sequential write: write when there are pending units on the queue.

Cons:
- May not see any performance improvement at all, because fwrite() writes to an internal buffer so it wouldn't block for a long time.


## N+1 threads: N compressors, 1 writer (two-pass)

Two passes: first pass to read & compress to buffer, second pass to write from buffer to output.

Read pass can be completely parallel.

Assign a chunck of input file to each thread.

- How to determine how large the chunck should be?
    - What syscall helps determine the file size
    - What parameter value should we choose for chunck size?
- Trade-off: larger chuncks --> finish time differ more, but should not see too much variation. (But this will matter in the single-pass variant?)

How to manage intermedite buffer? If we know the file size & chunck size, then it is straightforward to create a fixed size array (whose length = number of chuncks) of linked list (since we cannot know how many compressed units are in each chunck).

The writer walks down the array (& the linked lists), performs coalescing if necessary (when it gets to the tail of a linked list and can grab the head of the next one), and writes the coalesed units.
<!-- 
*Update:* A linked-list implementation requires huge amount of calls to malloc(), which is a blocking syscall.
Also there is little reason to use LL since we won't be performing any insert and delete (the coalescing can be done by the writer just before the write to stdout.)
What's worse, malloc() is thread safe (internally locked), so may incur huge amount of overhead. Not ideal for multi-threaded programs?

- Solution: allocate an array with length = chunck size, limited by memory constraint. Basically assume the worst case (all chars are distinct).
- Likely will lead to internal fragmentation
    - Question: Can we reuse the wasted space? e.g. by buffering writes on an array from both ends by two different threads, protected by a head lock and a tail lock? Or use a dynamically resized array?
- but only need to allocate N arrays once, and can reuse them later. No need for malloc() for every node.
- Subtlety: when **D** is signaled of the completion of a chunck, it cannot just allocate the array back to a new thread, since this array has not yet been coalesced and written out. But probabily this doesn't matter, assuming writes are slower than compressions. -->

Pros:
- First pass (read) can be fully parallelized.

Cons:
- Still require a second write pass.
- Worst case: all characters in the input file are distinct (no compression possible), then the time spent on the first pass will be completely wasted. Because of this, the single-threaded version is likely to outperform this approach since the bottleneck *is* the write. 

## ! N+2 threads: N compressors, 1 dispatcher, 1 writer (single-pass)

Idea is to always keep the writer busy. Whenever a compressor finishes its job, let the writer starts writing immediately.

Decide a chunck size based on available main memory via e.g. sysinfo(). Suppose the chuncks are numbered: 0 .. N-1.
Dispatch each chunck to a thread.
Keep track of whether a chunck is assigned / compressed / written.

```
(2 CPUs:)
Chunckes   0 1 2 3 ... N-1
Dispatch   Y Y Y N ...  .   (locked)
Compressed N Y N N ...  .   (locked)
Written    N N N N ...  .   (locked)
Data buf (only 2, one per C thread)
```
**M** Main thread
- creates a **D** thread (**D** thread can just be the main thread) and a **W** thread,
- waits for **W** thread to complete (i.e. all data are compressed and all writes are complete).

**D** Dispatcher thread (only 1)
- allocate N arrays of (char, count) pairs of chunck size (only once)
- dispatches chuncks (and an array) to **C** threads together with allocated buffers, and wait for **C** threads to complete,
- when signaled of a completion by a **C**, dispatches the next chunck to a new **C** threads (**C** thread creation) until there are no free CPU cores, and wait for **W** threads to free up buffers
- free the arrays at the end

**C** Compressor threads (# of CPUs)
- compresses data,
- store the compressed data (int, char) pair in the designated array (could be struct, but padding space would be wasted?)
- send a signal to **D** and **W** when done.

**W** Writer thread (only 1)
- wait for (any) **C** thread completion signal, and checks if the next chunck is compressed.
    - if not, goes back to sleep (hopefully this will never happen, or happen only at the very beginning);
    - else, coaleces compressed data created by **C** threads, and write them out
    - Goes back to sleep if the next chunck is not compressed yet (hopefully this will never happen).
- If everything has been written out, sends a signal to **M** and returns.


## ? N+M threads: N compressors, M writers

It is even possible to have multiple writers? No. Writes can only be performed serially, as later compressed data can depend on what comes before it.