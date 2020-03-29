libmsi_types_h = files('libmsi-types.h')

libmsi_enums_h = gnome.mkenums('libmsi-enums',
  h_template: 'libmsi-enums.h.etemplate',
  sources: libmsi_types_h,
)

introspectable_headers = files(
  'libmsi-database.h',
  'libmsi-query.h',
  'libmsi-record.h',
  'libmsi-summary-info.h',
  'libmsi-types.h',
  'libmsi.h',
) + [
  libmsi_enums_h,
]
