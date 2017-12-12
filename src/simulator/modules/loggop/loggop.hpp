#ifndef __LOGGOP_HPP__
#define __LOGGOP_HPP__

#include <list>

#include "../../sim.hpp"
#include "../../simEvents.hpp"
#include "../../Parser.hpp"
#include "../../Noise.hpp"

typedef struct {
    // TODO: src and tag can go in case of hashmap matching
    btime_t starttime; // only for visualization
    uint32_t size, src, tag, offset;
    simEvent * origin_elem;
} ruqelem_t;




class NetMsgEvent : public simEvent {
private:
    simEvent * payload;

public:
    uint32_t source;
    uint32_t dest;
    uint32_t size;

    NetMsgEvent(simEvent * g, uint32_t source, uint32_t dest, uint32_t size, btime_t time): payload(g), source(source), dest(dest), size(size){
        type = NET_MSG;
        this->time = time;
        g->keepalive = true;
    }

    NetMsgEvent(LogGPBaseEvent * g, uint32_t size, btime_t time): NetMsgEvent(g, g->host, g->target, size, time){;}

    NetMsgEvent(LogGPBaseEvent * g, btime_t time): NetMsgEvent(g, g->host, g->target, g->size, time){;}

    simEvent * getPayload(bool extract=true){ 
        payload->keepalive = !extract;
        return payload; 
    }
};



//template<typename T>
class LogGOPmod: public simModule {
protected:
    typedef std::list<ruqelem_t> ruq_t;

    std::vector<std::vector<btime_t>> nexto, nextgr, nextgs;
    std::vector<ruq_t> rq, uq; // receive queue, unexpected queue


    Simulator& sim;
    Parser& parser;

    Noise * osnoise;

    uint32_t o, O, g, G, L, S;
    bool print=true;
    bool batchmode=false;
    uint32_t p;

    bool simplenet;


public:
    

    LogGOPmod(Simulator& sim, Parser& parser, bool simplenet=true): sim(sim), parser(parser), simplenet(simplenet) {
 
      
        p = parser.schedules.size();
        const int ncpus = parser.GetNumCPU();
        const int nnics = parser.GetNumNIC();

        o=sim.args_info.LogGOPS_o_arg;
        O=sim.args_info.LogGOPS_O_arg;
        g=sim.args_info.LogGOPS_g_arg;
        L=sim.args_info.LogGOPS_L_arg;
        G=sim.args_info.LogGOPS_G_arg;

        S=sim.args_info.LogGOPS_S_arg;

        print=sim.args_info.verbose_given;
        batchmode = sim.args_info.batchmode_given;

        osnoise = new Noise(&(sim.args_info), p);

        nexto.resize(p);
        nextgr.resize(p);
        nextgs.resize(p);
        rq.resize(p);
        uq.resize(p);
    
        for(uint32_t i=0; i<p; ++i) {
            nexto[i].resize(ncpus);
            std::fill(nexto[i].begin(), nexto[i].end(), 0);

            nextgr[i].resize(nnics);
            std::fill(nextgr[i].begin(), nextgr[i].end(), 0);

            nextgs[i].resize(nnics);
            std::fill(nextgs[i].begin(), nextgs[i].end(), 0);
        }
    
        printf("LogGOPMod: hosts %i (%i CPUs, %i NICs); L=%i, o=%i g=%i, G=%i, O=%i, P=%i, S=%u\n", p, ncpus, nnics, L, o, g, G, O, p, S);

    } 

    ~LogGOPmod(){

        if (print){
            for (uint32_t i=0; i<p; i++){
                if (!rq[i].empty()) printf("Host %u: RQ **NOT** EMPTY! (size: %lu)\n", i, rq[i].size());
                if (!uq[i].empty()) printf("Host %u: UQ **NOT** EMPTY! (size: %lu)\n", i, rq[i].size());

            }
        }

        delete osnoise;
    }
    
    int send(goalevent& ev);
    int receive(goalevent& elem);
    int receive_msg(goalevent& elem);
    int network(NetMsgEvent& elem);
    int locop(goalevent& elem);


    void * complete_send(goalevent& elem);

    bool match(const LogGPBaseEvent &elem, ruq_t *q, ruqelem_t *retelem=NULL, bool erase=true);


    virtual int registerHandlers(Simulator& sim){
        sim.addEventHandler(this, OP_SEND, LogGOPmod::dispatch);
        sim.addEventHandler(this, OP_MSG, LogGOPmod::dispatch);
        sim.addEventHandler(this, OP_MSG_RTR, LogGOPmod::dispatch);
        sim.addEventHandler(this, OP_MSG_RTS, LogGOPmod::dispatch);
        if (simplenet) sim.addEventHandler(this, NET_MSG, LogGOPmod::dispatch);
        sim.addEventHandler(this, OP_RECV, LogGOPmod::dispatch);
        sim.addEventHandler(this, OP_LOCOP, LogGOPmod::dispatch);

        sim.addSignalHandler(this, SIG_SEND_COMPLETE, LogGOPmod::dispatch_signal);
 
        return 0;
    }



    static int dispatch(simModule* mod, simEvent* ev);
    static void * dispatch_signal(simModule* mod, sim_signal_t signal, void * arg);


    virtual void printStatus();
    virtual size_t maxTime();


};

#endif /* __LOGGOP_HPP__ */

