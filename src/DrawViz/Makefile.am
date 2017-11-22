bin_PROGRAMS = drawviz


drawviz_SOURCES = Drawviz.cpp TimelineDrawing.cpp cmdline.c
drawviz_CXXFLAGS = --no-inline -Wno-deprecated -Wall
drawviz_LDADD = -lps -lboost_regex
BUILT_SOURCES = cmdline.c

cmdline.c: drawviz.ggo
	gengetout < $(srcdir)/drawviz.ggo
