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

Description
===========

A (not-so-brief) description of this project can be seen at http://luiscubal.blogspot.pt/2012/11/a-simple-mark-garbage-collector.html

The code is licensed under the MIT license.

Copyright (C) 2012 Luís Reis <luiscubal@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
