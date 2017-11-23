#include "sim.hpp"
#include "simEvents.hpp"

//#include "liblsb.h" 


uint64_t simevent::gid=0;


void Simulator::addHandler(SimModule* mod, ekey_t key, efun_t fun){
    this->dmap[key] = std::make_pair(mod, fun);
}




void Simulator::printStatus(){
    printf("Total events: %lu;", simevent::gid); 
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



void Simulator::addModule(SimModule* mod){
    mod->registerHandlers(*this);
    mods.push_back(mod);   
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






