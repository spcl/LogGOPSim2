#include "p4.hpp"

 

#define OPT_USE_ONCE 1
#define OPT_OVERFLOW_LIST 2
#define OPT_PRIORITY_LIST 4

#define GET_MSG_SIZE 5




int P4Mod::dispatch(simModule* mod, simEvent* _elem){

    LogGOPmod::dispatch(mod, _elem);
    
    P4Mod* pmod = (P4Mod *) mod;

    switch(_elem->type){
        case OP_APPEND:
        case OP_NAPPEND:
            return pmod->append(*((goalevent *) _elem));
        case OP_PUT:
        case OP_GET:
            return pmod->putget(*((goalevent *) _elem));
        case OP_NPUT:
        case OP_NGET:
            return pmod->nicputget(*((goalevent *) _elem));
        case OP_PUTMSG:
        case OP_GETMSG:
            return pmod->putgetmsg(*((goalevent *) _elem));
        case OP_GETREPLY:
            return pmod->getreply(*((goalevent *) _elem));
        case OP_TPUT:
        case OP_TAPPEND:
        case OP_CTWAIT:
        case OP_TGET:
            return pmod->tops(*((goalevent *) _elem));
        case OP_NCTWAIT:
            return pmod->nctwait(*((goalevent *) _elem));

    }
    return -1;
}

void P4Mod::printStatus(){
    LogGOPmod::printStatus();
}


bool P4Mod::p4_match(const goalevent &elem, ruq_t *q, ruq_t::iterator * matchedit, uint32_t header_id) {

  //std::cout << "UQ size " << q->size() << "\n";

  //printf("++ [%i] searching matching queue for src %i tag %i\n", elem.host, elem.target, elem.tag);
  for(ruq_t::iterator iter=q->begin(); iter!=q->end(); ++iter) {

    //printf("elem.host %i; iter->target: %i\n", elem.host, iter->target);
    if(iter->target == ANY_SOURCE || iter->target==elem.target) {

      if(iter->tag == ANY_TAG || iter->tag  == elem.tag) {
        // printf("match: use_once: %i\n", iter->use_once);
        //if(retelem) *retelem= *iter;
        if(iter->use_once && header_id!=0 && iter->matched_to!=0 && iter->matched_to!=header_id) //matched to different header
        {
            continue; //
        }

        if(iter->use_once && header_id!=0 && iter->matched_to==0) //matched to current header
        {
            iter->matched_to=header_id;
        }

        *matchedit = iter;

        //if (iter->use_once && remove) q->erase(iter);


        return true;
      }
    }
  }
  return false;
}


bool P4Mod::p4_match_unexpected(const goalevent &elem, uqhdr_t *q, uqhdr_entry_t *retelem) {

  //std::cout << "UQ size " << q->size() << "\n";

  if(print) printf("++ [%i] searching matching unexpected header queue for src %i tag %i; uq size: %lu\n", elem.host, elem.target, elem.tag, q->size());
  for(uqhdr_t::iterator iter=q->begin(); iter!=q->end(); ++iter) {

    if (print) printf("++ [%i] searching matching unexpected header queue for src %i tag %i; iter->msg: (%i, %i)\n", elem.host, elem.target, elem.tag, iter->msg.host, iter->msg.tag);

    //iter->msg.target contains the rank of the sender (see NPUT case)
    if(elem.target == ANY_SOURCE || iter->msg.target == elem.target) {
      if(elem.tag == ANY_TAG || iter->msg.tag == elem.tag) {
        if (print) printf("++ [%i] header found in unexepcted queue", elem.host);
        if(retelem) *retelem=*iter;
        q->erase(iter);
        return true;
      }
    }
  }
  return false;
}


