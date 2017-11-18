/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@cs.indiana.edu>
 *            Timo Schneider <timoschn@cs.indiana.edu>
 *            Salvatore Di Girolamo <salvatore.digirolamo@inf.ethz.ch> (P4 extension)
 *
 */


#define STRICT_ORDER // this is needed to keep order between Send/Recv and LocalOps in NBC case :-/
#define LIST_MATCH // enables debugging the queues (check if empty)
#define HOSTSYNC // this is experimental to count synchronization times induced by message transmissions

#define P4EXT


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <algorithm>

#ifndef LIST_MATCH
#ifdef __GNUC__
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#endif

#include <queue>
#include "cmdline.h"

#include <sys/time.h>

#include "LogGOPSim.hpp"
#include "Network.hpp"
#include "TimelineVisualization.hpp"
#include "Noise.hpp"
#include "Parser.hpp"
#include "cmdline.h"


//TODO: move to a function
#define TRIGGER_OPS(HOST, CT, TIME)  \
    {graph_node_properties triggered_op; \
    cttable.ct_increase(HOST, CT); \
    while (cttable.getNextOperation(HOST, CT, triggered_op)) {\
        triggered_op.time = std::max(triggered_op.time, TIME);\
        aq.push(triggered_op);\
    }}



#ifndef LIST_MATCH
namespace std
{
   using namespace __gnu_cxx;
}
#endif

static bool print=true;

gengetopt_args_info args_info;

// TODO: should go to .hpp file

#ifndef P4EXT
typedef struct {
  // TODO: src and tag can go in case of hashmap matching
  btime_t starttime; // only for visualization
  uint32_t size, src, tag, offset;
} ruqelem_t;
#else
typedef struct ptl_list_entry{
    uint64_t size;
    uint32_t counter;
    bool use_once;
    uint32_t tag;
    uint32_t target;
    uint32_t src;
    char type;
    btime_t starttime;
    uint32_t offset;
} ruqelem_t;

typedef struct _unexpected_hdr{
    ruqelem_t buffer;
    graph_node_properties msg;
} uqhdr_entry_t;

typedef std::list<uqhdr_entry_t> uqhdr_t;
#endif // P4EXT

typedef unsigned int uint;
typedef unsigned long int ulint;
typedef unsigned long long int ullint;

#ifdef LIST_MATCH
// TODO this is really slow - reconsider design of rq and uq!
// matches and removes element from list if found, otherwise returns
// false
typedef std::list<ruqelem_t> ruq_t;


#ifdef P4EXT


static inline bool p4_match(const graph_node_properties &elem, ruq_t *q, ruqelem_t *retelem=NULL) {

  //std::cout << "UQ size " << q->size() << "\n";

  //printf("++ [%i] searching matching queue for src %i tag %i\n", elem.host, elem.target, elem.tag);
  for(ruq_t::iterator iter=q->begin(); iter!=q->end(); ++iter) {

    //printf("elem.host %i; iter->target: %i\n", elem.host, iter->target);
    if(iter->target == ANY_SOURCE || iter->target==elem.target) {

      if(iter->tag == ANY_TAG || iter->tag  == elem.tag) {
        //printf("match: use_once: %i\n", iter->use_once);
        if(retelem) *retelem=*iter;

        if (iter->use_once) q->erase(iter);


        return true;
      }
    }
  }
  return false;
}


