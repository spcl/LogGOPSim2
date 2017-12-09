
#include "loggop.hpp"


#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include "../../config.h"

bool LogGOPmod::match(const LogGPBaseEvent &elem, ruq_t *q, ruqelem_t *retelem) {

    //std::cout << "UQ size " << q->size() << "\n";

    if(print) printf("++ [%i] searching matching queue for src %i tag %i\n", elem.host, elem.target, elem.tag);
    for(ruq_t::iterator iter=q->begin(); iter!=q->end(); ++iter) {
        if(elem.target == ANY_SOURCE || iter->src == ANY_SOURCE || iter->src == elem.target) {
            if(elem.tag == ANY_TAG || iter->tag == ANY_TAG || iter->tag == elem.tag) {
                if(retelem) *retelem=*iter;
                q->erase(iter);
                return true;
            }
        }
    }
    return false;
}

int LogGOPmod::network(NetMsgEvent& elem){
    if(print) printf("[NETWORK] network msg from %i to %i, t: %lu\n", elem.source, elem.dest, elem.time);
    
    simEvent * msg = elem.getPayload();
    msg->time = elem.time+L;
    //msg.invert();
    sim.addEvent(msg);   
    return 0;
}

int LogGOPmod::dispatch(simModule* mod, simEvent* _elem){

    LogGOPmod* lmod = (LogGOPmod*) mod;
    switch (_elem->type){
        case OP_SEND: return lmod->send(*((goalevent *) _elem));
        case OP_RECV: return lmod->receive(*((goalevent *) _elem));
        case ACK_MSG: return lmod->receive_ack(*((AckEvent *) _elem));
        case NET_MSG: return lmod->network(*((NetMsgEvent *) _elem));
        case OP_MSG: return lmod->receive_msg(*((goalevent *) _elem));
        case OP_LOCOP: return lmod->locop(*((goalevent *) _elem));
        case OP_LOCGEM5OP: return lmod->locgem5op(*((goalevent *) _elem));

    }
    
    return -1;
}

static btime_t simulateGEM5(uint32_t binid, uint32_t length)
{

    btime_t execution_time = 100;
#ifdef HASCALCGEM5
    int pid, status;
    if(pid = fork())
    {
        waitpid(pid, &status,0);

    }
    else
    {


        char buffer [50];
        
        sprintf (buffer, "--options=%u %u", binid,length);
        //if(print)        printf ("buffer: [%s]\n",buffer);
    
        const char executable[] = GEM5DIR "/build/ARM/gem5.opt";

        char *parmList[] = { GEM5DIR "/build/ARM/gem5.opt",
                           "--outdir=" GEM5EXECUTABLE "/",
                            "--stats-file=" GEM5EXECUTABLE "/stats1.txt" ,
                           "--quiet","--redirect-stdout",
                            GEM5DIR "/configs/example/se.py",
                            "--mem-type=SimpleMemory","--cpu-type=arm_detailed","--caches",
                            "--cmd=" GEM5EXECUTABLE "/gem5_files/mainCPU/arm_bin",
                            buffer,
                            NULL};
        execv(executable, parmList);
    }

    std::string path = GEM5EXECUTABLE "/stats1.txt";

    std::ifstream inFile(path);

    std::string token ="sim_ticks";
    std::string line;
    char delim = ' ';
    int found =0;
    while (getline(inFile, line)) {
        if (line.find(token) != std::string::npos) {
            std::stringstream ss(line);
            std::string item;
            int counter = 0;
            while (std::getline(ss, item, delim)) {
                if (!item.empty())
                {
                    counter++;
                    if(counter==2)
                    {
                        found++;
                        if(found==2)
                        {
                            execution_time = std::stoi(item);
                            inFile.close();
                            return execution_time ; 
                        }
                        //break;
                    }
                }
            }
        }   
    }

    inFile.close();
#endif
    return execution_time; 
}


int LogGOPmod::locgem5op(goalevent& elem){

    if(print) printf("[%i] found locgem5op for binary %u of length %u - t: %lu (CPU: %i)\n", elem.host, elem.tag, elem.size, elem.time, elem.proc);
    if(nexto[elem.host][elem.proc] <= elem.time) {
        // check if OS Noise occurred
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+elem.size);


        btime_t  execution_time= simulateGEM5(elem.tag, elem.size); //elem.size
        if(print)        printf("execution_time %lu \n",execution_time);
        nexto[elem.host][elem.proc] = elem.time+execution_time+noise;
        // satisfy irequires
        //parser.schedules[elem.host].issue_node(elem.node);
        // satisfy requires+irequires
        //parser.schedules[elem.host].remove_node(elem.node);
        parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time);
        parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
        // add to timeline
        sim.tlviz->add_loclop(elem.host, elem.time, elem.time+execution_time, elem.proc, 0.6, 0.2, 0.4);
    } else {
        if(print) printf("[%i] -- locgem5op local o not available -- reinserting\n", elem.host);
        elem.time=nexto[elem.host][elem.proc];
        sim.reinsertEvent(&elem);
    }
    return 0;
}

