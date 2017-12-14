/*
    It can be used as a template for new visualisation modules
    Or for debugging
*/

#ifndef __EMPTYVIS_HPP__
#define __EMPTYVIS_HPP__


 
#include "../../sim.hpp"
#include "../../visualisation/visEvents.hpp"


class EmptyViz : public visModule  {

    public:
        EmptyViz(){ };

        ~EmptyViz(){};

        static int dispatch(visModule* mod, visEvent* ev){

            EmptyViz* lmod = (EmptyViz*) mod;
            switch (ev->type){
                /* simulation events */
                case VIS_HOST_INST: return lmod->do_nothing(ev);
                case VIS_HOST_DUR:  return  lmod->do_nothing(ev);
                case VIS_HOST_FLOW: return  lmod->do_nothing(ev);
             }
        
            return -1;
        };

        virtual int registerHandlers(Simulator& sim){
             sim.addVisualisationHandler(this, VIS_HOST_INST, EmptyViz::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_DUR,  EmptyViz::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_FLOW, EmptyViz::dispatch);        
            return 0;
        };

    private:
        int do_nothing(visEvent* ev){
            return 0;
        };
};


#endif
