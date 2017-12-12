
#include "loggop.hpp"

bool LogGOPmod::match(const LogGPBaseEvent &elem, ruq_t *q, ruqelem_t *retelem, bool erase) {

    //std::cout << "UQ size " << q->size() << "\n";

    if(print) printf("++ [%i] searching matching queue for src %i tag %i\n", elem.host, elem.target, elem.tag);
    for(ruq_t::iterator iter=q->begin(); iter!=q->end(); ++iter) {
        if(elem.target == ANY_SOURCE || iter->src == ANY_SOURCE || iter->src == elem.target) {
            if(elem.tag == ANY_TAG || iter->tag == ANY_TAG || iter->tag == elem.tag) {
                if(retelem) *retelem=*iter;
                if (erase) q->erase(iter);
                return true;
            }
        }
    }
    return false;
}

int LogGOPmod::network(NetMsgEvent& elem){
    if(print) printf("[NETWORK] network msg from %i to %i, t: %lu\n", elem.source, elem.dest, elem.time);
    
    simEvent * msg = elem.getPayload();

    //the send is completed immediately
    complete_send(*((goalevent *) msg));


    msg->time = elem.time+L;
    assert(!msg->keepalive);
    sim.addEvent(msg);   
    return 0;
}

int LogGOPmod::dispatch(simModule* mod, simEvent* _elem){

    LogGOPmod* lmod = (LogGOPmod*) mod;
    switch (_elem->type){
        /* simulation events */
        case OP_SEND: return lmod->send(*((goalevent *) _elem));
        case OP_RECV: return lmod->receive(*((goalevent *) _elem));
        case NET_MSG: return lmod->network(*((NetMsgEvent *) _elem));
        case OP_MSG: return lmod->receive_msg(*((goalevent *) _elem));
        case OP_MSG_RTR: return lmod->receive_msg(*((goalevent *) _elem));
        case OP_MSG_RTS: return lmod->receive_msg(*((goalevent *) _elem));
        case OP_LOCOP: return lmod->locop(*((goalevent *) _elem));
    }
    
    return -1;
}


void * LogGOPmod::dispatch_signal(simModule * mod, sim_signal_t signal, void * arg){
    LogGOPmod* lmod = (LogGOPmod*) mod;
    switch (signal){
        case SIG_SEND_COMPLETE: return lmod->complete_send(*((goalevent *) arg));
    }
    return NULL;
}

void * LogGOPmod::complete_send(goalevent& elem){
 

    if(print) printf("[%i] satisfy local requires for send at t: %lu; elem.offset: %u\n", elem.host, (long unsigned int) elem.time, elem.offset);
    // parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
    if (elem.type != OP_MSG_RTR && elem.type!=OP_MSG_RTS){ 
        parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
    }//else printf("-- ignore! (type: %i)\n", elem.type);

    //if (elem.type==OP_MSG_RTR){ printf("complete_send: OP_MSG_RTR\n"); }
    //if (elem.type==OP_MSG_RTS){ printf("complete_send: OP_MSG_RTS\n"); }

    return NULL;   
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
        //tara sim.tlviz->add_loclop(elem.host, elem.time, elem.time+elem.size, elem.proc, 0.6, 0.2, 0.4);
    } else {
        if(print) printf("[%i] -- locop local o not available -- reinserting\n", elem.host);
        elem.time=nexto[elem.host][elem.proc];
        sim.reinsertEvent(&elem);
    }
    return 0;
}


