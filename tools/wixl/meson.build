
libcommon = static_library('common',
  'util.vala',

  vala_header: 'libcommon.h',
  vala_vapi: 'common.vapi',
  vala_args: [
    '--enable-experimental',
    '--pkg', 'gio-2.0',
    '--pkg', 'posix',
    '--library', 'common',
    '--vapidir=@0@'.format(meson.source_root() / 'vapi'),
    '--vapidir=@0@'.format(meson.current_source_dir()),
  ],


  dependencies: [
    glib,
    uuid,
  ],
)




wixl = executable('wixl',
  'builder.vala',
  'msi-default.vala',
  'msi.vala',
  'preprocessor.vala',
  'wix.vala',
  'wixl.vala',

  vala_args: [
    '-H', 'wixl.h',
    '--use-header',

    '--enable-experimental',
    '--pkg', 'config',
    '--pkg', 'libmsi-1.0',
    '--pkg', 'posix',
    '--vapidir=@0@'.format(meson.source_root()),
    '--vapidir=@0@'.format(meson.build_root() / 'libmsi'),
  ],

  c_args: [
    '-include', config_h.full_path(),
  ],

  include_directories: [
    rootInclude,
    includeInclude,
  ],

  link_with: [
    libmsi,
    libcommon,
  ],

  dependencies: [
    glib,
    libgcab,
    libxml,
  ],
)


wixl_heat = executable('wixl-heat',
  'wixl-heat.vala',

  vala_args: [
    '-H', 'wixl.h',
    '--use-header',

    '--enable-experimental',
    '--pkg', 'config',
    '--pkg', 'libmsi-1.0',
    '--pkg', 'posix',
    '--vapidir=@0@'.format(meson.source_root()),
    '--vapidir=@0@'.format(meson.build_root() / 'libmsi'),
  ],

  c_args: [
    '-include', config_h.full_path(),
  ],

  include_directories: [
    rootInclude,
    includeInclude,
  ],

  link_with: [
    libmsi,
    libcommon,
  ],

  dependencies: [
    glib,
    libgcab,
    libxml,
  ],
)
