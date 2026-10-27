# File System Module

**Language:** C
**Environment:** Linux

## Overview

A command-line utility for traversing directories and
inspecting files that follow a custom binary format
called SF (section file).

## Key Concepts

- Recursive directory traversal and file filtering
- File metadata and permission inspection
- Binary file-format parsing and validation
- File offsets, sections, and data extraction
- Command-line argument processing

## Implementation

The program supports directory listing with filtering
options, validation of SF file headers and section
metadata, extraction of a selected line from a section,
and recursive searching for files that meet specific
section-content criteria.

The `findall` operation searches for SF files containing
at least two sections with exactly 13 lines each.

## Source Code

See `a1.c`.

## Compilation

    gcc -Wall a1.c -o a1