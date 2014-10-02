#!/bin/sh
build/linux/benchmark/kodo_storage/kodo_storage $@
build/linux/benchmark/isa_throughput/isa_throughput $@
build/linux/benchmark/jerasure_throughput/jerasure_throughput $@
build/linux/benchmark/openfec_throughput/openfec_throughput $@

