NULL =

ENUMS_FILES = libmsi-types.h

ENUMS =						\
	../libmsi/libmsi-enums.c		\
	libmsi-enums.h				\
	$(NULL)

$(ENUMS): %: %.etemplate $(ENUMS_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $^ > $@

BUILT_SOURCES = $(ENUMS)

EXTRA_DIST = libmsi-enums.h.etemplate


