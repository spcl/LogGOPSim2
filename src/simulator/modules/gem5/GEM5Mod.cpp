#include "../../../config.h"

#ifdef HAVE_GEM5

#include <cstdlib> 
#include <iostream> 
#include <sstream>

#include "GEM5Mod.hpp"

#include "base/statistics.hh" 
#include "base/str.hh" 
#include "base/trace.hh" 
#include "base/socket.hh"
#include "cpu/base.hh" 
#include "sim/cxx_config_ini.hh" 
#include "sim/init_signals.hh" 
#include "sim/serialize.hh" 
#include "sim/simulate.hh" 
#include "sim/stat_control.hh" 
#include "sim/system.hh"
#include "stats.hh"
#include "sim/process.hh"

#include "arch/arm/linux/process.hh"

#include "clientlib/portals.h"

#include <map>

/* Warning: here we make a strong assumption on the config.ini: cpus appear in system #id order. */
#define HPU(i,j) cpus[i*hpus + j]
#define DUMMYCACHE(i, j) dummycaches[i*hpus + j]

inline bool ends_with(std::string const & value, std::string const & ending, int rpad=0){
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin()+rpad);
}


int P4SMPMod::dispatch(simModule* mod, simEvent* _elem){
    GEM5Mod* pmod = (GEM5Mod*) mod;

    switch(_elem->type){
        case GEM5_SIM_REQUEST:
            return pmod->executeHandler(*((gem5SimRequest *) _elem));
    }
    return -1;
}

/* TODO ranks has to go */
GEM5Mod::GEM5Mod(Simulator& sim, uint32_t ranks): sim(sim), p(ranks){

    /* Load parameters */
    char * cfile = sim.args_info.gem5_conf_file_arg;
    timemult = sim.args_info.gem5_time_mult_arg;    
    print = sim.args_info.verbose_given;
     
    /* Initialise the cxx_config directory */
    cxxConfigInit();

    /* set event queues */
    numMainEventQueues=1;
    mainEventQueue.push_back(new myQueue("main"));
    curEventQueue(getEventQueue(0));

    /* disable GDB listener (too many ports) */
    ListenSocket::disableAll(); 
    initSignals();

    /* set stats */
    Stats::initSimStats();     
    Stats::registerHandlers(CxxConfig::statsReset, CxxConfig::statsDump);

    /* load config file */
    CxxConfigFileBase *conf = new CxxIniFile();
    if (!conf->load(cfile)) {
        printf("Can't open config file! (%s)\n", cfile);
        std::exit(EXIT_FAILURE);     
    }

    /*std::vector<std::string> objnames;
    conf->getAllObjectNames(objnames);

    for (auto objname = objnames.begin(); objname != objnames.end(); ++objname){
        printf("objname: %s\n", objname->c_str());
    }*/

    /* Config manager is created out of the config file */
    config_manager = new CxxConfigManager(*conf); 

    /* enable stats */
    CxxConfig::statsEnable();

    /* Floating Point Excpetion without this. 1 tick is 1ps */
    setClockFrequency(1000000000000);


    //config_manager->findAllObjects();

    /* Instantiate configuration */
    try {  
        printf("Instantiating simulation\n");   
        config_manager->instantiate(true);     
        config_manager->initState();     
        config_manager->startup();     
    } catch (CxxConfigManager::Exception &e) {
        std::cerr << "Config problem in sim object " << e.name << ": " << e.message << "\n";         
        std::exit(EXIT_FAILURE);
    }

    /*for(std::list<SimObject*>::iterator it = config_manager->objectsInOrder.begin(); it != config_manager->objectsInOrder.end(); ++it) {

        printf("objectsInOrder: %s %p\n", (*it)->name().c_str(), *it);
    }*/
       
    uint32_t systems=0;
    /* get cpus and memories */
    for(std::map<std::string,SimObject*>::iterator it = config_manager->objectsByName.begin(); it != config_manager->objectsByName.end(); ++it) {
        if (ends_with(it->first, "cpu", 1)){
            cpus.push_back((DerivO3CPU*) it->second);
            //printf("SimObject: %s %p\n", it->first.c_str(), it->second);
        }else if (ends_with(it->first, "dummycache")){
            //printf("SimObject: DummyCache found (%s)!\n", it->first.c_str());
            dummycaches.push_back((DummyCache*) it->second);         
        }else if (ends_with(it->first, "mem_ctrls")){
            //printf("SimObject: %s %p\n", it->first.c_str(), it->second);
            //mems.push_back((ControlledSimpleMemory*) it->second);
            systems++;
        }
       // printf("SimObject: %s\n", it->first.c_str());
    }

    uint32_t hpusperhost = cpus.size() / systems; /* hpus per system */
    if (print) printf("hpus per host is : %u\n", hpusperhost);
    hpus=hpusperhost;
 
   
    /* set intercept vars */ 
    //TODO: remove these hardcoded values!!!!!
    for (uint32_t i=0; i<dummycaches.size(); i++){
        //printf("i: %u; systems: %u; system: %i; cpu: %i\n", i, systems, i/hpus, i%hpus);
        dummycaches[i]->setID(i/hpus, i%hpus);

        dummycaches[i]->intercept(CTRL, 4, true);
        dummycaches[i]->intercept(OP, 4, false);
        dummycaches[i]->intercept(SIMCALL, 4, true);
        dummycaches[i]->intercept(SIMCALL_NUM, 4, false);
        dummycaches[i]->intercept(SIMCALL_DATA, 400, false);

        if (i % hpus != 0) dummycaches[i]->setParent(dummycaches[(i/hpus)*hpus]);
        dummycaches[i]->intercept(DATA, 33568, false, i % hpus!=0);

    }
    
    for (uint32_t i=0; i<systems; i++){
        /* suspend all the hpus but 0 (we need just one cpu to perform the memory binding) */
        for (uint32_t j=1; j<hpus; j++){
            HPU(i,j)->suspendContext(0);
        }
    }

    uint32_t * hpu_executing = (uint32_t *) calloc(systems, sizeof(uint32_t));
        
    
    /* simulate until all the variables are intercepted */
    while (!curEventQueue()->empty() /*&& suspended < mems.size()*/){
        curEventQueue()->serviceOne();
            
        /* TODO: there could be a way to get the system index of the last serviced event */
        for (uint32_t i=0; i<systems; i++){
            uint32_t hpuid = hpu_executing[i];

            //printf("system: %u; hpuid: %u\n", i, hpuid);    
            if (hpuid<hpus && DUMMYCACHE(i, hpuid)->monitor(CTRL)){
                //printf("suspending hpu %i on system %i\n", hpuid, i);   
                HPU(i, hpuid)->suspendContext(0);
    
                DUMMYCACHE(i, hpuid)->resetMonitor(CTRL);

                if (!DUMMYCACHE(i, hpuid)->allIntercepted()){
                    printf("## Warning: DummyCache(%i %i) suspending context before intercepting\n", i, hpuid);
                }


                hpu_executing[i]++;
                if (hpu_executing[i]<hpus) {
                    //printf("activating hpu %i on system %i\n", hpu_executing[i], i);
                    HPU(i, hpu_executing[i])->activateContext(0);                
                }

            }
        }
    }

    nexthpu.resize(ranks);
    waiting_queue.resize(ranks);
    hpu_t hpu;
    for(int j=0; j<ranks; j++){
        for(int i=0; i<hpus; i++){
            hpu.hid = i;
            nexthpu[j].push(hpu);
        }
    }
}


