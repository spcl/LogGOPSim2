#ifndef __SIMEVENTS_HPP__
#define __SIMEVENTS_HPP__

typedef uint64_t btime_t;

class simEvent{
public:
    btime_t time;
    uint32_t type;
    bool keepalive;
    uint64_t id;
    static uint64_t gid;
    simEvent * payload;

       
    simEvent(simEvent * _payload=NULL){ 
        keepalive=false; 
        id=gid++;
        payload = _payload;
        //printf("simEvent: new: id: %u;\n", id);
    }

    bool hasPayload(){ return payload!=NULL; }
    simEvent * getPayload(){ return payload; }

    /*simEvent(const simEvent& ev){
        time = ev.time;
        type = ev.type;
        keepalive = ev.keepalive;
        id = gid++;
    }*/
};


/* used to resolve a duplicate definition of the cmdline.h btw the sim and txt2bin (differnt content, same names)*/
class ISimulator{
public:
    virtual void addEvent(simEvent * ev)=0;
};

class IParser{
public:
    virtual void addRootNodes(ISimulator * sim)=0;
    virtual void addNewNodes(ISimulator * sim)=0;
    virtual uint32_t getHostCount()=0;

};


class LogGPBaseEvent: public simEvent{
public:
    uint32_t host;
    uint32_t target;
    uint16_t proc;
    uint16_t nic;
    uint32_t size;
    uint32_t tag;
    uint32_t offset;

    LogGPBaseEvent(LogGPBaseEvent g, int type, btime_t time): simEvent(){
        host = g.host;
        target = g.target;
        proc = g.proc;
        nic = g.nic;
        size = g.size;
        tag = g.tag;
        offset = g.offset;
        this->time = time;
        this->type = type;
    }

    LogGPBaseEvent(): simEvent(){;}

    //LogGPBaseEvent(const LogGPBaseEvent& ev): simEvent(ev) {;}


};





class goalevent : public LogGPBaseEvent {

public:
    btime_t starttime;         // only used for MSGs to identify start times
#ifdef HOSTSYNC
    btime_t syncstart;
#endif
#ifdef STRICT_ORDER
    btime_t ts; /* this is a timestamp that determines the (original) insertion order of
                  elements in the queue, it is increased for every new element, not for
                  re-insertions! Needed for correctness. */
#endif
    //uint64_t size;						// number of bytes to send, recv, or time to spend in loclop
    //uint32_t target;					// partner for send/recv
    //uint32_t host;            // owning host
    //uint32_t offset;          // for Parser (to identify schedule element)
    //uint32_t tag;							// tag for send/recv
    uint32_t handle;          // handle for network layer :-/
    //uint8_t proc;							// processing element for this operation
    //uint8_t nic;							// network interface for this operation
    uint32_t oct;
    uint32_t ct;
    uint64_t threshold;
    char options;
    uint32_t hh, ph, ch;
    uint32_t mem;

    uint64_t arg[4];

    bool isParsed=true;    

    goalevent(uint32_t host, uint32_t target, uint32_t size, uint32_t tag, uint32_t type, btime_t time): LogGPBaseEvent()
    {

        this->host=host;
        this->target = target;
        this->size = size;
        this->tag = tag;
        this->type = type;
        this->oct = (uint32_t) -1;
        this->ct = (uint32_t) -1;
        this->time = time;

        this->nic=0;
        this->proc=0;
        isParsed=false;
    }

    goalevent(uint32_t host, uint32_t target, uint32_t size, uint32_t tag, uint32_t type, btime_t time, uint64_t arg1): LogGPBaseEvent()
    {

        this->host=host;
        this->target = target;
        this->size = size;
        this->tag = tag;
        this->type = type;
        this->oct = (uint32_t) -1;
        this->ct = (uint32_t) -1;
        this->time = time;
        this->arg[0] = arg1;
        this->nic=0;
        this->proc=0;
        isParsed=false;
    }

    goalevent(): LogGPBaseEvent(){;}

    
    //goalevent(const goalevent& ev): LogGPBaseEvent(ev) {;}
};



// mnemonic defines for op type
static const int OP_SEND = 1;
static const int OP_RECV = 2;
static const int OP_LOCOP = 3;
static const int OP_MSG = 4;
static const int OP_APPEND = 5;
static const int OP_PUT = 6;
static const int OP_GET = 7;
static const int OP_TAPPEND = 8;
static const int OP_TPUT = 9;
static const int OP_TGET = 10;
static const int OP_CTWAIT = 11;
static const int OP_NPUT = 12;
static const int OP_PUTMSG = 13;
static const int OP_GETMSG = 14;
static const int OP_NAPPEND = 15;
static const int OP_NCTWAIT = 16;
static const int OP_NGET = 17;
static const int OP_GETREPLY = 18;

static const int NET_MSG = 19;
static const int ACK_MSG = 20;

static const int NET_PKT = 21;
static const int HOST_DATA_PKT = 22;

static const int OP_LOCGEM5OP = 23;

static const int MATCHED_HOST_DATA_PKT = 24;
static const int DMA_WRITE = 25;
static const int DMA_READ = 26;
static const int DMA_WAIT = 27;

static const int GEM5_SIM_REQUEST = 28;


static const int SIG_SEND_COMPLETE = 29;
static const int SIG_PKT_RECEIVED = 30;

static const int OP_MSG_RTR = 31;
static const int OP_MSG_RTS = 32;

static const uint32_t ANY_SOURCE = ~0;
static const uint32_t ANY_TAG = ~0;


/* this is a comparison functor that can be used to compare and sort
 * operation types of graph_node_properties */
class gnp_op_comp_func {
  public:
  bool operator()(simEvent * x, simEvent * y) {
    if(x->type < y->type) return true;
    return false;
  }
};


#endif /* __SIMEVENTS_HPP__ */
