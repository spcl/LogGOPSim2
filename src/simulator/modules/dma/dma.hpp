#ifndef __DMAMOD_HPP__
#define __DMAMOD_HPP__

#include <list>

#include "../../sim.hpp"
#include "../../simEvents.hpp"
#include "../../cmdline.h"

class DMAEvent: public simevent{
public:
    uint32_t host;
    simevent * originatingEvent;
    uint32_t size;
    bool isBlocking; //used by NB operations and wait

    uint64_t targetid; //used only by the wait, otw equal to id

    DMAEvent(simevent * pkt, uint32_t size, uint32_t host, bool isBlocking, btime_t time): simevent(){
        this->originatingEvent = pkt;
        this->size = size;
        this->time = time;
        this->isBlocking = isBlocking;
        this->host = host;
        this->targetid = id;

        originatingEvent->keepalive = true;
    }

    simevent * extractOriginatingEvent(){
        originatingEvent->time = time;
        originatingEvent->keepalive = false;
        return originatingEvent;
    }

    bool operator==(const DMAEvent& ev) const{
        return targetid==ev.targetid;
    }

};

class DMAReadEvent: public DMAEvent{
public:
    DMAReadEvent(simevent * pkt, uint32_t size, uint32_t host, bool isBlocking, btime_t time):
        DMAEvent(pkt, size, host, isBlocking, time){
        type = DMA_READ;
    }
};

class DMAWriteEvent: public DMAEvent{
public:
    DMAWriteEvent(simevent * pkt, uint32_t size, uint32_t host, bool isBlocking, btime_t time):
        DMAEvent(pkt, size, host, isBlocking, time){
        type = DMA_WRITE;
    }
};

class DMAWaitEvent: public DMAEvent{
public:
    DMAWaitEvent(simevent * pkt, uint32_t host, uint64_t reqid, btime_t time):
        DMAEvent(pkt, 0, host, true, time){
        targetid = reqid;
        type = DMA_WAIT;
    }
};


class DMAmod: public SimModule {
protected:

    std::vector<std::list<DMAEvent>>    uq;
    std::vector<std::list<DMAEvent>> wq; // unexpected and waiting queue

    Simulator& sim;
    
    bool contention;
    uint32_t g, G, L;
    uint32_t p;
    bool print;
    std::vector<btime_t>  nextgr, nextgs; // TODO discuss oreceive and osend

public:
    

    DMAmod(Simulator& sim, int hosts): sim(sim) {
 
        p = hosts;
        
        g = sim.args_info.DMA_g_arg; //3;
        L = sim.args_info.DMA_L_arg; //4;
        G = sim.args_info.DMA_G_arg; //5;
        contention = sim.args_info.DMA_contention_arg;
        print = sim.args_info.verbose_given;


        nextgr.resize(p);
        nextgs.resize(p);
        uq.resize(p);
        wq.resize(p);
    
        printf("DMAmod: hosts %i; L=%i; g=%i; G=%i; \n", p, L, g, G);
    } 

   
    int handleDMARequest(DMAEvent& elem);
    int wait(DMAEvent& elem);


    virtual int registerHandlers(Simulator& sim){
        sim.addHandler(this, DMA_WRITE, DMAmod::dispatch);
        sim.addHandler(this, DMA_READ, DMAmod::dispatch);
        sim.addHandler(this, DMA_WAIT, DMAmod::dispatch);        
        return 0;
    }

    virtual void printStatus();
    virtual size_t maxTime();

    static int dispatch(SimModule* mod, simevent* ev);

};

#endif /* __DMAMOD_HPP__ */