int LogGOPmod::receive_msg(goalevent& elem){

    //elem.invert(); //to be compliant with the old code (it inverts host and target)
    
    if(print) printf("[%i] found msg from %i, t: %lu (CPU: %i)\n", elem.target, elem.host, elem.time, elem.proc);

    //this is broken: proc & nic should be of the matching receive, not of the incoming message
    int nicavail_recv = nextgr[elem.target][elem.nic] <= elem.time;
    int nicavail_send = nextgs[elem.target][elem.proc] <= elem.time;
    int hostavail = nexto[elem.target][elem.proc] <= elem.time;

    if(nicavail_recv && ((elem.type!=OP_MSG_RTR && hostavail) || (elem.type==OP_MSG_RTR && nicavail_send))){

        if(print) printf("-- msg o,g available (nexto: %lu, nextgr: %lu; nextgs: %lu)\n", (long unsigned int) nexto[elem.host][elem.proc], (long unsigned int) nextgr[elem.host][elem.nic], nextgs[elem.host][elem.nic]);

        uint32_t tmp = elem.host;
        elem.host = elem.target;
        elem.target = tmp;


        ruqelem_t matched_elem;

        if (elem.type==OP_MSG_RTR){ /* we don't pay the o here (like it is a get from the receiver */

            
            nextgs[elem.host][elem.nic] = elem.time + g + ((uint64_t) elem.size-1)*G;
            nextgr[elem.host][elem.nic] = elem.time + g;

            elem.type = OP_MSG;
            sim.addEvent(new NetMsgEvent(&elem, elem.time /* + something? */));
            if(print) printf("-- got RTR, sending DATA to: %i; elem.time: %lu; nextgs: %lu; size: %u (%u, %u)\n", elem.target, elem.time, nextgs[elem.host][elem.nic], elem.size, elem.host, elem.nic);

            /* now this is done in complete_send */
            //parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);    
           
        } else {
            if(match(elem, &rq[elem.host], &matched_elem, elem.type!=OP_MSG_RTS)) { // found it in RQ (do not remove the matching from the queue if it's a RTS)
                if(print) printf("-- found in RQ\n");
                btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);



                if(elem.type==OP_MSG_RTS) { // rendezvous - send RTR back

                    nexto[elem.host][elem.proc] = elem.time + o + noise;
                    nextgr[elem.host][elem.nic] = elem.time + g;

                    elem.type = OP_MSG_RTR;
                    sim.addEvent(new NetMsgEvent(&elem, elem.host, elem.target, 1, elem.time + o));   
                    if(print) printf("-- sending RTR to: %i\n", elem.target);
                }else{ //eager or rendezvous data

                    nexto[elem.host][elem.proc] = elem.time + o + noise + std::max(((uint64_t) elem.size-1)*O, ((uint64_t) elem.size-1)*G);
                    /* message is only received after G is charged !! 
                    TODO: consuming o seems a bit odd in the LogGP model but well in practice */
                    nextgr[elem.host][elem.nic] = elem.time + g + ((uint64_t) elem.size-1)*G;

                    // satisfy local requires
                    parser.MarkNodeAsDone(elem.host, matched_elem.offset, elem.time);

                    //tara sim.tlviz->add_transmission(elem.target, elem.host, elem.starttime+o, elem.time, elem.size, G);
                    //tara sim.tlviz->add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);
                    //tara sim.tlviz->add_noise(elem.host, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc);
                }
            } else { // not in RQ
    
                //assert(elem.size<=S || elem.type==OP_MSG_RTR);
                assert(elem.size<=S || elem.type==OP_MSG_RTS);
                if(print) printf("-- not found in RQ - add to UQ\n");
                ruqelem_t nelem;
                nelem.size = elem.size;
                nelem.src = elem.target;
                nelem.tag = elem.tag;
           
                nelem.starttime = elem.starttime;

                if (elem.type==OP_MSG_RTS) {
                    nelem.origin_elem = &elem;
                    elem.keepalive=true;
                }

                uq[elem.host].push_back(nelem);

            }
        }
    } else {

        elem.time = std::max(nextgr[elem.target][elem.nic] , elem.type==OP_MSG_RTR ? nextgs[elem.target][elem.proc] : nexto[elem.target][elem.proc]);

        //elem.time=std::max(nexto[elem.target][elem.proc], nextgr[elem.target][elem.nic]);
        if(print) printf("-- msg o,g not available -- reinserting with time: %lu\n", elem.time);
        sim.reinsertEvent(&elem);

    }
    return 0;
}

