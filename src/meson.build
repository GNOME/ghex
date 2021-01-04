libghex_sources = [
  'gtkhex.c',
  'hex-document.c'
]

libghex_headers = [
  'hex-document.h',
  'gtkhex.h'
]

libghex_deps = [
  atk_dep,
  gtk_dep,
  gail_dep
]

libghex_c_args = [
  '-DG_LOG_DOMAIN="libgtkhex-3"'
]

libghex_link_args = cc.get_supported_link_arguments([
  '-Wl,--no-undefined'
])

install_headers(
  libghex_headers,
  subdir: 'gtkhex-@0@'.format(libghex_version_major)
)

libghex = library(
  'gtkhex-@0@'.format(libghex_version_major),
  libghex_sources + libghex_headers,
  version: '0.0.0',
  darwin_versions: ['1', '1.0'],
  include_directories: ghex_root_dir,
  dependencies: libghex_deps,
  c_args: libghex_c_args,
  link_args: libghex_link_args,
  install: true
)

libghex_dep = declare_dependency(
  link_with: libghex,
  include_directories: ghex_root_dir,
  dependencies: libghex_deps,
  sources: libghex_headers
)

ghex_sources = [
  'chartable.c',
  'chartable.h',
  'config.c',
  'configuration.h',
  'converter.c',
  'converter.h',
  'findreplace.c',
  'findreplace.h',
  'ghex-window.c',
  'ghex-window.h',
  'hex-dialog.c',
  'hex-dialog.h',
  'hex-document-ui.c',
  'main.c',
  'preferences.c',
  'preferences.h',
  'print.c',
  'print.h',
  'ui.c',
  'ui.h'
]

ghex_deps = [
  atk_dep,
  gio_dep,
  gtk_dep
]

ghex_res = gnome.compile_resources(
  'ghex-resources',
  'ghex.gresource.xml',
  c_name: 'ghex'
)

ghex_c_args = [
  '-DG_LOG_DOMAIN="GHex"'
]

ghex = executable(
  meson.project_name(),
  ghex_sources + ghex_res,
  include_directories: ghex_root_dir,
  dependencies: ghex_deps + [libghex_dep, ],
  c_args: ghex_c_args,
  install: true
)
