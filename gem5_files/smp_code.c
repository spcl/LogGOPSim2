#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "smp.h"


/**********************************************************************
 * start of the broadcast handler section
 *********************************************************************/

/* For goal file:
 Append: arg1 <my_rank> arg2 <p> arg3 <bcastmatch> arg4 <len>
*/


typedef struct {
  uint32_t my_rank;
  uint32_t p;           // total number of hosts
  uint32_t bcastmatch;
  uint32_t len;
} bcast_state_t;

void bcast_payload_handler(void*  message, void *state) {
  bcast_state_t *s = (bcast_state_t*)state;
  ptl_payload_t p = *(ptl_payload_t*)message;

  for (int half =  s->p>>1; half>=1; half>>=1 )
    if (s->my_rank % (half<<1) ==0)
      PtlHandlerPutFromDevice(0, p.length, PTL_LOCAL_ME_CT, 0, s->my_rank+half, s->bcastmatch, 0, NULL, 0);
 
  PtlReturn(SUCCESS);
  return;
}


void bcast_completion_handler(void*  message, void *state) {
  bcast_state_t *s = (bcast_state_t*)state;
  for (int half = s->p>>1; half>=1; half>>=1 )
    if (s->my_rank % (half<<1) == 0)
      PtlHandlerPutFromHost(0, s->len, PTL_LOCAL_ME_CT, 0, s->my_rank+half, s->bcastmatch, 0, NULL, 0);

  PtlReturn(SUCCESS);
  return;
}




/**********************************************************************
* end of the broadcast handler section
*********************************************************************/


/**********************************************************************
 * start of the accumulate of complex numbers 
 *********************************************************************/

typedef struct {
  ptl_size_t offset;
} accumulate_state_t;



void accumulate_payload_handler(void* message,void * state) {
  ptl_payload_t p = *(ptl_payload_t*)message;
  accumulate_state_t *s = (accumulate_state_t*)state;  
  float buf[PACKET_SIZE/sizeof(float)];
  PtlHandlerDMAFromHostB(s->offset+p.offset, NULL,  p.length, PTL_ME_HOST_MEM); // NULL stands for buf
  float *data = (float*)p.base;
  for(int i=0; i<p.length/sizeof(float); i+=2) {	
    asm("");
    buf[i] = data[i]*buf[i] - data[i+1]*buf[i+1];
    buf[i+1] = data[i]*buf[i+1] - data[i+1]*buf[i];
  }
  PtlHandlerDMAToHostB(NULL, s->offset+p.offset, p.length, PTL_ME_HOST_MEM); // NULL stands for buf
  PtlReturn(SUCCESS);
  return;
}



void accumulate_payload_pong_handler(void* message,void * state) {
  ptl_payload_t p = *(ptl_payload_t*)message;
  accumulate_state_t *s = (accumulate_state_t*)state;  
  float buf[PACKET_SIZE/sizeof(float)+16];
  PtlHandlerDMAFromHostB(s->offset+p.offset, NULL,  p.length, PTL_ME_HOST_MEM); // NULL stands for buf
  float *data = (float*)p.base;
  for(int i=0; i<p.length/sizeof(float); i+=2){	
    asm("");
    buf[i] = data[i]*buf[i] - data[i+1]*buf[i+1];
    buf[i+1] = data[i]*buf[i+1] - data[i+1]*buf[i];
  }
  PtlHandlerDMAToHostB(NULL, s->offset+p.offset,  p.length, PTL_ME_HOST_MEM); // NULL stands for buf
  PtlHandlerPutFromDevice(NULL,p.length, 1,0, 0,10,0, NULL, 0); // NULL stands for buf
  PtlReturn(SUCCESS);
  return;
}



/**********************************************************************
 * end of the accumulate for complex numbers 
 *********************************************************************/



void cache_test_header_handler(void* message,void * state){



  uint32_t * test = (uint32_t *) malloc(sizeof(uint32_t)*128);

  srand(12345);
  int sum=0;

  for (int i=0; i<8192*4; i++){
    sum += test[rand() % 8192];    
  }

  printf("#############sum: %u\n", sum);

  PtlReturn(SUCCESS);
  return;
}



/**********************************************************************
 * start of the PINGPONG handler section
 *********************************************************************/

