
#include "NetMod.hpp"


uint64_t HeadPkt::idgen = 0;

int NetMod::dispatch(SimModule* mod, simevent* _elem){

    NetMod* nmod = (NetMod*) mod;

    switch (_elem->type){
    	case NET_MSG: return nmod->ingress(*((NetMsgEvent *) _elem));
	    case NET_PKT: return nmod->forward(*((DataPkt *) _elem));
    }
    return 0;
}
