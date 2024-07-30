project('msitools',
  'c', 'vala',
  version: run_command('build-aux/git-version-gen', '@0@/.tarball-version'.format(meson.project_source_root()), check: true).stdout().strip(),
  meson_version: '>= 1.4',
)
meson.add_dist_script('build-aux/meson-dist', meson.project_version(), meson.project_source_root())

gnome = import('gnome')

perl = find_program('perl')
bison = find_program('bison')
bats = find_program('subprojects/bats-core/bin/bats')
glib = dependency('glib-2.0', version: '>= 2.12')
gobject = dependency('gobject-2.0', version: '>= 0.9.4')
gio = dependency('gio-2.0', version: '>= 2.14')
libgsf = dependency('libgsf-1')
libgcab = dependency('libgcab-1.0', version: '>= 0.1.10')
libxml = dependency('libxml-2.0', version: '>= 2.7')

config = configuration_data()
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('PACKAGE_VERSION', meson.project_version())
config.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
config.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.gnome.org/GNOME/msitools/issues')
config.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
config.set_quoted('DATADIR', get_option('prefix') / get_option('datadir'))

config_h = configure_file(
  output: 'config.h',
  configuration: config,
)

prefix = get_option('prefix')
includedir = join_paths(prefix, get_option('includedir'))

inc_dirs = include_directories('.', 'include')

subdir('completion')
subdir('data')
subdir('po')
subdir('include')
subdir('libmsi')
subdir('tools')

if host_machine.system() != 'windows' #FIXME
  subdir('tests')
endif
