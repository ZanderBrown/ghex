ghex_sources = [
  'application.c',
  'application.h',
  'chartable.c',
  'chartable.h',
  'configuration.h',
  'converter.c',
  'converter.h',
  'dialog.c',
  'dialog.h',
  'factory.c',
  'factory.h',
  'findreplace.c',
  'findreplace.h',
  'find.c',
  'find.h',
  'goto.c',
  'goto.h',
  'replace.c',
  'replace.h',
  'ghex-window.c',
  'ghex-window.h',
  'hex-dialog.c',
  'hex-dialog.h',
  'hex-document-ui.c',
  'main.c',
  'preferences.c',
  'preferences.h',
  'print.c',
  'print.h',
  'ui.c',
  'ui.h'
]

ghex_deps = [
  atk_dep,
  gio_dep,
  gtk_dep
]

ghex_res = gnome.compile_resources(
  'ghex-resources',
  'ghex.gresource.xml',
  c_name: 'ghex'
)

ghex_c_args = [
  '-DG_LOG_DOMAIN="GHex"'
]

libdir_abs = join_paths(get_option('prefix'), get_option('libdir'))

ghex = executable(
  meson.project_name(),
  ghex_sources + ghex_res,
  include_directories: ghex_root_dir,
  dependencies: ghex_deps + [libghex_dep, ],
  c_args: ghex_c_args,
  install: true,
  install_rpath: libdir_abs
)
