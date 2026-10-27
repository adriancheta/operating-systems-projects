# Inter-Process Communication Mechanisms

**Language:** C
**Environment:** Linux, POSIX system interfaces

## Overview

An Operating Systems laboratory project focused on
communication between processes and memory management
using Linux and POSIX mechanisms.

## Key Concepts

- Named pipes and request-response communication
- POSIX shared memory
- Memory-mapped files using `mmap()`
- Reading and writing data at specified file offsets
- Parsing file sections and translating logical offsets
- Input validation and memory-boundary checks

## Implementation

The program establishes communication through named
pipes and handles requests using a command-based
protocol.

It creates and writes to a shared-memory region, maps
files into memory, and supports reading data from file
offsets, file sections, and logical memory-space offsets.

## Source Code

See `a3.c`.

## Compilation

    gcc -Wall a3.c -o a3 -lrt