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
    uint32_t size, src, tag, offset, proc, nic;
} ruqelem_t;




/*class MsgEvent : public LogGPBaseEvent {
public:
    btime_t starttime;

    MsgEvent(LogGPBaseEvent& g, btime_t time): LogGPBaseEvent(g, OP_MSG, time) {
        starttime = g.time;
    }

    MsgEvent(LogGPBaseEvent& g): MsgEvent(g, g.time) {}

    void invert(){
        uint32_t th = this->host;
        this->host = this->target;
        this->target = th;
    }
};*/


class NetMsgEvent : public simevent {
private:
    simevent * payload;

public:
    uint32_t source;
    uint32_t dest;
    uint32_t size;


    NetMsgEvent(simevent * g, uint32_t source, uint32_t dest, uint32_t size, btime_t time): payload(g), source(source), dest(dest), size(size){
        type = NET_MSG;
        this->time = time;
        g->keepalive = true;
    }


    NetMsgEvent(LogGPBaseEvent * g, uint32_t size,  btime_t time): NetMsgEvent(g, g->host, g->target, size, time){;}

    NetMsgEvent(LogGPBaseEvent * g, btime_t time): NetMsgEvent(g, g->host, g->target, g->size, time){;}


    simevent * getPayload(){ 
        payload->keepalive=false;
        return payload; 
    }
};

class AckEvent : public LogGPBaseEvent {
public:

    AckEvent(LogGPBaseEvent ev) : LogGPBaseEvent(ev){
        type = ACK_MSG;
        this->host = ev.target;
        this->target = ev.host;
    }

    AckEvent(ruqelem_t el, btime_t time) : LogGPBaseEvent(){
        type = ACK_MSG;
        this->host = el.src;
        this->offset = el.offset;
        this->time = time;
        this->proc = el.proc;
        this->nic = el.nic;
    }

};



//template<typename T>
class LogGOPmod: public SimModule {
protected:
    typedef std::list<ruqelem_t> ruq_t;

    std::vector<std::vector<btime_t>> nexto, nextgr, nextgs;
    std::vector<ruq_t> rq, uq; // receive queue, unexpected queue


    Simulator& sim;
    Parser& parser;


    TimelineVisualization * tlviz;
    Noise * osnoise;

    uint32_t o, O, g, G, L, S;
    bool print=true;
//    bool batchmode=false;
    uint32_t p;

    bool simplenet;

public:
    

    LogGOPmod(Simulator& sim, Parser& parser, bool simplenet=true): sim(sim), parser(parser), simplenet(simplenet) {
 

       // cmdline_parser(argc, argv, &args_info);


        p = parser.schedules.size();
        const int ncpus = parser.GetNumCPU();
        const int nnics = parser.GetNumNIC();

        o = sim.args_info.LogGOPS_o_arg;
        O = sim.args_info.LogGOPS_O_arg;
        g = sim.args_info.LogGOPS_g_arg;
        L = sim.args_info.LogGOPS_L_arg;
        G = sim.args_info.LogGOPS_G_arg;

        S = sim.args_info.LogGOPS_S_arg;

        print = sim.args_info.verbose_given;
     //   batchmode = sim.args_info.batchmode_given;

        //tlviz = new TimelineVisualization(&args_info, p);
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
    ~LogGOPmod();

/*
    ~LogGOPmod(){
        //delete tlviz;
        delete osnoise;
    }
*/    
    int send(goalevent& ev);
    int receive(goalevent& elem);
    int receive_ack(AckEvent& elem);
    int receive_msg(goalevent& elem);
    int network(NetMsgEvent& elem);
    int locop(goalevent& elem);
    int locgem5op(goalevent& elem);

    bool match(const LogGPBaseEvent &elem, ruq_t *q, ruqelem_t *retelem=NULL);


    virtual int registerHandlers(Simulator& sim){
        sim.addHandler(this, OP_SEND, LogGOPmod::dispatch);
        sim.addHandler(this, OP_MSG, LogGOPmod::dispatch);
        sim.addHandler(this, ACK_MSG, LogGOPmod::dispatch);
        if (simplenet) sim.addHandler(this, NET_MSG, LogGOPmod::dispatch);
        sim.addHandler(this, OP_RECV, LogGOPmod::dispatch);
        sim.addHandler(this, OP_LOCOP, LogGOPmod::dispatch);
        sim.addHandler(this, OP_LOCGEM5OP, LogGOPmod::dispatch);
        return 0;
    }

    virtual void printStatus();
    virtual size_t maxTime();


    static int dispatch(SimModule* mod, simevent* ev);

};

#endif /* __LOGGOP_HPP__ */
