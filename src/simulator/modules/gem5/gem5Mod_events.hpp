#ifndef __GEM5MOD_EVENTS_HPP__
#define __GEM5MOD_EVENTS_HPP__

#include "../../simEvents.hpp"
#include <string.h>

struct hpu {
    btime_t time = 0;
    unsigned char hid;

    bool operator<(const hpu & d2) const {
           return d2.time < time; 
    }

    bool operator==(const hpu & d2) const {
           return d2.hid == hid; 
    }
};
typedef struct hpu hpu_t;

typedef void (*gem5_copy_data_t)(void*, void*, size_t);


class gem5SimRequest : public simEvent {
private:
  bool is_suspended = false;
  hpu_t current_hpu;

  /* handler data */
  //void *data = NULL;
  //size_t data_size = 0;
   

public:
  /* where/how to run */
  uint32_t host;
  uint32_t handlerIndex;

  gem5_copy_data_t copy_fun = NULL;
  gem5_copy_data_t copy_back_fun = NULL;

  void * copy_fun_ref = NULL; /* to play along with the pointers to member functions */
  void * copy_back_fun_ref = NULL;

  /* needed for drawing */
  btime_t start_hpu_time;

public:
  //void copyHandlerData(void *dest) { memcpy(dest, data, data_size); }

  void setCopyFun(gem5_copy_data_t fun, void * ref){
    copy_fun = fun; 
    copy_fun_ref = ref;
  }

  void setCopyBackFun(gem5_copy_data_t fun, void * ref){
    copy_back_fun = fun; 
    copy_back_fun_ref = ref;
  }



  bool hasCopyFun(){ return copy_fun!=NULL; }  
  bool hasCopyBackFun(){ return copy_back_fun!=NULL; }  

  bool isSuspended() { return is_suspended; }
  //bool hasData() { return data != NULL && data_size > 0; }
  hpu_t resumeHandler() {
    assert(is_suspended);
    is_suspended = false;
    return current_hpu;
  }
  void suspendHandler(hpu_t hpu) {
    current_hpu = hpu;
    is_suspended = true;
  }
  
  gem5SimRequest(uint32_t host, uint32_t handlerIndex, btime_t time){
    type = GEM5_SIM_REQUEST;
    this->time = time;
    this->host = host;
    this->handlerIndex = handlerIndex;
  }
};


class gem5SimCall : public simEvent {

public: 
  void * data;
  gem5SimRequest * simreq;
};

#endif /* __GEM5MOD_EVENTS_HPP__ */
