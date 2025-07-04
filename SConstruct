from distutils.version import LooseVersion as Version
import re
import sys
import os
import subprocess

# Python 2.6 doesn't have subprocess.check_output
try:
    subprocess.check_output
except AttributeError:
    def check_output_compat(*popenargs, **kwargs):
        # This function was copied from Python 2.7's subprocess module.
        # This function only is:
        # Copyright (c) 2003-2005 by Peter Astrand <astrand@lysator.liu.se>
        # Licensed to PSF under a Contributor Agreement.
        # See http://www.python.org/2.4/license for licensing details.
        if 'stdout' in kwargs:
            raise ValueError('stdout argument not allowed, it will be overridden.')
        process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
        output, unused_err = process.communicate()
        retcode = process.poll()
        if retcode:
            cmd = kwargs.get("args")
            if cmd is None:
                cmd = popenargs[0]
            raise subprocess.CalledProcessError(retcode, cmd, output=output)
        return output
    subprocess.check_output = check_output_compat

# Access through env['VERSION'], env['RPM_VERSION'], and env['RPM_RELEASE'] to
# allow users to override these. RPM versioning is explained here:
# https://fedoraproject.org/wiki/Packaging:NamingGuidelines#NonNumericRelease
_VERSION = '1.2.0-alpha.0'
_RPM_VERSION = '1.2.0'
_RPM_RELEASE = '0.1.alpha.0'

opts = Variables('Local.sc')

opts.AddVariables(
    ("CC", "C Compiler"),
    ("CPPPATH", "The list of directories that the C preprocessor "
                "will search for include directories", []),
    ("CXX", "C++ Compiler"),
    ("CXX_FAMILY", "C++ compiler family (gcc or clang)", "auto"),
    ("CXX_VERSION", "C++ compiler version", "auto"),
    ("CXXFLAGS", "Options that are passed to the C++ compiler", []),
    ("PROTOCXXFLAGS", "Options that are passed to the C++ compiler "
                      "for ProtoBuf files", []),
    ("GTESTCXXFLAGS", "Options that are passed to the C++ compiler "
                      "for compiling gtest", []),
    ("LINKFLAGS", "Options that are passed to the linker", []),
    ("AS", "Assembler"),
    ("LIBPATH", "Library paths that are passed to the linker", []),
    ("LINK", "Linker"),
    ("BUILDTYPE", "Build type (RELEASE or DEBUG)", "DEBUG"),
    ("VERBOSE", "Show full build information (0 or 1)", "0"),
    ("NUMCPUS", "Number of CPUs to use for build (0 means auto).", "0"),
    ("VERSION", "Override version string", _VERSION),
    ("RPM_VERSION", "Override version number for rpm", _RPM_VERSION),
    ("RPM_RELEASE", "Override release string for rpm", _RPM_RELEASE),
)

env = Environment(options = opts,
                  tools = ['default', 'protoc', 'packaging'],
                  ENV = os.environ)
Help(opts.GenerateHelpText(env))

# Needed for Clang Static Analyzer's scan-build tool
env["CC"] = os.getenv("CC") or env["CC"]
env["CXX"] = os.getenv("CXX") or env["CXX"]
for k, v in os.environ.items():
    if k.startswith("CCC_"):
        env["ENV"][k] = v

def detect_compiler():
    reflags = re.IGNORECASE|re.MULTILINE
    output = subprocess.check_output([env['CXX'], '-v'],
                                     stderr=subprocess.STDOUT)
    if isinstance(output, bytes):
        output = output.decode('utf-8', errors='ignore')
    m = re.search(r'gcc version (\d+\.\d+\.\d+)',
                  output, reflags)
    if m is not None:
        env['CXX_FAMILY'] = 'gcc'
        env['CXX_VERSION'] = m.group(1)
        return
    m = re.search(r'clang version (\d+\.\d+\.\d+)',
                  output, reflags)
    if m is not None:
        env['CXX_FAMILY'] = 'clang'
        env['CXX_VERSION'] = m.group(1)
        return

if env['CXX_FAMILY'].lower() == 'auto':
    try:
        detect_compiler()
        print('Detected compiler %s %s' % (env['CXX_FAMILY'],
                                           env['CXX_VERSION']))
    except BaseException as e:
        print('Could not detect compiler: %s' % e)
        pass

CXX_STANDARD = 'c++11'

if (env['CXX_FAMILY'] == 'gcc' and
    Version(env['CXX_VERSION']) < Version('4.7')):
    CXX_STANDARD = 'c++0x'

