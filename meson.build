project(
  'ghex', 'c',
  version: '67.0',
  meson_version: '>= 1.0.0',
  license: 'GPL2',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
includedir = join_paths(prefix, get_option('includedir'))
datadir = join_paths(prefix, get_option('datadir'))
localedir = join_paths(prefix, get_option('localedir'))
docdir = join_paths(prefix, get_option('docdir'))
pkgconfigdir = join_paths(libdir, 'pkgconfig')
applicationsdir = join_paths(datadir, 'applications')
schemasdir = join_paths(datadir, 'glib-2.0/schemas')
metainfodir = join_paths(datadir, 'metainfo')
servicedir = join_paths(datadir, 'dbus-1/services')
iconsdir = join_paths(datadir, 'icons')
girdir = join_paths(datadir, 'gir-1.0')
typelibdir = join_paths(libdir, 'girepository-1.0')
po_dir = join_paths(meson.project_source_root(), 'po')

# compiler options

add_project_arguments(['-I' + meson.project_build_root()], language: 'c')

project_c_args = []
test_c_args = [
  '-Wcast-align',
  #'-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wno-unused-variable',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  #'-Wswitch-default',
  #  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

subdir('libgtkhex')
subdir('ghex')

summary({'prefix': prefix,
        'libdir': libdir,
        'datadir': datadir,
	'docdir': docdir,
        'includedir': includedir,
        }, section: 'Directories')
