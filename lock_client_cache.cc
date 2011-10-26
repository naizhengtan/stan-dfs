// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  //check if the lock is in the pool
  int ret = lock_protocol::OK;

  pthread_mutex_lock(&mutex);
  cached_lock::iterator it = lock_pool.find(lid);

  //if not add it
  if(it==lock_pool.end()){
	//new lock_t
	lock_t temp;
	temp.state = NONE;
	temp.retry = false;
	//temp.count = 1;
	assert(pthread_cond_init(&(temp.cond_v),NULL)==0); 
	//assert(pthread_cond_init(&(temp.retry),NULL)==0); 
	assert(pthread_mutex_init(&(temp.mux),NULL)==0); 
	assert(pthread_mutex_init(&(temp.acq),NULL)==0); 
	lock_pool.insert(cached_lock::value_type(lid,temp));
	it = lock_pool.find(lid);
  }
  pthread_mutex_unlock(&mutex);

  //check the state of lock
  //FIXME: concurrentcy bug here
  ScopedLock acq(&it->second.acq);
  if(it->second.state == NONE){
	it->second.state = ACQUIRING;
	while(1){
	  ret = lock_client::acquire(lid); //RPC happens
	  if(ret==lock_protocol::RETRY){
		if(it->second.retry){
		  it->second.retry = false;
		  continue;
		}
		pthread_cond_wait(&it->second.cond_v,&it->second.acq);
	  }
	  else
		break;
	}
	assert(ret==lock_protocol::OK);
	it->second.state = LOCKED;
  }
  else if(it->second.state == FREE){
	it->second.state = LOCKED;	
  }
  else if(it->second.state == LOCKED || it->second.state == ACQUIRING){// acquiring/locked
	//while(it->second.state==LOCKED)
	pthread_cond_wait(&it->second.cond_v,&it->second.acq);
	it->second.state=LOCKED;
  }else if(it->second.state==RELEASING){//releasing(release success should call signal)
	//???
  }
  else{
	printf("---ERROR---\nCOME TO THE WRONG PLACE!!\n");
	assert(0);
  }

  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  cached_lock::iterator it = lock_pool.find(lid);
  //there should be only one can be here!
  if(it==lock_pool.end()){
	printf("---ERROR---\nSHOULD NEVER BE HERE!!!\n");
	assert(0);
  }

  ScopedLock rel(&it->second.acq);
  it->second.state = FREE;
  pthread_cond_signal(&it->second.cond_v);
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  cached_lock::iterator it = lock_pool.find(lid);
  if(it==lock_pool.end()){
	printf("---ERROR---\nSHOULD NEVER BE HERE!!!\n");
	assert(0);
  }

  ScopedLock acq(&it->second.acq);
  while(it->second.state == LOCKED)
	pthread_cond_wait(&it->second.cond_v,&it->second.acq);
  it->second.state = RELEASING;
  int ret = lock_client::release(lid); //RPC happens
  assert(ret==rlock_protocol::OK);
  it->second.state = NONE;
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  cached_lock::iterator it = lock_pool.find(lid);
  if(it==lock_pool.end()){
	printf("---ERROR---\nSHOULD NEVER BE HERE!!!\n");
	assert(0);
  }

  //ScopedLock acq(&it->second.acq);
  //FIXME: MAY have some bugs here...
  if(!it->second.retry)
	it->second.retry = true;
  pthread_cond_signal(&it->second.cond_v);
  int ret = rlock_protocol::OK;
  return ret;
}



