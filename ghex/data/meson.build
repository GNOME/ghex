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
  po_dir: po_dir,
  type: 'desktop',
  install_dir: applicationsdir,
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

schema_conf = configuration_data()
schema_conf.set('shaded_box_max', shaded_box_max)
schema_conf.set('app_id', app_id)
schema_conf.set('resource_base_path', resource_base_path)

configure_file(
  input: 'org.gnome.GHex.gschema.xml.in',
  output: '@0@.gschema.xml'.format(app_id),
  configuration: schema_conf,
  install_dir: schemasdir,
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
  po_dir: po_dir,
  install_dir: metainfodir,
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
service_conf.set('bindir', bindir)
configure_file(
  input: 'org.gnome.GHex.service.in',
  output: '@0@.service'.format(app_id),
  configuration: service_conf,
  install_dir: servicedir
)
