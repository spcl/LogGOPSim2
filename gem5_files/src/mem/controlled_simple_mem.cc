/*
 * Copyright (c) 2010-2013, 2015 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ron Dreslinski
 *          Ali Saidi
 *          Andreas Hansson
 */

#include "base/random.hh"
#include "mem/controlled_simple_mem.hh"
#include "debug/Drain.hh"

using namespace std;

/* LGS2 functions */


void ControlledSimpleMemory::intercept(uint32_t key, bool monitor_reads, uint32_t multiplicity){
    tointercept[key] = std::make_pair(monitor_reads, multiplicity);
}

bool ControlledSimpleMemory::allIntercepted(){
    return tointercept.empty();
}


void ControlledSimpleMemory::resetMonitor(uint32_t key, uint32_t idx){
    //printf("resetting monitor of %u %lu\n", key, intercepted[key]);
    monitored[intercepted[key][idx]] = 0;
}

uint32_t ControlledSimpleMemory::monitor(uint32_t key, uint32_t idx){
    //printf("checking monitor of %u %lu:\n", key, intercepted[key].size());
    if (idx>=intercepted[key].size()) return 0;
    return monitored[intercepted[key][idx]];
}


int ControlledSimpleMemory::getVal(uint32_t key, uint32_t idx, void * buff, size_t size){
    auto addr = intercepted.find(key);
    if (addr == intercepted.end()) return -1;
    uint8_t * ptr =  pmemAddr + addr->second[idx] - range.start(); 
    memcpy(buff, ptr, size);
    return 0;
} 

void * ControlledSimpleMemory::getPtr(uint32_t key, uint32_t idx){
    auto addr = intercepted.find(key);
    if (addr == intercepted.end()) return NULL;
    return (void *) (pmemAddr + addr->second[idx] - range.start());
}

int ControlledSimpleMemory::setVal(uint32_t key, uint32_t idx, void * buff, size_t size){
    auto addr = intercepted.find(key);
    if (addr == intercepted.end()) return -1;
    uint8_t * ptr =  pmemAddr + addr->second[idx] - range.start(); 
    memcpy(ptr, buff, size);
    return 0;
}   

void ControlledSimpleMemory::handle_recv(PacketPtr pkt){

    //printf("[ControlledSimpleMem] handling recv\n");
    // we want the virtual address so it's equal for all the cpus in a system.
    uint64_t addr = pkt->getAddr();

    
   /* if (pkt->isWrite()){
        uint32_t * ptr = pkt->getPtr<uint32_t>();
        printf("[MEM] sees write: %u; addr: %lu; value: %u\n", *ptr, pkt->getAddr(), *ptr);
    }*/
    
    if (pkt->isWrite() && !allIntercepted()){
        //printf("pkt: %s\n", pkt->print().c_str());
        uint32_t * ptr = pkt->getPtr<uint32_t>();
        //if (pkt->getSize()<32) printf("Error here!!!\n");

        //printf("[MEM] sees write: %u; addr: %lu; value: %u\n", *ptr, pkt->getAddr(), *ptr);
        if (ptr==NULL || !pkt->hasData() || pkt->getSize()<4) return;

        //if (pkt->hadBadAddress()) printf("BAD ADDRESS!!!\n");
        auto v = tointercept.find(*ptr);
        if (v != tointercept.end()){
            //printf("[MEM] intercepting: %u; addr: %lu; size: %u; hpu: %lu\n", *ptr, pkt->getAddr(), pkt->getSize(), intercepted[*ptr].size());
            std::pair<bool, uint32_t> ti = v->second;

            intercepted[*ptr].push_back(addr);

            if (ti.first) monitored[addr] = 0;           

            if (intercepted[*ptr].size() > ti.second){
                printf("[MEM] Error: too many intercepts for this key!!! (%u)\n", *ptr);
            }else if (intercepted[*ptr].size() == ti.second) {
                tointercept.erase(*ptr);
            }
        }
    }

    if (pkt->isRead() && monitored.find(addr)!=monitored.end()){
        monitored[addr]++;
        //printf("[MEM] monitoring %lu read: %i\n", pkt->getAddr(), monitored[pkt->getAddr()]);

    }
    //printf("[ControlledSimpleMem] recv OK\n");

}


/*  end */



ControlledSimpleMemory::ControlledSimpleMemory(const ControlledSimpleMemoryParams* p) :
    AbstractMemory(p),
    port(name() + ".port", *this), latency(p->latency),
    latency_var(p->latency_var), bandwidth(p->bandwidth), isBusy(false),
    retryReq(false), retryResp(false),
    releaseEvent(this), dequeueEvent(this)
{
}

void
ControlledSimpleMemory::init()
{
    AbstractMemory::init();

    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
}

Tick
ControlledSimpleMemory::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");
    //handle_recv(pkt);
    access(pkt);
    return getLatency();
}

