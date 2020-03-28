
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
  input : 'sql-parser.y',
  output: [
    'sql-parser.h',
    # 'sql-parser.c',
  ],
  command: [
    bison, '@INPUT@', '-o', '@OUTPUT0@',
  ],
)


libmsi = static_library('msi',
  libmsi_sources,

  include_directories: [
    includeInclude,
    rootInclude,
  ],
  dependencies: [
    glib,
    gio,
    gobject,
    libgsf,
  ],
)



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
  dependencies: [

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


libmsi_vapi = custom_target('libmsi-1.0.vapi',
  input: [
    libmsi_gir[0],
    'Libmsi-1.0.metadata',
  ],
  output: 'libmsi-1.0.vapi',

  command: [
    vapigen, '-q',
    '--metadatadir=@0@'.format(meson.current_source_dir()),
    '--vapidir=@0@'.format(meson.current_source_dir()),
    '--directory=@0@'.format(meson.current_build_dir()),
    '--library', 'libmsi-1.0',
    '--pkg', 'gio-2.0',
    '@INPUT0@',
  ],

  install: true,
  install_dir: get_option('datadir') / 'vala' / 'vapi',
)
