// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <set>
#include <pthread.h>
#include "extent_protocol.h"

class extent_server {

  typedef struct{
	std::string value;
	extent_protocol::attr attr;
	//for dir
	//std::map<usinged long long,std::map<extent_protocol::extentid_t,extent_data_t>::iterator *> dir;
	//std::set<unsigned long long> dir;//pair ()
  } extent_data_t;
  typedef std::map<extent_protocol::extentid_t,extent_data_t> extent_map_t;

 protected:
  pthread_mutex_t mutex;
  extent_map_t extent_map;

 public:
  extent_server();
  

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
};

#endif 