/* For goal file:
 Append: arg1 <len>
*/

typedef struct {
  uint64_t len;
} pingpong_state_t;

void pingpong_empty_handler(void* message,void * state){
  PtlReturn(SUCCESS);
  return;
}

void pingpong_payload_handler(void*  message,void * state){
  ptl_payload_t p = *(ptl_payload_t*)message;	
  PtlHandlerPutFromDevice( NULL, p.length, 1,  0,  0, 10, 0, NULL, 0);
  PtlReturn(SUCCESS);
  return;
}

void pingpong_completion_handler(void*  message,void * state){
  pingpong_state_t *s = (pingpong_state_t*)state;
 // printf("gem5: %llu \n",s->len);  
  PtlHandlerPutFromHost(0,(uint32_t)s->len, 1, 0, 0, 10, 0,NULL,  0);
  PtlReturn(SUCCESS);
  return;
}

/**********************************************************************
 * end of the PINGPONG handler section
 *********************************************************************/


/**********************************************************************
 * start of the DATATYPES handler section
 *********************************************************************/

/* For goal file:
 Append: arg1 <vlen> arg2 <stride> arg3 <offset>
*/

typedef struct {
  uint32_t vlen;
  uint32_t stride;
  uint32_t offset;
} ddtvec_state_t;

void ddtvec_header_handler(void* message, void *state){
  ptl_header_t h = *(ptl_header_t*)message;
  ddtvec_state_t *s = (ddtvec_state_t*)state; 
  PtlReturn(PROCESS_DATA);
  return;
}

void ddtvec_payload_handler(void* message, void *state) {
  asm("");
  ptl_payload_t p = *(ptl_payload_t*)message;
  ddtvec_state_t *s = (ddtvec_state_t*)state; 
  uint32_t first_seg_num = (s->offset+p.offset)/s->vlen; // rounded down implicitly
  uint32_t last_seg_num = (s->offset+p.offset+p.length)/s->vlen; // rounded down implicitly
  
  int offset_in_packet = 0;
  for(int i = first_seg_num; i<=last_seg_num; i++){
    uint32_t temp = ((s->offset+p.offset+offset_in_packet)%s->vlen);
    ptl_size_t offset =  i * (s->vlen+s->stride) + temp;
    ptl_size_t size =  (s->vlen - temp) < (p.length - offset_in_packet) ? (s->vlen - temp) : (p.length - offset_in_packet) ;
    PtlHandlerDMAToHostB(NULL, 0, size, PTL_ME_HOST_MEM);  //NULL stands for offset
    offset_in_packet+=size;
  }
  
  PtlReturn(SUCCESS);
  return;
}
/**********************************************************************
 * end of the DATATYPES handler section
 *********************************************************************/

/**********************************************************************
 * start of distributed storage  handler section
 *********************************************************************/
#define PARITY_TAG 53
#define READREPLY_TAG 50

#define WRITEREPLY_TAG 40
#define UPDATEREPLY_TAG 30

typedef struct {
  uint32_t length;
} dataserver_state_t;



void dataserver_write_header_handler(void* message,void * state) {
  ptl_header_t h = *(ptl_header_t*)message; 
  dataserver_state_t *s = (dataserver_state_t*)state;  
  s->length= h.length;
  PtlReturn(PROCESS_DATA);
  return;
}


void 	dataserver_write_payload_handler(void* message,void * state) {
  ptl_payload_t p = *(ptl_payload_t*)message;
  dataserver_state_t *s = (dataserver_state_t*)state;  
  uint32_t buf[PACKET_SIZE/sizeof(uint32_t)];
  PtlHandlerDMAFromHostB(0, NULL, p.length, PTL_ME_HOST_MEM); // NULL = buf
  uint32_t* hpos = (uint32_t*)p.base;
  PtlHandlerDMAToHostB(NULL, 0,  p.length, PTL_ME_HOST_MEM); // NULL = hpos
  
  for(int i=0; i<p.length/sizeof(uint32_t); i+=8){
    asm("");     
    buf[i] = (buf[i] ^ hpos[i]); 
    buf[i+1] = (buf[i+1] ^ hpos[i+1]); 
    buf[i+2] = (buf[i+2] ^ hpos[i+2]); 
    buf[i+3] = (buf[i+3] ^ hpos[i+3]); 
    buf[i+4] = (buf[i+4] ^ hpos[i+4]); 
    buf[i+5] = (buf[i+5] ^ hpos[i+5]); 
    buf[i+6] = (buf[i+6] ^ hpos[i+6]); 
    buf[i+7] = (buf[i+7] ^ hpos[i+7]); 
  } 
  if(p.offset+p.length == s->length) //lastpacket
    PtlHandlerPutFromDevice(NULL, p.length, PTL_LOCAL_ME_CT, 0, 1, PARITY_TAG, 0, NULL, 1);
  else
    PtlHandlerPutFromDevice(NULL, p.length, PTL_LOCAL_ME_CT, 0, 1, PARITY_TAG, 0, NULL, 0);
   
  PtlReturn(SUCCESS);
  return;
}

