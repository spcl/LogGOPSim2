#ifndef __CHROMEVIS_HPP__
#define __CHROMEVIS_HPP__

 
#include "../../sim.hpp"
#include "../../visualisation/visEvents.hpp"

 
#include <fstream>
#include <iostream>

#include "json.hpp"

using json = nlohmann::json;

enum unit_t : uint64_t  { milli=0, micro = 1, nano = 1000, pico = 1000000 }; 

class ChromeViz : public visModule  {

    private:

        std::ofstream outfile;
        std::string outfilename;

        uint64_t unit_time_convert;  
        bool unit_time_convert_smaller_than_micro;

        std::map<std::string, int32_t> arc;

        uint64_t flow_id = 0;

    public:
        ChromeViz()
        {
            ChromeViz("default.json");  
        };

        ChromeViz(std::string outfilename){
            this->outfilename = outfilename; 
            outfile.open(outfilename);
            outfile << "[\n";
            unit_time_convert_smaller_than_micro = true;
            unit_time_convert = unit_t::micro;
            this->arc = { {"CPU", 0}, {"NIC", 1} };

            write_begin_duration_event("DUMP",0,arc["CPU"],convert_time(0));
            write_end_duration_event("DUMP",0,arc["CPU"],convert_time(0));


        }




        ChromeViz(std::string outfilename, enum unit_t unit):ChromeViz(outfilename){
             
            if(unit == unit_t::milli){
                unit_time_convert_smaller_than_micro = false;
                unit_time_convert = 1000;
            }
            else {
                unit_time_convert_smaller_than_micro = true;
                unit_time_convert = unit;
            }
        }


        ChromeViz(std::string outfilename, uint64_t unit_time_convert,bool unit_time_convert_smaller_than_micro ):ChromeViz(outfilename){
            this->unit_time_convert_smaller_than_micro = unit_time_convert_smaller_than_micro;
            this->unit_time_convert = unit_time_convert;
        }


        ChromeViz(std::string outfilename, 
                 uint64_t unit_time_convert,
                 bool unit_time_convert_smaller_than_micro, 
                 uint32_t hpu_number
                ) : ChromeViz(outfilename,unit_time_convert,unit_time_convert_smaller_than_micro) {
            
            if(hpu_number == 0){
                 this->arc = { {"CPU", 0}, {"NIC", 1} };
            }
            else{
                 this->arc = { {"CPU", 0}, {"MEM", 1}, {"NIC", 2+hpu_number} };
                 for(int i = 0; i<hpu_number; i++){
                    this->arc["HPU"+std::to_string(i)] = 1 + hpu_number - i;
                 }
            }

            json j2 = {
              {"name", "thread_name"},
              {"ph", "M"},
              {"pid", 0},
              {"tid", 0},
              {"args", {
                {"name", "CPU"}
              }}
            };
            for(int host = 0; host<10; host++)
            {
                j2["pid"] = host;
                for (const auto &p : this->arc) {
                    j2["tid"] = p.second;
                    j2["args"]["name"] = p.first;
                    outfile<< std::setw(4) << j2 << ","<<std::endl;
                }
               
            }
            

        }


        ~ChromeViz(){
            if(outfile.is_open()){
                outfile << "{}\n]\n";
                outfile.close();
            }
        }

        int add_duration(HostDurationVisEvent ev){
            write_x_duration_event(ev.event_name, ev.host, arc[ev.module_name],
                                    convert_time(ev.stime),convert_time(ev.etime-ev.stime) );
           
            return 0; 
        }

        int add_instant(HostInstantVisEvent ev){

            return -1; 
        }


        int add_simple_flow(HostSimpleFlowVisEvent ev){

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


        //FLOWs
        void add_simple_transmission(uint32_t source, uint32_t dest, uint64_t start_time, uint64_t end_time){


            write_begin_flow_event("Latency", source, arc["NIC"], convert_time(start_time), flow_id);
            write_end_flow_event("Latency", dest, arc["NIC"], convert_time(end_time),  flow_id);

            flow_id++;
        }


        void add_transmission(uint32_t source, uint32_t dest, uint64_t start_time, uint64_t end_time, uint32_t size, uint32_t G){

       
            write_begin_flow_event("Latency", source, arc["NIC"], convert_time(start_time), flow_id);
            write_end_flow_event("Latency", dest, arc["NIC"], convert_time(end_time),  flow_id);

            flow_id++;
        }

        // CPU 
        void add_cpuop(std::string name, uint32_t host, uint64_t start_time, uint64_t end_time){

            write_x_duration_event(name,host,arc["CPU"],convert_time(start_time),convert_time(end_time-start_time));
           // write_end_duration_event(name,host,arc["CPU"],convert_time(end_time));
        }
       
        // NIC
        void add_nicop(std::string name, uint32_t host, uint64_t start_time, uint64_t end_time){
              write_x_duration_event(name,host,arc["NIC"],convert_time(start_time),convert_time(end_time-start_time));
           // write_begin_duration_event(name,host,arc["NIC"],convert_time(start_time));
          //  write_end_duration_event(name,host,arc["NIC"],convert_time(end_time));
        }

        // HPUs
        void add_ohpu(std::string name, uint32_t host, uint64_t start_time, uint64_t end_time, uint32_t hpu)
        {
              write_x_duration_event(name,host,arc["HPU"+std::to_string(hpu)],convert_time(start_time),convert_time(end_time-start_time));
           // write_begin_duration_event(name,host,arc["HPU"+std::to_string(hpu)],convert_time(start_time));
           // write_end_duration_event(name,host,arc["HPU"+std::to_string(hpu)],convert_time(end_time));
        }

        // DMA
        void add_odma(std::string name, uint32_t host, uint64_t start_time, uint64_t end_time){
            write_begin_duration_event(name,host,arc["MEM"],convert_time(start_time));
            write_end_duration_event(name,host,arc["MEM"],convert_time(end_time));
        }   

       
    private:

        std::string convert_time(uint64_t time){

            std::string norm_time;
            if (unit_time_convert_smaller_than_micro){
                norm_time = std::to_string(time/unit_time_convert) +  "." + std::to_string(time%unit_time_convert); 
            } 
            else {
                norm_time = std::to_string(time*unit_time_convert);
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