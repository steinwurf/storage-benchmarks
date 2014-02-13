Introduction
============

The external-benchmarks repository is used for performance comparison of
different erasure coding libraries.

Installation
=============

Clone the repository::

    git clone https://github.com/steinwurf-internal/external-benchmarks.git

Since the external libraries are added as git submodules, we need to run
these extra commands::

    cd external-benchmarks
    git submodule init
    git submodule update