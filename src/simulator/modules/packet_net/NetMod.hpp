#ifndef __NETMOD_HPP__
#define __NETMOD_HPP__

/* TODOs:
    - ptime is now multiplied by msg.size. Check if this is consistent. 

*/

#include <math.h>
#include <queue>
#include "Network.hpp"

#include "../../sim.hpp"
#include "../../simEvents.hpp"
#include "../../cmdline.h"

#include "../loggop/loggop.hpp"

//#include "../portals4/p4.hpp"
//#include "../portals4_smp/p4smp.hpp"

class HeadPkt: public NetMsgEvent {
public:
    static uint64_t idgen;

    uint64_t id;
    uint32_t toreceive;

    HeadPkt(NetMsgEvent ev, uint32_t totpackets): NetMsgEvent(ev), toreceive(totpackets){
        id = ++idgen;
    }

    uint64_t getID(){ return id; }
};

class DataPkt: public simEvent{
public:
    HeadPkt * header=NULL;
    uint32_t currenthop;
    uint32_t lasthop;
    uint32_t lastport;
    uint32_t destid;
    uint32_t size;
    uint32_t starttime;
    bool buffered=false;
    bool is_credit=false;    
    
    uint16_t id; // should be only for debug
    uint32_t pktoffset;

    bool ishead=false;
    bool istail=false;

    DataPkt(uint32_t currenthop, uint32_t destid): currenthop(currenthop), destid(destid){
        type=NET_PKT;
        header=NULL;
        size=0;
    }

    DataPkt(HeadPkt * header, uint32_t currenthop, uint32_t size):  header(header), currenthop(currenthop), size(size) {
        type=NET_PKT;
    }

    DataPkt(DataPkt& pkt, uint32_t nexthop): DataPkt(pkt){
        type=NET_PKT;
        currenthop=nexthop;
    }

    bool isHeader(){ return ishead; }
    bool isPayload(){ return true; }
    bool isTail(){ return istail;}
};



class HostDataPkt: public DataPkt{

public:
    HostDataPkt(DataPkt& pkt): DataPkt(pkt){
        type = HOST_DATA_PKT;
    }

    HostDataPkt(HostDataPkt& pkt, int _type): HostDataPkt(pkt){
        type = _type;
    }

};


class NetMod: simModule {
private:
    Simulator& sim;

    TopoGraph * topology;
    uint32_t pktsize;
    uint32_t maxcredits;
  
    uint32_t ptime;
    uint32_t G;
    uint32_t latency;
#ifdef STATS
    uint64_t totpkt=0;
    uint64_t nocredits=0;
    uint64_t creditpkts_payload=0;
#endif

    bool print=false;
    std::vector<btime_t> nexto;
    std::vector<uint32_t> buffsize;

    std::vector<std::vector<uint32_t>> credits;
    std::vector<std::vector<std::queue<DataPkt*>>> buffer;

    bool deliver_single_packets;


public:
    NetMod(Simulator& sim, bool deliver_single_packets=false): 
        sim(sim), deliver_single_packets(deliver_single_packets) {

        ptime = sim.args_info.network_ptime_arg;
        latency = sim.args_info.network_latency_arg;

        maxcredits = sim.args_info.network_maxcredits_arg;
        pktsize = sim.args_info.network_pktsize_arg;
        char * filename = sim.args_info.network_file_arg;
        char * mapping_file = sim.args_info.network_mapping_given ? 
                                sim.args_info.network_mapping_arg : NULL;

        G = sim.args_info.LogGOPS_G_arg;
        print = sim.args_info.verbose_given;

        topology = new TopoGraph(filename, mapping_file, print);

        int p = topology->get_size();
    
        printf("NetMod: nodes: %i; ptime: %u; latency: %u; max_credits: %u;  \
                pktsize: %u; deliver_single_packets: %i\n", p, ptime, latency, \
                maxcredits, pktsize, deliver_single_packets);
        
        nexto.resize(p);
        buffer.resize(p);     
        credits.resize(p);

        for (int i=0; i<p; i++){
            uint32_t ports = topology->get_outports(i);
            credits[i].resize(ports, maxcredits);
            buffer[i].resize(ports);
        }

    }

    ~NetMod(){
        delete topology;
    }



    //Switch attached to source host
    int ingress(NetMsgEvent& msg){
        /* just queue datapkts at the first switch */
    
        if (print) printf("[NET] ingress packet: time: %lu\n", msg.time);
        //get the ingress switch
        uint32_t srcid = topology->get_id(msg.source);
        uint32_t destid = topology->get_id(msg.dest);
        //uint32_t sw = topology->next_hop(srcid, destid);

        HeadPkt * head = new HeadPkt(msg, ceil(msg.size*1.f / pktsize));
        //head->destid=destid;

        //create data packets
        btime_t time = msg.time;
        uint32_t tosend = msg.size;
        uint16_t count=0;
        DataPkt * pkt=NULL;
        while (tosend>0){
            pkt = new DataPkt(head, srcid, std::min(tosend, pktsize));
            pkt->starttime= time; //for visualisation
            time += (pktsize-1)*G; //ptime;

            pkt->destid=destid;
            pkt->lasthop=srcid;
            pkt->time = time;
            pkt->ishead = count==0;
            pkt->pktoffset=pktsize*count;
            pkt->id = count++;
            //time += ptime;
            //time += G*pktsize;;
            sim.addEvent(pkt);
            //if (print) printf("\tnew data pkt: srcid: %i size: %i; \
                                 time: %lu\n", srcid, pkt->size, pkt->time);
            tosend -= pkt->size;
#ifdef STATS
            totpkt++;
#endif
        }
        if (pkt!=NULL) { pkt->istail=true; }

        return 0;
    }

