// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

#include <map>
#include <pthread.h>

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

  //===stan added===
  enum statoflock{NONE,FREE,LOCKED,ACQUIRING,RELEASING};
  typedef struct{
	statoflock state;
	pthread_cond_t cond_v; //init no free
	//pthread_cond_t retry;
	pthread_mutex_t mux;
	pthread_mutex_t acq;
	bool retry;
	//int count;
  } lock_t;
  typedef unsigned long long inum;
  typedef std::map<lock_protocol::lockid_t,lock_t > cached_lock;

  cached_lock lock_pool;
  pthread_mutex_t mutex;
  //================

 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
