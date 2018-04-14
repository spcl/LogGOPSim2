
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <lgslib.h>
#include <spin.h>


void echo_hh_handler(void* data, size_t size)  {
    ptl_header_t * hh = (ptl_header_t *) data;
    printf("Hey! I'm the HEADER handler! Source: %u; Length: %u; Tag: %u\n", hh->source_id, hh->length, hh->match_bits);   

}

void echo_pp_handler(void* data, size_t size)  {
    printf("Hey! I'm the PAYLOAD handler!\n");    
}

void echo_cc_handler(void* data, size_t size)  {
    printf("Hey! I'm the COMPLETION handler!\n");    
}



int main(){
  init();
  handler_t h[3] = { echo_hh_handler, echo_pp_handler, echo_cc_handler };     
    
  set_handlers(h);
  printf("HELLO!!!\n");

  int res = libmain();
  return res;
}
