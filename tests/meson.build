

testrecord = executable('testrecord',
  'testrecord.c',
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
