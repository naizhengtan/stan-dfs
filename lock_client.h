// lock client interface.

#ifndef lock_client_h
#define lock_client_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include <vector>

// Client interface to the lock server
class lock_client {
 public:
  class rScopedLock{
	lock_protocol::lockid_t lid;
	lock_client* lc;
  public:
	rScopedLock(lock_client* lclient,lock_protocol::lockid_t id){
	  lid = id;
	  lc = lclient;
	  lc->acquire(id);
	}
	~rScopedLock(){
	  lc->release(lid);
	}
  };
 protected:
  rpcc *cl;
 public:
  lock_client(std::string d);
  virtual ~lock_client() {};
  virtual lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  virtual lock_protocol::status stat(lock_protocol::lockid_t);
};


#endif 
