
#ifndef __NODEPRINT_HPP__
#define __NODEPRINT_HPP__


 
#include "../../sim.hpp"
#include "../../visualisation/visEvents.hpp"


class NodePrint : public visModule  {

    public:
        NodePrint(){ };

        ~NodePrint(){};

        static int dispatch(visModule* mod, visEvent* ev){

            NodePrint* lmod = (NodePrint*) mod;
            switch (ev->type){
                /* simulation events */
                case VIS_HOST_INST: return lmod->do_nothing(ev);
                case VIS_HOST_DUR:  return  lmod->do_nothing(ev);
                case VIS_HOST_FLOW: return  lmod->do_nothing(ev);
             }
        
            return -1;
        };

        virtual int registerHandlers(Simulator& sim){
             sim.addVisualisationHandler(this, VIS_HOST_INST, NodePrint::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_DUR,  NodePrint::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_FLOW, NodePrint::dispatch);        
            return 0;
        };

    private:
        int do_nothing(visEvent* ev){
            return 0;
        };
};


#endif
