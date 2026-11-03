if get_option('msi')
  fs = import('fs')
  msi_build_script = find_program('build-msi.py')
  msi_output = 'msitools-v@0@.msi'.format(meson.project_version())

  if get_option('msi-wixl') != ''
    msi_wixl = find_program(get_option('msi-wixl'))
    # Assume wixl-heat sits next to the requested wixl.
    msi_wixl_heat = find_program(
      fs.parent(msi_wixl.full_path()) / 'wixl-heat',
      required: false,
    )
    if not msi_wixl_heat.found()
      error('wixl-heat was not found next to ' + msi_wixl.full_path())
    endif
  elif is_variable('wixl') and not meson.is_cross_build()
    # A cross-built wixl is a host binary and cannot be run while building
    # the MSI, so only reuse the in-tree wixl for native builds.
    msi_wixl = wixl
    msi_wixl_heat = wixl_heat
  else
    msi_wixl = find_program('wixl')
    msi_wixl_heat = find_program('wixl-heat')
  endif

  # The target runs `meson install` on the build tree, so depend on everything
  # that goes into the payload: otherwise a plain `ninja msitools-msi` packages
  # stale binaries. Always rerun the staging as well, because the package is not
  # made out of date by rebuilding the tree.
  msi_payload_deps = []
  foreach name : ['msiinfo', 'msibuild', 'msiextract', 'wixl', 'wixl_heat']
    if is_variable(name)
      msi_payload_deps += [get_variable(name)]
    endif
  endforeach

  msitools_msi = custom_target('msitools-msi',
    output: msi_output,
    command: [
      msi_build_script,
      find_program('meson'),
      meson.project_build_root(),
      meson.project_source_root(),
      get_option('prefix'),
      host_machine.system(),
      msi_wixl,
      msi_wixl_heat,
      meson.project_version(),
      '@OUTPUT@',
    ],
    depends: msi_payload_deps,
    depend_files: files(
      'build-msi.py',
      'validate-msi.py',
      'smoke-msi.py',
      'msitools.wxs',
      'msitools.cmd',
      'License.rtf',
      'banner.bmp',
      'dialog.bmp',
      'msitools.ico',
      'deps.wxi',
      '../wixl/libgsf.wxi',
      '../wixl/libgcab1.wxi',
      '../wixl/bzip2.wxi',
      '../wixl/glib2.wxi',
      '../wixl/libxml2.wxi',
      '../wixl/libffi.wxi',
      '../wixl/gettext.wxi',
      '../wixl/pcre2.wxi',
      '../wixl/zlib.wxi',
      '../wixl/win-iconv.wxi',
      '../wixl/gcc.wxi',
      '../wixl/gcc-c++.wxi',
      '../wixl/termcap.wxi',
      '../wixl/winpthreads.wxi',
    ),
    build_by_default: false,
    build_always_stale: true,
    install: false,
  )

  alias_target('msitools-msi', msitools_msi)

  validate_msi = find_program('validate-msi.py')

  # Validation runs on the build machine, so it needs build-machine tools: the
  # in-tree ones for a native build, external ones for a cross build (a
  # cross-built msiinfo is a host binary and cannot run here).
  msi_validate_args = []
  msi_validate_deps = [msitools_msi]

  if get_option('msi-msiinfo') != ''
    msi_validate_args += [find_program(get_option('msi-msiinfo')).full_path()]
  elif is_variable('msiinfo') and not meson.is_cross_build()
    msi_validate_args += [msiinfo]
    msi_validate_deps += msiinfo
  endif

  if msi_validate_args.length() == 0
    warning('no build-machine msiinfo, skipping the msitools MSI FeatureTree ' +
            'test; pass -Dmsi-msiinfo=/path/to/msiinfo to enable it')
  else
    msi_validate_args += [msitools_msi]

    if get_option('msi-msiextract') != ''
      msi_validate_args += [find_program(get_option('msi-msiextract')).full_path()]
    elif is_variable('msiextract') and not meson.is_cross_build()
      msi_validate_args += [msiextract]
      msi_validate_deps += msiextract
    endif

    if is_variable('msiextract')
      msi_validate_args += ['--require-msiextract']
    endif
    if is_variable('wixl')
      msi_validate_args += ['--require-wix']
    endif
    if host_machine.system() == 'windows'
      msi_validate_args += ['--windows-payload']
    endif

    test('msitools MSI FeatureTree',
      validate_msi,
      args: msi_validate_args,
      depends: msi_validate_deps,
      timeout: 120,
    )
  endif

  wine = find_program('wine', required: false)
  if host_machine.system() == 'windows' and wine.found()
    smoke_msi = find_program('smoke-msi.py')
    test('msitools MSI install smoke',
      smoke_msi,
      args: [msitools_msi],
      depends: [msitools_msi],
      timeout: 300,
    )
  endif
endif
