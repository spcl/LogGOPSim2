from m5.params import *
from m5.proxy import *
from MemObject import MemObject

class DummyCache(MemObject):
    type = 'DummyCache'
    cxx_header = "mem/cache/dummy_cache.hh"

    cpu_port = SlavePort("CPU side port, receives requests")
    mem_side = MasterPort("Memory side port, sends requests")

