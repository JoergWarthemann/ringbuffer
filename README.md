# RingBuffer

RingBuffer is a circular buffer container for data items. It implements the last-in-first-out (LIFO) principle.

## Features

* uses std::aligned_storage internally to decouple memory allocation from object creation
* adding elements using copy or move semantics
* fetching elements using copy semantics
* adding new elements to a full circular buffer will overwrite the oldest elements

## Implementation

This is a header only library.

## Requirements

RingBuffer requires a C++14 aware compiler.  
The tests are implemented using [Catch2](https://github.com/catchorg/Catch2).

## Compilation

RingBuffer comes with a [CMake](https://cmake.org) build script. CMake works by generating native Makefiles or build projects which can be used in the particular environment.

## Usage

