

#include "cmdline.h"
#include "sim.hpp"
#include "Parser.hpp"
#include "modules/loggop/loggop.hpp"
#include "modules/packet_net/NetMod.hpp"

#include "config.h"



#ifdef HAVE_GEM5
#include "modules/portals4_smp/p4smp.hpp"
#endif

#include "modules/portals4/p4.hpp"

#include "modules/dma/dma.hpp"



#include "visualisation/ChromeViz/chrome_viz.hpp"

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

#ifdef HAVE_GEM5
    
    if (gem5 && !simplenet){
        //printf("SMP\n");
        lmod = new P4SMPMod(sim, parser, simplenet);
        dmamod = new DMAmod(sim, parser.schedules.size());
    }else{
        lmod = new P4Mod(sim, parser, simplenet);
    }
#else

    if (gem5) {
        printf("Warning: configured without GEM5 support. Ignoring gem5 conf file.\n");
        gem5=false;
    }
    lmod = new P4Mod(sim, parser, simplenet);


#endif
   
    sim.addModule(lmod);    
   
    if (!simplenet){
        nmod = new NetMod(sim, gem5);
        sim.addModule((simModule *) nmod);
    }  
    if(dmamod!=NULL) sim.addModule(dmamod);    

    //TODO; include vis modules
    // write constructors and args_info
    // add calls in the code
    // delete modules here
   // TimelineVisualization * tlviz = new TimelineVisualization(args_info.vizfile_arg, args_info.vizfile_given, ranks);
    printf("before creating \n");
     ChromeViz* vis = new  ChromeViz("test.json",1,false,4);
     sim.addVisModule(vis);

    sim.simulate(parser);


    sim.printStatus(); 



    if (nmod!=NULL) delete nmod; 
    if (lmod!=NULL) delete lmod; 
    if (dmamod!=NULL) delete dmamod;
    delete vis;

}

