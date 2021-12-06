if get_option('development')
  app_name = 'GHex (Development)'
else    
  app_name = 'GHex'
endif

desktop_file_in_config = configuration_data()
desktop_file_in_config.set('app_id', app_id)
desktop_file_in_config.set('app_name', app_name)
desktop_file_in = configure_file(
          input: 'org.gnome.GHex.desktop.in.in',
         output: '@0@.desktop.in'.format(app_id),
  configuration: desktop_file_in_config,
)

desktop_file = i18n.merge_file(
  input: desktop_file_in,
  output: '@0@.desktop'.format(app_id),
  po_dir: ghex_po_dir,
  type: 'desktop',
  install_dir: ghex_applicationsdir,
  install: true
)

desktop_file_validate_prg = find_program(
  'desktop-file-validate',
  required: false
)

if desktop_file_validate_prg.found()
  test(
    'Validate desktop file',
    desktop_file_validate_prg,
    args: [desktop_file]
  )
endif

pkg_conf = configuration_data()
pkg_conf.set('prefix', ghex_prefix)
pkg_conf.set('exec_prefix', ghex_prefix)
pkg_conf.set('libdir', ghex_libdir)
pkg_conf.set('includedir', ghex_includedir)

pkg_conf.set('VERSION', meson.project_version())


configure_file(
  input: 'gtkhex-4.pc.in',
  output: 'gtkhex-4.pc',
  configuration: pkg_conf,
  install_dir: ghex_pkgconfigdir,
  install: true
)

schema_conf = configuration_data()
schema_conf.set_quoted('XML_SHADED_BOX_MAX', shaded_box_max.to_string())

configure_file(
  input: 'org.gnome.GHex.gschema.xml.in',
  output: 'org.gnome.GHex.gschema.xml',
  configuration: schema_conf,
  install_dir: ghex_schemasdir,
  install: true
)

appdata_in_conf = configuration_data()
appdata_in_conf.set('app_id', app_id)
appdata_in = configure_file(
  input: 'org.gnome.GHex.appdata.xml.in.in',
  output: '@0@.appdata.xml.in'.format(app_id),
  configuration: appdata_in_conf,
)

appdata = i18n.merge_file(
  input: appdata_in,
  output: '@0@.appdata.xml'.format(app_id),
  po_dir: ghex_po_dir,
  install_dir: ghex_appdatadir,
  install: true
)

appstream_util_prg = find_program(
  'appstream-util',
  required: false
)

if appstream_util_prg.found()
  test(
    'Validate appstream file',
    appstream_util_prg,
    args: ['validate-relax', '--nonet', appdata]
  )
endif
