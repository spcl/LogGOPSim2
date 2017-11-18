
#ifndef __SPCLSIM_HPP__
#define __SPCLSIM_HPP__

#include <stdint.h>
#include <queue>
#include <unordered_map>
#include <assert.h>

#include "simEvents.hpp"

#include "cmdline.h"
#include "TimelineVisualization.hpp"


#define STATS

class Simulator;

class SimModule{
public:
    virtual int registerHandlers(Simulator& sim)=0;
    virtual void printStatus()=0;
    virtual size_t maxTime()=0;
    //virtual int dispatch(simevent* ev)=0;
    virtual ~SimModule() { ; }
};


/* this is a comparison functor that can be used to compare and sort
 * simevent objects by time */
class aqcompare_func {
  public:
  bool operator()(simevent* x, simevent* y) {
    if(x->time > y->time) return true;
    if(x->time == y->time && x->id > y->id) return true;
    return false;
  }
};

//NOTE: Bad that evqueue_t contains pointers but needed if we want to allow different event typess :(
typedef std::priority_queue<simevent*, std::vector<simevent*>, aqcompare_func> evqueue_t;
typedef int ekey_t;
typedef int (*efun_t) (SimModule*, simevent*);
typedef std::unordered_map<ekey_t, std::pair<SimModule*, efun_t>> dispatch_map_t;
//typedef std::unordered_map<ekey_t, SimModule*> dispatch_map_t;




class Simulator : ISimulator{

private:
    evqueue_t aq;
    dispatch_map_t dmap;

    std::vector<SimModule*> mods;
#ifdef STATS
    uint64_t reinserted=0;
#endif

public:

    TimelineVisualization * tlviz;
    gengetopt_args_info args_info;
    uint32_t ranks;

    void addHandler(SimModule* mod, ekey_t key, efun_t fun);
    virtual void addEvent(simevent* elem){
        //printf("adding event: type; %i; id: %u; ptr: %p\n", elem->type, elem->id, elem);
        this->aq.push(elem);   
    }

    virtual void reinsertEvent(simevent * elem){
        //printf("reinserting event: type: %i; id: %u; ptr: %p\n", elem->type, elem->id, elem);

        elem->keepalive = true;
#ifdef STATS
        reinserted++;
#endif
        this->aq.push(elem);
    }

    inline int simulate(){
        simevent* elem = aq.top();
        //printf("[SIM] ev ptr: %p\n", elem);
        //printf("[SIM] simulating event: %i with id: %u (%p); time: %lu aq size: %i\n", elem->type, elem->id, elem, elem->time, aq.size());
        assert(elem!=NULL);

        aq.pop();    

        if (dmap.find(elem->type)==dmap.end()) {
            printf("Error: no handler for %i %lu\n", elem->type, elem->id);
            return -1;
        }

        (*dmap[elem->type].second)(dmap[elem->type].first, elem);
        //dmap[elem->type]->dispatch(elem);
 
        if (!elem->keepalive) delete elem;   
    
        return 0;
    }

    int simulate(IParser& parser);

    void addModule(SimModule* mod);

    void printStatus();

    void maxTime();

    Simulator(int argc, char * argv[], uint32_t ranks): ranks(ranks){
    
        cmdline_parser(argc, argv, &args_info);
        tlviz = new TimelineVisualization(args_info.vizfile_arg, args_info.vizfile_given, ranks);
    }

    ~Simulator(){
        delete tlviz;
    }

};


#endif /* __SPCLSIM_HPP__ */

