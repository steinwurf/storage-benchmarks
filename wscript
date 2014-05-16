#! /usr/bin/env python
# encoding: utf-8

APPNAME = 'storage-benchmarks'
VERSION = '0.1.0'


def recurse_helper(ctx, name):
    if not ctx.has_dependency_path(name):
        ctx.fatal('Load a tool to find %s as system dependency' % name)
    else:
        p = ctx.dependency_path(name)
        ctx.recurse([p])


def options(opt):

    import waflib.extras.wurf_dependency_bundle as bundle
    import waflib.extras.wurf_dependency_resolve as resolve

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='boost',
        git_repository='github.com/steinwurf/external-boost-light.git',
        major_version=1))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='cpuid',
        git_repository='github.com/steinwurf/cpuid.git',
        major_version=3))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='gauge',
        git_repository='github.com/steinwurf/cxx-gauge.git',
        major_version=7))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='fifi',
        git_repository='github.com/steinwurf/fifi.git',
        major_version=11))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='kodo',
        git_repository='github.com/steinwurf/kodo.git',
        major_version=17))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='platform',
        git_repository='github.com/steinwurf/platform.git',
        major_version=1))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='sak',
        git_repository='github.com/steinwurf/sak.git',
        major_version=10))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='tables',
        git_repository='github.com/steinwurf/tables.git',
        major_version=4))

    bundle.add_dependency(opt, resolve.ResolveGitMajorVersion(
        name='waf-tools',
        git_repository='github.com/steinwurf/external-waf-tools.git',
        major_version=2))

    opt.load('wurf_configure_output')
    opt.load('wurf_dependency_bundle')
    opt.load('wurf_tools')


def configure(conf):

    if conf.is_toplevel():
        conf.load('wurf_dependency_bundle')
        conf.load('wurf_tools')

        conf.load_external_tool('install_path', 'wurf_install_path')
        conf.load_external_tool('mkspec', 'wurf_cxx_mkspec_tool')
        conf.load_external_tool('project_gen', 'wurf_project_generator')
        conf.load_external_tool('runners', 'wurf_runner')

        recurse_helper(conf, 'boost')
        recurse_helper(conf, 'cpuid')
        recurse_helper(conf, 'fifi')
        recurse_helper(conf, 'gauge')
        recurse_helper(conf, 'kodo')
        recurse_helper(conf, 'platform')
        recurse_helper(conf, 'sak')
        recurse_helper(conf, 'tables')

    if conf.is_mkspec_platform('linux'):
        if not conf.env['LIB_PTHREAD']:
            conf.check_cxx(lib='pthread')
        if not conf.env['LIB_M']:
            conf.check_cc(lib='m')

    set_simd_flags(conf)
    conf.load('asm')
    conf.find_program(['yasm'], var='AS')
    conf.env.AS_TGT_F = ['-o']
    conf.env.ASLNK_TGT_F = ['-o']


def set_simd_flags(conf):
    """
    Sets flags used to compile in SIMD mode
    """
    CC = conf.env.get_flat("CC")
    flags = []
    defines = []

    if 'gcc' in CC or 'clang' in CC:
        flags += ['-O3', '-fPIC']
        flags += conf.mkspec_try_flags(
            'cflags', ['-mmmx', '-msse', '-msse2', '-msse3', '-mssse3',
                       '-mpclmul', '-msse4.1', '-msse4.2', '-mavx'])

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

    bld.stlib(
        features='c',
        source=bld.path.ant_glob('gf-complete/src/**/*.c'),
        target='gf_complete',
        includes=['gf-complete/include'],
        export_includes=['gf-complete/include'],
        use=['SIMD_SHARED'])

    bld.stlib(
        features='c',
        source=bld.path.ant_glob('jerasure/src/**/*.c',
                                 excl='jerasure/src/cauchy_best_r6.c'),
        target='jerasure',
        includes=['jerasure/include'],
        export_includes=['jerasure/include'],
        use=['SIMD_SHARED', 'gf_complete'])

    bld.stlib(
        features='c asm',
        source=bld.path.ant_glob('isa-l_open_src_2.8/isa/*.c') +
        bld.path.ant_glob('isa-l_open_src_2.8/isa/*.asm'),
        target='isa',
        asflags=get_asmformat(bld),
        includes=['isa-l_open_src_2.8/isa'],
        export_includes=['isa-l_open_src_2.8/isa'])

    openfec_flags = ['-O4']
    bld.env['CFLAGS_OPENFEC_SHARED'] = openfec_flags
    bld.env['CXXFLAGS_OPENFEC_SHARED'] = openfec_flags

    bld.stlib(
        features='c',
        source=bld.path.ant_glob('openfec-1.3/src/**/*.c'),
        target='openfec',
        includes=['openfec-1.3/src'],
        export_includes=['openfec-1.3/src'],
        use=['OPENFEC_SHARED', 'PTHREAD'])

    if bld.is_toplevel():

        bld.load('wurf_dependency_bundle')

        recurse_helper(bld, 'boost')
        recurse_helper(bld, 'cpuid')
        recurse_helper(bld, 'fifi')
        recurse_helper(bld, 'gauge')
        recurse_helper(bld, 'kodo')
        recurse_helper(bld, 'platform')
        recurse_helper(bld, 'sak')
        recurse_helper(bld, 'tables')

        # Only build test and benchmarks when executed from the
        # top-level wscript i.e. not when included as a dependency
        # in a recurse call

        bld.recurse('benchmark/isa_throughput')
        bld.recurse('benchmark/jerasure_throughput')
        bld.recurse('benchmark/kodo_throughput')
        bld.recurse('benchmark/openfec_throughput')