static inline bool p4_match_unexpected(const graph_node_properties &elem, uqhdr_t *q, uqhdr_entry_t *retelem=NULL) {

  //std::cout << "UQ size " << q->size() << "\n";

  if(print) printf("++ [%i] searching matching unexpected header queue for src %i tag %i; uq size: %i\n", elem.host, elem.target, elem.tag, q->size());
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
//#else
#endif // P4EXT


static inline bool match(const graph_node_properties &elem, ruq_t *q, ruqelem_t *retelem=NULL) {

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
//#endif // P4EXT

#else
class myhash { // I WANT LAMBDAS! :)
  public:
  size_t operator()(const std::pair<int,int>& x) const {
    return (x.first>>16)+x.second;
  }
};
typedef std::hash_map< std::pair</*tag*/int,int/*src*/>, std::queue<ruqelem_t>, myhash > ruq_t;
static inline bool match(const graph_node_properties &elem, ruq_t *q, ruqelem_t *retelem=NULL) {

  if(print) printf("++ [%i] searching matching queue for src %i tag %i\n", elem.host, elem.target, elem.tag);

  ruq_t::iterator iter = q->find(std::make_pair(elem.tag, elem.target));
  if(iter == q->end()) {
    return false;
  }
  std::queue<ruqelem_t> *tq=&iter->second;
  if(tq->empty()) {
    return false;
  }
  if(retelem) *retelem=tq->front();
  tq->pop();
  return true;
}
#endif


int main(int argc, char **argv) {

#ifdef STRICT_ORDER
  btime_t aqtime=0;
#endif

	if (cmdline_parser(argc, argv, &args_info) != 0) {
    fprintf(stderr, "Couldn't parse command line arguments!\n");
    throw(10);
	}

  // read input parameters
  const int o=args_info.LogGOPS_o_arg;
  const int O=args_info.LogGOPS_O_arg;
  const int g=args_info.LogGOPS_g_arg;
  const int L=args_info.LogGOPS_L_arg;
  const int G=args_info.LogGOPS_G_arg;
  //TODO: read the following two from input params

  print=args_info.verbose_given;
  const uint32_t S=args_info.LogGOPS_S_arg;



  Parser parser(args_info.filename_arg, args_info.save_mem_given);
  Network net(&args_info);

  const uint p = parser.schedules.size();
  const int ncpus = parser.GetNumCPU();
  const int nnics = parser.GetNumNIC();

#ifdef P4EXT
  const int c=args_info.LogGOPS_c_arg;//100;
  const int C=args_info.LogGOPS_C_arg;//1;
  ptl_counter_table cttable(p);
#endif

  printf("size: %i (%i CPUs, %i NICs); L=%i, o=%i g=%i, G=%i, O=%i, P=%i, S=%u", p, ncpus, nnics, L, o, g, G, O, p, S);

  #ifdef P4EXT
  printf(", c: %i, C: %i", c, C);
  #endif

  printf("\n");

  TimelineVisualization tlviz(&args_info, p);
  Noise osnoise(&args_info, p);

  //tlviz.set_to_draw(1);
  // the active queue
  std::priority_queue<graph_node_properties,std::vector<graph_node_properties>,aqcompare_func> aq;
  // the queues for each host

  //IN PT4EXT uq is mapped on the Overflow list
  std::vector<ruq_t> rq(p), uq(p); // receive queue, unexpected queue
  #ifdef P4EXT
  std::vector<uqhdr_t> uq_hdr(p); //contains info about unexpected messages matching
  #endif // P4EXT

  // next available time for o, g(receive) and g(send)
  std::vector<std::vector<btime_t> > nexto(p), nextgr(p), nextgs(p), nextc(p);
#ifdef HOSTSYNC
  std::vector<btime_t> hostsync(p);
#endif

  // initialize o and g for all PEs and hosts
  for(uint i=0; i<p; ++i) {
    nexto[i].resize(ncpus);
    std::fill(nexto[i].begin(), nexto[i].end(), 0);
    nextgr[i].resize(nnics);
    std::fill(nextgr[i].begin(), nextgr[i].end(), 0);
    nextgs[i].resize(nnics);
    std::fill(nextgs[i].begin(), nextgs[i].end(), 0);
    nextc[i].resize(nnics);
    std::fill(nextc[i].begin(), nextc[i].end(), 0);
  }

	struct timeval tstart, tend;
	gettimeofday(&tstart, NULL);

  int host=0;
  uint64_t num_events=0;

  for(Parser::schedules_t::iterator sched=parser.schedules.begin(); sched!=parser.schedules.end(); ++sched, ++host) {
    // initialize free operations (only once per run!)
    //sched->init_free_operations();
    //char tmp[] = "temp";
    //sched->write_as_dot(tmp);

    // retrieve all free operations
    //typedef std::vector<std::string> freeops_t;
    //freeops_t free_ops = sched->get_free_operations();
    SerializedGraph::nodelist_t free_ops;
    sched->GetExecutableNodes(&free_ops);
    // ensure that the free ops are ordered by type
    std::sort(free_ops.begin(), free_ops.end(), gnp_op_comp_func());

    num_events += sched->GetNumNodes();

    // walk all new free operations and throw them in the queue
    for(SerializedGraph::nodelist_t::iterator freeop=free_ops.begin(); freeop != free_ops.end(); ++freeop) {
      //if(print) std::cout << *freeop << " " ;

      freeop->host = host;
      freeop->time = 0;
#ifdef STRICT_ORDER
      freeop->ts=aqtime++;
#endif

      switch(freeop->type) {
        case OP_LOCOP:
          if(print) printf("init %i (%i,%i) loclop: %lu\n", host, freeop->proc, freeop->nic, (long unsigned int) freeop->size);
          break;
        case OP_SEND:
          if(print) printf("init %i (%i,%i) send to: %i, tag: %i, size: %lu\n", host, freeop->proc, freeop->nic, freeop->target, freeop->tag, (long unsigned int) freeop->size);
          break;
        case OP_RECV:
          if(print) printf("init %i (%i,%i) recvs from: %i, tag: %i, size: %lu\n", host, freeop->proc, freeop->nic, freeop->target, freeop->tag, (long unsigned int) freeop->size);
          break;
        default: ;
          //printf("not implemented!\n");
      }

      aq.push(*freeop);
      //std::cout << "AQ size after push: " << aq.size() << "\n";
    }
  }

  bool new_events=true;
  uint lastperc=0;


  #ifdef P4EXT
  //In order to send data we have to read it from main memory (C: dma speed).
  int srate = std::max(G, C);
  uint64_t msg_size;
  #endif // P4EXT

  while(!aq.empty() || new_events) {

    graph_node_properties elem = aq.top();
    aq.pop();
    //std::cout << "AQ size after pop: " << aq.size() << "\n";

    // the lists of hosts that actually finished someting -- a host is
    // added whenever an irequires or requires is satisfied
    std::vector<int> check_hosts;

    /*if(elem.host == 0) print = 1;
    else print = 0;*/

    // the BIG switch on element type that we just found
    switch(elem.type) {
      case OP_LOCOP: {
        if(print) printf("[%i] found loclop of length %lu - t: %lu (CPU: %i)\n", elem.host, (ulint)elem.size, (ulint)elem.time, elem.proc);
        if(nexto[elem.host][elem.proc] <= elem.time) {
          // check if OS Noise occurred
          btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+elem.size);
          nexto[elem.host][elem.proc] = elem.time+elem.size+noise;
          // satisfy irequires
          //parser.schedules[elem.host].issue_node(elem.node);
          // satisfy requires+irequires
          //parser.schedules[elem.host].remove_node(elem.node);
          parser.schedules[elem.host].MarkNodeAsStarted(elem.offset);
          parser.schedules[elem.host].MarkNodeAsDone(elem.offset);
          check_hosts.push_back(elem.host);
          // add to timeline
          tlviz.add_loclop(elem.host, elem.time, elem.time+elem.size, elem.proc, 0.6, 0.2, 0.4);
        } else {
          if(print) printf("[%i] -- locop local o not available -- reinserting\n", elem.host);
          elem.time=nexto[elem.host][elem.proc];
          aq.push(elem);
        } } break;


      case OP_SEND: { // a send op

        if(print) printf("[%i] found send to %i (size: %lu)- t: %lu (CPU: %i) - nexto: %lu - nextgs: %lu\n", elem.host, elem.target, elem.size, (ulint)elem.time, elem.proc, nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic]);

        if(std::max(nexto[elem.host][elem.proc],nextgs[elem.host][elem.nic]) <= elem.time) { // local o,g available!
          if(print) printf("-- satisfy local irequires\n");
          parser.schedules[elem.host].MarkNodeAsStarted(elem.offset);
          check_hosts.push_back(elem.host);

          // check if OS Noise occurred
          btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
          nexto[elem.host][elem.proc] = elem.time + o + (elem.size-1)*O+ noise;
          nextgs[elem.host][elem.nic] = elem.time + g + (elem.size-1)*G; // TODO: G should be charged in network layer only
          if (print) printf("-- updated nexto: %lu - nextgs: %lu (g: %i; G: %i; (s-1)G: %lu)\n", nexto[elem.host][elem.proc], nextgs[elem.host][elem.nic], g, G, (elem.size-1)*G);
          tlviz.add_osend(elem.host, elem.time, elem.time+o+ (elem.size-1)*O, elem.proc, (int) elem.tag);
          tlviz.add_noise(elem.host, elem.time+o+ (elem.size-1)*O, elem.time + o + (elem.size-1)*O+ noise, elem.proc);

          // insert packet into network layer
          net.insert(elem.time, elem.host, elem.target, elem.size, &elem.handle);

          elem.type = OP_MSG;
          int h = elem.host;
          elem.host = elem.target;
          elem.target = h;
          elem.starttime = elem.time;
          elem.time = elem.time+o+L; // arrival time of first byte only (G will be charged at receiver ... +(elem.size-1)*G; // arrival time
#ifdef STRICT_ORDER
          num_events++; // MSG is a new event
          elem.ts = aqtime++; // new element in queue
#endif
          if(print) printf("-- [%i] send inserting msg to %i, t: %lu\n", h, elem.host, (ulint)elem.time);
          aq.push(elem);

          if(elem.size <= S) { // eager message
            if(print) printf("-- [%i] eager -- satisfy local requires at t: %lu\n", h, (long unsigned int) elem.starttime+o);
            // satisfy local requires (host and target swapped!)
            parser.schedules[h].MarkNodeAsDone(elem.offset);
            //tlviz.add_transmission(elem.target, elem.host, elem.starttime+o, elem.time-o, elem.size, G);
          } else {
            if(print) printf("-- [%i] start rendezvous message to: %i (offset: %i)\n", h, elem.host, elem.offset);
          }

        } else { // local o,g unavailable - retry later
          if(print) printf("-- send local o,g not available -- reinserting\n");
          elem.time = std::max(nexto[elem.host][elem.proc],nextgs[elem.host][elem.nic]);
          aq.push(elem);
        }

        } break;
      case OP_RECV: {
        if(print) printf("[%i] found recv from %i - t: %lu (CPU: %i)\n", elem.host, elem.target, (ulint)elem.time, elem.proc);

        parser.schedules[elem.host].MarkNodeAsStarted(elem.offset);
        check_hosts.push_back(elem.host);
        if(print) printf("-- satisfy local irequires\n");

        ruqelem_t matched_elem;
        if(match(elem, &uq[elem.host], &matched_elem))  { // found it in local UQ
          if(print) printf("-- found in local UQ\n");
          if(elem.size > S) { // rendezvous - free remote request
            // satisfy remote requires
            parser.schedules[matched_elem.src].MarkNodeAsDone(matched_elem.offset); // this must be the offset of the remote packet!
            check_hosts.push_back(matched_elem.src);

            // TODO: uh, this is ugly ... we actually need to set the
            // time of the remote freed elements to elem.time now :-/
            // however, there is no simple way to do this :-(
            // but since we just got the elem with elem.time from the
            // queue, we can safely set the remote clocks to elem.time
            // (there is no event < elem.time in the queue) this
            // manipulation is dangerous, think before you change
            // anything!
            // TODO: do we need to add some latency for the ACK to
            // travel??? -- this can *not* be added here safely!
            if(nexto[matched_elem.src][elem.proc] < elem.time) nexto[matched_elem.src][elem.proc] = elem.time;
            if(nextgs[matched_elem.src][elem.nic] < elem.time) nextgs[matched_elem.src][elem.nic] = elem.time;

            if(print) printf("-- satisfy remote requires at host %i (offset: %i)\n", elem.target, elem.offset);
            tlviz.add_transmission(elem.host, matched_elem.src, matched_elem.starttime+o, elem.time, elem.size, G);
          }
          //tlviz.add_transmission(matched_elem.src, elem.host, matched_elem.starttime+o, elem.time, elem.size, G, 0.5, 0.5, 0.5);


          //TODO: to double check. When an unexpected msg is found, the o must be drawed after the message is completely received.
          //In the original version (commented) the o is drawed in middle of the reception if the receive was posted during this last.
          //tlviz.add_orecv(elem.host, elem.time, elem.time+o, elem.proc, 1, 0, 0);
          btime_t drawtime = elem.time; //std::max(elem.time, matched_elem.starttime);

          tlviz.add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);

          //tlviz.add_orecv(elem.host, drawtime, drawtime+o, elem.proc, (int) elem.tag);
          tlviz.add_transmission(elem.target, elem.host, matched_elem.starttime+o, elem.time, elem.size, G, 1, 1, 0);
          btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
          nexto[elem.host][elem.proc] = elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G) /* message is only received after G is charged !! TODO: consuming o seems a bit odd in the LogGP model but well in practice */;
          nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G; //IS COMPLETELY INCORRECT HERE

          //End TODO


          // satisfy local requires
          parser.schedules[elem.host].MarkNodeAsDone(elem.offset);
          check_hosts.push_back(elem.host);
          if(print) printf("-- satisfy local requires\n");
        } else { // not found in local UQ - add to RQ
          if(print) printf("-- not found in local UQ -- add to RQ\n");
          ruqelem_t nelem;


          nelem.size = elem.size;
          nelem.src = elem.target;
          nelem.tag = elem.tag;
          nelem.offset = elem.offset;
#ifdef LIST_MATCH
          rq[elem.host].push_back(nelem);
#else
          rq[elem.host][std::make_pair(nelem.tag,nelem.src)].push(nelem);
#endif
        }
      } break;

      case OP_MSG: {
        if(print) printf("[%i] found msg from %i, t: %lu (CPU: %i)\n", elem.host, elem.target, (ulint)elem.time, elem.proc);
        uint64_t earliestfinish;
		    if((earliestfinish = net.query(elem.starttime, elem.time, elem.target, elem.host, elem.size, &elem.handle)) <= elem.time /* msg made it through network */ &&
           std::max(nexto[elem.host][elem.proc],nextgr[elem.host][elem.nic]) <= elem.time /* local o,g available! */) {
          if(print) printf("-- msg o,g available (nexto: %lu, nextgr: %lu)\n", (long unsigned int) nexto[elem.host][elem.proc], (long unsigned int) nextgr[elem.host][elem.nic]);




          //Salvo: charge the o only when the receive is (already) posted (OCH)
          // check if OS Noise occurred
          //btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
          //nexto[elem.host][elem.proc] = elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G) /* message is only received after G is charged !! TODO: consuming o seems a bit odd in the LogGP model but well in practice */;
          //nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G;

          ruqelem_t matched_elem;
          if(match(elem, &rq[elem.host], &matched_elem)) { // found it in RQ
            if(print) printf("-- found in RQ\n");


            //(OCH)
            //btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
            //printf("matched_elem.starttime: %i; elem.time: %i (diff: %i) \n", matched_elem.starttime, elem.time, elem.time - matched_elem.starttime);
            btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
            nexto[elem.host][elem.proc] = elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G) /* message is only received after G is charged !! TODO: consuming o seems a bit odd in the LogGP model but well in practice */;
            nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G;


            if(elem.size > S) { // rendezvous - free remote request
              // satisfy remote requires
              parser.schedules[elem.target].MarkNodeAsDone(elem.offset);
              check_hosts.push_back(elem.target);
              // TODO: uh, this is ugly ... we actually need to set the
              // time of the remote freed elements to elem.time now :-/
              // however, there is no simple way to do this :-(
              // but since we just got the elem with elem.time from the
              // queue, we can safely set the remote clocks to elem.time
              // (there is no event < elem.time in the queue) this
              // manipulation is dangerous, think before you change
              // anything!
              // TODO: do we need to add some latency for the ACK to
              // travel??? -- this can *not* be added here safely!
              if(nexto[elem.target][elem.proc] < elem.time) nexto[elem.target][elem.proc] = elem.time;
              if(nextgs[elem.target][elem.nic] < elem.time) nextgs[elem.target][elem.nic] = elem.time;

              if(print) printf("-- satisfy remote requires on host %i\n", elem.target);
            }
            tlviz.add_transmission(elem.target, elem.host, elem.starttime+o, elem.time, elem.size, G);
            tlviz.add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, (int) elem.tag);
            tlviz.add_noise(elem.host, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.time+o+noise+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc);
            // satisfy local requires
            parser.schedules[elem.host].MarkNodeAsDone(matched_elem.offset);
            check_hosts.push_back(elem.host);


          } else { // not in RQ

            //(OCH)
            //tlviz.add_transmission(elem.target, elem.host, elem.starttime+o, elem.time, elem.size, G, 0.8, 0.8, 0);
            //tlviz.add_orecv(elem.host, elem.time+(elem.size-1)*G-(elem.size-1)*O, elem.time+o+std::max((elem.size-1)*O,(elem.size-1)*G), elem.proc, 1, 0, 0);
            if(print) printf("-- not found in RQ - add to UQ\n");
            ruqelem_t nelem;
            nelem.size = elem.size;
            nelem.src = elem.target;
            nelem.tag = elem.tag;
            nelem.offset = elem.offset;
            //nelem.starttime = elem.time; // when it was started
            nelem.starttime = elem.starttime;
            //TODO: double check. This is used as the time at which the receving is finished (exluding o). It is used for
            //the drawing of the o when the receive is posted.
            //nelem.starttime = elem.time+noise+std::max((elem.size-1)*O,(elem.size-1)*G);


#ifdef LIST_MATCH
            uq[elem.host].push_back(nelem);
#else
            uq[elem.host][std::make_pair(nelem.tag,nelem.src)].push(nelem);
#endif
          }
        } else {
          elem.time=std::max(std::max(nexto[elem.host][elem.proc],nextgr[elem.host][elem.nic]), earliestfinish);
          if(print) printf("-- msg o,g not available -- reinserting\n");
          aq.push(elem);
        }

      } break;
