
#ifndef __DUMMY_CACHE__
#define __DUMMY_CACHE__


#include <vector>
#include <queue>
#include "mem/mem_object.hh"

#include "base/misc.hh" // fatal, panic, and warn
#include "params/DummyCache.hh"

#include <map>
#include <tuple>

using namespace std;

class DummyCache;

class MemMonitor{
    public:
        uint32_t key;
        Addr addr;
        uint64_t size;
        bool monitor_reads;
        void * memptr;
        uint32_t intercepted;
        DummyCache * cache;
    
    public:
        MemMonitor(DummyCache * cache, uint32_t key, Addr addr, uint64_t size, bool monitor_reads, bool from_parent);
};

class DummyCache : public MemObject{
  private:

    class CpuSidePort : public SlavePort{
      private:
        DummyCache *cache;

      public:
        CpuSidePort(const std::string& name, DummyCache *cache) :
            SlavePort(name, cache), cache(cache)
        { }

        AddrRangeList getAddrRanges() const override;

      protected:
        Tick recvAtomic(PacketPtr pkt) override{ 
            panic("recvAtomic unimpl."); 
        }

        void recvFunctional(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr pkt) override;

        void recvRespRetry() override;
    };

    class MemSidePort : public MasterPort{
      private:
        DummyCache * cache;

      public:
        MemSidePort(const std::string& name, DummyCache *cache) :
            MasterPort(name, cache), cache(cache)
        { }

      protected:
        bool recvTimingResp(PacketPtr pkt) override;

        void recvReqRetry() override;

        void recvRangeChange() override;
    };

    AddrRangeList getAddrRanges() const;

    void sendRangeChange();

    CpuSidePort cpuSidePort;

    MemSidePort memSidePort;

    std::queue<PacketPtr> wqueue;

  public:

    DummyCache(DummyCacheParams *params);

    void recvTimingResp(PacketPtr pkt);
    void recvReqRetry();
    void recvRangeChange();

    bool recvTimingReq(PacketPtr pkt);
    void recvFunctional(PacketPtr pkt);
    AddrRangeList getAddrRanges();


    BaseMasterPort& getMasterPort(const std::string& if_name, PortID idx = InvalidPortID) override;

    BaseSlavePort& getSlavePort(const std::string& if_name, PortID idx = InvalidPortID) override;


  private:
    map<uint32_t, std::tuple<uint64_t, bool, bool>> tointercept;
    map<uint32_t, MemMonitor *> intercepted;
    map<Addr, MemMonitor *> monitored;

    void handle_recv(PacketPtr pkt);
    void inject_lgs_mem(PacketPtr pkt);
    void catch_gem5_updates(PacketPtr pkt);

    uint32_t system, cpu;

  public:
    DummyCache * parent;

  public: /* interesting stuff */
    void intercept(uint32_t key, uint64_t size, bool monitor_reads=false, bool from_parent=false);
    bool allIntercepted();
    void resetMonitor(uint32_t key);
    uint32_t monitor(uint32_t key);
    int getVal(uint32_t key, void * buff, size_t size);
    int setVal(uint32_t key, void * buff, size_t size);
    void * getPtr(uint32_t key);
    void setID(uint32_t system, uint32_t cpu);
    void setParent(DummyCache * parent);


};

#endif /* __DUMMY_CACHE__ */
