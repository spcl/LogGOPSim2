bin_PROGRAMS = schedgen_mpi

schedgen_mpi_SOURCES = buffer_element.cpp process_trace.cpp schedgen.cpp cmdline.c
BUILT_SOURCES = cmdline.c

cmdline.c: schedgen.ggo
	gengetopt < $(srcdir)/schedgen.ggo

