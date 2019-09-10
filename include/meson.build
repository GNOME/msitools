includeInclude = include_directories('.')

libmsi_types_h = files('libmsi-types.h')

libmsi_enums = gnome.mkenums('libmsi-enums',
  h_template: 'libmsi-enums.h.etemplate',
  sources: libmsi_types_h,
)
