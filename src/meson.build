libgtkhex_sources = [
  'gtkhex-layout-manager.c',
  'gtkhex-paste-data.c',
  'gtkhex.c',
  'hex-buffer-iface.c',
  'hex-buffer-malloc.c',
  'hex-document.c',
  'hex-file-monitor.c',
]

libgtkhex_headers = [
  'gtkhex-paste-data.h',
  'gtkhex.h',
  'hex-buffer-iface.h',
  'hex-document.h',
]

libgtkhex_deps = [
  gmodule_dep,
  glib_dep,
  gio_dep,
  gtk_dep,
]

libgtkhex_c_args = [
  '-DG_LOG_DOMAIN="gtkhex-4"'
]

libgtkhex_link_args = cc.get_supported_link_arguments([
  '-Wl,--no-undefined'
])

install_headers(
  libgtkhex_headers,
  subdir: 'gtkhex-@0@'.format(libgtkhex_api_version)
)

osx_current = libgtkhex_version_major + 1
lib_osx_version = [osx_current, '@0@.@1@'.format(osx_current, libgtkhex_version_minor)]

src_conf = configuration_data()
src_conf.set('app_id', app_id)
src_conf.set('resource_base_path', resource_base_path)

app_ui = configure_file(
  input: 'ghex-application-window.ui.in',
  output: 'ghex-application-window.ui',
  configuration: src_conf
)

lib_res_in = configure_file(
  input: 'libgtkhex.gresource.xml.in',
  output: 'libgtkhex.gresource.xml',
  configuration: src_conf
)

lib_res = gnome.compile_resources(
  'libgtkhex-resources',
  lib_res_in,
  c_name: 'libgtkhex'
)

res_in = configure_file(
  input: 'ghex.gresource.xml.in',
  output: 'ghex.gresource.xml',
  configuration: src_conf
)

res = gnome.compile_resources(
  'ghex-resources',
  res_in,
  c_name: 'ghex',
  dependencies: app_ui
)

library_name = 'gtkhex-@0@'.format(libgtkhex_api_version)
libgtkhex = library(
  library_name,
  libgtkhex_sources + lib_res,
  version: '@0@.@1@.@2@'.format(
    libgtkhex_version_major, libgtkhex_version_minor, libgtkhex_version_micro),
  darwin_versions: lib_osx_version,
  include_directories: ghex_root_dir,
  dependencies: libgtkhex_deps,
  c_args: libgtkhex_c_args,
  link_args: libgtkhex_link_args,
  install: true
)

libgtkhex_dep = declare_dependency(
  link_with: libgtkhex,
  include_directories: ghex_root_dir,
  dependencies: libgtkhex_deps,
  sources: libgtkhex_headers
)

if mmap_backend
    shared_module('hex-buffer-mmap',
    sources: 'hex-buffer-mmap.c',
    c_args: libgtkhex_c_args,
    dependencies: libgtkhex_dep,
    install_dir: ghex_plugindir,
    install: true,
  )
endif

if direct_backend
    shared_module('hex-buffer-direct',
    sources: 'hex-buffer-direct.c',
    c_args: libgtkhex_c_args,
    dependencies: libgtkhex_dep,
    install_dir: ghex_plugindir,
    install: true,
  )
endif

ghex_sources = [
  'chartable.c',
  'common-ui.c',
  'configuration.c',
  'converter.c',
  'findreplace.c',
  'ghex-application-window.c',
  'hex-dialog.c',
  'hex-info-bar.c',
  'hex-statusbar.c',
  'main.c',
  'mark-dialog.c',
  'paste-special.c',
  'preferences.c',
  'print.c',
]

ghex_c_args = [
  '-DG_LOG_DOMAIN="GHex"'
]

ghex = executable(
  meson.project_name(),
  ghex_sources + res,
  include_directories: ghex_root_dir,
  dependencies: [adw_dep, libgtkhex_dep],
  c_args: ghex_c_args,
  install: true
)

if generate_gir
  gtkhex_gir_sources = [
    'gtkhex.c',
    'gtkhex.h',
    'hex-document.c',
    'hex-document.h',
    'hex-buffer-iface.c',
    'hex-buffer-iface.h',
  ]

  gtkhex_gir = gnome.generate_gir(libgtkhex,
              sources: gtkhex_gir_sources,
            nsversion: libgtkhex_api_version.to_string(),
            namespace: 'Hex',
        symbol_prefix: 'hex',
            link_with: libgtkhex,
         dependencies: libgtkhex_dep,
             includes: [ 'Gtk-4.0' ],
              install: true,
      install_dir_gir: ghex_girdir,
  install_dir_typelib: ghex_typelibdir,
           extra_args: [ '--warn-all' ],
  )

  if generate_vapi
    vapi_deps = [
      'gmodule-2.0',
      'glib-2.0',
      'gio-2.0',
      'gtk4',
    ]
    gnome.generate_vapi(library_name,
                        install  : true,
                        packages : vapi_deps,
                        sources  : gtkhex_gir [0],
    )
  endif
endif
