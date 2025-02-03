wxi_data = files(
  'wixl/adwaita-icon-theme.wxi',
  'wixl/atk.wxi',
  'wixl/brotli.wxi',
  'wixl/bzip2.wxi',
  'wixl/cairo.wxi',
  'wixl/curl.wxi',
  'wixl/dbus.wxi',
  'wixl/dlfcn.wxi',
  'wixl/expat.wxi',
  'wixl/fontconfig.wxi',
  'wixl/freetype.wxi',
  'wixl/fribidi.wxi',
  'wixl/gcc-c++.wxi',
  'wixl/gcc.wxi',
  'wixl/gdk-pixbuf.wxi',
  'wixl/gettext.wxi',
  'wixl/glib-networking.wxi',
  'wixl/glib2.wxi',
  'wixl/gmp.wxi',
  'wixl/gnutls.wxi',
  'wixl/gsm.wxi',
  'wixl/gstreamer1-plugins-bad-free.wxi',
  'wixl/gstreamer1-plugins-base.wxi',
  'wixl/gstreamer1-plugins-good.wxi',
  'wixl/gstreamer1.wxi',
  'wixl/gtk-vnc2.wxi',
  'wixl/gtk2.wxi',
  'wixl/gtk3.wxi',
  'wixl/gvnc.wxi',
  'wixl/harfbuzz.wxi',
  'wixl/hicolor-icon-theme.wxi',
  'wixl/icu.wxi',
  'wixl/jasper.wxi',
  'wixl/json-glib.wxi',
  'wixl/lcms2.wxi',
  'wixl/libcacard.wxi',
  'wixl/libcroco.wxi',
  'wixl/libdeflate.wxi',
  'wixl/libepoxy.wxi',
  'wixl/libffi.wxi',
  'wixl/libgcrypt.wxi',
  'wixl/libgovirt.wxi',
  'wixl/libgpg-error.wxi',
  'wixl/libidn2.wxi',
  'wixl/libjpeg-turbo.wxi',
  'wixl/libogg.wxi',
  'wixl/libpng.wxi',
  'wixl/libpsl.wxi',
  'wixl/librsvg2.wxi',
  'wixl/libsoup.wxi',
  'wixl/libssh2.wxi',
  'wixl/libtasn1.wxi',
  'wixl/libtheora.wxi',
  'wixl/libtiff.wxi',
  'wixl/libunistring.wxi',
  'wixl/libusb1.wxi',
  'wixl/libvirt-glib.wxi',
  'wixl/libvirt.wxi',
  'wixl/libvorbis.wxi',
  'wixl/libwebp.wxi',
  'wixl/libxml2.wxi',
  'wixl/nettle.wxi',
  'wixl/openal-soft.wxi',
  'wixl/openexr.wxi',
  'wixl/openjpeg.wxi',
  'wixl/openssl.wxi',
  'wixl/opus.wxi',
  'wixl/orc.wxi',
  'wixl/pango.wxi',
  'wixl/pcre.wxi',
  'wixl/pcre2.wxi',
  'wixl/pixman.wxi',
  'wixl/portablexdr.wxi',
  'wixl/readline.wxi',
  'wixl/rest.wxi',
  'wixl/speex.wxi',
  'wixl/spice-glib.wxi',
  'wixl/spice-gtk3.wxi',
  'wixl/sqlite.wxi',
  'wixl/termcap.wxi',
  'wixl/usbredir.wxi',
  'wixl/wavpack.wxi',
  'wixl/win-iconv.wxi',
  'wixl/winpthreads.wxi',
  'wixl/zlib.wxi',
)

install_data(wxi_data,
  install_dir: get_option('datadir') / 'wixl-' + meson.project_version() / 'include'
)

install_subdir('ext/',
  install_dir: get_option('datadir') / 'wixl-' + meson.project_version()
)

if get_option('validate-wxi')
  wxi_validate = find_program('wxi-validate.pl')
  foreach file: wxi_data
    filename = ('@0@'.format(file.full_path())).split('/')[-1].split('.wxi')[0]
    testname = 'wxi-@0@'.format(filename)

    r = run_command('rpm', '-q', 'mingw32-' + filename, 'mingw64-' + filename, check: false)
    if r.returncode() == 0
      test(testname,
        wxi_validate,
        args: file,
      )
    else
      warning('Skip validating ' + filename + '.wxi')
    endif
  endforeach
endif
