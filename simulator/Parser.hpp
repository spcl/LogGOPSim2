#ifndef __PARSER_H__
#define __PARSER_H__

#include <map>
#include <vector>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <algorithm>



#include "simEvents.hpp"

#define MAGIC_COOKIE 4223
#define MAGIC_COOKIE_INVALID 2342


#define OPTYPE_SEND 1
#define OPTYPE_RECV 2
#define OPTYPE_CALC 3
#define OPTYPE_PUT 4
#define OPTYPE_GET 5
#define OPTYPE_APPEND 6
#define OPTYPE_CTWAIT 7
#define OPTYPE_TPUT 8
#define OPTYPE_TGET 9
#define OPTYPE_TAPPEND 10
#define OPTYPE_GEM5CALC 11



typedef uint64_t base_t;

// TODO Experiment with packing of the node structure,
//      that could save us up to 30% in space, but might
//      harm time.

struct Node {

    uint64_t           Size;
    std::vector<Node*> DependOnMe;
    std::vector<Node*> StartDependOnMe;
    uint32_t           DependenciesCnt;
    uint32_t           offset;
    uint32_t           Peer;
    uint32_t           Tag;
    uint8_t            Proc;
    uint8_t            Nic;
    char               Type;
    char               options;
    uint32_t           ct;
    uint64_t           threshold;
    uint32_t           oct;
    uint32_t           hh; /* header handler for append or arg1 for put*/
    uint32_t           ph; /* payload handler or arg2 for put*/
    uint32_t           ch; /* completion handler or arg3 for put*/
    uint32_t           mem; /*mem id for sharing memory or arg4 for put*/
    uint64_t           arg[4];

};

struct DeserializedNode {

    uint32_t           DependenciesCnt;
    char               Type;
    uint32_t           Peer;
    uint64_t           Size;
    uint32_t           Tag;
    uint8_t            Proc;
    uint8_t            Nic;
    uint32_t           offset;
    std::vector<uint32_t> DependOnMe;
    std::vector<uint32_t> StartDependOnMe;
    char               options;
    uint32_t           ct;
    uint64_t           threshold;
    uint32_t           oct;
    uint32_t           hh; /* header handler for append or arg1 for put*/
    uint32_t           ph; /* payload handler or arg2 for put*/
    uint32_t           ch; /* completion handler or arg3 for put*/
    uint32_t           mem; /*mem id for sharing memory or arg4 for put*/
    uint64_t           arg[4];
    uint64_t           time;
};

class Graph {

    private:

    std::vector<Node*> RootNodes;
    std::vector<Node*> allNodes;

    //uint32_t num_nodes;
    uint32_t num_edges;
    uint32_t offset_cntr;

    char* mapping_start;

/*
    uint64_t get_file_size(int fd) {

        struct stat f_info;
        int r = fstat(fd, &f_info);
        assert(r == 0);
        return f_info.st_size;
    }
*/

    void find_root_nodes() {

        RootNodes.clear();

        for (std::vector<Node*>::iterator it =  allNodes.begin(); it != allNodes.end(); it++) {
            if ((**it).DependenciesCnt == 0) RootNodes.push_back(*it);
        }

    }

    public:

    ~Graph() {
        for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
            delete *it;
        }
    }

    Graph() {
    //  num_nodes = 0;
        num_edges = 0;
        offset_cntr = 0;
    }

    inline Node* addNode() {

        /**
            add a new node to the graph
        */

        Node* N = new Node;
        N->DependenciesCnt = 0;
        N->offset = offset_cntr;
        offset_cntr++;
        allNodes.push_back(N);

        return N;
    }

    inline void addDependency(Node* a, Node* b) {

        /**
           addDependency(a,b) means that a can not be
           executed before b is finished
        */

        b->DependOnMe.push_back(a);
        a->DependenciesCnt++;
        num_edges++;
    }

    inline void addStartDependency(Node* a, Node *b) {

        /**
           addStartDependency(a,b) means that a can not be
           executed before b is started
        */

        b->StartDependOnMe.push_back(a);
        a->DependenciesCnt++;
        num_edges++;
    }

    void write_as_dot() {
        /**
            Produces a dot representation of the graph. This is usefull for debugging purposes.
        */

        FILE* fd = fopen("graph.dot", "w");
        assert(fd != NULL);
        fprintf(fd, "digraph mygraph {\n");
        fprintf(fd, "graph [rankdir=LR];\n");
        fprintf(fd, "node [shape=record];\n");

            for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
                char typestr[5];
                if ((**it).Type == OPTYPE_SEND) strcpy(typestr, "Send");
                else if ((**it).Type == OPTYPE_RECV) strcpy(typestr, "Recv");
                else if ((**it).Type == OPTYPE_CALC) strcpy(typestr, "Calc");
                else strcpy(typestr, "Unkn");
                fprintf(fd, "%i [label=\"<f0> Type: %s | <f1> Peer: %i | <f2> Size: %lu | <f3> Tag: %i | <f4> Proc: %i | <f5> Nic: %i \"]\n",
                             (**it).offset,    typestr,        (**it).Peer, (unsigned long int) (**it).Size,  (**it).Tag  , (**it).Proc,     (**it).Nic);
            }

            for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
                for (std::vector<Node*>::iterator dit = (**it).DependOnMe.begin(); dit != (**it).DependOnMe.end(); dit++) {
                    fprintf(fd, "%i:f0 -> %i:f0\n", (**it).offset, (**dit).offset );
                }
                for (std::vector<Node*>::iterator dit = (**it).StartDependOnMe.begin(); dit != (**it).StartDependOnMe.end(); dit++) {
                    fprintf(fd, "%i:f0 -> %i:f0 [arrowhead=diamond]\n", (**it).offset, (**dit).offset );
                }
            }

        fprintf(fd, "} \n");

    }

