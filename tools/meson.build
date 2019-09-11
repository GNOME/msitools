

config_scripts = configuration_data()
config_scripts.set('PACKAGE_BUGREPORT', project_url + '/issues')
config_scripts.set('PACKAGE_VERSION', meson.project_version())


msidiff = configure_file(
  input : 'msidiff.in',
  output: 'msidiff',
  configuration: config_scripts,
  install: true,
  install_dir: get_option('bindir'),
)

msidump = configure_file(
  input : 'msidump.in',
  output: 'msidump',
  configuration: config_scripts,
  install: true,
  install_dir: get_option('bindir'),
)

msibuild = executable('msibuild',
  'msibuild.c',
  'sqldelim.c',
  libmsi_enums_h,

  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
    uuid,
  ],
  install: true,
)

msiinfo = executable('msiinfo',
  'msiinfo.c',
  libmsi_enums_h,

  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
  ],
  install: true,
)

msiextract = executable('msiextract',
  'msiextract.vala',
  libmsi_enums_h,

  c_args: [ '-include', config_h.full_path(), ],
  include_directories: [
    rootInclude,
    includeInclude,
  ],
  vala_args: [
    '--enable-experimental',
    '--pkg', 'gio-unix-2.0',
    '--pkg', 'libgcab-1.0',
    '--pkg', 'libmsi-1.0',
    '--pkg', 'posix',
    '--pkg', 'config',
    '--vapidir=@0@'.format(meson.source_root()),
    '--vapidir=@0@'.format(meson.build_root() / 'libmsi'),
  ],
  link_with: libmsi,
  dependencies: [
    glib,
    libgcab,
  ],
)

subdir('wixl')
