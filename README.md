# jsqlite
json Wrapper for libsqlite3.

This repository is a json wrapper for libsqlite3.

The easy way of using it is, clone this repository then run the example program.

# Requirements
- libsqlite3
- libjansson

# Example
<pre><code>
$ gcc -c jsqlite3.c
$ cd example
$ gcc -o simple_example simple_example.c ../jsqlite3.o -lsqlite3 -ljansson
$ $ valgrind --tool=memcheck --leak-check=full ./simple_example 

==10670== Memcheck, a memory error detector
==10670== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
==10670== Using Valgrind-3.11.0 and LibVEX; rerun with -h for copyright info
==10670== Command: ./simple_example
==10670== 
Finished db_init.
Insert data. res[{"id": "3dd36215-2393-4a98-8bd5-aafff809af91", "type": 1, "data": {"name": "pchero", "message": "hello world"}}]
Result. res[{"id": "3dd36215-2393-4a98-8bd5-aafff809af91", "type": 1, "data": {"name": "pchero", "message": "hello world"}}]
==10670== 
==10670== HEAP SUMMARY:
==10670==     in use at exit: 0 bytes in 0 blocks
==10670==   total heap usage: 566 allocs, 566 frees, 247,065 bytes allocated
==10670== 
==10670== All heap blocks were freed -- no leaks are possible
==10670== 
==10670== For counts of detected and suppressed errors, rerun with: -v
==10670== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
</code></pre>
