
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

libmsi_sources += gnome.mkenums('libmsi-enums',
  c_template: 'libmsi-enums.c.etemplate',
  sources: libmsi_types_h,
)

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
