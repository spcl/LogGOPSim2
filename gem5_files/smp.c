#include <stdio.h>
#include "smp.h"
#include <string.h>

volatile uint32_t op;
volatile uint32_t ctrl;
volatile uint32_t data[SHARED_DATA_SIZE];

volatile uint32_t simcall;
volatile uint32_t simcall_num;
volatile uint32_t simcall_data[SIMCALL_DATA_SIZE];


handler_t * handlers;

void init(){
    
    /* binding variables */
    op = OP;
    ctrl = CTRL;
    data[0] = DATA;
    simcall = SIMCALL;
    simcall_num = SIMCALL_NUM;
    simcall_data[0] = SIMCALL_DATA;       
}

void set_handlers(handler_t * _handlers){
    handlers=_handlers;
}


int libmain(){

    while (ctrl){ 
	   __sync_synchronize();
        void* message = (void*)&data[0];
        void* state = (void*)&data[MEM_MESSAGE];
        handlers[op](message, state);
        __sync_synchronize();
    }
    return op;
}


int PtlHandlerDMAToHostNB(void* local, ptl_size_t offset, ptl_size_t len, unsigned int options, ptl_dma_h *h){
    simcall_num = DMA_WRITE_NB;
    simcall_data[0] = len;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;

    *h = ((ptl_dma_h *) simcall_data)[0]; //handler_generator; 
    return 0;
}

int PtlHandlerDMAToHostB(void* local, ptl_size_t offset, ptl_size_t len, unsigned int options){
    simcall_num = DMA_WRITE_B;
    simcall_data[0] = len;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;
    return 0;
}

int PtlHandlerDMAFromHostNB(ptl_size_t offset, void* local, ptl_size_t len, unsigned int options, ptl_dma_h *h){
    simcall_num = DMA_READ_NB;
    simcall_data[0] = len;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;

    *h = ((ptl_dma_h *) simcall_data)[0]; //handler_generator;  
    return 0;    
}

 int __attribute__((always_inline)) inline PtlHandlerDMAFromHostB(ptl_size_t offset, void* local, ptl_size_t len, unsigned int options){
    simcall_num = DMA_READ_B;
    simcall_data[0] = len;
     __sync_synchronize();
    
    simcall++;
    return 0;	
}


int PtlHandlerDMATest(ptl_dma_h handle){
    printf("Warning! Action PtlHandlerDMATest is not implemented\n");
    return 0;	
}

int PtlHandlerDMAWait(ptl_dma_h handle){
    simcall_num = DMA_WAIT;
    ((ptl_dma_h *)simcall_data)[0] = handle;
    __sync_synchronize();
    /* make system call (like switching to kernel mode)  */
    simcall++;
    return 0;
}


int PtlHandlerDMACASNB(ptl_size_t offset, uint64_t *cmpval, uint64_t swapval, unsigned int options, ptl_dma_h *h){
   
    simcall_num = DMA_CAS_NB;
    simcall_data[0] = sizeof(uint64_t);
    /* make system call (like switching to kernel mode)  */
    __sync_synchronize();
    simcall++;

    *h = ((ptl_dma_h *) simcall_data)[0]; //handler_generator;  
    uint32_t success = ((uint32_t *) simcall_data)[2]; 
    if(!success)
         *cmpval = swapval;
    
    return 0;
}

int PtlHandlerDMACASB(ptl_size_t offset, uint64_t *cmpval, uint64_t swapval, unsigned int options){
    simcall_num = DMA_CAS_B;
    simcall_data[0] = sizeof(uint64_t);
    /* make system call (like switching to kernel mode)  */
    __sync_synchronize();
    simcall++;
    uint32_t success = ((uint32_t *) simcall_data)[2]; 
    if(!success)
      *cmpval = swapval;
   
    return 0;
}

int PtlHandlerDMAFetchAddNB(ptl_size_t offset, ptl_size_t inc, ptl_size_t *res, ptl_type_t t, unsigned int options , ptl_dma_h *h){
    simcall_num = DMA_FADD_NB;
    simcall_data[0] = t;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;

    *h = ((ptl_dma_h *) simcall_data)[0]; //handler_generator;  
	
    return 0;
}

int PtlHandlerDMAFetchAddB(ptl_size_t offset, ptl_size_t inc, ptl_size_t *res, ptl_type_t t, unsigned int options){
    simcall_num = DMA_FADD_B;
    simcall_data[0] =  t;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;

    return 0;
}

inline int PtlHandlerCAS(uint64_t *value, uint64_t cmpval, uint64_t swapval){
    if(*value==cmpval)
    {
	*value = swapval;
        return 1;
    }
    return 0;
}

int __attribute__((always_inline)) inline PtlHandlerFAdd(uint64_t *val, uint64_t *before, uint64_t inc){
    *before = *val;
    *val=*val+inc;
    return 0;
}


int __attribute__((always_inline)) inline PtlHandlerPutFromHost(ptl_size_t offset, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits,  ptl_size_t remote_offset, void* user_ptr, ptl_hdr_data_t hdr_data){

    simcall_num = PUTHOST;
    simcall_data[0] = target_id;
    simcall_data[1] = len;
    simcall_data[2] = match_bits;
    simcall_data[3] = hdr_data;
    __sync_synchronize();

    /* make system call (like switching to kernel mode)  */
    simcall++;
    return 0;
}


int __attribute__((always_inline)) inline PtlHandlerPutFromDevice (void *local, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits, ptl_size_t remote_offset , void* user_ptr , ptl_hdr_data_t hdr_data){

    simcall_num = PUTDEVICE;
    simcall_data[0] = target_id;
    simcall_data[1] = len;
    simcall_data[2] = match_bits;
	simcall_data[3] = hdr_data;
    
    __sync_synchronize();

    /* make system call (like switching to kernel mode)  */
    simcall++;
    return 0;
}

int __attribute__((always_inline)) inline PtlHandlerGetToHost(ptl_size_t offset, ptl_size_t len, ptl_req_ct_t ct, ptl_ack_req_t ack_req, ptl_process_t target_id, ptl_match_bits_t match_bits, ptl_size_t remote_offset, void* user_ptr, ptl_hdr_data_t hdr_data){

    simcall_num = GETHOST;
    simcall_data[0] = target_id;
    simcall_data[1] = len;
    simcall_data[2] = match_bits;
    simcall_data[3] = ct;
    __sync_synchronize();    
    /* make system call (like switching to kernel mode)  */
    simcall++;
    return 0;
}
 

void PtlReturn(uint32_t return_val){
    simcall_data[0] = return_val;
    __sync_synchronize();
    return;	
}

 

