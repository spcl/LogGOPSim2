#include "mem/cache/dummy_cache.hh"


MemMonitor::MemMonitor(DummyCache * cache, uint32_t key, Addr addr, uint64_t size, bool monitor_reads, bool from_parent): 
                    key(key), addr(addr), size(size), monitor_reads(monitor_reads){
            assert(!from_parent || cache->parent!=NULL);
            assert(!from_parent || cache->parent->getPtr(key)!=NULL);
            if (!from_parent) memptr = malloc(size);
            else memptr = cache->parent->getPtr(key);
            intercepted=0;
            this->cache = cache;
        }

void DummyCache::setID(uint32_t system, uint32_t cpu){
    this->system = system;
    this->cpu = cpu;
}

void DummyCache::setParent(DummyCache * parent){
    this->parent = parent;
}

void DummyCache::intercept(uint32_t key, uint64_t length, bool monitor_reads, bool from_parent){
    tointercept[key] = std::make_tuple(length, monitor_reads, from_parent);
}


bool DummyCache::allIntercepted(){
    return tointercept.empty();
}


void DummyCache::resetMonitor(uint32_t key){
    //printf("resetting monitor of %u %lu\n", key, intercepted[key]);
    MemMonitor * mm = intercepted[key];
    mm->intercepted=0;
}

uint32_t DummyCache::monitor(uint32_t key){
    //printf("checking monitor of %u %lu:\n", key, intercepted[key].size());
    MemMonitor * mm = intercepted[key];
    if (mm==NULL) return 0;
    return mm->intercepted;
}

int DummyCache::getVal(uint32_t key, void * buff, size_t size){
    auto addr = intercepted.find(key);
    if (addr == intercepted.end()) return -1;
    //uint8_t * ptr =  pmemAddr + addr->second[idx] - range.start(); 
    MemMonitor * mm = addr->second;
    memcpy(buff, mm->memptr, std::min(size, mm->size));
    return 0;
} 

void * DummyCache::getPtr(uint32_t key){
    auto addr = intercepted.find(key);
    if (addr == intercepted.end()) return NULL;
    return addr->second->memptr;
}


int DummyCache::setVal(uint32_t key, void * buff, size_t size){
    auto addr = intercepted.find(key);
    //printf("[DummyCache] setVal: key: %u; addr: %lu\n", key, addr!=intercepted.end() ? addr->second->addr : 0);
    if (addr == intercepted.end()) return -1;
    MemMonitor * mm = addr->second;
    memcpy(mm->memptr, buff, std::min(size, mm->size));
    return 0;
}   



void DummyCache::handle_recv(PacketPtr pkt){

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

        /*if (ptr!=NULL && *ptr==5555444){
            printf("DummyCache(%u, %u): seeing DATA: size: %u; hasData: %i\n", this->system, this->cpu, pkt->getSize(), pkt->hasData());
        }*/

        //printf("[DummyCache] sees write: %u; addr: %lu; hasData: %i; size: %u\n", *ptr, pkt->getAddr(), pkt->hasData(), pkt->getSize());
        if (ptr==NULL /*|| !pkt->hasData()*/ || pkt->getSize()<4) {
            return;
        }
        //if (pkt->hadBadAddress()) printf("BAD ADDRESS!!!\n");
        auto v = tointercept.find(*ptr);
        if (v != tointercept.end()){
            //printf("[DummyCache(%u, %u)] intercepting: %u; addr: %lu; size: %u;\n", this->system, this->cpu, *ptr, pkt->getAddr(), pkt->getSize());
            std::tuple<uint64_t, bool, bool> ti = v->second;
            
            MemMonitor * mm;
            if (monitored.find(addr)!=monitored.end()){
                mm = monitored[addr];

                if (mm->key!=*ptr) { 
                    printf("Error: same address intercepted for different keys!\n");
                }else{
                    printf("Warning: the same address (%lu) has been already intercepted for this key (%u)!\n", addr, *ptr);
                }
            }else{ 
                mm = new MemMonitor(this, *ptr, addr, std::get<0>(ti), std::get<1>(ti), std::get<2>(ti));
            }

            intercepted[*ptr] = mm;

            // now we always need to monitor the reads, since we swap the memory by intercepting them
            //if (mm->monitor_reads) {
            for (int i=addr; i < addr + mm->size; i++){
                monitored[i] = mm;  
            }
            //}         

            tointercept.erase(*ptr);
            
        }
    }

    if (pkt->isRead() && monitored.find(addr)!=monitored.end()){
        monitored[addr]->intercepted++;
        //if (addr!=monitored[addr]->addr) { printf("monitored read beloning to %lu: %lu\n", addr, monitored[addr]->addr); }
        //printf("[DummyCache(%u, %u)] monitoring %lu read: %i\n", this->system, this->cpu, pkt->getAddr(), monitored[pkt->getAddr()]->key);
    }
    //printf("[ControlledSimpleMem] recv OK\n");

}


