#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/m5/m5op.h"

#define SYSTEM_MEMORY 4194304/4

int memory[SYSTEM_MEMORY];
int msg[SYSTEM_MEMORY];

int dsblock_payload_handler(int length) {

 int i;

  unsigned char* buf = malloc(length);
  
  memcpy(buf,memory,length);  //read from main memory

  for(i=0; i<length; ++i) {   //xor msg and buffer
    buf[i] = buf[i] ^ msg[i];
  }

  memcpy(&memory[0],&msg[0],length); //write new data to memory


  // send buffer
  return 0;
}

int dsparity_payload_handler(int length) {

  int i;

  unsigned char* buf = malloc(length);
  
  memcpy(buf,memory,length);  //read from main memory

  for(i=0; i<length; ++i) {   //xor msg and buffer
    buf[i] = buf[i] ^ msg[i];
  }

  memcpy(&memory[0],buf,length); //write buffer to memory

  return 0;
}

int accumulate_payload_handler(int length) {
     
    
    for(int i=0; i<length/sizeof(int); i+=2)
    {	
	memory[i] = msg[i]*memory[i] - msg[i+1]*memory[i+1];
	memory[i+1] = msg[i]*memory[i+1] - msg[i+1]*memory[i];
    }
    return 0;
}


int main(int argc, char* argv[])
{
  if(argc<=2)
	return 1;
  

 
  int heandler = atoi(argv[1]);
  int data_length = atoi(argv[2]);
  m5_dumpreset_stats(0,0);
  switch(heandler)
  {
	case 0: return  dsblock_payload_handler(data_length);
	case 1: return dsparity_payload_handler(data_length);
	case 2: return accumulate_payload_handler(data_length);
	default: return 3;
  }
   m5_dumpreset_stats(0,0);
  return 0;
}
