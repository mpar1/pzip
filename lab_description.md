# Parallel Zip

Created and written by Professor Waterman and Griffin. [Link](https://cs334.cs.vassar.edu/assignments/pziplab/).

**Assigned**: Thursday, November 29th  
**Checkpoint**: Thursday, December 6th 11:59 PM **Due**: Friday, December 14th 11:59 PM

**Note**: This assignment may be done with one other partner, or may be done individually.

### About this lab

For the first part of this project, you will get practice building a small Linux compression utility, my-zip. Once you get a working version of this program, you will then use threads to implement a parallel version of your zip utility, (parallel zip).

Objectives:

*   Re-familiarize yourself with the C programming language.
*   Re-familiarize yourself with the shell / terminal / Linux command line.
*   Learn a little about how Linux utilities are implemented.
*   Familiarize yourself with the Linux pthreads.
*   Learn how to parallelize a program.
*   Learn how to program for high performance.

### Getting the starter code

You can get and extract the starter code with the commands below.

    cd cs334
    wget https://cs334.cs.vassar.edu/lab_materials/pziplab/pziplab.tar
    tar xf pziplab.tar
    cd pziplab

## Part 1 : Serial Run-Length Encoding

For the first part of this project you will implement a simple compression utility named `my_zip` and it’s counterpart `my_unzip`. Each takes a single command line argument – the name of the file – and writes the compressed/uncompressed version to standard out.

The first of these, `my_zip`, will compress a given input file via run-length encoding (RLE). RLE is a straightforward compression algorithm: when you encounter a block of **n** consecutive characters of the same type, `my_zip` turns the block (often called a run) into a single 5 byte entry containing the number **n** followed by the character.

For example

    aaaaaaaaaaaaaaaaaaaaaaaaaaaaa

would turn into the following (logically)

    30a

A file compressed by `my_zip` is a sequence of these 5-byte entries (why five bytes?) where each entry corresponds to a run in the original file. The run length must be written to stdout in its _binary format_ while the character remains as its ASCII representation. You should use `fwrite()` to accomplish this. Since the file is compressed to standard out, you’ll need to redirect the output of `my_zip` to a file in order to save the zipped version.

    csstudent@computer $ ./my_zip file.txt > file.z

`my_unzip` can then reverse the process by reading in the zipped file with `fread()` and then printing the uncompressed version to standard out with `printf()`.

Requirements:

*   Your zipped file _must_ be in the described format. You may assume that `my_unzip` is given properly formatted input. (What would happen if it wasn’t?)
*   Both `my_zip` and `my_unzip` must take a single command line argument and should print appropriate error messages if too many or too few arguments are given.
*   Similarly, if you get an error trying to open the file (e.g., if the given file does not exist), your program should exit with appropriate error messages.
*   All resources that are opened or allocated must be properly closed or freed before the program exits.

Things to consider:

*   You’ll be working with several unfamiliar function calls. Visit the man pages for more information about how each of them works.
*   There are multiple ways of opening and reading from a file, which makes the most sense for each scenario?
*   How are you reading the file in? Can your tool zip a file larger than main memory? What do you need to do to accomplish this?
*   What system calls do you need to make? Can they fail?
*   What is the best case scenario for this sort of compression? The worst case? What are their file sizes with respect to the original?
*   How can you check if your compressed file decompresses to the original? What Linux utility might be helpful?

## Part 2: Parallel Run-Length Encoding

By now you should have a serialized version of the zip tool working and an unzipping tool. These are both perfectly functional, however, they aren’t fast and with the death of Moore’s Law, we can’t hope for processor advances to save us. That means you’ll need to figure out a way to parallelize this task.

While conceptually straightforward – it’s `my_zip` but parallel – there are some serious design considerations that go into parallelization. Because of this, you’ll want to plan your design carefully.

### Considerations

Implementing your `pzip` effectively and with high performance will require you to address (at least) the following issues:

*   **How to parallelize the compression.** Of course, the central challenge of this project is to parallelize the compression process. Think about what can be done in parallel, and what must be done serially by a single thread, and design your parallel zip as appropriate.

    One interesting issue that the “best” implementations will handle is this: what happens if one thread runs more slowly than another? Does the compression give more work to faster threads?

*   **How to determine how many threads to create.** On Linux, this means using interfaces like `get_nprocs()` and `get_nprocs_conf()`; read the man pages for more details. Then, create threads to match the number of CPU resources available.

*   **How to efficiently perform each piece of work**. While parallelization will yield speed up, each thread’s efficiency in performing the compression is also of critical importance. Thus, making the core compression loop as CPU efficient as possible is needed for high performance.

*   **How to access the input file efficiently**. On Linux, there are many ways to read from a file, including C standard library calls like `fread()` and raw system calls like `read()`. One particularly efficient way is to use memory-mapped files, available via `mmap()`. By mapping the input file into the address space, you can then access bytes of the input file via pointers and do so quite efficiently.

### Grading

Your code will first be measured for correctness, ensuring that it zips input files correctly.

If you pass the correctness tests, your code will be tested for performance; higher performance will lead to better scores.

Grading percentages break down as follows:

*   20% my-zip implementation correctness
*   10% my-zip coding style
*   20% pzip design documentation
*   30% pzip correctness
*   10% pzip coding style
*   10% pzip performance