// called on the path from mem-> cache -> DummyCache -> CPU
void DummyCache::inject_lgs_mem(PacketPtr pkt){
    uint64_t addr = pkt->getAddr();
    uint8_t * ptr = pkt->getPtr<uint8_t>();
    auto mfind = monitored.find(addr);
    if (pkt->isRead() && mfind!=monitored.end()){
        MemMonitor * mm = mfind->second;
        uint32_t offset = addr - mm->addr;
        assert(mm->size - offset >= pkt->getSize());
        memcpy(ptr, (uint8_t *) mm->memptr + offset, pkt->getSize()); 
        //printf("[DummyCache(%i, %i)] Injecting memory (mem: %p): key: %u; offset: %u; pkt.size: %u; mm.size: %lu; val: %lu\n", this->system, this->cpu, mm->memptr, mm->key, offset, pkt->getSize(), mm->size, *((uint64_t *) ptr));

    }//else printf("[DummyCache] ignoring memory access: addr: %lu; isRead: %i\n", addr, pkt->isRead());
}


void DummyCache::catch_gem5_updates(PacketPtr pkt){
    uint64_t addr = pkt->getAddr();
    uint8_t * ptr = pkt->getPtr<uint8_t>();
    auto mfind = monitored.find(addr);
    if (pkt->isWrite() && mfind!=monitored.end()){
        MemMonitor * mm = mfind->second;
        uint32_t offset = addr - mm->addr;
        assert(mm->size - offset >= pkt->getSize());
        //printf("[DummyCache(%i, %i)] Catching update to %u (mem: %p); offset: %u; size: %lu; val: %lu\n", this->system, this->cpu, mm->key, mm->memptr, offset, mm->size, *((uint64_t *) ptr));
        memcpy((uint8_t *) mm->memptr + offset, ptr, pkt->getSize()); 
    }
}



DummyCache::DummyCache(DummyCacheParams *p)
    : MemObject(p), 
      cpuSidePort(p->name + ".cpu_side", this),
      memSidePort(p->name + ".mem_side", this)
{
    parent=NULL;
}


BaseMasterPort& DummyCache::getMasterPort(const std::string& if_name, PortID idx){
    if (if_name == "mem_side") {
        return memSidePort;
    } else {
        return MemObject::getMasterPort(if_name, idx);
    }
}

BaseSlavePort& DummyCache::getSlavePort(const std::string& if_name, PortID idx){
    if (if_name == "cpu_side") {
        return cpuSidePort;
    } else {
        return MemObject::getSlavePort(if_name, idx);
    }
}



/* FROM MEMORY */
void DummyCache::recvTimingResp(PacketPtr pkt){

    //printf("[DummyCache] recvTimingResp from memory; pkt: %lu (isWrite: %i; isRead: %i)\n", pkt->getAddr(), pkt->isWrite(), pkt->isRead());

    inject_lgs_mem(pkt);
    bool M5_VAR_USED success = cpuSidePort.sendTimingResp(pkt);

    assert(success);

}

void DummyCache::recvReqRetry(){
    assert(!wqueue.empty());

    while (!wqueue.empty()){
        PacketPtr waitingPkt = wqueue.front();

        //printf("[DummyCache] recvReqRetry from memory; pkt: %lu\n", waitingPkt->getAddr());
    
        bool M5_VAR_USED success = memSidePort.sendTimingReq(waitingPkt);       

        if (success) {
            wqueue.pop();  
            handle_recv(waitingPkt);
            catch_gem5_updates(waitingPkt);
        } 
        else return;
    }
}

void DummyCache::recvRangeChange(){
    cpuSidePort.sendRangeChange();
}


/* FROM CPU */
void DummyCache::recvFunctional(PacketPtr pkt){
    memSidePort.sendFunctional(pkt);
}

bool DummyCache::recvTimingReq(PacketPtr pkt){

    assert(pkt->isRequest());

    //printf("[DummyCache] recvTimingReq from CPU; pkt: %lu; queued: %lu\n", pkt->getAddr(), wqueue.size());


    if (!wqueue.empty()){
        wqueue.push(pkt);    
    }else{
        bool M5_VAR_USED success = memSidePort.sendTimingReq(pkt);       
        if (!success) { wqueue.push(pkt); } 
        else {
            handle_recv(pkt);
            catch_gem5_updates(pkt);
        }
        //printf("  result: %i\n", (int) success);
    }


    return true;
}

AddrRangeList DummyCache::getAddrRanges(){
    return memSidePort.getAddrRanges();
}


///////////////
//
// CpuSidePort
//
///////////////

AddrRangeList DummyCache::CpuSidePort::getAddrRanges() const{
    return cache->getAddrRanges();
}

bool DummyCache::CpuSidePort::recvTimingReq(PacketPtr pkt){
    return cache->recvTimingReq(pkt);
}

void DummyCache::CpuSidePort::recvFunctional(PacketPtr pkt){
    return cache->recvFunctional(pkt);
}



void DummyCache::CpuSidePort::recvRespRetry(){
    panic("rectRespRetry not expected!");
}

DummyCache* DummyCacheParams::create(){
    return new DummyCache(this);
}


///////////////
//
// MemSidePort
//
///////////////


bool DummyCache::MemSidePort::recvTimingResp(PacketPtr pkt){
    cache->recvTimingResp(pkt);
    return true;
}

void DummyCache::MemSidePort::recvReqRetry(){
    cache->recvReqRetry();
}

void DummyCache::MemSidePort::recvRangeChange(){
    cache->recvRangeChange();
}


