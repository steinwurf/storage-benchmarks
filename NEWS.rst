News for storage-benchmarks
===========================

This file lists the major changes between versions. For a more
detailed list of every change, see the Git log.

Latest
------
* Major: Upgrade to kodo 34
* Major: Upgrade to waf-tools 3
* Major: Upgrade to kodo 32
* Minor: Upgrade to gauge 10
* Minor: Upgrade to OpenFEC 1.4.2
* Minor: Update local dependency gauge to version 9.
* Minor: Added a perpetual benchmark in kodo_storage
* Major: Upgrade to kodo 22
* Major: Upgrade to kodo 21
* Major: Upgrade to ISA 2.10
* Major: Removed Jerasure (as it is no longer available)
* Minor: Disabled the SIMD flags that are not supported by the current CPU when
  compiling the Jerasure benchmarks. Note that Jerasure does not have any CPU
  dispatch logic, so enabling all SIMD flags would result in illegal instruction
  errors on older CPUs.
* Minor: Added Jerasure benchmarks with the Cauchy_Orig and Cauchy_Good
  coding techniques (to compare against the ReedSolVan default technique)
* Major: Upgrade to kodo 19
* Major: Upgrade to fifi 14
* Major: Upgrade to sak 12
* Major: Upgrade to tables 5
* Initial release using Kodo 17, ISA 2.8, Jerasure 2.0 and OpenFEC 1.3

