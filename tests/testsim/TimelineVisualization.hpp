#include <string>
#include <math.h>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <assert.h>
#include <vector>

//#define P4EXT

class TimelineVisualization {

	private:
	std::string content;
  bool enable;
	std::string filename;

    int _todraw = -2;


    bool todraw(int sender, int receiver){
        return _todraw==-2 || (sender==_todraw || receiver==_todraw);
    }
    bool todraw(int host){
        return todraw(host, -1);
    }

  void add_ranknum(int numranks) {

    std::stringstream os;
    os << "numranks " << numranks << ";\n";
    this->content.append(os.str());

  }

  void write_events(bool append) {

    std::ofstream myfile;
    if (append) myfile.open(filename.c_str(), std::ios::out | std::ios::app);
    else myfile.open(filename.c_str(), std::ios::out);
    if (myfile.is_open()) {
      myfile << this->content;
      myfile.close();
    }
    else {
      fprintf(stderr, "Unable to open %s\n", filename.c_str());
    }

  }


  void tag2color(int tag, float * r, float * g, float * b){
    float h = (tag*40) % 350;
    float s = (float) ((tag*20) % 100) / 100;
    float v = (float) ((tag*30) % 100) / 100;
    if (v<=0.4) v=0.4; 

    int i;
    float f, p, q, t;

    if( s == 0 ) {
        // achromatic (grey)
        *r = *g = *b = v;
        return;
    }

    h /= 60;            // sector 0 to 5
    i = floor( h );
    f = h - i;          // factorial part of h
    p = v * ( 1 - s );
    q = v * ( 1 - s * f );
    t = v * ( 1 - s * ( 1 - f ) );

    switch( i ) {
        case 0:
            *r = v;
            *g = t;
            *b = p;
            break;
        case 1:
            *r = q;
            *g = v;
            *b = p;
            break;
        case 2:
            *r = p;
            *g = v;
            *b = t;
            break;
        case 3:
            *r = p;
            *g = q;
            *b = v;
            break;
        case 4:
            *r = t;
            *g = p;
            *b = v;
            break;
        default:        // case 5:
            *r = v;
            *g = p;
            *b = q;
            break;
    }

    //printf("tag: %i; h: %f; s: %f; v: %f; r: %f; g: %f; b: %f\n", tag, h, s,v , *r, *g, *b);
/*
    *r = (*r)/255;
    *g = (*g)/255;
    *b = (*b)/255;
*/

  }


	public:

  TimelineVisualization(gengetopt_args_info *args_info, int p) : enable(enable) {
    enable = args_info->vizfile_given;
    if(!enable) return;

    filename = args_info->vizfile_arg;
    add_ranknum(p);
  }

  ~TimelineVisualization() {
    if(!enable) return;

    write_events(false);
  }
 void set_to_draw(int host){
    _todraw = host;
 }


  void add_osend(int rank, int start, int end, int cpu, int tag){
    float r,g,b;
    tag2color(tag, &r, &g, &b);
    add_osend(rank, start, end, cpu, r, g, b);
  }

  void add_orecv(int rank, int start, int end, int cpu, int tag){
    float r,g,b;
    tag2color(tag, &r, &g, &b);
    add_orecv(rank, start, end, cpu, r, g, b);
  }



  void add_osend(int rank, int start, int end, int cpu, float r=0.0, float g=0.0, float b=1.0) {
    if(!enable || !todraw(rank)) return;

    std::stringstream outstream;
    outstream << "osend " << rank << " " << cpu << " " << start << " " << end << " " << r << " " << g << " " << b << ";\n";
    this->content.append(outstream.str());

  }

  void add_orecv(int rank, int start, int end, int cpu, float r=0.6, float g=0.5, float b=0.3) {
    if(!enable || !todraw(rank)) return;

    std::stringstream os;
    os << "orecv " << rank << " " << cpu << " " << start << " " << end << " " << r << " " << g << " " << b << ";\n";
    this->content.append(os.str());

  }

  void add_loclop(int rank, int start, int end, int cpu, float r=1.0, float g=0.0, float b=0.0) {
    if(!enable) return;

    std::stringstream os;
    os << "loclop " << rank << " " << cpu << " " << start << " " << end << " " << r << " " << g << " " << b << ";\n";
    this->content.append(os.str());

  }

 #ifdef P4EXT
 void add_nicop(int rank, int start, int end, int nic, float r=1.0, float g=0.5, float b=0.0) {
    if(!enable) return;

    std::stringstream os;
    os << "nicop " << rank << " " << nic << " " << start << " " << end << " " << r << " " << g << " " << b << ";\n";
    this->content.append(os.str());
}
 #endif // P4EXT

  void add_noise(int rank, int start, int end, int cpu, float r=0.0, float g=1.0, float b=0.0) {
    if(!enable) return;

    std::stringstream os;
    os << "noise " << rank << " " << cpu << " " << start << " " << end << " " << r << " " << g << " " << b << ";\n";
    this->content.append(os.str());

  }

  void add_transmission(int source, int dest, int starttime, int endtime, int size, int G, float r=0.0, float g=0.0, float b=1.0) {
    if(!enable || !todraw(source, dest)) return;

    std::stringstream os;
    os << "transmission " << source << " " << dest << " " << starttime << " ";
    os << endtime << " " << size << " " << G <<  " " << r << " " << g << " " << b << ";\n";
    this->content.append(os.str());
  }
};