int GEM5Mod::executeHandler(gem5SimRequest& elem){

    int once = 1;
    btime_t  htot = 0;
    hpu_t hpu;

    uint32_t hindex = elem.handlerIndex;
    uint32_t host = elem.target;
   
    /* Check if we are resuming the handler or starting it from scratch */
    if (!elem.isSuspended()){ /* in this case we have to seek resources */
        /* 
        * Check if there is an avaialable context. In this case the handler 
        * is being executed (currently suspended) and we don't know 
        * when it will finish.
        */
        if (nexthpu[host].empty()){

            if (print) printf("--no context available; time: %lu; host[%i]; Keeping event in th gem5mod waiting queue\n", time, host);
            /* no HPU available, insert event in our local waiting queue */
            waiting_queue[host].push(&elem);

            /* this means that the caller should not reinser the event in the queue */
            return NO_CONTEXT_AVAIL;
        }        
    
        hpu = nexthpu[host].top();

        /* Check if the hpu is available at this time. If not, we now have an estimate. */
        if(hpu.time>time){ /* not avilable */
            if (print) printf("--no hpu available; time: %lu; nexthpu[%i]: %lu;\n", time, host, hpu.time);
            /* time at which it will be */
            time = hpu.time;

            /* the caller should reinsert the event in this case */
            return NO_HPU_AVAIL;
        }

        /* get the hpu => by popping we also remove the context availability */
        nexthpu[host].pop();  
       
        /* Set the handler to execute */
        DUMMYCACHE(host, hpu.hid)->setVal(OP, (void*)&hindex, sizeof(uint32_t));

        //printf("copying data (%i %i): %p\n", host, hpu.hid, DUMMYCACHE(host, hpu.hid)->getPtr(DATA));
        /* Copy message */
        if (elem.hasData()) elem.copyHandlerData(DUMMYCACHE(host, hpu.hid)->getPtr(DATA));

        //printf("done\n");
        hpu.time = time; /* the hpu time is set to the elem arrival time */
        elem.start_hpu_time = time; /* used for drawing */

    }else{ /* we are resuming an already started handler */
        hpu = elem.resumeHandler();
        if (print) printf("Resumuing handler @%lu\n",hpu.time);
    }

    /* Reset monitors (to intercept new event of this type) */
    DUMMYCACHE(host, hpu.hid)->resetMonitor(CTRL);
    DUMMYCACHE(host, hpu.hid)->resetMonitor(SIMCALL);

    /* Activate HPU context */
    if (print) printf("ExecuteHandler host: [%d]; hpu.hid: [%d]; hpus: %i (%p)\n", host, hpu.hid, hpus, HPU(host, hpu.hid));
    HPU(host, hpu.hid)->activateContext(0);

    Tick t = curTick();

    while (!curEventQueue()->empty() && !DUMMYCACHE(host, hpu.hid)->monitor(CTRL)){
        curEventQueue()->serviceOne();
 
        /* If I see the SIMCALL set */       
        if (DUMMYCACHE(host, hpu.hid)->monitor(SIMCALL)){
            DUMMYCACHE(host, hpu.hid)->resetMonitor(SIMCALL);

            /* Get SIMCALL info */
            uint32_t syscall_num = *((uint32_t *) DUMMYCACHE(host, hpu.hid)->getPtr(SIMCALL_NUM));
            void * syscall_data = DUMMYCACHE(host, hpu.hid)->getPtr(SIMCALL_DATA); 

            if (print) printf("-- GOT SIMCALL\n");
            if (print) printf("Time true: %lu\n",(curTick() - t)/timemult);
  
            htot = (curTick() - t)/timemult;
                 
            hpu.time=time+htot;
            btime_t simcall_time = hpu.time;    

            if (!simcall(elem, syscall_num, syscall_data)){
                /* Suspend the context */
                if (print) printf("-- Suspending handler @%lu\n",simcall_time);        

                HPU(host, hpu.hid)->suspendContext(0);
                elem.suspendHandler(hpu);
                return HANDLER_SUSPENDED;
            }else{
                /* the last parameter of simcall() is IN/OUT */
                hpu.time = simcall_time;
                time = simcall_time;
                //printf("New Time after symcall : %lu\n",simcall_time);
                t = curTick();
            }
        }
    }
    
    htot = (curTick() - t)/timemult;  
    if (print) printf("Time: %lu\n",htot);

    HPU(host, hpu.hid)->suspendContext(0);
 
    /* add to cpu time to hpu_time */
    hpu.time+=htot;

    /* make this context available */
    nexthpu[host].push(hpu);

    /* this thing of sending hpu descriptors around is a bit dangerous but cool */
    assert(nexthpu[host].size() <= hpus);

    if(!waiting_queue[host].empty()){ //have waiting events 
        gem5SimRequest * waiting_request = waiting_queue[host].front();
        waiting_queue[host].pop();
        waiting_request->time=hpu.time;
        sim.reinsertEvent(waiting_request);
    }

    time = hpu.time; //handlers should be executed sequentially 
 

    /* the handler has been simulated: send payload back */
    if (elem.hasPayload()){
        sim.insertEvent(elem.getPayload());
    }

    return HANDLER_EXECUTED;
} 


/* SIMCALLS */
bool GEM5Mod::simcall(gem5SimRequest& event, uint32_t num, void *data, btime_t& time){
    
    printf("SIMCALL not implemented in this version (yet)!!!\n");

    /* never reached */
    return true;
}


void GEM5Mod::printStatus(){
    printf("HPU times: \n");
    for(unsigned int i=0; i<p; ++i) {
        printf("Host%u: ",i);
        while(!nexthpu[i].empty()){
            hpu_t hpu = nexthpu[i].top();            
            printf("HPU[%u]=%lu \t",hpu.hid,hpu.time);
            nexthpu[i].pop();
        }
        std::cout << "\n";
    }
}


size_t GEM5Mod::maxTime(){
    size_t max = 0;
    for(unsigned int i=0; i<p; ++i){
        while(!nexthpu[i].empty()){
            hpu_t hpu = nexthpu[i].top();
            if(max < hpu.time) max = hpu.time;
            nexthpu[i].pop();
        } 
    }         
    return max;
}

#endif
