libghex_sources = [
  'accessiblegtkhex.c',
  'accessiblegtkhex.h',
  'accessiblegtkhexfactory.c',
  'accessiblegtkhexfactory.h',
  'gtkhex.c',
  'gtkhex-private.h',
  'hex-document.c'
]

libghex_headers = [
  'hex-document.h',
  'gtkhex.h'
]

libghex_deps = [
  atk_dep,
  gtk_dep,
  gail_dep
]

libghex_c_args = [
  '-DG_LOG_DOMAIN="libgtkhex-@0@"'.format(libghex_version_major)
]

libghex_link_args = [
  '-Wl,--no-undefined'
]

install_headers(
  libghex_headers,
  subdir: 'gtkhex-@0@'.format(libghex_version_major)
)

libghex = library(
  'gtkhex-@0@'.format(libghex_version_major),
  libghex_sources + libghex_headers,
  version: '0.0.0',
  include_directories: ghex_root_dir,
  dependencies: libghex_deps,
  c_args: libghex_c_args,
  link_args: libghex_link_args,
  install: true
)

libghex_dep = declare_dependency(
  link_with: libghex,
  include_directories: include_directories('.'),
  dependencies: libghex_deps,
  sources: libghex_headers
)
