

testrecord = executable('testrecord',
  'testrecord.c',
  libmsi_enums_h,
  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
  ],
)

testdatabase = executable('testdatabase',
  'testdatabase.c',
  libmsi_enums_h,
  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
  ],
)


if platform_windows
  testsuminfo = executable('testsuminfo',
    'testsuminfo.c',
    libmsi_enums_h,
    include_directories: [
      rootInclude,
      includeInclude,
    ],
    link_with: libmsi,
    dependencies: [
      glib,
    ],
  )
endif
