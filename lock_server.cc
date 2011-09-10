// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex,NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  lock_map::iterator it = server_map.find(lid);
  //if the lock is found
  if(it != server_map.end()){
    //lock is free
    if(it->second.state==LOCK_FREE){
      it->second.state=LOCK_LOCKED;
      it->second.owner = clt;

    }
    //lock is locked
    else{
      //wait in condition variable
      while(it->second.state==LOCK_LOCKED)
	pthread_cond_wait(&it->second.cond_v,&mutex);
      it->second.state=LOCK_LOCKED;
      it->second.owner = clt;
    }
  }
  //if the lock is not present
  else{
    lock_t temp;
    temp.state = LOCK_LOCKED;
    pthread_cond_init(&(temp.cond_v),NULL);
    temp.owner = clt;
    server_map.insert(lock_map::value_type(lid,temp));
  }

  dprintf("lock_server:lock %d acquire succeed.\n",lid);
  pthread_mutex_unlock(&mutex);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  lock_map::iterator it = server_map.find(lid);
  //if the lock is found
  if(it!=server_map.end()){
    //check owner
    if(it->second.owner == clt){
      it->second.state = LOCK_FREE;
      pthread_cond_signal(&it->second.cond_v);
      dprintf("lock_server:lock %d release successed.\n",lid);
    }
    else{
      //FIXME
    }
  }
  //if the lock is not present
  else{
    //FIXME
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}
