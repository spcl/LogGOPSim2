#include "p4smp.hpp"

#ifdef HAVE_GEM5

#define SET_NIC(id) "NIC" + std::to_string(id)
#define SET_CPU(id) "CPU" + std::to_string(id)

int P4SMPMod::dispatch(simModule *mod, simEvent *_elem) {

  P4Mod::dispatch(mod, _elem);

  P4SMPMod *pmod = (P4SMPMod *)mod;

  switch (_elem->type) {
  case HOST_DATA_PKT:
    return pmod->recvpkt(*((HostDataPkt *)_elem));
  case MATCHED_HOST_DATA_PKT:
    return pmod->processHandlers(*((MatchedHostDataPkt *)_elem));
  }
  return -1;
}

int P4SMPMod::recvpkt(HostDataPkt &pkt) {

  /* need a copy here bc we are going to modify some fields and want the origina
  back if the pkt gets reinserted */
  goalevent elem = *((goalevent *)pkt.header->getPayload());

  uint32_t host = elem.target;

  /* this case has to be taken into accout now since new operations can be
   * generates by SMP handlers */
  if (host >= nextgr.size()) {
    printf("Error: Host %i does not exist (check goal file)\n", host);
    exit(-1);
  }

  /* wanted to avoid this but p4_match uses this fields :( */
  uint32_t tmp = elem.host;
  elem.host = elem.target;
  elem.target = tmp;

  // printf("elem.host: %u; elem.target: %u; elem.tag: %u; pkt.id: %u\n",
  // elem.host, elem.target, elem.tag, pkt.id);

  if (print)
    printf("[%i] receivepkt. id: %i; tag: %i; nextgr: %lu; elem.time: %lu; "
           "pkt.time: %lu; pkt.size: %u; type: %u\n",
           host, pkt.id, elem.tag, nextgr[host][elem.nic], elem.time, pkt.time,
           pkt.size, elem.type);

  /* we don't consider the gets for now */
  /*if (elem.type==OP_GETMSG){
      if (fullpacket) putgetmsg(elem);
      return 0;
  }*/

  if ((nextgr[host][elem.nic] <= pkt.time)) {

    ruq_t *q = NULL;
    /* Check if there is a matching ME */
    p4_ruqelem_t matched;
    ruq_t::iterator matchedit;

    bool nomatch = false;

    nextgr[host][elem.nic] = pkt.time + (pkt.size - 1) * G;
    if (pkt.isHeader())            // account for  matching
      nextgr[host][elem.nic] += c; // big matching
    else
      nextgr[host][elem.nic] += c_packet; // small matching

    if (pkt.isTail())
      nextgr[host][elem.nic] +=
          std::max(g, c + pkt.id * c_packet) - (c + pkt.id * c_packet);

    sim.visualize(VIS_HOST_FLOW,
                  new HostComplexFlowVisEvent(
                      "Name:add_transmission", elem.target, host, SET_NIC(0),
                      SET_NIC(0), pkt.starttime, pkt.time, pkt.size, G));
    //+tara sim.tlviz->add_transmission(elem.target, host, pkt.starttime,
    //pkt.time, pkt.size, G);
    sim.visualize(VIS_HOST_DUR,
                  new HostDurationVisEvent("Name:add_nicop", elem.host,
                                           SET_NIC(elem.nic), pkt.time,
                                           pkt.time + (pkt.size - 1) * G));
    //+tara sim.tlviz->add_nicop(elem.host, pkt.time, pkt.time+(pkt.size-1)*G,
    //elem.nic);
    sim.visualize(VIS_HOST_DUR,
                  new HostDurationVisEvent(
                      "Name:add_nicop", elem.host, SET_NIC(elem.nic),
                      pkt.time + (pkt.size - 1) * G, nextgr[host][elem.nic]));
    //+tara sim.tlviz->add_nicop(elem.host, pkt.time+(pkt.size-1)*G,
    //nextgr[host][elem.nic] , elem.nic);

    if (elem.type == OP_GETREPLY) {

      if (print)
        printf("-- GET reply pkt\n");
      if (print && pkt.isTail())
        printf("-- Last GET reply received\n");

      btime_t triggertime = pkt.time + (pkt.size - 1) * G;
      if (pkt.isHeader()) // account for  matching
        triggertime += c; // big matching
      else
        triggertime += c_packet; // small matching

      sim.visualize(VIS_HOST_DUR,
                    new HostDurationVisEvent("Name:add_nicop", elem.host,
                                             SET_NIC(elem.nic), triggertime,
                                             triggertime + DMA_L));
      //+tara sim.tlviz->add_nicop(elem.host, triggertime, triggertime+DMA_L,
      //elem.nic,0.3,0.3,0.3);
      TRIGGER_OPS(elem.host, elem.oct, triggertime + DMA_L)

      if (print)
        printf("-- Triggering new ops at time: %lu\n", triggertime + DMA_L);
      return 0;
    }

    /* remove the matched ME only if the full message has been received */
    if (p4_match(elem, &rq[host], &matchedit, pkt.header->id)) {
      if (print)
        printf("-- ME found in PL \n");
      q = &rq[host];
      matched = *matchedit;
      // DOUBLE CHECK: it seems that we have do to nothing here.
    } else if (p4_match(elem, &uq[host], &matchedit, pkt.header->id)) {
      uqhdr_entry_t t;
      matched = *matchedit;
      t.buffer = matched;
      t.msg = elem;
      uq_hdr[host].push_back(t);
      q = &uq[host];
      if (print)
        printf("-- ME found in OL\n");
    } else {
      if (print) {
        printf("-- NO MATCHING! \n");
      }
      nomatch = true;
    } // drop message

    if (!nomatch) {

      pkt.time += (pkt.size - 1) * G; // account for receiving
      if (pkt.isHeader())             // account for  matching
        pkt.time += c;                // big matching
      else
        pkt.time += c_packet; // small matching

      if (print)
        printf("-- Start processing handlers @%lu\n", pkt.time);

      // executehandlers(new MatchedHostDataPkt(pkt,matched,ExecuteHandlers))
      processHandlers(*(new MatchedHostDataPkt(pkt, host, matched)));
    }

  } else {
    /* reinsert packet */
    if (print)
      printf("-- no time: reinserting pkt. pkt.time: %lu; nextgr[%i]: %lu;\n",
             pkt.time, host, nextgr[host][elem.nic]);
    uint64_t gwait = nextgr[host][elem.nic];
    pkt.time = gwait;
    sim.reinsertEvent(&pkt);
  }
  return 0;
}

