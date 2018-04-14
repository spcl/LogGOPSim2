#ifndef __GEM5_MOD_H__
#define __GEM5_MOD_H__

#include <vector>

#include "cpu/base.hh"
#include "cpu/o3/deriv.hh"
#include "mem/cache/dummy_cache.hh"
#include "sim/cxx_manager.hh"

#include "../../sim.hpp"
#include "../../simEvents.hpp"

#include "gem5Mod_events.hpp"

#define HANDLER_EXECUTED 0
#define NO_HPU_AVAIL -1
#define NO_CONTEXT_AVAIL -2
#define HANDLER_SUSPENDED -3

class gem5Mod : public simModule {

private:
  class myQueue : public EventQueue {
  public:
    myQueue(const char *str) : EventQueue(str) { ; }
  };

private:
  std::vector<DerivO3CPU *> cpus;
  std::vector<DummyCache *> dummycaches;

  CxxConfigManager *config_manager;

  uint32_t timemult;
  bool print;
  Simulator &sim;

  uint32_t hpus;
  uint32_t p;
  std::vector<std::priority_queue<hpu_t>> nexthpu;
  // std::vector<std::list<hpu_t>> busyhpu;

  std::vector<std::queue<gem5SimRequest *>> waiting_queue;

private:
  btime_t simcall(gem5SimRequest &event, uint32_t syscall_num, void *data,
            btime_t time, bool * suspend);

public:
  gem5Mod(Simulator &sim, uint32_t ranks);

  int executeHandler(gem5SimRequest &pkt);

  void printStatus();
  size_t maxTime();

  static int dispatch(simModule *mod, simEvent *ev);

  virtual int registerHandlers(Simulator &sim) {
    sim.addEventHandler(this, GEM5_SIM_REQUEST, gem5Mod::dispatch);
  }
};

#endif /* __GEM5_MOD_H__ */
