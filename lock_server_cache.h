#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <deque>
#include <pthread.h>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {

 public:
  enum lock_state{LOCK_FREE,LOCK_LOCKED,LOCK_WAITING};
  typedef struct{
	lock_protocol::lockid_t lockid;
	std::string current;
	lock_state state;
	std::deque<std::string> wqueue;
	pthread_t awake;
	pthread_mutex_t mux;
  } lock_info;
  typedef std::map<lock_protocol::lockid_t,lock_info> lock_map;

 private:
  int nacquire;
  lock_map lock_set;
  pthread_t checker;
  pthread_mutex_t mutex;
  // protected:
  //void *check(void*);
  //void *wakeup(void*);

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
