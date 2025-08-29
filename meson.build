project(
  'ghex', 'c',
  version: '48.0',
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
libgtkhex_version_minor = 11
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

gnome = import('gnome')
i18n = import('i18n')

cc = meson.get_compiler('c')

# just to avoid manually punching it into the XML files as well as the source.
shaded_box_max = 1000

config_h = configuration_data()
config_h.set_quoted('APP_ID', app_id)
config_h.set_quoted('RESOURCE_BASE_PATH', resource_base_path)
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
config_h.set_quoted('PACKAGE_DATADIR', ghex_datadir)
config_h.set_quoted('PACKAGE_LIBDIR', ghex_libdir)
config_h.set_quoted('PACKAGE_PLUGINDIR', ghex_plugindir)
config_h.set_quoted('PACKAGE_LOCALE_DIR', ghex_localedir)
config_h.set_quoted('PACKAGE_DOCDIR', ghex_docdir)

config_h.set('VERSION', 'PACKAGE_VERSION')
config_h.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
config_h.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')

config_h.set('CONFIG_H_SHADED_BOX_MAX', shaded_box_max)

config_h.set_quoted('LIBGTKHEX_RELEASE_STRING', 'gtkhex-@0@.0'.format(libgtkhex_api_version))

config_h.set('STATIC_HTML_HELP', get_option('static-html-help'))

config_h.set('DEBUG', get_option('debug'))

# i18n:  Always enabled
config_h.set10('ENABLE_NLS', true) 

# mmap: Check for required headers and fcns
if mmap_backend
  message('Checking dependencies for `mmap` buffer backend...')

  check_headers_mmap = [
    'unistd.h',
    'fcntl.h',
    'sys/stat.h',
    'sys/mman.h',
    ]
  foreach h : check_headers_mmap
    cc.has_header(h, required: true)
  endforeach
  
  check_functions_mmap = [
    'mmap',
    'mremap',
  ]
  foreach f : check_functions_mmap
    if cc.has_function(f) == false
      error('Required C function not found: @0@'.format(f))
    endif
  endforeach

  message('...DONE')
endif

# direct buffer backend: Check for required headers
if direct_backend
  message('Checking dependencies for `direct` buffer backend...')

  check_headers_direct = [
    'unistd.h',
    'fcntl.h',
    'sys/ioctl.h',
    'linux/fs.h',
    ]
  foreach h : check_headers_direct
    cc.has_header(h, required: true)
  endforeach

  message('...DONE')
endif

glib_ver = '>= 2.68.0'
gmodule_dep = dependency('gmodule-2.0')
glib_dep = dependency('glib-2.0', version: glib_ver)
gio_dep = dependency('gio-2.0', version: glib_ver)
gtk_dep = dependency('gtk4', version: '>= 4.17.5')
adw_dep = dependency('libadwaita-1', version: '>= 1.7')

configure_file(
  output: 'config.h',
  configuration: config_h
)

subdir('src')
subdir('data')
subdir('docs')
subdir('icons')
subdir('po')
subdir('help')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)

### SUMMARY PRINTOUT ###

summary({'prefix': ghex_prefix,
        'libdir': ghex_libdir,
        'datadir': ghex_datadir,
	'docdir': ghex_docdir,
        'includedir': ghex_includedir,
        }, section: 'Directories')

summary({'Development build': get_option('development'),
	  '`mmap` buffer backend': mmap_backend,
	  '`direct` buffer backend': direct_backend,
	  'Static HTML help': get_option('static-html-help'),
	  'API Documentation': build_gtk_doc,
        }, section: 'Configuration')
