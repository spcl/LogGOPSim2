

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
#include "visualisation/DrawViz/TimelineVisualization.hpp"
#include "visualisation/EmptyViz/empty_viz.hpp"

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

#ifdef HAVE_GEM5
    
    if (gem5 && !simplenet){
        //printf("SMP\n");
        lmod = new P4SMPMod(sim, parser, simplenet);
        DMAmod *dmamod = new DMAmod(sim, parser.schedules.size());
        sim.addModule(dmamod);  
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
        NetMod * nmod = new NetMod(sim, gem5);
        sim.addModule((simModule *) nmod);
    }  
 

     if(args_info.chromefile_given){
       ChromeViz* vis = new  ChromeViz(args_info.chromefile_arg,1,false,4);
       sim.addVisModule(vis);
     }

     if(args_info.vizfile_given){
        TimelineVisualization* tlviz = new  TimelineVisualization(args_info.vizfile_arg, parser.schedules.size());
        sim.addVisModule(tlviz);
     }
     

    sim.simulate(parser);
    sim.printStatus(); 

}

