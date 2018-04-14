

#include "cmdline.h"
#include "sim.hpp"
#include "Parser.hpp"
#include "modules/loggop/loggop.hpp"
#include "modules/packet_net/NetMod.hpp"

#include "config.h"



#ifdef HAS_GEM5
#include "modules/spin/p4smp.hpp"
#include "modules/gem5/gem5Mod.hpp"
#endif

#include "modules/portals4/p4.hpp"

#include "modules/dma/dma.hpp"

gengetopt_args_info args_info;

int main(int argc, char * argv[]){


    if (cmdline_parser(argc, argv, &args_info) != 0) {
        fprintf(stderr, "Couldn't parse command line arguments!\n");
        throw(10);
    }

    Parser parser(args_info.filename_arg, args_info.save_mem_given);
    Simulator sim(argc, argv, parser.schedules.size());

    bool simplenet = !args_info.network_file_given;
    bool gem5 = args_info.gem5_conf_file_given;   

    simModule * lmod=NULL;
    simModule * dmamod=NULL;
    NetMod * nmod=NULL;
    gem5Mod * gem5mod=NULL;


    /* sPIN configuration */
    //TODO: configure via external config file
#ifndef HAS_GEM5 
    printf("Current configuration requires gem5! (sPIN)\n");
    exit(1);
#endif

    lmod = new P4SMPMod(sim, parser, simplenet); //logic
    dmamod = new DMAmod(sim, parser.schedules.size()); //Host DMA
    nmod = new NetMod(sim, true); //Network
    gem5mod = new gem5Mod(sim, parser.schedules.size()); //gem5

    sim.addModule(lmod);
    sim.addModule(dmamod);
    sim.addModule(nmod);
    sim.addModule(gem5mod);

    sim.simulate(parser);
    sim.printStatus(); 

    if (nmod!=NULL) delete nmod; 
    if (lmod!=NULL) delete lmod; 
    if (dmamod!=NULL) delete dmamod;
}

