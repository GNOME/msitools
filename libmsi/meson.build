libmsi_sources = files(
  'alter.c',
  'create.c',
  'debug.c',
  'debug.h',
  'delete.c',
  'distinct.c',
  'drop.c',
  'insert.c',
  'libmsi-database.c',
  'libmsi-istream.c',
  'libmsi-istream.h',
  'libmsi-query.c',
  'libmsi-record.c',
  'libmsi-summary-info.c',
  'list.h',
  'msipriv.h',
  'query.h',
  'select.c',
  'storages.c',
  'streams.c',
  'string.c',
  'table.c',
  'tokenize.c',
  'update.c',
  'where.c',
)

libmsi_enums_c = gnome.mkenums('libmsi-enums',
  c_template: 'libmsi-enums.c.etemplate',
  sources: libmsi_types_h,
)

libmsi_sources += [
  libmsi_enums_h,
  libmsi_enums_c,
]

libmsi_sources += custom_target('sql-parser',
  input: 'sql-parser.y',
  output: ['sql-parser.c', 'sql-parser.h'],
  command: [
    bison, '@INPUT@', '-y', '-Wno-yacc', '--defines', '-o', '@OUTPUT0@',
  ],
)

libmsi_deps = [
  glib,
  gio,
  gobject,
  libgsf,
]

libmsi = shared_library('msi-1.0',
  libmsi_sources,
  include_directories: inc_dirs,
  dependencies: libmsi_deps,
  version: '0.1.0',
  install: true,
)

pc = import('pkgconfig')
pc.generate(
  libraries: libmsi,
  requires: [ 'gio-2.0' ],
  subdirs: 'libmsi-1.0',
  version: meson.project_version(),
  name: 'libmsi-1.0',
  filebase: 'libmsi-1.0',
  description: 'MSI manipulation library',
)

if get_option('introspection')
  gir_extra_args = ['--c-include=libmsi.h']
  gir_env = []

  if meson.is_cross_build() and host_machine.system() == 'windows'
    # Meson does not pass the cross-file exe_wrapper to g-ir-scanner.  Run the
    # scanner's temporary dumper through wine so it can load the target DLLs.
    gir_runner = find_program('wine', native: true, required: false)
    if not gir_runner.found()
      error('wine is required to generate introspection data for Windows')
    endif

    mingw_sysroot = meson.get_external_property('mingw_sysroot',
      '/usr/x86_64-w64-mingw32/sys-root/mingw')
    wine_sysroot = 'Z:' + mingw_sysroot.replace('/', '\\')
    wine_build = 'Z:' + meson.current_build_dir().replace('/', '\\')

    gir_extra_args += ['--use-binary-wrapper=' + gir_runner.full_path()]
    gir_env += [
      'GI_HOST_OS=nt',
      'WINEPATH=' + wine_sysroot + '\\bin;' + wine_build,
    ]

    mingw_dlltool = meson.get_external_property('mingw_dlltool', '')
    if mingw_dlltool != ''
      gir_env += ['DLLTOOL=' + mingw_dlltool]
    endif
  endif

  libmsi_gir = gnome.generate_gir(libmsi,
    namespace: 'Libmsi',
    nsversion: '1.0',
    export_packages: 'libmsi-1.0',
    sources: [
      introspectable_headers,
      libmsi_enums_c,
      'libmsi-database.c',
      'libmsi-query.c',
      'libmsi-record.c',
      'libmsi-summary-info.c',
    ],
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
    ],
    extra_args: gir_extra_args,
    env: gir_env,
    install: true,
  )

  libmsi_vapi = gnome.generate_vapi('libmsi-1.0',
    sources: libmsi_gir[0],
    vapi_dirs: meson.current_source_dir(),
    metadata_dirs: meson.current_source_dir(),
    packages: 'gio-2.0',
    install: true,
  )
endif

libmsi = declare_dependency(
  link_with: libmsi,
  include_directories: inc_dirs,
  dependencies: libmsi_deps,
)