int P4Mod::append(goalevent& elem){

    if (print) printf("[%i] append found. tag: %i; hh: %u; ph: %u; th: %u; mem: %u\n", elem.host, elem.tag, elem.hh, elem.ph, elem.ch,elem.mem);
    if((elem.type==OP_APPEND && nexto[elem.host][elem.proc] <= elem.time) || (elem.type==OP_NAPPEND && nextgs[elem.host][elem.nic] <= elem.time)) {
        if (elem.type!=OP_NAPPEND){
            btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
            //if (noise>0) printf("append noise: %lu\n", noise);
            nexto[elem.host][elem.proc] = elem.time + o + noise;
            sim.tlviz->add_loclop(elem.host, elem.time, elem.time+o, elem.proc);
        }else{
            //NAPPEND: is the triggered append operations (executed on the NIC)
            //The cost of this operations is charged on nextgs because while
            //the nextgr is something that depends on external events (incoming messages),
            //the nextgs depends on the operations performed on the NIC by the owner host.
            //TODO: double check. Verify if it is the case to model it in a different way.
            //The cost c is necessary to scan the unexpected list in order to find
            //unexpected messages.
            nextgs[elem.host][elem.nic] = elem.time + c;
            sim.tlviz->add_nicop(elem.host, elem.time, elem.time+c, elem.nic);
        }
        parser.MarkNodeAsStarted(elem.host, elem.offset, elem.time); //useless?

            
        int i=0; //count the number of matches
        if ((elem.options & OPT_PRIORITY_LIST) == OPT_PRIORITY_LIST){
            uqhdr_entry_t matched_elem;
            while((i==0 || (elem.options & OPT_USE_ONCE) != OPT_USE_ONCE) && p4_match_unexpected(elem, &uq_hdr[elem.host], &matched_elem)){
                if (print) printf("-- PTL: match found in UL\n");
                if (matched_elem.msg.type == OP_GETMSG){
                    //TODO: nothing. The P4 spec says that in this case a proper event is generetad. We are not modelling P4 events.
                }else if (matched_elem.msg.type == OP_PUTMSG){
                    //ACCOUNT FOR COPY. PORTALS4 spec said that no copy is performed here. A proper event indicating the start
                    //address of the data in the overflow buffer is raised. However, in order to simulate the MPI semantic, we must
                    //take in account the copy to the posted buffer (recall that simulated_behavior = f(traces, params)).
                    //Assumption: we assume that the copy perfermoed through MM/IO has the same speed of the one perfomerd through DMA.
                    //if (elem.type==OP_NAPPEND){ //If it is a triggered append, the copy is charged on the NIC
                    //    nextgs[elem.host][elem.nic] += (matched_elem.msg.size)*C; //latency?
                    //    tlviz.add_nicop(elem.host, elem.time+c, elem.time+c+(matched_elem.msg.size)*C, elem.nic, 0, 0, 0);
                    //The copy is charged only if the op is an APPEND. If it is a NAPPEND, thus executed on the NIC, then
                    //no copy is perfomed since the NIC cannot copy data from MAINMEMORY TO MAINMEMORY
                    if (elem.type==OP_APPEND) {
                        //Accounting of unex. msg copy cost disabled to be comparable with the MPI case (where it's not accounted)
                        //nexto[elem.host][elem.proc] += (matched_elem.msg.size)*C; //latency?
                        sim.tlviz->add_loclop(elem.host, elem.time+o, elem.time+o+(matched_elem.msg.size)*C, elem.proc, 0, 0, 0);
                    }

                }else{
                   if(print) printf("-- unexpected message request\n");
                }


                TRIGGER_OPS(elem.host, elem.oct, elem.time+o+DMA_L)
                i++;
            }
        }
        //if the ME is for the OVERFLOW_LIST OR is for the PRIORITY LIST but no matching in UL are found OR is for the
        //priority list, matching in UL are found AND the ME is persistent:
        if ((elem.options & OPT_OVERFLOW_LIST) == OPT_OVERFLOW_LIST || i==0 || (elem.options & OPT_USE_ONCE) != OPT_USE_ONCE){
            p4_ruqelem_t nelem;
            nelem.counter = elem.oct;
            nelem.size = elem.size;
            nelem.tag = elem.tag;
            nelem.target = elem.target;
            nelem.src = elem.host;
            nelem.use_once = ((elem.options & OPT_USE_ONCE) == OPT_USE_ONCE);
            nelem.type = 0; //Useless here. It is used (DOUBLE CHECK)
            nelem.hh = elem.hh;
            nelem.ph = elem.ph;
            nelem.ch = elem.ch;
            nelem.mem = elem.mem;
            nelem.matched_to = 0;

            nelem.pending = (uint8_t*)malloc(sizeof(uint8_t));
          //  nelem.mem = 32;
            if(nelem.mem){
                nelem.shared_mem=malloc( nelem.mem );
            } 

            int i;
            for(i=3; i>=0; i--){
            
               // printf("arg[%d]=%lu \n",i,elem.arg[i]);
                if(elem.arg[i] != (uint64_t)-1) break;

            }

            if(i>=0){
                if((i+1)*sizeof(uint64_t)>nelem.mem){
                    printf("Error: specify size of shared memory before using it in Append\n");
                    printf("You need at least %lu bytes \n",(i+1)*sizeof(uint64_t));
                    exit(-1);
                } else {
                    memcpy(nelem.shared_mem,elem.arg,(i+1)*sizeof(uint64_t));
                }
            }


            if ((elem.options & OPT_OVERFLOW_LIST) == OPT_OVERFLOW_LIST) uq[elem.host].push_back(nelem);
            else rq[elem.host].push_back(nelem);
        }

        parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);


    }else{
        if (print) printf("-- no time. Reinserting.\n");
        elem.time=nexto[elem.host][elem.proc];
        sim.reinsertEvent(&elem);
    }
    return 0;
}

