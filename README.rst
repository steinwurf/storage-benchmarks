Introduction
============

The storage-benchmarks repository is used for the performance comparison of
different erasure coding libraries.

Installation
=============

Clone the repository::

    git clone https://github.com/steinwurf-internal/storage-benchmarks.git

Since some external libraries are added as git submodules, we need to run
these extra commands::

    cd storage-benchmarks
    git submodule init
    git submodule update

Requirements
============

1. A recent C++11 compiler
2. yasm (for compiling the Assembly sources in ISA)

How to Build It
===============

The benchmarks can be built like any other Steinwurf project::

  python waf configure
  python waf build