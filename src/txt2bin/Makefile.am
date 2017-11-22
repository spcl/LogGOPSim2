bin_PROGRAMS = txt2bin

txt2bin_SOURCES = txt2bin.cpp txt2bin_cmdline.c
BUILT_SOURCES = txt2bin.cpp txt2bin_cmdline.c

txt2bin.cpp: txt2bin.re
	re2c $(srcdir)/txt2bin.re > txt2bin.cpp

txt2bin_cmdline.c: txt2bin_cmdline.ggo
	gengetopt --file-name=txt2bin_cmdline < $(srcdir)/txt2bin_cmdline.ggo
