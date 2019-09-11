
project('msitools',
  'c', 'cpp', 'vala',
  version: '0.99.18',
  default_options: [
    'warning_level=2',
  ],
)

i18n = import('i18n')
gnome = import('gnome')

config = configuration_data()

###############################################################################
# Project configuration

project_url = 'https://gitlab.gnome.org/GNOME/msitools'


getttext_package = meson.project_name()

config.set_quoted('PACKAGE', meson.project_name())
config.set_quoted('GETTEXT_PACKAGE', getttext_package)

config.set_quoted('VERSION',         '@VCS_TAG@')
config.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')

config.set('PACKAGE_STRING', 'PACKAGE " " PACKAGE_VERSION')

###############################################################################
# System detection

os = host_machine.system()

platform_windows = (
  os.contains('windows') or
  os.contains('mingw') or
  os.contains('cygwin')
)

config.set('USE_GMTIME_S', platform_windows)

###############################################################################
# Compiler arguments

c_args = [
  '-Wno-unused-variable',
  '-Wno-unused-parameter',
  '-Wno-sign-compare',
]

add_project_arguments(c_args, language: [ 'c', 'cpp'])

###############################################################################
# Tools

bison = [ find_program('bison'), '-y' ]

perl = find_program('perl')

vapigen = find_program('vapigen')

###############################################################################
# Dependencies


glib    = dependency('glib-2.0', version: '>=2.12')
gobject = dependency('gobject-2.0', version: '>=0.9.4')
gio     = dependency('gio-2.0', version: '>=2.14')
libgsf  = dependency('libgsf-1')
uuid    = dependency('uuid', version: '>=1.41.3', required: false)


libgcab = dependency('libgcab-1.0', version: '>=0.1.10')
libxml  = dependency('libxml-2.0', version: '>=2.7')


###############################################################################

rootInclude = include_directories('.')

config.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
config.set('PKGDATADIR', '"@0@" @1@'.format(
  get_option('prefix') / get_option('datadir') / '',
  'PACKAGE_STRING'
))

config_h_in = configure_file(
  output: 'config.h.in',
  configuration: config,
)

config_h = vcs_tag(
  input : config_h_in,
  output: 'config.h',
  command: [ 'bash', './build-aux/git-version-gen', '.tarball-version', ],
)


subdir('completion')
subdir('data')
subdir('po')

subdir('include')
subdir('libmsi')
subdir('tools')
subdir('tests')