int P4Mod::putget(goalevent& elem){
    if (nexto[elem.host][elem.proc] <= elem.time) {
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
        nexto[elem.host][elem.proc] = elem.time + o + noise;
        //if (noise>0) printf("put noise: %lu\n", noise);

        sim.tlviz->add_loclop(elem.host, elem.time, elem.time+o, elem.proc);

        elem.time = elem.time + o;


        if (elem.type==OP_PUT) elem.type = OP_NPUT;
        else elem.type = OP_NGET;
        if (elem.isParsed) parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);

        sim.tlviz->add_nicop(elem.host, elem.time, elem.time+DMA_L, elem.nic,0.3,0.3,0.3);
        elem.time += DMA_L; //ktaranov

    }else{
        elem.time=nexto[elem.host][elem.proc];
    }
    sim.reinsertEvent(&elem);
    return 0;
}

int P4Mod::nicputget(goalevent& elem){
    uint32_t msg_size;
    if (nextgs[elem.host][elem.nic] <= elem.time) {
        if (print) printf("[%i] nput/nget op to %i elem.time: %lu\n", elem.host, elem.target, elem.time);

        //Hack: if elem.tag=-1 then is a ct_inc
        if (elem.tag == (uint32_t) -1 && elem.oct != (uint32_t) -1 && elem.isParsed){
           if(print) printf("ctinc!\n");
            TRIGGER_OPS(elem.host, elem.oct, elem.time+DMA_L)
            return 0;
        }
        //end hack


        int srate = std::max(G, C);

        /* if elem.type==OP_GET, the field "size" contains the size of the data to be read, not
        the size of the message to be sent, which will be only a get request. */
        msg_size = (elem.type==OP_NPUT) ? elem.size : GET_MSG_SIZE;

        /*  FIXME: P4MOD does not support eager, rendezvous  
        msg_size = (elem.type==OP_NPUT) ? ((elem.size > S) ? pktsize :  elem.size): GET_MSG_SIZE;
        */

         
        //update time:
        //printf("PUT: time: %i\n", elem.time + g +(msg_size-1)*srate);
        nextgs[elem.host][elem.nic] = elem.time + g +(msg_size-1)*srate;

        //Insert into the network layer (in order to simulate congestion)
        //net.insert(elem.time, elem.host, elem.target, msg_size, &elem.handle);
        //printf("nput op STARTED\n");
        sim.tlviz->add_nicop(elem.host, elem.time, elem.time+(msg_size-1)*srate, elem.nic);
        sim.tlviz->add_nicop(elem.host, elem.time+(msg_size-1)*srate, elem.time+(msg_size-1)*srate+g, elem.nic);
   
      
        //With the GET the operations are triggered on the event PTL_MD_EVENT_REPLY (when we receive the reply)
        if (elem.type!=OP_NGET && elem.isParsed) TRIGGER_OPS(elem.host, elem.oct, elem.time+DMA_L)

        elem.type = (elem.type==OP_NPUT) ? OP_PUTMSG : OP_GETMSG;
        //uint32_t source = elem.host;
        //elem.host = elem.target;
        //elem.target = source;
        elem.starttime = elem.time; //ktaranov
        
        sim.addEvent(new NetMsgEvent(&elem, msg_size, elem.time));
        /*elem.time+=L;
        elem.keepalive=true;
        sim.addEvent(&elem);
        */

        //Triggering operations
        //graph_node_properties triggered_op;
        //while (cttable.getNextOperation(elem.host, elem.oct, triggered_op)) aq.push(triggered_op);


    }else{
        elem.time=nextgs[elem.host][elem.nic];
        sim.reinsertEvent(&elem);
    }
    return 0;
}


