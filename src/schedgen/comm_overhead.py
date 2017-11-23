#!/usr/bin/python

import sys


if(len(sys.argv) < 2):
  print "usage: " + sys.argv[0] + " <trace-file>"
  sys.exit()

start = 0
end = 0
comm = float(0)

f = open(sys.argv[1], "r")
line = "1"
while line:
  line = f.readline()
  s = line.split(":")
  if(len(s) > 1 and s[0][0] != "#"):
    if(s[0] == "MPI_Init"): 
      start = s[len(s)-1]
      print "start: ", start
      continue
    if(s[0] == "MPI_Finalize"): 
      end = s[1]
      print "end: ", end
      continue
    comm = comm - float(s[1]) + float(s[len(s)-1])

print "total= ", float(end)-float(start)
print "comm= ", comm
print "share= ", (float(end)-float(start))/comm

f.close()

