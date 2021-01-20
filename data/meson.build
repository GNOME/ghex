ghex_desktop = i18n.merge_file(
  input: 'org.gnome.GHex.desktop.in',
  output: 'org.gnome.GHex.desktop',
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
    args: [ghex_desktop]
  )
endif

pkg_conf = configuration_data()
pkg_conf.set('prefix', ghex_prefix)
pkg_conf.set('exec_prefix', ghex_prefix)
pkg_conf.set('libdir', ghex_libdir)
pkg_conf.set('includedir', ghex_includedir)

pkg_conf.set('VERSION', meson.project_version())

pkg_conf.set_quoted('XML_SHADED_BOX_MAX', shaded_box_max.to_string())

configure_file(
  input: 'gtkhex-4.pc.in',
  output: 'gtkhex-4.pc',
  configuration: pkg_conf,
  install_dir: ghex_pkgconfigdir,
  install: true
)

configure_file(
  input: 'org.gnome.GHex.gschema.xml.in',
  output: 'org.gnome.GHex.gschema.xml',
  configuration: pkg_conf,
  install_dir: ghex_schemasdir,
  install: true
)

ghex_appdata = i18n.merge_file(
  input: 'org.gnome.GHex.appdata.xml.in',
  output: 'org.gnome.GHex.appdata.xml',
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
    args: ['validate-relax', '--nonet', ghex_appdata]
  )
endif
