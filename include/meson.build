libmsi_types_h = files('libmsi-types.h')

libmsi_includedir = join_paths(includedir, 'libmsi-1.0')

libmsi_enums_h = gnome.mkenums('libmsi-enums',
  h_template: 'libmsi-enums.h.etemplate',
  sources: libmsi_types_h,
  install_header: true,
  install_dir: libmsi_includedir,
)

libmsi_headers = files(
  'libmsi-database.h',
  'libmsi-query.h',
  'libmsi-record.h',
  'libmsi-summary-info.h',
  'libmsi-types.h',
  'libmsi.h',
)

introspectable_headers = libmsi_headers + [libmsi_enums_h]

install_data(
  libmsi_headers,
  install_dir: libmsi_includedir,
)
