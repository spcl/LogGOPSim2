bin_PROGRAMS = schedgen

schedgen_SOURCES = buffer_element.cpp process_trace.cpp schedgen.cpp cmdline.c
BUILT_SOURCES = cmdline.c

cmdline.c: schedgen.ggo
	gengetout < $(srcdir)/schedgen.ggo