int P4SMPMod::processHandlers(MatchedHostDataPkt &pkt) {

  btime_t current = pkt.time; // time when execution starts
  btime_t htot = 0;
  int res = 0;

  goalevent elem = *((goalevent *)pkt.header->getPayload());
  int host = elem.target;

  while (pkt.hasHandlers()) {
    // printf("time: %lu\n", current);
    res = gem5->executeHandler(pkt, current);

    /* the execution has to succeeed if this is a "resumed" handler */
    assert(!pkt.isCurrentHandlerSuspended() ||
           (pkt.isCurrentHandlerSuspended() && res != NO_CONTEXT_AVAIL &&
            res != NO_HPU_AVAIL));

    if (res == NO_CONTEXT_AVAIL) {
      if (print)
        printf("Event in waiting queue. time: %lu; host[%i]\n", current, host);
      return 0;
    }

    if (res == NO_HPU_AVAIL) {
      if (print)
        printf("--no hpu available; time: %lu; nexthpu[%i]: %lu;\n", current,
               host, htot);
      pkt.time = current; // reinsert event
      sim.reinsertEvent(&pkt);
      return 0;
    }

    if (res == HANDLER_SUSPENDED) {
      return 0;
    }
    pkt.nextHandler();
  }
  pkt.header->toreceive--;
  bool fullpacket = pkt.header->toreceive == 0;

  if (fullpacket) {
    // printf("trigger ops %lu, %lu, %lu \n", pkt.matched.src,
    // pkt.matched.counter, current);
    if (elem.type == OP_GETMSG) {
      goalevent *elemptr = (goalevent *)pkt.header->getPayload();
      uint32_t tmp = elemptr->host;
      elemptr->host = elemptr->target;
      elemptr->target = tmp;
      elemptr->type = OP_GETREPLY;
      elemptr->starttime = current;
      sim.addEvent(new NetMsgEvent(elemptr, current));
    }

    sim.visualize(VIS_HOST_DUR,
                  new HostDurationVisEvent("Name:add_nicop", elem.host,
                                           SET_NIC(elem.nic), current,
                                           current + DMA_L));
    //+tara sim.tlviz->add_nicop(host, current, current+DMA_L, elem.nic,
    //0.3,0.3,0.3);
    current += DMA_L;
    if (print)
      printf("-- delivering full packet; trying to trigger new ops at: %lu\n",
             current);

    TRIGGER_OPS(pkt.matched.src, pkt.matched.counter, current);

    if (pkt.matched.use_once) {
      if (*pkt.matched.pending)
        reset_matching_entry(host, pkt.header->id);
      else
        erase_matching_entry(host, pkt.header->id);
    }
  }

  return 0;
}

void P4SMPMod::erase_matching_entry(uint32_t host, uint32_t header_id) {

  ruq_t *q = &rq[host];
  for (ruq_t::iterator iter = q->begin(); iter != q->end(); ++iter) {
    if (header_id == iter->matched_to) {
      if (print)
        printf("Erase ME from rq[%u]\n", host);
      if (iter->mem)
        free(iter->shared_mem);
      free(iter->pending);
      q->erase(iter);
      return;
    }
  }
  q = &uq[host];
  for (ruq_t::iterator iter = q->begin(); iter != q->end(); ++iter) {
    if (header_id == iter->matched_to) {
      if (print)
        printf("Erase ME from uq[%u]\n", host);
      if (iter->mem)
        free(iter->shared_mem);
      free(iter->pending);
      q->erase(iter);
      return;
    }
  }
}

void P4SMPMod::reset_matching_entry(uint32_t host, uint32_t header_id) {

  ruq_t *q = &rq[host];
  for (ruq_t::iterator iter = q->begin(); iter != q->end(); ++iter) {
    if (header_id == iter->matched_to) {
      iter->matched_to = 0;
      *iter->pending = 0;
      if (print)
        printf("Reset ME from rq[%u]\n", host);

      return;
    }
  }
  q = &uq[host];
  for (ruq_t::iterator iter = q->begin(); iter != q->end(); ++iter) {
    if (header_id == iter->matched_to) {
      if (print)
        printf("Reset ME from uq[%u]\n", host);
      iter->matched_to = 0;
      *iter->pending = 0;
      return;
    }
  }
}

void P4SMPMod::printStatus() {
  LogGOPmod::printStatus();
  gem5->printStatus();
}

size_t P4SMPMod::maxTime() {
  size_t a = LogGOPmod::maxTime();
  size_t b = gem5->maxTime();
  return a > b ? a : b;
}

#endif
