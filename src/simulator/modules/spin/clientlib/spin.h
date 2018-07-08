#ifndef __SPIN_HH__
#define __SPIN_HH__

#include <stdint.h>

#include "../../gem5/clientlib/lgslib.h"

#define MEM_MESSAGE 4096+100  //in ints
#define PACKET_SIZE 4096
#define MEM_PER_ME 100+PACKET_SIZE   // in ints <- this is a max, the actual one is specified in the append



// handler return codes
#define SUCCESS 0
#define PROCEED 1
#define PROCESS_DATA 2
#define PROCESS_DATA_PENDING 3
#define DROP 4
#define DROP_PENDING 5
#define SUCCESS_PENDING 6



// mock-up portals
typedef uint32_t ptl_match_bits_t;
typedef uint32_t ptl_req_ct_t;
typedef uint32_t ptl_process_t;
typedef uint64_t ptl_dma_h;
typedef uint32_t ptl_size_t;
typedef uint32_t ptl_hdr_data_t;
typedef uint32_t ptl_request_type_t;
typedef uint32_t ptl_ack_req_t;
typedef uint32_t ptl_type_t;


#define PTL_INT 1

#define PTL_ME_HOST_MEM 1
#define PTL_HANDLER_HOST_MEM 2
#define PTL_LOCAL_ME_CT 1


typedef struct {
  uint32_t arg[4];
} ptl_user_header_t;

typedef struct {
  ptl_request_type_t type; 
  ptl_size_t length ; 
  ptl_process_t target_id ; 
  ptl_process_t source_id ; 
  ptl_match_bits_t match_bits ; 
  ptl_size_t offset ;
  ptl_hdr_data_t hdr_data ; 
  ptl_user_header_t user_hdr ; 
}  ptl_header_t;

typedef struct {
  ptl_size_t length ; 
  ptl_size_t offset ; 
  uint8_t base[0]; 
}  ptl_payload_t;


typedef struct {
  simcall_hdr_t hdr;
  uint32_t size;
  uint32_t target;
  uint32_t tag;
} ptl_device_put_t;


void PtlDevicePut(uint32_t target, uint32_t size, uint32_t tag);

/*
#define GETHOST       10
#define PUTDEVICE     11
#define PUTHOST       12
#define DMA_WRITE_NB  13
#define DMA_WRITE_B   14
#define DMA_READ_NB   15
#define DMA_READ_B    16
#define DMA_WAIT      17
#define DMA_TEST      18
#define DMA_CAS_NB    19
#define DMA_CAS_B     20
#define DMA_FADD_NB   21
#define DMA_FADD_B    22
*/


#endif /* __SPIN_HH__ */