#ifdef P4EXT
      case OP_APPEND:
      case OP_NAPPEND:
        if (print) printf("[%i] append found. tag: %i\n", elem.host, elem.tag);
        if((elem.type==OP_APPEND && nexto[elem.host][elem.proc] <= elem.time) ||
        (elem.type==OP_NAPPEND && nextgs[elem.host][elem.nic] <= elem.time)) {
            if (elem.type!=OP_NAPPEND){
                btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
                nexto[elem.host][elem.proc] = elem.time + o + noise;
                tlviz.add_loclop(elem.host, elem.time, elem.time+o, elem.proc);
            }else{
                //NAPPEND: is the triggered append operations (executed on the NIC)
                //The cost of this operations is charged on nextgs because while
                //the nextgr is something that depends on external events (incoming messages),
                //the nextgs depends on the operations performed on the NIC by the owner host.
                //TODO: double check. Verify if it is the case to model it in a different way.
                //The cost c is necessary to scan the unexpected list in order to find
                //unexpected messages.
                nextgs[elem.host][elem.nic] = elem.time + c;
                tlviz.add_nicop(elem.host, elem.time, elem.time+c, elem.nic);
            }
            parser.schedules[elem.host].MarkNodeAsStarted(elem.offset); //useless?
            check_hosts.push_back(elem.host);
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
                            tlviz.add_loclop(elem.host, elem.time+o, elem.time+o+(matched_elem.msg.size)*C, elem.proc, 0, 0, 0);
                        }

                    }else{
                        printf("-- unexpected message request\n");
                    }


                    TRIGGER_OPS(elem.host, elem.oct, elem.time+o)
                    i++;
                }
            }
            //if the ME is for the OVERFLOW_LIST OR is for the PRIORITY LIST but no matching in UL are found OR is for the
            //priority list, matching in UL are found AND the ME is persistent:
            if ((elem.options & OPT_OVERFLOW_LIST) == OPT_OVERFLOW_LIST || i==0 || (elem.options & OPT_USE_ONCE) != OPT_USE_ONCE){
                ruqelem_t nelem;
                nelem.counter = elem.oct;
                nelem.size = elem.size;
                nelem.tag = elem.tag;
                nelem.target = elem.target;
                nelem.src = elem.host;
                nelem.use_once = ((elem.options & OPT_USE_ONCE) == OPT_USE_ONCE);
                nelem.type = 0; //Useless here. It is used (DOUBLE CHECK)
                if ((elem.options & OPT_OVERFLOW_LIST) == OPT_OVERFLOW_LIST) uq[elem.host].push_back(nelem);
                else rq[elem.host].push_back(nelem);
            }

            parser.schedules[elem.host].MarkNodeAsDone(elem.offset);


        }else{
            if (print) printf("-- no time. Reinserting.\n");
            elem.time=nexto[elem.host][elem.proc];
            aq.push(elem);
        }

        break;
      case OP_PUT:
      case OP_GET:
        if (nexto[elem.host][elem.proc] <= elem.time) {
            btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
            nexto[elem.host][elem.proc] = elem.time + o + noise;

            tlviz.add_loclop(elem.host, elem.time, elem.time+o, elem.proc);

            elem.time = elem.time + o;
            if (elem.type==OP_PUT) elem.type = OP_NPUT;
            else elem.type = OP_NGET;
            parser.schedules[elem.host].MarkNodeAsDone(elem.offset);
            check_hosts.push_back(elem.host);
        }else{
            elem.time=nexto[elem.host][elem.proc];
        }
        aq.push(elem);
        break;
      case OP_NPUT: //it is executed on the nic
      case OP_NGET:
        if (nextgs[elem.host][elem.nic] <= elem.time) {
            //printf("[%i] nput/nget op elem.time: %lu\n", elem.host, elem.time);

            //Hack: if elem.tag=-1 then is a ct_inc
            if (elem.tag==-1){
                //printf("ctinc!\n");
                TRIGGER_OPS(elem.host, elem.oct, elem.time)
                break;
            }
            //end hack


            /* if elem.type==OP_GET, the field "size" contains the size of the data to be read, not
            the size of the message to be sent, which will be only a get request. */
            msg_size = (elem.type==OP_NPUT) ? elem.size : GET_MSG_SIZE;

            //update time:
            //printf("PUT: time: %i\n", elem.time + g +(msg_size-1)*srate);
            nextgs[elem.host][elem.nic] = elem.time + g +(msg_size-1)*srate;

            //Insert into the network layer (in order to simulate congestion)
            net.insert(elem.time, elem.host, elem.target, msg_size, &elem.handle);
            //printf("nput op STARTED\n");
            tlviz.add_nicop(elem.host, elem.time, elem.time+(msg_size-1)*srate, elem.nic);
            tlviz.add_nicop(elem.host, elem.time+(msg_size-1)*srate, elem.time+(msg_size-1)*srate+g, elem.nic);


            //With the GET the operations are triggered on the event PTL_MD_EVENT_REPLY (when we receive the reply)
            if (elem.type!=OP_NGET) TRIGGER_OPS(elem.host, elem.oct, elem.time)

            elem.type = (elem.type==OP_NPUT) ? OP_PUTMSG : OP_GETMSG;
            uint32_t source = elem.host;
            elem.host = elem.target;
            elem.target = source;
            elem.starttime = elem.time;
            elem.time = elem.time+L; //arrival time of the first byte
            aq.push(elem);


            //Triggering operations
            //graph_node_properties triggered_op;
            //while (cttable.getNextOperation(elem.host, elem.oct, triggered_op)) aq.push(triggered_op);


        }else{
            elem.time=nextgs[elem.host][elem.nic];
            aq.push(elem);
        }


        break;
      case OP_PUTMSG:
      case OP_GETMSG:
        if (print) printf("[%i] put/get msg (received). tag: %i; nextgr: %lu; elem.time: %lu\n", elem.host, elem.tag, nextgr[elem.host][elem.nic], elem.time);
        uint64_t earliestfinish;

        msg_size = (elem.type==OP_PUTMSG) ? elem.size : (uint64_t) GET_MSG_SIZE;

        //###########CHECK BETTER THIS CONDITION!!!
        /*if((earliestfinish = net.query(elem.starttime, elem.time, elem.target, elem.host, elem.size, &elem.handle)) <= elem.time &&
            std::max(std::max(nextgr[elem.host][elem.nic],nextc[elem.host][elem.nic]), earliestfinish) <= elem.time) &&
            (elem.type!=OP_GETMSG || std::max(std::max(nextgs[elem.host][elem.nic],nextc[elem.host][elem.nic]), earliestfinish) <= elem.time)){*/
        if((earliestfinish = net.query(elem.starttime, elem.time, elem.target, elem.host, msg_size, &elem.handle)) <= elem.time &&
            (nextgr[elem.host][elem.nic] <= elem.time) &&
            (elem.type!=OP_GETMSG || nextgs[elem.host][elem.nic] <= elem.time)){

            nextgr[elem.host][elem.nic] = elem.time+g+(elem.size-1)*G+c; //TODO: Double check here for "c"


            ruqelem_t matched;
            bool nomatch=false;
            if (p4_match(elem, &rq[elem.host], &matched)){
                if (print) printf("-- ME found in PL\n");
                //DOUBLE CHECK: it seems that we have do to nothing here.
            }else if(p4_match(elem, &uq[elem.host], &matched)){
                uqhdr_entry_t t;
                t.buffer = matched;
                t.msg = elem;
                uq_hdr[elem.host].push_back(t);
                if (print) printf("-- ME found in OL\n");
            }else { if (print) {printf("-- NO MATCHING!\n");} nomatch=true; } //drop message



            tlviz.add_nicop(elem.host, elem.time, elem.time+(msg_size-1)*srate, elem.nic, 0.5, 0.5, 0.5);
            tlviz.add_nicop(elem.host, elem.time+(msg_size-1)*srate, elem.time+(msg_size-1)*srate+c, elem.nic);

            if (print) printf("-- msgsize: %lu from %i to %i\n", msg_size, elem.target, elem.host);
            tlviz.add_transmission(elem.target, elem.host, elem.starttime, elem.time, msg_size, srate);

            if (!nomatch) {


                btime_t triggertime;
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
                    tlviz.add_nicop(elem.host, elem.time, elem.time+(elem.size-1)*srate, elem.nic);

                    //Insert into the network layer (in order to simulate congestion)
                    net.insert(elem.time, elem.host, elem.target, elem.size, &elem.handle);

                    //Add the new operation to the queue
                    elem.type=OP_GETREPLY;
                    uint32_t h = elem.host;
                    elem.host = elem.target;
                    elem.target = h;
                    elem.starttime = elem.time;
                    elem.time = elem.time + L;
                    aq.push(elem);
                }


                if (print) printf("-- MSG: matched.src: %i; counter: %i; time: %lu\n", matched.src, matched.counter, triggertime);
                if (print) printf("-- Triggering new ops at time: %lu\n", triggertime);
                // To trigger an operation we don't have to wait g
                TRIGGER_OPS(matched.src, matched.counter, triggertime)

                //nextc[elem.host][elem.nic] = elem.time+c+(elem.size-1)*C; //message copy through DMA (assumed)
                //add trasmission from nic to cpu? (visualization) NO! the trasmission is btw the nic and the memory!!!!

            }




        }else{
            //####THIS IS WRONG!!
            //elem.time = std::max(std::max(nextgr[elem.host][elem.nic], nextc[elem.host][elem.nic]), earliestfinish);

            uint64_t gwait = (elem.type==OP_PUTMSG) ? nextgr[elem.host][elem.nic] : std::max(nextgr[elem.host][elem.nic], nextgs[elem.host][elem.nic]);
            elem.time = std::max(gwait, earliestfinish);
            aq.push(elem);
        }

        break;
      case OP_GETREPLY:

        if((earliestfinish = net.query(elem.starttime, elem.time, elem.target, elem.host, elem.size, &elem.handle)) <= elem.time &&
            nextgr[elem.host][elem.nic] <= elem.time){

            //As in the PUTMSG case, we don't have to charge the copy. ((elem.size-1)*C)
            nextgr[elem.host][elem.nic] = elem.time + g + (elem.size-1)*srate;// + (elem.size-1)*C;
            TRIGGER_OPS(elem.host, elem.oct, elem.time)

            tlviz.add_nicop(elem.host, elem.time, elem.time + (elem.size-1)*srate, elem.nic);
            //tlviz.add_nicop(elem.host, elem.time+(elem.size-1)*srate, elem.time + (elem.size-1)*srate + (elem.size-1)*C, elem.nic, 0, 0, 0);
            tlviz.add_transmission(elem.target, elem.host, elem.starttime, elem.time, elem.size, srate);

        }else{
            elem.time = std::max(nextgr[elem.host][elem.nic], earliestfinish);
            aq.push(elem);
        }
        break;
      case OP_TPUT:
      case OP_TAPPEND:
      case OP_CTWAIT:
      case OP_TGET:
        if (nexto[elem.host][elem.proc] <= elem.time) {
            nexto[elem.host][elem.proc] = elem.time + o;

            if (elem.type!=OP_CTWAIT){
                parser.schedules[elem.host].MarkNodeAsDone(elem.offset);
                check_hosts.push_back(elem.host);
            }

            if      (elem.type==OP_TPUT) elem.type = OP_NPUT;
            else if (elem.type==OP_TAPPEND) elem.type = OP_NAPPEND;
            else if (elem.type==OP_CTWAIT) elem.type = OP_NCTWAIT;
            else if (elem.type==OP_TGET) elem.type = OP_NGET;

            if (print) printf("[%i] triggered op setup: type: %i; ct: %i; threshold: %i; time: %lu\n", elem.host, elem.type, elem.ct, elem.threshold, elem.time);

            tlviz.add_loclop(elem.host, elem.time, elem.time+o, elem.proc, 0.5, 0.7, 1);
            btime_t noise = osnoise.get_noise(elem.host, elem.time, elem.time+o);
            elem.time = elem.time+o+noise;

            if (cttable.ct_get_val(elem.host, elem.ct)>=elem.threshold){
                //if (elem.type==OP_NCTWAIT && aq.size()>0) printf("CT WAIT: something will overlap with CTWAIT (size: %i)!\n", aq.size());
                aq.push(elem);
            }else cttable.ct_enqueue_op(elem.host, elem.ct, elem);


        }else{
            elem.time=nexto[elem.host][elem.proc];
            aq.push(elem);
        }
        break;
      case OP_NCTWAIT:
        nexto[elem.host][elem.proc] = elem.time;
        parser.schedules[elem.host].MarkNodeAsDone(elem.offset);
        check_hosts.push_back(elem.host);
        break;