/*
    void serialize(FILE* fd, uint32_t rank, uint32_t num_ranks) {

        uint32_t buf32;

        rewind(fd);

        if (rank == 0) {
            // make room for jumptable
            fseek(fd, sizeof(uint64_t)*2*num_ranks + sizeof(uint32_t), SEEK_SET);
        }
        else {
            // jump to the end of the file
            fseek(fd, sizeof(uint32_t) + (2*(rank-1)+1)*sizeof(uint64_t), SEEK_SET);
            uint64_t eos;
            fread(&eos, sizeof(uint64_t), 1, fd);
            fseek(fd, eos, SEEK_SET);
        }

        find_root_nodes();

        long start = ftell(fd);
        // appendix starts right after all nodes
        long pos_in_appendix = start +
                               sizeof(uint32_t)*2 + // for Nodecount and Independent Actions count
                               sizeof(uint32_t)*RootNodes.size() + // for indp. actions offsets
                               (sizeof(char) + sizeof(uint8_t)*2 + sizeof(uint32_t)*7 + sizeof(uint64_t)) * allNodes.size(); // for actual nodeinfo
        // number of elements in the appendix so far
        uint32_t num_in_appendix = 0;

        // number of nodes in the schedule
        buf32 = (uint32_t) allNodes.size();
        fwrite( &buf32, sizeof(uint32_t), 1, fd );


        // write independant actions offsets
        buf32 = RootNodes.size();
        fwrite(&buf32, sizeof(uint32_t), 1, fd); // number of independent actions
        for (std::vector<Node*>::iterator it = RootNodes.begin(); it != RootNodes.end(); it++) {
            buf32 = (**it).offset;
            fwrite(&buf32, sizeof(uint32_t), 1, fd); // offset of independent action
        }


        Node n;
        int cnt = 0;
        for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {

            cnt++;

            // write the actual operation info - this is a bit complicated thanks to padding that the compiler might do...
            fwrite( &((**it).DependenciesCnt), sizeof(uint32_t), 1, fd );
            fwrite( &((**it).Type), sizeof(char), 1, fd );
            fwrite( &((**it).Peer), sizeof(uint32_t), 1, fd );
            fwrite( &((**it).Size), sizeof(uint64_t), 1, fd );
            fwrite( &((**it).Tag), sizeof(uint32_t), 1, fd );
            fwrite( &((**it).Proc), sizeof(uint8_t), 1, fd );
            fwrite( &((**it).Nic), sizeof(uint8_t), 1, fd );

            // handling dependencies
            buf32 = (**it).DependOnMe.size();
            fwrite( &buf32, sizeof(uint32_t), 1, fd ); // number of ops depending on this node

            if ((**it).DependOnMe.size() > 0) {
                fwrite( &num_in_appendix, sizeof(uint32_t), 1, fd ); // start of dependant offsets in appendix
            }
            else {
                uint32_t b = 999;
                fwrite( &b, sizeof(uint32_t), 1, fd ); // undefined
            }

            long pos = ftell(fd);
            // write offsets of depending ops into appendix
            fseek(fd, pos_in_appendix, SEEK_SET);
            for (std::vector<Node*>::iterator dit = (**it).DependOnMe.begin(); dit != (**it).DependOnMe.end(); dit++) {
                fwrite( &((**dit).offset), sizeof(uint32_t), 1, fd );
            }
            // adjust number of elements and position in appendix
            num_in_appendix += (**it).DependOnMe.size();
            pos_in_appendix = ftell(fd);
            // jump back to the node info (we're in appendix right now)
            fseek(fd, pos, SEEK_SET);

            // handling start-dependencies
            buf32 = (**it).StartDependOnMe.size();
            fwrite( &buf32, sizeof(uint32_t), 1, fd ); // number of ops start-depending on this node
            if ((**it).StartDependOnMe.size() > 0) {
                fwrite( &num_in_appendix, sizeof(uint32_t), 1, fd ); // start of start-dependant offsets in appendix
            }
            else {
                uint32_t b = -1;
                fwrite( &b, sizeof(uint32_t), 1, fd ); // undefinedx
            }
            pos =  ftell(fd);
            // write offsets of start-depending ops into appendix
            fseek(fd, pos_in_appendix, SEEK_SET);
            for (std::vector<Node*>::iterator dit = (**it).StartDependOnMe.begin(); dit != (**it).StartDependOnMe.end(); dit++) {
                fwrite( &((**dit).offset), sizeof(uint32_t), 1, fd );
            }
            // adjust number of elements and position in appendix
            num_in_appendix += (**it).StartDependOnMe.size();
            pos_in_appendix = ftell(fd);
            // jump back to the node info (we're in appendix right now)
            fseek(fd, pos, SEEK_SET);

        }

        // write jumptable info: the schedule started at start and ended at pos_in_appendix
        fseek(fd, 0, SEEK_SET);

        // the number of ranks in this file
        buf32 = num_ranks;
        fwrite(&buf32, sizeof(uint32_t), 1, fd);
        fseek(fd, sizeof(uint64_t)*2*rank + sizeof(uint32_t), SEEK_SET);
        uint64_t buf64 = start;
        fwrite(&buf64, sizeof(uint64_t), 1, fd);
        buf64 = pos_in_appendix;
        fwrite(&buf64, sizeof(uint64_t), 1, fd);
    }

*/

    void serialize_mmap(int fd, uint32_t rank, uint32_t num_ranks, uint8_t max_cpu, uint8_t max_nic) {

        char *start_rankdata;
        uint64_t end_of_lastrank;
        static uint64_t filesize;
        char *pos;

        find_root_nodes();

        if (rank == 0) {
            // calculate the size of the file
            filesize = 0;
            filesize += sizeof(uint64_t); // magic cookie
            filesize += sizeof(uint32_t); // num ranks
            filesize += sizeof(uint8_t); // max_cpu
            filesize += sizeof(uint8_t); // max_nic
            filesize += sizeof(uint64_t)*2*num_ranks; // jumptable
            filesize += sizeof(uint32_t); // num nodes
            filesize += sizeof(uint32_t); // num indp actions
            filesize += (sizeof(uint32_t)*RootNodes.size()); // rootnodes offsets
            //filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t))*allNodes.size(); // nodeinfo
            filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t)/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/)*allNodes.size(); // nodeinfo
            filesize += (sizeof(uint32_t)*num_edges); //appendix

            // enlarge the file
            lseek(fd, filesize-1, SEEK_SET);
            int r = write(fd, "", 1);
            assert(r == 1);

            // mmap the file
            mapping_start = (char*) mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mapping_start == MAP_FAILED) {
                perror("couldn't mmap the output file");
                exit(EXIT_FAILURE);
            }
            *( (uint64_t*) mapping_start ) = (uint64_t) MAGIC_COOKIE;
            mapping_start += sizeof(uint64_t); // jump over magic cookie
            end_of_lastrank = sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*num_ranks;
            start_rankdata = mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*num_ranks;  // our rankdata starts right after the jumptable

        }
        else {
            filesize += sizeof(uint32_t); // num nodes
            filesize += sizeof(uint32_t); // num indp actions
            filesize += sizeof(uint32_t)*RootNodes.size(); // rootnodes offsets
            //filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t))*allNodes.size(); // nodeinfo
            filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t)/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/)*allNodes.size(); // nodeinfo
            filesize += sizeof(uint32_t)*num_edges; //appendix


            // enlarge the file
            lseek(fd, filesize-1, SEEK_SET);
            int r = write(fd, "", 1);
            assert(r == 1);

            // mmap the file
            mapping_start = (char*) mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mapping_start == MAP_FAILED) {
                perror("mmap failed");
                exit(EXIT_FAILURE);
            }

            mapping_start += sizeof(uint64_t); // jump over magic cookie

            end_of_lastrank = *((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*(2*(rank-1)+1)));
            start_rankdata = mapping_start + end_of_lastrank;
        }

        pos = start_rankdata;
        uint32_t num_in_appendix = 0;

        *((uint32_t*) pos) = (uint32_t) allNodes.size();        pos += sizeof(uint32_t);    // number of nodes in the schedule
        *((uint32_t*) pos) = (uint32_t) RootNodes.size();       pos += sizeof(uint32_t);    // number of independent actions

        // independent action offsets
        for (std::vector<Node*>::iterator it = RootNodes.begin(); it != RootNodes.end(); it++) {
            *((uint32_t*) pos) = (**it).offset; pos += sizeof(uint32_t);
        }

        // node data

        for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {

            *((uint32_t*) pos) = (**it).DependenciesCnt;        pos += sizeof(uint32_t);    // number of actions this action depends on
            *((char*) pos)     = (**it).Type;                   pos += sizeof(char);        // type of this action
            *((uint32_t*) pos) = (**it).Peer;                   pos += sizeof(uint32_t);    // peer of this action
            *((uint64_t*) pos) = (**it).Size;                   pos += sizeof(uint64_t);    // size of the data transfer / length of the local calculation
            *((uint32_t*) pos) = (**it).Tag;                    pos += sizeof(uint32_t);    // tag of the send/recv operation
            *((uint8_t*) pos)  = (**it).Proc;                   pos += sizeof(uint8_t);     // processor used for this action
            *((uint8_t*) pos)  = (**it).Nic;                    pos += sizeof(uint8_t);     // network interface used for this action
            *((uint32_t*) pos) = (**it).DependOnMe.size();      pos += sizeof(uint32_t);    // number of actions that depend on this actions termination
            *((uint32_t*) pos) = num_in_appendix;               pos += sizeof(uint32_t);    // start index of dependent actions (in appendix)
            num_in_appendix += (**it).DependOnMe.size();
            *((uint32_t*) pos) = (**it).StartDependOnMe.size(); pos += sizeof(uint32_t);    // number of actions that depend on this actions start
            *((uint32_t*) pos) = num_in_appendix;               pos += sizeof(uint32_t);    // start index of start-dependent actions (in appendix)
            num_in_appendix += (**it).StartDependOnMe.size();
            //printf("[salvo][serializing] option field is in pos: %ld\n", pos - mapping_start);
            *((char*) pos)     = (**it).options;                pos += sizeof(char);        //options
            *((uint32_t*) pos) = (**it).ct;                     pos += sizeof(uint32_t);    //counter id
            *((uint64_t*) pos) = (**it).threshold;              pos += sizeof(uint64_t);    //threshold
            *((uint32_t*) pos) = (**it).oct;                    pos += sizeof(uint32_t);    //owned counter

            //printf("[salvo][serializing] hh field is in pos: %ld\n", pos - mapping_start);

            *((uint32_t*) pos) = (**it).hh;                    pos += sizeof(uint32_t);    
            *((uint32_t*) pos) = (**it).ph;                    pos += sizeof(uint32_t);    
            *((uint32_t*) pos) = (**it).ch;                    pos += sizeof(uint32_t);
            *((uint32_t*) pos) = (**it).mem;                   pos += sizeof(uint32_t);  

            *((uint64_t*) pos) = (**it).arg[0];                   pos += sizeof(uint64_t);  
            *((uint64_t*) pos) = (**it).arg[1];                   pos += sizeof(uint64_t);  
            *((uint64_t*) pos) = (**it).arg[2];                   pos += sizeof(uint64_t);  
            *((uint64_t*) pos) = (**it).arg[3];                   pos += sizeof(uint64_t);  

        }

        // appendix data
        //printf("[salvo][serializing] appendix is in pos: %ld\n", pos - mapping_start);

        for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
            for (std::vector<Node*>::iterator dit = (**it).DependOnMe.begin(); dit != (**it).DependOnMe.end(); dit++) {
                *((uint32_t*) pos) = (**dit).offset;            pos += sizeof(uint32_t);    // offset of dependent action
            }
            for (std::vector<Node*>::iterator dit = (**it).StartDependOnMe.begin(); dit != (**it).StartDependOnMe.end(); dit++) {
                *((uint32_t*) pos) = (**dit).offset;            pos += sizeof(uint32_t);    // offset of start-dependent action
            }
        }

        // jumptable info
        *((uint32_t*) mapping_start) = num_ranks;                                                       // number of ranks in this schedule-file

        *((uint8_t*) (mapping_start + sizeof(uint32_t) )) = max_cpu;    // minimal number of cpu required to simulate this schedule
        *((uint8_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t) )) = max_nic;  // minimal number of nics required to simulate this schedule

        *((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*rank)) = end_of_lastrank;  // start of this ranks info
        *((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*(2*rank+1))) = pos - mapping_start;          // end of this ranks info

        //printf("s: %llu e: %llu\n", (long long unsigned int) end_of_lastrank, (long long unsigned int) (pos - mapping_start));

        // munmap the files so that the contents get written
        int r = munmap(mapping_start-sizeof(uint64_t), (size_t) (pos - mapping_start));
        assert(r == 0);
    }

};

