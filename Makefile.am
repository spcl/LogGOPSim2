SUBDIRS = src/DrawViz src/p4schedgen src/schedgen src/simulator src/txt2bin

.PHONY = lgs drawviz schedgen_portals schedgen_mpi goal2bin

lgs:
	cd src/simulator && make

drawviz:
	cd src/DrawViz && make

schedgen_portals:
	cd src/p4schedgen && make

schedgen_mpi:
	cd src/schedgen && make

goal2bin:
	cd src/txt2bin && make
