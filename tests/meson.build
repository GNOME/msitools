c_args = ['-Wno-unused']

testrecord = executable('testrecord',
  'testrecord.c',
  libmsi_enums_h,
  c_args: c_args,
  include_directories: inc_dirs,
  dependencies: libmsi,
)

testdatabase = executable('testdatabase',
  'testdatabase.c',
  libmsi_enums_h,
  c_args: c_args,
  include_directories: inc_dirs,
  dependencies: libmsi,
)

if host_machine.system() == 'windows'
  testsuminfo = executable('testsuminfo',
    'testsuminfo.c',
    libmsi_enums_h,
    c_args: c_args,
    include_directories: inc_dirs,
    dependencies: libmsi,
  )
endif

test('Tools tests',
  bats,
  workdir: meson.current_build_dir(),
  args: ['--tap', meson.current_source_dir()],
  protocol: 'tap',
  depends: [testrecord, testdatabase],
  env: {
    'SRCDIR': meson.project_source_root(),
    'BUILDDIR': meson.project_build_root(),
  },
)
