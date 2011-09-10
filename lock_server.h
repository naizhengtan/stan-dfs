// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

#include <map>
#include <pthread.h>
//#define LOCK_FREE 0
//#defnie LOCK_LOCKED 1
#define DEBUG
#ifdef DEBUG
#define dprintf(x...) printf(x)
#else
#define dprintf(x...)
#endif

class lock_server {

  enum lock_state{LOCK_FREE,LOCK_LOCKED};
  typedef struct{
    lock_state state;
    pthread_cond_t cond_v;
    int owner;
  } lock_t;
  typedef std::map<lock_protocol::lockid_t,lock_t> lock_map;

 protected:
  int nacquire;
  lock_map server_map;
  pthread_mutex_t mutex;
  

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







