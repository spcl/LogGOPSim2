
#include "NetMod.hpp"


uint64_t HeadPkt::idgen = 0;

int NetMod::dispatch(simModule* mod, simEvent* _elem){

    NetMod* nmod = (NetMod*) mod;

    switch (_elem->type){
        case NET_MSG: return nmod->ingress(*((NetMsgEvent *) _elem));
        case NET_PKT: return nmod->_forward(*((DataPkt *) _elem));
    }
    return 0;
}

void * NetMod::dispatch_signal(simModule* mod, sim_signal_t signal, void * arg){

    NetMod* nmod = (NetMod*) mod;

    switch (signal){
        case SIG_PKT_RECEIVED: return nmod->release_credit(*((DataPkt *) arg), ((DataPkt *) arg)->time);
    }
    return NULL;
}

