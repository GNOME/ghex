libghex_sources = [
  'gtkhex.c',
  'hex-document.c',
  'gtkhex-layout-manager.c',
  'gtkhex-paste-data.c'
]

libghex_headers = [
  'hex-document.h',
  'gtkhex.h',
  'gtkhex-paste-data.h'
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
  dependencies: ghex_deps + [libghex_dep, ],
  c_args: ghex_c_args,
  install: true
)