int LogGOPmod::locop(goalevent& elem){



    if(print) printf("[%i] found loclop of length %u - t: %lu (CPU: %i)\n", elem.host, elem.size, elem.time, elem.proc);
    if(nexto[elem.host][elem.proc] <= elem.time) {

        // check if OS Noise occurred
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+elem.size);
        nexto[elem.host][elem.proc] = elem.time+elem.size+noise;
        // satisfy irequires
        //parser.schedules[elem.host].issue_node(elem.node);
        // satisfy requires+irequires
        //parser.schedules[elem.host].remove_node(elem.node);
        parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time);
        parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
        // add to timeline
        sim.tlviz->add_loclop(elem.host, elem.time, elem.time+elem.size, elem.proc, 0.6, 0.2, 0.4);
    } else {
        if(print) printf("[%i] -- locop local o not available -- reinserting\n", elem.host);
        elem.time=nexto[elem.host][elem.proc];
        sim.reinsertEvent(&elem);
    }
    return 0;
}

//receive the RTR message (rendez-vous case)
int LogGOPmod::receive_ack(AckEvent& elem){

    //printf("[%i] received rndvz ack at %lu\n", elem.host, elem.time);

    if (print) printf("[%i] received rndvz ack at %lu\n", elem.host, elem.time);

    parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);

    // TODO: do we need to add some latency for the ACK to
    // travel??? -- this can *not* be added here safely!

    //need to check the min bc other non-dependend ops could have been executed
    if(nexto[elem.host][elem.proc] < elem.time) nexto[elem.host][elem.proc] = elem.time;
    if(nextgs[elem.host][elem.nic] < elem.time) nextgs[elem.host][elem.nic] = elem.time;

    return 0;
}

int LogGOPmod::receive_msg(goalevent& elem){

    //elem.invert(); //to be compliant with the old code (it inverts host and target)
    

    if(print) printf("[%i] found msg from %i, t: %lu (CPU: %i)\n", elem.target, elem.host, elem.time, elem.proc);

    //this is broken: proc & nic should be of the matching receive, not of the incoming message
    if(std::max(nexto[elem.target][elem.proc], nextgr[elem.target][elem.nic]) <= elem.time /* local o,g available! */) {


        if(print) printf("-- msg o,g available (nexto: %lu, nextgr: %lu)\n", (long unsigned int) nexto[elem.host][elem.proc], (long unsigned int) nextgr[elem.host][elem.nic]);

        uint32_t tmp = elem.host;
        elem.host = elem.target;
        elem.target = tmp;

        ruqelem_t matched_elem;
        if(match(elem, &rq[elem.host], &matched_elem)) { // found it in RQ
            if(print) printf("-- found in RQ\n");
          
          
            btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
            nexto[elem.host][elem.proc] = elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G);
            /* message is only received after G is charged !! 
            TODO: consuming o seems a bit odd in the LogGP model but well in practice */
            nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G;

            if(elem.size > S) { // rendezvous - free remote request
                //printf("seding ack, expected\n");
                sim.addEvent(new AckEvent(elem)); // true to invert the host/target (ugly)   
                if(print) printf("-- satisfy remote requires on host %i\n", elem.target);
            }
            sim.tlviz->add_transmission(elem.target, elem.host, elem.starttime+o, elem.time, elem.size, G);
            sim.tlviz->add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);
            sim.tlviz->add_noise(elem.host, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc);
            // satisfy local requires
            parser.MarkNodeAsDone(elem.host, matched_elem.offset, elem.time);

        } else { // not in RQ

            if(print) printf("-- not found in RQ - add to UQ\n");

            ruqelem_t nelem;
            nelem.size = elem.size;
            nelem.src = elem.target;
            nelem.tag = elem.tag;
            nelem.proc = elem.proc;
            nelem.nic = elem.nic;
            nelem.offset = elem.offset;
            //nelem.starttime = elem.time; // when it was started
            nelem.starttime = elem.starttime;
            //TODO: double check. This is used as the time at which the receving is finished (exluding o). It is used for
            //the drawing of the o when the receive is posted.
            //nelem.starttime = elem.time+noise+std::max((elem.size-1)*O,(elem.size-1)*G);

            uq[elem.host].push_back(nelem);
        }
    } else {
        elem.time=std::max(nexto[elem.target][elem.proc], nextgr[elem.target][elem.nic]);
        if(print) printf("-- msg o,g not available -- reinserting with time: %lu\n", elem.time);
        sim.reinsertEvent(&elem);

    }
    return 0;
}