if env['CXX_FAMILY'] == 'gcc':
    env.Prepend(CXXFLAGS = [
        "-Wall",
        "-Wextra",
        "-Wcast-align",
        "-Wcast-qual",
        "-Wconversion",
        "-Weffc++",
        "-Wformat=2",
        "-Wmissing-format-attribute",
        "-Wno-non-template-friend",
        "-Wno-unused-parameter",
        "-Woverloaded-virtual",
        "-Wwrite-strings",
        # "-DSWIG", # 已移除，防止 protobuf 报错
    ])
elif env['CXX_FAMILY'] == 'clang':
    # I couldn't find a descriptive list of warnings for clang, so it's easier
    # to enable them all with -Weverything and then disable the problematic
    # ones.
    env.Prepend(CXXFLAGS = [
        '-Wno-c++98-compat-pedantic',
        '-Wno-covered-switch-default',
        '-Wno-deprecated',
        '-Wno-disabled-macro-expansion',
        '-Wno-documentation-unknown-command',
        '-Wno-exit-time-destructors',
        '-Wno-float-equal',
        '-Wno-global-constructors',
        '-Wno-gnu-zero-variadic-macro-arguments',
        '-Wno-missing-noreturn',
        '-Wno-missing-prototypes',
        '-Wno-missing-variable-declarations',
        '-Wno-packed',
        '-Wno-padded',
        '-Wno-reserved-id-macro',
        '-Wno-shadow',
        '-Wno-shift-sign-overflow',
        '-Wno-switch-enum',
        '-Wno-undef',
        '-Wno-unknown-warning-option',
        '-Wno-unused-macros',
        '-Wno-unused-member-function',
        "-Wno-unused-parameter",
        '-Wno-used-but-marked-unused',
        '-Wno-vla',
        '-Wno-vla-extension',
        '-Wno-weak-vtables',
    ])

    # Clang 3.4 is known to emit warnings without -Wno-unreachable-code:
    if Version(env['CXX_VERSION']) < Version('3.5'):
        env.Prepend(CXXFLAGS = ['-Wno-unreachable-code'])

    env.Prepend(CXXFLAGS = ['-Weverything'])

env.Prepend(CXXFLAGS = [
    "-std=%s" % CXX_STANDARD,
    "-fno-strict-overflow",
    "-fPIC",
])
env.Prepend(PROTOCXXFLAGS = [
    "-std=%s" % CXX_STANDARD,
    "-fno-strict-overflow",
    "-fPIC",
])
env.Prepend(GTESTCXXFLAGS = [
    "-std=%s" % CXX_STANDARD,
    "-fno-strict-overflow",
    "-fPIC",
])

if env["BUILDTYPE"] == "DEBUG":
    env.Append(CPPFLAGS = [ "-g", "-DDEBUG" ])
elif env["BUILDTYPE"] == "RELEASE":
    env.Append(CPPFLAGS = [ "-DNDEBUG", "-O2" ])
else:
    print("Error BUILDTYPE must be RELEASE or DEBUG")
    sys.exit(-1)

if env["VERBOSE"] == "0":
    env["CCCOMSTR"] = "Compiling $SOURCE"
    env["CXXCOMSTR"] = "Compiling $SOURCE"
    env["SHCCCOMSTR"] = "Compiling $SOURCE"
    env["SHCXXCOMSTR"] = "Compiling $SOURCE"
    env["ARCOMSTR"] = "Creating library $TARGET"
    env["LINKCOMSTR"] = "Linking $TARGET"

env.Append(CPPPATH = '#')
env.Append(CPPPATH = '#/include')

# Define protocol buffers builder to simplify SConstruct files
def Protobuf(env, source):
    # First build the proto file
    cc = env.Protoc(os.path.splitext(source)[0] + '.pb.cc',
                    source,
                    PROTOCPROTOPATH = ["."],
                    PROTOCPYTHONOUTDIR = ".",
                    PROTOCOUTDIR = ".")[1]
    # Then build the resulting C++ file with no warnings
    return env.StaticObject(cc,
                            CXXFLAGS = env['PROTOCXXFLAGS'] + ['-Ibuild'])
env.AddMethod(Protobuf)

def GetNumCPUs():
    if env["NUMCPUS"] != "0":
        return int(env["NUMCPUS"])
    if "SC_NPROCESSORS_ONLN" in os.sysconf_names:
        cpus = os.sysconf("SC_NPROCESSORS_ONLN")
        if isinstance(cpus, int) and cpus > 0:
            return 2*cpus
        else:
            return 2
    import subprocess
    cpus = subprocess.check_output(["sysctl", "-n", "hw.ncpu"])
    if isinstance(cpus, bytes):
        cpus = cpus.decode("utf-8", errors="ignore")
    return 2*int(cpus.strip())

