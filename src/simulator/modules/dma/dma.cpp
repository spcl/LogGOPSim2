
#include "dma.hpp"

#include <algorithm>

#define SET_DMA "DMA"

void DMAmod::printStatus() {
  printf("DMA times:\n");

  for (unsigned int i = 0; i < p; ++i) {
    printf("Host %u; gs: %lu gr %lu \n", i, nextgs[i], nextgr[i]);
  }
}

size_t DMAmod::maxTime() {
  size_t max = 0;

  size_t temp = *std::max_element(nextgs.begin(), nextgs.end());
  if (temp > max)
    max = temp;
  temp = *std::max_element(nextgr.begin(), nextgr.end());
  if (temp > max)
    max = temp;

  return max;
}

int DMAmod::dispatch(simModule *mod, simEvent *_elem) {

  DMAmod *lmod = (DMAmod *)mod;

  switch (_elem->type) {

  case DMA_WAIT:
    return lmod->wait(*((DMAEvent *)_elem));
  case DMA_READ:
  case DMA_WRITE:
    return lmod->handleDMARequest(*((DMAEvent *)_elem));
  }

  return -1;
}

int DMAmod::handleDMARequest(DMAEvent &elem) {

  // printf("[%i] found send to %i (size: %u)- t: %lu (CPU: %i) - nexto: %lu -
  // nextgs: %lu\n", elem.host,  elem.size, elem.time, nextos[elem.host],
  // nextgs[elem.host]);

  if (nextgs[elem.host] <= elem.time) { // local g available!

    btime_t starttime = elem.time;
    if (print)
      printf("[%i] DMA servicing event @%lu\n", elem.host, elem.time);

    if (elem.type == DMA_WRITE) {
      if (contention)
        nextgs[elem.host] = elem.time + g + (elem.size - 1) * G;

      /* Just a placeholder to remember that we can account the write cost here
       */
      elem.time = elem.time; // + (elem.size-1)*G;

    } else { /* it's a DMA_READ */

      /* the data has to pay lantecy of the request + data transfer + latency of
       * response */
      if (contention)
        nextgs[elem.host] = elem.time + g;
      elem.time = elem.time + L + (elem.size - 1) * G + L;
    }

    if (print)
      printf("-- DMA completed @%lu\n", elem.time);

    sim.visualize(VIS_HOST_DUR, new HostDurationVisEvent(
                                    "Name:odma", elem.host, SET_DMA, starttime,
                                    std::max(nextgs[elem.host], elem.time)));
    //+tara sim.tlviz->add_odma(elem.host, starttime,
    //std::max(nextgs[elem.host],elem.time));
    // sim.tlviz->add_odma(elem.host, elem.time, elem.time + g);

    if (elem.isBlocking) { // blocking
      // elem.originatingEvent->time = elem.time;
      sim.addEvent(elem.extractOriginatingEvent());
    } else { // nonblocking
      uint32_t host = elem.host;
      auto it = std::find(wq[host].begin(), wq[host].end(), elem);
      if (it != wq[host].end()) {

        if (print)
          printf("-- wait already received\n");
        // there is a hpu which waits completion of this event
        // elem.originatingEvent->time = elem.time;
        sim.addEvent(elem.extractOriginatingEvent());
        wq[host].erase(it);
      } else {

        if (print)
          printf("-- still no wait, put elem in wait queue\n");

        // add it to unexpected queue. It means that read completed earlier than
        // blocking wait
        uq[host].push_back(elem);
      }
    }
  } else { // local o,g unavailable - retry later
    if (print)
      printf("-- send local g not available -- reinserting \n");
    elem.time = (nextgs[elem.host]);
    sim.reinsertEvent(&elem);
  }

  return 0;
}

int DMAmod::wait(DMAEvent &elem) {
  uint32_t host = elem.host;
  auto it = std::find(uq[host].begin(), uq[host].end(), elem);
  if (it != uq[host].end()) {
    // read completed earlier than blocking wait

    if (print)
      printf("[%i] DMA got wait (expected) @%lu\n", elem.host, elem.time);
    elem.time = (*it).time;
    if (print)
      printf("-- reinserting at @%lu\n", elem.time);
    sim.addEvent(elem.extractOriginatingEvent());
    uq[host].erase(it);
  } else {
    if (print)
      printf("[%i] DMA got wait (UNexpected) @%lu\n", elem.host, elem.time);

    // add it to waiting queue. It means that  blocking wait is earlier than
    // read completed
    wq[host].push_back(elem);
  }
  return 0;
}
