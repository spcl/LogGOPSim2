#ifndef __GEM5_MOD_H__
#define __GEM5_MOD_H__


#include <vector>

//#include "mem/controlled_simple_mem.hh"
#include "mem/cache/dummy_cache.hh"
#include "cpu/o3/deriv.hh"
#include "sim/cxx_manager.hh" 
#include "cpu/base.hh" 

#include "../../../simEvents.hpp"
#include "../../../sim.hpp"

#include "../../packet_net/NetMod.hpp"

#include "../../dma/dma.hpp"
#include "../p4smp_events.hpp"


#define HANDLER_EXECUTED 0
#define NO_HPU_AVAIL -1
#define NO_CONTEXT_AVAIL -2
#define HANDLER_SUSPENDED -3


class GEM5Mod: SimModule {

private:

    class myQueue: public EventQueue{
    public:
        myQueue(const char * str): EventQueue(str){ ; }
    };

private:

    std::vector<DerivO3CPU*> cpus;
    std::vector<DummyCache*> dummycaches; 

    CxxConfigManager * config_manager;

    uint32_t timemult;
    bool print;
    Simulator& sim;



    uint32_t hpus;
    uint32_t p;
    std::vector<std::priority_queue<hpu_t>> nexthpu;
    //std::vector<std::list<hpu_t>> busyhpu;

    std::vector<std::queue<MatchedHostDataPkt*>> waiting_queue;
private:

    bool simcall(MatchedHostDataPkt& event, uint32_t syscall_num, void *data, btime_t& time);

public:
    GEM5Mod(Simulator& sim, const std::string config_file, uint32_t ranks, uint32_t timemult, uint32_t o_dma,  uint32_t DMA_L, uint32_t o_hpu, uint8_t cas_failure_rate, bool print=false);


    int executeHandler(MatchedHostDataPkt& pkt, btime_t& time);

    void printStatus();

    size_t maxTime();

};




#endif /* __GEM5_MOD_H__ */