#endif // P4EXT
      default:
        printf("not supported;\n"); //operation: type %i; peer %u; size %lu; tag %lu; options %i; oct: %u; ct: %u; threshold: %lu\n", (int) elem.type, elem.target, elem.size, elem.tag, elem.options, elem.oct, elem.ct, elem.threshold);
        break;
    }

    // do only ask hosts that actually completed something in this round!
    new_events=false;
    std::sort(check_hosts.begin(), check_hosts.end());
    check_hosts.erase(unique(check_hosts.begin(), check_hosts.end()), check_hosts.end());


    for(std::vector<int>::iterator iter=check_hosts.begin(); iter!=check_hosts.end(); ++iter) {
      host = *iter;
    //for(host = 0; host < p; host++)
      SerializedGraph *sched=&parser.schedules[host];

      // retrieve all free operations
      SerializedGraph::nodelist_t free_ops;
      sched->GetExecutableNodes(&free_ops);
      // ensure that the free ops are ordered by type
      std::sort(free_ops.begin(), free_ops.end(), gnp_op_comp_func());

      // walk all new free operations and throw them in the queue
      for(SerializedGraph::nodelist_t::iterator freeop=free_ops.begin(); freeop != free_ops.end(); ++freeop) {
        //if(print) std::cout << *freeop << " " ;

        new_events = true;

        // assign host that it starts on
        freeop->host = host;

#ifdef STRICT_ORDER
        freeop->ts=aqtime++;
#endif

        switch(freeop->type) {
          case OP_LOCOP:
            freeop->time = nexto[host][freeop->proc];
            if(print) printf("%i (%i,%i) loclop: %lu, time: %lu, offset: %i\n", host, freeop->proc, freeop->nic, (long unsigned int) freeop->size, (long unsigned int)freeop->time, freeop->offset);
            break;
          case OP_SEND:
            freeop->time = std::max(nexto[host][freeop->proc], nextgs[host][freeop->nic]);
            if(print) printf("%i (%i,%i) send to: %i, tag: %i, size: %lu, time: %lu, offset: %i\n", host, freeop->proc, freeop->nic, freeop->target, freeop->tag, (long unsigned int) freeop->size, (long unsigned int)freeop->time, freeop->offset);
            break;
          case OP_RECV:
            freeop->time = nexto[host][freeop->proc];
            if(print) printf("%i (%i,%i) recvs from: %i, tag: %i, size: %lu, time: %lu, offset: %i\n", host, freeop->proc, freeop->nic, freeop->target, freeop->tag, (long unsigned int) freeop->size, (long unsigned int)freeop->time, freeop->offset);
            break;
          case OP_PUT:
          case OP_TPUT:
          case OP_APPEND:
          case OP_NCTWAIT:
          case OP_CTWAIT:
          case OP_GET:
          case OP_NGET:
          case OP_TGET:
            freeop->time = nexto[host][freeop->proc];
            break;
          case OP_NPUT:
          case OP_NAPPEND:
            freeop->time = nextgs[host][freeop->nic];
            break;
          default:
            printf("not implemented!\n");
        }

        aq.push(*freeop);
      }
    }
    if(args_info.progress_given) {
      if(num_events/100*lastperc < aqtime) {
        printf("progress: %u %% (%llu) ", lastperc, (unsigned long long)aqtime);
        lastperc++;
        uint maxrq=0, maxuq=0;
        for(uint j=0; j<rq.size(); j++) maxrq=std::max(maxrq, (uint)rq[j].size());
        for(uint j=0; j<uq.size(); j++) maxuq=std::max(maxuq, (uint)uq[j].size());
        printf("[sizes: aq: %i max rq: %u max uq: %u]\n", (int)aq.size(), maxrq, maxuq);
      }
    }
  }

  gettimeofday(&tend, NULL);
	unsigned long int diff = tend.tv_sec - tstart.tv_sec;

