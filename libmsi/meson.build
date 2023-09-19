libmsi_sources = files(
  'alter.c',
  'create.c',
  'debug.c',
  'debug.h',
  'delete.c',
  'distinct.c',
  'drop.c',
  'insert.c',
  'libmsi-database.c',
  'libmsi-istream.c',
  'libmsi-istream.h',
  'libmsi-query.c',
  'libmsi-record.c',
  'libmsi-summary-info.c',
  'list.h',
  'msipriv.h',
  'query.h',
  'select.c',
  'storages.c',
  'streams.c',
  'string.c',
  'table.c',
  'tokenize.c',
  'update.c',
  'where.c',
)

libmsi_enums_c = gnome.mkenums('libmsi-enums',
  c_template: 'libmsi-enums.c.etemplate',
  sources: libmsi_types_h,
)

libmsi_sources += [
  libmsi_enums_h,
  libmsi_enums_c,
]

libmsi_sources += custom_target('sql-parser',
  input: 'sql-parser.y',
  output: ['sql-parser.c', 'sql-parser.h'],
  command: [
    bison, '@INPUT@', '-y', '-Wno-yacc', '--defines', '-o', '@OUTPUT0@',
  ],
)

libmsi_deps = [
  glib,
  gio,
  gobject,
  libgsf,
]

libmsi = shared_library('msi-1.0',
  libmsi_sources,
  include_directories: inc_dirs,
  dependencies: libmsi_deps,
  version: '0.0.0',
  install: true,
)

pc = import('pkgconfig')
pc.generate(
  libraries: libmsi,
  requires: [ 'gio-2.0' ],
  subdirs: 'libmsi-1.0',
  version: meson.project_version(),
  name: 'libmsi-1.0',
  filebase: 'libmsi-1.0',
  description: 'MSI manipulation library',
)

if get_option('introspection')
  libmsi_gir = gnome.generate_gir(libmsi,
    namespace: 'Libmsi',
    nsversion: '1.0',
    export_packages: 'libmsi-1.0',
    sources: [
      introspectable_headers,
      libmsi_enums_c,
      'libmsi-database.c',
      'libmsi-query.c',
      'libmsi-record.c',
      'libmsi-summary-info.c',
    ],
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
    ],
    extra_args: [
      '--c-include=libmsi.h',
    ],
    install: true,
  )

  libmsi_vapi = gnome.generate_vapi('libmsi-1.0',
    sources: libmsi_gir[0],
    vapi_dirs: meson.current_source_dir(),
    metadata_dirs: meson.current_source_dir(),
    packages: 'gio-2.0',
    install: true,
  )
endif

libmsi = declare_dependency(
  link_with: libmsi,
  include_directories: inc_dirs,
  dependencies: libmsi_deps,
)