void dataserver_read_header_handler(void* message,void * state) {
  ptl_header_t h = *(ptl_header_t*)message;    
  PtlHandlerPutFromHost(0, h.user_hdr.arg[0], PTL_LOCAL_ME_CT, 0, 0, READREPLY_TAG, 0, NULL, 0);
  PtlReturn(SUCCESS);
  return;
}

void dataserver_acknowledgement_header_handler(void* message,void * state) {   
    PtlHandlerPutFromDevice(NULL, 1, PTL_LOCAL_ME_CT, 0, 0, WRITEREPLY_TAG, 0, NULL, 0);
    PtlReturn(SUCCESS);
    return;
}


typedef struct {
  uint32_t last_packet;
  uint32_t source;
} parity_state_t;

void parity_update_header_handler(void* message,void * state) {
  ptl_header_t h = *(ptl_header_t*)message; 
  parity_state_t *s = (parity_state_t*)state;  
  s->source= h.source_id;
  s->last_packet=h.user_hdr.arg[0];
  PtlReturn(PROCESS_DATA);
  return;
}

void parity_update_payload_handler(void*  message,void * state)  {
  ptl_payload_t p = *(ptl_payload_t*)message;
 
  uint32_t buf[PACKET_SIZE/sizeof(uint32_t)];
  PtlHandlerDMAFromHostB(0, NULL, p.length, PTL_ME_HOST_MEM); // NULL = buf

  uint32_t* hpos = (uint32_t*)p.base;
 	
  for(int i=0; i<p.length/sizeof(uint32_t); i+=8){
    asm("");     
    buf[i] = (buf[i] ^ hpos[i]); 
    buf[i+1] = (buf[i+1] ^ hpos[i+1]); 
    buf[i+2] = (buf[i+2] ^ hpos[i+2]); 
    buf[i+3] = (buf[i+3] ^ hpos[i+3]); 
    buf[i+4] = (buf[i+4] ^ hpos[i+4]); 
    buf[i+5] = (buf[i+5] ^ hpos[i+5]); 
    buf[i+6] = (buf[i+6] ^ hpos[i+6]); 
    buf[i+7] = (buf[i+7] ^ hpos[i+7]); 
  } 
  PtlHandlerDMAToHostB(NULL, 0, p.length, PTL_ME_HOST_MEM); // NULL = buf
  PtlReturn(SUCCESS);
  return;
}


void parity_update_completion_handler(void*  message,void * state)  {
  parity_state_t *s = (parity_state_t*)state;  
  if(s->last_packet)
    PtlHandlerPutFromDevice(NULL,1, PTL_LOCAL_ME_CT, 0, s->source, UPDATEREPLY_TAG, 0, NULL, 0);
  PtlReturn(SUCCESS);
  return;
}



/**********************************************************************
 * end of the distributed storage handler section
 *********************************************************************/


/**********************************************************************
 * start of YCSB handler section
 *********************************************************************/
#include "uthash.h"

#define CLIENT 0
#define ACK 1

#define INSERT(KVS, KEY, DPTR, DSIZE) {\
    kval_entry_t * new_entry = (kval_entry_t *) malloc(sizeof(kval_entry_t));\
    void * data = malloc(DSIZE); \
    memcpy(data, DPTR, DSIZE); \
    new_entry->key = KEY; \
    new_entry->value = data; \
    new_entry->length = DSIZE; \
    HASH_ADD_INT(KVS, key, new_entry); \
}