int LogGOPmod::send(goalevent& elem){



    if(print) printf("[%i] found send to %i (size: %u)- t: %lu (CPU: %i) - nexto: %lu - nextgs: %lu\n", elem.host, elem.target, elem.size, elem.time, elem.proc, nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic]);

    if(std::max(nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic]) <= elem.time) { // local o,g available!


        if(print) printf("-- satisfy local irequires\n");
        parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time);
        //check_hosts.push_back(elem.host);

        // check if OS Noise occurred
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
        nexto[elem.host][elem.proc] = elem.time + o + (elem.size-1)*O+ noise;
        nextgs[elem.host][elem.nic] = elem.time + g + (elem.size-1)*G; // TODO: G should be charged in network layer only
        if (print) printf("-- updated nexto: %lu - nextgs: %lu (g: %i; G: %i; (s-1)G: %u)\n", nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic], g, G, (elem.size-1)*G);
        sim.tlviz->add_osend(elem.host, elem.time, elem.time+o+ (elem.size-1)*O, elem.proc, (int) elem.tag);
        sim.tlviz->add_noise(elem.host, elem.time+o+ (elem.size-1)*O, elem.time + o + (elem.size-1)*O+ noise, elem.proc);

        //these two are useless, kept just for the eager stuff below
        int h = elem.host;
        elem.starttime = elem.time;

        elem.type = OP_MSG;
        elem.starttime = elem.time;

        if(print) printf("-- [%i] send inserting msg to %i, t: %lu\n", h, elem.target, elem.time);
        sim.addEvent(new NetMsgEvent(&elem, elem.time + o));

        if(elem.size <= S) { // eager message
            if(print) printf("-- [%i] eager -- satisfy local requires at t: %lu\n", h, (long unsigned int) elem.starttime+o);
            // satisfy local requires (host and target swapped!)
            parser.MarkNodeAsDone(h, elem.offset, elem.time);
            //tlviz.add_transmission(elem.target, elem.host, elem.starttime+o, elem.time-o, elem.size, G);
        } else {
            if(print) printf("-- [%i] start rendezvous message to: %i (offset: %i)\n", h, elem.host, elem.offset);
        }

    } else { // local o,g unavailable - retry later
        if(print) printf("-- send local o,g not available -- reinserting\n");
        elem.time = std::max(nexto[elem.host][elem.proc],nextgs[elem.host][elem.nic]);
        sim.reinsertEvent(&elem);
    }
    return 0;
}


int LogGOPmod::receive(goalevent& elem){

    if(print) printf("[%i] found recv from %i - t: %lu (CPU: %i)\n", elem.host, elem.target, elem.time, elem.proc);

    parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time);
    if(print) printf("-- satisfy local irequires\n");

    ruqelem_t matched_elem;
    if(match(elem, &uq[elem.host], &matched_elem))  { // found it in local UQ
        if(print) printf("-- found in local UQ\n");

        elem.target = matched_elem.src; // to handle wildcards

        if(elem.size > S) { // rendezvous - free remote request
            //printf("seding ack, UNexpected\n");

            sim.addEvent(new AckEvent(matched_elem, elem.time));   
            if(print) printf("-- satisfy remote requires at host %i (offset: %i)\n", elem.target, elem.offset);

            sim.tlviz->add_transmission(elem.host, matched_elem.src, matched_elem.starttime+o, elem.time, elem.size, G);
        }

        //btime_t drawtime = elem.time; //std::max(elem.time, matched_elem.starttime);

        sim.tlviz->add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);
        sim.tlviz->add_transmission(elem.target, elem.host, matched_elem.starttime+o, elem.time, elem.size, G, 1, 1, 0);
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
        nexto[elem.host][elem.proc] = elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G) /* message is only received after G is charged !! TODO: consuming o seems a bit odd in the LogGP model but well in practice */;
        nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G; //IS COMPLETELY INCORRECT HERE

        // satisfy local requires
        parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
        if(print) printf("-- satisfy local requires\n");

    } else { // not found in local UQ - add to RQ
        if(print) printf("-- not found in local UQ -- add to RQ\n");

        ruqelem_t nelem;

        nelem.size = elem.size;
        nelem.src = elem.target;
        nelem.tag = elem.tag;
        nelem.offset = elem.offset;
        rq[elem.host].push_back(nelem);
    }
    return 0;
}

void LogGOPmod::printStatus(){
        printf("Times: \n");
        for(unsigned int i=0; i<p; ++i) {
            btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
            btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
            btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
            std::cout << "Host " << i <<": o: " << maxo << "; gs: " << maxgs << "; gr: " << maxgr << "\n";
        }
 
     
}


size_t LogGOPmod::maxTime(){
    size_t max = 0;
    for(uint i=0; i<p; ++i) { // find maximum end time
        btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
        btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
        btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
        btime_t cur = std::max(std::max(maxgr,maxgs),maxo);
        //btime_t cur = maxo;
        if(cur > max) max=cur;
    }

    return max;
}

LogGOPmod::~LogGOPmod(){
    for(uint32_t i=0; i<p; ++i) {

        if(!uq[i].empty()) {
            printf("Warning: unexpected queue on host %i contains %lu elements!\n", i, uq[i].size());
            for(ruq_t::iterator iter=uq[i].begin(); iter != uq[i].end(); ++iter) {
                printf(" src: %i, tag: %i\n", iter->src, iter->tag);
            }
        }

        if(!rq[i].empty()) {
            printf("Warning: receive queue on host %i contains %lu elements!\n", i, rq[i].size());
            for(ruq_t::iterator iter=rq[i].begin(); iter != rq[i].end(); ++iter) {
                printf(" src: %i, tag: %i\n", iter->src, iter->tag);
            }
        }
    }
    delete osnoise;
}
