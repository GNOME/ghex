libgtkhex_sources = [
  'gtkhex.c',
  'hex-buffer-iface.c',	      # TEST
  'hex-buffer-mmap.c',        # TEST
  'hex-buffer-malloc.c',      # TEST
  'gtkhex-layout-manager.c',
  'gtkhex-paste-data.c',
  'hex-document.c'
]

libgtkhex_headers = [
  'hex-document.h',
  'hex-buffer-iface.h',
  'hex-buffer-malloc.h',
  'hex-buffer-mmap.h',
  'gtkhex.h',
  'gtkhex-paste-data.h'
]

libgtkhex_deps = [
  gtk_dep,
]

libgtkhex_c_args = [
  '-DG_LOG_DOMAIN="libgtkhex-4"'
]

libgtkhex_link_args = cc.get_supported_link_arguments([
  '-Wl,--no-undefined'
])

install_headers(
  libgtkhex_headers,
  subdir: 'gtkhex-@0@'.format(libgtkhex_version_major)
)

libgtkhex = library(
  'gtkhex-@0@'.format(libgtkhex_version_major),
  libgtkhex_sources + libgtkhex_headers,
  version: '0.0.0',
  darwin_versions: ['1', '1.0'],
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

# for i in `ls *.{c,h}`; do echo "  '${i}',"; done
ghex_sources = [
  'chartable.c',
  'chartable.h',
  'common-ui.c',
  'common-ui.h',
  'configuration.c',
  'configuration.h',
  'converter.c',
  'converter.h',
  'findreplace.c',
  'findreplace.h',
  'ghex-application-window.c',
  'ghex-application-window.h',
  'ghex-notebook-tab.c',
  'ghex-notebook-tab.h',
  'gtkhex-paste-data.c',
  'gtkhex-paste-data.h',
  'hex-dialog.c',
  'hex-dialog.h',
  'main.c',
  'paste-special.c',
  'paste-special.h',
  'preferences.c',
  'preferences.h',
  'print.c',
  'print.h'
]

ghex_deps = [
  gio_dep,
  gtk_dep
]

src_conf = configuration_data()
src_conf.set('app_id', app_id)
src_conf.set('resource_base_path', resource_base_path)

app_ui = configure_file(
  input: 'ghex-application-window.ui.in',
  output: 'ghex-application-window.ui',
  configuration: src_conf
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

ghex_c_args = [
  '-DG_LOG_DOMAIN="GHex"'
]

ghex = executable(
  meson.project_name(),
  ghex_sources + res,
  include_directories: ghex_root_dir,
  dependencies: ghex_deps + [libgtkhex_dep, ],
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
    'hex-buffer-malloc.c',
    'hex-buffer-malloc.h',
    'hex-buffer-mmap.c',
    'hex-buffer-mmap.h',
  ]

  gtkhex_gir = gnome.generate_gir(libgtkhex,
              sources: gtkhex_gir_sources,
            nsversion: libgtkhex_version_major.to_string(),
            namespace: 'Hex',
        symbol_prefix: 'hex',
            link_with: libgtkhex,
         dependencies: libgtkhex_deps,
             includes: [ 'Gtk-4.0' ],
              install: true,
      install_dir_gir: ghex_girdir,
  install_dir_typelib: ghex_typelibdir,
           extra_args: [ '--warn-all' ],
  )
endif
