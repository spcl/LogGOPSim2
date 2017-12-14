#include "sim.hpp"
#include "simEvents.hpp"

//#include "liblsb.h" 


uint64_t simEvent::gid=0;

uint64_t visEvent::gid=0;

void Simulator::addEventHandler(simModule* mod, ekey_t key, efun_t fun){
    this->dmap[key] = std::make_pair(mod, fun);
}

void Simulator::addSignalHandler(simModule* mod, sim_signal_t signal, sfun_t fun){
    this->dmap_signals[signal] = std::make_pair(mod, fun);
}

void Simulator::addVisualisationHandler(visModule* mod, vis_signal_t signal, visfun_t fun){
    this->dmap_vis[signal].push_back(std::make_pair(mod, fun));
}


void Simulator::printStatus(){
    printf("Total events: %lu;", simEvent::gid); 
#ifdef STATS
    printf(" Reinserted: %lu;\n", reinserted);
#else
    printf("\n");
#endif

    if (args_info.batchmode_given){
        maxTime();
    }else{
        for (unsigned int i=0; i<mods.size(); i++){
            mods[i]->printStatus();
        }
    }
}


void Simulator::maxTime(){
    size_t max = 0;
    size_t temp;
    for (unsigned int i=0; i<mods.size(); i++){
        temp = mods[i]->maxTime();
        if(temp>max) max=temp;
    }
    printf("Maximum finishing time %lu\n",max);
}



void Simulator::addModule(simModule* mod){
    mod->registerHandlers(*this);
    mods.push_back(mod);   
}

void Simulator::addVisModule(visModule* vismod){
    vismod->registerHandlers(*this);
    vismods.push_back(vismod);  
}

int Simulator::simulate(IParser& parser){

    parser.addRootNodes(this);
    
 
    //printf("aq size: %i\n", aq.size());       
    while(!aq.empty()){
        simulate();

        parser.addNewNodes(this);
    }


    return 0;
}






