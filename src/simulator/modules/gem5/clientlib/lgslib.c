#include <stdio.h>
#include "lgslib.h"
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
        void* dataptr = (void*) &data[0];
        /* TODO: pass the actual size instead of SHARED_DATA_SIZE*/
        handlers[op]((void *) dataptr, SHARED_DATA_SIZE);
        __sync_synchronize();
    }
    return op;
}
