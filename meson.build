project(
  'ghex', 'c',
<<<<<<< HEAD
  version: '51.beta',
  meson_version: '>=0.59.0',
  license: 'GPL2'
)

# Options

if get_option('development')
  app_id = 'org.gnome.GHex.Devel'
  resource_base_path = '/org/gnome/GHex/Devel'
else
  app_id = 'org.gnome.GHex'
  resource_base_path = '/org/gnome/GHex'
endif

# Backend plugins (over and above 'malloc', which is baked in)
mmap_backend = get_option('mmap-buffer-backend')
direct_backend = get_option('direct-buffer-backend')

gir = find_program('g-ir-scanner', required : get_option('introspection'))
vapigen = find_program('vapigen', required : get_option('vapi'))
generate_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())

if get_option('vapi')
  assert (generate_gir, 'vapi requires GObject Introspection. Use -Dvapi=false to disable it')
endif

generate_vapi = generate_gir and vapigen.found() and get_option('vapi')
build_gtk_doc = get_option('gtk_doc')

# Directories, variables, etc.

version_arr = meson.project_version().split('.')
ghex_version_major = version_arr[0].to_int()

# libgtkhex version info
libgtkhex_api_version   = 4
libgtkhex_version_major = 1  # for soname (ABI major version) only
libgtkhex_version_minor = 12
libgtkhex_version_micro = 0
libgtkhex_version = '@0@.@1@.@2@'.format(libgtkhex_api_version, libgtkhex_version_minor, libgtkhex_version_micro)

ghex_prefix = get_option('prefix')
ghex_bindir = join_paths(ghex_prefix, get_option('bindir'))
ghex_libdir = join_paths(ghex_prefix, get_option('libdir'))
ghex_includedir = join_paths(ghex_prefix, get_option('includedir'))
ghex_datadir = join_paths(ghex_prefix, get_option('datadir'))
ghex_localedir = join_paths(ghex_prefix, get_option('localedir'))
ghex_docdir = join_paths(ghex_prefix, get_option('docdir'))
ghex_plugindir = join_paths(ghex_libdir, 'gtkhex-@0@.0'.format(libgtkhex_api_version))
ghex_pkgconfigdir = join_paths(ghex_libdir, 'pkgconfig')
ghex_applicationsdir = join_paths(ghex_datadir, 'applications')
ghex_schemasdir = join_paths(ghex_datadir, 'glib-2.0/schemas')
ghex_metainfodir = join_paths(ghex_datadir, 'metainfo')
ghex_servicedir = join_paths(ghex_datadir, 'dbus-1/services')
ghex_iconsdir = join_paths(ghex_datadir, 'icons')
ghex_girdir = join_paths(ghex_datadir, 'gir-1.0')
ghex_typelibdir = join_paths(ghex_libdir, 'girepository-1.0')

ghex_root_dir = include_directories('.')
ghex_po_dir = join_paths(meson.project_source_root(), 'po')

# warning cflags
warning_cflags = [
  '-Wno-unused-variable',
  '-Wno-unused-parameter',
  '-Werror=implicit',
  '-Wformat=2',
]
c_compiler = meson.get_compiler('c')
supported_warning_cflags = c_compiler.get_supported_arguments(warning_cflags)
add_global_arguments(supported_warning_cflags, language : 'c')

=======
  version: '67.0',
  meson_version: '>= 1.0.0',
  license: 'GPL2',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

cc = meson.get_compiler('c')
>>>>>>> eaf5c112 (WIP - restructure lib/app - intial import of libgtkhex rewrite)
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
  '-Wdeclaration-after-statement',
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
  '-Wswitch-default',
  '-Wswitch-enum',
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
