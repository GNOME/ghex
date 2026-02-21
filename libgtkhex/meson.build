libgtkhex_api_version   = 5
libgtkhex_version_major = 0  # for soname (ABI major version) only
libgtkhex_version_minor = 0
libgtkhex_version_micro = 0
libgtkhex_version = '@0@.@1@.@2@'.format(libgtkhex_api_version, libgtkhex_version_minor, libgtkhex_version_micro)

libgtkhex_plugindir = join_paths(libdir, 'gtkhex-@0@.0'.format(libgtkhex_api_version))

config_h = configuration_data()
config_h.set_quoted('PACKAGE_PLUGINDIR', libgtkhex_plugindir)
config_h.set_quoted('LIBGTKHEX_RELEASE_STRING', 'gtkhex-@0@.0'.format(libgtkhex_api_version))

configure_file(
  output: 'config.h',
  configuration: config_h
)

libgtkhex_sources = [
  'gtkhex-layout-manager.c',
  'gtkhex-paste-data.c',
  'hex-auto-highlight.c',
  'hex-buffer-iface.c',
  'hex-buffer-malloc.c',
  'hex-document.c',
  'hex-file-monitor.c',
  'hex-highlight.c',
  'hex-mark.c',
  'hex-search-info.c',
  'hex-selection.c',
  'hex-text.c',
  'hex-text-ascii.c',
  'hex-text-common.c',
  'hex-text-editable.c',
  'hex-text-hex.c',
  'hex-text-offsets.c',
  'hex-view.c',
  'hex-widget.c',
  'util.c',
]

libgtkhex_res = gnome.compile_resources('libgtkhex-resources',
  'libgtkhex.gresource.xml',
  c_name: 'libgtkhex'
)

libgtkhex_headers = [
  'hex-buffer-iface.h',
  'hex-document.h',
]

libgtkhex_deps = [
  dependency('gtk4', version: '>= 4.18'),
  dependency('gmodule-2.0'),
]

libgtkhex_c_args = [
  '-DG_LOG_DOMAIN="gtkhex-@0@"'.format(libgtkhex_api_version)
]

osx_current = libgtkhex_version_major + 1
lib_osx_version = [osx_current, '@0@.@1@'.format(osx_current, libgtkhex_version_minor)]

library_name = 'gtkhex-@0@'.format(libgtkhex_api_version)
libgtkhex = library(
  library_name,
  [libgtkhex_sources, libgtkhex_res],
  version: '@0@.@1@.@2@'.format(
    libgtkhex_version_major, libgtkhex_version_minor, libgtkhex_version_micro),
  darwin_versions: lib_osx_version,
  dependencies: libgtkhex_deps,
  c_args: libgtkhex_c_args,
  install: true
)

if get_option('mmap-buffer-backend')

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

  shared_module('hex-buffer-mmap',
    sources: 'hex-buffer-mmap.c',
    c_args: libgtkhex_c_args,
    dependencies: libgtkhex_deps,
    install_dir: libgtkhex_plugindir,
    install: true,
  )

endif # mmap

if get_option('direct-buffer-backend')

  # Check for required headers
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

  shared_module('hex-buffer-direct',
    sources: 'hex-buffer-direct.c',
    c_args: libgtkhex_c_args,
    dependencies: libgtkhex_deps,
    install_dir: libgtkhex_plugindir,
    install: true,
  )

endif # direct

install_headers(
  libgtkhex_headers,
  subdir: 'gtkhex-@0@'.format(libgtkhex_api_version)
)

pkg_conf = configuration_data()
pkg_conf.set('prefix', prefix)
pkg_conf.set('exec_prefix', prefix)
pkg_conf.set('libdir', libdir)
pkg_conf.set('includedir', includedir)

pkg_conf.set('LIBGTKHEX_VERSION', libgtkhex_version)

configure_file(
  input: 'libgtkhex-5.pc.in',
  output: 'libgtkhex-5.pc',
  configuration: pkg_conf,
  install_dir: pkgconfigdir,
  install: true
)

if get_option('introspection').allowed()
  libgtkhex_gir = gnome.generate_gir(libgtkhex,
              sources: [libgtkhex_sources, libgtkhex_headers],
            nsversion: libgtkhex_api_version.to_string(),
            namespace: 'Hex',
        symbol_prefix: 'hex',
            link_with: libgtkhex,
         dependencies: libgtkhex_deps,
             includes: [ 'Gtk-4.0' ],
              install: true,
      install_dir_gir: girdir,
  install_dir_typelib: typelibdir,
           extra_args: [ '--warn-all' ],
  )

  if get_option('vapi')
    vapi_deps = [
      'gmodule-2.0',
      'glib-2.0',
      'gio-2.0',
      'gtk4',
    ]
    gnome.generate_vapi(library_name,
                        install  : true,
                        packages : vapi_deps,
                        sources  : libgtkhex_gir [0],
    )
  endif
endif

libgtkhex_dep = declare_dependency(
  link_with: libgtkhex,
  dependencies: libgtkhex_deps,
  sources: libgtkhex_headers
)

if get_option('docs')
  subdir('docs')
endif

summary({
	  '`mmap` buffer backend': get_option('mmap-buffer-backend'),
	  '`direct` buffer backend': get_option('direct-buffer-backend'),
          'GObject Introspection': get_option('introspection'),
	  'VAPI': get_option('vapi'),
	  'API Documentation': get_option('docs'),
        }, section: 'libgtkhex Options')