#ifndef STRICT_ORDER
  ulint aqtime=0;
#endif
	printf("PERFORMANCE: Processes: %i \t Events: %lu \t Time: %lu s \t Speed: %.2f ev/s\n", p, (long unsigned int)aqtime, (long unsigned int)diff, (float)aqtime/(float)diff);

  // check if all queues are empty!!
  bool ok=true;
  for(uint i=0; i<p; ++i) {

#ifdef LIST_MATCH
    if(!uq[i].empty()) {
      printf("unexpected queue on host %i contains %lu elements!\n", i, (ulint)uq[i].size());
      for(ruq_t::iterator iter=uq[i].begin(); iter != uq[i].end(); ++iter) {
        printf(" src: %i, tag: %i\n", iter->src, iter->tag);
      }
      ok=false;
    }
    if(!rq[i].empty()) {
      printf("receive queue on host %i contains %lu elements!\n", i, (ulint)rq[i].size());
      for(ruq_t::iterator iter=rq[i].begin(); iter != rq[i].end(); ++iter) {
        printf(" src: %i, tag: %i\n", iter->src, iter->tag);
      }
      ok=false;
    }
#endif

  }

#ifdef P4EXT
//In portals4 could be the case in which some entries of the PL or OL remain attached after the termination
  ok=true;
