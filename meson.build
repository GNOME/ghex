project(
  'ghex', 'c',
  version: '3.18.3',
  meson_version: '>=0.37.0',
  license: 'GPL2'
)

version_arr = meson.project_version().split('.')
ghex_version_major = version_arr[0].to_int()
ghex_version_minor = version_arr[1].to_int()
ghex_version_micro = version_arr[2].to_int()

libghex_version_major = version_arr[0].to_int()

ghex_prefix = get_option('prefix')
ghex_libdir = join_paths(ghex_prefix, get_option('libdir'))
ghex_includedir = join_paths(ghex_prefix, get_option('includedir'))
ghex_datadir = join_paths(ghex_prefix, get_option('datadir'))
ghex_localedir = join_paths(ghex_prefix, get_option('localedir'))
ghex_pkgconfigdir = join_paths(ghex_libdir, 'pkgconfig')
ghex_applicationsdir = join_paths(ghex_datadir, 'applications')
ghex_schemasdir = join_paths(ghex_datadir, 'glib-2.0/schemas')
ghex_appdatadir = join_paths(ghex_datadir, 'metainfo')
ghex_iconsdir = join_paths(ghex_datadir, 'icons')

ghex_root_dir = include_directories('.')
ghex_po_dir = join_paths(meson.source_root(), 'po')

gnome = import('gnome')
i18n = import('i18n')

cc = meson.get_compiler('c')

ghex_conf = configuration_data()
ghex_conf.set_quoted('PACKAGE_NAME', meson.project_name())
ghex_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
ghex_conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
ghex_conf.set_quoted('PACKAGE_DATADIR', ghex_datadir)
ghex_conf.set_quoted('PACKAGE_LIBDIR', ghex_libdir)
ghex_conf.set_quoted('PACKAGE_LOCALE_DIR', ghex_localedir)

ghex_conf.set('VERSION', 'PACKAGE_VERSION')
ghex_conf.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
ghex_conf.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')

ghex_conf.set_quoted('LIBGTKHEX_RELEASE_STRING', 'gtkhex-@0@.0'.format(libghex_version_major))

ghex_conf.set10('ENABLE_NLS', true) # Always enabled

check_headers = [
  'dlfcn.h',
  'inttypes.h',
  'memory.h',
  'stdint.h',
  'stdlib.h',
  'string.h',
  'sys/stat.h',
  'sys/types.h',
  'unistd.h'
]

foreach h : check_headers
  if cc.has_header(h)
    ghex_conf.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

check_functions = [
  'pow',
  'strstr',
  'strtoul'
]

foreach f : check_functions
  if cc.has_function(f)
    ghex_conf.set('HAVE_' + f.underscorify().to_upper(), 1)
  endif
endforeach

atk_dep = dependency('atk', version: '>= 1.0.0')
gio_dep = dependency('gio-2.0', version: '>= 2.31.10')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.3.8')
gail_dep = dependency('gail-3.0', version: '>= 1.0.0')

configure_file(
  output: 'config.h',
  configuration: ghex_conf
)

update_icon_cache_prg = find_program('gtk-update-icon-cache', required : false)
update_desktop_database_prg = find_program('update-desktop-database', required : false)
compile_schemas_prg = find_program('glib-compile-schemas', required : false)

subdir('src')
subdir('data')
subdir('icons')
subdir('po')
subdir('help')

meson.add_install_script('meson_post_install.py')
