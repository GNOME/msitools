msidiff = configure_file(
  input: 'msidiff.in',
  output: 'msidiff',
  configuration: config,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr-xr-x',
)

msidump = configure_file(
  input : 'msidump.in',
  output: 'msidump',
  configuration: config,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr-xr-x',
)

msibuild = executable('msibuild',
  'msibuild.c',
  'sqldelim.c',
  libmsi_enums_h,
  dependencies: libmsi,
  install: true,
)

misiinfo = executable('msiinfo',
  'msiinfo.c',
  libmsi_enums_h,
  include_directories: inc_dirs,
  dependencies: libmsi,
  install: true,
)

if get_option('introspection')
  msiextract = executable('msiextract',
    'msiextract.vala',
    libmsi_enums_h,
    config_h,
    include_directories: inc_dirs,
    vala_args: [
      '--enable-experimental',
      '--pkg', 'gio-unix-2.0',
      '--pkg', 'libgcab-1.0',
      '--pkg', 'posix',
      '--pkg', 'config',
      '--vapidir=@0@'.format(meson.project_source_root()),
      '--vapidir=@0@'.format(meson.project_build_root() / 'libmsi'),
    ],
    c_args: ['-include', meson.project_build_root() / 'config.h'],
    dependencies: [libmsi, libgcab, libmsi_vapi],
    install: true,
  )

  subdir('wixl')
endif
