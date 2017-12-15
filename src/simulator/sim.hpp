
#ifndef __SPCLSIM_HPP__
#define __SPCLSIM_HPP__

#include <assert.h>
#include <queue>
#include <stdint.h>
#include <unordered_map>

#include "simEvents.hpp"

#include "visualisation/visEvents.hpp"

#include "cmdline.h"

#define STATS

class Simulator;

class simModule {
public:
  virtual int registerHandlers(Simulator &sim) = 0;
  virtual void printStatus() = 0;
  virtual size_t maxTime() = 0;
  // virtual int dispatch(simEvent* ev)=0;
  virtual ~simModule() { ; }
};

// interface for visualisation modules
class visModule {
public:
  virtual int registerHandlers(Simulator &sim) = 0;
  virtual ~visModule() { ; }
};

/* this is a comparison functor that can be used to compare and sort
 * simEvent objects by time */
class aqcompare_func {
public:
  bool operator()(simEvent *x, simEvent *y) {
    if (x->time > y->time)
      return true;
    if (x->time == y->time && x->id > y->id)
      return true;
    return false;
  }
};

// NOTE: Bad that evqueue_t contains pointers but needed if we want to allow
// different event typess :(
typedef std::priority_queue<simEvent *, std::vector<simEvent *>, aqcompare_func>
    evqueue_t;
typedef int ekey_t;
typedef uint32_t sim_signal_t;
typedef uint32_t vis_signal_t;
typedef int (*efun_t)(simModule *, simEvent *);
typedef void *(*sfun_t)(simModule *, sim_signal_t, void *);

typedef std::unordered_map<ekey_t, std::pair<simModule *, efun_t>>
    events_dispatch_map_t;
typedef std::unordered_map<ekey_t, std::pair<simModule *, sfun_t>>
    signals_dispatch_map_t;

// for visualisation
typedef int (*visfun_t)(visModule *, visEvent *);
typedef std::unordered_map<vis_signal_t,
                           std::vector<std::pair<visModule *, visfun_t>>>
    visualisation_dispatch_map_t;

class Simulator : ISimulator {

private:
  evqueue_t aq;
  events_dispatch_map_t dmap;
  signals_dispatch_map_t dmap_signals;

  std::vector<simModule *> mods;

  std::vector<visModule *> vismods;
  visualisation_dispatch_map_t dmap_vis;

#ifdef STATS
  uint64_t reinserted = 0;
#endif

public:
  gengetopt_args_info args_info;
  uint32_t ranks;

  void addEventHandler(simModule *mod, ekey_t key, efun_t fun);
  void addSignalHandler(simModule *mod, sim_signal_t signal, sfun_t fun);
  void addVisualisationHandler(visModule *mod, vis_signal_t signal,
                               visfun_t fun);

  virtual void addEvent(simEvent *elem) {
    // printf("adding event: type; %i; id: %u; ptr: %p\n", elem->type, elem->id,
    // elem);
    this->aq.push(elem);
  }

  virtual void reinsertEvent(simEvent *elem) {
    // printf("reinserting event: type: %i; id: %u; ptr: %p\n", elem->type,
    // elem->id, elem);

    elem->keepalive = true;
#ifdef STATS
    reinserted++;
#endif
    this->aq.push(elem);
  }

  inline int simulate() {
    simEvent *elem = aq.top();
    // printf("[SIM] ev ptr: %p\n", elem);
    // printf("[SIM] simulating event: %i with id: %u (%p); time: %lu aq size:
    // %i\n", elem->type, elem->id, elem, elem->time, aq.size());
    assert(elem != NULL);

    aq.pop();

    if (dmap.find(elem->type) == dmap.end()) {
      printf("Error: no handler for %i %lu\n", elem->type, elem->id);
      return -1;
    }

    (*dmap[elem->type].second)(dmap[elem->type].first, elem);
    // dmap[elem->type]->dispatch(elem);

    if (!elem->keepalive)
      delete elem;

    return 0;
  }

  /* Not timed interface */
  inline void *signal(sim_signal_t signal, void *arg) {

    if (dmap_signals.find(signal) == dmap_signals.end()) {
      printf("Error: no handler for this event %i\n", signal);
      return NULL;
    }
    return (*dmap_signals[signal].second)(dmap_signals[signal].first, signal,
                                          arg);
  }

  // the main method for visualisation
  inline void visualize(vis_signal_t signal, visEvent *event) {

    auto it = dmap_vis.find(signal);

    if (it == dmap_vis.end()) {
      printf("Error: no visualisation for this event %i\n", signal);
      return;
    }
    for (auto &mod : it->second) { // access by reference to avoid copying
      (*mod.second)(mod.first, event);
    }

    delete event;
  }

  int simulate(IParser &parser);

  void addModule(simModule *mod);

  void addVisModule(visModule *mod);

  void printStatus();

  void maxTime();

  Simulator(int argc, char *argv[], uint32_t ranks) : ranks(ranks) {

    cmdline_parser(argc, argv, &args_info);
  }

  ~Simulator() {
    printf("Destructor is called \n");

    for (int i = 0; i < vismods.size(); i++) {
      delete vismods[i];
    }
    vismods.clear();

    for (int i = 0; i < mods.size(); i++) {
      delete mods[i];
    }
    mods.clear();
  }
};

#endif /* __SPCLSIM_HPP__ */