int P4Mod::putgetmsg(goalevent& elem){

    if (print) printf("[%i] put/get msg (received). tag: %i; nextgr: %lu; elem.time: %lu\n", elem.target, elem.tag, nextgr[elem.target][elem.nic], elem.time);


    uint32_t msg_size = (elem.type==OP_PUTMSG) ? elem.size : (uint64_t) GET_MSG_SIZE;

    //###########CHECK BETTER THIS CONDITION!!!
    /*if((earliestfinish = net.query(elem.starttime, elem.time, elem.target, elem.host, elem.size, &elem.handle)) <= elem.time &&
        std::max(std::max(nextgr[elem.host][elem.nic],nextc[elem.host][elem.nic]), earliestfinish) <= elem.time) &&
        (elem.type!=OP_GETMSG || std::max(std::max(nextgs[elem.host][elem.nic],nextc[elem.host][elem.nic]), earliestfinish) <= elem.time)){*/
    if((nextgr[elem.target][elem.nic] <= elem.time) &&
        (elem.type!=OP_GETMSG || nextgs[elem.target][elem.nic] <= elem.time)){

        uint32_t tmp = elem.host;
        elem.host = elem.target;
        elem.target = tmp;

        nextgr[elem.host][elem.nic] = elem.time+(elem.size-1)*G+std::max(g, c); //TODO: Double check here for "c"


        ruq_t * q;
        p4_ruqelem_t matched;
        ruq_t::iterator matchedit;
        bool nomatch=false;
        if (p4_match(elem, &rq[elem.host], &matchedit)){
            if (print) printf("-- ME found in PL\n");
            matched = *matchedit;
            q = &rq[elem.host];
            //DOUBLE CHECK: it seems that we have do to nothing here.
        }else if(p4_match(elem, &uq[elem.host], &matchedit)){
            uqhdr_entry_t t;
            matched = *matchedit;
            q = &uq[elem.host];
            t.buffer = matched;
            t.msg = elem;
            uq_hdr[elem.host].push_back(t);
            if (print) printf("-- ME found in OL\n");
        }else { if (print) {printf("-- NO MATCHING!\n");} nomatch=true; } //drop message


        int srate = std::max(G, C);

        sim.tlviz->add_nicop(elem.host, elem.time, elem.time+(msg_size-1)*srate, elem.nic, 0.5, 0.5, 0.5);
        sim.tlviz->add_nicop(elem.host, elem.time+(msg_size-1)*srate, elem.time+(msg_size-1)*srate+c, elem.nic);

        if (print) printf("-- msgsize: %u from %i to %i\n", msg_size, elem.target, elem.host);
        sim.tlviz->add_transmission(elem.target, elem.host, elem.starttime, elem.time, msg_size, srate);

        if (!nomatch) {
            btime_t triggertime;
            
            /* remove ME */
            if (matched.use_once) q->erase(matchedit);


            if (elem.type==OP_PUTMSG){

                //don't charge the copy to the buffer here. Assume that, after the header is received,
                //a byte is copied in the matching buffer as soon as received. This means that the copy
                //is charged in (elem.size-1) * srate, where srate=max(G, C) (we can assume G>C).
                //nextgr[elem.host][elem.nic] += (elem.size-1)*C;
                //tlviz.add_nicop(elem.host, elem.time+(elem.size-1)*srate+c, elem.time+(elem.size-1)*srate+c+(elem.size-1)*C, elem.nic, 0, 0, 0);
                triggertime = elem.time+(msg_size-1)*srate+c;//+(msg_size-1)*C;

            }else{

                triggertime = elem.time+(msg_size-1)*srate+c;

                //The message will be sent after the request will be received ((in_msg_size-1)*C) and evaluated (c)
                elem.time = elem.time+(msg_size-1)*srate+c;

                //Charge gs for the sending for the GET REPLY message
                nextgs[elem.host][elem.proc] = elem.time + g + (elem.size-1)*srate;

                //Visualize the sending step
                sim.tlviz->add_nicop(elem.host, elem.time, elem.time+(elem.size-1)*srate, elem.nic);

                //Insert into the network layer (in order to simulate congestion)
                //net.insert(elem.time, elem.host, elem.target, elem.size, &elem.handle);

                //Add the new operation to the queue
                elem.type=OP_GETREPLY;
                //uint32_t h = elem.host;
                //elem.host = elem.target;
                //elem.target = h;
                elem.starttime = elem.time;
                sim.addEvent(new NetMsgEvent(&elem, elem.time));
            }
            sim.tlviz->add_nicop(elem.host, triggertime, triggertime+DMA_L, elem.nic,0.3,0.3,0.3);
            triggertime+=DMA_L; //ktaranov

            if (print) printf("-- MSG: matched.src: %i; counter: %i; time: %lu;\n", matched.src, matched.counter, triggertime);
            if (print) printf("-- Triggering new ops at time: %lu\n", triggertime);
            // To trigger an operation we don't have to wait g
            TRIGGER_OPS(matched.src, matched.counter, triggertime)

            //nextc[elem.host][elem.nic] = elem.time+c+(elem.size-1)*C; //message copy through DMA (assumed)
            //add trasmission from nic to cpu? (visualization) NO! the trasmission is btw the nic and the memory!!!!

        }




    }else{
        //####THIS IS WRONG!!
        //elem.time = std::max(std::max(nextgr[elem.host][elem.nic], nextc[elem.host][elem.nic]), earliestfinish);

        uint64_t gwait = (elem.type==OP_PUTMSG) ? nextgr[elem.target][elem.nic] : std::max(nextgr[elem.target][elem.nic], nextgs[elem.target][elem.nic]);
        if (print) printf("-- no time, reinserting: elem.time: %lu; new time: %lu\n", elem.time, gwait);
        
        elem.time = gwait;
        sim.reinsertEvent(&elem);
    }

    return 0;
}