#define READ(KVS, KEY, D2PTR, DSIZE) {\
    kval_entry_t * read_entry; \
    HASH_FIND_INT(KVS, &KEY, read_entry);\
    if (read_entry==NULL) { *D2PTR=NULL; *DSIZE=0; }\
    else{ *D2PTR = read_entry->value; *DSIZE = read_entry->length; }\
}

typedef struct kval_entry{
    uint64_t key;
    uint32_t length;
    void * value;
    UT_hash_handle hh;
} kval_entry_t;

kval_entry_t * kvs = NULL;

typedef struct kvs_state_t{
    uint64_t length;
    uint64_t arg[3]; 
} kvs_state_t;

void common_header_ycsb(void* message,void * state){
 	ptl_header_t h = *(ptl_header_t*)message; 
	kvs_state_t *s = (kvs_state_t*)state; 
    s->length = h.length;
	s->arg[0]=h.user_hdr.arg[0];
	s->arg[1]=h.user_hdr.arg[1];
	s->arg[2]=h.user_hdr.arg[2];
    //uint64_t t = s->arg[0];
    //printf("key: %llu==%llu: %i (%llu)\n", s->arg[0], h.user_hdr.arg[0], s->arg[0]==h.user_hdr.arg[0], t);
}


//Handler 20 INSERT
//put arg1 - <key> 
void kvs_insert(void* message, void * state) {
  kvs_state_t *s = (kvs_state_t*)state;    
  

  //printf("state: %p; length: %i; key: %llu\n", state, s->length, s->arg[0]);
  //INSERT( storage, key, data pointer, data size)
  INSERT(kvs, s->arg[0], message, s->length);

   // reply when is done
  PtlHandlerPutFromHost(0, ACK, 1, 0, CLIENT, 10, 0,NULL,  0);

  PtlReturn(SUCCESS);
  return;
}


//Handler 21 UPDATE
//put arg1 - <key> arg2 - <field>
void kvs_update(void* message,void * state) {
  kvs_state_t *s = (kvs_state_t*)state;     
  void* data = NULL; 
  uint64_t size = 0;
  //READ( storage, key, pointer to data pointer, data size)
  READ(kvs, s->arg[0], &data, &size);

  //update specific field in the record if it exists
  if(size>0) {
      memcpy(data+100*s->arg[1],message,s->length);
  }

  // reply when is done
  PtlHandlerPutFromHost(0, ACK, 1, 0, CLIENT, 10, 0,NULL,  0);
  PtlReturn(SUCCESS);
  return;
}


//Handler 22 READ
//put arg1 - <key> arg2 - <size>. 
//arg2 is redundant. It can be used for checikng
void kvs_read(void* message,void * state) {
  kvs_state_t *s = (kvs_state_t*)state; 

  void* data = NULL; 
  uint64_t size = 0;
  //READ( storage, key, pointer to data pointer, data size)
  READ(kvs, s->arg[0], &data, &size);

  // put to CLIENT from data with size
  if(size>0) {
     // key was found
     PtlHandlerPutFromHost(0, size, 1, 0, CLIENT, 10, 0,NULL,  0);
  } else {
	// key does not exist
     PtlHandlerPutFromHost(0, ACK, 1, 0, CLIENT, 10, 0,NULL,  0);
  }

  PtlReturn(SUCCESS);
  return;
}



/**********************************************************************
 * end of the YCSB handler section
 *********************************************************************/




int main(){
  init();
  handler_t h[50] = {
/*0*/		bcast_payload_handler,		
		bcast_completion_handler,
		accumulate_payload_handler,
		accumulate_payload_pong_handler,
		pingpong_empty_handler,
/*5*/		pingpong_payload_handler,
		pingpong_completion_handler,
		ddtvec_header_handler,
		ddtvec_payload_handler,
		dataserver_write_header_handler,
/*10*/		dataserver_write_payload_handler,
		dataserver_read_header_handler,
		dataserver_acknowledgement_header_handler,
		parity_update_header_handler,
		parity_update_payload_handler,
/*15*/		parity_update_completion_handler,
		common_header_ycsb,
        kvs_insert,
        kvs_update,
        kvs_read,
        cache_test_header_handler        
};        
	
  set_handlers(h);
  int res = libmain();
  return res;
}


