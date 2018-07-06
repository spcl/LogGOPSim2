
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <lgslib.h>
#include <spin.h>


void echo_hh_handler(void* data, size_t size)  {
    ptl_header_t * hh = (ptl_header_t *) data;
    uint8_t * shared_mem = (uint8_t *) &(((uint32_t *) data)[MEM_MESSAGE]);

    /* the APPEND args are written in the shared mem */
    uint64_t * append_args = (uint64_t *) shared_mem;

    printf("Hey! I'm the HEADER handler! Source: %u; Length: %u; Tag: %u\n", hh->source_id, hh->length, hh->match_bits);   

    for (int i=0; i<4; i++){
        printf("HH PUT arg[%i]: %u\n", i, hh->user_hdr.arg[i]);
    }


    for (int i=0; i<4; i++){
        printf("HH APPEND arg[%i]: %llu\n", i, append_args[i]);
    }


    /* let's write a message in the shared memory so the other handlers can read it */
    const char * msg = "written by header handler\0";
    strcpy(shared_mem, msg);

}

void echo_pp_handler(void* data, size_t size)  {
    uint8_t * shared_mem = &(((uint8_t *) data)[MEM_MESSAGE]);
    printf("Hey! I'm the PAYLOAD handler (msg: %s)!\n", (char *) shared_mem);    
}

void echo_cc_handler(void* data, size_t size)  {
    uint8_t * shared_mem = &(((uint8_t *) data)[MEM_MESSAGE]);
    printf("Hey! I'm the COMPLETION handler (msg: %s)!\n", (char *) shared_mem);    
}



int main(){
  init();
  handler_t h[3] = { echo_hh_handler, echo_pp_handler, echo_cc_handler };     
    
  set_handlers(h);
  printf("HELLO!!!\n");

  int res = libmain();
  return res;
}
