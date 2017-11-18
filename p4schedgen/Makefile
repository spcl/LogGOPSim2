CXX=g++
CXXFLAGS= -O3 -g -pedantic -Wno-deprecated -Wall -Wno-long-long -std=c++11
CCFLAGS= -O3 -g 
LDFLAGS= -lcgraph -g

#AUTOGEN_SRC= cmdline.c cmdline.h
LOGGOPSIM_OBJECTS= p4schedgen.o 
ALL_OBJECTS= $(LOGGOPSIM_OBJECTS)
BINARY= p4schedgen

all: $(ALL_OBJECTS) 
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS) -o $(BINARY) $(LDFLAGS)


%.o: %.cpp $(AUTOGEN_SRC) *.hpp 
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c $(AUTOGEN_SRC) *.h
	$(CXX) $(CCFLAGS) -c $<

clean:
	rm -f $(AUTOGEN_SRC)
	rm -f $(ALL_OBJECTS)
	rm -f $(BINARY)
