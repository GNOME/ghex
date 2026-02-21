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

pkg_conf.set('LIBGTKHEX_VERSION', libgtkhex_version)

configure_file(
  input: 'gtkhex-4.pc.in',
  output: 'gtkhex-4.pc',
  configuration: pkg_conf,
  install_dir: ghex_pkgconfigdir,
  install: true
)

schema_conf = configuration_data()
schema_conf.set('shaded_box_max', shaded_box_max)
schema_conf.set('app_id', app_id)
schema_conf.set('resource_base_path', resource_base_path)

configure_file(
  input: 'org.gnome.GHex.gschema.xml.in',
  output: '@0@.gschema.xml'.format(app_id),
  configuration: schema_conf,
  install_dir: ghex_schemasdir,
  install: true
)

metainfo_in_conf = configuration_data()
metainfo_in_conf.set('app_id', app_id)
metainfo_in = configure_file(
  input: 'org.gnome.GHex.metainfo.xml.in.in',
  output: '@0@.metainfo.xml.in'.format(app_id),
  configuration: metainfo_in_conf,
)

metainfo = i18n.merge_file(
  input: metainfo_in,
  output: '@0@.metainfo.xml'.format(app_id),
  po_dir: ghex_po_dir,
  install_dir: ghex_metainfodir,
  install: true
)

appstreamcli_prg = find_program(
  'appstreamcli',
  required: false
)

if appstreamcli_prg.found()
  test(
    'Validate appstream file',
    appstreamcli_prg,
    args: ['validate', '--no-net', '--explain', metainfo]
  )
endif

service_conf = configuration_data()
service_conf.set('app_id', app_id)
service_conf.set('bindir', ghex_bindir)
configure_file(
  input: 'org.gnome.GHex.service.in',
  output: '@0@.service'.format(app_id),
  configuration: service_conf,
  install_dir: ghex_servicedir
)
