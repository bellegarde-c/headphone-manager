configure_file(
  input: 'headphone-manager.service.in',
  output: 'headphone-manager.service',
  configuration: config_h,
  install: true,
  install_dir: systemd_user_dir
)

schema_src = 'org.adishatz.HeadphoneManager.gschema.xml'
compiled = gnome.compile_schemas(build_by_default: true,
                                 depend_files: files(schema_src))

install_data(schema_src,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
  args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

gnome.post_install(glib_compile_schemas: true)