class SerializedGraph {

    private:

    char* mapping_start; // argh. this should be void* but c++ is anal on void*-arithmetic
    uint32_t num_root_nodes;
    uint32_t num_nodes;
    uint32_t num_ranks_in_schedule;
    uint32_t my_rank;

    char * _map_start;

    std::vector<DeserializedNode> executableNodes;



    void add_root_nodes() {

        uint32_t num_root_nodes = (uint32_t) *( (uint32_t*) (mapping_start + sizeof(uint32_t)) );

        for (uint32_t cnt=0; cnt<num_root_nodes; cnt++) {

             //printf("[timos] trying to get root node number %i\n", cnt);
            uint32_t offset = (uint32_t) *( (uint32_t*) (mapping_start + sizeof(uint32_t)*2 + cnt*sizeof(uint32_t)) );
            //printf("[timos] is's offset is %i\n", offset);
            DeserializedNode N = get_node_by_offset(offset);
            executableNodes.push_back(N);
        }

    }

    DeserializedNode get_node_by_offset(uint32_t offset, uint64_t time=0) {

        //printf("[timos] trying to get node with offset %i\n", offset);

        uint32_t num_nodes = (uint32_t) *((uint32_t*) mapping_start);

        if (offset > num_nodes) {
            fprintf(stderr, "[rank %i] got offset %i, have %i nodes\n", my_rank, offset, num_nodes);
            exit(EXIT_FAILURE);
        }
        // printf("yyy 1\n");
        //int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2;
        int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/;
        char* start_of_node = mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*offset;
        DeserializedNode N;
        // printf("yyy 2\n");

        N.time=time;

        N.DependenciesCnt = (uint32_t) *( (uint32_t*) start_of_node);
        N.Type = (char) *(start_of_node + sizeof(uint32_t)); // after depcnt
        N.Peer = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(uint32_t) + sizeof(char)) ); // after depcnt + type

        N.Size = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t)));
        N.Tag = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t) + sizeof(uint64_t)));
        N.Proc = (uint8_t) *( (uint8_t*) (start_of_node + sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t)));
        N.Nic = (uint8_t) *( (uint8_t*) (start_of_node + sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint8_t)));
        N.offset = (uint32_t) offset;
        uint32_t num_deps =                      (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*3 + sizeof(uint8_t)*2));
        uint32_t deps_startoffset_in_apdx =      (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*4 + sizeof(uint8_t)*2));
        uint32_t num_startdeps =                 (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*5 + sizeof(uint8_t)*2));
        uint32_t startdeps_startoffset_in_apdx = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*6 + sizeof(uint8_t)*2));
        //printf("[salvo][deserializing] option field should be in pos: %ld\n", (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2) - _map_start);
        N.options =  (char) *( (char*) (start_of_node + sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2));
        N.ct = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2));
        N.threshold = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t) + sizeof(uint32_t)*8 + sizeof(uint8_t)*2));
        N.oct = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*8 + sizeof(uint8_t)*2));
        //printf("[salvo][deserializing] hh field should be in pos: %ld\n", (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*9 + sizeof(uint8_t)*2) - _map_start);

        N.hh = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*9 + sizeof(uint8_t)*2));
        N.ph = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*10 + sizeof(uint8_t)*2));
        N.ch = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*11 + sizeof(uint8_t)*2));
        N.mem = (uint32_t) *( (uint32_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*12 + sizeof(uint8_t)*2));

        N.arg[0] = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*2 + sizeof(uint32_t)*13 + sizeof(uint8_t)*2));
        N.arg[1]  = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*3 + sizeof(uint32_t)*13 + sizeof(uint8_t)*2));
        N.arg[2]  = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*4 + sizeof(uint32_t)*13 + sizeof(uint8_t)*2));
        N.arg[3]  = (uint64_t) *( (uint64_t*) (start_of_node + sizeof(char)*2 + sizeof(uint64_t)*5 + sizeof(uint32_t)*13 + sizeof(uint8_t)*2));


        //printf("[Parser] deserializing: type: %c; peer: %u; deps from: %u; dep to: %u\n", N.Type, N.Peer, N.DependenciesCnt, num_deps);

        // printf("yyy 3\n");
        //printf("yyy 3 start of apdx = mapping start + %i\n", sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*num_nodes);
        // printf("yyy 3 numrootnodes = %i, SIZEOFNODEINFO = %i, num_nodes = %i\n", num_root_nodes, SIZEOF_NODE_INFO, num_nodes);
        char* start_of_apdx = mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*num_nodes;
        for (uint32_t cnt=0; cnt<num_deps; cnt++) {
            // printf("yyy 3.5 (%i, %i)\n", num_deps, cnt);
            // printf("yyy (start of appendix: %i, deps startoffset in apdx %i, cnt %i)\n", start_of_apdx, deps_startoffset_in_apdx, cnt);
            uint32_t depnode = (uint32_t) *( (uint32_t*) (start_of_apdx + (deps_startoffset_in_apdx + cnt)*sizeof(uint32_t)));

            //printf("num_root_nodes: %u\n", num_root_nodes);
            //printf("SIZEOF_NODE_INFO: %i\n", SIZEOF_NODE_INFO);
            //printf("num_nodes: %u\n", num_nodes);
            //printf("start of appdx: %lu bytes after mapping_start\n", sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*num_nodes );

            assert(depnode < num_nodes);

            N.DependOnMe.push_back(depnode);
            // printf("yyy 3.75\n");
        }
        // printf("yyy 4\n");
        for (uint32_t cnt=0; cnt<num_startdeps; cnt++) {
            N.StartDependOnMe.push_back((uint32_t) *( (uint32_t*) (start_of_apdx + (startdeps_startoffset_in_apdx + cnt)*sizeof(uint32_t))));
        }
        // printf("yyy 5\n");

        return N;
    }

    public:

    uint32_t GetNumNodes() {
        return this->num_nodes;
    }

    void write_as_dot(char *filename) {
        /**
            Produces a dot representation of the graph. This is usefull for debugging purposes.
        */

        std::vector<DeserializedNode> allNodes;
        std::vector<DeserializedNode> executableNodes;

        executableNodes = GetExecutableNodes_DSN();
        while (executableNodes.size() > 0) {
            for (uint32_t cnt=0; cnt < executableNodes.size(); cnt++) {
                allNodes.push_back(executableNodes[cnt]);
                MarkNodeAsStarted_DSN(executableNodes[cnt]);
                MarkNodeAsDone_DSN(executableNodes[cnt]);
            }
            executableNodes.clear();
            executableNodes = GetExecutableNodes_DSN();
        }

        FILE* fd = fopen(filename, "w");
        assert(fd != NULL);
        fprintf(fd, "digraph mygraph {\n");
        fprintf(fd, "graph [rankdir=LR];\n");
        fprintf(fd, "node [shape=record];\n");

            for (std::vector<DeserializedNode>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
                char typestr[5];
                if ((*it).Type == OPTYPE_SEND) strcpy(typestr, "Send");
                else if ((*it).Type == OPTYPE_RECV) strcpy(typestr, "Recv");
                else if ((*it).Type == OPTYPE_CALC) strcpy(typestr, "Calc");
                else strcpy(typestr, "Unkn");
                fprintf(fd, "%i [label=\"<f0> Type: %s | <f1> Peer: %i | <f2> Size: %llu | <f3> Tag: %i | <f4> Proc: %i | <f5> Nic: %i \"]\n",
                             (*it).offset,    typestr,        (*it).Peer, (unsigned long long) (*it).Size,  (*it).Tag  , (*it).Proc,     (*it).Nic);
            }

            for (std::vector<DeserializedNode>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
                for (std::vector<uint32_t>::iterator dit = (*it).DependOnMe.begin(); dit != (*it).DependOnMe.end(); dit++) {
                    fprintf(fd, "%i:f0 -> %i:f0\n", (*it).offset, (*dit));
                }
                for (std::vector<uint32_t>::iterator dit = (*it).StartDependOnMe.begin(); dit != (*it).StartDependOnMe.end(); dit++) {
                    fprintf(fd, "%i:f0 -> %i:f0 [arrowhead=diamond]\n", (*it).offset, (*dit));
                }
            }

        fprintf(fd, "} \n");

    }

    SerializedGraph(char* map_start, size_t map_length, uint32_t rank) {

         //printf("[timos] Creating graph for rank %i\n", rank);
        _map_start = map_start;
        mapping_start = map_start;
        my_rank = rank;

        uint64_t ssched;

        //printf("xxx 1\n");
        num_ranks_in_schedule = *( ((uint32_t*) mapping_start) );

         //printf("xxx 2\n");
        char* tmp = mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*rank;
        ssched = *( (uint64_t*) tmp); // jumping over num_schedules + max_cpu/max_nic + jumptable

         //printf("xxx 3\n");
        //printf("ssched = %i\n", ssched);

        mapping_start += ssched;
        executableNodes.clear();

         //printf("xxx 4\n");
        num_nodes = *((uint32_t*) mapping_start);
        //printf("rank %u: %u nodes\n", rank, num_nodes);
        //printf("num-nodes: %i\n", num_nodes);
         //printf("xxx 5\n");
        num_root_nodes = *((uint32_t*) (mapping_start+sizeof(uint32_t)));
        //printf("num-root-nodes: %i\n", num_root_nodes);
         //printf("xxx 6\n");
        add_root_nodes();
         //printf("xxx 7\n");

    }

    void MarkNodeAsStarted_DSN(DeserializedNode node) {

        DeserializedNode N = get_node_by_offset(node.offset);
        for (uint32_t cnt=0; cnt<N.StartDependOnMe.size(); cnt++) {
            uint32_t offset = N.StartDependOnMe[cnt];

            int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/;
            uint32_t* dep_cnt = (uint32_t*) (mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*offset);
            (*dep_cnt)--;
            if ((*dep_cnt) == 0) {
                executableNodes.push_back(get_node_by_offset(offset));
            }
        }
    }

    void MarkNodeAsDone_DSN(DeserializedNode node) {

        DeserializedNode N = get_node_by_offset(node.offset);
        for (uint32_t cnt=0; cnt<N.DependOnMe.size(); cnt++) {
            uint32_t offset = N.DependOnMe[cnt];

            int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/;
            uint32_t* dep_cnt = (uint32_t*) (mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*offset);
            (*dep_cnt)--;
            if ((*dep_cnt) == 0) {
                executableNodes.push_back(get_node_by_offset(offset));
            }
        }
    }

    std::vector<DeserializedNode> GetExecutableNodes_DSN() {

        std::vector<DeserializedNode> ret;

        for (uint32_t cnt=0; cnt<executableNodes.size(); cnt++) {
            ret.push_back(executableNodes[cnt]);
        }
        executableNodes.clear();
        return ret;
    }

  typedef std::vector<goalevent*> nodelist_t;


    void GetExecutableNodes(nodelist_t *ret_ptr) {
        nodelist_t& ret = *ret_ptr;

        for (uint32_t cnt=0; cnt<executableNodes.size(); cnt++) {
            goalevent * gp = new goalevent();
            gp->target = executableNodes[cnt].Peer;
            gp->size = executableNodes[cnt].Size;
            gp->tag = executableNodes[cnt].Tag;
            gp->proc = executableNodes[cnt].Proc;
            gp->nic = executableNodes[cnt].Nic;
            gp->oct = executableNodes[cnt].oct;
            gp->ct = executableNodes[cnt].ct;
            gp->threshold = executableNodes[cnt].threshold;
            gp->options = executableNodes[cnt].options;
            gp->hh = executableNodes[cnt].hh;
            gp->ph = executableNodes[cnt].ph;
            gp->ch = executableNodes[cnt].ch;
            gp->mem = executableNodes[cnt].mem;
            gp->arg[0] =  executableNodes[cnt].arg[0];
            gp->arg[1] =  executableNodes[cnt].arg[1];
            gp->arg[2] =  executableNodes[cnt].arg[2];
            gp->arg[3] = executableNodes[cnt].arg[3];

            gp->time = executableNodes[cnt].time;
            /*if (executableNodes[cnt].Type == OPTYPE_SEND) gp.type = OP_SEND;
            else if (executableNodes[cnt].Type == OPTYPE_RECV) gp.type = OP_RECV;
            else if (executableNodes[cnt].Type == OPTYPE_CALC) gp.type = OP_LOCOP;
            */
            switch (executableNodes[cnt].Type){
                case OPTYPE_SEND: gp->type = OP_SEND; break;
                case OPTYPE_RECV: gp->type = OP_RECV; break;
                case OPTYPE_CALC: gp->type = OP_LOCOP; break;
                case OPTYPE_GEM5CALC: gp->type = OP_LOCGEM5OP; break;
                case OPTYPE_APPEND: gp->type = OP_APPEND; break;
                case OPTYPE_TAPPEND: gp->type = OP_TAPPEND; break;
                case OPTYPE_PUT: gp->type = OP_PUT; break;
                case OPTYPE_GET: gp->type = OP_GET; break;
                case OPTYPE_TPUT: gp->type = OP_TPUT; break;
                case OPTYPE_TGET: gp->type = OP_TGET; break;
                case OPTYPE_CTWAIT: gp->type = OP_CTWAIT; break;
            }

            gp->offset = executableNodes[cnt].offset;
            ret.push_back(gp);
        }
        executableNodes.clear();
    }

    int MarkNodeAsStarted(uint32_t offset, uint64_t time=0) {

        DeserializedNode N = get_node_by_offset(offset);
        for (uint32_t cnt=0; cnt<N.StartDependOnMe.size(); cnt++) {
            uint32_t offset = N.StartDependOnMe[cnt];
            assert(offset < num_nodes);

            int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/;
            uint32_t* dep_cnt = (uint32_t*) (mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*offset);
            (*dep_cnt)--;
            if ((*dep_cnt) == 0) {
                executableNodes.push_back(get_node_by_offset(offset, time));
                return 1;
            }
        }
        return 0;
    }

    int MarkNodeAsDone(uint32_t offset, uint64_t time=0) {

        DeserializedNode N = get_node_by_offset(offset);

        
        int check=0;
        //printf("[Parser] Marking node as done: deps to: %u\n", N.DependOnMe.size());
        for (uint32_t cnt=0; cnt<N.DependOnMe.size(); cnt++) {
            uint32_t offset = N.DependOnMe[cnt];

            int SIZEOF_NODE_INFO = sizeof(char) + sizeof(uint64_t) + sizeof(uint32_t)*7 + sizeof(uint8_t)*2/*salvo: */+sizeof(char)+sizeof(uint32_t)*6+sizeof(uint64_t)*5 /* end salvo*/;
            uint32_t* dep_cnt = (uint32_t*) (mapping_start + sizeof(uint32_t)*2 + sizeof(uint32_t)*num_root_nodes + SIZEOF_NODE_INFO*offset);
            (*dep_cnt)--;


            if ((*dep_cnt) == 0) {
                executableNodes.push_back(get_node_by_offset(offset, time));
                check=1;
            }
        }
        return check;
    }

};