int LogGOPmod::send(goalevent& elem){

    if(print) printf("[%i] found send to %i (size: %u)- t: %lu (CPU: %i) - nexto: %lu - nextgs: %lu; offset: %u\n", elem.host, elem.target, elem.size, elem.time, elem.proc, nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic], elem.offset);

    if(std::max(nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic]) <= elem.time) { // local o,g available!
        if(print) printf("-- satisfy local irequires\n");
        parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time);
        //check_hosts.push_back(elem.host);

        elem.starttime = elem.time;
        

        // check if OS Noise occurred
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);

        NetMsgEvent * tosend;        

        int h = elem.host;


        if (elem.size > S){

            elem.type = OP_MSG_RTS;

            if(print) printf("-- [%i] start rendezvous message to: %i (offset: %i)\n", h, elem.host, elem.offset);

            nexto[elem.host][elem.proc] = elem.time + o + ((uint64_t) elem.size-1)*O + noise;
            nextgs[elem.host][elem.nic] = elem.time + g; // TODO: G should be charged in network layer only
            if (print) printf("-- updated nexto: %lu - nextgs: %lu (g: %i; G: %i; (s-1)G: %lu)\n", nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic], g, G, ((uint64_t) elem.size-1)*G);
            //tara sim.tlviz->add_osend(elem.host, elem.time, elem.time+o, elem.proc, (int) elem.tag);
            //tara sim.tlviz->add_noise(elem.host, elem.time + o, elem.time + o + noise, elem.proc);

            //override the msg size to 1B
            tosend = new NetMsgEvent(&elem, h, elem.target, 1, elem.time + o);

        }else { //eager 

            elem.type = OP_MSG;

            //Send message
            nexto[elem.host][elem.proc] = elem.time + o + ((uint64_t) elem.size-1)*O+ noise;
            nextgs[elem.host][elem.nic] = elem.time + g + ((uint64_t) elem.size-1)*G; // TODO: G should be charged in network layer only
            if (print) printf("-- updated nexto: %lu - nextgs: %lu (g: %i; G: %i; (s-1)G: %lu)\n", nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic], g, G, ((uint64_t) elem.size-1)*G);
            //tara sim.tlviz->add_osend(elem.host, elem.time, elem.time+o+ (elem.size-1)*O, elem.proc, (int) elem.tag);
            //tara sim.tlviz->add_noise(elem.host, elem.time+o+ (elem.size-1)*O, elem.time + o + (elem.size-1)*O+ noise, elem.proc);

            tosend = new NetMsgEvent(&elem, elem.time + o);

            /* now this is done in complete_send */
            //if(print) printf("-- [%i] eager -- satisfy local requires at t: %lu\n", h, (long unsigned int) elem.starttime+o);
            // satisfy local requires (host and target swapped!)
            //parser.MarkNodeAsDone(h, elem.offset, elem.time);
    
        }


        //these two are useless, kept just for the eager stuff below


        if(print) printf("-- [%i] send inserting msg to %i, t: %lu\n", h, tosend->dest, tosend->time);
        sim.addEvent(tosend);


    

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

    bool addtorq = true;

    ruqelem_t matched_elem;
    if(match(elem, &uq[elem.host], &matched_elem))  { // found it in local UQ
        if(print) printf("-- found in local UQ\n");

        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);


        if(elem.size > S) { // rendezvous - free remote request

            // send RTR back
            goalevent * oelem = (goalevent *) matched_elem.origin_elem;
            oelem->keepalive=false;
            oelem->type = OP_MSG_RTR;
            sim.addEvent(new NetMsgEvent(oelem, oelem->host, oelem->target, 1, elem.time + o));   
            if(print) printf("-- it's an unexpected RTS, sending RTR to: %i\n", elem.target);

            nexto[elem.host][elem.proc] = elem.time + o + noise;
            nextgs[elem.host][elem.nic] = elem.time + g;
 
            //tara sim.tlviz->add_orecv(elem.host, elem.time, elem.time+o, elem.proc, (int) elem.tag);
            //TODO: need to add sim.tlviz transmission of the RTR
           
        }else{

            addtorq=false;
            //tara sim.tlviz->add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);
            //tara sim.tlviz->add_transmission(elem.target, elem.host, matched_elem.starttime+o, elem.time, elem.size, G, 1, 1, 0);
            nexto[elem.host][elem.proc] = elem.time + o + noise + std::max(((uint64_t) elem.size-1)*O,((uint64_t) elem.size-1)*G);
            nextgr[elem.host][elem.nic] = elem.time + g + ((uint64_t) elem.size-1)*G; 

            // satisfy local requires
            parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
            if(print) printf("-- satisfy local requires\n");
        }
    }

    if (addtorq) { // not found in local UQ - add to RQ
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
    if(!batchmode) { // print all hosts
        printf("Times: \n");
        for(unsigned int i=0; i<p; ++i) {
            btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
            btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
            btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
            std::cout << "Host " << i <<": o: " << maxo << "; gs: " << maxgs << "; gr: " << maxgr << "\n";
        }
    } else { // print only maximum
        long long unsigned int max=0;
        int host=0;
        for(uint i=0; i<p; ++i) { // find maximum end time
            btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
            btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
            btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
            btime_t cur = std::max(std::max(maxgr,maxgs),maxo);
            if(cur > max) {
                host=i;
                max=cur;
            }
        }
        std::cout << "Maximum finishing time at host " << host << ": " << max << " ("<<(double)max/1e9<< " s)\n";
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

