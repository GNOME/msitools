NULL =

wxidir = $(datadir)/wixl-$(VERSION)/include

EXTRA_DIST = wxi-validate.pl

dist_wxi_DATA =					\
	wixl/SDL.wxi				\
	wixl/OpenEXR.wxi			\
	wixl/adwaita-icon-theme.wxi		\
	wixl/atk.wxi				\
	wixl/bzip2.wxi				\
	wixl/cairo.wxi				\
	wixl/celt051.wxi			\
	wixl/curl.wxi				\
	wixl/dlfcn.wxi				\
	wixl/expat.wxi				\
	wixl/fontconfig.wxi			\
	wixl/freetype.wxi			\
	wixl/gcc.wxi				\
	wixl/gcc-c++.wxi			\
	wixl/gdk-pixbuf.wxi			\
	wixl/gettext.wxi			\
	wixl/glib2.wxi				\
	wixl/glib-networking.wxi		\
	wixl/gmp.wxi				\
	wixl/gnutls.wxi				\
	wixl/gsm.wxi				\
	wixl/gstreamer-plugins-bad-free.wxi	\
	wixl/gstreamer-plugins-base.wxi		\
	wixl/gstreamer-plugins-good.wxi		\
	wixl/gstreamer.wxi			\
	wixl/gstreamer1-plugins-bad-free.wxi	\
	wixl/gstreamer1-plugins-base.wxi	\
	wixl/gstreamer1-plugins-good.wxi	\
	wixl/gstreamer1.wxi			\
	wixl/gtk-vnc2.wxi			\
	wixl/gtk2.wxi				\
	wixl/gtk3.wxi				\
	wixl/gvnc.wxi				\
	wixl/harfbuzz.wxi			\
	wixl/hicolor-icon-theme.wxi		\
	wixl/icu.wxi				\
	wixl/ilmbase.wxi			\
	wixl/jasper.wxi				\
	wixl/json-glib.wxi			\
	wixl/libcacard.wxi			\
	wixl/libepoxy.wxi			\
	wixl/libffi.wxi				\
	wixl/libgcrypt.wxi			\
	wixl/libgpg-error.wxi			\
	wixl/libgovirt.wxi			\
	wixl/libidn2.wxi			\
	wixl/libjpeg-turbo.wxi			\
	wixl/libogg.wxi				\
	wixl/libpng.wxi				\
	wixl/libtheora.wxi			\
	wixl/libsoup.wxi			\
	wixl/libssh2.wxi			\
	wixl/libtasn1.wxi			\
	wixl/libtiff.wxi			\
	wixl/libusbx.wxi			\
	wixl/libvirt.wxi			\
	wixl/libvirt-glib.wxi			\
	wixl/libvorbis.wxi			\
	wixl/libxml2.wxi			\
	wixl/nettle.wxi				\
	wixl/nspr.wxi				\
	wixl/nss.wxi				\
	wixl/openssl.wxi			\
	wixl/opus.wxi				\
	wixl/orc.wxi				\
	wixl/p11-kit.wxi			\
	wixl/pango.wxi				\
	wixl/pcre.wxi				\
	wixl/pixman.wxi				\
	wixl/portablexdr.wxi			\
	wixl/pthreads.wxi			\
	wixl/readline.wxi			\
	wixl/rest.wxi				\
	wixl/speex.wxi				\
	wixl/spice-glib.wxi			\
	wixl/spice-gtk3.wxi			\
	wixl/sqlite.wxi				\
	wixl/termcap.wxi			\
	wixl/usbredir.wxi			\
	wixl/wavpack.wxi			\
	wixl/win-iconv.wxi			\
	wixl/zlib.wxi				\
	wixl/winpthreads.wxi			\
	$(NULL)

check-wxi-%:
	$(AM_V_GEN)rpm -q mingw32-$* mingw64-$* 1>/dev/null 2>&1 ; \
	if test $$? = 0 ; \
	then \
		$(PERL) $(srcdir)/wxi-validate.pl wixl/$*.wxi ; \
	else \
		echo "Skipping wixl/$*.wxi as mingw32-$* and/or mingw64-$* are not installed" ; \
	fi

check-wxi: $(dist_wxi_DATA:wixl/%.wxi=check-wxi-%)

-include $(top_srcdir)/git.mk