void
ControlledSimpleMemory::recvFunctional(PacketPtr pkt)
{
    //handle_recv(pkt);
    pkt->pushLabel(name());
    
    functionalAccess(pkt);

    bool done = false;
    auto p = packetQueue.begin();
    // potentially update the packets in our packet queue as well
    while (!done && p != packetQueue.end()) {
        done = pkt->checkFunctional(p->pkt);
        ++p;
    }

    pkt->popLabel();
}

bool
ControlledSimpleMemory::recvTimingReq(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller, "
             "saw %s to %#llx\n", pkt->cmdString(), pkt->getAddr());

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    // if we are busy with a read or write, remember that we have to
    // retry
    if (isBusy) {
        retryReq = true;
        return false;
    }

    // technically the packet only reaches us after the header delay,
    // and since this is a memory controller we also need to
    // deserialise the payload before performing any write operation
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    // update the release time according to the bandwidth limit, and
    // do so with respect to the time it takes to finish this request
    // rather than long term as it is the short term data rate that is
    // limited for any real memory

    // calculate an appropriate tick to release to not exceed
    // the bandwidth limit
    Tick duration = pkt->getSize() * bandwidth;

    // only consider ourselves busy if there is any need to wait
    // to avoid extra events being scheduled for (infinitely) fast
    // memories
    if (duration != 0) {
        schedule(releaseEvent, curTick() + duration);
        isBusy = true;
    }

    handle_recv(pkt);

    // go ahead and deal with the packet and put the response in the
    // queue if there is one
    bool needsResponse = pkt->needsResponse();
    recvAtomic(pkt);

    // turn packet around to go back to requester if response expected
    if (needsResponse) {
        // recvAtomic() should already have turned packet into
        // atomic response
        assert(pkt->isResponse());

        Tick when_to_send = curTick() + receive_delay + getLatency();

        // typically this should be added at the end, so start the
        // insertion sort with the last element, also make sure not to
        // re-order in front of some existing packet with the same
        // address, the latter is important as this memory effectively
        // hands out exclusive copies (shared is not asserted)
        auto i = packetQueue.end();
        --i;
        while (i != packetQueue.begin() && when_to_send < i->tick &&
               i->pkt->getAddr() != pkt->getAddr())
            --i;

        // emplace inserts the element before the position pointed to by
        // the iterator, so advance it one step
        packetQueue.emplace(++i, pkt, when_to_send);

        if (!retryResp && !dequeueEvent.scheduled())
            schedule(dequeueEvent, packetQueue.back().tick);
    } else {
        pendingDelete.reset(pkt);
    }



    return true;
}

void
ControlledSimpleMemory::release()
{
    assert(isBusy);
    isBusy = false;
    if (retryReq) {
        retryReq = false;
        port.sendRetryReq();
    }
}

void
ControlledSimpleMemory::dequeue()
{
    assert(!packetQueue.empty());
    DeferredPacket deferred_pkt = packetQueue.front();

    retryResp = !port.sendTimingResp(deferred_pkt.pkt);

    if (!retryResp) {
        packetQueue.pop_front();

        // if the queue is not empty, schedule the next dequeue event,
        // otherwise signal that we are drained if we were asked to do so
        if (!packetQueue.empty()) {
            // if there were packets that got in-between then we
            // already have an event scheduled, so use re-schedule
            reschedule(dequeueEvent,
                       std::max(packetQueue.front().tick, curTick()), true);
        } else if (drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of ControlledSimpleMemory complete\n");
            signalDrainDone();
        }
    }
}

Tick
ControlledSimpleMemory::getLatency() const
{
    return latency +
        (latency_var ? random_mt.random<Tick>(0, latency_var) : 0);
}

void
ControlledSimpleMemory::recvRespRetry()
{
    assert(retryResp);

    dequeue();
}

BaseSlavePort &
ControlledSimpleMemory::getSlavePort(const std::string &if_name, PortID idx)
{

    //printf("[ControlledSimpleMemory] getSlavePort: if_name: |%s|; if_name!=port: %i; port: %p\n", if_name.c_str(), if_name!="port", port);

    if (if_name != "port") {
        return MemObject::getSlavePort(if_name, idx);
    } else {
        return port;
    }
}


/*void ControlledSimpleMemory::setMemID(uint32_t _id){
    memid = _id;
}*/

DrainState
ControlledSimpleMemory::drain()
{
    if (!packetQueue.empty()) {
        DPRINTF(Drain, "ControlledSimpleMemory Queue has requests, waiting to drain\n");
        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

ControlledSimpleMemory::MemoryPort::MemoryPort(const std::string& _name,
                                     ControlledSimpleMemory& _memory)
    : SlavePort(_name, &_memory), memory(_memory)
{ }

AddrRangeList
ControlledSimpleMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}

Tick
ControlledSimpleMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return memory.recvAtomic(pkt);
}

void
ControlledSimpleMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    memory.recvFunctional(pkt);
}

bool
ControlledSimpleMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return memory.recvTimingReq(pkt);
}

void
ControlledSimpleMemory::MemoryPort::recvRespRetry()
{
    memory.recvRespRetry();
}

ControlledSimpleMemory*
ControlledSimpleMemoryParams::create()
{
    return new ControlledSimpleMemory(this);
}
