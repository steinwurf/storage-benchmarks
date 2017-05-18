#! /usr/bin/env python
# encoding: utf-8

APPNAME = 'storage-benchmarks'
VERSION = '1.0.0'


def configure(conf):

    if conf.is_mkspec_platform('linux'):
        if not conf.env['LIB_PTHREAD']:
            conf.check_cxx(lib='pthread')
        if not conf.env['LIB_M']:
            conf.check_cc(lib='m')

    set_simd_flags(conf)

    conf.load('asm')
    if not conf.is_mkspec_platform('windows'):
        conf.find_program(['yasm'], var='AS')
        conf.env.AS_TGT_F = ['-o']
        conf.env.ASLNK_TGT_F = ['-o']


def get_cpu_flags(conf):
    """
    Returns compiler flags for the available instruction sets on this CPU
    """
    cflags = ['-mmmx', '-msse', '-msse2', '-msse3', '-mssse3',
              '-mpclmul', '-msse4.1', '-msse4.2', '-mavx']

    if conf.is_mkspec_platform('linux'):
        # Check the supported CPU flags in /proc/cpuinfo
        cpuflags = None
        with open('/proc/cpuinfo', 'r') as cpuinfo:
            for line in cpuinfo:
                if line.startswith('flags'):
                    cpuflags = line.split()
                    break

        if cpuflags:
            cflags = []
            if 'mmx' in cpuflags:
                cflags += ['-mmmx']
            if 'sse' in cpuflags:
                cflags += ['-msse']
            if 'sse2' in cpuflags:
                cflags += ['-msse2']
            # pni stands for Prescott New Instructions (i.e. SSE-3)
            if 'pni' in cpuflags:
                cflags += ['-msse3']
            if 'ssse3' in cpuflags:
                cflags += ['-mssse3']
            if 'pclmulqdq' in cpuflags:
                cflags += ['-mpclmul']
            if 'sse4_1' in cpuflags:
                cflags += ['-msse4.1']
            if 'sse4_2' in cpuflags:
                cflags += ['-msse4.2']
            if 'avx' in cpuflags:
                cflags += ['-mavx']

    return cflags


def set_simd_flags(conf):
    """
    Sets flags used to compile in SIMD mode
    """
    CC = conf.env.get_flat("CC")
    flags = []
    defines = []

    if 'gcc' in CC or 'clang' in CC:
        flags += ['-O3', '-fPIC']
        flags += conf.mkspec_try_flags('cflags', get_cpu_flags(conf))

        if '-msse' in flags:
            defines.append('INTEL_SSE')
        if '-msse2' in flags:
            defines.append('INTEL_SSE2')
        if '-msse3' in flags:
            defines.append('INTEL_SSE3')
        if '-mssse3' in flags:
            defines.append('INTEL_SSSE3')
        if '-mpclmul' in flags:
            defines.append('INTEL_SSE4_PCLMUL')
        if '-msse4.1' in flags:
            defines.append('INTEL_SSE4')
        if '-msse4.2' in flags:
            defines.append('INTEL_SSE4')

    elif 'CL.exe' in CC or 'cl.exe' in CC:
        pass

    else:
        conf.fatal('Unknown compiler - no SIMD flags specified')

    conf.env['CFLAGS_SIMD_SHARED'] = flags
    conf.env['CXXFLAGS_SIMD_SHARED'] = flags
    conf.env['DEFINES_SIMD_SHARED'] = defines


def get_asmformat(bld):

    if bld.get_mkspec_platform() == 'linux':
        if bld.env['DEST_CPU'] == 'x86':
            return ['-felf32']
        else:
            return ['-felf64']

    elif bld.get_mkspec_platform() == 'mac':
        if bld.env['DEST_CPU'] == 'x86_64':
            return ['-fmacho64']

    elif bld.get_mkspec_platform() == 'windows':
        if bld.env['DEST_CPU'] == 'x86':
            return ['-fwin32']
        else:
            return ['-fwin64']

    elif bld.get_mkspec_platform() == 'android':
        if bld.env['DEST_CPU'] == 'arm':
            return ['--defsym', 'ARCHITECTURE=5', '-march=armv5te']

    # If we get to this point then no asmformat was selected
    raise ValueError("Unknown platform/CPU combination: "
                     + bld.get_mkspec_platform() + " / " + bld.env['DEST_CPU'])


def build(bld):

    if '-O2' in bld.env['CFLAGS']:
        bld.env['CFLAGS'].remove('-O2')

    isa_enabled = True
    # ISA is not compatible with clang and the VS compiler and it does not
    # compile for 32-bit CPUs
    if bld.is_mkspec_platform('windows') or bld.env['DEST_CPU'] == 'x86' or \
       'clang' in bld.env.get_flat("CC"):
        isa_enabled = False

    if isa_enabled:
        bld.stlib(
            features='c asm',
            source=(
                bld.path.ant_glob('isa-l_open_src_2.13/isa/*.c') +
                bld.path.ant_glob('isa-l_open_src_2.13/isa/*.asm')),
            target='isa',
            asflags=get_asmformat(bld),
            includes=['isa-l_open_src_2.13/isa'],
            export_includes=['isa-l_open_src_2.13/isa'])

    openfec_enabled = True
    # OpenFEC is not compatible with clang and the VS compiler
    if 'clang' in bld.env.get_flat("CC") or bld.is_mkspec_platform('windows'):
        openfec_enabled = False

    if openfec_enabled:
        openfec_flags = ['-O4']
        bld.env['CFLAGS_OPENFEC_SHARED'] = openfec_flags
        bld.env['CXXFLAGS_OPENFEC_SHARED'] = openfec_flags

        bld.stlib(
            features='c',
            source=bld.path.ant_glob('openfec-1.4.2/src/**/*.c'),
            target='openfec',
            includes=['openfec-1.4.2/src'],
            export_includes=['openfec-1.4.2/src'],
            use=['OPENFEC_SHARED', 'PTHREAD'])

    if bld.is_toplevel():

        # Only build test and benchmarks when executed from the
        # top-level wscript i.e. not when included as a dependency
        # in a recurse call

        bld.recurse('benchmark/kodo_storage')

        if openfec_enabled:
            bld.recurse('benchmark/openfec_throughput')
        if isa_enabled:
            bld.recurse('benchmark/isa_throughput')
            bld.recurse('benchmark/isa_arithmetic')
