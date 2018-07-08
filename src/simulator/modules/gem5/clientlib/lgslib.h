#ifndef __LGSLIB_HH__
#define __LGSLIB_HH__

#include <stdint.h>
#include <stdio.h>

typedef void (*handler_t)(void*, size_t size);

#define CTRL 5555222
#define OP   5555333
#define DATA 5555444
#define SIMCALL 5555555
#define SIMCALL_NUM 5555000
#define SIMCALL_DATA 5555111

#define SHARED_DATA_SIZE 16384 /* in ints */
#define SIMCALL_DATA_SIZE 100 /* in ints */

typedef struct simcall_hdr{
    int event_type;
    int suspend;
} simcall_hdr_t;

void init();
void set_handlers(handler_t * _handlers);
int libmain();

void simtrap();
void * get_simcall_data();
#endif
