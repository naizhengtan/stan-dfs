// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <set>


void *wakeup(void*);

int isThreadAlive(pthread_t &thread_id){
  int kill_rc = pthread_kill(thread_id,0);

  if(kill_rc == ESRCH)
	//printf("the specified thread did not exists or already quit/n");
	return 0;
  else if(kill_rc == EINVAL)
	//printf("signal is invalid/n");
	return -1;
  else
	//printf("the specified thread is alive/n");
	return 1;
	}


void * check(void *lockset){
  lock_server_cache::lock_map *lock_set = (lock_server_cache::lock_map *)lockset;
  lock_server_cache::lock_map::iterator it;
  std::set<pthread_t*> threads;
  while(1){
	//printf("-------checker check-----\n");
	it = lock_set->begin();
	for(;it!=lock_set->end();it++){
	  if(!it->second.wqueue.empty()){
		if(threads.find(&it->second.awake)!=threads.end())//exist
		  continue;
		printf("create awake for lock %llx \n",it->second.lockid);
		if(pthread_create(&it->second.awake,NULL,wakeup,&it->second)){
		  printf("ERROR: CANNOT CREATE WAKEUP THREAD!\n");
		  assert(0);
		}
		//pthread_detach(it->second.awake);
		threads.insert(&it->second.awake);
	  }
	}
	std::set<pthread_t*>::iterator join = threads.begin();
	for(;join!=threads.end();join++){
	  if(isThreadAlive(**join)==0){//if the thread is over
		if(pthread_join(**join,NULL)){//collect it
		  printf("ERROR: CANNOT JOIN WAKEUP THREAD!\n");
		  assert(0);
		}
		threads.erase(join);//threads maintain the live threads
	  }
	}
	//threads.clear();
  }
}

void *wakeup(void* lf){
  int ret,r;
  lock_server_cache::lock_info* lockinfo=(lock_server_cache::lock_info*)lf;
  /**one lock cannot be reached together
	 because the checker will wait 
	 till all the thread end.
  **/
  //revoke
  std::string old = lockinfo->current;
  printf("===revoke[%s]%llx:...",lockinfo->current.c_str(),lockinfo->lockid);
  handle h(lockinfo->current);
  if(h.safebind()){
	ret = h.safebind()->call(rlock_protocol::revoke,lockinfo->lockid,r);
	if(ret!=lock_protocol::OK){
	  printf("ERROR %d\n",ret);
	  assert(0);
	}
  }else{
	printf("**ERROR** revoke [%s][%s] bind failed!\n",lockinfo->current.c_str(),old.c_str());
  }
  printf("...done\n");
  //retry
  //std::string next = lockinfo->wqueue.front();
  std::string next = lockinfo->current;
  handle hr(next);
  printf("===retry[%s]%llx:...",next.c_str(),lockinfo->lockid);
  if(hr.safebind()){
	ret = hr.safebind()->call(rlock_protocol::retry,lockinfo->lockid,r);
	if(ret!=lock_protocol::OK){
	  printf("ERROR \n");
	  assert(0);
	}
  }else{
	printf("**ERROR** retry [%s] bind failed!\n",next.c_str());
  }
  printf("...done\n");
  pthread_exit(NULL);
}


lock_server_cache::lock_server_cache()
{
  assert(pthread_mutex_init(&mutex,NULL)==0);
  if(pthread_create(&checker,NULL,check,&lock_set)){
	printf("ERROR: CAN NOT CREATE CHECKER!\n");
	assert(0);
  }
  printf("lock_server_cache established!\n");
}



int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  lock_map::iterator it = lock_set.find(lid);
  pthread_mutex_lock(&mutex);
  if(it==lock_set.end()){
	lock_info temp;
	temp.lockid = lid;
	temp.current = "";
	temp.state = LOCK_FREE;
	assert(pthread_mutex_init(&temp.mux,NULL)==0);
	lock_set.insert(lock_map::value_type(lid,temp));
	it = lock_set.find(lid);
  }
  pthread_mutex_unlock(&mutex);
  
  ScopedLock acq(&it->second.mux);
  tprintf("server: [%s] acquire lock %llx ",id.c_str(),lid);
  if(it->second.state==LOCK_FREE){
	it->second.state=LOCK_LOCKED;
	it->second.current = id;
	printf("OK\n");
	r = lock_protocol::OK;
  }
  else if(it->second.state==LOCK_WAITING){
	if(id==it->second.current){
	//if(id==it->second.wqueue.front()){
	  //it->second.current = it->second.wqueue.front();
	  //it->second.wqueue.pop_front();
	  it->second.state=LOCK_LOCKED;
	  r = lock_protocol::OK;
	  printf("WAIT OK\n");
	}else{
	  it->second.wqueue.push_back(id);
	  r = lock_protocol::RETRY;
	  printf("WAIT RETRY\n");
	}
  }
  else if(it->second.state==LOCK_LOCKED){
	it->second.wqueue.push_back(id);
	/*pthread_join(it->second.awake,NULL);
	if(pthread_create(&it->second.awake,NULL,awake,NULL)){
	  printf("ERROR: thread can not be created!\n");
	  assert(0);
	  }*/
	r = lock_protocol::RETRY;
	printf("RETRY\n");
  }else{
	  printf("ERROR: SHOULD NOT BE HERE!\n");
	  assert(0);
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  lock_map::iterator it = lock_set.find(lid);
  if(it==lock_set.end()){
	printf("ERROR: SHOULD NOT BE HERE!\n");
	assert(0);
  }

  //should only be used in revoke process! no lock.
  //FIXME: it the id do not hold the lock
  ScopedLock rel(&it->second.mux);
  tprintf("server: [%s] release lock %llx ",id.c_str(),lid);
  if(it->second.current!=id){
	printf("WARNING: %s TRYING TO RELEASE LOCK WHICH IS HOLD BY %s\n",id.c_str(),it->second.current.c_str());
  }
  else{
	if(it->second.wqueue.empty()){
	  it->second.current = ""; //have error
	  it->second.state=LOCK_FREE;
	  printf("FREE\n");
	}
	else{
	  it->second.current = it->second.wqueue.front();
	  it->second.wqueue.pop_front();
	  it->second.state=LOCK_WAITING;
	  printf("WAITING\n");
	}
  }
  r = ret;
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  lock_map::iterator it = lock_set.find(lid);
  if(it==lock_set.end())
	nacquire = 0;
  else
	nacquire = it->second.wqueue.size();
  r = nacquire;
  return lock_protocol::OK;
}