class Parser : public IParser {

    private:

    char* mapping_start; // argh. this should be void* but c++ is anal on void*-arithmetic
    size_t mapping_length;
    uint32_t num_ranks_in_schedule;
    uint8_t max_cpu;
    uint8_t max_nic;
    FILE *schedules_fd;


    std::vector<int> check_hosts; 


    uint64_t get_file_size(FILE* fd) {

        struct stat f_info;
        int r = fstat(fileno(fd), &f_info);
        assert(r == 0);
        return f_info.st_size;
    }

    public:

  typedef std::vector<SerializedGraph> schedules_t ;
    schedules_t schedules;

    Parser(char* filename, bool save_mem) {

        schedules_fd = fopen(filename, "r+");

        if (schedules_fd == NULL) {
            fprintf(stderr, "Couldn't open input file %s!\n", filename);
            exit(EXIT_FAILURE);
        }

        uint64_t magic_cookie = 0;

        mapping_length = get_file_size(schedules_fd);
        fread(&magic_cookie, sizeof(uint64_t), 1, schedules_fd);

        if (magic_cookie == MAGIC_COOKIE_INVALID) {
            fprintf(stderr, "This is serialized goal schedule was invalidated by a prior simulation run!\n");
            exit(EXIT_FAILURE);
        }
        if (magic_cookie != MAGIC_COOKIE) {
            fprintf(stderr, "This is not a serialized goal schedule - the magic cookie is missing\n");
            exit(EXIT_FAILURE);
        }

        fread(&num_ranks_in_schedule, sizeof(uint32_t), 1, schedules_fd);
        fread(&max_cpu, sizeof(uint8_t), 1, schedules_fd);
        fread(&max_nic, sizeof(uint8_t), 1, schedules_fd);

        if (save_mem == true) {
            // mmap can fail with map_private and prot_write on machines where the virtual mem is smaller than
            // the mapped region - so we fall back to map_shared. This destroys the schedule, so we invalidate
            // the magic cookie if we do this
            mapping_start = (char*) mmap(NULL, mapping_length, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(schedules_fd), 0);
            *((uint64_t*) mapping_start) = MAGIC_COOKIE_INVALID;
            printf("The schedule will be invalid after this simulation!\n");
        }

        else if (save_mem == false) {
            mapping_start = (char*) mmap(NULL, mapping_length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(schedules_fd), 0);
            // THIS NEEDS MORE MEMORY - but it is also more convinient for interacrive use
            // because it preserves the schedules
            // Note that there is no fall-through to MAP_SHARED, we put the user in charge now!
        }

        if (mapping_start == MAP_FAILED) {
            fprintf(stderr, "mmap does not work on your system! Try to use the --save-mem option.\n");
            exit(EXIT_FAILURE);
        }

        for (uint32_t cnt=0; cnt<num_ranks_in_schedule; cnt++) {
            schedules.push_back(SerializedGraph(mapping_start+sizeof(uint64_t), mapping_length-sizeof(uint64_t), cnt));
        }
    }

