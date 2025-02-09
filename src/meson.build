headphone_manager_sources = [
  'alsa.c',
  'bluetooth.c',
  'events.c',
  'headphone-manager.c',
  'main.c',
  'mpris.c'
]

headphone_manager_deps = [
  dependency('glib-2.0'),
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
  dependency('alsa')
]

executable('headphone-manager', headphone_manager_sources,
  dependencies: headphone_manager_deps,
  install_dir: bindir,
  install: true,
)