gctest
======

A simple mark and sweep garbage collector.

This is a very simple (but not too inefficient) toy garbage collector, based on the mark and sweep algorithm.
It supports three types: Objects, Arrays and 32-bit Integers.
The type names follow a Java-like convention at this point: [I means "array of integers", [Lfoo.bar; means "array of objects of type foo.bar".
This GC supports inheritance.

The program must be single-threaded.

Any value in the stack is treated as a GC root, even if it's not a pointer.
The GC is only run on allocation. If there's no lack of memory, then the GC will never run.

The "gc_heap" are pages of heap memory that are used to store the objects, arrays and array contents.