    uint8_t GetNumCPU() {
        return max_cpu+1;
    }

    uint8_t GetNumNIC() {
        return max_nic+1;
    }

    virtual uint32_t getHostCount(){
        return this->schedules.size();
    }

    void MarkNodeAsStarted(uint32_t host, uint32_t offset, uint64_t time){
        if (schedules[host].MarkNodeAsStarted(offset, time)){
            check_hosts.push_back(host);
        }
    } 
    
    void MarkNodeAsDone(uint32_t host, uint32_t offset, uint64_t time){
        if (schedules[host].MarkNodeAsDone(offset, time)){
            check_hosts.push_back(host);
        }
    }

    virtual void addRootNodes(ISimulator * sim){

        int host=0;
        int num_events=0;
        for(Parser::schedules_t::iterator sched=this->schedules.begin(); sched!=this->schedules.end(); ++sched, ++host) {
    
            SerializedGraph::nodelist_t free_ops;
            sched->GetExecutableNodes(&free_ops);
            // ensure that the free ops are ordered by type
            std::sort(free_ops.begin(), free_ops.end(), gnp_op_comp_func());

            num_events += sched->GetNumNodes();
            //printf("new sched\n");

            // walk all new free operations and throw them in the queue
            for(SerializedGraph::nodelist_t::iterator freeop=free_ops.begin(); freeop != free_ops.end(); ++freeop) {
                //printf("free op\n");
                (*freeop)->host = host;
                //freeop->time = 0;
#ifdef STRICT_ORDER
                (*freeop)->ts=aqtime++;
#endif
                sim->addEvent(*freeop);
            }
        }
    }

