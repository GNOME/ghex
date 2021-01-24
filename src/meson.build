libghex_sources = [
  'gtkhex.c',
  'hex-document.c',
  'gtkhex-layout-manager.c'
]

libghex_headers = [
  'hex-document.h',
  'gtkhex.h'
]

libghex_deps = [
  gtk_dep,
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
  'gtkhex.c',
  'gtkhex.h',
  'gtkhex-layout-manager.c',
  'gtkhex-layout-manager.h',
  'hex-dialog.c',
  'hex-dialog.h',
  'hex-document.c',
  'hex-document.h',
  'main.c',
  'preferences.c',
  'preferences.h',
  'print.c',
  'print.h'
]

ghex_deps = [
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