    void * release_credit(DataPkt& msg, btime_t t){
        assert(msg.lastport==topology->next_port(msg.lasthop, msg.destid));
        DataPkt * creditpkt = new DataPkt(msg.lasthop, msg.destid);
        creditpkt->is_credit=true;
        creditpkt->time=t; 
        sim.addEvent(creditpkt);

        if (print){
            printf("\tsending credit back to: %s; port: %i;\n", \
                    topology->get_nodename(msg.lasthop).c_str(), msg.lastport);
        }

        return NULL;
    }


    int _forward(DataPkt& msg){
        //receive packet (if there is time)
        uint32_t sw = msg.currenthop;
        uint32_t sw_port = topology->next_port(sw, msg.destid);
        bool endswitch = sw==msg.destid;

        if (print) {
            printf("[NET %s] forwarding packet: %i->%i(%i); msg.time: %lu; \
                    nexto[%i]: %lu\n", topology->get_nodename(sw).c_str(), \
                    msg.header->source, msg.destid, msg.id, msg.time, sw, \
                    nexto[sw]);
        }

        /* check if there is time */
        if (nexto[sw] > msg.time){ /* no time */ 
            if (print) printf("\tno time; nexto[%i]: %lu; msgtime: %lu\n", sw, \
                               nexto[sw], msg.time);

            msg.time = nexto[sw];
            msg.buffered = true;
            sim.reinsertEvent(&msg);
            return 0;
        }
    
        if (msg.is_credit){
            credits[sw][topology->next_port(sw, msg.destid)]++;

            assert(credits[sw][topology->next_port(sw, msg.destid)]<=maxcredits);

            if (!buffer[sw][topology->next_port(sw, msg.destid)].empty()){
#ifdef STATS
                creditpkts_payload++;
#endif 
                //printf("-- forwarding queued pkt\n");
                DataPkt * toforward = buffer[sw][topology->next_port(sw, msg.destid)].front();
                buffer[sw][topology->next_port(sw, msg.destid)].pop();
                toforward->time = msg.time;
                _forward(*toforward);
            }
            return 0;
        }

        if (!endswitch && credits[sw][topology->next_port(sw, msg.destid)]==0){
            if (print) {
                printf("\tno credits: switch: %i; port: %i; msg.time: %lu; \
                        new time: %lu\n", sw, \
                        topology->next_port(sw, msg.destid), msg.time, \
                        std::max(msg.time, nexto[topology->next_hop(sw, msg.destid)]));
            }

            //there is now guarantee that there will be a credit at this time
            msg.time = std::max(msg.time, nexto[topology->next_hop(sw, msg.destid)]);
            //TODO: add processing time and credit latency (if any)
            msg.buffered = true;
            //sim.addEvent(new DataPkt(msg));
        
            buffer[sw][topology->next_port(sw, msg.destid)].push(&msg);
            msg.keepalive=true;

#ifdef STATS
            nocredits++;
#endif

            return 0;
        }


        //we are handling the forwarding now
        nexto[sw] = msg.time + ptime*msg.size;

        /* msg.lasthop==msg.currenthop only if this is the source host 
           (hence the message is already buffered and we don't have to increase the credits) */
        if (msg.lasthop!=msg.currenthop) {


            if (!(endswitch && deliver_single_packets)){
                release_credit(msg, msg.time);  
            }/* else the credit has to be released after another module signals it. 
                (e.g., sPIN NIC). */    

        }else if(msg.isTail()){
            /* here we are the source and we are able to forward the last packet. 
               Notify the host.*/
            sim.signal(SIG_SEND_COMPLETE, msg.header->getPayload());
        }

        //forward to next hop or check&deliver
        if (endswitch){
            if (!deliver_single_packets){
                msg.header->toreceive--;
                if (msg.header->toreceive==0) {
                    simEvent * payload = msg.header->getPayload();
                    /* this is already the host, the nic processing cost will 
                    be accounted by the handler of the payload (e.g., loggp or p4) */
                    payload->time = msg.header->time; 
                    sim.addEvent(payload);
                }
            }else{
                sim.addEvent(new HostDataPkt(msg));
            }
        }else{ /* just forward */
            uint32_t nexthop = topology->next_hop(sw, msg.destid);
            msg.lasthop = msg.currenthop;
            msg.time += ptime*msg.size + latency;
            msg.lastport = topology->next_port(msg.currenthop, msg.destid);
            msg.currenthop = nexthop;
            credits[msg.lasthop][msg.lastport]--;

            if (print) {
                printf("\tforward to: %s; port: %i; remaining credits: %i\n", \
                        topology->get_nodename(nexthop).c_str(), msg.lastport, \
                        credits[msg.lasthop][msg.lastport]);
            }
            sim.reinsertEvent(&msg);
        }
        return 0;
    }

    virtual int registerHandlers(Simulator& sim){
        sim.addEventHandler(this, NET_MSG, NetMod::dispatch);
        sim.addEventHandler(this, NET_PKT, NetMod::dispatch);

        sim.addSignalHandler(this, SIG_PKT_RECEIVED, NetMod::dispatch_signal);
        return 0;
    }


    virtual void printStatus(){
#ifdef STATS
        printf("NetMod: Total packets: %lu; \
                nocredits: %lu; \
                credit packets with payload: %lu\n", \
                totpkt, nocredits, creditpkts_payload);
#endif
    }
    virtual size_t maxTime(){ return 0;}

    static int dispatch(simModule* mod, simEvent* ev);
    static void * dispatch_signal(simModule * mod, sim_signal_t signal, void * arg);

};


#endif /* __NETMOD_HPP__ */ 
