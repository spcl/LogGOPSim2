#include <inttypes.h>

#include <unordered_map>
#include <queue>

#ifndef GRAPH_NODE_PROPERTIES
#define GRAPH_NODE_PROPERTIES 1

//#define P4EXT // Portals 4 extension
#define OPT_USE_ONCE 1
#define OPT_OVERFLOW_LIST 2
#define OPT_PRIORITY_LIST 4

#define GET_MSG_SIZE 5



typedef uint64_t btime_t;

/* this class is CRITICAL -- keep it as SMALL as possible!
 *
 * current size: 39 bytes
 *
 */
class graph_node_properties {
	public:
    btime_t time;
    btime_t starttime;         // only used for MSGs to identify start times
#ifdef HOSTSYNC
    btime_t syncstart;
#endif
#ifdef STRICT_ORDER
    btime_t ts; /* this is a timestamp that determines the (original) insertion order of
                  elements in the queue, it is increased for every new element, not for
                  re-insertions! Needed for correctness. */
#endif
    uint64_t size;						// number of bytes to send, recv, or time to spend in loclop
    uint32_t target;					// partner for send/recv
    uint32_t host;            // owning host
    uint32_t offset;          // for Parser (to identify schedule element)
    uint32_t tag;							// tag for send/recv
    uint32_t handle;          // handle for network layer :-/
    uint8_t proc;							// processing element for this operation
    uint8_t nic;							// network interface for this operation
    char type;							  // see below
    uint32_t oct;
    uint32_t ct;
    uint64_t threshold;
    char options;
};

/* this is a comparison functor that can be used to compare and sort
 * operation types of graph_node_properties */
class gnp_op_comp_func {
  public:
  bool operator()(graph_node_properties x, graph_node_properties y) {
    if(x.type < y.type) return true;
    return false;
  }
};

/* this is a comparison functor that can be used to compare and sort
 * graph_node_properties by time */
class aqcompare_func {
  public:
  bool operator()(graph_node_properties x, graph_node_properties y) {
    if(x.time > y.time) return true;
#ifdef STRICT_ORDER
    if(x.time == y.time && x.ts > y.ts) return true;
#endif
    return false;
  }
};




#ifdef P4EXT

/* this is a comparison functor that can be used to compare and sort
 * graph_node_properties by counter threshold */
class triggerd_compare_func {
  public:
  bool operator()(graph_node_properties x, graph_node_properties y) {
    return x.threshold > y.threshold;
#ifdef STRICT_ORDER
    //TODO: not implemented
#endif
  }
};


class ptl_counter{
    public:
    uint64_t value;
    std::priority_queue<graph_node_properties, std::vector<graph_node_properties>, triggerd_compare_func> triggered;

    ptl_counter() : value(0), triggered() {;}
};

class ptl_counter_table{
    private:
    std::vector< std::unordered_map<uint32_t, ptl_counter *> > table;

    public:
    ptl_counter_table(int p) : table(p){;}

    uint64_t ct_get_val(int host, int handle){
        if (table[host][handle] == NULL) return 0;
        return table[host][handle]->value;
    }

    void ct_enqueue_op(int host, int handle, graph_node_properties t_op){
        ptl_counter * &ct = table[host][handle];
        if (ct==NULL) ct = new ptl_counter();
        //printf("ct: new trigger op added\n");
        ct->triggered.push(t_op);
    }

    //return true if there is at least one operation to be triggered
    bool ct_increase(int host, int handle){
        ptl_counter * &ct = table[host][handle];
        if (ct==NULL) ct = new ptl_counter();
        ct->value++;
        //printf("HOST: %i. Increasing counter %i. Value: %i; Tsize: %i\n", host, handle, ct->value, ct->triggered.size());
        return !ct->triggered.empty() && ct->triggered.top().threshold==ct->value;
    }

    bool getNextOperation(int host, int handle, graph_node_properties &t_op){
        ptl_counter * ct = table[host][handle];
        if (ct==NULL) return false;
       // if (!ct->triggered.empty()) printf("ct: value: %i; next threshold: %i\n", ct->value, ct->triggered.top().threshold);
        if (!ct->triggered.empty() && ct->value == ct->triggered.top().threshold){
            //printf("getNextOperation: op to trigger found\n");
            t_op = ct->triggered.top(); ct->triggered.pop();
            return true;
        }
        return false;
    }

    ~ptl_counter_table(){
        //TODO: empty the table.
    }
};

#endif // P4EXT


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




static const uint32_t ANY_SOURCE = ~0;
static const uint32_t ANY_TAG = ~0;


#endif
