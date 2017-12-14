#ifndef __CHROMEVIS_HPP__
#define __CHROMEVIS_HPP__

 
#include "../../sim.hpp"
#include "../../visualisation/visEvents.hpp"

 
#include <fstream>
#include <iostream>

#include "json.hpp"

using json = nlohmann::json;

//enum unit_t : uint64_t  { milli=0, micro = 1, nano = 1000, pico = 1000000 }; 

class ChromeViz : public visModule  {

    private:

        std::ofstream outfile;
        std::string outfilename;

        uint64_t time_unit_multiplyer;  
        bool time_unit_smaller_than_micro;

        std::map<std::string, int32_t> arc;

        uint32_t max_host_id = 0;
        uint32_t max_tid = 0;

        uint64_t flow_id = 0;

    public:
    

     ChromeViz(std::string outfilename, uint64_t time_unit_multiplyer,bool time_unit_smaller_than_micro ){

            // if time_unit_smaller_than_micro than we multiply by time_unit_multiplyer
            //otherwise we will devide by it
            this->time_unit_smaller_than_micro = time_unit_smaller_than_micro;
            this->time_unit_multiplyer = time_unit_multiplyer;


            this->outfilename = outfilename; 
            outfile.open(outfilename);
            outfile << "{ \"traceEvents\": [\n";

            write_begin_duration_event("dummy",0,0,convert_time(0));
            write_end_duration_event("dummy",0,0,convert_time(0));

        }



        ~ChromeViz(){
            if(outfile.is_open()){
                   json j2 = {
                  {"name", "thread_name"},
                  {"ph", "M"},
                  {"pid", 0},
                  {"tid", 0},
                  {"args", {
                    {"name", "CPU"}
                  }}
                };
                printf("max_host_id: %d\n",max_host_id);
                for(int host = 0; host<=max_host_id; host++)
                {
                    j2["pid"] = host;
                    for (const auto &p : this->arc) {
                        j2["tid"] = p.second;
                        j2["args"]["name"] = p.first;
                        outfile<< std::setw(4) << j2 << ","<<std::endl;
                    }
                   
                }
                // print authors:
                outfile << "{}\n],";
                outfile << "\"Name\": \"Trace of LogGOPSim 0.1\", \n ";
                outfile << "\"Author 0\": \"Salvatore Di Girolamo\", \n ";
                outfile << "\"Author 1\": \"Konstantin Taranov\", \n ";
                outfile << "\"Author 2\": \"Timo Schneider\" \n ";
                outfile << "}\n";
                outfile.close();
            }
        }

        void update_stats(std::string module_name,uint32_t host){
            auto it = arc.find(module_name);
            if (it == arc.end()){
              arc[module_name] = max_tid++;
            }

            max_host_id = std::max(max_host_id,host);

        }

        int add_duration(HostDurationVisEvent ev){
            update_stats(ev.module_name,ev.host);
            write_x_duration_event(ev.event_name, ev.host, arc[ev.module_name],
                                    convert_time(ev.stime),convert_time(ev.etime-ev.stime) );
           
            return 0; 
        }

        int add_instant(HostInstantVisEvent ev){
            update_stats(ev.module_name,ev.host);
            return -1; 
        }


        int add_simple_flow(HostSimpleFlowVisEvent ev){
            update_stats(ev.imodule_name,ev.ihost);
            update_stats(ev.rmodule_name,ev.rhost);
            write_begin_flow_event(ev.event_name, ev.ihost, arc[ev.imodule_name], convert_time(ev.stime), flow_id);
            write_end_flow_event(ev.event_name, ev.rhost, arc[ev.rmodule_name], convert_time(ev.etime),  flow_id);

            flow_id++;

            return 0; 
        }


        static int dispatch(visModule* mod, visEvent* ev){

            ChromeViz* lmod = (ChromeViz*) mod;
            switch (ev->type){
                /* simulation events */
                case VIS_HOST_INST: return lmod->add_instant(*((HostInstantVisEvent *) ev));
                case VIS_HOST_DUR: return lmod->add_duration(*((HostDurationVisEvent *) ev));
                case VIS_HOST_FLOW: return lmod->add_simple_flow(*((HostSimpleFlowVisEvent *) ev));
             }
        
            return -1;
        };

        virtual int registerHandlers(Simulator& sim){
             sim.addVisualisationHandler(this, VIS_HOST_INST, ChromeViz::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_DUR, ChromeViz::dispatch);
             sim.addVisualisationHandler(this, VIS_HOST_FLOW, ChromeViz::dispatch);        
            return 0;
        }

       
    private:

        std::string convert_time(uint64_t time){

            std::string norm_time;
            if (time_unit_smaller_than_micro){
                norm_time = std::to_string(time/time_unit_multiplyer) +  "." + std::to_string(time%time_unit_multiplyer); 
            } 
            else {
                norm_time = std::to_string(time*time_unit_multiplyer);
            }
            return norm_time;

        }
    
        void write_x_duration_event(std::string name, uint32_t host, uint32_t elem, std::string time,std::string dur ){
            json j;
            j["name"] = name;
            j["ph"] = "X";
            j["dur"] = dur;
            j["pid"] = host;
            j["tid"] = elem;
            j["ts"] = json::parse(time);
            outfile<< std::setw(4) << j << ","<<std::endl;
        }

        void write_begin_duration_event(std::string name, uint32_t host, uint32_t elem, std::string time){
            json j;
            j["name"] = name;
            j["ph"] = "B";
            j["pid"] = host;
            j["tid"] = elem;
            j["ts"] = json::parse(time);
            outfile<< std::setw(4) << j << ","<<std::endl;
        }

        void write_end_duration_event(std::string name, uint32_t host, uint32_t elem, std::string time){
          json j;
            j["name"] = name;
            j["ph"] = "E";
            j["pid"] = host;
            j["tid"] = elem;
            j["ts"] = json::parse(time);
            outfile<< std::setw(4) << j << ","<<std::endl;
        }


        void write_begin_flow_event(std::string name, uint32_t source, uint32_t source_elem, std::string time, uint64_t id){
            json j;
            j["name"] = name;
            j["ph"] = "s";
            j["id"] = id;
            j["pid"] = source;
            j["tid"] = source_elem;
            j["ts"] = json::parse(time);
            outfile<< std::setw(4) << j << ","<<std::endl;
        }

        void write_end_flow_event(std::string name, uint32_t dest, uint32_t dest_elem, std::string time, uint64_t id){
            json j;
            j["cat"] = "test,cat";
            j["name"] = name;
            j["ph"] = "f";
            j["id"] = id;
            j["pid"] = dest;
            j["tid"] = dest_elem;
            j["ts"] = json::parse(time);
            outfile<< std::setw(4) << j << ","<<std::endl;
        }

 
};


#endif