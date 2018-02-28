#ifndef __SMP_HH__
#define __SMP_HH__

#include <stdint.h>
#include "portals.h"

typedef void (*handler_t)(void* ,void*);



#define PACKET_SIZE 4096

#define MEM_PER_ME 100+PACKET_SIZE   // in ints
 
#define SHARED_DATA_SIZE  MEM_MESSAGE+MEM_PER_ME /* in ints */
 
#define SIMCALL_DATA_SIZE 100 /* in ints */


int PtlHandlerDMAToHostNB(void* local, ptl_size_t offset, ptl_size_t len, unsigned int options, ptl_dma_h *h);
int PtlHandlerDMAToHostB(void* local, ptl_size_t offset, ptl_size_t len, unsigned int options);
int PtlHandlerDMAFromHostNB(ptl_size_t offset, void* local, ptl_size_t len, unsigned int options, ptl_dma_h *h);
int PtlHandlerDMAFromHostB(ptl_size_t offset, void* local, ptl_size_t len, unsigned int options);
int PtlHandlerDMATest(ptl_dma_h handle);
int PtlHandlerDMAWait(ptl_dma_h handle); 


int PtlHandlerDMACASNB(ptl_size_t offset, uint64_t *cmpval, uint64_t swapval, unsigned int options, ptl_dma_h *h);
int PtlHandlerDMACASB(ptl_size_t offset, uint64_t *cmpval, uint64_t swapval, unsigned int options);
int PtlHandlerDMAFetchAddNB(ptl_size_t offset, ptl_size_t inc, ptl_size_t *res, ptl_type_t t, unsigned int options , ptl_dma_h *h);
int PtlHandlerDMAFetchAddB(ptl_size_t offset, ptl_size_t inc, ptl_size_t *res, ptl_type_t t, unsigned int options);


int PtlHandlerCAS(uint64_t *value, uint64_t cmpval, uint64_t swapval);
int PtlHandlerFAdd(uint64_t *val, uint64_t *before, uint64_t inc);

int PtlHandlerPutFromHost(ptl_size_t offset, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits,  ptl_size_t remote_offset, void* user_ptr, ptl_hdr_data_t hdr_data);
int PtlHandlerPutFromDevice (void *local, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits, ptl_size_t remote_offset , void* user_ptr , ptl_hdr_data_t hdr_data);
int PtlHandlerGetToHost(ptl_size_t offset, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits, ptl_size_t remote_offset, void* user_ptr, ptl_hdr_data_t hdr_data);

void PtlReturn(uint32_t return_val);


/* These function have to be called from inside the simulation code */

void init();
void set_handlers(handler_t * _handlers);
int libmain();

/* old code
void PtlHandlerGetToHost(uint32_t target, uint32_t size, uint32_t tag,uint32_t oct);
void PtlHandlerPutFromHost(uint32_t target, uint32_t size, uint32_t tag,uint32_t oct);
void PtlHandlerPutFromDevice(uint32_t target, uint32_t size, uint32_t tag,uint32_t oct);
void PtlHandlerDMAToHostNB(uint32_t size,uint64_t *handle);
void PtlHandlerDMAToHostB(uint32_t size);
void PtlHandlerDMAFromHostNB(uint32_t size,uint64_t *handle);
void PtlHandlerDMAFromHostB(uint32_t size);
void PtlHandlerDMAWait(uint64_t handle);
*/

#endif 