    virtual void addNewNodes(ISimulator * sim){

        int host=0;
        int num_events=0;
        std::sort(check_hosts.begin(), check_hosts.end());
        check_hosts.erase(unique(check_hosts.begin(), check_hosts.end()), check_hosts.end());

        for(std::vector<int>::iterator iter=check_hosts.begin(); iter!=check_hosts.end(); ++iter) {
            host = *iter;
            SerializedGraph *sched= &schedules[host];  
    
            SerializedGraph::nodelist_t free_ops;
            sched->GetExecutableNodes(&free_ops);
            // ensure that the free ops are ordered by type
            std::sort(free_ops.begin(), free_ops.end(), gnp_op_comp_func());

            num_events += sched->GetNumNodes();
            //printf("new sched\n");

            // walk all new free operations and throw them in the queue
            for(SerializedGraph::nodelist_t::iterator freeop=free_ops.begin(); freeop != free_ops.end(); ++freeop) {
                //printf("free op\n");
                (*freeop)->host = host;
                //freeop->time = 0;
#ifdef STRICT_ORDER
                (*freeop)->ts=aqtime++;
#endif
                sim->addEvent(*freeop);
            }
        }

        check_hosts.clear();
    }


    ~Parser() {
        int r = munmap(mapping_start, mapping_length);
        assert(r == 0);
        fclose(schedules_fd);
    }

};

#endif /* __PARSER_H__ */