int P4Mod::getreply(goalevent& elem){
    if(nextgr[elem.target][elem.nic] <= elem.time){
        if (print) printf("[%i] getreply (received). tag: %i; nextgr: %lu; elem.time: %lu\n", elem.target, elem.tag, nextgr[elem.target][elem.nic], elem.time);
       
        uint32_t tmp=elem.host;
        elem.host = elem.target;
        elem.target=tmp;

        
       
        int srate = std::max(G, C);
        //As in the PUTMSG case, we don't have to charge the copy. ((elem.size-1)*C)
        nextgr[elem.host][elem.nic] = elem.time + g + (elem.size-1)*srate;// + (elem.size-1)*C;

        btime_t triggertime = elem.time + (elem.size-1)*srate + c ;
        sim.tlviz->add_nicop(elem.host, triggertime, triggertime+DMA_L, elem.nic,0.3,0.3,0.3);
        TRIGGER_OPS(elem.host, elem.oct, triggertime+DMA_L)

        if (print) printf("-- Triggering new ops at time: %lu\n", triggertime+DMA_L);
       // if (print) printf("-- msgsize: %u from %i to %i %u %u\n", elem.size, elem.target, elem.host,elem.oct, cttable.ct_get_val(elem.host, elem.oct));
       

        

        sim.tlviz->add_nicop(elem.host, elem.time, elem.time + (elem.size-1)*srate, elem.nic, 0.5, 0.5, 0.5);
        sim.tlviz->add_nicop(elem.host, elem.time+(elem.size-1)*srate, elem.time+(elem.size-1)*srate+c, elem.nic);

        //tlviz.add_nicop(elem.host, elem.time+(elem.size-1)*srate, elem.time + (elem.size-1)*srate + (elem.size-1)*C, elem.nic, 0, 0, 0);
        sim.tlviz->add_transmission(elem.target, elem.host, elem.starttime, elem.time, elem.size, srate);

    }else{
        elem.time = nextgr[elem.target][elem.nic];
        sim.reinsertEvent(&elem);
    }
    return 0;
}


int P4Mod::tops(goalevent& elem){
    if (nexto[elem.host][elem.proc] <= elem.time) {
        nexto[elem.host][elem.proc] = elem.time + o;

        if (elem.type!=OP_CTWAIT){
            parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
        }

        if      (elem.type==OP_TPUT) elem.type = OP_NPUT;
        else if (elem.type==OP_TAPPEND) elem.type = OP_NAPPEND;
        else if (elem.type==OP_CTWAIT) elem.type = OP_NCTWAIT;
        else if (elem.type==OP_TGET) elem.type = OP_NGET;


        if (print) printf("[%i] triggered op setup: type: %i; ct: %i; threshold: %lu; time: %lu\n", elem.host, elem.type, elem.ct, elem.threshold, elem.time);

        sim.tlviz->add_loclop(elem.host, elem.time, elem.time+o, elem.proc, 0.5, 0.7, 1);
        btime_t noise = osnoise->get_noise(elem.host, elem.time, elem.time+o);
        elem.time = elem.time+o+noise;
        elem.keepalive=true;

        if (cttable.ct_get_val(elem.host, elem.ct)>=elem.threshold){
            //if (elem.type==OP_NCTWAIT && aq.size()>0) printf("CT WAIT: something will overlap with CTWAIT (size: %i)!\n", aq.size());
            sim.reinsertEvent(&elem);
        }else cttable.ct_enqueue_op(elem.host, elem.ct, &elem);


    }else{
        elem.time=nexto[elem.host][elem.proc];
        sim.reinsertEvent(&elem);
    }
    return 0;
}

int P4Mod::nctwait(goalevent& elem){
    nexto[elem.host][elem.proc] = elem.time;
    printf("nctwait time: %lu\n", elem.time);
    parser.MarkNodeAsDone(elem.host, elem.offset, elem.time);
    return 0;
}

