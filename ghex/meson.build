# define app_id and resource_base_path

if get_option('development')
  app_id = 'org.gnome.GHex.Devel'
  resource_base_path = '/org/gnome/GHex/Devel'
else
  app_id = 'org.gnome.GHex'
  resource_base_path = '/org/gnome/GHex'
endif

# < config.h >

# just to avoid manually punching it into the XML files as well as the source.
shaded_box_max = 1000

config_h = configuration_data()
config_h.set_quoted('APP_ID', app_id)
config_h.set_quoted('RESOURCE_BASE_PATH', resource_base_path)
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
config_h.set_quoted('PACKAGE_DATADIR', datadir)
config_h.set_quoted('PACKAGE_LIBDIR', libdir)
config_h.set_quoted('PACKAGE_LOCALE_DIR', localedir)
config_h.set_quoted('PACKAGE_DOCDIR', docdir)

config_h.set('VERSION', 'PACKAGE_VERSION')
config_h.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
config_h.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')

config_h.set('CONFIG_H_SHADED_BOX_MAX', shaded_box_max)

config_h.set('DEBUG', get_option('debug'))

configure_file(
  output: 'config.h',
  configuration: config_h
)

# </ config.h >

# configuration for .in files

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

ghex_res = gnome.compile_resources(
  'ghex-resources',
  res_in,
  c_name: 'ghex',
  dependencies: app_ui
)

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
  ghex_sources + ghex_res,
  dependencies: [adw_dep, libgtkhex_dep],
  c_args: ghex_c_args,
  install: true
)

subdir('data')
subdir('icons')
subdir('po')
subdir('help')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