#endif // P4EXT

  if (ok) {
    if(!args_info.batchmode_given) { // print all hosts
      printf("Times: \n");
      host = 0;
      for(uint i=0; i<p; ++i) {
        btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
        btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
        btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
        //std::cout << "Host " << i <<": "<< std::max(std::max(maxgr,maxgs),maxo) << "\n";
        //std::cout << "Host " << i <<": "<< maxo << "\n";
        std::cout << "Host " << i <<": o: " << maxo << "; gs: " << maxgs << "; gr: " << maxgr << "\n";
      }
    } else { // print only maximum
      long long unsigned int max=0;
      int host=0;
      for(uint i=0; i<p; ++i) { // find maximum end time
        btime_t maxo=*(std::max_element(nexto[i].begin(), nexto[i].end()));
        //btime_t maxgr=*(std::max_element(nextgr[i].begin(), nextgr[i].end()));
        //btime_t maxgs=*(std::max_element(nextgs[i].begin(), nextgs[i].end()));
        //btime_t cur = std::max(std::max(maxgr,maxgs),maxo);
        btime_t cur = maxo;
        if(cur > max) {
          host=i;
          max=cur;
        }
      }
      std::cout << "Maximum finishing time at host " << host << ": " << max << " ("<<(double)max/1e9<< " s)\n";
    }
  }
}
