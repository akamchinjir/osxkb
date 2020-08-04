bin_PROGRAMS = osxkb

osxkb_SOURCES = bundle.c		\
				data.c			\
				error.c			\
				keyboard.c		\
				keymap.c		\
				main.c			\
				out.c			\
				prefixmap.c		\
				util.c

noinst_HEADERS = 	bundle.h		\
					common.h		\
					error.h			\
					keyboard.h		\
					keymap.h		\
					out.h			\
					prefixmap.h		\
					util.h

AM_CPPFLAGS = -I. -I.. ${gio_CFLAGS}
AM_CFLAGS = ${WARN_CFLAGS}

osxkb_LDADD = ${gio_LIBS}

BUILT_SOURCES = data.h
CLEANFILES = data.h
EXTRA_DIST = data.h.in

p = ${top_builddir}/data
ascii_data = $p/ascii.in
other_data = $p/ansi.dvorak $p/ansi.qwerty $p/osxopt
all_data = ${ascii_data} ${other_data}

mkfilestr = ${top_builddir}/tools/mkfilestr
mkascii = ${top_builddir}/tools/mkascii

data.h : ${all_data} data.h.in
	cp data.h.in data.h
	${mkascii} ${ascii_data} >> data.h
	for file in ${other_data}; do \
		basename=$${file#$p/} ; \
		${mkfilestr} $${basename/./_} $${file} >> data.h; \
	done
	echo "#endif" >> data.h
if WINDOWS
	sed "s/\r//" data.h > data.h.tmp
	mv data.h.tmp data.h
endif