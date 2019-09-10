

config_scripts = configuration_data()
config_scripts.set('PACKAGE_BUGREPORT', project_url + '/issues')
config_scripts.set('PACKAGE_VERSION', meson.project_version())


msidiff = configure_file(
  input : 'msidiff.in',
  output: 'msidiff',
  configuration: config_scripts,
)

msidump = configure_file(
  input : 'msidump.in',
  output: 'msidump',
  configuration: config_scripts,
)


msibuild = executable('msibuild',
  'msibuild.c',
  'sqldelim.c',

  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
  ],
)

msiinfo = executable('msiinfo',
  'msiinfo.c',

  include_directories: [
    rootInclude,
    includeInclude,
  ],
  link_with: libmsi,
  dependencies: [
    glib,
  ],
)
