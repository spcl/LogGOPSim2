CXX=g++
CXXFLAGS= -O3 -pedantic -Wno-deprecated -Wall -Wno-long-long -std=c++11
CCFLAGS= -O3 
LDFLAGS= 

txt2bin:
	re2c -o txt2bin.cpp txt2bin.re
	gengetopt --file-name=cmdline_txt2bin < txt2bin.ggo
	$(CXX) -g -O3 txt2bin.cpp cmdline_txt2bin.c -o txt2bin -std=c++11

clean:
	rm -f txt2bin.cpp txt2bin bin2txt bin2dot simtest
	rm -f cmdline_txt2bin.*
