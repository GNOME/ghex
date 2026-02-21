if not get_option('introspection').allowed()
  error('API reference requires introspection.')
endif

gidocgen = find_program('gi-docgen', required: true)
gir_apiname = 'gtkhex-@0@.0'.format(libgtkhex_api_version)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

toml_conf = configuration_data()
toml_conf.set('version', libgtkhex_version)

gtkhex_toml = configure_file(
  input: 'GtkHex.toml.in',
  output: 'GtkHex.toml',
  configuration: toml_conf,
  install: true,
  install_dir: join_paths(docdir, gir_apiname),
)

docs_cmd = [
  gidocgen,
  'generate',
  gidocgen_common_args,
  '--config=@INPUT0@',
  '--output-dir=@OUTPUT@',
  '--content-dir=@0@'.format(meson.current_source_dir()),
  '@INPUT1@',
]

custom_target('libgtkhex-doc',
  input: [ gtkhex_toml, libgtkhex_gir[0] ],
  output: gir_apiname,
  command: docs_cmd,
  build_by_default: true,
  install: true,
  install_dir: docdir,
)
