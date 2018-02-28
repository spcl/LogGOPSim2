#ifndef __P4_HPP__
#define __P4_HPP__

/*#include "../../sim.hpp"
#include "../../Noise.hpp"
#include "../../Parser.hpp"
#include "../../TimelineVisualization.hpp"
#include "../../cmdline.h"
#include "../../simEvents.hpp"
*/

#include "../../sim.hpp"
#include "../loggop/loggop.hpp"

#include "../packet_net/NetMod.hpp"

#include <queue>
#include <unordered_map>

// TODO: move to a function
#define TRIGGER_OPS(HOST, CT, TIME)                                            \
  {                                                                            \
    goalevent *triggered_op;                                                   \
    cttable.ct_increase(HOST, CT);                                             \
    while (cttable.getNextOperation(HOST, CT, &triggered_op)) {                \
      triggered_op->keepalive = false;                                         \
      triggered_op->time = std::max(                                           \
          triggered_op->time,                                                  \
          TIME + (btime_t)(triggered_op->type == OP_NCTWAIT ? 0 : DMA_L));     \
      sim.addEvent(triggered_op);                                              \
    }                                                                          \
  }

typedef struct ptl_list_entry {
  uint64_t size;
  uint32_t counter;
  bool use_once;
  uint32_t tag;
  uint32_t target;
  uint32_t src;
  char type;
  btime_t starttime;
  uint32_t offset;
  uint32_t hh, ph, ch;
  uint32_t mem;
  uint32_t matched_to;
  uint8_t *pending;
  void *shared_mem;
} p4_ruqelem_t;

typedef struct _unexpected_hdr {
  p4_ruqelem_t buffer;
  goalevent msg;
} uqhdr_entry_t;

/* this is a comparison functor that can be used to compare and sort
 * graph_node_properties by counter threshold */
class triggerd_compare_func {
public:
  bool operator()(goalevent *x, goalevent *y) {
    return x->threshold > y->threshold;
#ifdef STRICT_ORDER
// TODO: not implemented
#endif
  }
};

class ptl_counter {
public:
  uint64_t value;
  std::priority_queue<goalevent *, std::vector<goalevent *>,
                      triggerd_compare_func>
      triggered;

  ptl_counter() : value(0), triggered() { ; }
};

class ptl_counter_table {
private:
  std::vector<std::unordered_map<uint32_t, ptl_counter *>> table;

public:
  ptl_counter_table(int p) : table(p) { ; }

  ptl_counter_table() { ; }

  void resize(int p) { table.resize(p); }

  uint64_t ct_get_val(int host, int handle) {
    if (table[host][handle] == NULL)
      return 0;
    return table[host][handle]->value;
  }

  void ct_enqueue_op(int host, int handle, goalevent *t_op) {
    ptl_counter *&ct = table[host][handle];
    if (ct == NULL)
      ct = new ptl_counter();
    // printf("ct: new trigger op added\n");
    ct->triggered.push(t_op);
  }

  // return true if there is at least one operation to be triggered
  bool ct_increase(int host, int handle) {
    ptl_counter *&ct = table[host][handle];
    if (ct == NULL)
      ct = new ptl_counter();
    ct->value++;
    // printf("HOST: %i. Increasing counter %i. Value: %i; Tsize: %i\n", host,
    // handle, ct->value, ct->triggered.size());
    return !ct->triggered.empty() &&
           ct->triggered.top()->threshold == ct->value;
  }

  bool getNextOperation(int host, int handle, goalevent **t_op) {
    ptl_counter *ct = table[host][handle];
    if (ct == NULL)
      return false;
    // if (!ct->triggered.empty()) printf("ct: value: %i; next threshold: %i\n",
    // ct->value, ct->triggered.top().threshold);
    if (!ct->triggered.empty() && ct->value == ct->triggered.top()->threshold) {
      // printf("getNextOperation: op to trigger found\n");
      *t_op = (goalevent *)ct->triggered.top();
      ct->triggered.pop();
      return true;
    }
    return false;
  }

  ~ptl_counter_table() {
    // TODO: empty the table.
  }
};

class P4Mod : public LogGOPmod {
protected:
  typedef std::list<p4_ruqelem_t> ruq_t;
  typedef std::list<uqhdr_entry_t> uqhdr_t;
  uint32_t c, C;

  uint32_t DMA_L;
  ptl_counter_table cttable;
  std::vector<uqhdr_t>
      uq_hdr;                // contains info about unexpected messages matching
  std::vector<ruq_t> rq, uq; // receive queue, unexpected queue

public:
  P4Mod(Simulator &sim, Parser &parser, bool simplenet = true)
      : LogGOPmod(sim, parser, simplenet) {
    c = sim.args_info.LogGOPS_c_arg; // 100;
    C = sim.args_info.LogGOPS_C_arg; // 1;
    DMA_L = sim.args_info.DMA_L_arg;

    cttable.resize(p);
    rq.resize(p);
    uq.resize(p);
    uq_hdr.resize(p);
  }

  // bool p4_match(const goalevent &elem, P4SMPMod::ruq_t *q, p4_ruqelem_t *
  // retelem=NULL, bool remove=true, ruq_t::iterator * matchedit = NULL);
  bool p4_match(const goalevent &elem, P4Mod::ruq_t *q,
                ruq_t::iterator *matchedit, uint32_t header_id = 0);

  bool p4_match_unexpected(const goalevent &elem, uqhdr_t *q,
                           uqhdr_entry_t *retelem = NULL);

  /* these inline are going to be ignored if called from the extern */
  inline int append(goalevent &elem);
  inline int putget(goalevent &elem);
  inline int nicputget(goalevent &elem);
  inline int putgetmsg(goalevent &elem);
  inline int getreply(goalevent &elem);
  inline int tops(goalevent &elem);
  inline int nctwait(goalevent &elem);

  static int dispatch(simModule *mod, simEvent *ev);

  virtual void printStatus();

  virtual int registerHandlers(Simulator &sim) {
    LogGOPmod::registerHandlers(sim);
    const int handlerscnt = 14;
    int handlers[handlerscnt] = {OP_APPEND,   OP_NAPPEND, OP_PUT,     OP_GET,
                                 OP_NPUT,     OP_NGET,    OP_PUTMSG,  OP_GETMSG,
                                 OP_GETREPLY, OP_TPUT,    OP_TAPPEND, OP_CTWAIT,
                                 OP_TGET,     OP_NCTWAIT};
    for (int i = 0; i < handlerscnt; i++)
      sim.addEventHandler(this, handlers[i], P4Mod::dispatch);

    return 0;
  }
};

#endif /* __P4_HPP__ */