env.SetOption('num_jobs', GetNumCPUs())

object_files = {}
Export('object_files')

Export('env')
SConscript('Core/SConscript', variant_dir='build/Core')
SConscript('Event/SConscript', variant_dir='build/Event')
SConscript('RPC/SConscript', variant_dir='build/RPC')
SConscript('Protocol/SConscript', variant_dir='build/Protocol')
SConscript('Tree/SConscript', variant_dir='build/Tree')
SConscript('Client/SConscript', variant_dir='build/Client')
SConscript('Storage/SConscript', variant_dir='build/Storage')
SConscript('Server/SConscript', variant_dir='build/Server')
SConscript('Examples/SConscript', variant_dir='build/Examples')
SConscript('test/SConscript', variant_dir='build/test')

# This function is taken from http://www.scons.org/wiki/PhonyTargets
def PhonyTargets(env = None, **kw):
    if not env: env = DefaultEnvironment()
    for target,action in kw.items():
        env.AlwaysBuild(env.Alias(target, [], action))

PhonyTargets(check = "scripts/cpplint.py")
PhonyTargets(lint = "scripts/cpplint.py")
PhonyTargets(doc = "doxygen docs/Doxyfile")
PhonyTargets(docs = "doxygen docs/Doxyfile")
PhonyTargets(tags = "ctags -R --exclude=build --exclude=docs .")

clientlib = env.StaticLibrary("build/logcabin",
                  (object_files['Client'] +
                   object_files['Tree'] +
                   object_files['Protocol'] +
                   object_files['RPC'] +
                   object_files['Event'] +
                   object_files['Core']))
env.Default(clientlib)

daemon = env.Program("build/LogCabin",
            (["build/Server/Main.cc"] +
             object_files['Server'] +
             object_files['Storage'] +
             object_files['Tree'] +
             object_files['Client'] +
             object_files['Protocol'] +
             object_files['RPC'] +
             object_files['Event'] +
             object_files['Core']),
            LIBS = [ "pthread", "protobuf", "rt", "cryptopp" ])
env.Default(daemon)

storageTool = env.Program("build/Storage/Tool",
            (["build/Storage/Tool.cc"] +
             [ # these proto files should maybe move into Protocol
                "build/Server/SnapshotMetadata.pb.o",
                "build/Server/SnapshotStateMachine.pb.o",
             ] +
             object_files['Storage'] +
             object_files['Tree'] +
             object_files['Protocol'] +
             object_files['Core']),
            LIBS = [ "pthread", "protobuf", "rt", "cryptopp" ])
env.Default(storageTool)

# Create empty directory so that it can be installed to /var/log/logcabin
try:
    os.mkdir("build/emptydir")
except OSError:
    pass # directory exists

### scons install target

env.InstallAs('/etc/init.d/logcabin',           'scripts/logcabin-init-redhat')
env.InstallAs('/usr/bin/logcabinctl',           'build/Client/ServerControl')
env.InstallAs('/usr/bin/logcabind',             'build/LogCabin')
env.InstallAs('/usr/bin/logcabin',              'build/Examples/TreeOps')
env.InstallAs('/usr/bin/logcabin-benchmark',    'build/Examples/Benchmark')
env.InstallAs('/usr/bin/logcabin-reconfigure',  'build/Examples/Reconfigure')
env.InstallAs('/usr/bin/logcabin-smoketest',    'build/Examples/SmokeTest')
env.InstallAs('/usr/bin/logcabin-storage',      'build/Storage/Tool')
env.InstallAs('/var/log/logcabin',              'build/emptydir')
env.Alias('install', ['/etc', '/usr', '/var'])


#### 'scons rpm' target

# Work-around for older versions of SCons (2.3.0) that had LC_ALL set to
# lowercase c instead of uppercase C. SCons hg changeset 2943:8e42d865bdda in
# Nov 3, 2013 fixed this upstream in SCons. Without this workaround, building
# the RPM with 2.3.0 would sometimes fail.
env['RPM'] = 'LC_ALL=C rpmbuild'

# monkey-patch for SCons.Tool.packaging.rpm.collectintargz, which tries to put
# way too many files into the source tarball (the source tarball should only
# contain the installed files, since we're not building it)
def decent_collectintargz(target, source, env):
    tarball = env['SOURCE_URL'].split('/')[-1]
    from SCons.Tool.packaging import src_targz
    tarball = src_targz.package(env, source=source, target=tarball,
                                PACKAGEROOT=env['PACKAGEROOT'])
    return target, tarball
