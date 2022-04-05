if build_gtk_doc and not get_option('introspection').allowed()
  error('API reference requires introspection.')
endif

toml_conf = configuration_data()
toml_conf.set('version', libgtkhex_version)

gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))
gir_apiname = 'gtkhex-@0@.0'.format(libgtkhex_api_version)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

if get_option('gtk_doc')
  gtkhex_toml = configure_file(
    input: 'GtkHex.toml.in',
    output: 'GtkHex.toml',
    configuration: toml_conf,
    install: true,
    install_dir: join_paths(ghex_docdir, gir_apiname),
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
    input: [ gtkhex_toml, gtkhex_gir[0] ],
    output: gir_apiname,
    command: docs_cmd,
    build_by_default: true,
    install: true,
    install_dir: ghex_docdir,
  )
endif
