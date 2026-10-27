# Processes and Threads

**Language:** C
**Environment:** Linux, POSIX threads

## Overview

An Operating Systems laboratory project focused on
process creation, multithreading, and synchronization
in Linux.

## Key Concepts

- Creating processes using `fork()`
- Creating and managing POSIX threads
- Synchronizing thread execution and enforcing ordering
  constraints
- Controlling the number of concurrently running threads
- Coordinating execution across processes

## Implementation

The program implements a predefined process hierarchy
and synchronization requirements specified by the
laboratory assignment.

It uses semaphores and mutexes to coordinate threads,
including constraints on execution order and the number
of simultaneously running threads.

## Source Code

See `a2.c`.

Note: The laboratory supplied helper files used for
evaluation; these are not included in this portfolio copy.
The source depends on the supplied `a2_helper.h` interface.

## Compilation

When the course-supplied helper files are available:

    gcc -Wall a2.c a2_helper.c -o a2 -pthread -lrt