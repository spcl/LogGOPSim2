
#ifndef __P4SMP_EVENTS_HPP__
#define __P4SMP_EVENTS_HPP__

#include "../portals4/p4.hpp"

#include "gem5/clientlib/portals.h"

#define ISSET(X) (X != (uint32_t) -1)

#define HSTART 0 
#define HHEADER 1
#define HPAYLOAD 2
#define HCOMPLETION 3
#define NOHANDLER 4

static char * handler_ret_code[] = {"SUCCESS", "PROCEED", "PROCESS_DATA", "PROCESS_DATA_PENDING", "DROP", "DROP_PENDING", "SUCCESS_PENDING"};
 

struct hpu {
    btime_t time = 0;
    unsigned char hid;

    bool operator<(const hpu & d2) const {
           return d2.time < time; 
    }

    bool operator==(const hpu & d2) const {
           return d2.hid == hid; 
    }
};

typedef struct hpu hpu_t;



class MatchedHostDataPkt: public HostDataPkt{
public:

    p4_ruqelem_t matched;
    bool isSuspended = false;
   
    uint32_t host;
 
    btime_t start_hpu_time;
 
    /* hpu where handler was executing before suspension */
    hpu_t hpu; 

    /* handler state machine */
    uint32_t currentHandler; 

    bool isCurrentHandlerSuspended(){
        return isSuspended;
    }
    
    void suspendCurrentHandler(hpu_t _hpu){
        assert(!isSuspended);
        hpu = _hpu;
        isSuspended = true;
    }    

    hpu_t resumeCurrentHandler(){
        assert(isSuspended);
        hpu.time = time;
        isSuspended=false;
        return hpu;
    }

    uint32_t currentHandlerIdx(){
        assert(currentHandler!=HSTART && currentHandler!=NOHANDLER);
        if (currentHandler==HHEADER) return matched.hh;
        if (currentHandler==HPAYLOAD) return matched.ph;
        if (currentHandler==HCOMPLETION) return matched.ch;
        return -1;
    }
    
    void nextHandler(){
        uint32_t newHandler = currentHandler;
 
        switch(currentHandler)
        {
            case HSTART:
                 if (isHeader() && ISSET(matched.hh)) {newHandler=HHEADER; break;}
            case HHEADER:   
                 if (isPayload() && ISSET(matched.ph)) {newHandler=HPAYLOAD; break;}
            case HPAYLOAD:
                 if (isTail() && ISSET(matched.ch)) {newHandler=HCOMPLETION; break;}
                 break;
        }


        //printf("currenthandler: %u; newHandler: %u\n", currentHandler, newHandler);
        /* if no new state has been reached then there are no handlers to execute */
        if (currentHandler==newHandler) currentHandler=NOHANDLER;
        else currentHandler=newHandler;
        //printf("currenthandler: %u\n", currentHandler);


    }
    
    bool hasHandlers(){
        assert(currentHandler!=HSTART);
        return currentHandler!=NOHANDLER;
    }
    
    MatchedHostDataPkt(HostDataPkt& pkt, uint32_t _host, p4_ruqelem_t &_matched_entry): HostDataPkt(pkt){
        matched = _matched_entry;
        type = MATCHED_HOST_DATA_PKT;
        time = pkt.time;
        host = _host;
        currentHandler = HSTART;
        nextHandler();

    }


    void copyCurrentHandlerData(void * dest){
        goalevent elem = *((goalevent *) header->getPayload());
        void * state =  (void*)&(((uint32_t *)dest)[MEM_MESSAGE]); 
        if(matched.mem){
            memcpy(state, matched.shared_mem, matched.mem);
        }

        ptl_header_t * h = (ptl_header_t *) dest;
        ptl_payload_t* p = (ptl_payload_t *) dest;
        switch(currentHandler){

            case HHEADER:
            {
                h->type = elem.type;
                h->length = elem.size; 
                h->target_id =elem.target; 
                h->source_id = elem.host; 
                h->match_bits =elem.tag; 
                h->offset = 0;
                h->hdr_data = 0; 


                for(int i=0; i<4; i++){
                    h->user_hdr.arg[i] = elem.arg[i];
                    //printf("LGS: arg[%d]=%lu,%lu  \n",i,elem.arg[i],h->user_hdr.arg[i]);
                }

                break;
            }   
            case HPAYLOAD:
            {
                p->length = size; 
                p->offset = pktoffset; 
                break;
            }   
            case HCOMPLETION:
            {
                //printf("elem.target %u \n",elem.target);
                //printf("elem.host %u \n",elem.host);
                ((uint32_t*)dest)[0] = elem.host;
                break;
            }   
        }

    }

    void processHandlerReturn(void * syscall_data, void* dest){
        void * state =  (void*)&(((uint32_t *)dest)[MEM_MESSAGE]); 
        if(matched.mem)
        {
            //printf("Get back shared memory\n");
            memcpy(matched.shared_mem,state,matched.mem);
        }


        uint32_t return_value = ((uint32_t*)syscall_data)[0];
         
//        printf("return value of handler is %s \n", handler_ret_code[return_value]); 
  
        
        *matched.pending = 0;
        switch(return_value)
        {
            case SUCCESS:
                break;  
            case PROCEED: 
                currentHandler = HCOMPLETION;
                break;
            case PROCESS_DATA: 
                break;  
            case PROCESS_DATA_PENDING:
                *matched.pending = 1;  
                break;  
            case DROP: 
                currentHandler = HCOMPLETION;
                break; 
            case DROP_PENDING:
                *matched.pending = 1;
                currentHandler = HCOMPLETION;
                break;
            case SUCCESS_PENDING:
                *matched.pending = 1;
                break;


        }
    }

};



#endif /* __P4SMP_EVENTS_HPP__ */