import SCons.Tool.packaging.rpm as RPMPackager
RPMPackager.collectintargz = decent_collectintargz

# set the install target in the .spec file to just copy over the files that
# 'scons install' would install. Default scons behavior is to invoke scons in
# the source tarball, which doesn't make a ton of sense unless you're doing the
# build in there.
install_commands = []
for target in env.FindInstalledFiles():
    parent = target.get_dir()
    source = target.sources[0]
    install_commands.append('mkdir -p $RPM_BUILD_ROOT%s' % parent)
    install_commands.append('cp -r %s $RPM_BUILD_ROOT%s' % (source, target))

pre_commands = [
    ('/usr/bin/getent group logcabin ||' +
     '/usr/sbin/groupadd -r logcabin'),
    ('/usr/bin/getent passwd logcabin ||' +
     '/usr/sbin/useradd -r -g logcabin -d / -s /sbin/nologin logcabin'),
]

post_commands = [
    'chown -R logcabin:logcabin /var/log/logcabin',
    'mkdir -p /var/lib/logcabin',
    'chown logcabin:logcabin /var/lib/logcabin',
]

# We probably don't want rpm to strip binaries.
# This is kludged into the spec file.
skip_stripping_binaries_commands = [
    # The normal __os_install_post consists of:
    #    %{_rpmconfigdir}/brp-compress
    #    %{_rpmconfigdir}/brp-strip %{__strip}
    #    %{_rpmconfigdir}/brp-strip-static-archive %{__strip}
    #    %{_rpmconfigdir}/brp-strip-comment-note %{__strip} %{__objdump}
    # as shown by: rpm --showrc | grep ' __os_install_post' -A10
    #
    # brp-compress just gzips manpages, which is fine. The others are probably
    # undesirable.
    #
    # This can go anywhere in the spec file.
    '%define __os_install_post /usr/lib/rpm/brp-compress',

    # Your distro may also be configured to build -debuginfo packages by default,
    # stripping the binaries and placing their symbols there. Let's not do that
    # either.
    #
    # This has to go at the top of the spec file.
    '%global _enable_debug_package 0',
    '%global debug_package %{nil}',
]

PACKAGEROOT = 'logcabin-%s' % env['RPM_VERSION']

rpms=RPMPackager.package(env,
    target            = ['logcabin-%s' % env['RPM_VERSION']],
    source            = env.FindInstalledFiles(),
    X_RPM_INSTALL     = '\n'.join(install_commands),
    X_RPM_PREINSTALL  = '\n'.join(pre_commands),
    X_RPM_POSTINSTALL = '\n'.join(post_commands),
    PACKAGEROOT       = PACKAGEROOT,
    NAME              = 'logcabin',
    VERSION           = env['RPM_VERSION'],
    PACKAGEVERSION    = env['RPM_RELEASE'],
    LICENSE           = 'ISC',
    SUMMARY           = 'LogCabin is clustered consensus deamon',
    X_RPM_GROUP       = ('Application/logcabin' + '\n' +
                         '\n'.join(skip_stripping_binaries_commands)),
    DESCRIPTION       =
    'LogCabin is a distributed system that provides a small amount of\n'
    'highly replicated, consistent storage. It is a reliable place for\n'
    'other distributed systems to store their core metadata and\n'
    'is helpful in solving cluster management issues.',
)

# Rename .rpm files into build/
def rename(env, target, source):
    for (t, s) in zip(target, source):
        os.rename(str(s), str(t))

# Rename files used to build .rpm files
def remove_sources(env, target, source):
    garbage = set()
    for s in source:
        garbage.update(s.sources)
        for s2 in s.sources:
            garbage.update(s2.sources)
    for g in list(garbage):
        if str(g).endswith('.spec'):
            garbage.update(g.sources)
    for g in garbage:
        if env['VERBOSE'] == '1':
            print('rm %s' % g)
        try:
            os.remove(str(g))
        except OSError:
            os.rmdir(str(g))

# Rename PACKAGEROOT directory and subdirectories (should be empty)
def remove_packageroot(env, target, source):
    if env['VERBOSE'] == '1':
        print('rm -r %s' % PACKAGEROOT)
    import shutil
    shutil.rmtree(str(PACKAGEROOT))

# Wrap cleanup around (moved) RPM targets
rpms = env.Command(['build/%s' % str(rpm) for rpm in rpms],
                   rpms,
                   [rename, remove_sources, remove_packageroot])

env.Alias('rpm', rpms)
