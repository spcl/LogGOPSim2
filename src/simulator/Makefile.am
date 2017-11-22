AUTOMAKE_OPTIONS = subdir-objects

MODULES = modules/dma/dma.cpp \
          modules/loggop/loggop.cpp \
          modules/packet_net/NetMod.cpp \
          modules/portals4/p4.cpp \
          modules/portals4_smp/p4smp.cpp


bin_PROGRAMS = lgs
lgs_SOURCES = sim.cpp cmdline.c main.cpp ${MODULES}
lgs_CPPFLAGS = -std=c++11
lgs_LDADD = -lcgraph
BUILT_SOURCES = cmdline.c

cmdline.c: cmdline.ggo
	gengetout < $(srcdir)/cmdline.ggo

